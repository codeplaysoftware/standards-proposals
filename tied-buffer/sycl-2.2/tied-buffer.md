# Buffer tied to a Context

| Proposal ID | CP008 |
|-------------|--------|
| Name | Buffer tied to a Context |
| Date of Creation | 14 March 2017 |
| Target | SYCL 2.2 |
| Current Status | _Work In Progress_ |
| Reply-to | Ruyman Reyes <ruyman@codeplay.com> |
| Original author | Ruyman Reyes <ruyman@codeplay.com> |
| Contributors | Gordon Brown <gordon@codeplay.com>, Mehdi Goli <mehdi.goli@codeplay.com> |

## Overview

This proposal aims to offer a buffer object that is tied to a specific SYCL 
context, which allow  users to provide extra information to the SYCL 
implementation to optimize usage for a single context and device.
This concept vaguely exists when using Shared Virtual Memory constructors
on buffers, but it is defined as a specific use case for SVM buffers.
With a specific `tied_buffer` class we aim to take advantage of
the lower-overhead of a single-context buffer and simplify the 
usage of SVM memory on SYCL 2.2

## Revisions

This revision uses buffer properties instead of a separate buffer
class to describe the behaviour of the tied buffer.

## Interface changes

This proposal adds a new property for buffers, *buffer::property::context\_bound*.
A SYCL buffer with the *context bound* property can only be used with one context,
and its values are only accessible from queues that are associated with
said context.

A separate buffer property makes a clear distinction between the normal SYCL
buffer and this context-specific variant:

```cpp
struct context_bound {
  context_bound(cl::sycl::context context);
};

All the API interface for the normal buffers is available.


```cpp
template<typename T, int dim,
          typename AllocatorT = cl::sycl::buffer_allocator<T>>
class buffer {
  public:

// Additional method with property context_bound
    context get_context() const;
};
```

Note that in this case we can offer a `get_context` method for the memory
object, that returns the SYCL context that this `tied_buffer` is associated
with.

In case of an error, the `runtime_error` exception should be thrown.

## Shared Virtual Memory support

The current SYCL 2.2 specification adds an extra constructor to the `buffer`
class that takes a context, but it is only enabled when using the specific
`svm::allocator` type.
However, this constructor uses an entirely different set of rules
than the rest of the buffer constructors, effectively behaving like a
separate object. 
If a buffer is constructed this way, then the contents of the
SVM are invalidated. However, the user has no way of knowing that this
has happened, and may expect the data on the SVM pointer to be valid.
When passing the buffer across libraries, this situation may occur
even without the user knowledge.

By enabling the usage of the `svm_allocator` only in buffers with
the  `context_bound` property, 
we avoid accidental invalidation of SVM pointers by presenting
a clear differentiation between a buffer tied to a context
and a buffer that can move across multiple ones.

## Example

The following example is a simplification of a reduction kernel 
that uses a temporary storage to avoid modifying the input buffer.
The intermediate buffer only needs to be used on the context associated
with the queue, hence developer uses a `tied_buffer` to hint to the SYCL
runtime that this storage is only required on a single context.

```cpp
context deviceContext;
queue q{context};
range<3> myRange{3, 3, 3};
/* A normal buffer that holds the data for 
 * the execution.
 */
buffer<float> myBuffer{someHostPointer, myRange};

/**
 * We can use the normal buffer on any other queue,
 * SYCL will migrate transparently across contexts if required.
 */
std::experimental::for_each(sycl_named_policy<example>(otherQueue),
      std::begin(myBuffer), std::end(myBuffer),
      [=](float &elem) { 
          elem = 1.0
  });

/* This temporary memory only needs to exist 
 * on the context where the operation will be
 * performed.
 */
buffer<float, 1> tmp{deviceContext, myRange, 
                     buffer::property::context_bound(deviceContext)};

bool firstIter = true;

auto length = tmp.get_size();

do {
  auto f = [&](handler &h) mutable {
    auto aIn = myBuffer.template get_access<access::mode::read>(h);
    auto aInOut = tmp.template get_access<access::mode::read_write>(h);
    accessor<float, 1, access::mode::read_write, access::target::local>
          scratch(range<1>(local), h);
    
    if (first) {
      h.parallel_for(r, ReductionKernel(aIn, aInOut));
      first = false;
    } else {
      h.parallel_for(r, ReductionKernel(aInOut, aInOut));
    }
  };
  q.submit(f);
  length = length / local;
} while (length > 1);

// Retrieve the result from the tmp buffer
{
  auto h = tmp.get_accessor<access::mode::read>(h);
  std::cout << " Reduction value " << h[0] << std::endl;
}
```

## Implementation notes

An implementation may decide to not take advantage of this hint and 
simply construct a normal buffer object underneath.
However, if the bound buffer is used on an invalid context, this must
raise an error.

## Alternative implementations

The original approach was to use a derived class, but has now
been updated to use the new properties mechanism.

## References

* SYCL 1.2 specification
* SYCL 2.2 specification
