# Buffer Properties

| Proposal ID | CP008 |
|-------------|--------|
| Name | Buffer Properties |
| Date of Creation | 14 March 2017 |
| Target | SYCL 2.2 |
| Current Status | _Work In Progress_ |
| Reply-to | Ruyman Reyes <ruyman@codeplay.com> |
| Original author | Ruyman Reyes <ruyman@codeplay.com> |
| Contributors | Gordon Brown <gordon@codeplay.com>, Mehdi Goli <mehdi.goli@codeplay.com> |

## Overview

This proposal aims to define an interface for specialising the construction of the buffer class whilst still allowing buffers of different types to be stored in a generic container, via the use of buffer tags. 
Each tag represent a different property of a buffer.

## Revisions

This revision clarifies the behaviour of the tags and how they map to buffer
properties.

## Requirements

This proposal aims to provide a solution to two problems.

Firstly many use cases for specialisations of the buffer have emerged, each providing alternate semantics to the traditional buffer. These include a context tied buffer, a map buffer and an SVM buffer:

* The context tied buffer is a buffer that can only be associated with a single context which can be useful particularly for clarifying the semantics when using OpenCL interoperability; this requires an additional member function on the buffer to return the associated context which is not normally available.
* The map buffer is a buffer that does not allocate memory within the runtime and instead uses the host provided pointer directly; this mainly affects the runtime semantics of the buffer but also restricts the constructors that are available for the buffer.
* The SVM buffer is a buffer that allocates memory as shared virtual memory (either course-grained or buffer level fine grained).

Secondly, buffers currently suffer from the problem that they cannot be stored generically, for example, if a user wanted to have a vector of buffers and store a map buffer with a non-map buffer.

## Previous Proposals

We have considered many alternative approaches to solving these problems:

* We considered having unique classes such as a `tied_buffer` class, which would be able to provide additional semantics and interface but would inherit from the original buffer class. However this approach is limited as you cannot combine the buffer types described above, for example, you could not have a tied buffer that is also a map buffer.

## Proposal

The proposal is to add a variadic parameter pack to the constructors of the buffer class that can be used to specify tags to a buffer. This will mean removing the default argument for the allocator parameter and instead, having two overloads of each constructor instead.

```cpp
template <typename dataT, int kElems, typename allocatorT = buffer_allocator<dataT>>
class buffer {
  ...

  template <typename... tagTN>
  buffer(T *hostData, const range<kElems> bufferRange, tagTN... tags);

  template <typename... tagTN>
  buffer(T *hostData, const range<kElems> bufferRange, allocatorT alloc, tagTN... tags);

  ...
};
```

With this interface, a user can specify a specialisation of the buffer class by simply providing one or more values of the following tag enum class to the constructor of a buffer.

```cpp
namespace property {
  struct map_ptr;
  struct cl_interop;
  struct gl_interop;
  struct svm;  // SYCL 2.2 Only
};
```

Some of these tags will require an additional interface for the buffer class, for example, the `property::context_bound` tag will need a `get_context()` member function.

```cpp
template <typename dataT, int kElems, typename allocatorT = buffer_allocator<dataT>>
class buffer {
  ...

  context get_context() const;

  ...
};
```

However functions such as `get_context()` will only be valid if the `buffer` was constructed with the `property::context_bound` tag.
The user can query for a specific property on any buffer by using the
*has\_property* method.

```cpp
template <typename dataT, int kElems, typename allocatorT = buffer_allocator<dataT>>
class buffer {
  ...

  bool has_property(tag t)

  ...
};
```

## Defined properties

### SYCL 1.2.1 

* *map_ptr*: Buffer will use the given pointer exclusively for host access.

```cpp
struct map_ptr {

  map_ptr() = default;
};
```

* *cl_interop*: Buffer constructed for interoperability with OpenCL cl\_mem objects

```cpp
struct cl_interop {
  ...
  // Construct a property for interoperability
  cl_interop(cl_mem clMemObject) { }
  // Retrieves the OpenCL memory object
  cl_mem get_cl_mem();
};
```


* *gl_interop*: Buffer constructed for interoperability with OpenGL objects

```cpp
struct gl_interop {
  gl_interop() = default;
};
```

### SYCL 2.2

All the SYCL 1.2.1 properties plus:

* *svm*: The buffer is used for SVM allocation purposes

```cpp
struct svm {
  svm() = default;
};
```

## Example

The following example creates a list of buffers with different
properties.

```cpp
int main() {

  std::vector<buffer<int, 1>> bufferList;

  // Normal buffer
  bufferList.push_back(buffer<int, 1>(hostPtr, range));
  // Buffer that is maping a host pointer with the device
  bufferList.push_back(buffer<int, 1>(hostPtr, range, 
                                      property::map));
  // Buffer using an interop constructor
  bufferList.push_back(buffer<int, 1>(hostPtr, range, 
                                      property::cl_interop(clMemObject)));

  for(auto& buf : bufferList) {
    if (buf.has_property<property::cl_interop>())) {
      // Custom OpenCL-interop code
    }
  }
}
```

## Alternative Solutions

An alternative solution that was considered was to have the buffer tags be specified as template arguments to the buffer class.

```cpp
template <typename dataT, int kElems, typename allocatorT, typename... tagTN>
class buffer;
```

The benefit of this approach is that the interface of the buffer class can be specialised (providing alternate constructors or additional member functions) with each tag that is specified. Unfortunately, this approach is problematic as the variadic template pack would conflict with the default argument for the allocator template parameter.

Allowing a generic container of buffers would also be slightly different in this approach, there would be a specialisation of the buffer class for no tags that can act as a base class that is convertible with other buffer types with tags and vice versa.

A potential solution to this would be to either require that the user always specifies the allocator explicitly when they are using tags or that we have some kind of tag container type that wraps the variadic parameter pack for the tags.

## References

* SYCL 1.2 Specification: https://www.khronos.org/registry/SYCL/specs/sycl-1.2.pdf
* SYCL 2.2 Specification (provisional): https://www.khronos.org/registry/SYCL/specs/sycl-2.2.pdf
