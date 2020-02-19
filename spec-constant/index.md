| Proposal ID | CP015 |
|-------------|--------|
| Name | SYCL Specialization Constant |
| Date of Creation | 18 April 2018 |
| Target | SYCL 1.2.1 extension |
| Depends on | `kernel_handler` |
| Current Status | _Work in progress_ |
| Reply-to | Victor Lomüller <victor@codeplay.com> |
| Original author | Victor Lomüller <victor@codeplay.com>, Toomas Remmelg <toomas.remmelg@codeplay.com> |
| Contributors | Victor Lomüller <victor@codeplay.com>, Toomas Remmelg <toomas.remmelg@codeplay.com>, Ruyman Reyes <ruyman@codeplay.com> |

# SYCL Specialization Constant

## Motivation

Many applications use runtime known constants to adapt their behaviors to their runtime environment.
Such constants are unknown when the developer compiles the application but will remain invariant throughout the application execution.
This is especially true for highly tuned software that requires information about the hardware on which the application is running.

Since OpenCL C kernels are being fully compiled at runtime, those constants are usually expressed as macro and the value is passed to online compiler when the kernel is being compiled.
However, SYCL being statically compiled, it is not possible to use this approach. Template based techniques might not be possible or come at the price of code size explosion.

SPIR-V, the standard intermediate representation for shader and compute kernels, introduced *specialization constants* as a way to replace this macro usage in statically compiled kernels.
Specialization constants in SPIR-V are treated as constants whose value is not known at the time of the SPIR-V module generation.
Providing these constants before building the module for the actual target provides the compiler with the opportunity to further optimize the program.

This proposal introduces a way to express such runtime constants in SYCL programs.
Even if the motivation is derived from a SPIR-V concept, its usage is not limited to device compilers targeting SPIR-V.

## Specialization Constant Overview

The following present a minimal SYCL 1.2.1 program using specialization constant written with C++17:

```cpp
#include <CL/sycl.hpp>
#include <vector>

class specialized_kernel;

// Fetch a value at runtime.
float get_value();

// Declare a specialization constant id.
// The variable `runtime_const` will be used as the id.
cl::sycl::specialization_id<float> runtime_const(42.f);

int main() {
  cl::sycl::queue queue;
  std::vector<float> vec(1);

  {
    cl::sycl::buffer<float, 1> buffer(vec.data(), vec.size());

    queue.submit([&](cl::sycl::handler &cgh) {
      auto acc = cgh.get_access<cl::sycl::access::mode::write>(buffer);

      // Set a runtime as a JIT constant.
      // This may force the runtime to compile `specialized_kernel`
      // using the value returned by get_value.
      cgh.set_specialization_constant<runtime_const>(get_value());

      cgh.single_task<specialized_kernel>([=](cl::sycl::kernel_handler h) {
        // The value returned by get_specialization_constant is the value
        // returned by get_value().
        acc[0] = h.get_specialization_constant<runtime_const>();
      });
    });
  }
  return 0;
}
```

The global variable `runtime_const` declare a specialization constant id with the default value `42.0`.
This variable acts as an identifier for SYCL to refer to a constant whose value may only be set at runtime.
In the main function, a SYCL kernel is enqueued. Before it is enqueued, the call `cgh.set_specialization_constant<runtime_const>(get_value())` is binding the value returned by `get_value()` to the current module and the specialization id `runtime_const`.
In the kernel, the value return by `h.get_specialization_constant<runtime_const>()` will be the value returned by `get_value()` before the enqueue.
If the target natively supports specialization constant, this value will be known by the underlying consumer when it builds the kernel.
Without the call to `set_specialization_constant<runtime_const>`, the call `get_specialization_constant<runtime_const>` would still be valid and would have return the default value `42.0`.

The following present an equivalent program as above but in C++11 and with a prebuild kernel:

