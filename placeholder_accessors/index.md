# Placeholder Accessors

| Proposal ID | CP004 |
|-------------|--------|
| Name | Placeholder Accessors  |
| Date of Creation | 20 July 2016 |
| Target | SYCL 2.2 |
| Current Status | _Work In Progress_ |
| Reply-to | Ruyman Reyes <ruyman@codeplay.com> |
| Original author | Ruyman Reyes <ruyman@codeplay.com> |
| Requirements | CP001 |
| Contributors | Gordon Brown <gordon@codeplay.com>, Victor Lomuller <victor@codeplay.com>, Mehdi Goli <mehdi.goli@codeplay.com>, Peter Zuzek <peter@codeplay.com>, Luke Iwanski <luke@codeplay.com>, Michael Haidl <michael.haidl@uni-muenster.de> |

## Overview

SYCL enables C++ developers to fully integrate the usage of accelerators into their applications
with a single-source approach. This facilitates developers to port more complex code bases
to OpenCL platforms with less effort. In some cases, such as TensorFlow or Parallel STL, this porting would not be possible without a single-source programming model.
The feedback received from various projects suggests that restricting the creation
of accessors to the command group makes difficult porting existing applications.
This particularly affects the porting of complex data structures that would normally contain pointers to data.
In this proposal, we present a special type of accessor, called a _placeholder_
that is not associated (bound) to a specific command group handler on creation,
but that can be bound later on after its creation.

## Revisions

This is the initial revision of the proposal.

## Motivation

A common recurring pattern from SYCL developers is to reference an OpenCL
memory object inside a host structure, that is later used on the device.
Examples of this case can be seen in [SYCL-BLAS][1] or [TensorFlow/Eigen][2].
In other cases, users want to create a reference to a future accessor, for example,
in iterators for [ParallelSTL][3].
This is currently not possible, as the SYCL specification mandates that accessors
can only be constructed with a handler, which only exists inside of a command group.
The rationale of this requirement still holds: Accessors define not only a pointer to an object
on the device, but are also used for dependency analysis and identification of kernel
arguments. Any of those can be defined outside of a command group scope.


An example of this situation can be seen in the SYCL-BLAS _view_ classes.
In normal C++, the _view_ class stores a reference to a container, such
as _std::vector<ScalarT>_.
The elements of the container can be accessed using _ScalarT& view::eval(int elemId)_ method.
When implementing a SYCL variant of the view, developers encountered the problem that,
although the _view_ class can store a reference to the buffer, the _eval_ method cannot
be implemented, since buffers cannot be accessed directly.
Creating a host accessor will enable accessing the element on the host, but not
on the device. In addition, it is not possible to return a reference to the element
since the host accessor will be destroyed at the end of the function, therefore
rendering the reference invalid.

Another example of this situation can be seen in [TensorFlow issue][4]

The current implementation of SYCL-BLAS uses a compile-time depth-first-search
mechanism to replace the usage of buffers to accessors, recreating the same
user-defined tree with a new device tree that has accessors instead of buffers.
(See the *blas::make_accessor* function on SYCL-BLAS
code for implementation details).
This effectively offers a different type to the users on the device than
in the host, although somehow the user of the library does not need to care
about them.

In this particular case, the ability for developers to create accessors
not bound to a specific context simplifies the development.
The _blas::view_ will store a "placeholder accessor" instead of a reference to a
buffer. This can be registered (bound) to a specific command group later on.


## Placeholder accessor

A placeholder accessor defines an accessor type that is not bound to a specific
command group.
The placeholder accessor still cannot be accessed on the host, keeping the
SYCL semantics. However, on the device side, the placeholder accessor can
be used as a normal device accessor to the type of memory associated with it.
In order to associate an accessor with the actual command group where it
will be executed, the accessor needs to be registered using the handler
method *require*.

The following example illustrates a simple use case:

```
buffer<int, 1> buf(range<1>(10));
// We construct the placeholder accessor
// The SYCL buffer will be alive for as long as this accessor is
// alive
auto pAcc = buf.get_access<access::mode::read_write, access::target::global,
                           access::placeholder::true_t>();

myQueue.submit([&](handler& h) {
  // We register this accessor to use it on the kernel
  h.require(pAcc);
  h.single_task<class myKernel>([=]() {
    pAcc[0] = 3;
  })
});

myQueue.submit([&](handler& h) {
  // We register this accessor for using it on the kernel
  h.require(pAcc);
  h.single_task<class myKernel>([=]() {
    pAcc[0] = 2;
  })
});

/* Invalid  - Placeholders are not host accessors.
   However, this will only cause an error (exception) at runtime.
ASSERT(pAcc[0] == 2) */

{ // Host accessors can be constructed as usual from the buffer
  auto hostAcc = buf.get_accessor<access::mode::read_write,
                                  access::target::global>();
  ASSERT(hostAcc[0] == 2);
}
```

A placeholder accessor type on its own does not define an access requirement
since it is not associated with a command group.
Therefore, the creation of a placeholder accessor does not affect the
scope of accessing the data or the memory updates on device or host.
In a way, a _placeholder accessor represents a set of requirements to
a memory object to an unknown context_, enabling the lazy definition of
the context to which the requirements can be applied.

The registration of the placeholder accessor on the command group creates a
requirement on the command group to access the underlying memory object.
The action will be defined at runtime as with any other accessor type.

The placeholder accessor is not a replacement for the normal accessors,
and it is intended for advanced SYCL developers that need to port
existing code-bases.

## API changes

A new enum class inside the accessor types needs to be defined:

```
namespace access {
enum class placeholder {
   false_t,  // This is a normal accessor
   true_t  // This is a placeholder accessor
}; // placeholder
} // access
```

An extra template parameter should be added to the accessor class, which
will be defaulted to false to ensure the current behaviour of accessors is
not modified.

```
template <typename elemT, int kDims, access::mode kMode, access::target kTarget,
          access::placeholder placeholderT = access::placeholder::false_t>
class accessor;
```

A new method must be added to the accessor class:

```
constexpr bool is_placeholder() const;
```

This returns true if the accessor is a placeholder, and false otherwise.

### When `is_placeholder` returns false

The accessor API should be the same as the current SYCL specification,
except for the aforementioned modifications.

### When `is_placeholder` returns true

The accessor API features constructors that don't take the handler parameter
and/or memory object as a constructor. Accessors can then be default
constructed, and the memory object can be assigned later when registering
the accessor in the command group.

In addition, a new method to obtain a normal accessor from the placeholder
accessor is provided.
This enables users to retrieve a normal accessor that can be used in
other command groups that don't require placeholder accessors directly.

```
accessor<T, dim, mode, target,
          access::placeholer::false_t> get_access(handler& h) const;
```


### New handler API entries

The handler gains a new method,
`handler::require(accessor<T, dim, mode, target, acess::placeholder::true_t>)`
that registers the requirements of the placeholder accessor on the given
command group.

Another method, that allows specifying the memory object the placeholder
accessor will be associated is also provided:

`handler::require(buffer<T, dim> b,
    accessor<T, dim, mode, target, access::placeholder::true_t>)`

### Placeholder `accessor` without a buffer

If a placeholder accessor which was not constructed with a buffer is not tied
to a buffer within a command group, then an exception is thrown. An accessor
can be checked for a buffer using `has_buffer()`.

|Member function        |Description                                              |
|-----------------------|---------------------------------------------------------|
|bool has_buffer() const|Returns true if the accessor is associated with a buffer,|
|                       |and false otherwise.                                     |

[1]: https://github.com/codeplaysoftware/sycl-blas "SYCL-BLAS"
[2]: https://github.com/lukeiwanski/tensorflow "TensorFlow/Eigen"
[3]: https://github.com/KhronosGroup/SyclParallelSTL
[4]: https://github.com/lukeiwanski/tensorflow/issues/89
