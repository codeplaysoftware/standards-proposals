# Temporary buffer

| Proposal ID | CP017 |
|-------------|--------|
| Name | Host access |
| Date of Creation | 17 September 2017 |
| Target | SYCL 1.2.1 vendor extension |
| Current Status | _Work In Progress_ |
| Reply-to | Ruyman Reyes <ruyman@codeplay.com> |
| Original author | Ruyman Reyes <ruyman@codeplay.com> |

## Overview

Developers in some situations want to create memory that can only be accessed from the device. This can enable the underlying device driver to allocate memory on places where it is more efficient for the device to access than the host. 
This is possible in OpenCL 1.2 by using the CL_MEM_HOST_NO_ACCESS, CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_WRITE_ONLY flags during buffer creation.

A SYCL implementation for OpenCL needs to analyze the access pattern at compile time, or replace the underlying cl_mem object, in order to infer the host access pattern and create buffers with the appropriate flags.

This proposal enables developers to specify the intended use pattern of a buffer, facilitating a SYCL runtime the usage of optimized buffer flags creation.

## Revisions

This is the first revision of this proposal.

## Interface changes

This proposal adds a new property for buffers, *buffer::property::host_access*.
A SYCL buffer with the *host_access* property can only be used within one context,
and its values are only accessible from queues that are associated with
said context.

```cpp
namespace codeplay {

enum class host_access_mode {
  read,
  read_write,
  write,
  none
};

struct host_access {
  host_access(host_access_mode hostAccessMode);

  host_access_mode get_host_access_mode() const;
};

}  // namespace codeplay
```


## Example

The following example is a simplification of a reduction kernel 
that uses a temporary storage to avoid modifying the input buffer.

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
std::for_each(sycl_named_policy<example>(otherQueue),
      std::begin(myBuffer), std::end(myBuffer),
      [=](float &elem) { 
          elem = 1.0;
  });

/* This temporary memory only needs to exist 
 * on the context where the operation will be
 * performed.
 */
buffer<float, 1> tmp{myRange, 
                     property::buffer::context_bound(deviceContext)
                    | codeplay::property::buffer::host_access(none) };

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

/**
 * Value can be used directly on a different command group
 */
auto f = [&](handler &h) mutable {
    auto aIn = tmp.template get_access<access::mode::read>(h);
    auto aOut = myBuffer.template get_access<access::mode::read_write>(h);
    h.parallel_for(range<1>(local), [=](id<i> i) {
      aOut[i] = aIn[0] * aOut[i];
    })
}

// Data is now in buffer out, can be copied to the host as usual

```

## Implementation notes

An implementation may decide to not take advantage of this hint and 
simply construct a normal buffer object underneath.
However, if the buffer is accessed on the host, this must raise an error.

## References

* SYCL 1.2.1 specification
* OpenCL 1.2 specification
