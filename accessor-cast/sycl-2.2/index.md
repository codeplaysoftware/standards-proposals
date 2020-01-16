# Casting accessors

## Motivation

The current SYCL 1.2.1 specification allows casting buffers
using the `reinterpret` member function.
This essentially provides a different view of the same data,
however it cannot be performed inside a kernel.

One option would be to cast the `multi_ptr` obtained from an accessor.
However, that is a separate proposal,
plus it would add another layer of first obtaining the pointer
and then using that pointer instead of the accessor.

## Summary

This proposal adds an implicit conversion operator
to a const-qualified accessor type
as a member function of the `accessor` class,
and also adds several free functions to the `cl::sycl` namespace
that follow the naming and semantics of the `std::shared_ptr` pointer cast functions
defined by the C++17 standard: https://en.cppreference.com/w/cpp/memory/shared_ptr/pointer_cast.
In order to help with the `const` conversions
we also propose adding a few type traits.

## Const casting type traits

In order to help with casting the const qualifier,
we propose adding a few type traits:
1. `add_const_access` -
resolves any access mode to `access::mode::read`,
except for `access::mode::atomic`,
which resolves to the same mode.
2. `remove_const_access` -
resolves `access::mode::read` to `access::mode::read_write`,
and keeps all other modes the same.
3. `access_mode_from_cast` -
takes an access mode and two types
and then decides, based on the types,
whether to use `add_const_access`, `remove_const_access`, or none,
in order to resolve the access mode.

```cpp
namespace cl {
namespace sycl {

/// access_mode_constant
template <access::mode mode>
using access_mode_constant =
  std::integral_constant<access::mode, mode>;

/// add_const_access
template <access::mode mode>
struct add_const_access
    : access_mode_constant<access::mode::read> {};
template <>
struct add_const_access<access::mode::atomic>
    : access_mode_constant<access::mode::atomic> {};

/// remove_const_access
template <access::mode mode>
struct remove_const_access
    : access_mode_constant<mode> {};
template <>
struct remove_const_access<access::mode::read>
    : access_mode_constant<access::mode::read_write> {};

/// access_mode_from_cast
template <access::mode mode, typename oldT, typename newT>
struct access_mode_from_cast
    : access_mode_constant<mode> {};
template <access::mode mode, typename oldT, typename newT>
struct access_mode_from_cast<mode, const oldT, const newT>
    : access_mode_constant<mode> {};
template <access::mode mode, typename oldT, typename newT>
struct access_mode_from_cast<mode, const oldT, newT>
    : remove_const_access<mode> {};
template <access::mode mode, typename oldT, typename newT>
struct access_mode_from_cast<mode, oldT, const newT>
    : add_const_access<mode> {};

} // namespace sycl
} // namespace cl
```

| Type trait      | Description |
|-----------------|-------------|
| *`template <access::mode mode> access_mode_constant`* | Alias that stores `access::mode` into `std::integral_constant`. |
| *`template <access::mode mode> add_const_access`* | Used when casting an accessor by adding `const` to the type, deduces the new access mode. Resolves any access mode to `access::mode::read`, except for `access::mode::atomic`, which resolves to the same mode. |
| *`template <access::mode mode> remove_const_access`* | Used when casting an accessor by removing `const` from the type, deduces the new access mode. Resolves `access::mode::read` to `access::mode::read_write`, and keeps all other modes the same, since other modes already allow write access. |
| *`template <access::mode mode, typename oldT, typename newT> access_mode_from_cast`* | Used when casting an accessor from `oldT` to `newT`, deduces the new access mode. If both types have the same constness, `mode` is deduced as the new access mode. Otherwise it uses either `add_const_access` or `remove_const_access` to deduce the new access mode based on how the constness changed. |

## Conversion operator

The new interface of the `accessor` class would look like this:

```cpp
namespace cl {
namespace sycl {

template <typename dataT,
          int dimensions,
          access::mode accMode,
          access::target accTarget,
          access::placeholder isPlaceholder>
class accessor {
 public:
  /// All existing members here

  ...
  
  // Implicit conversion to `const dataT`
  operator accessor<const dataT,
                    dimensions,
                    add_const_access<dataT>::value,
                    accTarget,
                    isPlaceholder>() const;
};

} // namespace sycl
} // namespace cl
```

