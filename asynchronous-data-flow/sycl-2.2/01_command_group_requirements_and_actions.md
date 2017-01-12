## Command group requirements and actions

In this section, we extend the definition of an _accessor in a command group_
to be composed into two concepts; the **requisite** and the **action**. Furthermore
by generalizing the concept of accessor, this proposal aims to extend the
ability of the SYCL runtime to integrate with other synchronization
mechanisms that are not limited to memory operations.

### New definition of the **Command Group**

A **requisite** (r<sub>i</sub>) is a requirement that must be fulfilled for
one or more kernel-functions (k<sub>i</sub>) to be executed on a particular device.
For example, a requirement may be that certain data is available on a device,
or that some other task has completed.
A SYCL **Command Group** (CG) contains a set of requisites (**R**) and a set of
kernel-functions (K).
A **Command Group** is _submitted_ to a queue when using the
 _cl::sycl::queue::submit_ method.
An implementation may evaluate the requirements of a command group at any
point in time after it has been submitted.
We define the _processing of a command group_ as the process by which the
runtime evaluates all requirements in **R**.
The SYCL runtime will execute K only when all r<sub>i</sub> in **R** are
 _satisfied_ (when all requisites are satisfied).
To simplify the notation, in the following sections we will refer to the set
of requirements of a command group named _foo_ as
*CG<sub>foo</sub> = {r<sub>0</sub>, ..., r<sub>n</sub>}*.

The *evaluation of a requisite* (Eval(r<sub>i</sub>)) returns the status of the
requisite, which can be either _satisfied_ or _not satisfied_.
A _satisfied_ requisite implies the requirement is met.
Eval(r<sub>i</sub>) never alters the requisite, only observe the current status.
The implementation may not block to check the requisite.

An **action** (a<sub>i</sub>) is a collection of implementation-defined operations that must be performed in order to satisfy a requisite.
The _set of actions for a given CG_ (A) is permitted to be empty if no operation
is required.
The notation *a<sub>i</sub>* represents the action required to satisfy
r<sub>i</sub>.
Actions of different requisites can be satisfied in any order w.r.t each other
without side effects (i.e, given two requirements r<sub>i</sub> and r<sub>k</sub>, r<sub>i</sub>, r<sub>k</sub> == r<sub>k</sub>,r<sub>j</sub>)
performed.
The intersection of two actions is not necessarily empty.

*Performing an action* (Perform(a<sub>i</sub>)) execute all operations of
an action in order to satisfy the requisite r<sub>i</sub>.
Note that, after _Perform(a<sub>i</sub>)_, the evaluation _Eval(r<sub>i</sub>)_
will indicate the requirement is satisfied.
Actions of different requisites can be satisfied in any order w.r.t each other
without side effects.
i.e, given two requirements _r<sub>j</sub>_ and _r<sub>k</sub>_,
_Perform(a<sub>j</sub>)_ followed of _Perform(a<sub>k</sub>)_ is
equivalent to _Perform(a<sub>k</sub>)_ followed of _Perform(a<sub>j</sub>)_.

### Accessors as requisites

The most common requisite in SYCL is requiring access to a memory object (a
**buffer** or **image** object) on a particular device.
This requisite is represented by the **accessor** class. When an
accessor is constructed inside a CG with the associated command group handler,
a requisite for accessing the associated memory object is added to the CG
with the chosen access mode.

For example, the code in the listing below creates an accessor (**acc**)
to a memory object **bufA** that is later used within **kernelA**.

When the CG is submitted to a queue, this creates a requirement
for **kernelA** to access **bufA** on write mode on the device associated
with the queue.
The  (CG<sub>1</sub>{bufA}).

This *requirement* can be evaluated independently (for
example, by checking if the memory object is available on the context
which contains the device) from the kernel-function execution.

The *action* associated will vary depending on the SYCL implementation
decisions and where the latest updated copy of the data resides. For example,
if the latest placement of the data was on the host, the action
performed by the runtime could be to copy memory from the host to the
device associated with the queue. However, a different SYCL
implementation may decide to use a mapping operation to perform the same
action.
Note that the contents of the *action* are irrelevant, but an *action* must be
executed for a *requirement* to be satisfied.
In this case, independently of what the actual implementation
of the action happens to be, it must satisfies the requirement of
accessing the memory object on the context associated with the device.
If this action is not performed before the kernel is executed on the
device, the result of executing the kernel may be invalid.
A special case would be if the data required for the kernel-function to be
executed on the device is already present.
In this case, the empty action will be performed.

```cpp
    auto cg1 = [=] (handler& h) {
      accessor<float, 1, access::mode::write, access::target::global_buffer> acc(bufA, h);

      h.single_task<class kernelA>([=]() {
        acc[0] = 33.0;
      }
      };
    myQueue.submit(cg1);
```

