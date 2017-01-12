**Document number: PXXXXR0**

**Date: 2016-06-22**

**Project: Programming Language C++, SG14, SG1**

**Authors: Michael Wong, Gordon Brown**

**Emails: michael@codeplay.com, gordon@codeplay.com**

**Reply to: michael@codeplay.com, gordon@codeplay.com**

# Managed Container #

## TODO: ##

* Decide how to integrate with executors proposal.
* Decide how to incorporate shared virtual memory.
* Remove the implementation defined S parameter on the temporal class and update aliases.
* Add considerations for motivations
* Shared source
* Data dependencies
* Address spaces
* Interaction  with array_view for access
* Integration with executors for consistency support and allocation
* Add future work consideration for dimensionalities higher than 3
* Should a managed_view synchronise on destruction?
* Inter-op-ability between implementations.
* Mention hierarchy of memory regions as future work.
* Mention the relationship between data movement and compute and the requirement for them to be coupled.
* Add heterogeneous memory model to future work section.
* Should a temporal object allow non-blocking access to the memory region it owns?
* Should a temporal object allow dynamically adding or removing elements from on the CPU, this would require synchronisation.
* Should a temporal support iterators?
* How to integrate with allocators?
* Should a temporal support allocators, how would this work for data that is not allocated?
* Also have a synchronise member function on the continuation executor.

## Wording to incorporate: ##

**
For single-compiler heterogeneous compiler solutions the P parameter can be used to allow code generation to construct symbols for different ABIs to avoid conflict between the generated code of the different heterogeneous architectures. For multi-compiler heterogeneous compiler solution the P parameter is not necessary.
**

**
As a note it is also important to consider heterogeneous systems which have a system-level shared memory, in this situation memory is accessed on both the CPU and heterogeneous devices via a shared virtual address space providing fine grained cache coherency allowing a memory region to be modified by the CPU and multiple heterogeneous devices concurrently. This idea will be continued later in this paper when discussing the integration of the temporal class with array_views.
**

## 1. Summary ##

This paper proposes an addition to the C\+\+ standard library to facilitate the management of a memory allocation which can exist across the memory region of the host CPU and the memory region(s) of one or more remote heterogeneous or distributed devices.

## 2. Introduction ##

### Motivation ###

In contrast to non-heterogeneous or distributed systems where there is typically a single device; a host CPU with a single memory region, heterogeneous and distributed systems have multiple devices (including the host CPU) with their own discrete memory regions. This introduces additional complexity for accessing a memory allocation that is not present in any existing containers in C\+\+, the requirement that said memory allocation be available in one of many memory regions within a system at any given time. For the purposes of this paper we will refer to such a memory allocation as a virtual memory allocation as it describes memory allocated across multiple memory regions. This raises the possibility that a virtual memory region may not have the most recently modified copy data, therefore requiring synchronisation to update it.

Due to varying data movement latencies, the performance of heterogeneous and distributed systems is often overwhelmingly dominated by efficient locality and movement of data. This means it is not enough for a virtual memory allocation to simply be available in one or more memory regions at any one time, it’s also important to know how that virtual memory allocation is being accessed; i.e. is it being simply observed (read-only) or is it being modified (read-write). Knowing this can allow for runtime scheduling and optimizations which can reduce unnecessary and often performance negating data movement. In order to allow for this distinction there must also be a strict consistency model which defines where a virtual memory allocation can be observed and modified in order to prevent race conditions between the memory accesses on different devices.

### Influence ###

This proposal is heavily influenced by the SYCL specification [1] and the work that was done in defining it, this was largely due to the intention of the SYCL specification to define a standard that was entirely standard C\+\+ and as in line with the direction of C\+\+ as possible. However the features suggested in this proposal are not directly derived from the SYCL specification, they merely lent inspiration. The suggested approach in this proposal aims to solve the problems of heterogeneous and distributed data movement in C\+\+ for a wide range of systems.

