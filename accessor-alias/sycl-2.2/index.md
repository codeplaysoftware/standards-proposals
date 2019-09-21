# Accessor aliases

## Motivation

Accessors are the cornerstone of SYCL
as they are used inside and outside kernels to access data
in a pointer-like manner.
However, SYCL ecosystem projects have shown that
they can be a bit awkward to handle due to their verbosity.
Storing accessors in functors requires specifying all template parameters
and accessors of different access modes are treated as different types,
even though there isn't much difference in accessing data.

### Existing `local_accessor` alias

SYCL 1.2.1 already has an alias for local memory,
which can serve as an illustrative example of an alias reducing verbosity.

```cpp
// Without the alias
accessor<int,
          1,
          access::mode::read_write,
          access::target::local>
    localAccOld{range<1>{32}};

// With the alias
local_accessor<int>
    localAcc{range<1>{32}};
```

## Summary

This proposal introduces access-target-specific aliases
for accessors.
In order to simplify access modes,
the aliases use only one of two modes:
`access::mode::read` for `const dataT` data parameters
and `access::mode::read_write` for non-const `dataT` data parameters.
This allows each alias to have only two template parameters,
`dataT` and `dims`, where `dims` is also defaulted to `1`.

The new aliases are:
* `buffer_accessor<dataT, dims>`
* `constant_buffer_accessor<dataT, dims>`
* `host_accessor<dataT, dims>`

We also propose defaulting all accessor template parameters,
except for the type parameter.

The proposal slightly changes the semantics of accessing data,
but still aims to be completely backwards compatible with SYCL 1.2.1.

