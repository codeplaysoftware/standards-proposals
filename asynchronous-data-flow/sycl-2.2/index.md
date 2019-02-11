# Extending the data-flow execution model for SYCL 2.2

The current SYCL 2.2 specification bases its memory model on OpenCL 2.2.
The OpenCL 2.2 specification details how the values in memory, as seen from
each execution unit, interact so that programmers can reason about the
correctness of OpenCL programs.

It is important to note that, in OpenCL, being a relatively low-level language,
it is the developers sole responsibility to ensure that the order
of execution of different kernels in different contexts and devices maintains
correct results. This is typically achieved via queues and event
synchronizations.
In OpenCL 2.2, users can also define a mutex in some configurations of the
Shared Virtual Memory that enable further fine-grained control of the
memory synchronization.

SYCL offers a higher abstraction in terms of kernel ordering synchronization.
Developers only need to specify what data is required to execute a particular
kernel. The SYCL runtime will guarantee that kernels are executed in an
order that guarantees the correct expected result. By specifying access
modes and types of memory, a DAG of kernels is built, similar to what other 
models offer (e.g OpenMP tasks, OMPSs, ...).

However, the SYCL 2.2 specification, and the current SYCL 1.2 specification,
do not describe the behavior of the SYCL runtime precisely as a DAG, but
vaguely describe the expected behaviour of the different accessors and kernels.
In addition, when developers try to leverage the asynchronous execution
features of the SYCL programming model, they lack sufficient direct control
over these features to (a) integrate the SYCL dataflow model into other 
scheduler-based applications and to (b) provide enough low-level control of
the host/device interface.

In this document we extend the data flow memory model for SYCL 2.2 to define
a set of rules for memory synchronization, so that programmers
can reason about the correctness of SYCL programs properly. This proposal also
extends the current API with a number of methods and mechanism that allow
fine-tuned control of the memory operations performed by the SYCL runtime,
allowing expert developers to feed the runtime with information about the
application being executed - enabling SYCL to seamlessly integrate with other
programming models and frameworks.

This proposal aims to
contribute the following to the SYCL 2.2 specification:

1. [A description of the expected behaviour of a SYCL program in terms of data dependencies between command groups](01_command_group_requirements_and_actions.md)
2. [How the definition of accessors affects the synchronization of the program](02_memory_consistence.md)
3. [Additional functionality to interact with data on the host](03_interacting_with_data_on_the_host.md)
4. [Extending the buffer/image interface to provide hints to the runtime](04_update_on_specific_context.md)

The following sections elaborate the different aspects of the proposal.
