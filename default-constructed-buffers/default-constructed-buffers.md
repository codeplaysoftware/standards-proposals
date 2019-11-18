# Default-constructed Buffers

Proposal ID      | CP021
---------------- | ---------------------------
Name             | Default-constructed Buffers
Date of Creation | 2019-08-20
Target           | SYCL 1.2.1
Status           | _Draft_
Author           | Duncan McBain [Duncan McBain](mailto:duncan@codeplay.com)
Contributors     | Duncan McBain, [Gordon Brown](mailto:gordon@codeplay.com), [Ruyman Reyes](mailto:ruyman@codeplay.com)

## Description

In SYCL, there are no buffer constructors that take no arguments, which means
that `cl::sycl::buffer` cannot be default-constructed. However, this limits the
use of buffers in some interfaces. Consider a library function that has an
optional user-provided allocation for temporary data. The library can avoid
allocations if the library user so desires by providing a buffer here, but how
should the user indicate that allocation is allowed? A natural way would be to
provide a default argument in the API function:

```c++
template <typename Allocation_t>
void foo(Allocation_t input_a,
         Allocation_t input_b,
         Allocation_t temp = Allocation_t{});
```

For `Allocation_t` types that are pointer-like, the library can check for
`nullptr` and allocate if this condition is true. A similar mechanism that
allows `cl::sycl::buffer` to be constructed and checked for usability would
aid their use in this style of generic interface.

## Proposal

The `cl::sycl::buffer` class should be augmented with an additional constructor
that takes no arguments, which default-constructs the buffer in an
implementation-defined manner.
```c++
namespace cl {
namespace sycl {
template <typename T, int dimensions = 1,
typename AllocatorT = cl::sycl::buffer_allocator>
class buffer {
  buffer();

  bool has_storage() const noexcept;

  explicit operator bool() const noexcept;
};
} // namespace sycl
} // namespace cl
```
The template arguments should remain the same, so that the argument can be
rebound to a new `buffer` instance later using the copy constructor.

The `has_storage()` call would allow the programmer to query whether or not
the buffer can be used, i.e. whether or not it was default-constructed. The
explicit conversion operator would call this same function but allow its use
in `if` statements.

Requesting access from a default-constructed buffer should throw an exception.
It is not meaningful to use a default-constructed buffer. Since there
is no allocation associated with a default-constructed `buffer`,
`cl::sycl::buffer::set_final_data` and `cl::sycl::buffer::set_write_back`
should behave as if the `buffer` had a final pointer of `nullptr` at all times.
The other functions in the `buffer` API should behave as though the buffer were
constructed with a `range` of size zero and otherwise behave normally.

Explicitly constructing a buffer with a range of zero is not allowed. This does
not correspond to a valid allocation.

## Sample code

```c++
#include <CL/sycl.hpp>
using namespace sycl = cl::sycl;
using namespace access = sycl::access;

class name;

void api_func(sycl::queue& q,
              sycl::buffer<float, 1> input, sycl::buffer<float, 1> output,
              sycl::buffer<float, 1> workspace = sycl::buffer<float, 1>{}) {
  if (!workspace) {
    // calculate required size for workspace
    constexper auto size = 2048llu;
    workspace = sycl::buffer<float, 1>{sycl::range<1>{size}};
  }
  // use buffer
  q.submit([&] (sycl::handler& cgh) {
    auto acc_in = input.get_access<access::mode::read>(cgh);
    auto acc_out = input.get_access<access::mode::write>(cgh);
    auto acc_workspace = input.get_access<access::mode::read_write>(cgh);
    cgh.single_task<name>([=]() {
      // uses accessors
    });
  });
}
```
