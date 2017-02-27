# P0567R0: Asynchronous managed pointer for Heterogeneous computing

| Proposal ID | CP002 |
|-------------|--------|
| Name | Asynchronous managed pointer for Heterogeneous computing |
| Date of Creation | 22 July 2016 |
| Target | ISO C++ SG1, SG14 |
| Current Status | _Work In Progress_ |
| Reply-to | Gordon Brown <gordon@codeplay.com> |
| Original author | Gordon Brown <gordon@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com>, Michael Wong <michael.wong@codeplay.com>, Simon Brand <simon@codeplay.com> |

## Overview

This paper proposes an addition to the C\+\+ standard library to facilitate the management of a memory allocation which can exist consistently across the memory region of the host CPU and the memory region(s) of one or more remote devices. This addition is in the form of the class template ``managed_ptr``; similar to the ``std::shared_ptr`` but with the addition that it can share its memory allocation across the memory region of the host CPU and the memory region(s) of one or more remote devices.

## Versions

| Version | Last Modified | Document |
|---------|----- | ---------|
| D0567R1 | 23 February 2017 | [Link](cpp-20/P0567R0.md) |
| P0567R0 | 6 February 2017 | [Link](cpp-20/P0567R0.md) |