| Member function | Description |
|-----------------|-------------|
| *`operator accessor<const dataT, dimensions, add_const_access<accMode>::value, accTarget, isPlaceholder>() const`* | Returns a new accessor of type `const dataT`. The access mode of the new accessor is a read-only mode. |

## Conversion functions

In addition to the conversion operator,
we propose adding the following free functions to the `cl::sycl` namespace:

```cpp
namespace cl {
namespace sycl {

// Performs a static_cast of the contained pointer
template <typename dataU,
          typename dataT,
          int dimensions,
          access::mode accMode,
          access::target accTarget,
          access::placeholder isPlaceholder>
accessor<dataU,
         dimensions,
         access_mode_from_cast<accMode, dataT, dataU>::value,
         accTarget,
         isPlaceholder>
static_pointer_cast(
  const accessor<
    dataT, dimensions, accMode, accTarget, isPlaceholder>&
  acc);

// Performs a dynamic_cast of the contained pointer
template <typename dataU,
          typename dataT,
          int dimensions,
          access::mode accMode,
          access::target accTarget,
          access::placeholder isPlaceholder>
accessor<dataU,
         dimensions,
         accMode,
         accTarget,
         isPlaceholder>
dynamic_pointer_cast(
  const accessor<
    dataT, dimensions, accMode, accTarget, isPlaceholder>&
  acc);

// Performs a const_cast of the contained pointer
template <typename dataU,
          typename dataT,
          int dimensions,
          access::mode accMode,
          access::target accTarget,
          access::placeholder isPlaceholder>
accessor<dataU,
         dimensions,
         access_mode_from_cast<accMode, dataT, dataU>::value,
         accTarget,
         isPlaceholder>
const_pointer_cast(
  const accessor<
    dataT, dimensions, accMode, accTarget, isPlaceholder>&
  acc);

// Performs a reinterpret_cast of the contained pointer
template <typename dataU,
          typename dataT,
          int dimensions,
          access::mode accMode,
          access::target accTarget,
          access::placeholder isPlaceholder>
accessor<dataU,
         dimensions,
         accMode,
         accTarget,
         isPlaceholder>
reinterpret_pointer_cast(
  const accessor<
    dataT, dimensions, accMode, accTarget, isPlaceholder>&
  acc);

} // namespace sycl
} // namespace cl
```

The following function declarations are simplified
in order to reduce table verbosity.
For full declarations see the code above.

| Function | Description |
|-----------------|-------------|
| *`template <typename dataU, typename dataT, ...> accessor<dataU, ...> static_pointer_cast(const accessor<dataT, ...>& acc)`* | Performs a `static_cast` of the underlying pointer `dataT*` contained within `acc` to `dataU*` and returns a new `accessor` instance containing the cast pointer. When casting from a non-`const` to a `const` type, `add_const_access` is used to resolve the access mode. All other template parameters stay the same. This conversion is only valid if the `static_cast` from `dataT*` to `dataU*` is valid. |
| *`template <typename dataU, typename dataT, ...> accessor<dataU, ...> dynamic_pointer_cast(const accessor<dataT, ...>& acc)`* | Performs a `dynamic_cast` of the underlying pointer `dataT*` contained within `acc` to `dataU*` and returns a new `accessor` instance containing the cast pointer. All other template parameters stay the same. This conversion is only valid if the `dynamic_cast` from `dataT*` to `dataU*` is valid. If `sizeof(dataT) != sizeof(dataU)`, member functions `get_count()`, `get_range()`, and `get_offset()` of the returned accessor return values appropriate to the new type size. |
| *`template <typename dataU, typename dataT, ...> accessor<dataU, ...> const_pointer_cast(const accessor<dataT, ...>& acc)`* | Performs a `const_cast` of the underlying pointer `dataT*` contained within `acc` to `dataU*` and returns a new `accessor` instance containing the cast pointer. `access_mode_from_cast` is used to resolve the access mode. All other template parameters stay the same. This conversion is only valid if the `const_cast` from `dataT*` to `dataU*` is valid. |
| *`template <typename dataU, typename dataT, ...> accessor<dataU, ...> reinterpret_pointer_cast(const accessor<dataT, ...>& acc)`* | Performs a `reinterpret_cast` of the underlying pointer `dataT*` contained within `acc` to `dataU*` and returns a new `accessor` instance containing the cast pointer. All other template parameters stay the same. This conversion is only valid if the `reinterpret_cast` from `dataT*` to `dataU*` is valid. If `sizeof(dataT) != sizeof(dataU)`, member functions `get_count()`, `get_range()`, and `get_offset()` of the returned accessor return values appropriate to the new type size. |

