| Proposal ID      | CP025                                                                                                                                                  |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Name             | Properties for context, program, sampler, and accessor: Expanding use of property lists                                                                |
| Date of Creation | 23 January 2020                                                                                                                                        |
| Target           | SYCL 1.2.1 extension                                                                                                                                   |
| Current Status   | Draft                                                                                                                                     |
| Original author  | Steffen Larsen steffen.larsen@codeplay.com                                                                                                             |
| Contributors     | Steffen Larsen steffen.larsen@codeplay.com, Stuart Adams stuart.adams@codeplay.com, Gordon Brown gordon@codeplay.com, Ruyman Reyes ruyman@codeplay.com |

# Properties for context, program, sampler, and accessor: Expanding use of property lists

## Overview

With the move towards a [generalizing SYCL](https://github.com/KhronosGroup/SYCL-Shared/blob/master/proposals/sycl_generalization.md),
future backends may have use of parameters passed to their parent SYCL object.
However, with only `buffer`, `image`, and `queue` exposing constructors with a
`property_list` parameter, the extensibility of the parameter space of all other
user-constructible SYCL objects is limited.

This proposal extends the constructors for `context`, `program`, `sampler`, and
`accessor` with an additional `property_list` parameter.

## Specification changes

Section 4.3.4 of the 1.2.1 SYCL specifications (Rev 6) is changed to mention
`context`, `program`, `sampler`, and `accessor` as providing a `property_list`
parameter in their constructors. This is in addition to the existing mention of
`buffer`, `image`, and `queue`.

## Interface changes

The constructors for `context`, `program`, `sampler`, and `accessor`
are extended to have a `property_list` parameter, similar to `buffer`, `image`, and
`queue`.

### Context changes

```cpp
namespace cl {
namespace sycl {
class context {
public:
    context(const property_list &propList = {});

    explicit context(async_handler asyncHandler,
        const property_list &propList = {});

    context(const device &dev, const property_list &propList = {});

    context(const device &dev, async_handler asyncHandler,
        const property_list &propList = {});

    context(const platform &plt, const property_list &propList = {});

    context(const platform &plt, async_handler asyncHandler,
        const property_list &propList = {});

    context(const vector_class<device> &deviceList,
        const property_list &propList = {});

    context(const vector_class<device> &deviceList,
        async_handler asyncHandler, const property_list &propList = {});

    ...
};
} // namespace sycl
} // namespace cl
```

### Program changes

```cpp
namespace cl {
namespace sycl {
class program {
public:
    explicit program(const context &context,
        const property_list &propList = {});

    program(const context &context, vector_class<device> deviceList,
        const property_list &propList = {});

    program(vector_class<program> &programList, string_class linkOptions = "",
        const property_list &propList = {});

    ...
};
} // namespace sycl
} // namespace cl
```

### Sampler changes

```cpp
namespace cl {
namespace sycl {
class sampler {
public:
    sampler(coordinate_normalization_mode normalizationMode,
        addressing_mode addressingMode, filterin_mode filteringMode,
        const property_list &propList = {});

    ...
};
} // namespace sycl
} // namespace cl
```

### Accessor changes

The following changes apply to both placeholder and non-placeholder accessors.

#### Buffer accessor changes

```cpp
namespace cl {
namespace sycl {
template <typename dataT, int dimensions, access::mode accessmode,
          access::target accessTarget = access::target::global_buffer,
          access::placeholder isPlaceholder = access::placeholder::false_t>
class accessor {
public:
    ...

    /* Available only when: ((isPlaceholder == access::placeholder::false_t &&
    accessTarget == access::target::host_buffer) || (isPlaceholder ==
    access::placeholder::true_t && (accessTarget == access::target::global_buffer
    || accessTarget == access::target::constant_buffer))) && dimensions == 0 */
    template <typename AllocatorT>
    accessor(buffer<dataT, 1, AllocatorT> &bufferRef,
        const property_list &propList = {});

    /* Available only when: (isPlaceholder == access::placeholder::false_t &&
    (accessTarget == access::target::global_buffer || accessTarget ==
    access::target::constant_buffer)) && dimensions == 0 */
    template <typename AllocatorT>
    accessor(buffer<dataT, 1, AllocatorT> &bufferRef,
        handler &commandGroupHandlerRef, const property_list &propList = {});

    /* Available only when: ((isPlaceholder == access::placeholder::false_t &&
    accessTarget == access::target::host_buffer) || (isPlaceholder ==
    access::placeholder::true_t && (accessTarget == access::target::global_buffer
    || accessTarget == access::target::constant_buffer))) && dimensions > 0 */
    template <typename AllocatorT>
    accessor(buffer<dataT, dimensions, AllocatorT> &bufferRef,
        const property_list &propList = {});

    /* Available only when: (isPlaceholder == access::placeholder::false_t &&
    (accessTarget == access::target::global_buffer || accessTarget ==
    access::target::constant_buffer)) && dimensions > 0 */
    template <typename AllocatorT>
    accessor(buffer<dataT, dimensions, AllocatorT> &bufferRef,
        handler &commandGroupHandlerRef, const property_list &propList = {});

    /* Available only when: (isPlaceholder == access::placeholder::false_t &&
    accessTarget == access::target::host_buffer) || (isPlaceholder ==
    access::placeholder::true_t && (accessTarget == access::target::global_buffer
    || accessTarget == access::target::constant_buffer)) && dimensions > 0 */
    template <typename AllocatorT>
    accessor(buffer<dataT, dimensions, AllocatorT> &bufferRef,
        range<dimensions> accessRange, id<dimensions> accessOffset = {},
        const property_list &propList = {});

    /* Available only when: (isPlaceholder == access::placeholder::false_t &&
    (accessTarget == access::target::global_buffer || accessTarget ==
    access::target::constant_buffer)) && dimensions > 0 */
    template <typename AllocatorT>
    accessor(buffer<dataT, dimensions, AllocatorT> &bufferRef,
        handler &commandGroupHandlerRef, range<dimensions> accessRange,
        id<dimensions> accessOffset = {}, const property_list &propList = {});

    ...
};
} // namespace sycl
} // namespace cl
```

#### Image accessor changes

```cpp
namespace cl {
namespace sycl {
template <typename dataT, int dimensions, access::mode accessmode,
          access::target accessTarget = access::target::global_buffer,
          access::placeholder isPlaceholder = access::placeholder::false_t>
class accessor {
public:
    ...

    /* Available only when: accessTarget == access::target::host_image */
    template <typename AllocatorT>
    accessor(image<dimensions, AllocatorT> &imageRef,
        const property_list &propList = {});

    /* Available only when: accessTarget == access::target::image */
    template <typename AllocatorT>
    accessor(image<dimensions, AllocatorT> &imageRef,
      handler &commandGroupHandlerRef, const property_list &propList = {});

    /* Available only when: accessTarget == access::target::image_array &&
    dimensions < 3 */
    template <typename AllocatorT>
    accessor(image<dimensions + 1, AllocatorT> &imageRef,
      handler &commandGroupHandlerRef, const property_list &propList = {});

    ...
};
} // namespace sycl
} // namespace cl
```

#### Local accessor changes

```cpp
namespace cl {
namespace sycl {
template <typename dataT, int dimensions, access::mode accessmode,
          access::target accessTarget = access::target::global_buffer,
          access::placeholder isPlaceholder = access::placeholder::false_t>
class accessor {
public:
    ...

    /* Available only when: dimensions == 0 */
    accessor(handler &commandGroupHandlerRef,
        const property_list &propList = {});

    /* Available only when: dimensions > 0 */
    accessor(range<dimensions> allocationSize, handler &commandGroupHandlerRef,
        const property_list &propList = {});

    ...
};
} // namespace sycl
} // namespace cl
```
