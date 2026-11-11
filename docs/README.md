# Opal documentation

Opal is a C++20 GPU abstraction with one public header, `<opal/opal.h>`, and one backend selected at CMake configuration time. This documentation describes the API in this repository. The API is alpha and backend support is uneven.

Start here:

1. [Getting started](getting-started.md): dependencies, integration, first frame, and lifetime rules.
2. [Examples](../examples/README.md): complete clear-window, indexed-triangle, and compute programs.
3. [API reference](api-reference.md): every public type, enum, method, and exposed data member.
4. [Rendering recipes](recipes.md): textures, offscreen passes, multisampling, diagnostics, and ray tracing.
5. [Backend notes](backends.md): shader languages, feature limits, and troubleshooting.
6. [C ABI manual](c-abi.md): C integration, ownership, errors, examples, and the [C function reference](c-abi-reference.md).

The [public header](../include/opal/opal.h) defines exact C++ signatures and default arguments. Internal headers under `src/` are implementation details. Bundled dependencies have their own APIs and documentation.
