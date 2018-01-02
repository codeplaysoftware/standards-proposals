| Proposal ID | CP003 |
|-------------|--------|
| Name | Buffer Tags |
| Date of Creation | 17 March 2017 |
| Target | SYCL 1.2.1 |
| Current Status | *Accepted with changes* |
| Reply-to | Gordon Brown <gordon@codeplay.com> |
| Original author | Gordon Brown <gordon@codeplay.com> |
| Contributors | Ruyman Reyes <ruyman@codeplay.com>, Gordon Brown <gordon@codeplay.com> |

# Implicit Accessor Conversions

## Motivation

In the current SYCL 2.2 provisional specification there is a requirement that buffers and accessors maintain strong type safety. However there are many cases where it would be very benefitial for users to alter the type of an accessor whilst maintaining the same underlying data. This enables users to create a generic buffer that can be later converted to other types, includig vector types.

## Proposal

In order to remove the need to manually convert the elements of an accessor from one type to another type within a SYCL kernel function, this proposal aims to provide developers with the ability to use a different type for the data at the accessor creation than the one used during constrction. This is achieved by providing implicit type conversions to the elements being accessed:

```cpp
using byte_t = unsigned char;

// memory allocation of 1024 bytes
byte_t ptr[1024];

// 1d buffer of 1024 bytes managing [ptr, ptr + 1024)
buffer<byte_t, 1> byteBuffer(ptr, range<1>(1024));

queue.submit([&](handler &cgh){

  // accessor to 256 elements of float
  auto acc = buf.get_access<float, access::mode::read_write>(cgh);

  // access float elements within SYCL kernel function
  cgh.parallel_for(acc.get_range(), [=](id<1> idx){
    acc[idx] = ...
  });

});
```

In this example `byteBuffer` is being accessed in elements of float type size, instead of byte type-size. The SYCL runtime implicitly casts the type and number of elements before passing the accessor to the SYCL kernel function.

```cpp
// memory allocation of 256 floats
float ptr[256];

// 1d buffer of 256 floats managing [ptr, ptr + 256)
buffer<float, 1> floatBuffer(ptr, range<1>(256));

queue.submit([&](handler &cgh){

  // accessor to 64 elements of float4
  auto acc = buf.get_access<float4, access::mode::read_write>(cgh);

  // access a 1d range of float4 elements within SYCL kernel function
  cgh.parallel_for(acc.get_range(), [=](id<1> idx){
    acc[idx] = ...
  });

});
```

In this example `floatBuffer` is being in elements of float4 type instead of scalar float elements. The SYCL runtime implicitly casts the type and number of elements before passing the accessor to the SYCL kernel function. In this example the dimensionality of the accessor is also being altered, this again can be implicitly offset by the accessor.

## Variations

We propose that we introduce these implicit conversions from buffers of type `byte_t` to accessors of any valid type, essentially allowing users to create non-typed bufferd, and  from buffers of a scalar type to the equivelant vector type, for example `float` to `float4`.

## Considerations

Currently this proposal is only for 1 dimensional buffers.

We also propose that the `accessor` class template have the member function `get_range()` which returns the range of the accessor. This is improtant for this feature as the accessor can now have a different range from that of the buffer it accesses.
