## Interacting with data on the host

Currently, the only possible way of directly interacting with device
data from the host is via a host accessor. However, host accessors
provide synchronous access to the data: The constructor of a host
accessor blocks access to the memory object, and no further command
groups can be submitted until the host accessor is destroyed. This
blocks the SYCL runtime to continue scheduling command groups, creating
a bottleneck that disables some optimizations, such as concurrent copy
and kernel execution. It also limits the possibility of users to asynchronously
retrieve data from the device.

By using the *requirements* and *actions* definitions,
we can define additional operations on the command group that
enable accessing SYCL memory objects from the host.

### Enqueuing host tasks on SYCL queues

Listing [hostcg:req] shows a command group
(CG<sub>h</sub>{r<sub>read(bufA)</sub>,r<sub>read_write(bufB)</sub>}) that it is enqueued on a device queue but
performs operations on the host.
We introduce a new type of handler, the **host\_handler**, which includes a new
**host\_task** method that executes a task on the host. The `host_handler` can
be passed to any function that expects a `handler` parameter (e.g. calling
`get_access(handler)` on a buffer).
By submitting this command group to the SYCL device queue, we guarantee it is
executed in-order w.r.t the other command groups on the same queue.
Simultaneously, we guarantee that this operation is performed
asynchronously w.r.t to the user-thread (therefore, enabling the user
thread to continue submitting command groups).
Other command groups enqueued in the same or different queues
can be executed following the sequential consistency by guaranteeing the
satisfaction of the requisites of this command group.

The possibility of enqueuing host task on SYCL queues also enables the
runtime to perform further optimizations when available.
For example, a SYCL runtime may decide to map / unmap instead of copy operations,
or  performing asynchronous transfers while data is being computed.

It is possible to run multiple host tasks in the same command group - the host
tasks are executed in-order within the command group. It is not possible to use
host tasks and kernel invocations (e.g. `parallel_for`) in the same command
group. This restriction is enforced by the use of `host_handler`, which does not
include kernel invocation methods.

```cpp
    auto cgH = [=] (host_handler& h) {
      auto accA = bufA.get_access<access::mode::read>(h);
      auto accB = bufB.get_access<access::mode::read_write>(h);

      h.host_task([=]() {
        accB[0] = accA[0] * std::rand();
      }

      auto accC = bufC.get_access<access::mode::read>(h);
      h.host_task([=]() {
        accC[0] += accA[0] * accB[0];
      }
    };
    qA.submit(cgH);
```

Note that in the code above `accC` was created after the first host task has
been scheduled, but the actual data may already be available before the first
host task starts executing. In the second host task, the value of `accB` is
consistent with the value set in the first host task.

#### API changes

```cpp
namespace cl {
namespace sycl {

class host_handler {
 private:
  // implementation defined constructor
  host_handler(__unspecified__);

 public:
  template <typename FunctorT>
  void host_task(FunctorT hostFunction);
};
}  // namespace sycl
}  // namespace cl
```

| Method | Description |
|--------|-------------|
| *`template <typename FunctorT> void host_task(FunctorT hostFunction)`* | The user-provided hostFunction will be executed once all requirements for the command group are met. The hostFunction must be a Callable object. |

### Updating data on the host or the device from SYCL queues

A specific use-case for a host task is to simply update some data on
the host after it has been modified on the device.
Currently in SYCL, the only opportunity of accessing data on the host is using
host accessors or destroying the buffer (to update the final pointer).
However, in some cases developers want to update some pointer on the host
without blocking the execution.

```cpp
    auto cgH = [=] (handler& h) {
      auto accA = bufA.get_access<access::mode::read>(h);
      h.parallel_for<class kernel>(range, SomeKernel(accA));
      h.update_from_device(accA, hostPtr);
      };
    qA.submit(cgH);
```

Note that the users can query the event returned by the submit to check if the
command group has finished (and, therefore, the host pointer has been updated).

#### API changes

| Method | Description |
|--------|-------------|
| `cpp template<typename AccessorT, typename T> void update_from_device(AccessorT acc)`  | Updates the pointer associated with the buffer or image on the host. |
| `cpp template<typename AccessorT, typename T> void update_from_device(AccessorT acc, shared_ptr<T> hostPtr)`  | Update the contents of the host pointer with the data in accessor _acc_. _hostPtr_ must have enough space allocated to hold the data. |
| `template<typename AccessorT, typename OutputIterator> void update_from_device(AccessorT acc, OutputIterator ot)`  | Write the contents of the memory pointed by _acc_ into the OutputIterator _ot_  |
