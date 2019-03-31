# P1436: Executor properties for affinity-based execution

|   |   |
|---|---|
| ID | CP013 |
| Name | Executor properties for affinity-based execution <br> System topology discovery for heterogeneous & distributed computing |
| Target | ISO C++ SG1, SG14, LEWG |
| Initial creation | 15 November 2017 |
| Last update | 31 March 2019 |
| Reply-to | Gordon Brown <gordon@codeplay.com> |
| Original author | Gordon Brown <gordon@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com>, Michael Wong <michael.wong@codeplay.com>, H. Carter Edwards <hcedwar@sandia.gov>, Thomas Rodgers <rodgert@twrodgers.com>, Mark Hoemmen <mhoemme@sandia.gov> |

## Overview

This paper is the result of a request from SG1 at the 2018 San Diego meeting to split [P0796: Supporting Heterogeneous & Distributed Computing Through Affinity][p1436r0] into two separate papers, one for the high-level interface and one for the low-level interface. The high-level interface paper; [P1436: Executor properties for affinity-based execution][D1436r1] proposes a series of properties for querying affinity relationships and requesting affinity on work being executed. The low-level paper; [P1437: System topology discovery for heterogeneous & distributed computing][d1437r0] proposes a mechanism for discovering the topology and affinity properties of a given system.

## Versions

### P0796: Supporting Heterogeneous & Distributed Computing Through Affinity

| Version | Status |
|---------|--------|
| [P0796r0][p0796r0] | _Published_ |
| [P0796r1][p0796r1] | _Published_ |
| [P0796r2][p0796r2] | _Published_ |
| [P0796r3][p0796r3] | _Published_ |

### P1436: Executor properties for affinity-based execution

| Version | Status |
|---------|--------|
| [P1436r0][p1436r0] | _Published_ |
| [D1436r1][d1436r1] | _Work In Progress_ |

### P1437: System topology discovery for heterogeneous & distributed computing

| Version | Status |
|---------|--------|
| [D1437r0][d1437r0] | _Work In Progress_ |

[p0796r0]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0796r0.pdf
[p0796r1]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0796r1.pdf
[p0796r2]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0796r2.pdf
[p0796r3]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0796r3.pdf
[p1436r0]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1436r0.html
[d1436r1]: cpp-23/d1436r1.md
[d1437r0]: cpp-23/d1437r0.md