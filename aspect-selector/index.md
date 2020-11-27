# Aspect selector

| Proposal ID | CP031  |
|-------------|--------|
| Name | Aspect selector |
| Date of Creation | 27 November 2020 |
| Revision | 0.1 |
| Latest Update | 27 November 2020 |
| Target | SYCL 2020 |
| Current Status | _Work in Progress_ |
| Reply-to | Peter Žužek <peter@codeplay.com> |
| Original author | Peter Žužek <peter@codeplay.com> |
| Contributors |  |

## Overview

This proposal defines a free function `sycl::aspect_selector`
that can be used to select a device based on desired device aspects.

## Revisions

### 0.1

* Initial proposal

## Motivation

The SYCL 2020 provisional specification provides a few device selectors
that perform device selection based on specific traits.
For example, the `cpu_selector` will select a CPU device,
or in other words, a device with the `aspect::cpu` device aspect.

However, the spec only defines a handful of device selectors.
There are two main reasons:

1. Historic. SYCL 1.2.1 spec defined selectors based on only device type,
   device aspects were introduced in the 2020 provisional.
   There is a small selection of device types compared to device aspects,
   and device types are for the most part (but not always) orthogonal to each other.
2. Combinatorial explosion. It wouldn't be feasible to define selectors
   for all device aspects and all combinations of aspects.

To give SYCL users more choice we propose adding a utility function `sycl::aspect_selector`,
which creates a callable object based on the aspects provided.
This utility should eliminate the need for a custom device selector in many cases.

## Changes

Add the following functions to the device selection section:

```cpp
namespace sycl {

constexpr __unspecified_callable__
  aspect_selector(const std::vector<aspect>& aspectList); // 1

// Each member of aspectListTN must be of type aspect
template <class... aspectListTN>
constexpr __unspecified_callable__
  aspect_selector(aspectListTN... aspectList); // 2

template <aspect... aspectListN>
constexpr __unspecified_callable__
  aspect_selector(); // 3

} // namespace sycl
```

Function 1 takes a vector of aspects and returns a callable object
that represents the actual device selector for the desired aspects.
The returned selector will only match devices
where `dev.has(devAspect) == true` for each `devAspect` from `aspectList`. 

Function 2 takes the list of aspects as function arguments,
without the need for a vector.

Function 3 takes aspects as compile time constants instead,
but performs essentially the same functionality as function 1.
A possible implementation of function 3 would be
to simply redirect the call to function 1.
However, the compile-time list also enables compile-time inspection,
leading to potentially better error messages or optimizations.

## Examples

```cpp
using namespace sycl;

// Unrestrained selection
auto dev0 = device{aspect_selector()};

// Pass aspects in a vector
// Only accept CPUs that support half
auto dev1 = device{aspect_selector(std::vector{aspect::cpu, aspect::fp16})};

// Pass aspects without a vector
// Only accept GPUs that support half
auto dev2 = device{aspect_selector(aspect::gpu, aspect::fp16)};

// Pass aspects as compile-time parameters
// Only accept devices that can be debugged on host and support half
auto dev3 = device{aspect_selector<aspect::host_debuggable, aspect::fp16>()};
```