Note that this proposal assumes that any accessor can be a placeholder,
essentially deprecating the `access::target::placeholder` template parameter.
This is covered by a separate proposal.
It also doesn't address image accessors.
We discuss this and some other issues
in the [Considerations and alternatives](#considerations-and-alternatives) section.

## Simplifying access modes

We can think of an accessor as performing two basic functions:
1. Requesting data access
2. Providing access to data

When requesting access to a buffer,
the user has to specify the access mode.
There are 6 access modes in the SYCL 1.2.1 specification,
and selecting the right modes when requesting access
can provide a lot of information to the scheduler.

However, when it comes to the second function of the accessor,
providing access to data,
the plethora of access modes is less useful.
If we consider a "standard", non-fancy C++ pointer,
it can either provide read and write access to the underlying data,
or read-only access (pointer-to-const).
It cannot provide write-only access -
that kind of functionality would be reserved to only niche applications anyway.

We would like accessors to behave in a similar manner.
As mentioned, access modes are still very useful to the scheduler,
so we would not deprecate any of them.
Instead, we propose that when it comes to providing data access,
there are only two relevant access modes:
1. `access::mode::read`
2. `access::mode::read_write`

This means that accessors would only provide different APIs
(in terms of accessing data)
for these two access modes,
and any other mode would resolve to one of these two.

Additionally, we propose that these two access modes
are directly tied to the constness of the data type.
So `access::mode::read` would be tied to `const dataT`
and "discourage" a non-const `dataT`,
while `access::mode::read_write` would be tied to a non-const `dataT`
and "discourage" `const dataT`.
The "discouraging" part isn't quite obvious
and we discuss it a bit more in the
[Discouraging access mode combinations](#discouraging-access-mode-combinations)
section.


### Type traits for resolving access modes

In order to help with resolving access modes and the constness of types,
we propose adding a few type traits:

```cpp
namespace cl {
namespace sycl {

/// access_mode_constant
template <access::mode mode>
using access_mode_constant =
  std::integral_constant<access::mode, mode>;

/// access_mode_from_type
template <typename dataT>
struct access_mode_from_type
    : access_mode_constant<access::mode::read_write> {};
template <typename dataT>
struct access_mode_from_type<const dataT>
    : access_mode_constant<access::mode::read> {};

/// type_from_access_mode
template <typename dataT, access::mode requestedMode>
struct type_from_access_mode {
    using type = dataT;
    static_assert(
        (!std::is_const<dataT>::value || (requestedMode == access::mode::read)),
        "Cannot request write access to const data");
};
template <typename dataT>
struct type_from_access_mode<dataT, access::mode::read> {
    using type = const dataT;
};
template <typename dataT, access::mode requestedMode>
using type_from_access_mode_t =
    typename type_from_access_mode<dataT, requestedMode>::type;

} // namespace sycl
} // namespace cl
```

| Type trait      | Description |
|-----------------|-------------|
| *`template <access::mode mode> access_mode_constant`* | Alias that stores `access::mode` into `std::integral_constant`. |
| *`template <typename dataT> access_mode_from_type`* | Deduces access mode based on the constness of `dataT`. |
| *`template <typename dataT, access::mode requestedMode> type_from_access_mode`* | Deduces the constness of `dataT` based on the access mode. Fails when requesting write access on `const dataT`. |

## Defaulting template parameters

We propose defaulting all accessor template parameters
except for the type parameter.
Defaulting to a read-only 1-dimensional global buffer accessor
reduces a large amount of verbosity for the simplest cases
and makes it easier to prototype SYCL code.

```cpp
namespace cl {
namespace sycl {

template <
  typename dataT,
  int dims = 1,
  access::mode accMode = access::mode::read_write,
  access::target accTarget = access::target::global_buffer,
  access::placeholder isPlaceholder = access::placeholder::false_t>
class accessor;

} // namespace sycl
} // namespace cl
```

## Aliases

We propose the following aliases to accessors:

```cpp
namespace cl {
namespace sycl {

template <class dataT, int dims = 1>
using buffer_accessor =
    accessor<dataT,
             dims,
             access_mode_from_type<dataT>::value,
             access::target::global_buffer>;

template <class dataT, int dims = 1>
using constant_buffer_accessor =
    accessor<dataT,
             dims,
             access::mode::read,
             access::target::constant_buffer>;

template <class dataT, int dims = 1>
using host_accessor =
    accessor<dataT,
             dims,
             access_mode_from_type<dataT>::value,
             access::target::host_buffer>;

} // namespace sycl
} // namespace cl
```

## Explicit conversions to aliases

We propose allowing an accessor
that isn't quite the same type as one of the aliases -
for example, by having a `write` instead of a `read_write` access mode -
to be explicitly converted to an alias type.

```cpp
namespace cl {
namespace sycl {

template <
  typename dataT,
  int dims = 1,
  access::mode accMode = access::mode::read_write,
  access::target accTarget = access::target::global_buffer,
  access::placeholder isPlaceholder = access::placeholder::false_t>
class accessor {
 public:
  /// All existing members here

  ...

  // Explicit conversion to `buffer_accessor`
  // Only allowed when `accTarget == access::target::global_buffer`
  explicit operator
    buffer_accessor<
      type_from_access_mode_t<dataT, accMode>, dims>();

  // Explicit conversion to `constant_buffer_accessor`
  // Only allowed when `accTarget == access::target::constant_buffer`
  explicit operator
    constant_buffer_accessor<
      type_from_access_mode_t<const dataT, accMode>, dims>();

  // Explicit conversion to `host_accessor`
  // Only allowed when `accTarget == access::target::host_buffer`
  explicit operator
    host_accessor<
      type_from_access_mode_t<dataT, accMode>, dims>();
};

} // namespace sycl
} // namespace cl
```

| Member function | Description |
|-----------------|-------------|
| *`explicit operator buffer_accessor<type_from_access_mode_t<dataT, accMode>, dims>()`* | Performs a cast to `buffer_accessor`. Only allowed when `accTarget == access::target::global_buffer` and if `accMode` doesn't discard the constness of `dataT`. |
| *`explicit operator constant_buffer_accessor<const dataT, dims>()`* | Performs a cast to `constant_buffer_accessor`. Only allowed when `accTarget == access::target::constant_buffer`. |
| *`explicit operator host_accessor<type_from_access_mode_t<dataT, accMode>, dims>()`* | Performs a cast to `host_accessor`. Only allowed when `accTarget == access::target::host_buffer` and if `accMode` doesn't discard the constness of `dataT`. |

## Extending buffer class to return aliases

In order to maintain backwards compatibility with SYCL 1.2.1,
the `get_access` member function should not change,
apart from defaulting the template parameters.
Instead, we propose adding new member functions to the `buffer` class
that request data access and return one of the new aliases.

```cpp
namespace cl {
namespace sycl {

template <typename dataT,
          int dims = 1,
          typename AllocatorT = cl::sycl::buffer_allocator>
class buffer {
 public:
  /// All existing members here

  ...

  
  /// Existing `get_access` gains defaulted parameters

  template <access::mode mode = access::mode::read_write,
            access::target target = access::target::global_buffer>
  accessor<dataT, dims, mode, target>
      get_access(handler& cgh);
  
  template <access::mode mode = access::mode::read_write>
  accessor<dataT, dims, mode, access::target::host_buffer>
      get_access();


  /// New functions

  template <access::mode mode = access::mode::read_write>
  buffer_accessor<
    type_from_access_mode_t<dataT, mode>, dims>
      get_device_access(handler& cgh);

  constant_buffer_accessor<const dataT, dims>
      get_device_constant_access(handler& cgh);

  template <access::mode mode = access::mode::read_write>
  host_accessor<
    type_from_access_mode_t<dataT, mode>, dims>
      get_host_access();
};

} // namespace sycl
} // namespace cl
```

| Member function | Description |
|-----------------|-------------|
| *`template <access::mode mode = access::mode::read_write> buffer_accessor<type_from_access_mode_t<dataT, mode>, dims> get_device_access(handler& cgh)`* | Calls `get_access<mode, access::target::global_buffer>`, but returns a `buffer_accessor`. |
| *`constant_buffer_accessor<const dataT, dims> get_device_constant_access(handler& cgh)`* | Calls `get_access<access::mode::read, access::target::constant_buffer>`, but returns a `constant_buffer_accessor`. |
| *`template <access::mode mode = access::mode::read_write> host_accessor<type_from_access_mode_t<dataT, mode>, dims> get_host_access()`* | Calls `get_access<mode, access::target::host_buffer>`, but returns a `host_accessor`. |

## Extending the handler

Reducing effective access modes from 6 to 2
would have some impact on the scheduler
which could now have less information to guide the scheduling process.
This would be especially problematic for placeholder accessors,
where the type of the accessor is determined
before they are registered with a command group.
To resolve this, we propose extending the `handler` class
by allowing `require` to be called on any accessor
and to add an overload of `require` that also takes an access mode.

An example would be to call `require` with `global_accessor<int>`
and `access::mode::discard_read_write`,
which would inform the scheduler to not copy over any old data.

```cpp
namespace cl {
namespace sycl {

class handler {
 public:
  /// Existing functions
  /// ...

  /// New functions

  // Registers an accessor with a command group submission
  template <class T,
            int dims,
            access::mode mode,
            access::target target,
            access::placeholder isPlaceholder>
  void require(accessor<T, dims, mode, target, isPlaceholder> acc);

  // Registers an accessor with a command group submission
  // `requestedMode` is a hint to the scheduler
  template <class T,
            int dims,
            access::mode mode,
            access::target target,
            access::placeholder isPlaceholder>
  void require(accessor<T, dims, mode, target, isPlaceholder> acc,
                access::mode requestedMode);
};

} // namespace sycl
} // namespace cl
```

| Member function | Description |
|-----------------|-------------|
| *`template <class T, int dims, ...> void require(accessor<T, dims, ...> acc)`* | Registers the accessor for command group submission. |
| *`template <class T, int dims, ...> void require(accessor<T, dims, ...> acc, access::mode requestedMode)`* | Registers the accessor for command group submission. `requestedMode` is used as a hint to the scheduler. `requestedMode` cannot be a write mode if the accessor mode is `access::mode::read`. |

## Considerations and alternatives

### Image accessors

Images are different from buffers
in the sense that they have three basic access modes
instead of just two for buffer:
in addition to being read-only and read-write,
they can also be write-only.
Additionally, read-write images are not supported on all devices,
this is even an extension in OpenCL.
This prevents us from using the constness of the data type,
which only has two states,
to determine the access mode.
Any kind of image accessor alias
would thus need to incorporate the access mode as a template parameter,
reducing the usability of such alias.

Image array access is another area of contention
and we are not sure what approach would best suit
in reducing verbosity there.

### Placeholder accessors

This proposal builds on top of allowing any accessor to be a placeholder.
The proposal still works without that assumption,
but its usefulness is significantly reduced.
If any accessor can be a placeholder,
then the `isPlaceholder` template parameter becomes obsolete,
making it easy to reduce the 5 template parameters of the `accessor`
to just 2 for each alias.
Without that change,
placeholder accessors would not benefit from this proposal,
unless some changes are made.

Here are some options:
1. Add an `isPlaceholder` parameter to the aliases, default it to `false_t`.
   This wouldn't reduce accessor verbosity as much as originally planned,
   by still requiring three template parameters for placeholder accessor aliases.
2. Introduce more aliases based on whether the accessor is a placeholder or not.
   We don't consider this option very desirable
   since it just replaces one kind of verbosity for another.

### Discouraging access mode combinations

It is not clear to what extent should the specification discourage
the use of a non-const accessor `dataT` with `access::mode::read`
and a const accessor `dataT` with a write mode.
We considered deprecating those use cases,
or maybe just adding a warning,
or even `static_assert` to prevent their usage.
However, at least the example of `access::mode::read` with a non-const `dataT`
is very common in existing SYCL code
because using `const` is not considered essential in SYCL 1.2.1.

## Examples

### Storing accessors in a kernel functor

There is an example in the
[ComputeCpp SDK](https://github.com/codeplaysoftware/computecpp-sdk/blob/master/samples/template-function-object.cpp)
that showcases a kernel functor with stored accessors.
It uses user-defined aliases `read_accessor` and `write_accessor`
to simplify the three private members -
two read accessors and one write accessor.
These kinds of user-defined aliases seem to be a regular occurrence
in SYCL ecosystem code.
As shown by the example below,
this proposal would essentially standardize the different aliases
from across different projects.

```cpp
using namespace cl::sycl;

template <typename dataT>
class vector_add_kernel {
 public:
  vector_add_kernel(buffer_accessor<const dataT> ptrA,
                    buffer_accessor<const dataT> ptrB,
                    buffer_accessor<dataT> ptrC)
      : m_ptrA(ptrA), m_ptrB(ptrB), m_ptrC(ptrC) {}

  void operator()(item<1> item) {
    m_ptrC[item.get_id()] = m_ptrA[item.get_id()] + m_ptrB[item.get_id()];
  }

 private:
  buffer_accessor<const dataT> m_ptrA;
  buffer_accessor<const dataT> m_ptrB;
  buffer_accessor<dataT> m_ptrC;
};

```

### New `get_access` functions

```cpp
using namespace cl::sycl;

// Assume these are available
handler cgh;
buffer<int> buf;

// Global buffer access
accessor<int> bufAcc =
    buf.get_access(cgh);
buffer_accessor<int> bufAccNew =
    buf.get_device_access(cgh);

// Global buffer access, read-only
accessor<int,
          1,
          access::mode::read>
    bufAccRead = buf.get_access<access::mode::read>(cgh);
buffer_accessor<const int> bufAccReadNew =
    buf.get_device_access<access::mode::read>(cgh);

// Global buffer access, ignore previous data
accessor<int,
          1,
          access::mode::discard_read_write>
    bufAccDiscard = buf.get_access<access::mode::discard_read_write>(cgh);
buffer_accessor<int> bufAccNewDiscard =
    buf.get_device_access<access::mode::discard_read_write>(cgh);

// Constant buffer access
accessor<int,
          1,
          access::mode::read,
          access::target::constant_buffer>
    bufAccConst = buf.get_access<access::mode::read, access::target::constant_buffer>(cgh);
constant_buffer_accessor<const int> bufAccConstNew =
    buf.get_device_constant_access(cgh);

// Host buffer access
accessor<int,
          1,
          access::mode::read_write,
          access::target::host_buffer>
    bufAccHost = buf.get_access();
host_accessor<int> bufAccHostNew =
    buf.get_host_access();

// Host buffer access, read-only
accessor<int,
          1,
          access::mode::read,
          access::target::host_buffer>
    bufAccHostRead = buf.get_access<access::mode::read>();
host_accessor<const int> bufAccHostNewRead =
    buf.get_host_access<access::mode::read>();

// Host buffer access, ignore previous data
accessor<int,
          1,
          access::mode::discard_read_write,
          access::target::host_buffer>
    bufAccHostDiscard = buf.get_access<access::mode::discard_read_write>();
host_accessor<int> bufAccHostNewDiscard =
    buf.get_host_access<access::mode::discard_read_write>();
```

### Calling `require`

```cpp
using namespace cl::sycl;

// Assume these are available
queue q;
buffer<int> buf1;
buffer<int> buf2;

q.submit([](handler& cgh) {
  // Request read-write access to both buffers
  buffer_accessor<int> bufAcc1 =
      buf1.get_device_access(cgh);
  buffer_accessor<int> bufAcc2 =
      buf2.get_device_access(cgh);

  // Register the accessor for command group submission
  // Not so useful in this case since it's already been registered,
  // but bufAcc1 could also be a placeholder
  cgh.require(bufAcc1);

  // Register the accessor for command group submission
  // bufAcc2 has also already been registered,
  // but this instructs the scheduler to ignore previous data
  cgh.require(bufAcc2, access::mode::discard_read_write);

  ...
});

```
