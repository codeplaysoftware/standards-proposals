| Proposal ID | TBC |
|-------------|--------|
| Name |  |
| Date of Creation | 16 January 2019 |
| Target | SYCL 1.2.1 extension |
| Current Status | _Work in progress_ |
| Reply-to | Victor Lomüller <victor@codeplay.com> |
| Original author | Victor Lomüller <victor@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Peter Zuzek <peter@codeplay.com> |
| Contributors | Victor Lomüller <victor@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Peter Zuzek <peter@codeplay.com> |

# interop_task: Improving SYCL-OpenCL Interoperability

## Motivation

OpenCL applications often build around a set of of fixed function operations which take OpenCL buffers in order to run a hard-coded OpenCL kernels. However, because SYCL does not allow a user to access cl_mem object out of an cl::sycl::accessor, it difficult/impossible to integrate such library into a SYCL application, as the only current way to do this is to create all OpenCL buffers up-front, which is not always possible.

This proposal introduces a way for a user to retrieve the OpenCL buffer associate with a SYCL buffer and enqueue a host task that can execute an arbitrary portion of host code within the SYCL runtime, therefore taking advantage of SYCL dependency analysis and scheduling.

## Enqueuing host tasks on SYCL queues

We introduce a new type of handler, the **codeplay::handler**, which includes a new
**interop\_task** method that executes a task on the host.
By submitting this command group to the SYCL device queue, we guarantee it is
executed in-order w.r.t the other command groups on the same queue.
Simultaneously, we guarantee that this operation is performed
asynchronously w.r.t to the user-thread (therefore, enabling the user
thread to continue submitting command groups).
Other command groups enqueued in the same or different queues
can be executed following the sequential consistency by guaranteeing the
satisfaction of the requisites of this command group.

The possibility of enqueuing host tasks on SYCL queues also enables the
runtime to perform further optimizations when available.
For example, a SYCL runtime may decide to map / unmap instead of copy operations,
or performing asynchronous transfers while data is being computed.

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
  /* "Manually" enqueue a kernel */
  template <typename FunctorT>
  void interop_task(FunctorT hostFunction);
};
}  // namespace codeplay
}  // namespace sycl
}  // namespace cl
```

### codeplay::handler::interop_task

The `interop_task` allow the user to execute a task of the native host.
Unlike `single_task`, `parallel_for` and `parallel_for_work_group`, the `interop_task` do not enqueue a kernel on the device but allow the user to execute a custom action when the prerequisites are satisfied on the device associate with the queue.
The functor passed to the `interop_task` takes as input a const reference to a `cl::sycl::codeplay::interop_handle` which can be used to retrieve underlying OpenCL objects relative to the execution of the task.

It is not allowed to allocate new SYCL object inside an `interop_task`.
It is the user responsibilities to ensure all asynchronous executions using SYCL provided resources finished before returning from the `interop_task`.

## Accessing Underlying OpenCL Object

We introduce the `interop_handle` class which provide access to underlying OpenCL objects during the execution of the `interop_task`.

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
    Returns the underlying cl_mem object associated with the accessor
  */
  template <typename dataT, int dimensions, access::mode accessmode,
            access::target accessTarget,
            access::placeholder isPlaceholder>
  cl_mem get(const accessor<dataT, dimensions, accessmode, access::target accessTarget, access::placeholder isPlaceholder>&) const;
};
}  // namespace codeplay
}  // namespace sycl
}  // namespace cl
```

`interop_handle` objects are immutable object whose purpose is to allow the user to access objects relevant to the context.

## Example using regular accessor

```cpp
    auto cgH = [=] (codeplay::handler& cgh) {
      auto accA = bufA.get_access<access::mode::read>(cgh);  // Get device accessor to SYCL buffer (cannot be dereference directly in interop_task).
      auto accB = bufB.get_access<access::mode::read_write>(cgh);

      h.interop_task([=](codeplay::interop_handle &handle) {
        third_party_api(handle.get_queue(), // Get the OpenCL command queue to use, can be the fallback
                        handle.get(accA), // Get the OpenCL mem object behind accA
                        handle.get(accB)); // Get the OpenCL mem object behind accB
        // Assumes call has finish when exiting the task
      });
    };
    qA.submit(cgH);
```
