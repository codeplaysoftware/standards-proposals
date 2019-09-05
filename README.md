# Public proposals for standards groups
## Codeplay Software Ltd.

## Objective of the repository

This repository contains different proposals on various working groups
that Codeplay Software Ltd. is currently involved in.

The aim of this public repository is to facilitate sharing information
with our partners and public in general.

## Structure of the repository

Each proposal is stored on a separate directory, named after the proposal
itself. Inside the directory, some proposals may have different directories
for different working groups or standards - for example, some proposals may
be combined for Khronos SYCL for OpenCL and ISO C++.

## How to contribute

We encourage interested users and developers in the community to contribute
to our proposals. Feedback can be sent via Github issues, or by forking
the repository and contributing pull requests.

Developers and members of the community can contact us directly via our
[website](https://www.codeplay.com/support/contact/).

## Status of the proposals

This repository contains proposals in different states of work,
some of them will be work in progress while others are published and finished.
Each proposal in the table below will be tagged with one of the following states:

* _Work In Progress_ : The proposal is still a work in progress, so large changes should be expected.
* _Draft_ : A draft of the proposal is ready, and no major changes are expected. This status normally indicates that the proposal is ready for feedback from the general public.
* _Final Draft_ : The proposal is a draft submitted to the relevant standards body, and only minor changes are expected.
* _Published_ : The proposal is finished and no more work is expected.
* _Accepted_, _Accepted with changes_ or _Partially accepted_ : Accepted on the standard version indicated as target. Refer to that standard document from now on for the latest status of the feature.

## Current list of proposals and status

| ID | Name                   | Target | Initial creation | Latest update | Status |
| --- | ---------------------- | ------ | ---------------- | ------------- | ------ |
| CP001 | [Asynchronous Data Flow](asynchronous-data-flow/index.md) | SYCL 1.2.1 |   20 July 2016   | 11 Jan 2017   | _Partially accepted_ |
| CP003 | [Implicit Accessor Conversions](implicit-accessor-conversions/sycl-2.2/implicit-accessor-conversions.md) | SYCL 1.2.1 | 28 March 2017 | 30 March 2017 | _Accepted with changes_ |
| CP004 | [Placeholder Accessors](placeholder_accessors.md) | SYCL 1.2.1 | 20 July 2016 | 12 Jun 2017 | _Accepted with changes_ |
| CP005 | [Asynchronous managed pointer for Heterogeneous computing](managed-pointer/index.md) | ISO C++ SG1, SG14 | 22 July 2016 | 6 Feb 2017 | _Published_ |
| CP006 | [Maybe unused attribute in decomposition declarations](defects-2017-02/cpp-17/maybe-unused-decomposition-decls.md) | ISO C++ EWG | 22 February 2017 | 22 February 2017 | _Published_ |
| CP007 | [Vector Load and Store Operations](vector-operations/sycl-2.2/vector-loads-and-stores.md) | SYCL 1.2.1 | 29 March 2017 | 30 March 2017 | _Accepted with changes_ |
| CP008 | [Buffer tied to a context](tied-buffer/index.md) | SYCL 1.2.1 | 17 March 2017 | 4 July 2017 | _Accepted_ |
| CP009 | [Async Work Group Copy & Prefetch Builtins](async-work-group-copy/index.md) | SYCL 1.2.1 | 07 August 2017 | 07 August 2017 | _Accepted with changes_ |
| CP011 | [Mem Fence Builtins](mem-fence/index.md) | SYCL 1.2.1 | 11 August 2017 | 9 September 2017 | _Accepted_ |
| CP012 | [Data Movement in C++](data-movement/index.md) | ISO C++ SG1, SG14 | 30 May 2017 | 28 August 2017 | _Work in Progress_ |
| CP013 | [P1436 & P1437: Papers for affinity-based execution](affinity/index.md) | ISO C++ SG1, SG14, LEWG | 15 November 2017 | 17 June 2019 | _Published_ |
| CP014 | [Shared Virtual Memory](svm/index.md) | SYCL 2.2 | 22 January 2018 | 22 January 2018 | _Work in Progress_ |
| CP015 | [Specialization Constant](spec-constant/index.md) | SYCL 1.2.1 extension | 24 April 2018 | 24 April 2018 | _Work in Progress_ |
| CP017 | [Host Access](host_access/index.md) | SYCL 1.2.1 vendor extension | 17 September 2018 | 13 December 2018 |  _Available since CE 1.0.3_ | 
| CP018 | [Built-in kernels](builtin_kernels/index.md) | SYCL 1.2.1 vendor extension | 12 October 2018 | 12 October 2018 | _Available since CE 1.0.3_ | 
| CP019 | [On-chip Memory Allocation](onchip-memory/index.md) | SYCL 1.2.1 vendor extension  | 03 December 2018 | 03 December 2018 | _Available since CE 1.0.3_ |
| CP020 | [Default-Constructed Buffers](default-constructed-buffers/default-constructed-buffers.md) | SYCL 1.2.1 | 27 August 2019 | 5 September 2019 | _Draft_ |
