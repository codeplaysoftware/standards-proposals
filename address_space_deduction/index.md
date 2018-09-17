# Supporting Non-Flat Address Space Through Implicit Type Deduction

|   |   |
|---|---|
| ID | TBC |
| Name | Supporting Non-Flat Address Space Through Implicit Type Deduction |
| Target | SYCL 1.2.1 and ISO C++ SG1 SG14 |
| Initial creation | 10 September 2018 |
| Last update | 16 September 2018 |
| Reply-to | Victor Lomüller <victor@codeplay.com> |
| Original author | Victor Lomüller <victor@codeplay.com> |
| Contributors | TBC |

## Overview

This paper discuss SYCL's strategy to deal with explicit address spaces in C++ which can only view the memory as uniform.


## Introduction

SYCL is a higher-level programming model for OpenCL as a single-source domain specific embedded language (DSEL) based on pure C++11/14.
Like any standard, there are ambiguities that arise from the interaction of heterogeneous domain with a language such as C++ designed for homogeneous computing.
These ambiguities are potential issues in any future Heterogeneous integration with ISO C++.

The address space representation of SYCL 1.2.1 can be distributive and, as it will be shown, the standard can be ambiguous on how address space should be deduced and the type propagated in the user code.
The purpose of this document is to provide an exhaustive list of current ambiguities and propose recommendations on how to resolve them.

### SYCL 1.2.1 Representation of Address Spaces

From a user point of view, address spaces on pointers are never directly expressed.
This is important as SYCL is a pure C++ programming model, therefore expressing them the way it is in OpenCL C would make SYCL code not compilable by a C++ compiler in a portable way.
So an efficient address space representation is critical to ensure that SYCL remains compatible with the C++ standard (C++11 and higher) and still be executable with OpenCL devices.

To hide address spaces, SYCL encapsulates them into objects of the following type
```c++
namespace cl {
namespace sycl {
 template<typename T, cl::sycl::access::address_space Space>
 class multi_ptr;
}
}
```
which represent a pointer to a type T in a specific address space named using one of enum class value `cl::sycl::access::address_space`.
For convenience, SYCL also provides type aliases `global_ptr<T>`, `local_ptr<T>`, `private_ptr<T>` and `constant_ptr<T>` to represent pointers to global, local, private and constant memory respectively.

The class is designed to represent the boundary from which SYCL can start to deduce address space while compiling to a device.
Full details of the class can be found in the section 4.7.7 of the SYCL 1.2.1 specification.

When users declares a pointer or reference initialized using a `cl::sycl::multi_ptr` object, the address space deduction can apply constraint on variable types to implicitly change them by adding the address space.
This constraint can then be propagated to other variables.
If at some point a typing issue is detected, the compiler can raise an error.

### Maintaining Exact Address Space Information

SYCL cannot rely on the generic address space like OpenMP or CUDA do.
This means any pointer or reference type used need to hold the exact address space it is pointing/referring even if the user is not employing an explicit address space in his code.
To propagate this address space information, SYCL employs a type deduction system to update users type according to the context in which it is used.
This address space propagation mechanism is described in the section 6.8 of the SYCL 1.2.1 specification.

In a context where a C++ code can be run on device (will be referred as "device context" : i.e. piece of code that can be reached from a SYCL kernel), all pointer/reference variables initializers, arguments to functions and return values can act as a constraint on the types and the compiler is then allowed to change the type of the value on which it is applied.

For instance:
```
global_ptr<int> g_ptr;
int* i = g_ptr;
```
In this piece of code, the `g_ptr` object represent a pointer to an integer in the global memory.
In OpenCL C, this would be equivalent to `__global int* g_ptr = NULL;`.
`multi_ptr` objects can implicitly be converted to pointers.
In a device context, the returned pointer of this conversion will be a pointer to an integer in the global memory.
This is the type constraint of the variable i, and in this context, the variable type can be promoted from `int*` to `__global int*`.
Using this promotion rule, the compiler build this view of the code:

| User code | Internal compiler view after type inference |
|---|---|
|`global_ptr<int> g_ptr;`<br>`int* i = g_ptr;` | `global_ptr<int> g_ptr;`<br>`__global int* i = g_ptr;`|

This kind of promotion can lead to conflict in terms of type such as:
```c++
global_ptr<int> g_ptr;
local_ptr<int> l_ptr;
int* i = g_ptr;
int* j = l_ptr;
i = j;
```
In this code, the variable `i` is constrained to be promoted to `__global int*` and `j` to be promoted to `__local int*`.
The assignment on the last line poses a problem in terms of type as `__local int*` cannot be converted into `__global int*`.
In this case, the SYCL 1.2.1 specs allows the compiler to raise an error as the deduced type of `i` is incompatible with the deduced type of `j`.

