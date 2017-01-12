## Updating data on a specific context

The SYCL runtime can only see the requirements of a command group once
it is submitted to a queue. This implies that the user-code between two
SYCL command groups is blurring the view of the dependency graph the
SYCL runtime creates as the program is executed. In those situations,
where the time spent in the user thread between two command groups is
larger than the actual command group execution time, the runtime can not
perform optimizations to prepare the requisites of the next command
group until it is submitted. Although this is not relevant for execution
synchronization purposes, it can affect the performance if the actions
require memory operations.

For example, lets define CG_<sub>a</sub>{r<sub>read(bufA)</sub>, r<sub>read_write(bufB)</sub>} and
CG_<sub>b</sub>{r<sub>read(bufC)</sub>, r<sub>read_write(bufB)</sub>}.
Let *qA* and *qB* be 2 queues with context that execute on distinct
devices on separate contexts.
Listing [noupdate:req] shows a situation where two queue
submissions of command groups are separated among each other by some
user code and library calls. The developer cannot move the submissions of
command groups because the value used in buffer C is obtained from user
code that occurs before the submission of B. The SYCL runtime cannot see
that buffer B is going to be used on a separate context until the
command group is submitted, hence will not update the data on it until
the command group is submitted, potentially delaying the execution of
the second kernel.

```cpp
    ... user code ...
    qA.submit(cgA);
    ... user code ...
    ... some library call that gives the value of C...
    ... user code ...
    qB.submit(cgB);
```

Listing [update:req] shows how the developer uses the **prefetch**
method of the buffer to indicate to the SYCL runtime that the data will
be needed on a specific context. Note that this method requires a SYCL
context, but can optionally take a queue to simplify usage (and
potentially re-use the underlying OpenCL queue for the operations).

```cpp
    ... user code ...
    qA.submit(cgA);
    ... user code ...
    bufA.prefetch(qB);
    ... some library call that gives the value of C...
    ... user code ...
    qB.submit(cgB);
```

The **update_on** indicates to the runtime that a requirement to access
the memory object on a certain context will be defined in a later
command group. Depending on implementation decisions, the method will
trigger a copy operation, a DMA transfer or simply perform a no
operation. When later the command group is submitted, and the evaluation
of requisites is performed, the requisite of accessing bufA
will be satisfied so the kernel can be executed immediately.

Note that we avoid here the direct copy semantics (e.g, **std::copy**).
SYCL memory objects are *managed\_containers* that handle memory
operations internally. This heavily simplifies code encapsulation and
refactoring: Explicit copy operations require a particular origin and
destination of the copy, which directly enforce an execution flow of the data.
By avoiding specifying the origin of the data, we enable the runtime to,
for example, ignore the hint if the data is already on the context due
to a previous operation (that may be hidden on a third-party library).

This extension to the memory object syntax also facilitates interacting with
legacy code that relies on separate host and device codes, since we can now
simply replace explicit copy operations to the device by
update operations without interfering with the device execution.

It is possible also to create a command group _ad-hoc_ to request the prefetch
on the specific context (potentially via an specific queue or using some builtin
  hardware feature).

```cpp
q.submit([=](cl::sycl::handler& h) {
  auto accA = bufA.get_access<access::mode::read_write>(h);
  h.prefetch(accA)
})
```  

### API interface

New methods added to *cl::sycl::buffer* and *cl::sycl::image*:

| Method | Description |
|--------|-------------|
| *`void prefetch(cl::sycl::queue q)`* | The context associated with the queue _q_ will have the latest copy of the data available by the time the next command group using the memory object on the context is ready to begin execution. _q_ may be used by the SYCL runtime as the queue to perform the update |
| *`void prefetch(cl::sycl::context c)`* | The context _c_ will have the latest copy of the data available by the time the next command group using the memory object on the context is ready to begin execution. |

New method added to the *cl::sycl::handler*:

| Method | Description |
|--------|-------------|
| *`template <typename T, int Dim, typename Mode, typename Target> void prefetch(cl::sycl::accessor<T, Dim, Mode, Target> acc)`* | The runtime will prefetch the contents of the given accessor into the context associated with the queue. The accessor must be at least a read accessor. |
