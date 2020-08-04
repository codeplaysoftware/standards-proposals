| Proposal ID      | CP027                                                                                                                                                                                |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Name             | Global device symbols                                                                                                                                                                |
| Date of Creation | 2 July 2019                                                                                                                                                                          |
| Last Update      | 3 August 2020                                                                                                                                                                        |
| Target           | SYCL 2020 Proposal                                                                                                                                                                   |
| Current Status   | *Draft*                                                                                                                                                                              |
| Written against  | SYCL 2020 provisional                                                                                                                                                                |
| Available        | N/A                                                                                                                                                                                  |
| Reply-to         | Victor Lomuller <victor@codeplay.com>                                                                                                                                                |
| Original author  | Victor Lomuller <victor@codeplay.com>                                                                                                                                                |
| Contributors     | TBD                                                                                                                                                                                  |

## Change log

- v1: initial proposal

## TODO

- Handling of plain global variable.

## Overview


A symbol object allows the representation of a global value accessible by a device kernel without being passed as a kernel parameter.
From the device point of view, the value of the symbol lives in the device's global or constant memory.
From the host point of view, this is a regular global variable whose value can be explicitly transferred to the device using copy operations inside a `sycl::queue`.

In CUDA, this functionality is covered by the runtime library functions `cudaMemcpyToSymbol`, `cudaMemcpyToSymbolAsync`, `cudaMemcpyFromSymbol` and `cudaMemcpyFromSymbolAsync`.

The end of this document present a toy example using this proposal followed by an equivalent CUDA code.

## API definitions

### symbol class overview

A symbol object `sycl::symbol<T>` represents a global value of type `T` accessible by both the device and the host.
It allows the user to manage (read, write and move values) a global variable value across different devices.
In contrast to regular global variables (which can be used in regular SYCL code), `sycl::symbol` allow users to manage the values and the address space in which they should be allocated in.
It is particularly useful for platforms where pushing a constant for a kernel is more efficiently done using a global variable.

Symbol objects are bound to `sycl::module<module_status::executable>`.
It is the responsibility of the user to maintain coherency between the SYCL symbol and its value in the different SYCL modules.
The SYCL runtime does not guarantee that the value of the symbol will be the same on all devices and on the host.
Loading and building the same module twice under the same context will lead to the materialization of two distinct symbols on the device.

There is exactly one symbol on the host, which has the same lifetime as defined by the C++ specs.

`symbol` values can be set or retrieved using handler copy operations.

`sycl::symbol` objects must have a static storage duration and be created in a globally visible scope.
If the `symbol` is named, then it may live in a non globally visible scope.

The underlying type `T` of the symbol follows the same restriction as the underlying type of accessors with the following exceptions:

- The called constructor(s) does not have to follow the device's restrictions;
- The destructor does not have to follow the device's restrictions and can be declared virtual.
`T`'s constructor and destructor are only called on the host.

#### Interface

