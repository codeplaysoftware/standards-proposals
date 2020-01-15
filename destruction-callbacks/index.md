| Proposal ID | CP023 |
|-------------|--------|
| Name | SYCL Context Destruction Callback |
| Date of Creation | 23 January 2020 |
| Target | SYCL Next |
| Current Status | _Work in progress_ |
| Reply-to | Stuart Adams <stuart.adams@codeplay.com> |
| Original author | Stuart Adams <stuart.adams@codeplay.com> |
| Contributors | Stuart Adams <stuart.adams@codeplay.com> |

# SYCL Context Destruction Callback

## Motivation

[SYCL's new interoperability features](https://github.com/KhronosGroup/SYCL-Shared/blob/master/proposals/sycl_generalization.md) enable it to be integrated into legacy applications. 
A developer can use the `get_native` function to access an object's native handle for 
use in existing code. However, developers currently have no way to bind the lifetime of 
non-SYCL state to the lifetime of their SYCL contexts, making the adoption of SYCL in 
legacy codebases difficult.

## Overview

Several problems can arise upon the destruction of the application's `sycl::context`. 
All native handles that were related to the context are invalidated, and the application 
must release any resources it has allocated to store them. Further issues can arise if 
the native handles exhibit different behavior than their SYCL counterparts, for instance, 
if a native context is thread-bound or globally accessible. While it is possible to write an 
ad-hoc solution, the SYCL standard does not mandate when a `sycl::context` is 
destroyed, making it difficult to implement a solution that works across different SYCL 
implementations.

This proposal suggests that the `sycl::context` class be extended. A new member 
function, `set_destruction_callback` is described. Developers will use this function to 
register a callback, allowing them to respond to the destruction of the context in a way 
that is appropriate for their application.

## Examples

### CUDA Libraries

This code sample demonstrates interop with an imaginary legacy codebase, LegacyLib, 
which uses CUDA to offload expensive computation to the GPU. LegacyLibHandle_t 
is a pointer type to an opaque structure holding the LegacyLib library context, which is 
itself bound to a specific CUDA context. This style of API can be seen in [cuDNN](https://docs.nvidia.com/deeplearning/sdk/cudnn-api/index.html#cudnnHandle_t) and [cuBLAS](https://docs.nvidia.com/cuda/cublas/index.html#cublashandle_t).
Different approaches to this solution already exist in the landscape of heterogeneous programming [1][2][3][4][5].

```cpp
#include <SYCL/sycl.hpp>
#include <unordered_map>
#include "LegacyLib.h"

class handle_map {
public:
  static LegacyLibHandle_t get_lib_handle(CUcontext context) {
    auto it = data.find(context);

    if(it != data.end()) {
      LegacyLibHandle_t handle = it->second;
      return handle;
    }
    
    LegacyLibHandle_t handle;
    legacyLibCreate(&handle);
    data[context] = handle;
    return handle;
  }

  static void destroy_lib_handle(CUcontext context) {
    auto it = data.find(context);

    if(it != data.end()) {
      LegacyLibHandle_t handle = it->second;
      legacyLibDestroy(handle);
      data.erase(it);
    }
  }

private:
  static std::unordered_map<CUcontext, LegacyLibHandle_t> data;
};

void runA(const sycl::queue& queue)
{
  CUstream stream = get_native<sycl::backend::cuda>(queue);
  CUcontext context = get_native<sycl::backend::cuda>(queue.get_context());
  LegacyLibHandle_t handle = handle_map::get_lib_handle(context);

  legacyLibSetStream(handle, stream);
  legacyLibRunA(handle);
}

void runB(const sycl::queue& queue);
void runC(const sycl::queue& queue);

int main() {
  sycl::gpu_selector selector;

  // run on multiple threads:
  {
    sycl::queue queue(selector);
    runA(queue);
    runB(queue);
    runC(queue);
  }
}
```

With this technique, a global mapping of contexts to the lib handles is established. 
The application can use this to access the correct handle by passing in the active 
context. If we ignore the issue of thread-safe access to the map, a few problems 
remain. In a complex multithreaded application with multiple sycl contexts, it will 
become difficult ensure the correctness of an application with this design. The 
map will grow for every context used, and the library handles will not be released. 
Given `CUcontext` objects are thread-bound, it is not clear how to call
`handle_map::destroy_lib_handle` and free the legacy lib objects when they are 
no longer in use. If a `sycl::context` is destroyed before the data is removed from
the map, it will contain invalid data. Despite these issues, this technique can be seen 
in a surprising number of libraries.

In a SYCL application, the developer cannot reliably know when the SYCL runtime will
destroy a context and invalidate the handles. Ideally, all the resources and handles 
reliant on a `sycl::context` object should be destroyed once its destructor is called. 
To achieve this, the class must be extended to allow developers to register a callback.

```cpp
int main() {
  sycl::gpu_selector selector;

  // run on multiple threads:
  {
    sycl::queue queue(selector);
    sycl::context context = queue.get_context();

    context.set_destruction_callback([](const sycl::context& this_context) {
      CUcontext context = get_native<sycl::backend::cuda>(this_context);
      handle_map::destroy_lib_handle(context);
    });

    runA(queue);
    runB(queue);
    runC(queue);
  }
}
```

With this new version, all state related to the `sycl::context` object is cleaned up 
automatically when its destructor is called.

Similar problems exist in other libraries and APIs in the heterogeneous programming
ecosystem, requiring the user to explicitly release handles that have been passed to 
an existing library. The ```set_destruction_callback``` function will also be useful in these cases.

### ArrayFire
The ArrayFire GPGPU library provides functions to enable the use of user-defined OpenCL 
contexts for internal operations. The user is responsible for calling `afcl_add_device_context` 
and `afcl_delete_device_context` [6]. The developer must ensure that `afcl_delete_device_context` 
is called before the runtime destroys the `sycl::context`.

### OpenVX
OpenVX defines an extension for interoperability with OpenCL, defining the `vxCreateContextFromCL` 
function to create an OpenVX context from a user-generated OpenCL context and command queue [7]. 
If this function is used then the developer must ensure that `vxReleaseContext` is called before 
the runtime destroys the `sycl::context`.

### DNNL
DNNL can enable  OpenCL interop through the `dnnl_engine_create_ocl` function, which allows the
developer to create an execution engine from an existing OpenCL context and device [8]. If this function 
is used then the developer must ensure that `mkldnn_engine_destroy` is called before the runtime 
destroys the `sycl::context`.

## Specification Changes
4.9.3.1 Context interface
```cpp
namespace sycl {
  class context {
  public:
    template<typename F = std::nullptr_t>
    void set_destruction_callback(F&& callback = nullptr);
  };
}
```

| Member function | Description |
|-------------|--------|
| `template<typename F = std::nullptr_t> void set_destruction_callback(F&& callback = nullptr);` | Registers a callable object, `callback`, with the context. The callable object will be invoked once *immediately before* the native context is destroyed. `F` must be a callable type with the signature `void(const sycl::context&)`. It must be well-formed for a `sycl::context` destructor to call the callback using the form `callback(*this);`. Only one callback may be registered - subsequent calls to this member function will overwrite the previously registered callback. If `F` is `std::nullptr_t`, no callback is registered and any previous callback is destroyed. It is undefined behavior if an instance of any SYCL class with reference semantics (see 4.6.2) is stored in a function object, or captured in the closure of a lambda that is used as a callback.
<center>Table 4.15: Member functions of the context class</center>

##References

[1] JuliaGPU: https://github.com/JuliaGPU/CuArrays.jl/blob/4dedd0fadcf260cf008a9e73d8702e2f259b2cfc/src/blas/CUBLAS.jl#L30
[2] Intel ISAAC: https://github.com/intel/isaac/blob/b0a265ee45337f92f1e8e9f2fb08a057292b0240/lib/driver/dispatch.cpp#L223
[3] ISAAC: https://github.com/ptillet/isaac/blob/8ea6498a841fe1e63a4518797d332f538a9ba37e/lib/driver/dispatch.cpp#L216
[4] ND4J: https://github.com/deeplearning4j/nd4j/blob/8f005bcecb240d1fbb83b9d390ad801d1d3b6933/nd4j-backends/nd4j-backend-impls/nd4j-cuda/src/main/java/org/nd4j/jita/allocator/context/impl/BasicContextPool.java#L43
[5] GADGETRON: https://github.com/gadgetron/gadgetron/blob/fa39a340558a032bea10db07af00fbe6da5ff4cb/toolboxes/core/gpu/CUBLASContextProvider.cpp#L35
[6] ArrayFire: http://arrayfire.org/docs/group__opencl__mat.htm#ga37969cfa49416bbdb25910d15c454d01
[7] OpenVX: https://www.khronos.org/registry/OpenVX/extensions/vx_khr_opencl_interop/1.0/vx_khr_opencl_interop_1_0.html#_vxcreatecontextfromcl
[8] DNNL: https://github.com/intel/mkl-dnn/blob/master/doc/advanced/opencl_interoperability.md#c-api-extensions-for-interoperability-with-opencl-1