This approach is also heavily influenced by the proposal for a unified interface for execution [2] and for polymorphic multidimensional array views [3] as the interface proposed in this paper interacts closely with these.

**TODO(Gordon): Add additional influences from other programming models**

### Scope of this Paper ###

There are some additional important considerations when looking at a container for data movement in heterogeneous and distributed systems. In order to prevent this paper from becoming too verbose these have been left out of the scope of the proposed additions here, however it is important to note them in relation to this paper. For further details on these additional considerations that are considered for future work see Appendix A.

**
Additionally this paper aims to provide a foundation for developing a heterogeneous container that is compatible across a wide range of heterogeneous systems and underlying programming environments. It is understood that in addition to the library features proposed in this paper further additions to the C++ standard must be made in order to define the potential memory models of these heterogeneous systems. This is outside the scope of this paper, however it is considered when designing the features described.
**

## 3. Managed View Container  ##

### Managed View Class Template ###

The proposed addition to the standard library is the **managed_view** class template (Figure 1) which specifies a container which has ownership of a multi-dimensional array of elements stored in a contiguous allocation of memory that can be shared between the memory region of the host CPU and the memory region(s) of one or more remote devices.

```cpp
template <class T, std::size_t D>
class managed_view {
public:
  using value_type      = T;
  using pointer         = value_type *;
  using const_pointer   = const value_type *;
  using reference       = value_type &;
  using const_reference = const value_type &;
  using shape_type      = __impl_def_shape_type__<D>;
  using future_type     = __impl_def_future__;

  managed_view(pointer, shape_type);
  managed_view(const_pointer, shape_type);

  managed_view(const managed_view &);
  managed_view(const managed_view &&);
  managed_view &operator=(const managed_view &);
  managed_view &operator=(const managed_view &&);

  ~managed_view();

  future_type acquire() const;

  bool is_available() const;
};
```

*Figure 1: Temporal class template*

The **managed_view** class template has three template parameters; **T **specifying the type of the elements stored within the container and **D** specifying the dimensionality of the container.

The type parameter **T** specifies the element type of the container, any **T** satisfies the **managed_view** element type requirements if:

* **T** is a standard layout type.
* **T** is copy constructible.

The integral parameter **D** specifies the dimensionality of the temporal class, any **D** satisfies the dimensionality requires if:

* **D** falls between a lower limit of 1 and an upper limit of 3.

A **managed_view** can be constructed from a **pointer** or **const_pointer** and a multidimensional range of sizes in elements specified by an instance of **shape_type**; which will likely be std::bounds (proposed in N4512).

Constructing a **managed_view** object transfers ownership of the region of memory bound by the address of the pointer and the address of the pointer offset by the total size in number of range, for the duration of the temporal object’s lifetime (Figure 2).

```cpp
memory_region = [ hostPtr , hostPtr + (sizeof(T) * range[0] * … * range[D-1]) )
```

*Figure 2: Temporal class memory ownership*

All constructors assume that the region of memory is a contiguous allocation of N elements of type **T**, where N is the total size of range.

Any access to the memory region now owned by the **managed_view** object via the pointer used to construct the object is undefined behaviour for the duration of the **managed_view** object’s lifetime.

## 4. Consistency Model ##

### Concurrent Access ###

The consistency model for the **managed_view** class states the rules in which a memory allocation owned by a **managed_view** object can be concurrently observed (read-only access) or modified (read-write access) in the memory regions of the host CPU and remote device(s).

A **managed_view** object can be observed (read-only) on the host CPU and any number of heterogeneous devices concurrently or can only be modified (read-write) on either the host CPU or one remote device.

A **managed_view** object that is observed does not alter the value of the data stored at the memory region, however a **managed_view** object that is modified does alter the value of the data stored in the memory region. This means that once a **managed_view** object is modified, the device in which it was modified, whether that be the host CPU or a remote device now has the most recently modified copy.

