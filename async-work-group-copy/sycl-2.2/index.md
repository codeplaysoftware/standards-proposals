**Document number: C009**
**Date: 2017-07-19**
**Project: SYCL 2.2 Specification**
**Authors: Gordon Brown, Ruyman Reyes**
**Emails: gordon@codeplay.com, ruyman@codeplay.com**
**Reply to: gordon@codeplay.com, ruyman@codeplay.com**

# Async Work Group Copy & Prefetch Builtins

## Motivation

The OpenCL specification provides asynchronous work group builtins and event wait builtins for performing asynchronous copy and prefetch operations between global and local memory and waiting on thos operations to complete.

These builtins are currently missing from the SYCL specification.

For example:

```cpp
static const WORK_ITEMS = 2048;
static const WORK_ITEMS_PER_GROUP = 128;
static const WORK_GROUPS = WORK_ITEMS / WORK_ITEMS_PER_GROUP;

float floatData[2048 * sizeof(float)];

buffer<float4, 1> floatBuffer(floatData, range<1>(WORK_ITEMS));

queue.submit([&](handler &cgh){

  auto globalAcc = buf.get_access<access::mode::read>(cgh);

  accessor<float4, 1, access::mode::read_write, access::target::local> localAcc(range<1>(WORK_ITEMS_PER_GROUP), cgh);

  cgh.parallel_for_work_group(range<1>(WORK_GROUPS), [=](group<1> group){
    auto globalPtr = globalAcc.get_pointer();

    group.parallel_for_work_item(range<1>(WORK_ITEMS_PER_GROUP), [&](h_item<1> item) {
      auto localPtr = localAcc.get_pointer();

      // enqueue an asynchronous copy from global to local memory
      auto copyToLocal = async_work_group_copy(localPtr, globalPtr, WORK_ITEMS_PER_GROUP);

      // wait for copy to local memeory to complete
      copyToLocal.wait();

      // perform computation on local memory
      per_work_group_func(localPtr);

      // enqueue an asynchronous copy from local to global memory
      auto copyToGlobal = async_work_group_copy(globalPtr, localPtr, WORK_ITEMS_PER_GROUP);

      // wait for copy back to global memory to complete
      copyToGlobal.wait();
    });
  });
});
```

## Summary

This proposal will add async work group copy and prefetch builtins, event wait builtins and the device_event class.

## Builtin Functions

```cpp
template <typename elementT, int kDims>
device_event async_work_group_copy(local_ptr<vec<elementT, kDims>> dest, const global_ptr<vec<elementT, kDims>> src, size_t numElements, device_event event = {});

template <typename elementT, int kDims>
device_event async_work_group_copy(global_ptr<vec<elementT, kDims>> dest, const local_ptr<vec<elementT, kDims>> src, size_t numElements, device_event event = {});

template <typename elementT, int kDims>
device_event async_work_group_copy(local_ptr<vec<elementT, kDims>> dest, const global_ptr<vec<elementT, kDims>> src, size_t numElements, size_t srcStride, device_event event = {});

template <typename elementT, int kDims>
device_event async_work_group_copy(global_ptr<vec<elementT, kDims>> dest, const local_ptr<vec<elementT, kDims>> src, size_t numElements, size_t destStride, device_event event = {});

template <typename elementT, int kDims>
void prefetch(const global_ptr<vec<elementT, kDims>> dest, size_t numElements)
```

## Device Event Class

```cpp
class device_event {
public:
  device_event();

  void wait() const;
};

template <typename... eventTN>
void work_group_wait(eventTN... events);
```