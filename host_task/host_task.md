| Proposal ID      | CP022                                                                                                                                                                                |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Name             | Host task with interop capabilities                                                                                                                                                  |
| Date of Creation | 16 January 2019                                                                                                                                                                      |
| Revision         | 0.6                                                                                                                                                                                  |
| Target           | Vendor extension                                                                                                                                                                     |
| Current Status   | 0.1 _Available since CE 1.0.5_, 0.6 TBD                                                                                                                                              |
| Reply-to         | Ruyman Reyes <ruyman@codeplay.com>                                                                                                                                                   |
| Original author  | Victor Lomüller <victor@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Peter Zuzek <peter@codeplay.com>                                                                          |
| Contributors     | Victor Lomüller <victor@codeplay.com>, Gordon Brown <gordon@codeplay.com>, Peter Zuzek <peter@codeplay.com>, Ruyman Reyes <ruyman@codeplay.com>, Bjoern Knafla <bjoern@codeplay.com> |

# host_task with interoperability: Improving SYCL-OpenCL Interoperability

## Motivation

SYCL 1.2.1 specification defines a dependency graph (DAG) across all 
command groups executing on the devices, based on the dependency 
information obtained from the usage of accessors or, potentially
from other extensions, events.
However, there is no capability in SYCL 1.2.1 to execute a task on the host
based on the DAG dependencies.
This limits the capabilities of SYCL to coordinate execution of tasks in
the host when data depends on the device.
Furthermore, in some of these cases, the user needs to access the underlying
OpenCL objects associated with the command group, such as the `cl_mem` 
object that corresponds to a given accessor in the host.
This allows users to write their own OpenCL code for interoperability with existing
libraries or re-use existing code while benefiting from the existing SYCL
DAG.

This proposal introduces a way for a user to retrieve the low-level objects
associated with SYCL buffers and enqueue a host task that can execute an
arbitrary portion of host code within the SYCL runtime, therefore taking
advantage of SYCL dependency analysis and scheduling.

As in all other commands in command group scope, the interoperability task 
executes asynchronously w.r.t to the submission point - as part of the SYCL DAG -
once all requirements are satisfied.

## Revisions

### 0.6
* Internal feedback, editorial and minor fixes
* Renamed methods to match better SYCL Generalization proposal

### 0.5
* Clarifications from user and implementation feedback
* Interop handler methods detailed on description
* Host task with an interop cannot be executed on a host queue
* Described the interaction of this proposal with SYCL Generalization

### 0.4
* Editorial fixes and minor clarifications

### 0.3
* Merging `interop_task` with the host task, to create a unified interface for host execution and interoperability
* Adding clarifications on how the interop task is scheduled and how it fits into the SYCL DAG.

### 0.2
* `get_buffer` renamed to `get_mem`
* Clarified wording on `get_queue` and `get_mem`
* `interop_handle` is passed by value to the lambda instead of reference

### 0.1

Initial proposal

## Execution of host tasks

This extension adds a new method to the handler, **codeplay_host_task**.
The parameter to this function is a functor (or lambda) that optionally takes an
**interop_handler** object as parameter. 

The example below shows a host task operating on two accessors, *accA* and
*accB*. For accessors to be used on the host task to operate from data
on the host, the target must be *host_buffer*.
Note that, although *host_buffer* typically refers to host accessors, and 
hence feature blocking behavior, these *host_buffer* accessors are
attached to a command group, therefore, they are not blocking but
express dependencies on the host for this command group.
The SYCL runtime will guarantee that data is available on the host before 
executing the host task, and subsequent operations will observe the effects 
of *host_task*writing on *accB*, following standard SYCL rules.

```cpp
auto cgH = [=](handler& h) {
  auto accA = bufA.get_access<access::mode::read, 
                access::target::host_buffer>(h);
  auto accB = bufB.get_access<access::mode::read_write, 
                access::target::host_buffer>(h);

  h.codeplay_host_task([=]() {
    accB[0] = accA[0] * std::rand();
  }
};

myQueue.submit(cgH);
```

