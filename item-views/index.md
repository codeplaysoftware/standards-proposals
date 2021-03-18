| Proposal ID      | CP030                                        |
|------------------|----------------------------------------------|
| Name             | Item Views                                   |
| Date of Creation | 23 June 2021                                 |
| Target           | Vendor extension                             |
| Current Status   | *Under Review*                               |
| Original author  | Steffen Larsen <steffen.larsen@codeplay.com> |
| Contributors     | Steffen Larsen <steffen.larsen@codeplay.com> |

# Item Views

A common pattern in SYCL kernels is to only access the element at the work-item
offset, i.e. in SYCL 2020 vector addition can be written as

```c++
template <typename T, size_t N>
void simple_vadd(const std::array<T, N>& VA, const std::array<T, N>& VB,
                 std::array<T, N>& VC) {
  sycl::queue deviceQueue;
  sycl::buffer bufferA{VA};
  sycl::buffer bufferB{VB};
  sycl::buffer bufferC{VC};
 
  deviceQueue.submit([&](sycl::handler& cgh) {
    sycl::accessor accessorA{bufferA, cgh, sycl::read_only};
    sycl::accessor accessorB{bufferB, cgh, sycl::read_only};
    sycl::accessor accessorC{bufferC, cgh, sycl::write_only};
 
    cgh.parallel_for(N, [=](sycl::id<1> wiID) {
      accessorC[wiID] = accessorA[wiID] + accessorB[wiID];
    });
  });
}
```

where each work-item accesses exactly one element corresponding to its
identifier. Access patterns like these have a multitude of interesting
properties, but can potentially be hard to detect.

To alleviate this, we introduce the `item_view` class. This class forces this
access pattern onto kernels by supplying only the element corresponding to
the work-item identifier, disallowing any access outside it in the underlying
buffer. As an example, vector addition using `item_view` could be written as

```c++
template <typename T, size_t N>
void simple_vadd(const std::array<T, N>& VA, const std::array<T, N>& VB,
                 std::array<T, N>& VC) {
  sycl::queue deviceQueue;
  sycl::buffer bufferA{VA};
  sycl::buffer bufferB{VB};
  sycl::buffer bufferC{VC};
 
  deviceQueue.submit([&](sycl::handler& cgh) {
    sycl::item_view itemViewA{bufferA, cgh, sycl::read_only};
    sycl::item_view itemViewB{bufferB, cgh, sycl::read_only};
    sycl::item_view itemViewC{bufferC, cgh, sycl::write_only};
 
    cgh.parallel_for(N, itemViewA, itemViewB, itemViewC,
      [=](sycl::id<1> wiID, const T& a, const T& b, T& c) {
        c = a + b;
      });
  });
}
```

Notice that the use of item-views is similar to reductions in SYCL 2020 and
can be used in tandem, e.g. vector dot product could be written as

```c++
template <typename T, size_t N>
T simple_vdot(const std::array<T, N>& VA, const std::array<T, N>& VB) {
  sycl::queue deviceQueue;
  sycl::buffer bufferA{VA};
  sycl::buffer bufferB{VB};
  T dotResult;
 
  deviceQueue.submit([&](sycl::handler& cgh) {
    sycl::item_view<T, 1, sycl::read_only> itemViewA{bufferA, cgh};
    sycl::item_view<T, 1, sycl::read_only> itemViewB{bufferB, cgh}
 
    auto dotReduction = reduction(&dotResult, plus<>());
 
    cgh.parallel_for(sycl::range(N), itemViewA, itemViewB, dotReduction,
      [=](sycl::id<1> wiID, const T &a, const T &b, auto &dotAcc) {
        dotAcc += a * b;
      });
  });
 
  return dotResult;
}
```

## Interface

The following describes the interface of the `item_view` class, additions
required to the descriptions of `parallel_for` allowing `item_view` objects as
parameters, and the behavior of read and write between the `item_view`
parameters of a `parallel_for` and the corresponding kernel parameter.

### item_view class

This extensions add the `item_view` class, which is defined as:

