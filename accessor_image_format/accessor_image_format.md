| Proposal ID      | CP029                                                                                                                                                         |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Name             | Image accessors with format information                                                                                                                       |
| Date of Creation | 2 July 2020                                                                                                                                                   |
| Target           | Vendor extension                                                                                                                                              |
| Current Status   | *Under Review*                                                                                                                                                |
| Original authors | Steffen Larsen steffen.larsen@codeplay.com, Alexander Johnston alexander@codeplay.com                                                                         |
| Contributors     | Steffen Larsen steffen.larsen@codeplay.com, Alexander Johnston alexander@codeplay.com, Victor Lomüller victor@codeplay.com, Ruyman Reyes ruyman@codeplay.com  |

# Image accessors with format information

## Motivation

In SYCL 1.2.1 operations on images are closely tied to the corresponding
operations in OpenCL thus only allowing read and write operations to be done
using 4-element vectors of 32-bit floating points, 32-bit signed integers,
32-bit unsigned integers, or 16-bit floating points, which are in turn converted
to the types of the channels in the image. Because of this, backends that cannot
query the types of the channels in the image cannot determine the necessary
conversions to do.

## Overview

This extension adds `sycl::short4`, `sycl::ushort4`, `sycl::schar4`, and
`sycl::uchar4` to the list of types supported for `dataT` for image accessors.
In order to carry information about the format of the image, the
`image_format` enumerator from SYCL 2020 is adopted, given as

```c++
namespace sycl {
enum class image_format : unsigned int {
    r8g8b8a8_unorm,
    r16g16b16a16_unorm,
    r8g8b8a8_sint,
    r16g16b16a16_sint,
    r32b32g32a32_sint,
    r8g8b8a8_uint,
    r16g16b16a16_uint,
    r32b32g32a32_uint,
    r16b16g16a16_sfloat,
    r32g32b32a32_sfloat,
};
} // namespace sycl
```

and mappings from `image_format` to the corresponding accessor data type is
defined as

```c++

namespace sycl {
template<image_format> struct image_access;

template<> struct image_access<image_format::r8g8b8a8_snorm> {
    using type = float4;
};
template<> struct image_access<image_format::r16g16b16a16_snorm> {
    using type = float4;
};
template<> struct image_access<image_format::r8g8b8a8_unorm> {
    using type = float4;
};
template<> struct image_access<image_format::r16g16b16a16_unorm> {
    using type = float4;
};
template<> struct image_access<image_format::r8g8b8a8_sint> {
    using type = schar4;
};
template<> struct image_access<image_format::r16g16b16a16_sint> {
    using type = short4;
};
template<> struct image_access<image_format::r32b32g32a32_sint> {
    using type = int4;
};
template<> struct image_access<image_format::r8g8b8a8_uint> {
    using type = uchar4;
};
template<> struct image_access<image_format::r16g16b16a16_uint> {
    using type = ushort4;
};
template<> struct image_access<image_format::r32b32g32a32_uint> {
    using type = uint4;
};
template<> struct image_access<image_format::r16b16g16a16_sfloat> {
    using type = half4;
};
template<> struct image_access<image_format::r32g32b32a32_sfloat> {
    using type = float4;
};

template<image_format format>
using image_access_t = typename image_access<format>::type;
} // namespace sycl
```

With these, a new overload of `get_access` is added to `image` defined as

```c++
template <image_format format, access::mode accessMode>
accessor<image_access_t<format>, dimensions, accessMode, access::target::image>
get_access(handler & commandGroupHandler);
```

Calling `read` or `write` on an image accessor retrieved through the new
`get_access` above causes undefined behaviour unless the `image_channel_type` of
the underlying image and the `image_format` given to `get_access` is one of the
combinations shown in the following table:

| `image_channel_type` |     `image_format`    |
|----------------------|-----------------------|
| `snorm_int8`         | `r8g8b8a8_snorm`      |
| `snorm_int16`        | `r16g16b16a16_snorm`  |
| `unorm_int8`         | `r8g8b8a8_unorm`      |
| `unorm_int16`        | `r16g16b16a16_unorm`  |
| `signed_int8`        | `r8g8b8a8_sint`       |
| `signed_int16`       | `r16g16b16a16_sint`   |
| `signed_int32`       | `r32b32g32a32_sint`   |
| `unsigned_int8`      | `r8g8b8a8_uint`       |
| `unsigned_int16`     | `r16g16b16a16_uint`   |
| `unsigned_int32`     | `r32b32g32a32_uint`   |
| `fp16`               | `r16b16g16a16_sfloat` |
| `fp32`               | `r32g32b32a32_sfloat` |

## Example

```c++
cl::sycl::image<2> image(hostPtr, cl::sycl::image_channel_order::rgba,
                         cl::sycl::image_channel_type::unsigned_int8,
                         cl::sycl::range<2>{4, 4});

cl::sycl::buffer<cl::sycl::uchar4, 1> resultDataBuf(&resultData,
                                                    cl::sycl::range<1>(1));

cl::sycl::queue myQueue(selector);
myQueue.submit([&](cl::sycl::handler &cgh) {
  auto imageAcc =
      image.get_access<image_format::r8g8b8a8_uint, cl::sycl::access::mode::read>(cgh);
  cl::sycl::accessor<cl::sycl::uchar4, 1, cl::sycl::access::mode::write>
      resultDataAcc(resultDataBuf, cgh);

  cgh.single_task<unsampled_kernel_class>([=]() {
    cl::sycl::uchar4 RetColor = imageAcc.read(coord);
    resultDataAcc[0] = RetColor;
  });
});
```

## OpenCL implementation Note

Given that image read and write operations in OpenCL only support `cl_int4`,
`cl_uint4`, `cl_float4`, and `cl_half4`, which have direct relation to the
previously supported data types for image accessors, namely `sycl::int4`,
`sycl::uint4`, `sycl::float4`, and `sycl::half4` respectively. By adding support
for `sycl::short4`, `sycl::ushort4`, `sycl::schar4`, and `sycl::uchar4` an
OpenCL backend will require conversions to the types corresponding to those
supported by OpenCL after a read and before a write. That is `sycl::short4` and
`sycl::schar4` must be converted to and from `sycl::int4`, whereas
`sycl::ushort4` and `sycl::uchar4` must be converted to and from `sycl::uint4`.

## SYCL 2020 Note

Though not included in this proposal, we believe a change in this direction or a
similar one should be made to the SYCL 2020 provisional too. In SYCL 2020 the
data type of an image accessor is also currently restricted to `sycl::int4`,
`sycl::uint4`, `sycl::float4`, and `sycl::half4`. This should be updated to be
deduced from the `image_format` of the image the image accessor is produced
from. This will enable each backend to perform appropriate hardware supported
operations for image reads and writes, and use the appropriate data type for
them. The OpenCL backend will still be able to use the four types listed
above, while the CUDA backend, and others where appropriate, will be able to
take and data types suitable for their hardware operations.