It can be noted that even a SYCL 1.2.1 implementation can rely on the generic address space for implementation.
In such case, the implementation can assume that `i` and `j` are pointers to the generics address space, and so this last code sample would be legal.

The rest of this document will only consider implementations that do not rely on the generic address space.

## Challenges Raised by Duplication and Address Space Deduction

The current SYCL specification is ambiguous on how the newly deduced type should interacts with some of the core C++ features like lambda, `decltype` keyword, template instantiation, template argument deduction and more.
Even if SYCL's general intent clearly states that it follows the C++ language, many ambiguities arises in those situations.

This section lists known ambiguities.

### Lambda Captures

```c++
cl::sycl::global_ptr<int> g_ptr;
int* i = g_ptr;
auto l = [i]() {};
```

This simple code snippet captures the variable \emph{i} into a lambda.
The variable is declared as a pointer to an integer but the constraint system of SYCL makes it a pointer to an integer in the global memory.
Therefore the captured variable should be of this deduced type, so intuitively, the lambda needs to be reconstructed so that it takes into account this implicitly deduced type.

### decltype keyword

```c++
cl::sycl::global_ptr<int> g_ptr;
cl::sycl::local_ptr<int>  l_ptr;
int* i = g_ptr;
decltype(i) j = l_ptr;
```

On the last line of this code snippet, the result of the expression `decltype(i)` is unspecified by the specification.
The variable i is constrained by `g_ptr` which forces it to be a `__global int*` type.
The 2 possible interpretations for the type of `j` can be:
 *  The type of i has been deduced to `__global int*`, therefore `decltype(i)` should resolve to the same type hence the assigment should fail;
 *  The user wrote i to be of type `int*` therefore, `decltype(i)` should resolve to `int *` as well hence type deduction makes `j` a variable of type `__local int*`;

The issue is similar with local type such as lambda:
```c++
cl::sycl::global_ptr<int> g_ptr;
cl::sycl::local_ptr<int>  l_ptr;
int* i = g_ptr;
auto F = [=](decltype(i)) {};
F(l_ptr);
```
The 2 previous interpretations would be valid as well, the first would make this code illegal, the second may have a chance to compile.

Last issue is related to template instantiation:
```c++
template<typename T>
struct Foo {
  T i;
};

[...]

cl::sycl::global_ptr<int> g_ptr;
int* i = g_ptr;
Foo<decltype(i)> F = {i};
```
In this case, the first interpretation would make this code compile but not the second.
The template issue is discussed more detailed in the next section.

### Template Instantiation

Similarly, the question of re-instantiating templates according to the deduced type does not have a straight forward answer.
For instance:

```c++
template<typename T>
void funcCall(T i) {
  static_assert(
    std::is_same<T, int*>::value && "Invalid type T");
}

[...]
cl::sycl::global_ptr<int> g_ptr;
int* i = g_ptr;
funcCall(i);
```
Template re-evaluation may lead to compilation errors due to template traits not aware of address spaces.
In the code above, assuming that `T` is deduced to `global int*`, the re-evaluation of the template will make the `static_assert` be triggered if `global int*` and `int*` are 2 distinct types.

Fully aware users may use this system to their advantage to define C-style pointers with address spaces as illustrated bellow:
```c++
template<typename T>
struct Foo {
  T i;
};
cl::sycl::global_ptr<int> g_ptr;
int* i = g_ptr;
Foo<decltype(i)> foo{i};
```
Here, the object `foo` contains an address space qualified pointer and this information can be propagated throughout the user code.
Although, this can quickly become tricky to handle as from one user written type (`int*`) potentially four different types can be derived: `private int*` (which may be distinct from `int*`), `global int*`, `local int*`, `constant int*`.
And so templates for all type combinations can be instantiated and may interfere with each other.

### Template Parameter Deduction

```c++
template<typename T>
void funcCall(T* i)  {
  T t = *i;
}

[...]
cl::sycl::global_ptr<int> g_ptr;
int* i = g_ptr;
funcCall(i);
```
Considering the code snippet above, the variable `i` is forced to be promoted to `global int*`, therefore the template parameter could, depending on the interpretation, be deduced to;
  * `int` and let the address space deduction system promote the argument of `funcCall`;
  * `global int`, this makes the address space deduction system not needed but will make the variable `t` a local variable in the global address space, which is illegal in OpenCL 1.2.


## Versions

| Version | Status |
|---------|--------|
| Draft |  _Work In Progress_ |
