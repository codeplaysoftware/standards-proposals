# DXXXXr0: Executor properties for affinity-based execution

**Date: 2019-01-12**

**Audience: SG1, SG14, LEWG**

**Authors: Gordon Brown, Ruyman Reyes, Michael Wong, H. Carter Edwards, Thomas Rodgers, Mark Hoemmen**

**Contributors: Patrice Roy, Carl Cook, Jeff Hammond, Hartmut Kaiser, Christian Trott, Paul Blinzer, Alex Voicu, Nat Goodspeed, Tony Tye, Paul Blinzer**

**Emails: gordon@codeplay.com, ruyman@codeplay.com, michael@codeplay.com, hedwards@nvidia.com, rodgert@twrodgers.com, mhoemme@sandia.gov**

**Reply to: gordon@codeplay.com**


# Changelog

### PXXXXr0 (KON 2019)

* Separation of high-level features from [[35]][p0796].

### P0796r3 (SAN 2018)

* Remove reference counting requirement from `execution_resource`.
* Change lifetime model of `execution_resource`: it now either consistently identifies some underlying resource, or is invalid; context creation rejects an invalid resource.ster
* Remove `this_thread::bind` & `this_thread::unbind` interfaces.
* Make `execution_resource`s iterable by replacing `execution_resource::resources` with `execution_resource::begin` and `execution_resource::end`.
* Add `size` and `operator[]` for `execution_resource`.
* Rename `this_system::get_resources` to `this_system::discover_topology`.
* Introduce `memory_resource` to represent the memory component of a system topology.
* Remove `can_place_memory` and `can_place_agents` from the `execution_resource` as these are no longer required.
* Remove `memory_resource` and `allocator` from the `execution_context` as these no longer make sense.
* Update the wording to describe how execution resources and memory resources are structured.
* Refactor `affinity_query` to be between an `execution_resource` and a `memory_resource`.

### P0796r2 (RAP 2018)

* Introduce a free function for retrieving the execution resource underlying the current thread of execution.
* Introduce `this_thread::bind` & `this_thread::unbind` for binding and unbinding a thread of execution to an execution resource.
* Introduce `bulk_execution_affinity` executor properties for specifying affinity binding patterns on bulk execution functions.

### P0796r1 (JAX 2018)

* Introduce proposed wording.
* Based on feedback from SG1, introduce a pair-wise interface for querying the relative affinity between execution resources.
* Introduce an interface for retrieving an allocator or polymorphic memory resource.
* Based on feedback from SG1, remove requirement for a hierarchical system topology structure, which doesn't require a root resource.

### P0796r0 (ABQ 2017)

* Initial proposal.
* Enumerate design space, hierarchical affinity, issues to the committee.


# Abstract

This paper is the result of a request from SG1 at the 2018 San Diego meeting to split P0796: Supporting Heterogeneous & Distributed Computing Through Affinity [[35]][p0796] into two separate papers, one for the high-level interface and one for the low-level interface. This paper focusses on the high-level interface: a series of properties for querying affinity relationships and requesting affinity on work being executed. [[36]][pXXXX] focusses on the low-level interface: a mechanism for discovering the topology and affinity properties of a given system.

This paper provides an initial meta-framework for the drives toward an execution and memory affinity model for C\+\+.  It accounts for feedback from the Toronto 2017 SG1 meeting on Data Movement in C\+\+ [[1]][p0687r0] that we should define affinity for C\+\+ first, before considering inaccessible memory as a solution to the separate memory problem towards supporting heterogeneous and distributed computing.

This paper proposes a series of executor properties which can be used to apply affinity requirements to bulk execution functions.


# Motivation

*Affinity* refers to the "closeness" in terms of memory access performance, between running code, the hardware execution resource on which the code runs, and the data that the code accesses.  A hardware execution resource has "more affinity" to a part of memory or to some data, if it has lower latency and/or higher bandwidth when accessing that memory / those data.

On almost all computer architectures, the cost of accessing different data may differ. Most computers have caches that are associated with specific processing units. If the operating system moves a thread or process from one processing unit to another, the thread or process will no longer have data in its new cache that it had in its old cache. This may make the next access to those data slower. Many computers also have a Non-Uniform Memory Architecture (NUMA), which means that even though all processing units see a single memory in terms of programming model, different processing units may still have more affinity to some parts of memory than others. NUMA exists because it is difficult to scale non-NUMA memory systems to the performance needed by today's highly parallel computers and applications.

