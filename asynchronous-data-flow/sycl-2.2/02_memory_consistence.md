# Memory Consistence

The SYCL runtime guarantees there is a consistent view of the memory for
the various command groups on execution, including those submitted to
separate queues. This makes the SYCL runtime responsible of performing
those actions necessary to satisfy the memory requirements of the
kernels being executed in command groups. In OpenCL and, therefore,
SYCL, there is a non- unified view of the memory. This complicates the
execution of command groups on different devices, since manual
synchronization of memory must be performed in order to obtain the
correct result of the execution. For details on the SYCL and OpenCL
memory model, please refer to the appropriate specifications. Each
device in SYCL is associated with a context. The host and device do not
share global memory, except when using shared virtual memory, in which a
portion of the memory is visible between the host and a context. Devices
sharing the same context share the same view of the global memory,
whereas devices on separate context need to synchronize through the
host. Some devices offer system-level fine-grained shared virtual memory,
which makes every single memory object visible for all the devices
connected to the system. Note that this is very costly in terms of
hardware, and assumes a single vendor controls both host and device,
which is not the most common case.

## Command Group Execution Guarantees

In SYCL, the order of the command groups ensures a sequentially
consistent access to the memory on the devices. For example, lets
define two command groups CG<sub>a</sub>{bufA,bufB,bufC} and
CG<sub>b</sub>{bufA,bufB,bufD}, and two queues, qA and qB, in two different
devices in different contexts. If we submit CG<sub>a</sub> first to qA and
then CG<sub>b</sub> to qB, CG<sub>a</sub> must execute before CG<sub>b</sub>.
However, if we submit CG<sub>b</sub> first to qB and then CG<sub>a</sub> to qA,
CG<sub>b</sub> must execute before CG<sub>a</sub>. If both CG<sub>b</sub> and
CG<sub>a</sub> are submitted to qA, again CG<sub>b</sub> must execute before
CG<sub>b</sub>. Note that, although the requisites are the same in all cases,
the actions required to satisfy
them are very different: The SYCL runtime needs to perform the memory
consistency actions required to retain sequential consistence. This
creates a situation where only one context can have the latest updated
copy of the information (since there is no direct visibility of global
memory across context), hence, command groups submitted to queues in
separate contexts cannot execute simultaneously if they access the same
memory objects.

We can leverage this restrictions by considering the different *access modes*
of the memory object. SYCL defines six different access modes to memory objects
(Table 4.14 of SYCL 2.2 specification):

-   read (R): The kernel requests read-only access mode to the memory
    object

-   write (W): The kernel requests write-only access mode to the memory
    object

-   read_write (RW): The kernel requests read and write access to the
    memory object

-   discard_write (DW): The kernel requires write-only access to the
    memory object, and does not care about the contents that existed
    before.

-   discard\_read\_write (DRW): The kernel requires read-write access to
    the memory object, and does not care about the contents that existed
    before. [^1]

By using the access mode on the evaluation function of the requirements,
we can relax some of the restrictions as long as we keep the sequential
consistency.

Lets re-define the previous command groups by taking into account their
access modes: CG<sub>a</sub>{r<sub>read(bufA)</sub>,r<sub>read(bufB)</sub>,r<sub>read_write(bufC)</sub>} and
CG<sub>b</sub>{r<sub>read(bufA)</sub>,r<sub>read(bufB)</sub>,r<sub>read_write(bufD)</sub>},

In this case, even if both CG<sub>a</sub> and CG<sub>b</sub> are submitted to
different queues in different contexts, they can execute simultaneously,
since the only requirement is to read from the buffers shared among
them.

However, if we define CG<sub>a</sub>{r<sub>read(bufA)</sub>,r<sub>read_write(bufB)</sub>,r<sub>read_write(bufC)</sub>} and
CG<sub>b</sub>{r<sub>read(bufA)</sub>,r<sub>read_write(bufB)</sub>,r<sub>read_write(bufD)</sub>}, then CG<sub>b</sub> must be executed
after CG<sub>a</sub>, since it has a *read after write* dependency among them.

Note that if we define CG<sub>b</sub>{r<sub>read(bufA)</sub>,r<sub>discard_read_write(bufB)</sub>,r<sub>read_write(bufD)</sub>}, then the
*read after write* dependency is lifted (since we discard the contents
of the buffer) and both can be executed simultaneously.

## Combining multiple kernels on the same command group

Since we have defined strict rules about the ordering of command groups
and kernels, we can extend the existing interface to support multiple
kernels inside the same command group.

Synchronization requirements are shared between the multiple kernels in the
command group, even if data is only accessed from one of them: All requisites
for the command group must be satisfied before it is executed.

```cpp
    auto cgA = [=] (handler& h) {
      auto r1 = bufA.get_access<access::mode::read_write>(h);
      auto r2 = bufB.get_access<access::mode::read_write>(h);

      h.single_task<class kernelA>([=]() {
        r1[0] = r2[0];
      }

      h.single_task<class kernelB>([=]() {
        r1[0] = r2[1] + 1.0;
      }
      };
    qA.submit(cgA);
```

Note that this means it is not relevant where the accessor is specified
in a command group in terms of dependency, but it is relevant in terms
of the kernel arguments as shown below:

```cpp
    auto cgA = [=] (handler& h) {
      auto r1 = bufA.get_access<access::mode::read_write>(h);

      h.single_task<class kernelB>([=]() {
        r1[0] = r1[0] + 1.0;
      }

      auto r2 = bufB.get_access<access::mode::read_write>(h);

      h.single_task<class kernelA>([=]() {
        r1[0] = r2[1];
      }
      };
    qA.submit(cgA);
```

_kernelB_ uses only r<sub>1</sub>, thus, can be defined intermediately after the _r1_
accessor is created. The requirements for _kernelA_ include also _bufB_, thus,
we create the accessor _r2_. This ensures dependencies among command groups
are satisfied correctly.

## Shared Virtual Memory (SVM) and memory consistency

On certain OpenCL 2.2 platforms, Shared Virtual Memory (SVM) may be available.
When using SVM, the pointer used on the host and on the device(s) is shared.
Depending on the device capabilities, different features will be available.
The basic level of support is Coarse-grained buffer SVM, in which the
granularity of the sharing happens at the OpenCL buffer memory object.
The next level is Fine-grained buffer SVM, which enables individual loads and
stores within OpenCL buffer memory objects (and can optionally support atomics).
Finally, Fine-grained system SVM offers granularity of individual loads and
stores occurring anywhere within the host memory, with optional atomic support.
The latter does not require specific OpenCL memory object, only normal
system-wide allocations.

Although the usage of accessors to obtain a valid pointer in the kernel-function
is not mandatory in some of this combinations, the scheduling and data-flow
requirements still exist. For example, if a kernel-function requires certain
pointer to function, this pointer must contain the correct value before starting
the kernel.

However, it is true that, for fine-grained SVM modes, concurrent access to the
same memory allocation from the same pointer by various kernels is allowed.
This creates two separate  classes of memory requirements: *exclusive access* to a memory allocation or *concurrent access*.

**TODO: Update the API for SVM**

[^1]: Note that this access mode does not guarantee which value will be
    obtained when reading the accessor before doing any write!
