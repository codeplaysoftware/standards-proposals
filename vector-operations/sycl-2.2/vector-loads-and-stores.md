**Document number: **
**Date: 2017-03-30**
**Project: SYCL 2.2 Specification**
**Authors: Gordon Brown, Ruyman Reyes**
**Emails: gordon@codeplay.com, ruyman@codeplay.com**
**Reply to: gordon@codeplay.com, ruyman@codeplay.com**

# Vector Load and Store Operations

## Motivation

In the current SYCL 1.2 specification the vector load and store operations are not supported. These are operations which exist in OpenCL for loading and storing elements of a vector from and to an array of elements of the same type as the cahnnels of the vector.

For example:

```cpp

float floatData[1024];

buffer<float, 1> floatBuffer(floatData, range<1>(1024));

queue.submit([&](handler &cgh){

  auto acc = buf.get_access<access::mode::read>(cgh);

  cgh.parallel_for(range<1>(1024), [=](item<1> idx){

    size_t offset = (idx.get_global(0) / 4);

    float4 f4;

    f4.load(offset, acc.get_pointer());

    result = someComputation(f4);

    result.store(offset, acc.get_pointer());
  });

});
```

The proposal is to add the load and store member function templates to the vec class.

## Load and Store Member Functions

```cpp
template <access::address_space addressSpace>
void load(size_t offset, multi_ptr<dataT, addressSpace> ptr);

template <access::address_space addressSpace>
void store(size_t offset, multi_ptr<dataT, addressSpace> ptr);
```

In the declarations above `dataT` and `numElements` are template parameters of the `vec` class template and reflect the channel type and dimensionality of a `vec` class template specialisation.

The `load()` member function template will read values of type `dataT` from the memory at the address of the `multi_ptr`, offset in elements of `dataT` by `numElements * offset` and writes those values to the channels of this `vec`.

The `store()` member function template will read channels of this `vec` and writes those values to the memory at the address of the `multi_ptr`, offset in elements of `dataT` by `numElements * offset`.

The reason for having a `multi_ptr` parameter rather than an `accessor` is so that localy created pointers can also be used as well as pointers passed from the host.

The data type of the `multi_ptr` is `dataT` the data type of the components of the vec class template specialisation. This requires thhat the pointer passed to eiither `load()` or `store()` must match the type of the `vec` instance itself.