```c++
namespace sycl {
namespace ext {
namespace codeplay {

// Requires Dimensions > 0
template <typename DataT,
          int Dimensions = 1,
          access_mode AccessMode =
            (std::is_const_v<DataT> ? access_mode::read
                                    : access_mode::read_write)>
class item_view {
public:
  using value_type = DataT;
  using reference = std::conditional_t<AccessMode == access_mode::read,
                                       const DataT&, DataT&>;
  using const_reference = const DataT &;
  static constexpr int dimensions = Dimensions;

  template<typename AllocatorT>
  item_view(buffer<DataT, Dimensions, AllocatorT>&bufferRef,
            handler &commandGroupHandlerRef,
            const property_list &propList = {});

  /* -- common interface members -- */

  range<Dimensions> get_range() const;

  size_t byte_size() const noexcept;

  size_t size() const noexcept;
};

} // codeplay
} // ext
} // sycl
```

| Constructor | Description |
|-------------|-------------|
| `template<typename AllocatorT> item_view(buffer<DataT, Dimensions, AllocatorT>&bufferRef, handler &commandGroupHandlerRef, const property_list &propList = {})` | Constructs a `item_view` instance with the `bufferRef` provided. This creates a data dependency in `commandGroupHandlerRef` for access to `bufferRef` using access mode `AccessMode`. The optional `property_list` provides properties for the constructed `item_view`. |

| Member function | Description |
|-----------------|-------------|
| `range<Dimensions> get_range() const` | Returns a `range` object representing the size of the `item_view` in terms of number of elements in each dimension. |
| `size_t size() const noexcept` | Returns the total number of ele­ments in the `item_view`. Equal to `get_range()[0] * ... * get_range()[dimensions-1]`. |
| `size_t byte_size() const noexcept` | Returns the size of the `item_view` stor­age in bytes. Equal to `size()*size­of(DataT)`. |

The `item_view` class follows the common reference semantics specified in
section 4.5.2 of the SYCL 2020 specification (revision 3).

### parallel_for allowing item_view parameters

The description of the `handler` class member functions

```c++
template<typename KernelName, int dimensions, typename... Rest>
void parallel_for(range<dimensions> numWorkItems, Rest&&... rest);

template<typename KernelName, int dimensions, typename... Rest>
void parallel_for(nd_range<dimensions> executionRange, Rest&&... rest);
```

have part of their description changed from

> The `rest` parameter pack consists of 0 or more objects created by the
> `reduction` function, followed by a callable. For each object in `rest`, the
> kernel function must take an additional reference parameter corresponding to
> that object’s reducer type, in the same order.

to

> The `rest` parameter pack consists of 0 or more occurances of either
> `item_view` objects or objects created by the `reduction` function, followed
> by a callable. For each object in `rest`, the kernel function must take an
> additional reference with the following type:
> * A reference corresponding to that object’s `reducer` type.
> * `const dataT &` if the corresponding object is a `item_view` with access
> mode `access_mode::read`, where `dataT` is the data type template
> parameter of the `item_view` object.
> * `dataT &` if the corresponding object is a `item_view` with access mode
> other than `access_mode::read`, where `dataT` is the data type template
> parameter of the `item_view` object.
> 
> These parameters must occur in the same order as the corresponding object
> occur in `rest`.

Additionally, the following is appended to these descriptions:

> Throws an `exception` with `errc::invalid` if for any `item_view` object `d`
> in the `rest` parameter pack, `d.dimensions != dimensions` evaluates to
> `true`.
> Throws an `exception` with `errc::invalid` if for any `item_view` object `d`
> in the `rest` parameter pack, for any `i` where `i < d.dimensions`,
> `d.get_range()[0] < {numWorkItems|(executionRange.get_offset() + executionRange.get_global_range())}[0]`
> evaluates to `true`.

### Data movement

All `item_view` objects passed to a `parallel_for` call have a corresponding
parameter in the kernel function. For an `item_view` object `d` passed to a
`parallel_for`, the added parameter `p` in the kernel function has the following
data movement behavior:

* If `AccessMode` of `d` is `access_mode::read` or `access_mode::read_write`,
  the value of `p` at the entry of the kernel function will be the same as the
  element in the underlying data of `d` at index corresponding to the `id` of
  the work-item.
* If `AccessMode` of `d` is `access_mode::write` or `access_mode::read_write`,
  the value of `p` will be written to element in the underlying data of `d` at
  index corresponding to the `id` of the work-item after the kernel returns.

### Extension macro

A SYCL implementation that implements this extension should define the macro
`SYCL_EXT_CODEPLAY_ITEM_VIEW`.