Now if that **memory_view** object is to be observed or modified in another device that does not have the most recently modified copy, whether that be the host CPU or a remote device, synchronization must be performed to update the device that is currently observing or modifying the **managed_view** with the values of the most recently modified copy.

Extensions to the consistency model for allowing multiple concurrent modifiers are considered as later additions as this would require additional OS or hardware support not currently widely available across heterogeneous or distributed systems.

This consistency model is enforced via the use of the **array_ref** class in order to access the memory managed by a **managed_view** object.

### Synchronisation ###

All synchronization of a **managed_view** object are performed by synchronisation points, which are asynchronous operations which result in the values of the data at a memory region being updated with the values of the data at the memory region which contains the most recently modified copy.

There are two kinds of synchronization points:
* Explicit synchronisation points which synchronise via a member function of the executor for which the memory allocation is being made available to.
* Implicit synchronization points which synchronise via the passing on an **array_ref** object or via a member function of the executor for which the memory allocation is being made available to.

Synchronisation to the host CPU is performed via member functions of the **managed_view** class and are always explicit synchronisation points.

Synchronisation to a remote device is performed via the interaction with the **executor** object which is dispatching to the remote device the **managed_view** is to be accessed on.

## 5. Data Movement Policies ##

### Data Movement Policy Enum Class ###

The kind of synchronisation points that are supported are defined by an enum class called a **data_movement_policy**:

```cpp
enum class data_movement_policy {
  explicit_sync,
  implicit_sync
};
```

*Figure 3: Data movement policy enum class*

### Integration with Executors ###

In order to provide interoperability and allow for more control over the management of data movement and compute, the kind of synchronisation is not mandated by the **managed_view** class, instead it is tied to an **executor** class [?].

To achieve this, the following addition to the executors interface is suggested:

```cpp
template <class Executor>
struct executor_data_movement_policy;

template <class Executor>
constexpr data_movement_policy executor_data_movement_policy_v = executor_data_movement_policy<Executor>::value;
```

*Figure 4: Extension to Executors for Data Movement Policy*

This trait can be used to query an executor type for it's data movement policy.

It's important to note that a synchronization point would not be required for an executor that executes on the host CPU as it is not executed on a remote device, and therefore has the same memory region as the host CPU. Fortunately the current executors proposal already has a bifurcation of executor categories that specified whether the executor is remote or host based, host based meaning that it is executed on std::threads.

## 6. Synchronization ##

### Synchronization to the Host CPU ###

To synchronise back to the host CPU the **managed_view** class has the **acquire()** member function, which will submit an asynchronous operation that will wait for any work being performed on a remote device via an **executor** to complete. It will return a **future_type** object that allows the operation to be waited on.

Additionally the **managed_view** class has the **is_available()** member function, which will return true if the host CPU is already synchronized and false if it requires synchronization.

### Synchronization to a Remote Device ###

To synchronise to a remote device a synchronization point must be performed via the **executor** object which is dispatching to said remote device.

### Integration with Executors ###

In order to provide interoperability and allow for more control over the management of data movement and compute, the synchronization points are performed by the **executor** class.

For performing explicit synchronisation points, the following addition to the executors interface is suggested:

```cpp
executor_future_t<Executor> BaseExecutor::synchronize(managed_container &) const;
```

*Figure 5: Extension to Executors for Synchronization Points*

This **synchronize()** member function must be available to all **executor** categories, and will perform an explicit synchronization point returning an **executor_future_t** that can be used to wait on the synchronization point completing.

For performing implicit synchronisation points (if **data_movement_policy** is **implicit_sync**), the **executor** must perform a synchronization point an **array_ref** that is captured within the execution function. Note that this is an optional feature and does not have to be supported by every executor implementation.

### Integration with Array Ref ###

In order to allow a remote device to access a **managed_view** object the **array_ref** class can be captured by an **executor**'s execution function.

For constructing a **array_ref** from a **managed_view** object, the following addition to the **array_ref** interface is suggested:

```cpp
array_ref::array_ref(managed_view &);
```

