# P1436 & P1437: Papers for affinity-based execution

|   |   |
|---|---|
| ID | CP013 |
| Name | Executor properties for affinity-based execution <br> System topology discovery for heterogeneous & distributed computing |
| Target | ISO C++ SG1, SG14, LEWG |
| Initial creation | 15 November 2017 |
| Last update | 31 March 2019 |
| Reply-to | Gordon Brown <gordon@codeplay.com> |
| Original author | Gordon Brown <gordon@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com>, Michael Wong <michael.wong@codeplay.com>, H. Carter Edwards <hcedwar@sandia.gov>, Thomas Rodgers <rodgert@twrodgers.com>, Mark Hoemmen <mhoemme@sandia.gov>, Jeff Hammond <jeff.science@gmail.com>, Tom Scogland <tscogland@llnl.gov> |

## Overview

This paper is the result of a request from SG1 at the 2018 San Diego meeting to split [P0796: Supporting Heterogeneous & Distributed Computing Through Affinity][p1436] into two separate papers, one for the high-level interface and one for the low-level interface. The high-level interface paper; [P1436: Executor properties for affinity-based execution][p1436] proposes a series of properties for querying affinity relationships and requesting affinity on work being executed. The low-level paper; [P1437: System topology discovery for heterogeneous & distributed computing][p1437] proposes a mechanism for discovering the topology and affinity properties of a given system.

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
| [P1436r1][p1436r1] | _Published_ |

### P1437: System topology discovery for heterogeneous & distributed computing

| Version | Status |
|---------|--------|
| [P1437r0][p1437r0] | _Published_ |

[p0796]: https://wg21.link/p0796
[p1436]: https://wg21.link/p1436
[p1437]: https://wg21.link/p1437
[p0796r0]: https://wg21.link/p0796r0
[p0796r1]: https://wg21.link/p0796r1
[p0796r2]: https://wg21.link/p0796r2
[p0796r3]: https://wg21.link/p0796r3
[p1436r0]: https://wg21.link/p1436r0
[p1436r1]: https://wg21.link/p1436r1
[p1437r0]: https://wg21.link/p1437r0