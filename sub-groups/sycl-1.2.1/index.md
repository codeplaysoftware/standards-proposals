# Basic Sub group support

This proposal aims to define an interface for using OpenCL 2.2 sub groups in
SYCL the provisional SYCL 1.2.1 specification, relying on the underlying
OpenCL implementation supporting the extension `cl_codeplay_basic_sub_groups`.

The extension exposes to programmers the ability to identify sub-groups
on a work-group, count the number of sub-groups available.

Details of the execution and memory model changes can be found in the
documentation for the Codeplay's OpenCL vendor extension `cl_codeplay_basic_sub_groups` 
once available.

## Execution model

When this vendor extension is available, the execution model of SYCL 1.2.1
is extended to also include support for sub-groups of threads inside of a
work-group.
Overall, these sub-groups work following the description of the OpenCL 2.2
sub-groups, with some restrictions:

* The number of sub-groups available for each work-group is determined 
at compile-time and remains the same during the execution of the SYCL application.
* The number of threads per sub-group is known at compile-time, and remains the
same during execution of the SYCL application.
* Only those functions defined in this proposal are available. 
In particular, there is no sub-group pipe communication.

## Memory model

Sub-groups can access global and local memory, but, given there is no 
memory-scope to the atomic or barriers operations in SYCL 1.2.1, there is no
possibility to specify an equivalent of sub-group memory scope.

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

## Extensions to the nd\_item class

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

Below is trivial example showing how you would use `sub_group` to broadcast a value from one work-item within a sub-group to all other work-items in the sub-group.

```cpp
template <typename dimT>
void my_subgroup_load(sub_group<dimT> subG, global_ptr<float> myArray) {

  float4 f;
  if (subG.get_id() == 0) {
    f.load(myArray);
  }
  barrier(subG, access::fence_space::global_and_local);
  float4 res = broadcast(subG, 0, f);
}
```
