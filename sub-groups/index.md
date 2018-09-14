# Basic sub-group extension

| Proposal ID | CP016 |
|-------------|--------|
| Name | Basic sub group extension |
| Date of Creation | 14 September 2018 |
| Target | SYCL 1.2.1 |
| Current Status | _Work In Progress_ |
| Reply-to | Ruyman Reyes <ruyman@codeplay.com> |
| Original author | Ruyman Reyes <ruyman@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Victor Lomuller <victor@codeplay.com> |

## Overview

This vendor extension aims to define an interface to expose sub-group functionality,
as defined in the SYCL 2.2 provisional and the OpenCL 2.2 provisional, 
in SYCL 1.2.1.

The extension is only targeting OpenCL devices that expose 
`cl_codeplay_basic_sub_group` vendor extension.


## References

[1] SYCL 1.2.1 specification
https://www.khronos.org/registry/SYCL/specs/sycl-1.2.1.pdf

[2] SYCL 2.2 provisional specification (revision date 2016/02/15)
https://www.khronos.org/registry/SYCL/specs/sycl-2.2.pdf

[3] OpenCL 2.2 API specification
https://www.khronos.org/registry/OpenCL/specs/2.2/pdf/OpenCL_API.pdf

[4] OpenCL C++ 1.0 specification
https://www.khronos.org/registry/OpenCL/specs/2.2/pdf/OpenCL_Cxx.pdf

