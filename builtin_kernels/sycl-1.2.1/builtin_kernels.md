# Builtin kernels

| Proposal ID | CP018 |
|-------------|--------|
| Name | Builtin kernel support in SYCL |
| Date of Creation | 12 October 2018 |
| Target | SYCL 1.2.1 vendor extension |
| Current Status | _Work In Progress_ |
| Implemented in | _N/A_ |
| Reply-to | Ruyman Reyes <ruyman@codeplay.com> |
| Original author | Ruyman Reyes <ruyman@codeplay.com> |

## Overview

Some OpenCL implementations expose already pre-built and embedded _built-in kernels_ to the users. 
This functionality is used in some cases to expose optimal implementation of some operations, or to
access some fixed-function hardware available on the platform as a non-programmable OpenCL kernel.

Nowadays SYCL developers can use the OpenCL interoperability functionality to use built-in kernels whenever required.
However this can be cumbersome and error prone as it forces switching back and forward from OpenCL and SYCL/C++.

This vendor extension to the SYCL 1.2.1 specification exposes an interface to work directly from SYCL with
OpenCL built-in kernels. 

Builtin-kernels can be created explicitly via a new method in the program class, `create_from_built_in_kernel` and then
used later on from the kernel object, or can be used implicitly with a explicit dispatch function.

In addition, this vendor extension enables further vendor extensions to create SYCL headers that expose a C++ interface
to the built-in kernels, enabling compile-time checking of parameters.

## Outside of the scope of this extension

* Host emulation of built-in kernels
* Methods to check whether if the program class has builtin kernels or not

## Revisions

This is the first revision of this extension.

## Interface changes

### Program class

The program class is extended with a new method, `create_from_built_in_kernel`, 
which internally uses the relevant low-level API call(s) to create a valid program object for the devices associated with the instance of the program.
Built-in kernels do not require a building stage, so the program state is immediately set to executable.

Built-in kernels are not available on the host-device, and will raise `cl::sycl::invalid_context` exception when calling the method on a host context.

A program that contains other kernels cannot be mixed with built in kernels, this would raise a `cl::sycl::compile_program_error` exception.
Similarly, building, compiling or linking a program created from built-in kernels raises `cl::sycl::invalid_object` exception.


## Example

The complete example can be found in the ComputeCPP SDK, the relevant parts are highlighted below:

```cpp
   {
      buffer<float, 1> buf(&input, range<1>(1));
      buffer<float, 1> bufOut(range<1>(1));
      bufOut.set_final_data(&output);

      program syclProgram(testContext);

      syclProgram.create_from_built_in_kernel(kBuiltinKernelName);

      auto kernelC = syclProgram.get_kernel(kBuiltinKernelName);
      
      testQueue.submit([&](handler& cgh) {
        auto accIn = buf.get_access<access::mode::read>(cgh);
        auto accOut = bufOut.get_access<access::mode::write>(cgh);

        cgh.set_arg(0, accIn);
        cgh.set_arg(1, accOut);

        auto myRange = range<1>{1};
        cgh.parallel_for(myRange, kernelC);
      });
   }
```

## References

* SYCL 1.2.1 specification
* OpenCL 1.2 specification