``` c++
namespace sycl {

template<typename T, access::address_space asp = access::address_space::constant_space>
class symbol {
  // A symbol object can not be copied
  symbol(const symbol&) = delete;
  symbol(symbol&&) = delete;

  symbol& operator=(const symbol&) = delete;
  symbol& operator=(symbol&&) = delete;

  static_assert(!std::is_reference<T>::value, "T cannot be a reference");
  static_assert(asp == access::address_space::global_space || asp == access::address_space::constant_space,
   "Only Constant and Global address space are allowed");
 public:

  static constexpr access::address_space space = asp;
  using type = T;
  using const_type = std::add_const_t<T>;
  template<access::decorated DecorateAddress>
  using multi_ptr_type = sycl::multi_ptr<T, space, DecorateAddress>;

  // symbol constructor, Args arguments are forwarded to T's constructor.
  template<class... Args >
  explicit symbol(Args&&...);

  // Underlying value access

  // A symbol can be implicitly converted to the underlying type
  // Only if space != access::address_space::constant_space
  operator type&();
  // Only if space != access::address_space::constant_space
  operator const type&() const;
  // Only if space == access::address_space::constant_space
  operator type() const;

  // Assignment for T
  // Only if space != access::address_space::constant_space
  symbol& operator=(const T&);

  // Return a pointer to the underlying value as a multi_ptr object
  template<access::decorated DecorateAddress>
  operator multi_ptr<type, space, DecorateAddress>();
  template<access::decorated DecorateAddress>
  operator multi_ptr<const_type, space, DecorateAddress>() const;

  // Return a pointer to the underlying value as a multi_ptr object
  template<access::decorated DecorateAddress>
  multi_ptr<type, space, DecorateAddress> get();
  template<access::decorated DecorateAddress>
  multi_ptr<const_type, space, DecorateAddress> get() const;

  multi_ptr<type, space, access::decorated::yes> get_decorated();
  multi_ptr<const_type, space, access::decorated::yes> get_decorated() const;
  multi_ptr<type, space, access::decorated::no> get_raw();
  multi_ptr<const_type, space, access::decorated::no> get_raw() const;
};

template<typename T, std::size_t N, access::address_space asp>
class symbol<T[N], asp> {
  symbol(const symbol&) = delete;
  symbol(symbol&&) = delete;

  symbol& operator=(const symbol&) = delete;
  symbol& operator=(symbol&&) = delete;

  static_assert(!std::is_reference<T>::value, "T cannot be a reference");
  static_assert(asp == access::address_space::global_space || asp == access::address_space::constant_space,
   "Only Constant and Global address space are allowed");
 public:

  static constexpr access::address_space space = asp;
  static constexpr std::size_t length = N;
  using type = T;
  using const_type = std::add_const_t<T>;
  template<access::decorated DecorateAddress>
  using multi_ptr_type = sycl::multi_ptr<T, space, DecorateAddress>;

  // symbol constructor, Args arguments are forwarded to T's constructor.
  template<class... Args >
  explicit symbol(Args&&...);

  // Underlying value access

  // Return a pointer to the first element (array-to-pointer decay)
  // Only if space != access::address_space::constant_space
  operator type*();
  // Only if space != access::address_space::constant_space
  operator const_type*() const;
  // Only if space == access::address_space::constant_space
  operator multi_ptr_type<access::decorated::no>() const;

  // Return a pointer to the first element of the underlying array as a multi_ptr object.
  template<access::decorated DecorateAddress>
  operator multi_ptr<type, space, DecorateAddress>();
  template<access::decorated DecorateAddress>
  operator multi_ptr<const_type, space, DecorateAddress>() const;

  // Return a pointer to the first element of the underlying array as a multi_ptr object.
  template<access::decorated DecorateAddress>
  multi_ptr<type, space, DecorateAddress> get();
  template<access::decorated DecorateAddress>
  multi_ptr<const_type, space, DecorateAddress> get() const;

  multi_ptr<type, space, access::decorated::yes> get_decorated();
  multi_ptr<const_type, space, access::decorated::yes> get_decorated() const;
  multi_ptr<type, space, access::decorated::no> get_raw();
  multi_ptr<const_type, space, access::decorated::no> get_raw() const;

  // Access the i-th value of the underlying array.
  // Only if space != access::address_space::constant_space
  std::add_lvalue_reference_t<type> operator[](int i);
  // Only if space != access::address_space::constant_space
  std::add_lvalue_reference_t<const_type> operator[](int i) const;

  // Access the i-th value of the underlying array.
  // Only if space == access::address_space::constant_space
  type operator[](int i) const;
};

}  // namespace sycl
```

### handler class extension

The following extension handles memory transfers to/from a symbol. As symbols are bound to modules the added functions are sensitive to the `handler::use_module`.

```c++
namespace sycl {

class handler {
 public:

  /* Add the following interface */

  // Update the value of the host side symbol using its device counterpart.
  // This creates a dependency on the kernels referencing the symbol.
  template <auto& Sym>
  void update_host();

  // Update the value of the device side symbol using its host counterpart.
  // This creates a dependency on the kernels referencing the symbol.
  template <auto& Sym>
  void update_device();

  // Retrieve the value of the symbol from the device.
  // This creates a dependency on the kernels referencing the symbol.
  template <auto& Sym>
  void copy_from_symbol(shared_ptr_class<typename std::remove_reference_t<decltype(Sym)>::type> dest);

  // Set the value of the symbol on the device
  // This creates a dependency on the kernels referencing the symbol.
  template <auto& Sym>
  void copy_to_symbol(shared_ptr_class<typename std::remove_reference_t<decltype(Sym)>::type> src);
  template <auto& Sym>
  void copy_to_symbol(shared_ptr_class<typename std::remove_reference_t<decltype(Sym)>::const_type> src);

  // Retrieve the value of the symbol from the device.
  // This creates a dependency on the kernels referencing the symbol.
  template <auto& Sym>
  void copy_from_symbol(typename std::remove_reference_t<decltype(Sym)>::type& dest);

  // Set the value of the symbol on the device
  // This creates a dependency on the kernels referencing the symbol.
  template <auto& Sym>
  void copy_to_symbol(typename std::remove_reference_t<decltype(Sym)>::const_type& src);

  // Fill the symbol object accessed by Sym by the value v.
  template <auto& Sym>
  void fill(typename std::remove_reference_t<decltype(Sym)>::const_type& v);
};

}  // namespace sycl
```

## Global symbols vs specialization constants


