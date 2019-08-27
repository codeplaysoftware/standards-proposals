# Default-constructed Buffers

Proposal ID      | TBC
---------------- | ---------------------------
Name             | Default-constructed Buffers
Date of Creation | 2019-08-20
Target           |
Status           | WIP
Author           | Duncan McBain [Duncan McBain](mailto:duncan@codeplay.com)
Contributors     |

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
that takes no arguments, which initialises the buffer with a zero-size range.
```c++
namespace cl {
namespace sycl {
template <typename T, int dimensions = 1,
typename AllocatorT = cl::sycl::buffer_allocator>
class buffer {
  buffer();

  bool is_valid() const noexcept;

  explicit operator bool() const noexcept;
};
} // namespace sycl
} // namespace cl
```
The template arguments should remain the same, so that the argument can be
rebound to a new `buffer` instance later using the copy constructor.

The `is_valid()` call would allow the programmer to query whether or not
the buffer can be used, i.e. whether or not it was constructed with a range of
size zero. The operator would call this same function but allow its use in
if statements.

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