## Examples

All examples use
  `dimensions == 1`,
  `accTarget == access::target::global_buffer`,
  and `isPlaceholder == access::placeholder::false_t`,
but they should generally apply to all accessor types.

### Simple casts

```cpp
using namespace cl::sycl;
static constexpr auto accTarget = access::target::global_buffer;

// Obtain some accessors
accessor<int, 1, access::mode::read, accTarget>
  accIntR =
    buf.get_access<access::mode::read, accTarget>(cgh);
accessor<int, 1, access::mode::write, accTarget>
  accIntW =
    buf.get_access<access::mode::write, accTarget>(cgh);
accessor<int, 1, access::mode::read_write, accTarget>
  accIntRW =
    buf.get_access<access::mode::read_write, accTarget>(cgh);
accessor<const int, 1, access::mode::read, accTarget>
  accIntCR =
    buf.get_access<access::mode::read, accTarget>(cgh);

// Conversion operator
auto rAccIntCR = static_cast<
  accessor<const int, 1, access::mode::read, accTarget>>(
    accIntR);
auto rAccIntCW = static_cast<
  accessor<const int, 1, access::mode::read, accTarget>>(
    accIntW);
auto rAccIntCRW = static_cast<
  accessor<const int, 1, access::mode::read, accTarget>>(
    accIntRW);
auto rAccIntCCR = static_cast<
  accessor<const int, 1, access::mode::read, accTarget>>(
    accIntCR);

// static_pointer_cast
accessor<const int, 1, access::mode::read, accTarget>
  rAccIntCR2 = static_pointer_cast<const int>(accIntR);
accessor<const int, 1, access::mode::read, accTarget>
  rAccIntCW2 = static_pointer_cast<const int>(accIntW);
accessor<const int, 1, access::mode::read, accTarget>
  rAccIntCRW2 = static_pointer_cast<const int>(accIntRW);
accessor<const int, 1, access::mode::read, accTarget>
  rAccIntCCR2 = static_pointer_cast<const int>(accIntCR);

// const_pointer_cast
accessor<int, 1, access::mode::read_write, accTarget>
  rAccIntR = const_pointer_cast<int>(accIntR);
accessor<int, 1, access::mode::write, accTarget>
  rAccIntW = const_pointer_cast<int>(accIntW);
accessor<int, 1, access::mode::read_write, accTarget>
  rAccIntRW = const_pointer_cast<int>(accIntRW);
accessor<int, 1, access::mode::read_write, accTarget>
  rAccIntCR3 = const_pointer_cast<int>(accIntCR);

// reinterpret_pointer_cast
accessor<float, 1, access::mode::read, accTarget>
  rAccFloatR = reinterpret_pointer_cast<float>(accIntR);
accessor<float, 1, access::mode::write, accTarget>
  rAccFloatW = reinterpret_pointer_cast<float>(accIntW);
accessor<float, 1, access::mode::read_write, accTarget>
  rAccFloatRW = reinterpret_pointer_cast<float>(accIntRW);
accessor<const float, 1, access::mode::read, accTarget>
  rAccFloatCR3 = reinterpret_pointer_cast<const float>(accIntCR);

```

### `dynamic_pointer_cast`

All of these examples also use the same access mode for all casts.

```cpp
using namespace cl::sycl;
static constexpr auto accMode = access::mode::read_write;
static constexpr auto accTarget = access::target::global_buffer;

struct Base {
  virtual void foo() const {}
  virtual ~Base(){}
};
struct Derived : public Base {
  void foo() const override {}
};

accessor<Base, 1, accMode, accTarget>
  accBase =
    buf.get_access<accMode, accTarget>(cgh);

accessor<Derived, 1, accMode, accTarget>
  accDerived = dynamic_pointer_cast<Derived>(accBase);
accessor<Base, 1, accMode, accTarget>
  accBase2 = dynamic_pointer_cast<Base>(accDerived);
```
