| Proposal ID | CP015 |
|-------------|--------|
| Name | SYCL Specialization Constant |
| Date of Creation | 18 April 2018 |
| Target | SYCL 1.2.1 extension / SYCL 2.2 |
| Current Status | _Work in progress_ |
| Reply-to | Victor Lomüller <victor@codeplay.com> |
| Original author | Victor Lomüller <victor@codeplay.com>, Toomas Remmelg <toomas.remmelg@codeplay.com> |
| Contributors | Victor Lomüller <victor@codeplay.com>, Toomas Remmelg <toomas.remmelg@codeplay.com>, Ruyman Reyes <ruyman@codeplay.com> |

# SYCL Specialization Constant

## Motivation

Many applications use runtime known constants to adapt their behaviors to their runtime environment.
Such constants are unknown when the developer compiles the application but will remain invariant through-out the application execution.
This is especially true for highly tuned software that requires information about the hardware on which the the application is running.

Since OpenCL C kernels are being fully compiled at runtime, those constants are usually expressed as macro and the value is passed to online compiler when the kernel is being compiled.
However, SYCL being statically compiled, it is not possible to use this approach. Template based techniques might not be possible or come at the price of code size explosion.

SPIR-V, the standard intermediate representation for shader and compute kernels, introduced "specialization constants" as a way to replace this macro usage in statically compiled kernels.
Specialization constants in SPIR-V are treated as constants whose value is not known at the time of the SPIR-V module generation.
Providing these constants before building the module for the actual target provides the compiler with the opportunity to further optimize the program.

This proposal introduces a way to express such runtime constants in SYCL programs.
Even if the motivation is derived from a SPIR-V concept, its usage is not limited to device compilers targeting SPIR-V.

## Specialization Constant Overview

The following SYCL program present a specialization constant are expressed.

```cpp
#include <CL/sycl.hpp>
#include <vector>

class specialized_kernel;
class runtime_const;

// Fetch a value at runtime.
float get_value();

int main() {
 cl::sycl::queue queue;
 cl::sycl::program program(queue.get_context());

 // Create a specialization constant.
 cl::sycl::experimental::spec_constant<float, runtime_const> my_constant =
     program.set_spec_constant<runtime_const>(get_value());
 program.build_with_kernel_type<specialized_kernel>();

 std::vector<float> vec(1);
 {
   cl::sycl::buffer<float, 1> buffer(vec.data(), vec.size());

   queue.submit([&](cl::sycl::handler& cgh) {
     auto acc = cgh.get_access<cl::sycl::access::mode::write>(buffer);
     cgh.single_task<specialized_kernel>(
         program.get_kernel<specialized_kernel>(),
         [=]() { acc[0] = my_constant.get(); });
   });
 }
}
```
In this example, the call to `set_spec_constant` binds the value returned by the call to `get_value` to the SYCL `program`.
At static compilation time, the value is unknown to the SYCL device compiler, thus cannot be used by the optimizations.
At runtime, `get_value` is evaluated and bond to the SYCL `program`, giving the opportunity for the underlying OpenCL runtime to use it during the kernel build.
The function `set_spec_constant` returns a `spec_constant` object allowing the user to use the value inside the kernel.
After all runtime values are bounded to the program, the program is built.

The specialization constant `my_constant` is later used inside `specialized_kernel` and the expression `my_constant.get()` returns the value returned by the call to `get_value()`.
If the target natively supports specialization constant, this value will be known by the underlying OpenCL consumer when it builds the kernel.