*Figure 6: Extension to Array Ref for Accessing a Managed View*

**TODO(Gordon): Add properties (move tis to it's own section)**
**TODO(Gordon): Maybe change constructor for array_ref to make function**


Due to the temporal nature of access to the memory region owned by a temporal object from a CPU and one or more heterogeneous device the latest modified copy of a memory region could exist on the CPU or a heterogeneous device and the implicit overhead of moving data between the memory of the CPU and the memory of a heterogeneous device can be quite large.

For this reason it is important for runtime or library implementing the movement of data for heterogeneous dispatch to be able to optimize the scheduling of heterogeneous dispatches and data movement between the memory of the CPU and the memory of heterogeneous devices where possible.

In order to support this consistency model any  control structure capable of heterogeneous dispatch must be capable of knowing how an array_view object is accessing a temporal object, therefore how the memory region owned by the temporal object is being accessed, in order to perform suitable data movement optimizations at runtime.

In order to fully achieve this, this paper proposes a small non-invasive addition to the array_view class in the form of an enum class which describes the access mode of the array_view object (Figure N).

```cpp
enum class view_mode {
  modify,
  observe
};
```

*Figure N: View mode enum class*

An array_view object with the modify view mode is describing to the Viewable object that it wishes to read and write to the memory allocation it owns. An array_view with the observe view mode is describing to the Viewable object that it wishes only to read the memory allocation it owns.

This enum class is then added as a template parameter to the array_view class with a default argument such that if the view mode is not specified there is no effect (Figure N).

```cpp
template <class Viewable, view_mode Mode = view_mode::modify>
constexpr array_view(Viewable&& vw);
```

*FigureN: New array view class constructor*











## 7. Examples  ##

### Implicit Movement with One Execution Context ###

```cpp
my_execution_context executionContext;
auto exec = executionContext.executor();

float *hostPointer = new float[1024];
std::experimental::managed_view<float, 2>::shape_type range(32, 32);
std::experimental::managed_view<float, 2> container(hostPointer, range);

std::experimental::array_ref<float, property::read_write> arrayRef(container);

std::experimental::async(exec, arrayRef, [=](auto aR){

  device_func(aR);

});

container.aquire().get();

host_func(hostPointer);
```

### Implicit Movement with Two Execution Contexts ###

```cpp
thread_pool threadPool;
auto cpuExec = threadPool.executor();

gpu_execution_context gpuContext;
auto gpuExec = gpuContext.executor();

float *hostPointer = new float[1024];
std::experimental::managed_view<float, 2>::shape_type range(32, 32);
std::experimental::managed_view<float, 2> container(hostPointer, range);

std::experimental::array_ref<float, property::read_write> arrayRef(container);

std::experimental::async(cpuExec, arrayRef, [=](auto aR){

  cpu_func(aR);

}).then_async(gpuExec, arrayRef, [=](auto aR){

  gpu_func(aR);

});

container.acquire().get();

host_func(hostPointer);
```

### Implicit Movement with Concurrent Execution ###

```cpp
my_execution_context executionContext;
auto exec = executionContext.executor();

float *hostPointerA = new float[1024];
float *hostPointerB = new float[1024];
float *hostPointerC = new float[1024];
std::experimental::managed_view<float, 2>::shape_type range(32, 32);
std::experimental::managed_view<float, 2> containerA(hostPointerA, range);
std::experimental::managed_view<float, 2> containerB(hostPointerB, range);
std::experimental::managed_view<float, 2> containerC(hostPointerC, range);

std::experimental::array_ref<float, property::read_write> arrayRefA(containerA);
std::experimental::array_ref<float, property::read_write> arrayRefB(containerB);
std::experimental::array_ref<float, property::read_write> arrayRefC(containerC);

std::experimental::async(exec, arrayRefA [=](auto aR){

  device_func(aR);

});
std::experimental::async(exec, arrayRefB, [=](auto aR){

  device_func(aR);

});
std::experimental::async(exec, arrayRefA, arrayRefB, arrayRefC, [=](auto aRA, aRB aRC){

  device_func(aRA, aRB, aRC);

});

container.acquire().get();

host_func(hostPointer);
```

### Explicit Movement with One Execution Context ###

```cpp
my_execution_context executionContext;
auto exec = executionContext.executor();

float *hostPointer = new float[1024];
std::experimental::managed_view<float, 2>::shape_type range(32, 32);
std::experimental::managed_view<float, 2> container(hostPointer, range);

std::experimental::array_ref<float, property::read_write> arrayRef(container);

exec.synchronise(container).then_async(exec, arrayRef, [=](auto aR){

  device_func(aR);

});

container.aquire().get();

host_func(hostPointer);
```

### Explicit Movement with Two Execution Contexts ###

```cpp
thread_pool threadPool;
auto cpuExec = threadPool.executor();

gpu_execution_context gpuContext;
auto gpuExec = gpuContext.executor();

float *hostPointer = new float[1024];
std::experimental::managed_view<float, 2>::shape_type range(32, 32);
std::experimental::managed_view<float, 2> container(hostPointer, range);

std::experimental::array_ref<float, property::read_write> arrayRef(container);

cpuExec.synchronise(container).then async(cpuExec, arrayRef, [=](auto aR){

  cpu_func(aR);

}).get();

gpuExec.synchronise(container).then_async(gpuExec, arrayRef, [=](auto aR){

  gpu_func(aR);

});

container.acquire().get();

host_func(hostPointer);
```

### Explicit Movement with Concurrent Execution ###

```cpp
my_execution_context executionContext;
auto exec = executionContext.executor();

float *hostPointerA = new float[1024];
float *hostPointerB = new float[1024];
float *hostPointerC = new float[1024];
std::experimental::managed_view<float, 2>::shape_type range(32, 32);
std::experimental::managed_view<float, 2> containerA(hostPointerA, range);
std::experimental::managed_view<float, 2> containerB(hostPointerB, range);
std::experimental::managed_view<float, 2> containerC(hostPointerC, range);

std::experimental::array_ref<float, property::read_write> arrayRefA(containerA);
std::experimental::array_ref<float, property::read_write> arrayRefB(containerB);
std::experimental::array_ref<float, property::read_write> arrayRefC(containerC);

auto fA = exec.synchronise(container).then_async(exec, arrayRefA [=](auto aR){

  device_func(aR);

}).get();

auto fB = exec.synchronise(container).then_async(exec, arrayRefB, [=](auto aR){

  device_func(aR);

}).get();

exec.synchronise(container).then_async(exec, arrayRefA, arrayRefB, arrayRefC, [=](auto aRA, auto aRB auto aRC){

  device_func(aRA, aRB, aRC);

});

container.acquire().get();

host_func(hostPointer);
```

## 8. Related Work ##

There are other papers looking at proposing related or similar features to C\+\+; this section will describe how the proposal within this paper has been influenced by or could work towards assimilating.

### P0058R0 An Interface for Abstracting Execution ###

**TODO(Gordon): Extend executor_traits to include representation of address spaces**

P0367R0 Accessors; a C\+\+ standard library class to qualify data accesses

## Appendix A: Future Work ##

There are many areas in which the temporal class could be extended to support further features of heterogeneous dispatch however they out of the scope of this paper.

### Hierarchical Memory Structures ###

While CPUs have a single flat memory address space, most heterogeneous devices have a more complex hierarchy of memory address spaces for different memory segments, each with their own unique access scope, semantics and latency. Some heterogeneous programming languages provide a unified or shared memory address space model to allow more generic programming, this will not always result in the most efficient memory access. This can be supported either in hardware or software, and there are various different level of this feature in different heterogeneous programming languages such as OpenCL, HSA and CUDA. In general this means that pointers that are allocated in the host memory region can be used directly in heterogeneous dispatch to address into the memory region of a heterogeneous device. This can be implemented in many different ways ranging on the host and the heterogeneous device(s) having direct access to the same physical memory, having a cache coherency system in place which allows access to from the host and the heterogeneous device(s) as if they were accessing the same physical memory or having a runtime mapping of pointers across a shared virtual address space which maps address in the host memory region to addresses in the heterogeneous device memory region allowing code to access on the host and the heterogeneous device(s) as if they were accessing the same physical memory.

### Supporting Cache Coherent Systems ###

There are many heterogeneous architectures which a shared memory addressing model where both the host CPU and one or more heterogeneous devices can address the same memory region either via a unified memory region in hardware or via a cache coherent runtime that is able to synchronise a shared virtual address space between the host CPU and one or more heterogeneous devices.

In order to support this feature there would have to be some form of extension to the temporal container or perhaps new form of container with a different consistency model.

**TODO(Gordon): Supporting implementation interoperability.**
**TODO(Gordon): Integration with control structures**

## Appendix B: References ##

**TODO(Gordon): Fix titles**

[1] SYCL 1.2 Specification:
https://www.khronos.org/registry/sycl/specs/sycl-1.2.pdf

[2] P0443R0 A Unified Executors Proposal for C++:
https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0443r0.html

[3] P0009R3 Polymorphic Multidimensional Array Reference:
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0009r3.html



TODO(Gordon): Update the rest of the reference indexs

[3] N4352 Current working draft of parallelism TS:
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4352.html

[4] N4578 Current working draft of parallelism 2 TS:
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/n4578.html

[5] N4512 Multidimensional bounds, index and index_array:
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4512.html

[6] P0072R1 Light-weight execution agents:
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0072r1.pdf

[7] P0058R0 An Interface for Abstracting Execution:
http://open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0058r0.pdf

[8] P0367R0 Accessors; a C\+\+ standard library class to qualify data accesses:
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0367r0.pdf

## Appendix C: Glossary ##

**TODO(Gordon): Remove any glossary terms that are not used in the final paper.**

Below is a short description of some of the terms that are used throughout the paper.

### Heterogeneous Systems ###

A heterogeneous system is any computer system; desktop, mobile, embedded or high performance, which contains multiple devices of different architectures sharing some form of common memory hierarchy, generally this is a CPU and one or more devices of a different architecture, however the design of these systems vary greatly.

### Host CPU Device ###

A host device is the device which runs the systems operating system, generally this is the system in which work is dispatched from.

### Heterogeneous Devices ###

A heterogenous device is any device of a different architecture to other devices which it is being coupled with, generally this is the non CPU devices.

### Heterogeneous Dispatch ###

Heterogeneous dispatch is the act of copying a program and its requirement memory allocations to a heterogeneous device in order to be executed, before the data being copied back again.

### Separate Source ###

Separate source is where the program of the host device and the program(s) of the heterogeneous device(s) are written in separate source files, sometimes in different languages.

### Shared Source ###

Shared source is where the program of the host device and the program(s) of the heterogeneous device(s) are written in the same source file, generally this requires some form of library or language extension to specify the host device program and the heterogeneous device program.

### Multiple Compiler Compilation Model ###

A multiple compiler compilation model is one where there is a separate compiler for the host device program and for the heterogeneous device program(s). This compilation model can be applied to both separate source and shared source, as in the case of shared source the same source file is compiled by both compilers.

### Single Compiler Compilation Model ###

A single compiler compilation model is one where the is a single compiler for both the host device program and for the heterogeneous device program(s). This compilation model can generally only be applied to single source as even if the same compiler is used, it is in different invocations and likely with different parameters.

### Hierarchical Memory Addressing Model ###

A hierarchical memory addressing model is one where the different physical memories of a heterogeneous device or system are separated into logical memory segments each with their own address space. An example of this is in a GPU where you generally have program scope, read-only program scope, compute unit scope and processing element scope. This model requires extensions to the language in order to qualify variables and pointers with the address space to specify which memory segment it is stored in.

### Unified Memory Addressing Model ###

A unified memory addressing model is one where the different physical memories of a heterogeneous device or system are flattened into a single memory segment with a single address space. This means that library performing the heterogeneous dispatch is free to determine the physical memory each variable or pointer is stored in, representing addresses in single address space that is then converted to an address space relative address during execution. This approach removes the requirement for language extensions, however is generally comes at the cost of a hit to performance.

### Shared Memory Addressing Model ###

A shared memory addressing model is one where the different physical memories of a heterogeneous device or system are flattened into a single memory segment with a single address space similar to unified memory addressing, with the addition that all heterogeneous devices within a system can access the same physical memory. This is done either via hardware; by having a system in which each heterogeneous device can physically access the same memory or by software synchronisation; by having a cache coherency system which allows the host device and each heterogeneous device to access their own respective memory with a single address space, with synchronisation between devices being performed by the operating system.













# Later Paper!!! #

### Synchronisation Ordering ###

All synchronization points are guaranteed to be performed, however the order in which they are performed is subject to the synchronization ordering of a temporal class.

In order to describe the synchronisation ordering capabilities of a temporal class implementation there are two options of the sycnhronisation_ordering enum class (Figure N).

```cpp
enum class synchronisation_ordering {
  dependant_order,
  sequential_order
};
```

*Figure N: Synchronisation ordering enum class*

The synchronisation ordering properties of these options are described in the table below (Figure N):

| Synchronisation Ordering | Description |
| ------------------------ | ----------- |
| sequential_order         | An instance of a temporal class with sequential_order synchronisation ordering makes the guarantee that all synchronization points (explicit and implicit) will be performed in the order they are constructed. It is the users responsibility to ensure that synchronization is performed correctly prior to a compute operation.
| dependant_order          | An instance of a temporal class with dependant_order synchronisation ordering makes the guarantee that all synchronization points (explicit and implicit) will be performed prior to any subsequent compute operations that access an array_view instance that views the temporal class instance, until a later synchronization point is created synchronizing with a different memory region. |

*Figure N: Synchronisation ordering descriptions*

Another way in which synchronization points are enforced is via the wait() member function of the temporal class, which blocks, waiting for all previous synchronization points to before returning.

**TODO(Gordon): Add an enum for access mode?**

## 5. Data Movement Policy Class ##

### Data Movement Policy Class ###

An additional proposed addition to the standard library is the data_movement_policy class (Figure N) which acts as a tag which defines the a policy of data movement behaviour of the memory allocation own by the associated temporal class. The primary concern of the data_movement_policy is to specify whether the data movement between of the memory allocation, owned by the associated temporal class between the host CPU and one or more heterogeneous devices is performed implicitly or must be performed explicitly via the temporal class interface.

TODO(Gordon): Reword last paragraph

```cpp
class data_movement_policy;
```

*Figure N: Data movement policy class*

### Standard Data Movement Policies ###

There are two standard data movement policies with defined rules of data movement, each of which inherits from the data_movement_policy class (Figure N). This table describes each data movement policy in terms of data movement and synchronization ordering (for further details regarding the definitions of each of these see the consistency model).

| Data Movement Policy | Data Movement | Synchronisation Ordering |
| -------------------- | ------------- | ------------------------ |
| implicit_data_movement_policy | implicit_movement | dependant_ordering |
| explicit_data_movement_policy | explicit_movement | relaxed_ordering |

*Figure N: Standard data movement policies*

### Custom Data Movement Policies ###

It is possible to extend the data movement policy interface by implementing a custom data movement policy which inherits from the data_movement_policy class. Any custom data movement policy can have implementation- defined rules of data movement.

**TODO(Gordon): Add the requirements that a custom data movement policy must satisfy.**
**TODO(Gordon): Should the data movement policy also describe access targets and access modes, if so then the data movement policy should probably be associated with an extension to the array_view than the temporal class.**

