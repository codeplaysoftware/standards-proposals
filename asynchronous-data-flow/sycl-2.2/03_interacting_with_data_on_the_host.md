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
(CG<sub>h</sub>{r<sub>read(bufA)</sub>,r<sub>read_write(bufB)</sub>}) that is
enqueued on a device queue but performs operations on the host.

We introduce a new type of handler, the **host\_handler**, which includes a new
**single\_task** method that executes a single task on the host. The
`host_handler` can be passed to any function that expects a `handler` parameter
(e.g. calling `get_access(handler)` on a buffer).
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

      h.single_task([=]() {
        accB[0] = accA[0] * std::rand();
      }

      auto accC = bufC.get_access<access::mode::read>(h);
      h.single_task([=]() {
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
  void single_task(FunctorT hostFunction);
};
}  // namespace sycl
}  // namespace cl
```

| Method | Description |
|--------|-------------|
| *`template <typename FunctorT> void single_task(FunctorT hostFunction)`* | The user-provided hostFunction will be executed once all requirements for the command group are met. The hostFunction must be a Callable object. |

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
      h.copy(hostPtr, accA);
    };
    qA.submit(cgH);
```

Note that the users can query the event returned by the submit to check if the
command group has finished (and, therefore, the host pointer has been updated).

Additionally, users may want to manually update the device data with host data.

```cpp
auto cgH = [=] (handler& h) {
  auto accA = bufA.get_access<access::mode::read_write>(h);
  h.copy(accA, hostPtr);
  h.parallel_for<class kernel>(range, SomeKernel(accA));
};
qA.submit(cgH);
```

### Updating data directly on the device

In the current SYCL specification there is no functionality to copy the 
contents of one SYCL buffer to another.
Using the copy method, the user can specify also two accessors as 
origin and destination, thus enabling the runtime to perform the copy 
in the most efficient way giving the current location of the data
in the system.

```cpp
auto cgH = [=] (handler& h) {
  auto accA = bufA.get_access<access::mode::read_write>(h);
  auto accB = bufB.get_access<access::mode::read_write>(h);
  h.copy(accB, accA);
};
qA.submit(cgH);
```

### Filling data on the device

A special case of updating the data directly on the device is when
a certain pattern is to be replicated across a range of values in
the accessor. 
In C++, this is implemented via the std::fill function in the 
algorithm headers.

The example shows how to fill buffer A contents with an scalar.
Note that at least write access is required.

```cpp
auto cgH = [=] (handler& h) {
  auto accA = bufA.get_access<access::mode::write>(h);
  h.fill(accB, 10);
};
qA.submit(cgH);
```

#### Access restrictions

The following restrictions apply to access mode and target of the accessor
provided to `copy`.

When a call to `copy` triggers a read from the device data, the
only valid accessor modes are the following:
* `access::mode::read`
* `access::mode::read_write`
* `access::mode::discard_read_write`

Similarly, when a call to `copy` triggers a write to the device
data, the only valid accessor modes are the following:
* `access::mode::write`
* `access::mode::read_write`
* `access::mode::discard_write`
* `access::mode::discard_read_write`

#### API changes

| Method | Description |
|--------|-------------|
| `template <typename T, int dims, access::mode accessMode, access::target accessTarget> void copy(shared_ptr<T> hostPtr, accessor<T, dims, accessMode, accessTarget> acc)`  | Update the contents of the host pointer with the data in accessor `acc`. `hostPtr` must have enough space allocated to hold the data. |
| `template <typename T, int dims, access::mode accessMode, access::target accessTarget> void copy(accessor<T, dims, accessMode, accessTarget> acc, shared_ptr<T> hostPtr)` | Update the the data in accessor `acc` with the contents of the host pointer. `hostPtr` must have enough space allocated to hold the data. |
| `template <typename AccessorD, typename AccessorO> void copy(AccessorD acc, AccessorO acc)` | Update the the data in accessor `accD` with the contents of the buffer pointed by `accO` |
| `template <typename AccessorD, typename T> void fill(AccessorD acc, T val)` | Special case of copy from host to device where the origin is a scalar value that will be replicated across the range of the accessor.  |
