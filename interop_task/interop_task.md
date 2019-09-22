| Proposal ID | CP021 |
|-------------|--------|
| Name | Interop Task |
| Date of Creation | 16 January 2019 |
| Target | Vendor extension |
| Current Status | _Availalable since CE 1.0.5_ |
| Reply-to | Victor Lomüller <victor@codeplay.com> |
| Original author | Victor Lomüller <victor@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Peter Zuzek <peter@codeplay.com> |
| Contributors | Victor Lomüller <victor@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Peter Zuzek <peter@codeplay.com> |

# interop_task: Improving SYCL-OpenCL Interoperability

## Motivation

SYCL does not allow a user to access cl_mem object out of an cl::sycl::accessor, it is difficult to integrate low-level API functionality inside the data-flow execution model of SYCL, as the only current way to do this is to create all OpenCL buffers up-front, which is not always possible.

This proposal introduces a way for a user to retrieve the low-level objects associated with SYCL buffers and enqueue a host task that can execute an arbitrary portion of host code within the SYCL runtime, therefore taking advantage of SYCL dependency analysis and scheduling.

## Accessing low-level API functionality on SYCL queues

We introduce a new type of handler, the **codeplay::handler**, which includes a new
**interop\_task** method that enables submission of low-level API code from the host.
By submitting this command group to the SYCL device queue, we guarantee it is
executed in-order w.r.t the other command groups on the same queue.
Simultaneously, we guarantee that this operation is performed
asynchronously w.r.t to the user-thread (therefore, enabling the user
thread to continue submitting command groups).
Other command groups enqueued in the same or different queues
can be executed following the sequential consistency by guaranteeing the
satisfaction of the requisites of this command group.
It is the user's responsibility to ensure the lambda submitted via interop_task does not create race conditions with other command groups or with the host.

The possibility of enqueuing host tasks on SYCL queues also enables the
runtime to perform further optimizations when available.
For example, a SYCL runtime may decide to map / unmap instead of performing copy operations,
or perform asynchronous transfers while data is being computed.

### cl::sycl::codeplay::handler

```cpp
namespace cl {
namespace sycl {
namespace codeplay {

class handler : public cl::sycl::handler {
 private:
  // implementation defined constructor
  handler(__unspecified__);

 public:
  /* Submit a task with interoperability statements. */
  template <typename FunctorT>
  void interop_task(FunctorT hostFunction);
};
}  // namespace codeplay
}  // namespace sycl
}  // namespace cl
```

### codeplay::handler::interop_task

The `interop_task` allows users to submit tasks containing C++ statements with low-level API calls (e.g. OpenCL Host API entries).
The command group that encapsulates the task will execute following the usual SYCL dataflow execution rules.
The functor passed to the `interop_task` takes as input a const reference to a `cl::sycl::codeplay::interop_handle`. The handle can be used to retrieve underlying OpenCL objects relative to the execution of the task.

It is not allowed to allocate new SYCL object inside an `interop_task`.
It is the user's responsibility to ensure that all operations performed inside the `interop_task` are finished before returning from it.

Although the statements inside the lambda submitted to the `interop_task` are executed on the host, the requirements and actions for the command group are satisied for the device.
This is the opposite of the `host_handler` vendor extension, where requisites are satisfied for the host since the statements on the lambda submited to the single task are meant to have side effects on the host only.
The interop task lambda can have side effects on the host, but it is the programmer responsability to ensure requirements dont need to be satisfied for the host.

## Accessing low-level API objects

We introduce the `interop_handle` class which provide access to underlying OpenCL objects during the execution of the `interop_task`.
`interop_handle` objects are immutable objects whose purpose is to enable users access to low-level API functionality.

The interface of the `interop_handle` is defined as follow:
```cpp
namespace cl {
namespace sycl {
namespace codeplay {

class interop_handle {
 private:
  // implementation defined constructor
  interop_handle(__unspecified__);

 public:
  /* Return the context */
  cl_context get_context() const;

  /* Return the device id */
  cl_device_id get_device() const;

  /* Return the command queue associated with this task */
  cl_command_queue get_queue() const;

  /*
    Returns the underlying cl_mem object associated with a given accessor
  */
  template <typename dataT, int dimensions, access::mode accessmode,
            access::target accessTarget,
            access::placeholder isPlaceholder>
  cl_mem get_buffer(const accessor<dataT, dimensions, accessmode, access::target accessTarget, access::placeholder isPlaceholder>&) const;
};
}  // namespace codeplay
}  // namespace sycl
}  // namespace cl
```

## Example using regular accessor

```cpp
    auto cgH = [=] (codeplay::handler& cgh) {
      // Get device accessor to SYCL buffer (cannot be dereferenced directly in interop_task).
      auto accA = bufA.get_access<access::mode::read>(cgh);
      auto accB = bufB.get_access<access::mode::read_write>(cgh);

      h.interop_task([=](codeplay::interop_handle &handle) {
        third_party_api(handle.get_queue(), // Get the OpenCL command queue to use, can be the fallback
                        handle.get_buffer(accA), // Get the OpenCL mem object behind accA
                        handle.get_buffer(accB)); // Get the OpenCL mem object behind accB
        // Assumes call has finish when exiting the task
      });
    };
    qA.submit(cgH);
```

This example calls the clFFT library from SYCL using the `interop_task`:
```cpp
#include <stdlib.h>
#include <CL/sycl.hpp>

/* No need to explicitly include the OpenCL headers */
#include <clFFT.h>

int main( void )
{
  size_t N = 16;

  cl::sycl::queue device_queue;
  cl::sycl::buffer<float> X(range<1>(N * 2));

  /* Setup clFFT. */
  clfftSetupData fftSetup;
  err = clfftInitSetupData(&fftSetup);
  err = clfftSetup(&fftSetup);

  device_queue.submit([=](codeplay::handler& cgh) {
    auto X_accessor = X.get_access<access::mode::read_write>(cgh);
    h.interop_task([=](codeplay::interop_handle &handle) {
      /* FFT library related declarations */
      clfftPlanHandle planHandle;
      size_t clLengths[1] = {N};

      /* Create a default plan for a complex FFT. */
      err = clfftCreateDefaultPlan(&planHandle, handle.get_context(), CLFFT_1D, clLengths);

      /* Set plan parameters. */
      err = clfftSetPlanPrecision(planHandle, CLFFT_SINGLE);
      err = clfftSetLayout(planHandle, CLFFT_COMPLEX_INTERLEAVED, CLFFT_COMPLEX_INTERLEAVED);
      err = clfftSetResultLocation(planHandle, CLFFT_INPLACE);

      /* Bake the plan. */
      err = clfftBakePlan(planHandle, 1, &queue, NULL, NULL);

      /* Execute the plan. */
      cl_command_queue queue = handle.get_queue();
      cl_mem X_mem = handle.get_buffer(X_accessor);
      err = clfftEnqueueTransform(planHandle, CLFFT_FORWARD,
                                  1, &queue, 0, NULL, NULL,
                                  &X_mem, NULL, NULL);

      /* Wait for calculations to finish. */
      err = clFinish(queue);

      /* Release the plan. */
      err = clfftDestroyPlan( &planHandle );
    });
  });

  /* Release clFFT library. */
  clfftTeardown( );

  return 0;
}
```
