# Async Work Group Copy & Prefetch Builtins

| Proposal ID | C009 |
|-------------|--------|
| Name | Async Work Group Copy & Prefetch Builtins |
| Date of Creation | 19 July 2017 |
| Target | SYCL 2.2 |
| Current Status | _Work In Progress_ |
| Reply-to | Gordon Brown <gordon@codeplay.com> |
| Original author | Gordon Brown <gordon@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com> |

## Overview

This paper proposes the addition of the asycnhronous work group copy and prefetch builtin functions in SYCL. As these builtin functions require sycnhronization within a SYCL kernel, this paper also proposes the addition of the the device_event class and the wait group events builtin.

## Versions

[Version 1](sycl-2.2/index.md)