# Automatic placeholder requirements

| Proposal ID | CP028  |
|-------------|--------|
| Name | Automatic placeholder requirements |
| Date of Creation | 17 April 2020 |
| Revision | 0.1 |
| Latest Update | 28 April 2020 |
| Target | SYCL Next (after 1.2.1) |
| Current Status | _Work in Progress_ |
| Reply-to | Peter Žužek <peter@codeplay.com> |
| Original author | Peter Žužek <peter@codeplay.com> |
| Contributors | Gordon Brown <gordon@codeplay.com>, Ruyman Reyes <ruyman@codeplay.com> |

## Overview

This proposal allows placeholders to be automatically registered
when used in explicit memory functions.
It also provides convenience versions of those functions on the `queue` class.

## Revisions

### 0.1

* Initial proposal

## Motivation

[A separate proposal](https://github.com/codeplaysoftware/standards-proposals/pull/122)
deprecated `accessor::placeholder`,
making it easier to construct placeholder accessors.

An example on how this can be used to copy data
from an accessor to a host pointer:

```cpp
// Assume buf of type buffer<int, 1>
// Assume hostPtr of type int*

// Create the placeholder
accessor<int, 1, access::mode::read> acc{buf};

queue q;
q.submit([&](handler& cgh){
  // Register placeholder with the command group
  cgh.require(acc);

  // Perform the copy
  cgh.copy(acc, hostPtr);
});
```

At the point of calling `cgh.copy` the SYCL runtime knows where data is requested.
Calling `cgh.require` shouldn't really be necessary at that point
because the `copy` function could just call `require` on its own.
That would allow us to simplify the command group:

```cpp
q.submit([&](handler& cgh){
  // Perform the copy
  cgh.copy(acc, hostPtr);
});
```

Everything is automatically handled by the SYCL runtime.
However, we could go one step further,
and provide convenience member functions on the queue,
allowing us to skip the submission lambda
(just like the new queue member functions in the
[USM proposal](https://github.com/intel/llvm/blob/sycl/sycl/doc/extensions/USM/USM.adoc)):

```cpp
// Perform the copy
q.copy(acc, hostPtr);
```

## Changes

In the accessor section,
add the following paragraph:

```latex
Any member function of the \codeinline{queue} or the \codeinline{handler}
that accepts an \codeinline{accessor} automatically creates
a \textbf{requirement} for the command group.
This is not true for member functions that enqueue a kernel - 
\codeinline{single_task}, \codeinline{parallel_for}, \codeinline{parallel_for_work_group} -
in those cases it is still required to manually register the placeholder
with the command group.
```

In the section on explicit memory,
add the following paragraph:

```latex
The same members functions listed in Table~\ref{table.members.handler.copy}
are also available as member functions of the \codeinline{queue} class,
but they can only accept \keyword{placeholder} accessors.
```

## Limitations

There is a temptation to extend this automatic registration
to member functions of the `handler` that perform kernel enqueues:
`single_task`, `parallel_for`, `parallel_for_work_group`.
However, that might prove a bit more difficult
because the accessors are captured inside a function object
and C++ doesn't yet provide proper facilities for inspecting that object -
static reflection would be needed.
Therefore it is our concern that extending the proposal to kernel enqueues
may not be implementable on the SYCL host device.
