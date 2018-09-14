# Sub Groups

This proposal aims to define an interface for using OpenCL 2.x sub groups in
SYCL he provisional SYCL 2.2 specification (revision date 2016/02/15) already
contains SVM, but this proposal aims to make SVM in SYCL 2.2 more generic,
easier to program, better defined, and not necessarily tied to OpenCL 2.2.

## sub_group class

New class template `sub_group` for identifying the sub group range and the current sub group id and also for providing sub group barriers.

```cpp
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

  template <typename Predicate>
  bool all_of(Predicate predicate) const;

  template <typename Predicate>
  bool any_of(Predicate predicate) const;
};
```

## Free functions

```cpp
template <int Dimensions, typename Predicate>
bool all_of(group<Dimensions> group, Predicate predicate);

template <int Dimensions, typename Predicate>
bool all_of(sub_group<Dimensions> subGroup, Predicate predicate);

template <int Dimensions, typename Predicate>
bool any_of(group<Dimensions> group, Predicate predicate);

template <int Dimensions, typename Predicate>
bool any_of(sub_group<Dimensions> subGroup, Predicate predicate);

template <int Dimensions>
void barrier(group<Dimensions> group, access::fence_space accessSpace
  = access::fence_space::global_and_local) const;

template <int Dimensions>
void barrier(sub_group<Dimensions> subGroup, access::fence_space accessSpace
  = access::fence_space::global_and_local) const;
```

## Extensions to the nd_item class

New member function `get_sub_group` for identifying the current sub group and gaining access to sub group operations.

```cpp
...

group<Dimensions> nd_item<Dimensions>::get_sub_group() const;

...
```

## Example

TODO: Add example