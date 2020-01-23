| Proposal ID      | CP024                                                                                                              |
| ---------------- | ------------------------------------------------------------------------------------------------------------------ |
| Name             | Dimensionality-dependent maximum work-item sizes                                                                   |
| Date of Creation | 23 January 2020                                                                                                    |
| Target           | SYCL 1.2.1 extension                                                                                               |
| Current Status   | Draft                                                                                                              |
| Original author  | Steffen Larsen steffen.larsen@codeplay.com                                                                         |
| Contributors     | Steffen Larsen steffen.larsen@codeplay.com, Gordon Brown gordon@codeplay.com, Ruyman Reyes ruyman@codeplay.com     |

# Dimensionality-dependent maximum work-item sizes

## Overview

Consider a device that when queried for `CL_DEVICE_MAX_WORK_ITEM_SIZES` using
`clGetDeviceInfo` returns `{1024, 1024, 64}`. With the changes imposed by
[this clarification](https://github.com/KhronosGroup/SYCL-Docs/pull/56) the
result of querying `get_info` with `max_work_item_sizes` is `{64, 1024, 1024}`.
Let `q` be a SYCL queue for this device. For `q` the code

```cpp
range<3> wg_size{128,1,1};
q.submit([&](handler &cgh) {
  cgh.parallel_for<class wg_size_c>(
      nd_range<3>(wg_size, wg_size),
      [=](nd_item<3> it) {
        ...
      });
});
```

is illegal as the work group size exceeds the max work-item size in the first
dimension. However, if the dimensionality of the work-group is decreased to 2
dimensions, we have

```cpp
range<2> wg_size{128,1};
q.submit([&](handler &cgh) {
  cgh.parallel_for<class wg_size_c>(
      nd_range<2>(wg_size, wg_size),
      [=](nd_item<2> it) {
        ...
      });
});
```

which works as intended. This behaviour is caused by the mapping illustrated in
Table 4.89 of the SYCL 1.2.1 specification (Rev 6) as the dimensionality has an
effect on how the indices of the ranges are mapped to the dimensions, i.e.
the work-group size that OpenCL gets are `{1,1,128}` and `{1,128,1}` in the two
examples, respectively.
Let `maxWGSize` be the result of querying `get_info` with `max_work_item_sizes`.
Consider a range `A` used to define the work-group size. The following table
shows how the different number of dimensions for `A` affects the local work size
used for OpenCL and its effect on the relation to `maxWGSize`:

| `A`         | OpenCL local work size | Constraints                                                                      |
| ----------- | ---------------------- | -------------------------------------------------------------------------------- |
| `{x, y, z}` | `{z, y, x}`            | `A[0] <= maxWGSize[0] &&`<br>`A[1] <= maxWGSize[1] &&`<br>`A[2] <= maxWGSize[2]` |
| `{x, y}`    | `{y, x, 1}`            | `A[0] <= maxWGSize[1] &&`<br>`A[1] <= maxWGSize[2]`                              |
| `{x}`       | `{x, 1, 1}`            | `A[0] <= maxWGSize[2]`                                                           |


This proposal introduces a number of enumerators to `cl::sycl::info::device`
intended to replace the `max_work_item_sizes` enumerator. The result of querying
`get_info` with any of these enumerators is an `id<D>` where each value
represents the maximum width allowed in the corresponding dimension of a
`range<D>` given to a `parallel_for`, where `D` is a number of dimensions
specified by the enumerator.

## Interface changes

Three device information descriptors are added:

| Descriptor                             | Return type | Description                                                                                                                                                                                                                      |
| -------------------------------------- | ----------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `info::device::max_work_item_sizes_1d` | `id<1>`     | Returns the maximum number of work-items that are permitted in a work-group of a 1-dimensional `nd_range`. The minimum value is 1 for `device`s that are not of device type `info::device_type::custom`.                         |
| `info::device::max_work_item_sizes_2d` | `id<2>`     | Returns the maximum number of work-items that are permitted in each dimension of a work-group of a 2-dimensional `nd_range`. The minimum value is (1,1) for `device`s that are not of device type `info::device_type::custom`.   |
| `info::device::max_work_item_sizes_3d` | `id<3>`     | Returns the maximum number of work-items that are permitted in each dimension of a work-group of a 3-dimensional `nd_range`. The minimum value is (1,1,1) for `device`s that are not of device type `info::device_type::custom`. |

The `max_work_item_sizes` descriptor is deprecated and the new descriptors are
added as enumerators to `cl::sycl::info::device`:

```cpp
namespace cl {
namespace sycl {
namespace info {

enum class device : int {
  ...
  max_work_item_sizes,    // Deprecated
  max_work_item_sizes_1d,
  max_work_item_sizes_2d,
  max_work_item_sizes_3d,
  ...
}

} // namespace info
} // namespace sycl
} // namespace cl
```