Global symbols and specialization constants map to different use cases.
The first is meant to be a managed global value accessible on the device.
The value is read-only or read-write on the device and always read-write on the host.
The second is meant to specialize the underlying module by injecting a value that the driver can use to optimize the module. It can be seen as a macro whose value is only known at runtime.

The main difference between the two is the associated action performed by the SYCL runtime:

- global symbols: changing the value will trigger a memory copy to the device;
- specialization constants: changing the value means the module needs to be rebuild.

Global symbols represent a global value, bound to a `sycl::module<module_status::executable>`, whose value can be set/retrieved at runtime before a kernel enqueue.
Global symbols values may be changed by a kernel running on a device.

Specialization constants represent a value, bound to a `sycl::module<module_status::source>`, which can be set at runtime before the module is built.
As a module defines symbols and is built with two sets of specialization constant values, the runtime will yield two executable modules with 2 distinct instances of the symbols (1 per module).

## Example


[Compiler Explorer link](https://godbolt.org/z/98n5je)

```c++
class MutableGVName;
class GVArrayName;

sycl::symbol<int> GV;
sycl::symbol<int, sycl::access::address_space::global_space> MutableGV;
sycl::symbol<int[42], sycl::access::address_space::global_space> GVArray;

void call(int v1, int v2) {
  sycl::queue q;
  int res;

  GV = 42;
  MutableGV = 21;

  // Set device side GV
  q.submit([&](sycl::handler h) {
    h.copy_to_symbol<GV>(v1);
  });

  {
    sycl::buffer<int> buff(&res);

    q.submit([&](sycl::handler h) {
      // Create write accessor to store the value of GV in device
      auto acc = buff.get_accessor(h);
      h.parallel_for([=] () mutable {
        acc[0] = GV;
      });
    });
  }
  // Check that we copied back the device value
  assert(res == v1);
  // Check that the host GV was not affect by the initial copy
  assert(GV == 42);

  q.submit([&](sycl::handler h) {
    h.copy_to_symbol<GV>(v2);
  });

  {
    sycl::buffer<int> buff(&res);

    q.submit([&](sycl::handler h) {
      auto acc = buff.get_accessor(h);
      h.parallel_for([=] () mutable {
        acc[0] = GV;
      });
    });
  }
  assert(res == v2);

  q.submit([&](sycl::handler h) {
    h.copy_to_symbol<MutableGV>(v1);
  });

  auto ev = q.submit([&](sycl::handler h) {
    h.parallel_for([=] () mutable {
      MutableGV = v2;
    });
  });
  ev.wait();
  assert(MutableGV == 21);
  ev = q.submit([&](sycl::handler h) {
    // Copy the host symbol value using the device value
    h.update_host<MutableGV>();
  });
  ev.wait();
  assert(MutableGV == v2);

  q.submit([&](sycl::handler h) {
    // Fill the global array with a given value
    h.fill<GVArray>(42);
  });
}
```

The following code implements the same as above but in CUDA [Compiler Explorer link](https://godbolt.org/z/YjL4dn)

Device code
```c++
__constant__ int GV = 42;
__device__ int MutableGV = 42;
__device__ int dfactor[42];

__global__ void kernel1(int* acc) {
  *acc = GV;
}

__global__ void kernel2(int* acc) {
  *acc = GV;
}

__global__ void kernel3(int v2) {
  MutableGV = v2;
}
```

Host code

```c++
// test constant variable and cudaMemcpyToSymbol
#include <cuda.h>
#include <cuda_device_runtime_api.h>
#include <iostream>
#include <vector>

struct MiniQueue {
  CUstream stream;
  CUcontext ctx;
  CUmodule cuModule;
};

// Load kernel binary
void getFile(std::vector<char>& buffer);

// Host side equivalent to
// sycl::symbol<int> GV;
// sycl::symbol<int, sycl::access::address_space::global_space> MutableGV;
// sycl::symbol<int[42], sycl::access::address_space::global_space> GVArray;

CUdeviceptr GV;
CUdeviceptr MutableGV;
CUdeviceptr GVArray;

void call(int v1, int v2) {
  // sycl::queue q;
  std::vector<char> kernel_buffer;
  // Loading device code
  getFile(kernel_buffer);
  cuInit(0);

  CUdevice device;
  {
    // init device
    char name[100];
    int devID = 0;
    cuDeviceGet(&device, devID);
    cuDeviceGetName(name, 100, device);
    cuDeviceGet(&device, devID);
  }

  MiniQueue queue;
  cuCtxCreate(&queue.ctx, /*flags*/ 0, device);
  cuCtxSetCurrent(queue.ctx);
  cuStreamCreate(&queue.stream, 0);
  cuModuleLoadFatBinary(&queue.cuModule, kernel_buffer.data());
  size_t s;

  // Retrieve device pointer to "symbols"
  cuModuleGetGlobal(&GV, &s, queue.cuModule, "GV");
  cuModuleGetGlobal(&MutableGV, &s, queue.cuModule, "MutableGV");
  cuModuleGetGlobal(&GVArray, &s, queue.cuModule, "GVArray");

  // int res;
  int* res;
  cuMemAllocManaged((CUdeviceptr*)&res, sizeof(int), CU_MEM_ATTACH_GLOBAL);

  // GV = 42;
  // MutableGV = 21;

  // No equivalent !

  // // Set device side GV
  // q.submit([&](sycl::handler h) {
  //   h.copy_to_symbol<GV>(v1);
  // });
  cuMemcpyHtoDAsync(GV, &v1, sizeof(int), queue.stream);
  cuStreamSynchronize(queue.stream);

  // {
  //   sycl::buffer<int> buff(&res);
  //   q.submit([&](sycl::handler h) {
  //     // Create write accessor to store the value of GV in device
  //     auto acc = buff.get_accessor(h);
  //     h.parallel_for([=] () mutable {
  //       acc[0] = GV;
  //     });
  //   });
  // }

  {
    CUfunction cuFunc;
    cuModuleGetFunction(&cuFunc, queue.cuModule, "kernel1");

    void *param[] = {&res, 0};
    cuLaunchKernel(cuFunc,
                   1, 1, 1, // grid
                   1, 1, 1,               // block
                   0,                     // sharedMem
                   queue.stream,
                   param, // void** kernelParams,
                   NULL   // void** extra
      );
    cuStreamSynchronize(queue.stream);
  }
  // // Check that we copied back the device value
  // assert(res == v1);
  assert(*res == v1);
  // // Check that the host GV was not affect by the initial copy
  // assert(GV == 42);

  // ===> No equivalent !

  // q.submit([&](sycl::handler h) {
  //   h.copy_to_symbol<GV>(v2);
  // });
  cuMemcpyHtoDAsync(GV, &v2, sizeof(int), queue.stream);
  cuStreamSynchronize(queue.stream);

  // {
  //   sycl::buffer<int> buff(&res);
  //   q.submit([&](sycl::handler h) {
  //     auto acc = buff.get_accessor(h);
  //     h.parallel_for([=] () mutable {
  //       acc[0] = GV;
  //     });
  //   });
  // }
  // assert(res == v2);
  {
    CUfunction cuFunc;
    cuModuleGetFunction(&cuFunc, queue.cuModule, "kernel2");
    void *param[] = {&res, 0};
    cuLaunchKernel(cuFunc,
                   1, 1, 1, // grid
                   1, 1, 1,               // block
                   0,                     // sharedMem
                   queue.stream,
                   param, // void** kernelParams,
                   NULL   // void** extra
      );
    cuStreamSynchronize(queue.stream);
  }
  assert(*res == v2);

  // q.submit([&](sycl::handler h) {
  //   h.copy_to_symbol<MutableGV>(v1);
  // });
  cuMemcpyHtoDAsync(MutableGV, &v1, sizeof(int), queue.stream);
  cuStreamSynchronize(queue.stream);

  // auto ev = q.submit([&](sycl::handler h) {
  //   h.parallel_for([=] () mutable {
  //     MutableGV = v2;
  //   });
  // });
  // ev.wait();
  {
    CUfunction cuFunc;
    cuModuleGetFunction(&cuFunc, queue.cuModule, "kernel3");
    void *param[] = {&res, 0};
    cuLaunchKernel(cuFunc,
                   1, 1, 1, // grid
                   1, 1, 1,               // block
                   0,                     // sharedMem
                   queue.stream,
                   param, // void** kernelParams,
                   NULL   // void** extra
      );
    cuStreamSynchronize(queue.stream);
  }
  // assert(MutableGV == 21);
  // ====> no equivalent !

  // ev = q.submit([&](sycl::handler h) {
  //   // Copy the host symbol value using the device value
  //   h.update_host(MutableGV);
  // });
  // ev.wait();
  // assert(MutableGV == v2);
  int dev_state;
  cuMemcpyDtoHAsync(&dev_state, MutableGV, sizeof(int), queue.stream);
  cuStreamSynchronize(queue.stream);
  assert(dev_state == v2);

  // q.submit([&](sycl::handler h) {
  //   // Fill the global array with a given value
  //   h.fill<GVArray>(42);
  // });
  int tmp[42];
  std::fill(std::begin(tmp), std::end(tmp), 42);
  cuMemcpyHtoDAsync(GVArray, tmp, sizeof(int) * 42, queue.stream);
  cuStreamSynchronize(queue.stream);


  cuMemFree((CUdeviceptr)res);
}
```
