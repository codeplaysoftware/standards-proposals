# Accessors as Containers

| Proposal ID | CP027  |
|-------------|--------|
| Name | Accessors as Containers |
| Date of Creation | 22 March 2019 |
| Revision | 0.1 |
| Latest Update | 22 March 2020 |
| Target | SYCL Next (after 1.2.1) |
| Current Status | _Work in Progress_ |
| Reply-to | Peter Žužek <peter@codeplay.com> |
| Original author | Peter Žužek <peter@codeplay.com> |
| Contributors | Gordon Brown <gordon@codeplay.com> |

## Overview

This proposal aims to introduce a `Container` interface for accessors.

## Revisions

### 0.1

* Initial proposal

## Motivation

Users want to be able to iterate over accessors
and pass them to algorithms that rely on iterators.

## Changes

Apply the changes to accessors of the following access targets:

* `global_buffer`
* `constant_buffer`
* `local`
* `host_buffer`

Add a section to the spec that says roughly the following:
> Accessors of certain access targets
meet the C++ ContiguousContainer named requirement.
The exception to this is that only the destructor of an accessor of target
`access::target::local` destroys all containing elements and frees the memory.
Other accessor types do not own the memory,
so their destructor does not destroy the elements or free memory.

Meeting the [ContiguousContainer](https://en.cppreference.com/w/cpp/named_req/ContiguousContainer)
named requirement means adding all the types, member functions, and operators
as defined by the [Container](https://en.cppreference.com/w/cpp/named_req/Container)
named requirement.
This includes, among other things,
adding a default constructor to all accessors.
Additionally, the iterator for the `accessor` container
is the pointer obtained by calling `accessor::get_pointer`.

The change is not limited to 1D accessors.
For accessors of higher dimensions,
iterators are linearized according to standard SYCL linearization.

When the access mode is `read`,
for a given underlying data type `T`,
the `ContiguousContainer` requirement is met for the type `const T`.

## Examples

```cpp
queue q;
static constexpr size_t r0 = 32;
static constexpr size_t r1 = 16;
const auto bufRange = range<2>{r0, r1};
buffer<int, 2> buf{bufRange};

q.submit([&](handler& cgh) {
  auto globalAcc = buf.get_access<access::mode::write>(cgh);

  auto localAcc =
    accessor<int, 1, access::mode::read_write, access::target::local>{
      range<1>{2 * r1}, cgh};

  q.parallel_for<class init>(nd_range<2>{bufRange, range<2>{r1, 1}},
                             [=](nd_item<2> i) {
    using std::begin;
    using std::end;

    const auto globalId = i.get_global_linear_id();
    const auto localId = i.get_local_linear_id();

    // Initialize two consecutive values to the local ID
    std::fill_n(begin(localAcc) + localId, 2, static_cast<int>(localId));

    i.barrier();
    if (localId == 0) {
      // Accumulate local data and add global ID
      localAcc[0] = std::accumulate(begin(localAcc), end(localAcc), globalId);
    }
    i.barrier();

    // Iterator linearizes the 2D global buffer accessor
    *(begin(globalAcc) + globalId) = localAcc[0];
  });
});

{
  auto hostAcc = buf.get_access<access::mode::read>();
  for (auto val : hostAcc) {
    std::cout << val << std::endl;
  }
}
```
