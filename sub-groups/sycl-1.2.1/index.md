# Basic Sub group support

This proposal aims to define an interface for using OpenCL 2.x sub groups in
SYCL the provisional SYCL 1.2.1 specification, relying on the underlying
OpenCL implementation supporting the extension `cl_codeplay_basic_sub_groups`.

The extension exposes to programmers the ability to identify sub-groups
on a work-group, count the number of sub-groups available.

## Namespace `basic_sub_group`

All new functionality is exposed under the `basic_sub_group` namespace
in the `codeplay` vendor extension namespace.
When the vendor extension `basic_sub_group` is available, the macro
`SYCL_CODEPLAY_BASIC_SUB_GROUP` is defined in the header.

### Class `sub_group`

The extension adds a new class template `sub_group` that identifies the 
sub group range and the current sub group id. 
It also for providing sub group barriers.

```cpp
namespace cl {
namespace sycl {
namespace codeplay {

template <int Dimensions>
class sub_group {
 public:

  range<Dimensions> nd_item<Dimensions>::get_sub_group_range() const;

  size_t nd_item<Dimensions>::get_sub_group_range(int dimension) const;

  size_t nd_item<Dimensions>::get_sub_group_linear_range() const;

  id<Dimensions> nd_item<Dimensions>::get_sub_group_id() const;

  size_t nd_item<Dimensions>::get_sub_group_id(int dimension) const;

  size_t nd_item<Dimensions>::get_sub_group_linear_id() const;

  void nd_item<Dimensions>::barrier(access::fence_space accessSpace
    = access::fence_space::global_and_local) const;

  /* T is permitted to be int, unsigned int, long, unsigned long, 
    float, half, double */
  template <typename T>
  T broadcast(size_t subGroupId, T value);

  /* Predicate must be a callable type which returns bool */
  template <typename Predicate>
  bool all_of(Predicate predicate) const;

  /* Predicate must be a callable type which returns bool */
  template <typename Predicate>
  bool any_of(Predicate predicate) const;
};

}  // namespace codeplay
}  // namespace sycl
}  // namespace cl
```

## Free functions

```cpp
namespace cl {
namespace sycl {
namespace codeplay {

template <int Dimensions, T>
T broadcast(sub_group<Dimensions> subGroup, size_t subGroupId, T value);

template <int Dimensions, typename Predicate>
bool all_of(sub_group<Dimensions> subGroup, Predicate predicate);

template <int Dimensions, typename Predicate>
bool any_of(sub_group<Dimensions> subGroup, Predicate predicate);

template <int Dimensions>
void barrier(sub_group<Dimensions> subGroup, access::fence_space accessSpace
  = access::fence_space::global_and_local) const;

}  // namespace codeplay
}  // namespace sycl
}  // namespace cl
```

## Extensions to the nd_item class

Extensions to the `nd_item` interface will be exposed via the a derived `nd_item` class template in the `codeplay` vendor extension namespace.

New member function `get_sub_group` for identifying the current sub group and gaining access to sub group operations.

```cpp
namespace cl {
namespace sycl {
namespace codeplay {

template <int Dimensions>
class nd_item : public ::cl::sycl::nd_item<Dimensions> {
public:

  group<Dimensions> nd_item<Dimensions>::get_sub_group() const;

};

}  // namespace codeplay
}  // namespace sycl
}  // namespace cl
```

## Example

TODO: Add example