A more concrete example would be a blocked matrix-matrix multiply.
In this version of the algorithm, threads in a work-group collectively load elements from global to local memory and then perform part of the operation on the block.
Typically, the operation would look like this:
```cpp
template <typename T>
void mat_multiply(cl::sycl::queue& q, T* MA, T* MB, T* MC, int matSize) {
  auto device = q.get_device();
  // Choose a block size based on some information about the device.
  auto maxBlockSize =
      device.get_info<cl::sycl::info::device::max_work_group_size>();
  auto blockSize = prevPowerOfTwo(std::sqrt(maxBlockSize));
  blockSize = std::min(matSize, blockSize);

  {
    range<1> dimensions(matSize * matSize);
    buffer<T> bA(MA, dimensions);
    buffer<T> bB(MB, dimensions);
    buffer<T> bC(MC, dimensions);

    q.submit([&](handler& cgh) {
      auto pA = bA.template get_access<access::mode::read>(cgh);
      auto pB = bB.template get_access<access::mode::read>(cgh);
      auto pC = bC.template get_access<access::mode::write>(cgh);
      auto localRange = range<1>(blockSize * blockSize);

      accessor<T, 1, access::mode::read_write, access::target::local> pBA(
          localRange, cgh);
      accessor<T, 1, access::mode::read_write, access::target::local> pBB(
          localRange, cgh);

      cgh.parallel_for<class mxm_kernel<T>>(
          nd_range<2>{range<2>(matSize, matSize),
                      range<2>(blockSize, blockSize)},
          [=](nd_item<2> it) {
            // Current block
            int blockX = it.get_group(0);
            int blockY = it.get_group(1);

            // Current local item
            int localX = it.get_local(0);
            int localY = it.get_local(1);

            // Start in the A matrix
            int a_start = matSize * blockSize * blockY;
            // End in the b matrix
            int a_end = a_start + matSize - 1;
            // Start in the b matrix
            int b_start = blockSize * blockX;

            // Result for the current C(i,j) element
            T tmp = 0.0f;
            // We go through all a, b blocks
            for (int a = a_start, b = b_start; a <= a_end;
                 a += blockSize, b += (blockSize * matSize)) {
              // Copy the values in shared memory collectively
              pBA[localY * blockSize + localX] =
                  pA[a + matSize * localY + localX];
              // Note the swap of X/Y to maintain contiguous access
              pBB[localX * blockSize + localY] =
                  pB[b + matSize * localY + localX];
              it.barrier(access::fence_space::local_space);
              // Now each thread adds the value of its sum
              for (int k = 0; k < blockSize; k++) {
                tmp +=
                    pBA[localY * blockSize + k] * pBB[localX * blockSize + k];
              }
              // The barrier ensures that all threads have written to local
              // memory before continuing
              it.barrier(access::fence_space::local_space);
            }
            auto elemIndex =
                it.get_global(1) * it.get_global_range()[0] + it.get_global(0);
            // Each thread updates its position
            pC[elemIndex] = tmp;
          });
    });
  }
}
```
In this example, `blockSize` depends on a runtime feature that is a hardware constant for a given device, thus never changes once known.
The main issue is that this value is treated as a constant variable, and the compiler is unable to use it to perform optimizations like constant propagation or loop unrolling.
It can even have an adverse effect as the value will use a register and the compiler may be forced to spill or reload the value multiple times.

Using specialization constants, the routine can be rewritten as:
```cpp
template <typename T>
void mat_multiply(cl::sycl::queue& q, T* MA, T* MB, T* MC, int matSize) {
  auto device = q.get_device();
  // Choose a block size based on some information about the device.
  auto maxBlockSize =
      device.get_info<cl::sycl::info::device::max_work_group_size>();
  auto blockSizeCst = prevPowerOfTwo(std::sqrt(maxBlockSize));
  blockSizeCst = std::min(matSize, blockSize);

  cl::sycl::program program(queue.get_context());

  // Create a specialization constant to encapsulate blockSize.
  auto blockSize = program.set_spec_constant<class block_size>(blockSizeCst);
  program.build_with_kernel_type<class mxm_kernel<T>>();

  {
    range<1> dimensions(matSize * matSize);
    buffer<T> bA(MA, dimensions);
    buffer<T> bB(MB, dimensions);
    buffer<T> bC(MC, dimensions);

    q.submit([&](handler& cgh) {
      auto pA = bA.template get_access<access::mode::read>(cgh);
      auto pB = bB.template get_access<access::mode::read>(cgh);
      auto pC = bC.template get_access<access::mode::write>(cgh);
      auto localRange = range<1>(blockSize * blockSize);

      accessor<T, 1, access::mode::read_write, access::target::local> pBA(
          localRange, cgh);
      accessor<T, 1, access::mode::read_write, access::target::local> pBB(
          localRange, cgh);

      cgh.parallel_for<class mxm_kernel<T>>(
          program.get_kernel<class mxm_kernel<T>>(),
          nd_range<2>{range<2>(matSize, matSize),
                      range<2>(blockSize, blockSize)},
          [=](nd_item<2> it) {
            // Current block
            int blockX = it.get_group(0);
            int blockY = it.get_group(1);

            // Current local item
            int localX = it.get_local(0);
            int localY = it.get_local(1);

            // Start in the A matrix
            int a_start = matSize * blockSize * blockY;
            // End in the b matrix
            int a_end = a_start + matSize - 1;
            // Start in the b matrix
            int b_start = blockSize * blockX;

            // Result for the current C(i,j) element
            T tmp = 0.0f;
            // We go through all a, b blocks
            for (int a = a_start, b = b_start; a <= a_end;
                 a += blockSize, b += (blockSize * matSize)) {
              // Copy the values in shared memory collectively
              pBA[localY * blockSize + localX] =
                  pA[a + matSize * localY + localX];
              // Note the swap of X/Y to maintain contiguous access
              pBB[localX * blockSize + localY] =
                  pB[b + matSize * localY + localX];
              it.barrier(access::fence_space::local_space);
              // Now each thread adds the value of its sum
              for (int k = 0; k < blockSize; k++) {
                tmp +=
                    pBA[localY * blockSize + k] * pBB[localX * blockSize + k];
              }
              // The barrier ensures that all threads have written to local
              // memory before continuing
              it.barrier(access::fence_space::local_space);
            }
            auto elemIndex =
                it.get_global(1) * it.get_global_range()[0] + it.get_global(0);
            // Each thread updates its position
            pC[elemIndex] = tmp;
          });
    });
  }
}
```
In this example, `blockSize` is now a specialization constant holding the value same value as before, meaning that the value is now injected inside the module, allow the OpenCL consumer to use the value in the optimizations.
Note that the specialization constant ID is independent from the template parameter `T` from which the kernel depends on. This means that all kernel instances will share the same value.


