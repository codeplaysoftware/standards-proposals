# Casting multi_ptr pointers

## Motivation

The current SYCL 1.2.1 specification contains the `multi_ptr` class
which is designed mostly for OpenCL interoperability
and calling certain functions which rely on OpenCL builtins.
The `multi_ptr` class has two template parameters:
`ElementType` as the type of the underlying data,
and `Space` to designate the address space of the pointer.
It is not allowed to cast pointers to a different address space,
but casting pointers to a different underlying type is completely valid C++,
within the type restrictions for the cast.

Programmers wanting to cast `multi_ptr<A, Space>` to `multi_ptr<B, Space>`
don't have many options to do so in SYCL 1.2.1.
There are only two specified casts in SYCL 1.2.1:
  * Using `static_cast` to cast `multi_ptr<A, Space>` to `multi_ptr<void, Space>`
    (defined through an explicit conversion operator)
  * Using `static_cast` to cast `multi_ptr<void, Space>` to `multi_ptr<B, Space>`
    (defined through an explicit conversion operator)

Therefore, the only option to perform a cast from
`multi_ptr<A, Space>` to `multi_ptr<B, Space>`
is to use both casts like so:
```cpp
// Using global_ptr here to simplify the code
// decltype(multiPtrA) == global_ptr<A>
auto multiPtrB = static_cast<global_ptr<B>>(
                  static_cast<global_ptr<void>>(multiPtrA));
```

This is problematic on a few levels:
  * Verbosity
  * Only allows static casts
  * Allows casting the underlying `A*` pointer to a `B*` pointer
    even if the type system forbids it
  * Does not handle `const` qualifiers

Therefore, there is a clear need to provide more casting options for the `multi_ptr` class
in order to make the casting safer and easier to use.

## Summary

This proposal adds a few explicit conversion operators
as member functions of the `multi_ptr` class,
and also adds several free functions to the `cl::sycl` namespace
that follow the naming and semantics of the `std::shared_ptr` pointer cast functions
defined by the C++17 standard: https://en.cppreference.com/w/cpp/memory/shared_ptr/pointer_cast.

## Explicit conversion operators

The new interface of the `multi_ptr` class would look like this:

```cpp
namespace cl {
namespace sycl {

template <typename ElementType, access::address_space Space>
class multi_ptr {
 public:
  /// All existing members here

  ...

  // Explicit conversion to `multi_ptr<ElementTypeU, Space>`
  template <typename ElementTypeU>
  explicit operator multi_ptr<ElementTypeU, Space>() const;

  // Implicit conversion to `multi_ptr<const ElementType, Space>`
  operator multi_ptr<const ElementType, Space>() const;
};

} // namespace sycl
} // namespace cl
```

The conversion operator to `multi_ptr<ElementTypeU, Space>` replaces
the existing explicit conversion to `multi_ptr<void, Space>`.

| Member function | Description |
|-----------------|-------------|
| *`template <typename ElementTypeU>  explicit operator multi_ptr<ElementTypeU, Space>() const`* | Performs a static cast of the underlying pointer `ElementType*` to `ElementTypeU*` and returns a new `multi_ptr` instance containing the cast pointer. The address space stays the same. This conversion is only valid if the `static_cast` from `ElementType*` to `ElementTypeU*` is valid. |
| *`operator multi_ptr<const ElementType, Space>() const`* | Performs a static cast of the underlying pointer `ElementType*` to `const ElementType*` and returns a new `multi_ptr` instance containing the cast pointer. The address space stays the same. This conversion is implicit because it is always valid to add a `const` qualifier. |

## Conversion functions

In addition to the conversion operators,
we propose adding the following free functions to the `cl::sycl` namespace:

```cpp
namespace cl {
namespace sycl {

// Performs a static_cast of the contained pointer
template <typename ElementTypeU, typename ElementTypeT, access::address_space Space>
multi_ptr<ElementTypeU, Space>
  static_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr);

// Performs a dynamic_cast of the contained pointer
template <typename ElementTypeU, typename ElementTypeT, access::address_space Space>
multi_ptr<ElementTypeU, Space>
  dynamic_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr);

// Performs a const_cast of the contained pointer
template <typename ElementTypeU, typename ElementTypeT, access::address_space Space>
multi_ptr<ElementTypeU, Space>
  const_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr);

// Performs a reinterpret_cast of the contained pointer
template <typename ElementTypeU, typename ElementTypeT, access::address_space Space>
multi_ptr<ElementTypeU, Space>
  reinterpret_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr);

} // namespace sycl
} // namespace cl
```

