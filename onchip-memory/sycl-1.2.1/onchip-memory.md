# On-chip-Memory Allocation

| Proposal ID | CP019 |
|-------------|--------|
| Name | On-chip-Memory Allocation |
| Date of Creation | 03 December 2018 |
| Target | SYCL 1.2.1 vendor extension |
| Current Status | _Work In Progress_ |
| Implemented in | _N/A_ |
| Reply-to | Gordon Brown <gordon@codeplay.com> |
| Original author | Gordon Brown <gordon@codeplay.com> |
| Contributors | Gordon Brown <gordon@codeplay.com>, Ruyman Reyes <ruyman@codeplay.com> |

## Overview

Various OpenCL platforms for embedded system provides an additional dedicated on-chip memory region that is not available in the standard OpenCL memory model. This memory region is accessible in the global address space and adheres to the OpenCL memory model requirements for global memory, however is lower latency to access. This proposal is to introduce an extension to SYCL that will allow buffers to be allocated in this on-chip memory region.

# Requirements

In designing an interface for this feature we must adhere to the following requirement:

* We must provide a way for the SYCL interface to instruct the SYCL runtime that a buffer object should allocate memory in an on-chip memory region.
* We must allow buffers which are allocated in a on-chip memory region to also be accessed on the host.
* We must allow for the SYCL runtime to preemptively allocate buffers using a on-chip memory region in the buffer constructor.
* We must allow this extension to fall-back if on-chip memory is not supported.

# Proposed Design

```cpp
buffer<float, 1> onchipBuffer(range, {property::buffer::context_bound(queue.get_context()), codeplay::property::buffer::use_onchip_memory(cl::sycl::codeplay::property::required)});
 
queue.submit([&](handler &cgh) {
  auto onchipAcc = onchipBuffer.get_access<access::mode::read_write>(cgh);
 
  cgh.parallel_for(range, [=](id<1> id) {
    do_something(onchipAcc);
  });
});
```

## `use_onchip_memory` property

The `use_onchip_memory` is available to `buffer`s and can be used to instruct the SYCL runtime that it should use on-chip memory to allocate a buffer.

This approach allows SYCL buffers to support on-chip memory to be used with minimal changes to user code. The philosophy of this approach is that on-chip is an extension of the global address space and can be accessed as global memory can, so it should accessed as though it were a global buffer, but the underlying memory allocation is in a different location with different access latency.

A SYCL buffer that is using on-chip memory will have the following restriction:
* The buffer may not perform map/unmap operations.
* The buffer may not support the creation of sub buffers.

## `require` and `prefer` tags

In this proposal we also introduce the `require` and `prefer` property tags, one of which is passed to the construction of a property. These tags instruct the SYCL runtime the priority of the property, where `require` requires the SYCL runtime to throw an exception if the property cannot be support and `prefer` allows the SYCL runtime to fall-back ini the case that the property is not supported.

In the case of fall-back the SYCL runtime will behave **as-if** the property were not specified.

Currently this proposal only suggests using the `require` and `prefer` tags for the `use_onchip_memory` property, however the intention is that these tags can be applied to other SYCL properties.

## Relationship with `context_bound` property

The `context_bound` property is not required or inferred by using the `use_onchip_memory` property, however users may want to use the two properties in tandem to ensure that memory is only allocated on the context which supports on-chip memory and allow the constructor to preemptively allocate on-chip memory in the buffer constructor.

## References

* SYCL 1.2.1 specification
* OpenCL 1.2 specification
