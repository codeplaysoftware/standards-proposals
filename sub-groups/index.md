# Sub Groups

| Proposal ID | CP016 |
|-------------|--------|
| Name | Sub Groups |
| Date of Creation | 14 September 2018 |
| Target | SYCL 2.2 |
| Current Status | _Work In Progress_ |
| Reply-to | Ruyman Reyes <ruyman@codeplay.com> |
| Original author | Ruyman Reyes <ruyman@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Victor Lomuller <victor@codeplay.com> |

## Overview

This proposal aims to define an interface for using OpenCL 2.x sub groups in
SYCL he provisional SYCL 2.2 specification (revision date 2016/02/15) already
contains SVM, but this proposal aims to make SVM in SYCL 2.2 more generic,
easier to program, better defined, and not necessarily tied to OpenCL 2.2.

## References

[1] SYCL 1.2.1 specification
https://www.khronos.org/registry/SYCL/specs/sycl-1.2.1.pdf

[2] SYCL 2.2 provisional specification (revision date 2016/02/15)
https://www.khronos.org/registry/SYCL/specs/sycl-2.2.pdf