# Accessor aliases

## Table of Contents

* [Motivation](#motivation)
* [Revisions](#revisions)
* [Summary](#summary)
* Changes
  * [Defaulting template parameters](#defaulting-template-parameters)
  * [Simplifying access modes](#simplifying-access-modes)
  * [Aliases](#aliases)
  * [Extending the handler](#extending-the-handler)
* [Considerations and alternatives](#considerations-and-alternatives)

### TLDR: [Examples](#examples)

## Motivation

Accessors are the cornerstone of SYCL
as they are used inside and outside kernels to access data
in a pointer-like manner.
However, SYCL ecosystem projects have shown that
they can be a bit awkward to handle due to their verbosity.
Storing accessors in function objects requires specifying all template parameters,
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

## Revisions

### 0.2

* Simplified handling of non-writeable accessors
* Defined implicit conversions w.r.t. constness and access modes
* Removed `buffer_accessor` alias
* Removed type traits
* Removed member functions for returning aliases
* Added `access::mode` template parameter to `host_accessor`
* Removed section _Discouraging access mode combinations_
* Discuss CTAD
* Discuss trait for writeable accessors
* More and better examples
* Added table of contents
* Deprecate `access::mode::atomic` as part of this proposal
* Link to proposal that deprecates `accessor::placeholder`
* Minor fixes

### 0.1

* Initial proposal

## Summary

Main points of the proposal:

1. [Default `accessor` template parameters](#defaulting-template-parameters)
1. [Clarify and simplify access modes](#simplifying-access-modes)
    * `read` and `read_write` most important
    * Allow `const T` to denote read-only data access
    * Deprecate `access::mode::atomic`
1. [Introduce `access::target`-specific aliases for accessors](#aliases)
1. [Extending `handler::require`](#extending-the-handler)

The proposal slightly changes the semantics of accessing data,
but still aims to be completely backwards compatible with SYCL 1.2.1.

Note that this proposal assumes that any accessor can be a placeholder,
essentially deprecating the `access::placeholder` template parameter.
This is covered by a separate proposal:
[CP024 Default placeholders](https://github.com/codeplaysoftware/standards-proposals/pull/122).
It also doesn't address image accessors.
We discuss this and some other issues
in the [Considerations and alternatives](#considerations-and-alternatives) section.

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

## Simplifying access modes

We can think of an accessor as performing two basic functions:

1. Requesting access to data
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

Additionally, we propose that the constness of the data type be considered
when providing data access.

As a final step for simplifying access modes,
we propose `access::mode::atomic` be deprecated.
It is already possible to construct a `cl::sycl::atomic`
by using the pointer obtained from an accessor.

### Constness of data

Existing SYCL 1.2.1 code usually uses
non-`const`-qualified data types for accessor.
(For examples please take a look at SYCL ecosystem projects,
like SYCL-BLAS, SYCL-DNN, or the SYCL backend of TensorFlow.)
It might define it's own aliases to read and write accessors
when trying to store accessors in a kernel function object.

There is an example in the
[ComputeCpp SDK](https://github.com/codeplaysoftware/computecpp-sdk/blob/master/samples/template-function-object.cpp#L51)
that showcases a kernel functor with stored accessors.
It uses user-defined aliases `read_accessor` and `write_accessor`
to simplify the three private members -
two read accessors and one write accessor.
These kinds of user-defined aliases seem to be a regular occurrence
in SYCL ecosystem code.

We propose using the constness of the data type
to simplify some of these use cases.
An example on how the above-linked example could be rewritten:

```cpp
template <typename dataT>
class vector_add_kernel {
 public:
  vector_add_kernel(accessor<const dataT> ptrA,
                    accessor<const dataT> ptrB,
                    accessor<dataT> ptrC)
      : m_ptrA(ptrA), m_ptrB(ptrB), m_ptrC(ptrC) {}

  void operator()(item<1> item) { ... }

 private:
  accessor<const dataT> m_ptrA;
  accessor<const dataT> m_ptrB;
  accessor<dataT> m_ptrC;
};
```

To enable this, we propose allowing `dataT` accessor template parameter
to be `const`,
but only when the access mode is `read` or `read_write`.
It's natural to allow it for the `read` access mode
since `const dataT` and `access::mode::read`
express essentially the same concept
when it comes to accessing data.

The reason to allow `const dataT` for the `read_write` access mode
is because this is the default access mode,
which enables writing `accessor<const dataT>` in the above example.
The SYCL scheduler treats this combination
as if the access mode was `access::mode::read`.

### Subscript operator

After _requesting_ access to data by constructing an accessor,
`accessor::operator[]` is the function that enables
the second main functionality of the accessor class:
it _provides_ access to data.

We propose that `accessor::operator[]` returns `reference_t`,
which is defined as:

* `const T&` when the access mode is `access::mode::read`
  or when `dataT` is `const`-qualified
* `T&` otherwise

### Implicit conversions

In order to simplify user code,
we propose allowing certain implicit conversions
that don't modify scheduling information.

In standard C++ code, adding `const` qualifiers is almost always allowed.
In SYCL, going from a `read_write` accessor to a `read` accessor
is analogous to adding the `const` qualifier.
This allows us to have the following rules regarding `const`:

1. Convert an accessor of `dataT` data type
   to an accessor of `const dataT` data type,
   all other template parameters being equal
1. Convert an accessor of `const dataT` data type
   and `access::mode::read_write` mode
   to an accessor of `dataT` and `access::mode::read` mode,
   all other template parameters being equal
1. Convert an accessor of `const dataT` data type
   and `access::mode::read_write` mode
   to an accessor of `const dataT` and `access::mode::read` mode,
   all other template parameters being equal
1. A combination of the previous rules,
   allow any of these accessor combinations to be implicitly convertible
   between each other:
    * {`dataT`, `read`}
    * {`const dataT`, `read`}
    * {`const dataT`, `read_write`}

An example of code these rules enable:

```cpp
buffer<int> buf{...};

void const_int_read_write(accessor<const int> acc) {...}
void int_read(accessor<int, 1, access::mode::read> acc) {...}
void const_int_read(accessor<const int, 1, access::mode::read> acc) {...}

q.submit([&](handler& cgh){
    auto accInt =
        buf.get_access(cgh); // accessor<int, 1, read_write>
    auto accIntConst =
        accessor<const int>{buf, cgh}; // accessor<const int, 1, read_write>

    // 1
    // accInt requested `read_write` mode,
    // the scheduler may or may not be able to optimize this
    const_int_read_write(accInt);

    // 2
    // Scheduler treats the combination of `const int` and `read_write` mode
    // the same as if `read` mode was used,
    // so this code is optimal
    int_read(accIntConst);

    // 3
    // Similar to rule 2
    const_int_read(accIntConst);

    // 4
    // Allow all combinations
    const_int_read_write(accIntConst);
    const_int_read_write(accessor<int, 1, access::mode::read>{});
    const_int_read_write(accessor<const int, 1, access::mode::read>{});
    int_read(accInt);
    int_read(accessor<const int>{});
    int_read(accessor<const int, 1, access::mode::read>{});
    const_int_read(accInt);
    const_int_read(accessor<const int>{});
    const_int_read(accessor<const int, 1, access::mode::read>{});

    ...
});
```

We also propose the following rules for simplifying the access mode:

5. Convert an accessor of certain access modes
   to an accessor of mode `access::mode::read_write`,
   all other template parameters being equal
    * Allowed modes are `access::mode::write`, `access::mode::discard_write`, `access::mode::discard_read_write`

```cpp
buffer<int> buf{...};
q.submit([&](handler& cgh){
    // 5
    // The scheduler has been instructed to ignore previous data,
    // even though the resulting accessor has a `read_write` mode,
    // this is optimal code
    accessor<int> acc1 = buf.get_access<access::mode::discard_read_write>(cgh);

    ...
});
```

## Aliases

With all of the above changes and simplifications applied,
we propose adding the following alias templates:

```cpp
namespace cl {
namespace sycl {

template <class dataT, int dims = 1>
using constant_buffer_accessor =
    accessor<dataT,
             dims,
             access::mode::read,
             access::target::constant_buffer>;

template <class dataT,
          int dims = 1,
          access::mode accMode = access::mode::read_write>
using host_accessor =
    accessor<dataT,
             dims,
             accMode,
             access::target::host_buffer>;

} // namespace sycl
} // namespace cl
```

`constant_buffer_accessor` is very similar to `local_accessor`
in that it can only have one access mode.
It is allowed to use both `const dataT` and `dataT` as the data type,
the read-only access mode ensures data cannot be written to.

The `host_accessor` alias is similar to the regular global buffer accessor.

## Extending the handler

Reducing effective access modes from 6 to 2
would have some impact on the scheduler
which could now have less information to guide the scheduling process.
This would be especially problematic for placeholder accessors,
where the type of the accessor is determined
before they are registered with a command group.

To resolve this, we propose the following extensions to `handler::require`:

* Allow it to be called on any non-host-mode accessor
* Add an overload that takes an access mode as a template parameter
* Return the accessor instance that was passed in

See [Calling require](#calling-require) for an example.

```cpp
namespace cl {
namespace sycl {

class handler {
 public:
  /// Existing functions
  /// ...

  /// New functions

  // Registers an accessor with a command group submission
  // `requestedMode` can be used to weaken the access mode
  template <access::mode requestedMode,
            class T,
            int dims,
            access::mode mode,
            access::target target,
            access::placeholder isPlaceholder>
  accessor<T, dims, mode, target, isPlaceholder>
  require(accessor<T, dims, mode, target, isPlaceholder> acc);

  // Registers an accessor with a command group submission
  // Already existed in 1.2.1, now it can take any accessor
  // and return the same accessor back
  template <class T,
            int dims,
            access::mode mode,
            access::target target,
            access::placeholder isPlaceholder>
  accessor<T, dims, mode, target, isPlaceholder>
  require(accessor<T, dims, mode, target, isPlaceholder> acc) {
    return this->require<mode>(acc);
  }
};

} // namespace sycl
} // namespace cl
```

| Member function | Description |
|-----------------|-------------|
| *`template <class T, int dims, ...> accessor<T, dims, ...> require(accessor<T, dims, ...> acc)`* | Registers the accessor for command group submission. Host accessors are not allowed. Returns `acc`. |
| *`template <access::mode requestedMode, class T, int dims, ...> accessor<T, dims, ...> require(accessor<T, dims, ...> acc)`* | Registers the accessor for command group submission. `requestedMode` can be used to weaken the access mode. `requestedMode` cannot be a write mode if the accessor mode is `access::mode::read`. Host accessors are not allowed. Returns `acc`. |

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

This proposal builds on top of accessors to be a placeholders without a template parameter:
[CP024 Default placeholders](https://github.com/codeplaysoftware/standards-proposals/pull/122).

The current proposal still works without that assumption,
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

### Class template argument deduction

It was suggested CTAD would help with some of the accessor verbosity
when compiling in C++17 mode.
We agree, but there are limitations:
C++17 doesn't allow CTAD on alias templates.
That would mean that while CTAD might work well with `access::target::global_buffer`,
since that's the default target and one can just use `accessor`,
it wouldn't work with `access::target::constant_buffer`
or `access::target::host_buffer`
since those rely on alias templates
`constant_buffer_accessor` and `host_accessor`, respectively.
This also affects the `local_accessor` alias already in SYCL 1.2.1.

An option for solving this pre-C++20 would be
to define `constant_buffer_accessor` and `host_accessor` as new types
instead of alias templates.
They would publicly inherit from the `accessor` class
using the appropriate access target.

However, this would also require defining additional constructors,
implicit conversions, and deduction guides for the feature to work as desired.

### Detecting writeable accessors

We considered adding a type trait that would indicate
whether the accessor is writeable or not.
This relates to [Constness of data](#constness-of-data)
and [Subscript operator](#subscript-operator),
where data cannot be modified if the underlying data type is `const`-qualified
or if the access mode is read-only.

This could either be a `constexpr static` member of the accessor class
or a class template.

## Examples

* [Simple global and host accessor use](#simple-global-and-host-accessor-use)
* [Vector addition](#vector-addition)
* [Simpler accessor construction](#simpler-accessor-construction)
* See [Storing accessors in a kernel functor](#constness-of-data)
* See [Implicit conversions](#implicit-conversions)

### Simple global and host accessor use

```cpp
buffer<int> buf{...};
queue q;
q.submit([&](handler& cgh) {
  accessor<int> a{buf, cgh};
  ... // Write to buffer
});
{
  host_accessor<const int> a{B};
  ... // Read from buffer
}
```

### Vector addition

The following example uses C++17 and CTAD.

```cpp
std::vector<int> a{1, 2, 3, 4, 5};
std::vector<int> b{6, 7, 8, 9, 10};

const auto N = a.size();
const auto bufRange = range<1>(N);

queue myQueue;

// Create a buffer and copy `a` into it
buffer<int> bufA{bufRange};
myQueue.submit([&](handler &cgh) {
  accessor accA{bufA, cgh}; // accessor<int, 1, read_write, global_buffer>
  assert(!accA.is_null());
  assert(accA.has_handler());
  cgh.require<access::mode::discard_write>(
      accA); // Doesn't change type, scheduler can ignore previous data
  cgh.copy(a.data(), accA);
});

// Create a buffer and copy `b` into it
// Use placeholders
accessor<int> accB; // accessor<int, 1, read_write, global_buffer>
assert(accB.is_null());
assert(!accB.has_handler());
buffer<int> bufB{bufRange};
accB = accessor{bufB}; // accessor<int, 1, read_write, global_buffer>
assert(!accB.is_null());
assert(!accB.has_handler());
myQueue.submit([&](handler &cgh) {
  cgh.require<access::mode::discard_write>(
      accB); // Doesn't change type, scheduler can ignore previous data
  cgh.copy(b.data(), accB);
});

// Submit kernel that writes to output buffer
// Use constant buffer accessors
buffer<int> bufC{bufRange};
myQueue.submit([&](handler &cgh) {
  accessor<const int> A{
      bufA, cgh}; // accessor<const int, 1, read_write, global_buffer>
  constant_buffer_accessor B{
      bufB, cgh}; // accessor<const int, 1, read, constant_buffer>
  auto C = cgh.require<access::mode::discard_write>(
      accessor{bufC}); // accessor<int, 1, read_write, global_buffer>
  cgh.parallel_for<class vec_add>(bufRange,
                                  [=](id<1> i) { C[i] = A[i] + B[i]; });
});

{
  // Request host access
  host_accessor<const int> accC{
      bufC}; // accessor<const int, 1, read_write, host_buffer>
  assert(!accC.is_null());
  assert(!accC.has_handler());
  for (int i = 0; i < N; ++i) {
    std::cout << accC[i] << std::endl;
  }
}
```

### Simpler accessor construction

The following code shows two ways of expressing the same thing -
first in SYCL 1.2.1 code, and the second way according to this proposal.
It also assumes that accessor template parameters are defaulted in SYCL 1.2.1
as proposed in
[Defaulting template parameters](#defaulting-template-parameters),
even though that's not allowed in SYCL 1.2.1.

```cpp
using namespace cl::sycl;

// Assume these are available
handler cgh;
buffer<int> buf;

// Global buffer access
accessor<int> bufAcc =
    buf.get_access(cgh);
accessor<int> bufAccNew{buf, cgh};

// Global buffer access, read-only
accessor<int,
          1,
          access::mode::read>
    bufAccRead = buf.get_access<access::mode::read>(cgh);
accessor<const int> bufAccReadNew{buf, cgh};

// Global buffer access, ignore previous data
accessor<int,
          1,
          access::mode::discard_read_write>
    bufAccDiscard = buf.get_access<access::mode::discard_read_write>(cgh);
accessor<int> bufAccNewDiscard =
    buf.get_access<access::mode::discard_read_write>(cgh);

// Constant buffer access
accessor<int,
          1,
          access::mode::read,
          access::target::constant_buffer>
    bufAccConst = buf.get_access<access::mode::read, access::target::constant_buffer>(cgh);
constant_buffer_accessor<const int> bufAccConstNew{buf, cgh};

// Host buffer access
accessor<int,
          1,
          access::mode::read_write,
          access::target::host_buffer>
    bufAccHost = buf.get_access();
host_accessor<int> bufAccHostNew{buf};

// Host buffer access, read-only
accessor<int,
          1,
          access::mode::read,
          access::target::host_buffer>
    bufAccHostRead = buf.get_access<access::mode::read>();
host_accessor<const int> bufAccHostNewRead{buf};

// Host buffer access, ignore previous data
accessor<int,
          1,
          access::mode::discard_read_write,
          access::target::host_buffer>
    bufAccHostDiscard = buf.get_access<access::mode::discard_read_write>();
host_accessor<int> bufAccHostNewDiscard =
    buf.get_access<access::mode::discard_read_write>();
```

### Calling `require`

```cpp
using namespace cl::sycl;

// Assume these are available
queue q;
buffer<int> bufA;
buffer<int> bufB;
buffer<int> bufC;

q.submit([](handler& cgh) {
  // Request read-only access to bufA
  accessor<const int> accA{bufA, cgh};
  // Register accA for command group submission
  // Not so useful in this case since it's already been registered,
  // but accA could also be a placeholder
  cgh.require(accA);

  // Create a placeholder with read-only access to bufB
  // The accessor is immediately registered and returned
  // accB is of type accessor<const int>
  auto accB = cgh.require(accessor<const int>{bufB});

  // Create a placeholder with read-write access to bufC
  // The accessor is immediately registered and returned
  // The provided access mode instructs the scheduler to ignore previous data
  // and "weaken" the scheduling mode to write-only
  // accC is of type accessor<int>
  auto accC = cgh.require<access::mode::discard_write>(accessor<int>{bufC});

  ...
});
```
