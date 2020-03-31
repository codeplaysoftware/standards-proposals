# Default placeholders

| Proposal ID | CP024  |
|-------------|--------|
| Name | Default placeholders |
| Date of Creation | 9 March 2019 |
| Revision | 0.2 |
| Latest Update | 31 March 2020 |
| Target | SYCL Next (after 1.2.1) |
| Current Status | _Work in Progress_ |
| Reply-to | Peter Žužek <peter@codeplay.com> |
| Original author | Peter Žužek <peter@codeplay.com> |
| Contributors | Gordon Brown <gordon@codeplay.com> |

## Overview

This proposal aims to deprecate `access::placeholder`
and instead allow all accessors to global and constant memory to be placeholders.

## Revisions

### 0.1

* Initial proposal

## Motivation

SYCL 1.2.1 introduced the `access::placeholder` enumeration
which is used as the 5th accessor template parameter
to indicate whether the accessor can be used as a placeholder.
Only the `global_buffer` and `constant_buffer` access targets
support placeholder accessors.

The main reason for having placeholders is to store accessors into objects
without having a queue at the point of object creation,
and registering the access with a command group submission at a later time.
One of Codeplay's proposals that didn't make it into SYCL 1.2.1
was to allow placeholders to be default constructible as well,
i.e. not having to even know the buffer at the point of construction.
This extension is used in some SYCL ecosystem projects.

Accessors are pointer-like objects,
and placeholders try to fill that gap
that prevents accessors from being even more pointer-like.
A default constructed placeholder accessor is analogous to a null pointer.
A placeholder that's bound to a buffer
but hasn't been registered with a command group
is more like a fancy pointer:
the user doesn't own the data
until the accessor is registered and used in a kernel,
where it becomes more similar to a regular pointer.

