# SYCL Design Philosophy

| Proposal ID      |                                                           |
|------------------|-----------------------------------------------------------|
| Name             | SYCL Design Philosophy                                    |
| Date of Creation | 8 April 2022                                              |
| Last Update      | 18 October 2022                                           |
| Version          | v0.1                                                      |
| Target           | Any SYCL version, currently SYCL 2020 and a future version |
| Current Status   | _Work In Progress_                                        |
| Implemented in   | _N/A_                                                     |
| Reply-to         | Peter Žužek <peter@codeplay.com>                          |
| Original Author  | Peter Žužek <peter@codeplay.com>                          |
| Contributors     | Peter Žužek <peter@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Tadej Ciglarič <tadej.ciglaric@codeplay.com>, Lukas Sommer <lukas.sommer@codeplay.com>, Ewan Crawford <ewan@codeplay.com>, Aidan Belton <aidan@codeplay.com>, Ross Brunton <ross@codeplay.com>, Erik Tomusk <erik.tomusk@codeplay.com>, Ruyman Reyes <ruyman@codeplay.com> |

## Motivation

The SYCL programming model has been designed from the start
to be a portable programming model for multiple accelerators,
with the aim of helping application developers support their goals of performance portability.
To ensure the main goal of SYCL is preserved as its ecosystem and capabilities expand
and multiple hardware vendors support it,
this document aims to define what is the overall design philosophy of SYCL,
spelling out what is required for a feature to be accepted into the programming model
and how features should be generally exposed to developers.

## Implementation portability

Every non-optional SYCL feature should be portable and performantly implementable by any implementation.
Some features might need some sort of software emulation by some SYCL implementation vendors,
which is acceptable as long as the performance of such a feature is acceptable.
There is no clear threshold for what performance is acceptable,
but a guiding rule is that if a SYCL user has to completely avoid the feature
or re-implement the feature in a more performant manner themselves,
then that feature should not become part of the SYCL specification.
It could, however, be considered as a vendor or a KHR extension.

We refer to this rule in this document by saying that a feature has to be
**performantly implementable**.

## Path to standardization

There are a few main ways to bring a proposal into the SYCL realm:

1. Create a vendor extension
2. Create a KHR extension
3. Extend the SYCL specification directly

The ordering here is also the proposed workflow for extending the SYCL specification:
An implementation vendor could first create a vendor extension,
after some feedback and showing that it would be useful to other vendors
create an official Khronos extension (either EXT or KHR),
then after additional feedback move it into the SYCL specification document.

Not all of these steps are required.
A proposal could start or end at any point, or a step could even be skipped.
Multiple vendors could team up to write an EXT or KHR extension,
or try to go directly for the specification document.
This document tries to highlight the rationale and the criteria
for each of these steps.

### Vendor extensions

If a SYCL implementation vendor implements functionality
that is not part of an existing SYCL specification,
they are encouraged to formalize it as a SYCL vendor extension.
Generally the vendor has full control over the standardization process,
but they are encouraged to follow the structure
specified in the version of the SYCL specification they are targeting.
This is in order to avoid clashes with other vendor extensions
or future versions of the SYCL specification.

An example of why a vendor might want to consider a vendor extension
is when they want to expose SYCL functionality that only makes sense for them
and not other vendors.

However, formalizing the extension can pique interest from other vendors,
and is a good first step towards getting the functionality into the SYCL specification.
Multiple vendor could also team up to define a common extension,
i.e. an EXT extension.

The SYCL WG should make an effort to be aware of different vendor extensions
and try to standardize them more formally in a unified way
if it can be shown that standardization would benefit SYCL as a whole.

### KHR extensions

If multiple SYCL implementation vendors are interested in the same extended functionality,
they are encouraged to work together to try to define an extension
within the remit of the Khronos SYCL Working Group (SYCL WG),
i.e. a KHR extension.
Examples might include vendor B adopting vendor A's extension,
or multiple vendors working together on functionality they wish to standardize.
A feature might remain a KHR extension and not be included in the SYCL specification
if it is only performantly implementable on a limited set of devices.

It is also possible for a single vendor to push for a KHR extension,
but a KHR extension must be performantly implementable by other vendors as well -
otherwise it should remain a vendor extension.

The SYCL WG should maintain an easily accessible list of all KHR extensions.
A KHR extension follows the same guidelines for portable extensions
specified in the SYCL specification just as a vendor extension does,
just that the vendor is `KHR`.

All extensions should be versioned and old versions maintained for the record.

### Modifying the SYCL specification

Proposals for new features are encouraged to go through a vendor or KHR extension first
before being considered for inclusion in the SYCL specification,
in a similar process as how Technical Specifications (TS) are defined for the ISO C++ standard.

However, a feature can be added directly to the SYCL specification,
provided it can be performantly implemented by all implementations on all backends,
but not necessarily on all devices for each backend
(see [Optional features](#optional-features) for further discussion).
Even if a feature could in theory be performantly implemented by all vendors,
vendors are also encouraged to take into account how widely used the new feature might be
and the implementation burden being imposed on other vendors.

## Optional features

Every SYCL feature should be performantly implementable by all vendors
on all SYCL backends.
However, there might be cases where a feature would still meet this criteria,
but wouldn't necessarily be performantly implementable on all devices,
especially since different devices have different hardware capabilities.

If it's possible to check for a specific device capability at runtime,
it might make sense to make that feature optional in the SYCL specification.
Some possible runtime mechanisms for checking optional features are:

* Information descriptors (`sycl::info` namespace, `get_info` function)
* Properties (`sycl::property` namespace, `has_property`/`get_property` functions)
* Aspects (`sycl::aspect` enum class, `has` function)

There are some features where the functionality cannot be queried at runtime,
instead it requires a compile-time check.
These kinds of optional features are discouraged compared to runtime-checkable ones,
and are instead encouraged to become KHR extensions.
Some possible compile-time mechanisms for checking optional features are:

* Compile time properties (proposed for SYCL Next)
* `constexpr` boolean variables (can be used in a `constexpr` expression)
* Feature test macros (prefer `constexpr` if possible)

## How and when to ship new standard

## Library-only

TODO

## Reduced feature set

TODO

## Split host/device compilation model

TODO

## Alignment with C++

TODO

## Teachability

TODO

## Glossary

* **performantly implementable** - see [Implementation portability](#implementation-portability)
* **KHR extension** - a SYCL extension defined by the SYCL WG
* **SYCL WG** - the Working Group responsible for maintaining SYCL within Khronos
* **vendor** - an entity implementing the SYCL specification
