# Shared Virtual Memory

| Proposal ID | CP014 |
|-------------|--------|
| Name | Shared Virtual Memory |
| Date of Creation | 22 January 2018 |
| Target | SYCL 2.2 |
| Current Status | _Work In Progress_ |
| Reply-to | Peter Žužek <peter@codeplay.com> |
| Original author | Peter Žužek <peter@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Victor Lomuller <victor@codeplay.com>, Duncan McBain <duncan@codeplay.com>, Ralph Potter <ralph@codeplay.com> |

## Overview

This proposal aims to define an interface to use Shared Virtual Memory (SVM)
in SYCL 2.2. The provisional SYCL 2.2 specification (revision date 2016/02/15)
already contains SVM, but this proposal aims to make SVM in SYCL 2.2 more
generic, easier to program, better defined, and not necessarily tied to
OpenCL 2.2.

## References

[1] SYCL 1.2.1 specification
https://www.khronos.org/registry/SYCL/specs/sycl-1.2.1.pdf

[2] SYCL 2.2 provisional specification (revision date 2016/02/15)
https://www.khronos.org/registry/SYCL/specs/sycl-2.2.pdf

[3] OpenCL 1.2 specification
https://www.khronos.org/registry/OpenCL/specs/opencl-1.2.pdf

[4] OpenCL 2.2 specification
https://www.khronos.org/registry/OpenCL/specs/opencl-2.2.pdf

[5] C11 final draft specification
http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1548.pdf

[6] C++14 final draft specification
https://github.com/cplusplus/draft/blob/master/papers/n4140.pdf

[7] C++17 final draft specification
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4659.pdf

[8] Memory Consistence proposal
https://github.com/codeplaysoftware/standards-proposals/blob/master/asynchronous-data-flow/sycl-2.2/02_memory_consistence.md