Using methods or accessing any value of any accessor object, including 
placeholder accessors, that are not associated with the command group will 
throw a *sycl::invalid_object* exception.

The SYCL event returned by the command group will be completed when the
functor passed to `codeplay_host_task`  is completed (i.e. when the execution
reaches the end of the function). Note the SYCL event is completed regardless 
of the completion status of any third-party API or OpenCL operation enqueued 
or performed inside the `codeplay_host_task` scope. In particular, dispatching 
of asynchronous OpenCL operations inside of the `codeplay_host_task` requires manual
synchronization.

New SYCL objects must not be created in `codeplay_host_task` scope.
It is the user's responsibility to ensure that all operations with side effects
applying to the SYCL DAG must complete before the end of the `codeplay_host_task` 
scope. 

Since SYCL queues are out of order, and any underlying OpenCL queue can be as
well, there is no guarantee that OpenCL commands enqueued inside the
`codeplay_host_task` functor will execute in a particular order w.r.t other SYCL
commands or `codeplay_host_task` once dispatched to the OpenCL queue, unless this
is explicitly handled by using OpenCL events or barriers.

In addition, any of the following is undefined inside the **host_task** scope:

* Using any SYCL memory object (buffer or images)
* Construction of host accessors
* Using SYCL queue objects to synchronize or to submit work (e.g., queue.wait())

***Implementation note***: Command groups containing **host_task** commands
are asynchronous w.r.t the submission point, so it will likely be implemented 
on a separate thread.
Note that **host_task** does not have the restrictions that OpenCL callbacks do,
therefore cannot be implemented solely on the base of OpenCL callbacks.
A valid strategy for implementation is to launch a separate thread from an
OpenCL callback.

## Accessing low-level API functionality on SYCL queues

**host_task** can also provide an **interop\_handle** parameter, which
provides access to the underlying OpenCL objects inside the scope 
of the **host_task**.
This is achieved via a series of methods that retrieve 
underlying OpenCL objects, in particular:

* **cl_mem** objects for a given accessor that is on the list of requirements
* **cl_command_queue** for the current underlying queue
* **cl_device_id** for the device associated with the current SYCL queue
* **cl_context** for the context associated with the current SYCL queue

Executing a CG containing a  **host_task** on a host device is valid, 
but accessing the interoperability handler will cause undefined
behavior.

To obtain the underlying OpenCL object for a given accessor, the accessor
must target **global_buffer** or **constant_buffer**.
For the purposes of this proposal, all references to **global_buffer** are 
interchangeable with **constant_buffer**.
Trying to obtain the underlying OpenCL object of an accessor with 
**host_buffer** target throws an **sycl::invalid_object** exception. 

## Requirements and Accessors

The possibility of defining host and device accessors on the same command
group is not captured in the current SYCL Application Memory Model 
(Section 3.5.1). 
The SYCL Application Memory Model needs to be extended to take into account
that, using this extension, accessors can have requirements on both the device
and the host at the same time. 
It is out of the scope of this extension to fully clarify the SYCL 
Application Memory model to cater for a more complex set of requirements.
In this extension we simply extend the notation and the rules for combining
requirements to enable the required functionality.
The aim is (1) to clarify what are valid requirements to a command group and
(2) to guarantee that the side effects of the host task are visible
on a single accessor and no concurrent access to memory is required to
satisfy the requirements of the task.

The notation **buffer<sup>*Target*</sup><sub>*AccessMode*</sub>** extends
the notation used in Section 3.5.1 to include the *target* of the requirement.
The target can either be a *host_buffer (H)* or a *global_buffer (G)*, 
indicating whether the requirement is to be satisfied for the host or
a device respectively. When *Target* is not shown, it is assumed to be *G*.
Note that to satisfy requirements on either target, the actions performed can be different.
For example, lets define a buffer **b1** and the following command group submissions:

1. **CG0(b1<sub>RW</sub>)**
2. **CG1(b1<sup>H</sup><sub>RW</sub>)** 
  

