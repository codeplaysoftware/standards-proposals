# Shared Virtual Memory for SYCL 2.2

The current SYCL 2.2 provisional specification (revision date 2016/02/15)
defines sharing virtual memory addresses between host and device -
Shared Virtual Memory or just SVM - based on SVM as defined by OpenCL 2.2.
It defines SVM capabilities as coarse-grained buffer, fine-grained buffer,
and fine-grained system, along with atomics
(atomic are only available with fine-grained buffer and fine-grained system).
It also defines
`cl::sycl::capability_selector` for selecting device capabilities,
`cl::sycl::execution_handle` to replace `cl::sycl::handler`,
`cl::sycl::svm_allocator` to allocate data in the SVM space,
along with other minor additions. This proposal aims to simplify the current
SYCL 2.2 specification.

## SVM namespace

We propose that everything in SYCL connected to SVM be accessible in the
`cl::sycl::svm` namespace.

## SVM properties

We propose the `cl::sycl::property` namespace as defined in SYCL 1.2.1 be
extended with the namespace `cl::sycl::property::svm`,
which contains the following properties:

* `cl::sycl::property::svm::none`
* `cl::sycl::property::svm::coarse`
* `cl::sycl::property::svm::fine`
* `cl::sycl::property::svm::system`
* `cl::sycl::property::svm::atomic`

Each of these properties is a unique class type with a default constructor.
These properties can be used in many places to specify the desired SVM
capability, either at compile time or at program execution. These properties
replace `cl::sycl::exec_capabilities` as defined by the current
SYCL 2.2 provisional specification.

In order to maintain backwards compatibility with the OpenCL 1.2 memory model,
we propose adding the property `cl::sycl::property::opencl12`.
This property is not compatible with any SVM features.

In addition, we propose aliases to these properties to simplify programming:

* `cl::sycl::svm::none` -> `cl::sycl::property::svm::none`
* `cl::sycl::svm::coarse` -> `cl::sycl::property::svm::coarse`
* `cl::sycl::svm::fine` -> `cl::sycl::property::svm::fine`
* `cl::sycl::svm::system` -> `cl::sycl::property::svm::system`
* `cl::sycl::svm::atomic` -> `cl::sycl::property::svm::atomic`

### API changes

The following properties should be added:

```cpp
namespace cl {
namespace sycl {
namespace property {
namespace svm {

struct none {
  none() = default;
};

struct coarse {
  coarse() = default;
};

struct fine {
  fine() = default;
};

struct system {
  system() = default;
};

struct atomic {
  atomic() = default;
};

} // namespace svm

struct opencl12() {
  opencl12() = default;
};

} // namespace property

namespace svm {

using none = cl::sycl::property::svm::none;
using coarse = cl::sycl::property::svm::coarse;
using fine = cl::sycl::property::svm::fine;
using system = cl::sycl::property::svm::system;
using atomic = cl::sycl::property::svm::atomic;

} // namespace svm

} // namespace sycl
} // namespace cl
```

## SVM capabilities

We propose SYCL follows OpenCL 2.2 definitions of different SVM capabilities,
as is already the case in the provisional SYCL 2.2 specification,
but using the new SYCL 1.2.1 mechanism for properties.

### No SVM

In order to retain backwards compatibility with OpenCL 1.2 devices,
the `cl::sycl::property::svm::none` property is provided.

### Coarse-grained buffer sharing

The `cl::sycl::property::svm::coarse` property can be used to specify
coarse-grained buffer sharing as defined in OpenCL 2.2:

* Memory on the host needs to be allocated using `cl::sycl::svm::allocator`.
* SVM allocated memory needs to be mapped before being accessed on host
  and unmapped after the programmer is done using it.
* The pointer obtained from SVM allocated memory must be registered with the
  command group handler before being used in the kernel.
  Using a pointer to SVM allocated memory in a kernel without registering it
  first is undefined behavior.
  The pointers to SVM allocated memory that are used in the kernel
  can be divided into two categories:
    1. A pointer that is passed directly to the kernel is considered a kernel
       argument and must be registered as a kernel argument.
       See `clSetKernelArgSVMPointer` in the OpenCL 2.2 specification
       for more details.
    1. A pointer that is not directly passed to the kernel, e.g. a pointer
       contained in a structure that is passed as a kernel argument,
       is not considered a kernel argument. Instead, it needs to be registered
       as additional kernel information. See `clSetKernelExecInfo`
       in the OpenCL 2.2 specification for more details.

