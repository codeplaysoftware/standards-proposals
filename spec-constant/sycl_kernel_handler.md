# SYCL kernel_handle

|                  |                                        |
| ---------------- | ---------------------------------------|
| Name             | kernel_handle                          |
| Date of creation | 17th Feb 2020                          |
| Last updated     | 17th Feb 2020                          |
| Status           | WIP                                    |
| Current revision | 1                                      |
| Available        | N/A                                    |
| Reply-to         | Victor Lomuller <victor@codeplay.com>  |
| Original author  | Victor Lomuller <victor@codeplay.com>  |
| Contributors     | TBD |

## Overview

The library implementation of certain device features require the use of a device side handler.
This proposal introduces a `kernel_handler` to provide access to extra device capabilities implementable as a library.

## Motivation

The initial proposal of specialization constant forced the user to explicitly get individual `specialization_constant` objects that needed to be propagated in the final program.

Using a handler, the user has only 1 object to carry to access `specialization_constant` objects thus simplifying the interface.

For now, the proposal is limited to support specialization constant but can be extended to handle barriers or other functionalities.

## Revisions

v1:

    * Initial proposal

## `sycl::kernel_handler`

The `sycl::kernel_handler` is a non-user constructible class only passed to user as an argument of the functor passed to `handler::parallel_for` and `handler::parallel_for_work_group`.

```cpp
namespace sycl {
class kernel_handler {
private:
  kernel_handler();

public:

  // Return the value associate with the specialization constant id `s`.
  // The value returned is either the 
  template<class T, specialization_id<T>& s>
  T get_specialization_constant();

  template<auto& s>
  typename std::remove_reference_t<decltype(s)>::type get_specialization_constant();

};
}
```

## Update `sycl::handler` class definition

Functor passed to `sycl::handler::single_task`, `sycl::handler::parallel_for` and `sycl::handler::parallel_for_work_group` can take an extra `sycl::kernel_handler` as extra by-value argument.

Below is an example of invoking a SYCL kernel function with `single_task`:

```cpp
myQueue.submit([&](handler & cgh) {
  cgh.single_task<class myKernel>([=] () {});
});
```

or

```cpp
myQueue.submit([&](handler & cgh) {
  cgh.single_task<class myKernel>([=] (sycl::kernel_handler h) {});
});
```

Below is an example of invoking a SYCL kernel function with `parallel_for`:

```cpp
myQueue.submit([&](handler & cgh) {
  cgh.parallel_for<class myKernel>(range<1>(numWorkItems),
  [=] (id<1> index) {});
});
```

or

```cpp
myQueue.submit([&](handler & cgh) {
  cgh.parallel_for<class myKernel>(range<1>(numWorkItems),
  [=] (id<1> index, sycl::kernel_handler h) {});
});
```