**CG1** executes after **CG0**, since it requires read write access to the buffer that **CG0** is writing to. Given that **CG1** access targets **Host**, the SYCL Runtime will probably need to perform an action to update the **Host** data with the latest copy in the device.

Note that, if **CG1** target were **G**lobal memory, no action would
likely need to be performed to satisfy **CG1** requirements.
However, execution order would remain the same.

A command group can now have requirements to both Host and Global targets to the same buffer e.g. **CG0(b1<sub>RW</sub>, b1<sup>H</sup><sub>R</sub>)**.
However, the following applies:

1. **Accessor Mode Resolution**: For a given target, the existing SYCL 1.2.1 rule apply: The access mode is the union of all access modes to the same buffer, e.g. **CG0(b1<sub>RW</sub>, b1<sub>R</sub>) == CG0(b1<sub>RW</sub>)**, but **CG0(b1<sup>H</sup><sub>RW</sub>, b1<sub>R</sub>) != CG0(b1<sub>RW</sub>)**. This guarantees requirements are expressed in terms of the actual actions to be performed, not simply the access mode.
2. **Target Destination Resolution**: In a command group, given two requirements to the same buffer with different targets, only one of them can be *Write* (W) access. The target that contains the *Write* determines where the side-effects of the command-group will be observable for subsequent command groups.

Given the rules above, a *command group* submission must first ensure that 
*accessor mode resolution* is applied, guaranteeing that there is only one
requirement per target. Then, *target destination resolution* checks that
only one of the memory accesses requires writing to memory.
If more than one target requires writing, the *command group* is invalid and an error **sycl::invalid_object** is thrown.

## Interface changes

### Handler API

```cpp
namespace cl {
namespace sycl {

class handler {
 private:
  // implementation defined constructor
  handler(__unspecified__);

 public:
  /* ..... existing methods */
  /* Submit a task with interoperability statements. */
  template <typename FunctorT>
  void codeplay_host_task(FunctorT hostFunction);
};
}  // namespace codeplay
}  // namespace sycl
}  // namespace cl
```


| Method                                               | Description                                                                                                                                                                                                                                                                                                                                                      |
| ---------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ```void codeplay_host_task(FunctorT hostFunction)``` | Receives a C++ host functor or lambda to be executed on the host. The **hostFunction** must follow the rules defined in **Host Task Scope**. Submitting a command group using **codeplay_host_task** with an **interop_handle** parameter to a SYCL queue running on the host device is invalid, throws a **sycl::feature_not_supported** synchronous exception. |


### Interop Handle object

```cpp
namespace cl {
namespace sycl {

class interop_handle {
  interop_handle(__unspecified__); // Implementation defined

  public:

  template <typename dataT, int dimensions, access::mode accessmode,
            access::target accessTarget,
            access::placeholder isPlaceholder>
  cl_mem get_native_mem(const accessor<dataT, dimensions, accessmode, access::target accessTarget, access::placeholder isPlaceholder>&) const;

  cl_command_queue get_native_queue() const noexcept;

  cl_device_id get_native_device() const noexcept;

  cl_context get_native_context() const noexcept;
};

}  // namespace sycl
}  // namespace cl
```

