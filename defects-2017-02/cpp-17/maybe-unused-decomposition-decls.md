# Maybe unused attribute in a decomposition declaration

**Section:** 7.6.6 [dcl.attr.unused]
**Submitter:** Michael Wong
**Date:** 22 February 2017

In an example like

```cpp
[[maybe_unused]] auto [a, b] = std::make_pair(42, 0.23);
```

It is not guaranteed that the maybe unused attribute be applied to the variables
`a` and `b`. This may result in compiler warnings when `a` and `b` are not later
used.

Should the language describing where the maybe unused attribute applies be
extended to include decomposition declarations?
