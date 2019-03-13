# P1436: Executor properties for affinity-based execution

|   |   |
|---|---|
| ID | CP013 |
| Name | Executor properties for affinity-based execution |
| Target | ISO C++ SG1, SG14, LEWG |
| Initial creation | 15 November 2017 |
| Last update | 21 January 2019 |
| Reply-to | Gordon Brown <gordon@codeplay.com> |
| Original author | Gordon Brown <gordon@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com>, Michael Wong <michael.wong@codeplay.com>, H. Carter Edwards <hcedwar@sandia.gov>, Thomas Rodgers <rodgert@twrodgers.com>, Mark Hoemmen <mhoemme@sandia.gov> |

## Overview

This paper is the result of a request from SG1 at the 2018 San Diego meeting to split P0796: Supporting Heterogeneous & Distributed Computing Through Affinity [[35]][p0796] into two separate papers, one for the high-level interface and one for the low-level interface. This paper focusses on the high-level interface: a series of properties for querying affinity relationships and requesting affinity on work being executed. [[36]][p1437] focusses on the low-level interface: a mechanism for discovering the topology and affinity properties of a given system.

The aim of this paper is to provide a number of executor properties that if supported allow the user of an executor to query and manipulate the binding of *execution agents* and the underlying *execution resources* of the *threads of execution* they are run on.

## Versions

| Version | Status |
|---------|--------|
| [P0796r0][p0796r0] | _Published_ |
| [P0796r1][p0796r1] | _Published_ |
| [D0796r2][p0796r2] | _Published_ |
| [D0796r3][p0796r3] | _Published_ |
| [DXXX1r0](cpp-20/d1436r0.md) | _Work In Progress_ |

[p0796r0]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0796r0.pdf
[p0796r1]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0796r1.pdf
[p0796r2]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0796r2.pdf
[p0796r3]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0796r3.pdf