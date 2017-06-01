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
This concept vagely exists when using Shared Virtual Memory constructors
on buffers, but it is defined as a specific use case for SVM buffers.
With a specific `tied_buffer` class we aim to take advantage of
the lower-overhead of a single-context buffer and simplify the 
usage of SVM memory on SYCL 2.2

## Interface changes

This proposal adds a new type of buffer, called `tied_buffer` which
constructs a SYCL buffer that is tied to only one context, and therefore
will only be accessible from queues that are associated with that context.
The `tied_buffer` is derived from the `buffer` class to enable creating 
collections of both types of buffers. However, methods are not inherited
publicly to enforce type safety across buffer types.

A separate buffer type makes a clear distintion between the normal SYCL
buffer and this context-specific variant.

All the API interface for the normal buffers is available.


```cpp
template<typename T, int dim,
          typename AllocatorT = cl::sycl::buffer_allocator<T>>
class tied_buffer : protected buffer <T, AllocatorT> {

  public:
    tied_buffer(context& syclContext, T * hostPointer, 
                const range<dim> bufferRange,
                AllocatorT allocator = {});

    tied_buffer(context& syclContext, const T * hostPointer, 
                const range<dim> bufferRange,
                AllocatorT allocator = {});

    tied_buffer(context& syclContext, shared_ptr_class<T> hostData,
                const range<dim> bufferRange,
                const AllocatorT allocator = {});

    tied_buffer(context& syclContext, 
                const range<dimensions> &bufferRange,
                AllocatorT allocator = {});

    template<typename OAllocatorT = AllocatorT>
    tied_buffer(context& syclContext, tied_buffer<T, dim, OAllocatorT> &tb,
                const id<dimensions> &baseIndex,
                const range<dimensions> &subRange,
                AllocatorT allocator = {});

    tied_buffer<T, 1, AllocatorT)(context& syclContext,
                InputIterator first, InputIterator last,
                AllocatorT allocator = {});

    tied_buffer(const tied_buffer<T, dim, AllocatorT> &rhs);

    ~tied_buffer();

    size_t get_count() const;

    size_t get_size() const;

    AllocatorT get_allocator() const;

    context get_context() const;

		template <access::mode mode, access::target target = access::global_buffer>
			accessor<T, dimensions, mode, target> get_access(
					handler &command_group_handler);
		template <access::mode mode, access::target target = access::host_buffer>
			accessor<T, dimensions, mode, target> get_access();
		void set_final_data(weak_ptr_class<T> &finalData);
};
```

Note that in this case we can offer a `get_context` method for the memory
object, that returns the SYCL context that this `tied_buffer` is associated
with.

The interface of the SYCL buffer requires certain changes to 
implement the inheritance: 
The destructor of the buffer must be marked `virtual` and `override`.

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

By enabling the usage of the `svm_allocator` only with the `tied_buffer`
type, we avoid accidental invalidation of SVM pointers by presenting
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
tied_buffer<float, 1> tmp{deviceContext, myRange};

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
However, if the tied buffer is used on an invalid context, this must
raise an error.

## Alternative implementations

Alternatively to a derived class from the buffer, we could explore
other options for the interface, such as a different allocator type
or a template specialization for the buffer constructor.
However, these other alternative interface changes have larger impact
on the specification, so they are left from this proposal at this point.

## References

* SYCL 1.2 specification
* SYCL 2.2 specification
