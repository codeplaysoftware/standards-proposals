# Managed Pointer

| Proposal ID | CP002 |
|-------------|--------|
| Name | PXXXXR0: Managed Pointer |
| Date of Creation | 22 July 2016 |
| Target | ISO C++ SG1 |
| Current Status | _Work In Progress_ |
| Reply-to | Gordon Brown <gordon@codeplay.com> |
| Original author | Gordon Brown <gordon@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com>, Michael Wong <michael.wong@codeplay.com>, Simon Brand <simon@codeplay.com> |

## Overview

This paper proposes an addition to the C\+\+ standard library to facilitate the management of a memory allocation which can exist consistently across the memory region of the host CPU and the memory region(s) of one or more remote devices. This addition is in the form of the class template ``managed_ptr``; similar to the ``std::shared_ptr`` but with the addition that it can share its memory allocation across the memory region of the host CPU and the memory region(s) of one or more remote devices.

## Versions

| Version | Date | Document |
|---------|----- | ---------|
| 1.0 | 31 January 2017 | [Link](cpp-20/index.md) |