```cpp
#include <CL/sycl.hpp>
#include <vector>

class specialized_kernel;
class runtime_const;

// Fetch a value at runtime.
float get_value();

// Declare a specialization constant id.
// The variable `runtime_const` will be used as the id.
cl::sycl::specialization_id<float> runtime_const(42.f);

int main() {
  cl::sycl::queue queue;
  cl::sycl::program program(queue.get_context());

  // Set the value of the specialization constant.
  program.set_specialization_constant<float, runtime_const>(get_value());

  // Build the program, the value set by set_spec_constant
  // will be used as a constant by the underlying JIT
  // if it has native support for specialization constant.
  program.build_with_kernel_type<specialized_kernel>();

  std::vector<float> vec(1);
  {
    cl::sycl::buffer<float, 1> buffer(vec.data(), vec.size());

    queue.submit([&](cl::sycl::handler &cgh) {
      auto acc = cgh.get_access<cl::sycl::access::mode::write>(buffer);

      cgh.single_task<specialized_kernel>(
          program.get_kernel<specialized_kernel>(),
          [=](cl::sycl::kernel_handler h) {
            // This should become a constant.
            acc[0] = h.get_specialization_constant<float, runtime_const>();
          });
    });
  }
  return 0;
}
```

This code is doing the same as previously expect that the kernel is precompiled and compatible with C++11.

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

In this example, `blockSize` depends on a runtime feature that is a hardware constant depending on the device, thus never changes once known.
The main issue is that this value is treated as a constant variable, and the compiler is unable to use it to perform optimizations like constant propagation or loop unrolling.
It can even have an adverse effect as the value will use a register and the compiler may be forced to spill or reload the value multiple times.

Using specialization constants, the routine can be rewritten as:

```cpp
using namespace cl::sycl;

specialization_id<std::size_t> block_size{1};


template <typename T>
void mat_multiply(queue& q, T* MA, T* MB, T* MC, int matSize) {
  auto device = q.get_device();
  // Choose a block size based on some information about the device.
  auto maxBlockSize =
      device.get_info<info::device::max_work_group_size>();
  auto blockSizeCst = prevPowerOfTwo(std::sqrt(maxBlockSize));
  blockSizeCst = std::min(matSize, blockSize);

  {
    range<1> dimensions(matSize * matSize);
    buffer<T> bA(MA, dimensions);
    buffer<T> bB(MB, dimensions);
    buffer<T> bC(MC, dimensions);

    q.submit([&](handler& cgh) {
      auto pA = bA.template get_access<access::mode::read>(cgh);
      auto pB = bB.template get_access<access::mode::read>(cgh);
      auto pC = bC.template get_access<access::mode::write>(cgh);
      auto localRange = range<1>(blockSizeCst * blockSizeCst);

      accessor<T, 1, access::mode::read_write, access::target::local> pBA(
          localRange, cgh);
      accessor<T, 1, access::mode::read_write, access::target::local> pBB(
          localRange, cgh);
      cgh.set_spec_constant<std::size_t, block_size>(blockSizeCst);

      cgh.parallel_for<class mxm_kernel<T>>(
          program.get_kernel<class mxm_kernel<T>>(),
          nd_range<2>{range<2>(matSize, matSize),
                      range<2>(blockSizeCst, blockSizeCst)},
          [=](nd_item<2> it, kernel_handler handler) {
            // Current block
            int blockX = it.get_group(0);
            int blockY = it.get_group(1);

            // Current local item
            int localX = it.get_local(0);
            int localY = it.get_local(1);

            std::size_t blockSize = h.get_specialization_constant<block_size>();
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

In this example, `blockSize` is now depending on a specialization constant, meaning that the value can be now injected inside the module. The consumer can freely use the value to perform optimizations (loop unrolling for instance).
Note that the specialization constant ID is independent of the template parameter `T` from which the kernel depends on. This means that all kernel instances will share the same value.

## Specialization Constant Representation

Specialization constants are represented by instances of `cl::sycl::specialization_id` in the namespace scope.
Each instance of `cl::sycl::specialization_id` holds a default value that can be set using the constexpr constructor.
Note that the device compiler must be able to evaluate the default value at compile time to establish a valid module.

The `cl::sycl::specialization_id` interface is defined as follows:

```cpp
namespace cl {
namespace sycl {

template <typename T>
class specialization_id {
private:
  // Implementation defined constructor.
  specialization_id(const specialization_id&) = delete;
  specialization_id(specialization_id&&) = delete;


public:
  static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");