### Fine-grained buffer sharing

The `cl::sycl::property::svm::fine` property can be used to specify
fine-grained buffer sharing as defined in OpenCL 2.2. It retains most of the
properties of coarse-grained buffer sharing, with two exceptions:

* SVM allocated memory does not need to be mapped before being accessed on host
  (and, thus, also doesn't need unmapping).
  However, mapping and unmapping are still allowed.
* Fine-grained buffer sharing can also support fine-grained synchronization
  (i.e. atomics across host and device).

### Fine-grained system sharing

The `cl::sycl::property::svm::system` property can be used to specify
fine-grained system sharing as defined in OpenCL 2.2. It drops most of the
requirements of fine-grained buffer sharing:

* Memory on the host does not need to be allocated using
  `cl::sycl::svm::allocator`. However, using `cl::sycl::svm::allocator` is still
  allowed.
* Pointers that are used in the kernel directly and are thus considered a
  kernel argument, must still be registered.
  * Used for dependency tracking on the host
    and setting kernel arguments on the device.
* Pointers that are used in the kernel indirectly and are thus not considered a
  kernel argument, do not need to be registered, though that is still allowed.

### Fine-grained synchronization using atomics

The `cl::sycl::property::svm::atomic` property can be used to specify the use of
fine-grained synchronization between host and device using atomics
(atomics for short). It can be used in combination with the
`cl::sycl::property::svm::fine` or `cl::sycl::property::svm::system` properties.

## Device selection

This proposal assumes it is possible to pass a `cl::sycl::property_list` to
a `cl::sycl::device_selector`. `cl::sycl::property_list` is defined in
SYCL 1.2.1, but is currently not used for device selection. We propose using
SVM properties passed to the device selector as a way to select a device with
the desired SVM capabilities. This mechanism of performing device selection
replaces the `cl::sycl::capability_selector` as defined by SYCL 2.2.

An example that creates a SYCL device that supports fine-grained buffer sharing
with atomics:

```cpp
using namespace cl::sycl;
auto selector = device_selector(property_list{svm::fine(), svm::atomic()});
auto dev = device(selector);
```

In this example, device selection would fail if there were no devices available
in the system that supported the requested capabilities.

### Potential API changes

If not already covered by another proposal, the `cl::sycl::device_selector`
class gains a new constructor that takes a property list.

| Member function | Description |
|-----------------|-------------|
| *`device_selector(const cl::sycl::property_list& propList)`* | Constructor that ensures at time of device selection that a device based on the properties contained in `propList` will be selected. |

## Allocating memory in SVM space

The coarse- and fine-grained buffer sharing modes of SVM require memory
be allocated using a special SVM allocator. SYCL 2.2 already defines
a `cl::sycl::svm_allocator` - this proposal renames it to
`cl::sycl::svm::allocator` and drops the `exec_capabilities` class template
parameter.

Additionally, instead of the constructor taking a `cl::sycl::context`,
it takes a `cl::sycl::queue` and a `cl::sycl::property_list`. We propose using
an instance of an SVM allocator to perform the host mapping and unmapping
in a later section, therefore the need for storing the queue. The SYCL context
can be obtained from a SYCL queue.

It is possible to query for the SYCL queue used in the allocator
using `cl::sycl::svm::allocator::get_queue`
and to change the SYCL queue used in the allocator,
using `cl::sycl::svm::allocator::set_queue`. The new queue must be associated
with the same context as the one that it is replacing.

`cl::sycl::svm::allocator` stores a list of properties that are used when
allocating memory in SVM space. It is possible to query an SVM allocator
instance for the individual properties that were supplied on construction.

### API changes

* `cl::sycl::svm_allocator` is renamed to `cl::sycl::svm::allocator`.
* The only template parameter `cl::sycl::svm::allocator` defines is `<class T>`.
* `cl::sycl::svm::allocator` does not store a shared pointer to a
  `cl::sycl::context`, instead it stores the SYCL queue in an unspecified way.
* `cl::sycl::svm::allocator` stores a list of properties.
* `cl::sycl::svm::allocator` has a single user-accessible constructor.
  This constructor takes a SYCL queue and a list of properties.
* The properties provided to the constructor can be any single property
  from the `cl::sycl::property::svm` namespace,
  or two properties from these allowed combinations:
  * `cl::sycl::property::svm::fine` and `cl::sycl::property::svm::atomic`
  * `cl::sycl::property::svm::system` and `cl::sycl::property::svm::atomic`

New member functions added:

| Member function | Description |
|-----------------|-------------|
| *`allocator(const cl::sycl::queue& q, const cl::sycl::property_list& propList)`* | Constructor that takes a queue and a list of properties. |
| *`cl::sycl::queue get_queue() const`* | Returns the queue currently associated with the allocator. |
| *`void set_queue(const cl::sycl::queue& q)`* | Associates the allocator with the SYCL queue `q`. The queue currently associated with the allocator and the new SYCL queue `q` must be associated with the same context, otherwise an `invalid_object_error` runtime error is thrown. |
| *`template <typename propertyT> bool has_property() const`* | Returns true if the allocator was constructed using the specified property, false otherwise. |
| *`template <typename propertyT> propertyT get_property() const`* | Returns the value of the specified property of type `propertyT` that was passed to the allocator on construction. Must throw an `invalid_object_error` SYCL exception if the allocator was not constructed the `propertyT` property. |

## Mapping memory for use on host

When trying to access memory in SVM space allocated using the property
`cl::sycl::property::svm::coarse`, the programmer must map the acquired
pointer before accessing its contents on the host,
and must unmap the same pointer after access to the contents in no longer
immediately needed on the host.
We propose adding two new free functions and a new class type:

* `cl::sycl::svm::map`
* `cl::sycl::svm::unmap`
* `cl::sycl::svm::map_guard`

The free function `cl::sycl::svm::map` takes an allocator instance, a pointer
to a region of SVM allocated memory, and the number of elements in the region,
and maps the region for use on the host.
It also takes a template parameter to specify the access mode.
The reason this is a free function is to enable generic programming.
The programmer can pass an allocator of any type - if the allocator instance is
of type `cl::sycl::svm::allocator` and the allocator instance was created using
the `cl::sycl::property::svm::coarse` property, the function will map SVM
allocated memory to the host.
The function implementation is empty for any other type of allocator.
By storing a SYCL queue on the allocator instance,
the function `cl::sycl::svm::map` can perform the mapping using that queue.
Performing an SVM map operation on non-SVM allocated memory is
undefined behavior.
The function does not block. Instead it returns a `cl::sycl::event` that the
programmer can wait on.

The free function `cl::sycl::svm::unmap` also takes an allocator instance and
a pointer to a region of SVM allocated memory
(the size of the region is not required) and unmaps the pointer.
An SVM unmap operation is performed only in the case the
allocator instance is of type `cl::sycl::svm::allocator` and
was created using the `cl::sycl::property::svm::coarse` property,
otherwise the function implementation is empty. Unmapping a pointer that was not
previously mapped is undefined behavior.

In order to simplify mapping and unmapping, we also propose the class
`cl::sycl::svm::map_guard`. This is a RAII type that acts similarly to a lock -
it calls `cl::sycl::svm::map` on construction
and `cl::sycl::svm::unmap` on destruction.
The constructor takes an allocator instance, a pointer to a region of SVM
allocated memory, and the number of elements in the region.
It also takes a template parameter to specify the access mode.
Every instance of `cl::sycl::svm::map_guard` has to be named,
otherwise the destructor would be called immediately,
unmapping the data immediately.
`cl::sycl::svm::map_guard` adopts the same allocator behavior as
`cl::sycl::svm::map` and `cl::sycl::svm::unmap`.
Unlike `cl::sycl::svm::map`, the constructor of `cl::sycl::svm::map_guard`
performs a blocking map operation.

We strongly suggest the usage of `cl::sycl::svm::map_guard`
instead of calling the map and unmap functions.
Using `cl::sycl::svm::map` and `cl::sycl::svm::unmap` on the same pointer
to SVM allocated memory already covered by the scope of
a `cl::sycl::svm::map_guard` instance is undefined behavior.

Because the mapping relies on the SYCL queue (contained withing the SVM
allocator) and the SVM allocator can be re-associated with a different queue by
calling `cl::sycl::svm::allocator::set_queue`, calling
`cl::sycl::svm::allocator::set_queue` while the SVM memory is mapped
to the host - either in between calls to `cl::sycl::svm::map` and
`cl::sycl::svm::unmap` or within the scope of a `cl::sycl::svm::map_guard`
variable - is undefined behavior and will likely lead to data not being properly
synchronized.

### API changes

We propose adding the following two free functions:

| Function | Description |
|----------|-------------|
| *`template <cl::sycl::access::mode mode = cl::sycl::access::mode::read_write, typename AllocatorT, typename T> cl::sycl::event cl::sycl::svm::map(AllocatorT alloc, T\* svm_ptr, int num_elements)`* | Maps the region of SVM allocated memory represented by `svm_ptr` followed by `num_elements` number of elements, to the host. `num_elements` defaults to 1. Will use the access mode `mode` when mapping the data. Does not block, but rather returns an event to the map operation. |
| *`template <typename AllocatorT, typename T> void cl::sycl::svm::unmap(AllocatorT alloc, T\* svm_ptr)`* | Unmaps previously mapped SVM allocated memory represented by `svm_ptr`. |

We also propose adding the class `cl::sycl::svm::map_guard`:

```cpp
namespace cl {
namespace sycl {
namespace svm {

// C++17 supports automatic class template parameter deduction,
// so along with defaulting the `mode` parameter to
// `cl::sycl::access::mode::read_write` using a deduction guide,
// the template parameters could all be deduced
template <cl::sycl::access::mode mode, typename AllocatorT, typename T>
struct map_guard {
  map_guard(AllocatorT alloc, T* svm_ptr, int num_elements = 1);
  ~map_guard();
};

} // namespace svm
} // namespace sycl
} // namespace cl
```

| Member function | Description |
|----------|-------------|
| *`map_guard(AllocatorT alloc, T\* svm_ptr, int num_elements)`* | Maps the region of SVM allocated memory represented by `svm_ptr` followed by `num_elements` number of elements, to the host. `num_elements` defaults to 1. `alloc` and `svm_ptr` are stored in the instance. Will use the access mode `mode`, specified as the class template parameter, when mapping the data. Performs a blocking map. |
| *`~map_guard()`* | Unmaps previously mapped SVM allocated memory represented by the pointer received on construction. |

## Data dependency checking

In SYCL 1.2.1 and SYCL 2.2, specifying an access mode when constructing an
accessor guides the data dependency checking.
We propose extending this to pointers to SVM allocated memory.
Similar to how mapping a pointer to SVM allocated memory to host
requires specifying the access mode, we propose specifying the access mode
when registering a pointer for use in the kernel.

On the other hand, in order to relax dependency checking when requested,
we also propose adding a new access mode, `cl::sycl::access::mode::concurrent`,
which signals that concurrent access to the same data
from different host or device functions is possible and should not block.
This access mode acts like `cl::sycl::access::mode::read_write`
without full dependency checking.
If a command group with concurrent access is preceded or followed by
a command group with non-concurrent access (or data is accessed on the host),
the dependency checking still needs to take place to ensure proper data
visibility when data is accessed in a non-concurrent manner.
When the same data is accessed in multiple command groups using the
concurrent access mode (or accessed concurrently on the host),
that data can be accessed concurrently without any of the command groups
blocking each other or blocking concurrent host access.

### API changes

Define the enum class `cl::sycl::access::mode` as such:

```cpp
namespace cl {
namespace sycl {
namespace access {

enum class mode {
  read = 1024,
  write,
  read_write,
  discard_write,
  discard_read_write,
  atomic,
  concurrent
};

} // namespace access
} // namespace sycl
} // namespace cl
```

## Changes to the command group handler

In SYCL 1.2.1, when calling `cl::sycl::queue::submit`,
the user passes a function object representing the command group.
This function object takes a reference to a `cl::sycl::handler` as the only
argument.
The SYCL 2.2 provisional specification introduces `cl::sycl::execution_handler`
that can accept different execution capabilities.
We propose replacing `cl::sycl::execution_handler` with `cl::sycl::handler`
with a single template parameter denoting the SVM capability it supports.
The reason for the template parameter is that the handler capability needs to be
known at compile time so that the compiler can produce meaningful errors.

Using different properties leads to three distinct handler types:

1. Command group handler with no SVM support
    * Template parameter is `cl::sycl::svm::none`
1. OpenCL 1.2 compatible command group handler with no SVM support
    * Template parameter is `cl::sycl::property::opencl12`
    * Assumes the OpenCL 1.2 memory model
1. Command group handler with SVM support
    * Actually three different command group handlers, depending on the SVM
      property provided, but with the exact same interface
    * Template parameter can be either `cl::sycl::svm::coarse`,
      `cl::sycl::svm::fine`, or `cl::sycl::svm::system`

The property `cl::sycl::svm::atomic` cannot be used as a template parameter to
the handler. Instead, `cl::sycl::svm::atomic` can be used as a template
parameter to the device handler. This is discussed separately in the "Atomics"
section of this document.

#### API changes

Define the following `cl::sycl::handler` class:

```cpp
namespace cl {
namespace sycl {

template <typename PropertyT>
class handler;

} // namespace sycl
} // namespace cl
```

### Command group handler with no SVM support

A command group handler of type `cl::sycl::handler<cl::sycl::svm::none>`
can be passed to the function object submitted to `cl::sycl::queue::submit`.
This type of handler offers no SVM capabilities and features the exact same
interface as `cl::sycl::handler` in SYCL 1.2.1.

#### API changes

Define a template specialization for `cl::sycl::handler` for the property
`cl::sycl::svm::none`:

```cpp
namespace cl {
namespace sycl {

template <>
class handler<cl::sycl::svm::none> {
  /// Exact same interface as cl::sycl::handler in SYCL 1.2.1
};

} // namespace sycl
} // namespace cl
```

### OpenCL 1.2 compatible command group handler with no SVM support

A command group handler of type
`cl::sycl::handler<cl::sycl::property::opencl12>`
can be passed to the function object submitted to `cl::sycl::queue::submit`.
This type of handler offers no SVM capabilities and features the exact same
interface as `cl::sycl::handler` in SYCL 1.2.1.
All the other command group handlers assume the OpenCL 2.2 memory model,
whereas the OpenCL 1.2 compatible command group handler assumes the OpenCL 1.2
memory model and therefore behaves the same as `cl::sycl::handler`
in SYCL 1.2.1.

#### API changes

Define a template specialization for `cl::sycl::handler` for the property
`cl::sycl::svm::opencl12`:

```cpp
namespace cl {
namespace sycl {

template <>
class handler<cl::sycl::property::opencl12> {
  /// Exact same interface as cl::sycl::handler in SYCL 1.2.1
};

} // namespace sycl
} // namespace cl
```

### Command group handlers with SVM capabilities

Using `cl::sycl::svm::coarse`, `cl::sycl::svm::fine`, or `cl::sycl::svm::system`
as a template parameter to `cl::sycl::handler` results in three SVM-enabled
command group handlers. The interface for each of these handlers is exactly the
same, but the member function functionality differs according to the properties
of the specified SVM capability as described in section "SVM capabilities".
These handlers provide all the functions of
`cl::sycl::handler<cl::sycl::svm::none>`
and also add new functions to support SVM.

#### API changes

In order to distinguish between the types of pointers to SVM allocated memory
that are going to be used in the kernel,
define an enum class `cl::sycl::svm::pointer_type`. See the section on
"Coarse-grained buffer sharing" for more details.

Also define template specializations for `cl::sycl::handler` for the properties
`cl::sycl::svm::coarse`, `cl::sycl::svm::fine`, and `cl::sycl::svm::system`,
and define the common interface:

```cpp
namespace cl {
namespace sycl {

namespace svm {

enum class pointer_type {
  direct,
  indirect
};

} // namespace svm

// PropertyT is one of:
//  - svm::coarse
//  - svm::fine
//  - svm::system
template <typename PropertyT>
class handler<PropertyT> {
 public:
  /// All the same member functions as handler<cl::sycl::svm::none>

  // New functions:

  template <access::mode AccessMode = access::mode::read_write,
            svm::pointer_type PtrType = svm::pointer_type::direct,
            typename T>
  void require(T* svm_ptr);
};

} // namespace sycl
} // namespace cl
```

| Member function | Description |
|-----------------|-------------|
| *`template <access::mode AccessMode = access::mode::read_write, svm::pointer_type PtrType, typename T> void require(T\* svm_ptr)`* | Register `svm_ptr`, which is a pointer to SVM allocated memory, for use in the kernel. `PtrType` defines how the pointer should be registered: as a kernel argument if `PtrType == svm::pointer_type::direct`, or as kernel information if `PtrType == svm::pointer_type::indirect`. `AccessMode` specifies what the pointer will be used for in terms of read/write access and may be used to track dependencies and help with optimizations. `AccessMode` cannot be `cl::sycl::access::mode::atomic`. If `PropertyT == cl::sycl::svm::system && PtrType == svm::pointer_type::indirect`, the function implementation is empty.  |

## Device handler

In order to support more features inside kernels,
we propose adding a `cl::sycl::device_handler` class.
Similarly to how `cl::sycl::handler` represents a link between application scope
and command group scope, the device handler represents a link between command
group scope and kernel scope.

### API changes

```cpp
namespace cl {
namespace sycl {

template <typename PropertyT>
class device_handler {
 public:
  device_handler() = delete;

  /// Member functions depending on PropertyT
};

} // namespace sycl
} // namespace cl
```

## Atomics

Fine-grained synchronization between host and device is possible using SVM
atomics. SVM atomics are restricted to two SVM modes: using fine-grained buffer
sharing or using fine-grained system sharing.
Because SVM atomics offer more functionality than regular atomics
and we also want to keep compatibility with OpenCL 1.2,
we propose extending the existing `cl::sycl::atomic` class from SYCL 1.2.1.

### Atomic property

When performing device selection, the `cl::sycl::property::svm::atomic`
property needs to be specified in order to select a device with support for
SVM atomics.

When allocating memory than can be used for fine-grained synchronization between
host and device (when either the entire allocated region or just part of it
is needed for atomic synchronization), the `cl::sycl::property::svm::atomic`
property needs to be specified at the point of constructing an instance of
`cl::sycl::svm::allocator`.

### Memory order and scope

We propose deprecating the `cl::sycl::memory_order` enum class
defined in SYCL 1.2.1 and instead using `std::memory_order` as defined in C++14.

Additionally, we propose adding the enum class `cl::sycl::memory_scope`.
This is used to specify the scope of the memory constraints specified by the
memory order. The memory constraints can apply to either work-item scope,
work-group scope, device scope, or apply to all SVM devices in the system.

#### API changes

Use `std::memory_order` in place of `cl::sycl::memory_order`.

Define a new enum class `cl::sycl::memory_scope`:

```cpp
namespace cl {
namespace sycl {

enum class memory_scope {
  work_item,
  work_group,
  device,
  all_svm_device
};

} // namespace sycl
} // namespace cl
```

### Atomic class

We propose extending `cl::sycl::atomic` with a template parameter `RequireSVM`.
`RequireSVM` can be one of the two following properties:
`cl::sycl::property::opencl12` or `cl::sycl::svm::atomic`.
Using `cl::sycl::property::opencl12` (which is the default)
leads to the exact same atomic class as already defined in SYCL 1.2.1.

Using `cl::sycl::svm::atomic` for `RequireSVM` results in the new atomic type
that allows fine-grained synchronization between host and device.
The interface is exactly the same for both OpenCL 1.2 and SVM variants of
`cl::sycl::atomic`, but separate types allow for different implementations.
Additionally, SVM memory can only be allocated in global memory space -
using anything else than `cl::sycl::access::address_space::global_space`
when `RequireSVM` equals `cl::sycl::svm::atomic`, is undefined behavior.

All the member functions that used to take a `cl::sycl::memory_order` argument
in SYCL 1.2.1 now take an `std::memory_order` instead. For the OpenCL 1.2 atomic
class, only `std::memory_order_relaxed` is allowed. For the SVM atomic class,
any memory order defined by `std::memory_order` is allowed.

The SVM atomic class does not need to be used with a pointer to SVM allocated
memory. It is possible to retrieve an SVM atomic instance from global or local
memory, i.e. from accessors to global or local memory, though in that case
it is not possible to perform fine-grained synchronization between host and
device.

#### API changes

New definition of `cl::sycl::atomic`:

```cpp
namespace cl {
namespace sycl {

template <typename T, access::address_space addressSpace =
access::address_space::global_space, typename RequireSVM = property::opencl12>
class atomic {
  /// All the same member functions as defined in SYCL 1.2.1
  /// cl::sycl::memory_order is replaced with std::memory_order
};

} // namespace sycl
} // namespace cl
```

### Atomic device handler

We propose adding a template specialization of the `cl::sycl::device_handler`
class for the property `cl::sycl::svm::atomic`.
The atomic device handler is passed as an extra argument to the
kernel function object. The atomic device handler can be passed to kernels used
with `cl::sycl::handler<svm::fine>` and `cl::sycl::handler<svm::system>`.

The atomic device handler is used for synchronization between host and device
by supplying some memory fence and barrier functions and for constructing
SVM atomic objects.

An example of using the atomic device handler:

```cpp
using namespace cl::sycl;
...
// Assume we've already initialized the queue and allocated memory in SVM space
q.submit([&](handler<svm::fine>& cgh) {
  // svm_atomic_ptr is a pointer to memory allocated in SVM space,
  // using the property property::svm::atomic
  cgh.require<svm::pointer_type::direct>(svm_atomic_ptr);

  cgh.parallel_for<class some_kernel>(
    range<1>(num_work_items),
    [=](id<1> i, device_handler<svm::atomic> dH) {
      // Option 1: Construct the atomic object manually
      using T = std::remove_pointer_t<decltype(svm_atomic_ptr)>;
      auto svm_atomic_1 = atomic<T, access::address_space::global_space,
                                 svm::atomic>(svm_atomic_ptr);
      // Option 2: Call get_atomic
      auto svm_atomic_2 = dH.get_atomic(svm_atomic_ptr);
      // Some kernel code
  });
});
...
```

#### API changes

Define a template specialization for `cl::sycl::device_handler` for the property
`cl::sycl::svm::atomic`:

```cpp
namespace cl {
namespace sycl {

template <>
class device_handler<svm::atomic> {
 public:
  template <typename T>
  atomic<T, access::address_space::global_space, svm::atomic>
  get_atomic(T* svm_ptr) const;

  void work_item_fence(access::fence_space fence_space,
                       std::memory_order order,
                       memory_scope scope) const;
};

} // namespace sycl
} // namespace cl
```

| Member function | Description |
|-----------------|-------------|
| *`template <typename T> atomic<T, access::address_space::global_space, svm::atomic> get_atomic(T\* svm_ptr) const`* | Retrieves an SVM-enabled `cl::sycl::atomic` instance for the pointer provided. `svm_ptr` must be a pointer to SVM allocated memory that was allocated with the `cl::sycl::svm::atomic` property. |
| *`void work_item_fence(access::fence_space fence_space, std::memory_order order, memory_scope scope) const`* | Creates a memory fence based on the fence space (global, local, or both), the required memory order, and the memory scope.  |

## Examples

### Generic Linked List

```cpp
using cl::sycl;

struct Node {
   int val_;
   Node* ptr_;
};

template <typename AllocatorT>
struct LinkedList {
  Node* head_;
  AllocatorT a_;

  LinkedList(AllocatorT a) : head_{nullptr}, a_{a} { };
  ~LinkedList() { };

  Node* add_node(int i) {
      Node* ret = a_.allocate(1);
      {
          // Option 1: Scoped
          svm::map_guard s(a_, ret);
          // Option 2: svm::map(a_, ret);
          ret->val_ = i;
          ret->ptr_ = head_;
          head_ = ret;
          // Option 2: svm::unmap(a_, ret);
      }
      return ret;
  }

  void remove_node(Node* n) {
    if (!n) {
      return;
    }
    // Find the previous element in the list
    Node* current = head_;

    while (true) {
      // Option 1: map_guard
      svm::map_guard s(a_, current);
      // Option 2: svm::map(a_, current);

      if (current->ptr_ == n || current->ptr_ == nullptr) {
        // Option 2: svm::unmap(a_, current);
        break;
      }

      // current will be overwritten, so we need to store into a temporary
      // svm::map_guard already stored the original pointer, so it's fine
      // Option 2: Node* currentTmp = current;
      current = current->ptr_;

      // Unmap the pointer that was originally mapped
      // Option 2: svm::unmap(a_, currentTmp);
    }
    if (current->ptr_ == n) {
      {
        // We're accessing members of both current and n,
        // so we need to map both
        svm::map_guard s(a_, current);
        svm::map_guard s(a_, n);
        current->ptr_ = n->ptr_;
      }
      a_.deallocate(n, 1);
    }
  }
};

int main() {
  // This example should work with svm::coarse, svm::fine, and svm::system
  constexpr auto svm_mode = property::svm::fine;

  // Setup device, context, queue
  auto selector = device_selector({svm_mode});
  auto q = queue(selector);
  auto ctx = q.get_context();
  auto dev = q.get_device();

  // Create the allocator from the queue
  auto allocator = svm::allocator<Node>(q, {svm_mode});

  // Create the linked list with the svm::allocator
  LinkedList<decltype(allocator)> list(allocator);

  for (int i = 0; i < 42; i++) {
      list.add_node(i);
  }

  q.submit([&](handler<svm_mode>& cgh) {
    Node* current = list.head_;

    // Register the head as a kernel argument
    // See clSetKernelArgSVMPointer
    cgh.require(current);

    // All the other nodes need to be passed as extra kernel info
    while (true) {
      auto host_svm_lock = svm::map_guard(allocator, current);
      current = current->ptr_;
      if (current == nullptr) {
          break;
      }
      // These calls can be grouped together into a single call to
      // clSetKernelExecInfo
      cgh.require<pointer_type::indirect>(current);
    }

    // Start with the head in the kernel
    current = list.head_;

    cgh.single_task<class double_value>([=]() {
      while (current != nullptr) {
          current->val_ *= 2;
          current = current->ptr_;
      }
    });
  });

  // Wait for the queue before the linked list can be accessed again
  q.wait_and_throw();

  return 0;
}
```

### Atomics

```cpp
using cl::sycl;

// This example should work with svm::fine and and svm::system
constexpr auto svm_mode = property::svm::fine;

// Setup device, context, queue
auto selector = device_selector({svm_mode, svm::atomic});
auto q = queue(selector);
auto ctx = q.get_context();
auto dev = q.get_device();

// Create the allocator from the queue
auto allocator = svm::allocator<Node>(q, {svm_mode, svm::atomic});

// Allocate SVM data
auto sum_ptr = allocator.allocate(1);

// Create an SVM atomic on the host
auto sum_atomic = cl::sycl::atomic<int,
                                   access::address_space::global_space,
                                   svm::atomic>(sum_ptr);

// Initialize sum to zero
sum_atomic.store(0, std::memory_order_release);

constexpr int num_work_items = 42;

q.submit([&](handler<svm_mode>& cgh) {
  // Register the SVM pointer as a kernel argument
  cgh.require(sum_ptr);

  cgh.parallel_for<class sum>(range<1>(num_work_items),
                              [=](id<1> i, device_handler<svm::atomic> dH) {
    auto sum_atomic = dH.get_atomic(sum_ptr);
    // auto sum_atomic = cl::sycl::atomic<int,
    //                                    access::address_space::global_space,
    //                                    svm::atomic>(sum_ptr);
    sum_atomic.fetch_add(i[0], std::memory_order_acq_rel);
  });
});

// Wait for the queue before the linked list can be accessed again
q.wait_and_throw();

// The kernel performed a sum of the first num_work_items elements
auto sum_value = sum_atomic.load(std::memory_order_acquire);
constexpr int expected = (num_work_items * (num_work_items + 1));
TEST_EQ(expected, sum_value);

// Need to deallocate the SVM data
allocator.deallocate(sum_ptr, 1);
```