In the table below, each function has the following template parameters:
```cpp
template <typename ElementTypeU, typename ElementTypeT, access::address_space Space>
```

| Function | Description |
|-----------------|-------------|
| *`multi_ptr<ElementTypeU, Space> static_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr)`* | Performs a `static_cast` of the underlying pointer `ElementTypeT*` contained within `multiPtr` to `ElementTypeU*` and returns a new `multi_ptr` instance containing the cast pointer. The address space stays the same. This conversion is only valid if the `static_cast` from `ElementType*` to `ElementTypeU*` is valid. |
| *`multi_ptr<ElementTypeU, Space> dynamic_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr)`* | Performs a `dynamic_cast` of the underlying pointer `ElementTypeT*` contained within `multiPtr` to `ElementTypeU*` and returns a new `multi_ptr` instance containing the cast pointer. The address space stays the same. This conversion is only valid if the `dynamic_cast` from `ElementType*` to `ElementTypeU*` is valid. |
| *`multi_ptr<ElementTypeU, Space> const_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr)`* | Performs a `const_cast` of the underlying pointer `ElementTypeT*` contained within `multiPtr` to `ElementTypeU*` and returns a new `multi_ptr` instance containing the cast pointer. The address space stays the same. This conversion is only valid if the `const_cast` from `ElementType*` to `ElementTypeU*` is valid. |
| *`multi_ptr<ElementTypeU, Space> reinterpret_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr)`* | Performs a `reinterpret_cast` of the underlying pointer `ElementTypeT*` contained within `multiPtr` to `ElementTypeU*` and returns a new `multi_ptr` instance containing the cast pointer. The address space stays the same. This conversion is only valid if the `reinterpret_cast` from `ElementType*` to `ElementTypeU*` is valid. |

## Examples

### Simple casts

These examples focus on `global_ptr` for brevity,
but the same operation is valid on any other `multi_ptr` type.

```cpp
using namespace cl::sycl;

const global_ptr<int> ptrInt = get_some_global_ptr<int>();

// Conversion operator
auto ptrVoid1 = static_cast<global_ptr<void>>(ptrInt);
auto ptrConstInt1 = static_cast<global_ptr<const int>>(ptrInt);

// static_pointer_cast
global_ptr<void> ptrVoid2 =
  static_pointer_cast<void>(ptrInt);
global_ptr<const int> ptrConstInt2 =
  static_pointer_cast<const int>(ptrInt);

// const_pointer_cast
global_ptr<const int> ptrConstInt3 =
  const_pointer_cast<const int>(ptrInt);
// global_ptr<int> ptrIntStripConst =
//   static_cast<global_ptr<int>>(ptrConstInt1); // illegal
global_ptr<int> ptrIntStripConst =
  const_pointer_cast<int>(ptrConstInt1);

// reinterpret_pointer_cast
global_ptr<float> ptrFloat4 =
  reinterpret_pointer_cast<float>(ptrInt);
global_ptr<void> ptrVoid4 =
  reinterpret_pointer_cast<void>(ptrInt);
global_ptr<const int> ptrConstInt4 =
  reinterpret_pointer_cast<const int>(ptrInt);
```

### `dynamic_pointer_cast`

```cpp
struct Base {
  virtual void foo() const {}
  virtual ~Base(){}
};
struct Derived : public Base {
  void foo() const override {}
};

using namespace cl::sycl;
const global_ptr<Base> ptrBase = get_some_global_ptr<Base>();

auto ptrDerived = dynamic_pointer_cast<Derived>(ptrBase);
auto ptrBase1 = dynamic_pointer_cast<Base>(ptrDerived);
```

### Passing `multi_ptr` to functions

```cpp
using namespace cl::sycl;

template <typename ElementType, access::address_space Space>
void function_taking_ptr(const multi_ptr<ElementType, Space>& ptr);
template <typename ElementType, access::address_space Space>
void function_taking_const_ptr(const multi_ptr<const ElementType, Space>& ptr);

const global_ptr<int> ptrInt = get_some_global_ptr<int>();

function_taking_ptr(ptrInt);
// Implicit conversion to global_ptr<const int>:
function_taking_const_ptr(ptrInt);

const global_ptr<float> ptrFloat = get_some_global_ptr<float>();

// Convert float to int pointer
function_taking_ptr(reinterpret_ptr_cast<int>(ptrFloat));
// First explicit conversion to global_ptr<int>
// and then implicit conversion to global_ptr<const int>:
function_taking_const_ptr(reinterpret_ptr_cast<int>(ptrFloat));

```