Details on this particular requisite to memory objects are provided
later on Section [mem:sync].

## Host accessors as requirements

The SYCL specification provides users of host accessors to gain access
to the runtime-managed data on the host.
A host accessor creates a requirement for
the host.  The Listing [host:req] shows a host accessor that creates a requisite of
accessing bufferA on the host.

```cpp
    {
      accessor<float, 1, access::mode::write, access::target::host_buffer> hostAcc(bufA);
      for (i = 0; i < bufA.get_size(); i++) {
        do_something(hostAcc[0]);
      }
    }
```

## Events as requisites

SYCL 2.2 specification does not offer the possibility for command group
to interact with OpenCL (or SYCL) events (e.g, a command group cannot
start executing until an OpenCL event has finished).
SYCL events cannot be used for anything but synchronizing host
operations, as there is no command group ability to enqueue a command
that requires an event to be completed (whereas this is possible in
OpenCL).
By using the definitions of *requirements* and *actions*, we can expand the
functionality of the command group to cover this case.

Lets assume we have a command group A (CG<sub>a</sub>) that cannot start
executing until a certain event *E* has been completed (status is
CL\_COMPLETE (OpenCL) or command\_end (SYCL)).
We can define a
requirement R<sub>1</sub> that is satisfied if and only if the
status of *E* is completed.
The runtime can *evaluate* the requirement without affecting the event by
using the `get_info<info::command_execution_status>()` method of the
event.
The SYCL runtime can observe if the command group is available for execution
without affecting the status of the event, and can potentially execute
other command groups that have all the requisites satisfied.
Listing [event:req] shows an example of a command group that waits for an event
before executing. We have extended the handler interface with a
*require\_event* that takes a SYCL event as a parameter. Note that
SYCL events can be constructed from OpenCL events.

```cpp
    auto cgA = [=] (handler& h) {
      h.wait_for(syclEvent);
      auto acc = bufA.get_access<access::mode::write>(h);
      h.single_task<class kernelA>([=]() {
        acc[0] = 33.0;
      }
      };
    qA.submit(cgA);
```

#### API changes

The *handler* class gains a new method, *wait_for*, that can take a SYCL event.

| Method | Description |
|--------|-------------|
| *`void wait_for(cl::sycl::event syclEvent)`* | Indicates that the command group must wait for this event to be completed before executing the kernel. |

### C++ futures as requisites

C++ asynchronous operations use the class *std::future* to
represent the return of a value obtained from an asynchronous
operation. Developers can use futures as *placeholders* for the expected
value of a function. The value will only be retrieved when calling the
*get()* method of the future. The interface also offers the
possibility of waiting for the result without retrieving it. Note that an
*std::future* can also encapsulate an exception, which will be thrown
when getting the result. Currently, the SYCL 1.2 or SYCL 2.2 offers no
functionality for users to combine Command Groups with C++ futures.

A C++ future can be seen as a requisite to have the packed value available
for the kernel execution. The evaluation of the requisite is
the method that waits for a limited time (**wait\_until** or
**wait\_for**) to check if the value is available. The action can later
be getting the value from the future, and setting it as a scalar kernel
argument.

The Listing [future:req] shows an example, where a command group waits
for a future to be available and then performs an operation using the
value inside of the kernel (CG<sub>a</sub>{R<sub>1</sub>,R<sub>2</sub>}).
In this case, we have use the method **wait_for** of the handler to capture
the future into the runtime.


```cpp
    std::future<int> myFuture = std::async(
                                  std::launch::async, [=](){ return 8;});

    auto cgA = [=] (handler& h) {
      auto r1 = h.wait_for(std::move(myFuture));
      auto r2 = bufA.get_access<access::mode::write>(h);

      h.single_task<class kernelA>([=]() {
        r2[0] = r1;
      }
      };
    qA.submit(cgA);
```

Note that current type restrictions in the SYCL specification
still apply, therefore, the value type returned by the future
must be standard layout.

If an exception is thrown when getting the value of the future, the
runtime can abort the execution of the command-group and redirect the error to
the sycl **async\_handler**. Note that fallback execution is not triggered
in this case, since requirements for execution are not met.

#### API changes

The *handler* class gains a new method, *wait_for*, that can take a C++ future.

| Method | Description |
|--------|-------------|
| *`T wait_for(std::future<T>&& cppFuture)`* | The SYCL runtime will wait for the value of the future to be retrieved to execute the kernel. The value retrieved from the future can be used inside the kernel. Rules for passing arguments to kernel functions apply. In case of exception while retrieving the future, the runtime re-throws the exception. Note that fallback is not triggered in this case. |