<table>
  <tr>
    <th>Method</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><pre>cl_command_queue get_native_queue() const noexcept</pre></td>
    <td>The ``get_native_queue`` method returns an underlying OpenCL queue for the SYCL queue used to submit the command group, or the fallback queue if this command-group is re-trying execution on an OpenCL queue. The OpenCL command queue returned is implementation-defined in cases where the SYCL queue maps to multiple underlying OpenCL objects. It is responsibility of the SYCL runtime to ensure the OpenCL queue returned is in a state that can be used to dispatch work, and that other potential OpenCL command queues associated with the same SYCL command queue are not executing commands while the host task is executing.</td>
  </tr>
  <tr>
    <td><pre>cl_context get_native_context() const noexcept</pre></td>
    <td>The ``get_native_context`` method returns an underlying OpenCL context associated with the SYCL queue used to submit the command group, or the fallback queue if this command-group is re-trying execution on an OpenCL queue.</td>
  </tr>
  <tr>
    <td><pre>cl_device_id get_native_device() const noexcept</pre></td>
    <td>The ``get_native_device`` method returns an underlying OpenCL device associated with the SYCL queue used to submit the command group, or the fallback queue if this command-group is re-trying execution on an OpenCL queue.</td>
  </tr>
  <tr>
    <td><pre> template &#8249;typename dataT, int dimensions, access::mode accessmode, access::target accessTarget, access::placeholder isPlaceholder&#x203A;
  cl_mem get_native_mem(const accessor&#8249;dataT, dimensions, accessmode, access::target accessTarget, access::placeholder isPlaceholder&#x203A;&) const;
</pre></td>
    <td>The `get_native_mem` method receives a SYCL accessor that has been defined is a requirement for the command group, and returns the underlying OpenCL 
memory object that is used by the SYCL runtime. If the accessor passed as parameter is not part of the command group requirements (e.g. it is an unregistered placeholder accessor), the exception `cl::sycl::invalid_object` is thrown asynchronously.</td>
  </tr>
</table>

All objects returned by the `interop_handle` method are guaranteed to be alive on the host task scope. 
User does not need to retain them to use them on said scope.
It is undefined what happens when a returned object is retain by the user and used outside of the host task scope.

## Examples

Trivial example showing the `interop_handle` API to call an unnamed third party api:

```cpp
    auto cgH = [=] (handler& cgh) {
      // Default access is global buffer
      auto accA = bufA.get_access<access::mode::read>(cgh);
      auto accB = bufB.get_access<access::mode::read_write>(cgh);

      h.codeplay_host_task([=](interop_handle& handle) {
        third_party_api(handle.get_native_queue(), // Get the OpenCL command queue to use, can be the fallback
                        handle.get_native_mem(accA), // Get the OpenCL mem object behind accA
                        handle.get_native_mem(accB)); // Get the OpenCL mem object behind accB
        // Assumes call has finish when exiting the scope ends.
      });
    };
    qA.submit(cgH);
```

Example of a host task used to populate data from a file, writing to the
host memory.

```cpp
    auto cgH = [=] (handler& cgh) {
      auto accB = bufB.get_access<access::mode::write, 
                                  access::target::host_buffer>(cgh);

      h.codeplay_host_task([=]() {
        std::ifstream ifs(some_file_name, std::ifstream::in);
        std::for_each(std::begin(accB), std::end(accA), [&](auto& elem) {
          if (!ifs.good()) {
            elem = 0;
          } else {
            elem = ifs.get();
          }
        });
      });
    };
    qA.submit(cgH);
```

This example calls the clFFT library from SYCL using the `host_task` and 
retrieving the underlying OpenCL objects from the `interop_handle`:

```cpp
#include <stdlib.h>
#include <CL/sycl.hpp>

/* No need to explicitly include the OpenCL headers */
#include <clFFT.h>

int main( void )
{
  size_t N = 16;

  cl::sycl::queue device_queue;
  cl::sycl::buffer<float> X(range<1>(N * 2));

  /* Setup clFFT. */
  clfftSetupData fftSetup;
  err = clfftInitSetupData(&fftSetup);
  err = clfftSetup(&fftSetup);

  device_queue.submit([=](handler& cgh) {
    auto X_accessor = X.get_access<access::mode::read_write>(cgh);
    h.codeplay_host_task([=](interop_handle handle) {
      /* FFT library related declarations */
      clfftPlanHandle planHandle;
      size_t clLengths[1] = {N};

      /* Create a default plan for a complex FFT. */
      err = clfftCreateDefaultPlan(&planHandle, handle.get_context(), CLFFT_1D, clLengths);

      /* Set plan parameters. */
      err = clfftSetPlanPrecision(planHandle, CLFFT_SINGLE);
      err = clfftSetLayout(planHandle, CLFFT_COMPLEX_INTERLEAVED, CLFFT_COMPLEX_INTERLEAVED);
      err = clfftSetResultLocation(planHandle, CLFFT_INPLACE);

      /* Bake the plan. */
      err = clfftBakePlan(planHandle, 1, &queue, NULL, NULL);

      /* Execute the plan. */
      cl_command_queue queue = handle.get_native_queue();
      cl_mem X_mem = handle.get_native_mem(X_accessor);
      err = clfftEnqueueTransform(planHandle, CLFFT_FORWARD,
                                  1, &queue, 0, NULL, NULL,
                                  &X_mem, NULL, NULL);

      /* Wait for calculations to finish. */
      err = clFinish(queue);

      /* Release the plan. */
      err = clfftDestroyPlan( &planHandle );
    });
  });

  /* Release clFFT library. */
  clfftTeardown( );

  return 0;
}
```

The following example reads data from the host and writes it to the device on the same buffer:

```cpp
    auto cgH = [=] (handler& cgh) {
      auto accAH = bufA.get_access<access::mode::read, access::target::host_buffer>(cgh);
      auto accAD = bufA.get_access<access::mode::read_write>(cgh);

      h.codpelay_host_task([=](interop_handle handle) {
         clEnqueueWriteBuffer(handle.get_queue(), handle.get_native_mem(accAD),
              CL_TRUE, 0, bufA.get_size(), accAH.get_pointer(), 
              0, NULL, NULL);
      });
    };
    qA.submit(cgH);

    {
    auto accH = bufA.get_access<access::mode::read, access::target::host_buffer>();
    /* SYCL runtime will likely trigger another copy or map operation from
       device to host in this case, since it doesn't know the user triggered just
       a copy from device to host. The only thing that the SYCL runtime knows 
       is that the side-effects of the user-defined host task are visible on the
       device. */
    }
```

## Interaction with SYCL generalization proposal

This document describes an extension to SYCL 1.2.1 for OpenCL, and
does not cover the changes requires on this extension to integrate with the
SYCL generalization proposal.

It is the aim of the authors that this feature becomes part of the core of a
next SYCL specification. There are no major design changes that will prevent
it to be integrated without OpenCL, but some clarifications are needed.
Sections below describe the current known issues together with a possible
solution. 

### Name of the method

Were this feature made into SYCL core specification, the name should simply be **host_task**
rather than **codeplay_host_task**.

### Availability of host tasks across backends

The host task feature is described in terms of the SYCL General Application
model, hence, it is expected to work across all different SYCL backends.

It is the authors belief that the feature can be implemented directly 
in the SYCL runtime without requiring backend specific features or capabilities.

The **interop handle** parameter to the **host task** can be generalized to all backends
by using templated return types, as illustrated below:

```cpp
namespace sycl {

class interop_handle {
  interop_handle(__unspecified__); // Implementation defined

  public:

  template <typename dataT, int dimensions, access::mode accessmode,
            access::target accessTarget,
            access::placeholder isPlaceholder>
  auto get_native_mem(const accessor<dataT, dimensions, accessmode, access::target accessTarget, access::placeholder isPlaceholder>&) const
          -> typename interop<backendName, sycl::buffer>::type;

  template<backend backendName>
  auto get_native_queue() const noexcept 
          -> typename interop<backendName, sycl::queue>::type;

  template<backend backendName>
  auto get_native_device() const noexcept
          -> typename interop<backendName, sycl::device>::type;

  template<backend backendName>
  auto get_native_context() const noexcept
          -> typename interop<backendName, sycl::context>::type;
};

}  // namespace sycl
```

This allows a generic interface for all backends. SYCL developers can use the mechanism available in the generalization proposal to write backend-specific code.

Alternatively, SYCL developer can use different host task functor objects
for different backends, and select the right one at dispatch time.

Note that, when using the **interop_handle** methods in the host backend, 
the return value for **get_native_mem** will be a raw host pointer.
The return value of the rest of the methods is, at the time of writing, undefined.

## Not included

* Image support: Image support should work in the same way, but we have leave it out from the extension at this point.