One strategy to improve applications' performance, given the importance of affinity, is processor and memory *binding*. Keeping a process bound to a specific thread and local memory region optimizes cache affinity. It also reduces context switching and unnecessary scheduler activity. Since memory accesses to remote locations incur higher latency and/or lower bandwidth, control of thread placement to enforce affinity within parallel applications is crucial to fuel all the cores and to exploit the full performance of the memory subsystem on NUMA computers. 

Operating systems (OSes) traditionally take responsibility for assigning threads or processes to run on processing units. However, OSes may use high-level policies for this assignment that do not necessarily match the optimal usage pattern for a given application. Application developers must leverage the placement of memory and *placement of threads* for best performance on current and future architectures. For C\+\+ developers to achieve this, native support for *placement of threads and memory* is critical for application portability. We will refer to this as the *affinity problem*. 

The affinity problem is especially challenging for applications whose behavior changes over time or is hard to predict, or when different applications interfere with each other's performance. Today, most OSes already can group processing units according to their locality and distribute processes, while keeping threads close to the initial thread, or even avoid migrating threads and maintain first touch policy. Nevertheless, most programs can change their work distribution, especially in the presence of nested parallelism.

Frequently, data are initialized at the beginning of the program by the initial thread and are used by multiple threads. While some OSes automatically migrate threads or data for better affinity, migration may have high overhead. In an optimal case, the OS may automatically detect which thread access which data most frequently, or it may replicate data which are read by multiple threads, or migrate data which are modified and used by threads residing on remote locality groups. However, the OS often does a reasonable job, if the machine is not overloaded, if the application carefully uses first-touch allocation, and if the program does not change its behavior with respect to locality.

Consider a code example *(Listing 1)* that uses the C\+\+17 parallel STL algorithm `for_each` to modify the entries of an `std::vector` `data`.  The example applies a loop body in a lambda to each entry of the `std::vector` `data`, using an execution policy that distributes work in parallel across multiple CPU cores. We might expect this to be fast, but since `std::vector` containers are initialized automatically and automatically allocated on the master thread's memory, we find that it is actually quite slow even when we have more than one thread.

```cpp
// NUMA executor representing N NUMA regions.
numa_executor exec;

// Storage required for vector allocated on construction local to current thread
// of execution, (N == 0).
std::vector<float> data(N * SIZE);

// Require the NUMA executor to bind its migration of memory to the underlying
// memory resources in a scatter pattern.s
auto affinityExec = std::execution::require(exec,
  bulk_execution_affinity.scatter);

// Migrate the memory allocated for the vector across the NUMA regions in a
// scatter pattern.
std::execution::migrate(std::begin(data), std::end(data), affinityExec);

// Placement of data is local to NUMA region 0, so data for execution on other
// NUMA nodes must is migrated when accessed.
std::for_each(std::execution::par.on(affinityExec), std::begin(data),
  std::end(data), [=](float &value) { do_something(value): });
```
*Listing 1: Migrating previously allocated memory.*

```cpp
// NUMA executor representing N NUMA regions.
numa_executor exec;

// Reserve space in a vector for a unique_ptr for each index in the bulk
// execution.
std::vector<std::unique_ptr<float[SIZE]>> data{};
data.reserve(N);

// Require the NUMA executor to bind it's allocation of memory to the underlying
// memory resources in a scatter patter.
auto affinityExec = std::execution::require(exec,
  bulk_execution_affinity.scatter);

// Launch a bulk execution that will allocate each unique_ptr in the vector with
// locality to the nearest NUMA region.
affinityExec.bulk_execute([&](size_t id) {
  data[id] = std::make_unique<float>(); }, N, sharedFactory);

// Execute a for_each using the same executor so that each unique_ptr in the
// vector maintains it's locality.
std::for_each(std::execution::par.on(affinityExec), std::begin(data),
  std::end(data), [=](float &value) { do_something(value): });
```
*Listing 2: Aligning memory and process affinity.*