## Specialization Constant Representation

Specialization constants are encapsulated into a `cl::sycl::experimental::spec_constant` immutable object which can be passed to a SYCL kernel as a parameter.
This object can only be constructed by the SYCL runtime.
Accessing the value is done either explicitly via a `get` function or an implicit conversion.

The `cl::sycl::experimental::spec_constant` interface is defined as follows:

```cpp
namespace cl {
namespace sycl {
namespace experimental {

template <typename T, typename ID = T>
class spec_constant {
private:
  // Implementation defined constructor.
  spec_constant(/* Implementation defined */);
public:
  spec_constant();

  T get() const; // explicit access.
  operator T() const;  // implicit conversion.
};

}  // namespace experimental
}  // namespace sycl
}  // namespace cl
```

Where `T` is the type of the constant. To be valid, the type `T` must be standard layout and trivially copyable.
The template parameter `ID` is a unique name to designate the specialization constant.
The name follows the same requirement and restrictions as the SYCL kernel names.
It is valid for a program to reuse a kernel name for a specialization constant name and vice versa.

There is no guarantees about the size of the object, whether or not the constant is stored in memory is left as an implementation detail.

A `cl::sycl::experimental::spec_constant` object is considered initialized once the result of a `cl::sycl::program::set_spec_constant` is assigned to it.

Once initialized, `cl::sycl::experimental::spec_constant` objects are immutable, attempts to circumvent this property produces undefined behavior.

`cl::sycl::experimental::spec_constant` is default constructible, although the object is not considered initialized until the result of the call to `cl::sycl::program::set_spec_constant` is assigned to it.

Attempts to use an uninitialized `cl::sycl::experimental::spec_constant` produces undefined behavior.

## Building Programs with Specialization Constants

SYCL program requiring a specialization constant value to be built must first set them before building.

The program interface is extended to include a mechanism to set the constant.

```cpp
namespace cl {
namespace sycl {

class program {
// ...
public:
 template <typename ID, typename T>
 spec_constant<T, ID> set_spec_constant(T cst);
// ...
};

}  // namespace sycl
}  // namespace cl
```

The templated member function `set_spec_constant` takes a runtime value of type `T` that will be used to set the specialization constant named `ID`.
Multiple specialization constants can be set for the same program by calling `set_spec_constant` multiple times.
Previously created `cl::sycl::experimental::spec_constant` objects becomes invalids and any usage of invalided objects produce undefined behavior.

A specialization constant value can be overwritten if the program was not built before by recalling `set_spec_constant` with the same `ID` and the new value.
Although the type `T` of the specialization constant must remain the same.

Once all specialization constants are set, the program can be compile/built using program's function `compile_with_kernel_type`/`build_with_kernel_type`.

If a required specialization constant is not set before calling `compile_with_kernel_type` / `build_with_kernel_type`, a `cl::sycl::experimental::spec_const_error` is thrown and the build of the kernel fails.

For a same kernel, it is valid to set different specialization constants to different `cl::sycl::program` that builds it.

After the kernel is built, it is no longer possible to set new specialization constants.
A `cl::sycl::experimental::spec_const_error` exception will be thrown if the user attempt change it after the kernel has been built.

## Build issue caused by Specialization Constants

The following error class is added:
```cpp
namespace cl {
namespace sycl {
namespace experimental {

class spec_const_error : public compile_program_error;

}  // namespace experimental
}  // namespace sycl
}  // namespace cl
```

This error can be thrown if a specialization constant compilation error occurs.

## OpenCL Interoperability

In SYCL, specialization constants use typenames to identify them rather than using the SPIR-V/OpenCL numerical identifiers.

To allow interoperability with OpenCL, uses can use this a special templated type as the SYCL specialization constant identifier to specify the numerical identifier of a specialization constant inside the module:
```cpp
namespace cl {
namespace sycl {
namespace experimental {

template <unsigned NID>
struct spec_constant_id {
  static constexpr unsigned id = NID;
};

}  // namespace experimental
}  // namespace sycl
}  // namespace cl
```

The runtime will use the value `NID` provided by the template parameter to set the specialization constant.
If the specified identifier does not exist in the module, a `cl::sycl::experimental::spec_const_error` error is thrown.

For example:
```cpp
 // Create a specialization constant.
 auto my_constant = program.set_spec_constant<cl::sycl::experimental::spec_constant_id<42>>(get_value());
```
In this call, the runtime will bind the value with the specialization constant with the identifier `42`.