  using type = T;

  // Argument `Args` are forwarded to an underlying T Ctor.
  // This allow the user to setup a default value for the specialization_id instance.
  // The initialization of T must be evaluated at compile time to be valid.
  template<class... Args >
  explicit constexpr specialization_id(Args&&...);
};

}  // namespace sycl
}  // namespace cl
```

Where `T` is the type of the constant. To be valid, the type `T` must be trivially copyable.
Instances of `cl::sycl::specialization_id` must be forward declarable.

## Building Programs with Specialization Constants

SYCL program using specialization constants can be set before being built.
The `program` interface is extended to include a mechanism to set the constant.

```cpp
namespace cl {
namespace sycl {

class program {
// ...
public:

  /**
   * Returns true if the current program can support specialization constants natively.
   *
   */
  bool has_native_spec_constant() const noexcept;

  /**
   * Set a new the value for the specialization constant represented identified by the specialization_id<T> instance.
   */
  template <typename T, specialization_id<T>&>
  void set_specialization_constant(T cst);

  /**
   * Set a new the value for the specialization constant represented identified by the specialization_id instance `s`.
   */
  template<auto& s>
  void set_specialization_constant(typename std::remove_reference_t<decltype(s)>::type);

// ...
};

}  // namespace sycl
}  // namespace cl
```

The templated member function `set_specialization_constant` binds a runtime value of type `T` to `specialization_id<T>` id and the program.
If a value were already set for a given `specialization_id<T>` id then the value is overwritten by the new value.

Once all specialization constants are set, the program can be compile/built using program's function `compile_with_kernel_type`/`build_with_kernel_type`.
Once the program is in a build state, the specialization constant can no longer be changed for the program and call to `set_specialization_constant` will throw a `cl::sycl::spec_const_error` exception.

## Getting Specialization Constants via the command group handler

The handler interface is extended to include a mechanism to get a specialization constant.

```cpp
namespace cl {
namespace sycl {

class handler {
// ...
public:
 // Set a value for the specialization constant represented by `s`
 // and return the associated spec_constant.
 // Note, this call may require the underlying program to be rebuilt.
 template <typename T, specialization_id<T>& s>
 void set_spec_constant(T cst);

 // Set a value for the specialization constant represented by `s`
 // and return the associated spec_constant.
 // Note, this call may require the underlying program to be rebuilt.
 // Only if compiling with C++17 or above.
 template<auto& s>
 void set_specialization_constant(typename std::remove_reference_t<decltype(s)>::type);

// ...
};

}  // namespace sycl
}  // namespace cl
```

Upon invocation of a `single_task`/`parallel_for`/`parallel_for_work_group` construct, the runtime will build the appropriate kernel if it has never been built for the set of specialization constant passed to the kernel.
The SYCL device compiler and runtime are responsible to make sure that it is valid to build the module in which the invoked kernel is defined using only the provided specialization constants.

It is illegal to use this interface in conjunction with the `cl::sycl::program` interface.

It must be noted that setting a specialization constant has an underlying cost and that changing a constant value will force the OpenCL runtime to build a new kernel.

## Build issue caused by Specialization Constants

The following error class is added:
```cpp
namespace cl {
namespace sycl {

class spec_const_error : public compile_program_error;

}  // namespace sycl
}  // namespace cl
```

This error can be thrown if a specialization constant compilation error occurs.

## In kernel access to specialization constant

Specialization constants are accessed in kernel using the API provided by the `kernel_handler`.