Having this type separation between full accessors and placeholders
might be useful from a type safety perspective,
but we believe it makes development more difficult.
For example, [another one of our proposals](https://github.com/codeplaysoftware/standards-proposals/pull/100/files)
introduces alias templates for different access targets
and revises rules on how read-only accessors are handled,
all in the name of reducing accessor verbosity.
The placeholder template parameter makes that much more difficult,
meaning we either need to introduce another parameter to the alias template,
making it a lot less useful,
or simply ignore the reduction in verbosity for placeholder accessors.

## Changes

### Deprecate `access::placeholder`

Mark the `access::placeholder` enum class as deprecated,
but keep it for backwards compatibility
until it's eventually removed from a subsequent standard.

```cpp
namespace access {
...

enum class placeholder // deprecated
{...};
} // namespace access

template <typename dataT,
          int dimensions,
          access::mode accessMode,
          access::target accessTarget = access::target::global_buffer,
          access::placeholder isPlaceholder = access::placeholder::false_t>
class accessor;
```

### All accessors with a global or constant target can be placeholders

SYCL 1.2.1 allows `access::placeholder::true_t`
only when the access target is `global_buffer` or `constant_buffer`.
We propose that the same placeholder semantics
still apply to only these two targets,
just that the semantics and API is available
regardless of the `isPlaceholder` template parameter.

### Accept all accessors in `handler::require`

At the moment the member function `handler::require` only accepts
placeholder accessors.
This should stay the same,
but since the enum class is deprecated,
the function needs to be extended:

```cpp
class handler {
 public:
  ...

  // Adds isPlaceholder to existing SYCL 1.2.1 function
  template <typename dataT,
            int dimensions,
            access::mode accessMode,
            access::target accessTarget,
            access::placeholder isPlaceholder>
  void require(accessor<dataT,
                        dimensions,
                        accessMode,
                        accessTarget,
                        isPlaceholder>
                acc);
};
```

`handler::require` has to be called on a placeholder accessor
in order to register it with the command group submission.
It is valid to call the function more than once,
even on non-placeholder accessors.
Calling the function on a null accessor throws `cl::sycl::invalid_object_error`.

### Deprecate `is_placeholder`

The function `accessor::is_placeholder` doesn't make sense anymore,
we propose deprecating it.

### Allow constructing host accessors from placeholders

Consider the following example:

```cpp
template<typename AccTypeA, typename AccTypeB>
void some_library_function(AccTypeA accA, AccTypeB accB) {
  ...
  myQueue.submit([&](handler &cgh) {
    cgh.require(accA);
    cgh.require(accB);
    cgh.copy(accA, accB);
  });
  ...
  // We want to be able to access host data now
}
```

`some_library_function` in the example takes in two placeholder accessors
and performs a copy from one to another.
However, there is no way any of the data associated with the accessors
can be accessed on the host.
The placeholders are not bound to a command group anyway,
so we believe it should be possible to explicitly construct a host accessor
from a placeholder accessor.

### New constructors

We propose adding new constructors to the accessors class
to allow placeholder construction.

1. Default constructor - not part of SYCL 1.2.1,
   but there is a [Codeplay proposal](https://github.com/codeplaysoftware/standards-proposals/pull/89)
   for making that happen.
1. 0-dim accessor constructor from a buffer -
   normally an accessor constructor requires a buffer and a handler,
   we propose making the handler optional.
   This is the same constructor currently allowed for host buffers.
1. Constructor from a buffer -
   normally an accessor constructor requires a buffer and a handler,
   we propose making the handler optional.
   This is the same constructor currently allowed for host buffers.
1. Construct a host accessor from a placeholder one (`placeholderAcc`).
   Not valid to call in kernel code.
   Throws `cl::sycl::runtime_error` when called
   if `placeholderAcc.has_handler() == true`.
   Requesting host access is a synchronization point,
   and host accessors act as locks,
   meaning that the placeholder cannot be used
   while the host accessor is in scope.
   Even after host access is released,
   the programmer is required to call `require` again on the placeholder
   before it can be used in a kernel.

```cpp
template <typename dataT,
          int dimensions,
          access::mode accessMode,
          access::target accessTarget = access::target::global_buffer,
          access::placeholder isPlaceholder = access::placeholder::false_t>
class accessor {
 public:
  ...

  // 1
  // Only available when ((accessTarget == access::target::global_buffer) ||
  //                      (accessTarget == access::target::constant_buffer))
  accessor() noexcept;
  
  // 2
  // Only available when: ((accessTarget == access::target::global_buffer) ||
  //                       (accessTarget == access::target::constant_buffer)) &&
  //                      (dimensions == 0)
  explicit
  accessor(buffer<dataT, 1> &bufferRef);

  // 3
  // Only available when: ((accessTarget == access::target::global_buffer) ||
  //                       (accessTarget == access::target::constant_buffer)) &&
  //                      (dimensions > 0)
  accessor(buffer<dataT, dimensions> &bufferRef,
           range<dimensions> accessRange,
           id<dimensions> accessOffset = {});

  // 4
  // Only available when (accessTarget == access::target::host_buffer) &&
  //                     ((otherTarget == access::target::global_buffer) ||
  //                      (otherTarget == access::target::constant_buffer))
  template <access::target otherTarget, access::placeholder otherPlaceholder>
  explicit
  accessor(accessor<dataT,
                    dimensions,
                    accessMode,
                    otherTarget,
                    otherPlaceholder>&
            placeholderAcc);
};
```

### New `accessor` member functions

In order to query the accessor for its status,
we propose new member functions to the `accessor class`:

1. `is_null` - returns `true` if the accessor has been default constructed,
   which is only possible with placeholders.
   Not having an associated buffer is analogous to a null pointer.
   Available in both application code and kernel code,
   it is valid to pass a null accessor to a kernel.
1. `has_handler` - returns `true` if the accessor is associated
   with a command group `handler`.
   Will only be `false` with host accessors and placeholder accessors.
   This replaces the `is_placeholder` member function.
   Mainly meant as a way to enquire about whether this is a placeholder or not,
   this doesn't have to be checked before `require` is called.
1. `get_host_access` - constructs a host accessor from a placeholder accessor.
   Not valid to call in kernel code.

```cpp
template <typename dataT,
          int dimensions,
          access::mode accessMode,
          access::target accessTarget = access::target::global_buffer,
          access::placeholder isPlaceholder = access::placeholder::false_t>
class accessor {
 public:
  ...

  // 1
  bool is_null() const noexcept;
  
  // 2
  bool has_handler() const noexcept;

  // 3
  // Only available when ((accessTarget == access::target::global_buffer) ||
  //                      (accessTarget == access::target::constant_buffer))
  accessor<dataT,
           dimensions,
           accessMode,
           access::target::host_buffer,
           access::placeholder::false_t>
  get_host_access() const;
};
```

## Examples

Simple vector addition:

```cpp
std::vector<int> a{1, 2, 3, 4, 5};
std::vector<int> b{6, 7, 8, 9, 10};

using read_acc =
  accessor<int, 1, access::mode::read, access::target::global_buffer>;
using write_acc =
  accessor<int, 1, access::mode::discard_write, access::target::global_buffer>;

read_acc accA;
read_acc accB;
write_acc accC;

// Sanity checks
assert(accA.is_null());
assert(!accA.has_handler());

const auto N = a.size();
const auto bufRange = range<1>(N);

queue myQueue;

// Create a buffer and copy `a` into it
buffer<int> bufA{bufRange};

accA = read_acc{bufA};
assert(!accA.is_null());
assert(!accA.has_handler());

myQueue.submit([&](handler &cgh) {
  cgh.require(accA);
  cgh.copy(a.data(), accA);
});

// Create a buffer and copy `b` into it
buffer<int> bufB{bufRange};
accB = read_acc{bufB};
myQueue.submit([&](handler &cgh) {
  cgh.require(accB);
  cgh.copy(b.data(), accB);
});

// Submit kernel that writes to output buffer
buffer<int> bufC{bufRange};
accC = read_acc{bufC};
myQueue.submit([&](handler &cgh) {
  cgh.require(accA);
  cgh.require(accB);
  cgh.require(accC);
  cgh.parallel_for<class vec_add>(bufRange,
                                  [=](id<1> i) { accC[i] = accA[i] + accB[i]; });
});
```
