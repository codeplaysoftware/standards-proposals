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
  * Does not handle `const` cases

Therefore, there is a clear need to provide more casting options for the `multi_ptr` class
in order to make the casting safer and easier to use.

## Summary

This proposal adds a few explicit conversion operators
as member functions of the `multi_ptr` class
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

  // Explicit conversion to `multi_ptr<const ElementType, Space>`
  explicit operator multi_ptr<const ElementType, Space>() const;
};

template <access::address_space Space>
class multi_ptr<void, Space> {
 public:
  /// All existing members here

  ...

  // Explicit conversion to `multi_ptr<const void, Space>`
  explicit operator multi_ptr<const void, Space>() const;
};

} // namespace sycl
} // namespace cl
```

The conversion operator to `multi_ptr<ElementTypeU, Space>` replaces
the existing explicit conversion to `multi_ptr<void, Space>`.

TODO(Peter): Table

TODO(Peter): Handle multi_ptr<const void, Space>

## Conversion functions

In addition to the conversion operators,
we propose adding the following free functions to the `cl::sycl` namespace:

```cpp
namespace cl {
namespace sycl {

template <typename ElementTypeU, typename ElementTypeT, access::address_space Space>
multi_ptr<ElementTypeU, Space>
  static_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr);

template <typename ElementTypeU, typename ElementTypeT, access::address_space Space>
multi_ptr<ElementTypeU, Space>
  dynamic_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr);

template <typename ElementTypeU, typename ElementTypeT, access::address_space Space>
multi_ptr<ElementTypeU, Space>
  const_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr);

template <typename ElementTypeU, typename ElementTypeT, access::address_space Space>
multi_ptr<ElementTypeU, Space>
  reinterpret_pointer_cast(const multi_ptr<ElementTypeT, Space>& multiPtr);

} // namespace sycl
} // namespace cl
```

TODO(Peter): Description

TODO(Peter): Table