The affinity interface we propose should help computers achieve a much higher fraction of peak memory bandwidth when using parallel algorithms. In the future, we plan to extend this to heterogeneous and distributed computing. This follows the lead of OpenMP [[2]][design-of-openmp], which has plans to integrate its affinity model with its heterogeneous model [3]. (One of the authors of this document participated in the design of OpenMP's affinity model.)

# Background Research: State of the Art

The problem of effectively partitioning a system’s topology has existed for some time, and there are a range of third-party libraries and standards which provide APIs to solve the problem. In order to standardize this process for C\+\+, we must carefully look at all of these approaches and identify which we wish to adopt. Below is a list of the libraries and standards from which this proposal will draw:

* Portable Hardware Locality [[4]][hwloc]
* SYCL 1.2 [[5]][sycl-1-2-1]
* OpenCL 2.2 [[6]][opencl-2-2]
* HSA [[7]][HSA]
* OpenMP 5.0 [[8]][openmp-5]
* cpuaff [[9]][cpuaff]
* Persistent Memory Programming [[10]][pmem]
* MEMKIND [[11]][memkid]
* Solaris pbind() [[12]][solaris-pbind]
* Linux sched_setaffinity() [[13]][linux-sched-setaffinity]
* Windows SetThreadAffinityMask() [[14]][windows-set-thread-affinity-mask]
* Chapel [[15]][chapel]
* X10 [[16]][x10]
* UPC\+\+ [[17]][upc++]
* TBB [[18]][tbb]
* HPX [[19]][hpx]
* MADNESS [[20]][madness][[32]][madness-journal]

Libraries such as the [Portable Hardware Locality (hwloc) library][hwloc] provide a low level of hardware abstraction, and offer a solution for the portability problem by supporting many platforms and operating systems. This and similar approaches use a tree structure to represent details of CPUs and the memory system. However, even some current systems cannot be represented correctly by a tree, if the number of hops between two sockets varies between socket pairs [[2]][design-of-openmp].

Some systems give additional user control through explicit binding of threads to processors through environment variables consumed by various compilers, system commands, or system calls.  Examples of system commands include Linux's `taskset` and `numactl`, and Windows' `start /affinity`.  System call examples include Solaris' `pbind()`, Linux's `sched_setaffinity()`, and Windows' `SetThreadAffinityMask()`.

## Problem Space (TODO)

In this paper we describe the problem space of affinity for C\+\+, the various challenges which need to be addressed in defining a partitioning and affinity interface for C\+\+, and some suggested solutions.  These include:

* How to represent, identify and navigate the topology of execution resources available within a heterogeneous or distributed system.
* How to query and measure the relative affinity between different execution resources within a system.
* How to bind execution and allocation particular execution resource(s).
* What kind of and level of interface(s) should be provided by C\+\+ for affinity.

Wherever possible, we also evaluate how an affinity-based solution could be scaled to support both distributed and heterogeneous systems. We also have addressed some aspects of dynamic topology discovery.

There are also some additional challenges which we have been investigating but are not yet ready to be included in this paper, and which will be presented in a future paper:

* How to migrate memory work and memory allocations between execution resources.
* More general cases of dynamic topology discovery.
* Fault tolerance, as it relates to dynamic topology.

### Resource lifetime

The initial solution may only target systems with a single addressable memory region. It may thus exclude devices like discrete GPUs. However, in order to maintain a unified interface going forward, the initial solution should consider these devices and be able to scale to support them in the future. In particular, in order to support heterogeneous systems, the abstraction must let the interface query the *resource topology* of the *system* in order to perform device discovery.

The *resource* objects returned from the *topology discovery interface* are opaque, implementation-defined objects. They would not perform any scheduling or execution functionality which would be expected from an *execution context*, and they would not store any state related to an execution. Instead, they would simply act as an identifier to a particular partition of the *resource topology*. This means that the lifetime of a *resource* retrieved from an *execution context* must not be tied to the lifetime of that *execution context*.

The lifetime of a *resource* instance refers to both validity and uniqueness. First, if a *resource* instance exists, does it point to a valid underlying hardware or software resource? That is, could an instance's validity ever change at run time? Second, could a *resource* instance ever point to a different (but still valid) underlying resource? It suffices for now to define "point to a valid underlying resource" informally. We will elaborate this idea later in this proposal.

Creation of a *context* expresses intent to use the *resource*, not just to view it as part of the *resource topology*. Thus, if a *resource* could ever cease to point to a valid underlying resource, then users must not be allowed to create a *context* from the resource instance, or launch executions with that context. *Context* construction, and use of an *executor* with that *context* to launch an execution, both assert validity of the *context*'s *resource*.

If a *resource* is valid, then it must always point to the same underlying thing. For example, a *resource* cannot first point to one CPU core, and then suddenly point to a different CPU core. *Contexts* can thus rely on properties like binding of operating system threads to CPU cores. However, the "thing" to which a *resource* points may be a dynamic, possibly software-managed pool of hardware. Here are three examples of this phenomenon:

   1. The "hardware" may actually be a virtual machine (VM). At any point, the VM may pause, migrate to different physical hardware, and resume. If the VM presents the same virtual hardware before and after the migration, then the *resources* that an application running on the VM sees should not change.
   2. The OS may maintain a pool of a varying number of CPU cores as a shared resource among different user-level processes. When a process stops using the resource, the OS may reclaim cores. It may make sense to present this pool as an *execution resource*.
   3. A low-level device driver on a laptop may switch between a "discrete" GPU and an "integrated" GPU, depending on utilization and power constraints. If the two GPUs have the same instruction set and can access the same memory, it may make sense to present them as a "virtualized" single *execution resource*.

In summary, a *resource* either identifies a thing uniquely, or harmlessly points to nothing. The section that follows will justify and explain this.

### Querying the relative affinity of partitions

In order to make decisions about where to place execution or allocate memory in a given *system’s resource topology*, it is important to understand the concept of affinity between different *execution resources*. This is usually expressed in terms of latency between two resources. Distance does not need to be symmetric in all architectures.

The relative position of two components in the topology does not necessarily indicate their affinity. For example, two cores from two different CPU sockets may have the same latency to access the same NUMA memory node.

This feature could be easily scaled to heterogeneous and distributed systems, as the relative affinity between components can apply to discrete heterogeneous and distributed systems as well.


# Proposal

## Overview

In this paper we propose an interface for discovering the execution resources within a system, querying the relative affinity metric between those execution resources, and then using those execution resources to allocate memory and execute work with affinity to the underlying hardware those execution resources represent. The interface described in this paper builds on the existing interface for executors and execution contexts defined in the executors proposal [[22]][p0443r7].

A series of executor properties describe desired behavior when using parallel algorithms or libraries. These properties provide a low granularity and is aimed at users who may have little or no knowledge of the system architecture.

## Header `<execution>` synopsis

```cpp
namespace std {
namespace experimental {
namespace execution {

// Bulk execution affinity properties

struct bulk_execution_affinity_t;

constexpr bulk_execution_affinity_t bulk_execution_affinity;

// Concurrency property

struct concurrency_t;

constexpr concurrency_t concurrency;

// Execution locality intersection property

struct execution_locality_intersection_t;

constexpr execution_locality_intersection_t execution_locality_intersection;

// Memory locality intersection property

struct memory_locality_intersection_t;

constexpr memory_locality_intersection_t memory_locality_intersection;

}  // execution
}  // experimental
}  // std
```
*Listing 7: Header synopsis*

## Bulk execution affinity properties

We propose an executor property group called `bulk_execution_affinity` which contains the nested properties `none`, `balanced`, `scatter` or `compact`. Each of these properties, if applied to an *executor* enforces a particular guarantee of binding *execution agents* to the *execution resources* associated with the *executor* in a particular pattern.

### Example

Below is an example of executing a parallel task over 8 threads using `bulk_execute`, with the affinity binding `bulk_execution_affinity.scatter`.

```cpp
{
  auto exec = executionContext.executor();

  auto affExec = execution::require(exec, execution::bulk,
    execution::bulk_execution_affinity.scatter);

  affExec.bulk_execute([](std::size_t i, shared s) {
    func(i);
  }, 8, sharedFactory);
}
```
*Listing 3: Example of using the bulk_execution_affinity property*

### Proposed Wording

The `bulk_execution_affinity_t` property is a behavioral property as defined in P0443 [[22]][p0443] which describes the guarantees an executor provides to the binding of *execution agents* created by a call to `bulk_execute` to the underlying *threads of execution* and to the locality of those *threads of execution*.

The `bulk_execution_affinity_t` property provides nested property types and objects as described below, where:
* `e` denotes an executor object of type `E`,
* `f` denotes a function object of type `F&&`,
* `s` denotes a shape object of type `execution::executor_shape<E>`, and
* `sf` denotes a function object of type `SF`.

| Nested Property Type | Nested Property Name | Requirements |
|----------------------|----------------------|--------------|
| `bulk_execution_affinity_t::none_t` | `bulk_execution_affinity_t::none` | A call to `e.bulk_execute(f, s, sf)` has no requirements on the binding of *execution agents* to the underlying *execution resources*. |
| `bulk_execution_affinity_t::scatter_t` | `bulk_execution_scatter_t::scatter` | A call to `e.bulk_execute(f, s, sf)` must bind the created *execution agents* to the underlying *execution resources* such that they are distributed sparsely across the *execution resources*. The affinity binding pattern must be consistent across invocations of the executor's bulk execution function. |
| `bulk_execution_affinity_t::compact_t` | `bulk_execution_compact_t::compact` | A call to `e.bulk_execute(f, s, sf)` must bind the created *execution agents* to the underlying *execution resources* such that they are distributed as close as possible to the *execution resource* of the *thread of execution* which created them. The affinity binding pattern must be consistent across invocations of the executor's bulk execution function. |
| bulk_execution_affinity_t::balanced_t | bulk_execution_balanced_t::balanced | A call to `e.bulk_execute(f, s, sf)` must bind the created *execution agents* to the underlying *execution resources* such that they are collected into groups, each group is distributed sparsely across the *execution resources* and the *execution agents* within each group are  distributed as close as possible to the first *execution resource* of that group. The affinity binding pattern must be consistent across invocations of the executor's bulk execution function. |

> [*Note:* The requirements of the `bulk_execution_affinity_t` nested properties do not enforce a specific binding, simply that the binding follows the requirements set out above and that the pattern is consistent across invocations of the bulk execution functions. *--end note*]

> [*Note:* If two *executors* `e1` and `e2` invoke a bulk execution function in order, where `execution::query(e1, execution::context) == query(e2, execution::context)` is `true` and `execution::query(e1, execution::bulk_execution_affinity) == query(e2, execution::bulk_execution_affinity)` is `false`, this will likely result in `e1` binding *execution agents* if necessary to achieve the requested affinity pattern and then `e2` rebinding to achieve the new affinity pattern. Rebinding *execution agents* to *execution resources* may take substantial time and may affect performance of subsequent code. *--end note*]

> [*Note:* The terms used for the `bulk_execution_affinity_t` nested properties are derived from the OpenMP properties [[33]][openmp-affinity] including the Intel specific balanced affinity binding [[[34]][intel-balanced-affinity] *--end note*]

## Concurrency property

We propose a query-only executor property called `concurrency_t` which returns the maximum potential concurrency available to *executor*.

### Example

```cpp
TODO
```

## Proposed Wording

The `concurrency_t` property is a query-only property as defined in P0443 [[22]][p0443]. 

```cpp
struct concurrency_t
{
  static constexpr bool is_requirable = false;
  static constexpr bool is_preferable = false;

  using polymorphic_query_result_type = size_t;

  template<class Executor>
    static constexpr decltype(auto) static_query_v
      = Executor::query(concurrency_t());
};
```

The `concurrency_t` property can be used only with `query`, which returns the maximum potential concurrency available to the executor. If the value is not well defined or not computable, `0` is returned.

The value returned from `execution::query(e, concurrency_t)`, where `e` is an executor, shall not change between invocations.

> [*Note:* The expectation here is that the maximum available concurrency for an *executor* as described here is equivalent to calling `this_thread::hardware_concurrency()` *--end note*]

## Execution locality intersection property

We propose a query-only executor property called `execution_locality_intersection_t` which returns the maximum potential concurrency that ia available to both of two *executors*.

### Example

```cpp
TODO
```

## Proposed Wording

The `execution_locality_intersection_t` property is a query-only property as defined in P0443 [[22]][p0443]. 

```cpp
template <class DestExecutor>
struct execution_locality_intersection_t
{
  constexpr execution_locality_intersection_t(const DestExecutor &);

  static constexpr bool is_requirable = false;
  static constexpr bool is_preferable = false;

  using polymorphic_query_result_type = size_t;

  template<class Executor>
    static constexpr decltype(auto) static_query_v
      = Executor::query(execution_locality_intersection_t(DestExecutor{}));
};
```

The `execution_locality_intersection_t` property can be used only with `query`, which returns the maximum potential concurrency available to both *executors*. If the value is not well defined or not computable, `0` is returned.

The value returned from `execution::query(e1, execution_locality_intersection_t(e2))`, where `e1` and `e2` are executors, shall not change between invocations.

> [*Note:* The expectation here is that the maximum available concurrency for an *executor* as described here is equivalent to calling `this_thread::hardware_concurrency()` *--end note*]

## Memory locality intersection property

We propose a query-only executor property called `execution_locality_intersection_t` which specifies whether two *executors* share a common address space.

### Example

```cpp
TODO
```

## Proposed Wording

The `memory_locality_intersection_t` property is a query-only property as defined in P0443 [[22]][p0443]. 

```cpp
template <class DestExecutor>
struct memory_locality_intersection_t
{
  constexpr memory_locality_intersection_t(const DestExecutor &);

  static constexpr bool is_requirable = false;
  static constexpr bool is_preferable = false;

  using polymorphic_query_result_type = bool;

  template<class Executor>
    static constexpr decltype(auto) static_query_v
      = Executor::query(memory_locality_intersection_t(DestExecutor{}));
};
```

The `memory_locality_intersection_t` property can be used only with `query`, which returns `true` if both *executors* share a common address space, and `false` otherwise. If the value is not well defined or not computable, `false` is returned.

The value returned from `execution::query(e1, memory_locality_intersection_t(e2))`, where `e1` and `e2` are executors, shall not change between invocations.


# Future Work

## Who should have control over bulk execution affinity?

This paper currently proposes the `bulk_execution_affinity_t` properties and its nested properties for allowing an *executor* to make guarantees as to how *execution agents* are bound to the underlying *execution resources*. However providing control at this level may lead to *execution agents* being bound to *execution resources* within a critical path. A possible solution to this is to allow the *execution context* to be configured with `bulk_execution_affinity_t` nested properties, either instead of the *executor* property or in addition. This would allow the binding of *threads of execution* to be performed at the time of the *execution context* creation.

| Straw Poll |
|------------|
| Should the *execution context* be able to manage the binding of all *threads of execution* which it manages using the `bulk_execution_affinity_t` nested properties? |
| Should the *executor* be able to manage the binding of all *execution agents* which it manages using the `bulk_execution_affinity_t` nested properties? |
| Should both the *execution context* and the *executor* be able to manage the binding of *threads of execution* and subsequently *execution agents* using the `bulk_execution_affinity_t` nested properties? |

## Migrating data from memory allocated in one partition to another

With the ability to place memory with affinity comes the ability to define algorithms or memory policies which describe at a higher level how memory is distributed across large systems. Some examples of these are pinned, first touch, and scatter. This is outside the scope of this paper, though we would like to investigate this in a future paper.

| Straw Poll |
|------------|
| Should the interface provide a way of migrating data between partitions? |


# Acknowledgments

Thanks to Christopher Di Bella, Toomas Remmelg, and Morris Hafner for their reviews and suggestions.


# References

[p0687]: http://wg21.link/p0687
[[1]][p0687] P0687: Data Movement in C\+\+

[design-of-openmp]: https://link.springer.com/chapter/10.1007/978-3-642-30961-8_2
[[2]][design-of-openmp] The Design of OpenMP Thread Affinity

[3] Euro-Par 2011 Parallel Processing: 17th International, Affinity Matters

[hwloc]: https://www.open-mpi.org/projects/hwloc/
[[4]][hwloc] Portable Hardware Locality

[sycl-1-2-1]: https://www.khronos.org/registry/SYCL/specs/sycl-1.2.1.pdf
[[5]][sycl-1-2-1] SYCL 1.2.1

[opencl-2-2]: https://www.khronos.org/registry/OpenCL/specs/opencl-2.2.pdf
[[6]][opencl-2-2] OpenCL 2.2

[HSA]: http://www.hsafoundation.com/standards/
[[7]][HSA] HSA

[openmp-5]: http://www.openmp.org/wp-content/uploads/openmp-TR5-final.pdf
[[8]][openmp-5] OpenMP 5.0

[cpuaff]: https://github.com/dcdillon/cpuaff
[[9]][cpuaff] cpuaff

[pmem]: http://pmem.io/
[[10]][pmem] Persistent Memory Programming

[memkid]: https://github.com/memkind/memkind
[[11]][memkid] MEMKIND

[solaris-pbind]: https://docs.oracle.com/cd/E26502_01/html/E29031/pbind-1m.html
[[12]][solaris-pbind] Solaris pbind()

[linux-sched-setaffinity]: https://linux.die.net/man/2/sched_setaffinity
[[13]][linux-sched-setaffinity] Linux sched_setaffinity() 

[windows-set-thread-affinity-mask]: https://msdn.microsoft.com/en-us/library/windows/desktop/ms686247(v=vs.85).aspx
[[14]][windows-set-thread-affinity-mask] Windows SetThreadAffinityMask()

[chapel]: https://chapel-lang.org/
[[15]][chapel] Chapel

[x10]: http://x10-lang.org/
[[16]][x10] X10

[upc++]: https://bitbucket.org/berkeleylab/upcxx/wiki/Home
[[17]][upc++] UPC\+\+

[tbb]: https://www.threadingbuildingblocks.org/
[[18]][tbb] TBB

[hpx]: https://github.com/STEllAR-GROUP/hpx
[[19]][hpx] HPX

[madness]: https://github.com/m-a-d-n-e-s-s/madness
[[20]][madness] MADNESS

[lstopo]: https://www.open-mpi.org/projects/hwloc/lstopo/
[[21]][lstopo] Portable Hardware Locality Istopo

[p0443]:
http://wg21.link/p0443
[[22]][p0443] A Unified Executors Proposal for C\+\+

[p0737]: http://wg21.link/p0737
[[23]][p0737] P0737 : Execution Context of Execution Agents

[exposing-locality]: https://docs.google.com/viewer?a=v&pid=sites&srcid=bGJsLmdvdnxwYWRhbC13b3Jrc2hvcHxneDozOWE0MjZjOTMxOTk3NGU3
[[24]][exposing-locality] Exposing the Locality of new Memory Hierarchies to HPC Applications

[mpi]: http://mpi-forum.org/docs/
[[25]][mpi] MPI

[pvm]: http://www.csm.ornl.gov/pvm/
[[26]][pvm] Parallel Virtual Machine

[pvm-callback]: http://etutorials.org/Linux+systems/cluster+computing+with+linux/Part+II+Parallel+Programming/Chapter+11+Fault-Tolerant+and+Adaptive+Programs+with+PVM/11.2+Building+Fault-Tolerant+Parallel+Applications/
[[27]][pvm-callback] Building Fault-Tolerant Parallel Applications

[mpi-post-failure-recovery]: http://journals.sagepub.com/doi/10.1177/1094342013488238
[[28]][mpi-post-failure-recovery] Post-failure recovery of MPI communication capability

[mpi-fault-tolerance]: http://www.mcs.anl.gov/~lusk/papers/fault-tolerance.pdf
[[29]][mpi-fault-tolerance] Fault Tolerance in MPI Programs

[p0323r4]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0323r4.html
[[30]][p0323r4] p0323r4 std::expected

[movidius]: https://developer.movidius.com/
[[31]][movidius]: Intel® Movidius™ Neural Compute Stick

[madness-journal]: http://dx.doi.org/10.1137/15M1026171
[[32]][madness-journal] MADNESS: A Multiresolution, Adaptive Numerical Environment for Scientific Simulation

[openmp-affinity]: http://pages.tacc.utexas.edu/~eijkhout/pcse/html/omp-affinity.html
[[33]][openmp-affinity] OpenMP topic: Affinity

[intel-balanced-affinity]: https://software.intel.com/en-us/node/522518
[[34]][intel-balanced-affinity] Balanced Affinity Type

[p0796]: http://wg21.link/p0796
[[35]][p0796] Supporting Heterogeneous & Distributed Computing Through Affinity

[pXXXX]: http://wg21.link/pXXXX
[[36]][pXXXX] ######
