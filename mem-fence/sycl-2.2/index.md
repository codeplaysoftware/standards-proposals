**Document number: C011**
**Date: 2017-07-28**
**Project: SYCL 2.2 Specification**
**Authors: Gordon Brown, Ruyman Reyes, Victor Lomuller**
**Emails: gordon@codeplay.com, ruymna@codeplay.com, victor@codeplay.com**
**Reply to: gordon@codeplay.com**

# Mem Fence Builtins

## Motivation

The OpenCL specification provides builtins for performing a memory fence operation only, without having to use a barrier operation.

These builtins are currently missing from the SYCL specification.

## Summary

This proposal will add the `mem_fence` member function to the `nd_item` and `group` classes.

## Mem Fence Builtin Functions

```cpp
template <int dimensions>
class nd_item {
...
tempalte <access::mode accessMode = access::mode::read_write>
void mem_fence(access::fence_space addressSpace =
  access::fence_space::global_and_local) const;
...
};

template <int dimensions>
class group {
...
tempalte <access::mode accessMode = access::mode::read_write>
void mem_fence(access::fence_space addressSpace =
  access::fence_space::global_and_local) const;
...
};
```

The member function `mem_fence` called with the access mode `access::mode::read_write` will guarantee that all loads and stores before the memory fence are complete before performing after loads or stores after the memory fence. The default fence space if one is not specified is `access::fence_space::global_and_local`. This is the default access mode if noe is provided.

The member function `mem_fence` called with the access mode `access::mode::read` will guarantee that all loads before the memory fence are complete before performing after loads after the memory fence. The default fence space if one is not specified is `access::fence_space::global_and_local`.

The member function `mem_fence` called with the access mode `access::mode::write` will guarantee that all stores before the memory fence are complete before performing after stores after the memory fence. The default fence space if one is not specified is `access::fence_space::global_and_local`.