# function_wrapper

**A header-only C++ library for type-erased callables with multiple declared signatures.**

`fw::function_wrapper` stores one callable and exposes it through any number of declared call signatures — with deterministic, library-defined dispatch. It is a drop-in upgrade for `std::function` in contexts where one signature is not enough.

---

## The Problem With `std::function`

`std::function<R(Args...)>` models exactly one call signature. When a library needs to accept both `int` and `float` arguments, or both `std::string` and `std::string_view`, the usual options are:

- Write separate overloaded functions — breaks composability, doubles storage.
- Use a template parameter — impossible to store in a container or pass as a value.
- Use `std::variant` of multiple `std::function` objects — verbose, awkward to call.

None of these give you one callable value with multiple declared call surfaces.

## The Solution

```cpp
// One wrapper, two signatures, one stored lambda.
fw::function_wrapper<int(int, int), float(float, float)> add =
    [](auto a, auto b) { return a + b; };

int   r1 = add(1, 2);     // dispatches to int(int, int)
float r2 = add(1.f, 2.f); // dispatches to float(float, float)
```

`fw::function_wrapper` selects the correct declared signature at compile time using a deterministic ranking policy. The stored callable is invoked through the chosen signature — no runtime branching, no ambiguity.

---

## Why Use function_wrapper

| Need | `std::function` | `fw::function_wrapper` |
|---|---|---|
| Single call signature | yes | yes |
| Multiple call signatures | no | yes |
| Value semantics (copy/move) | yes | yes |
| Small-buffer optimization | implementation-defined | yes, tunable |
| Deterministic multi-sig dispatch | n/a | yes |
| Explicit conversion boundary | n/a | yes |
| Null-check / empty state | yes | yes |
| Target introspection | yes | yes |
| Header-only | no | yes |
| CMake package | no | yes |
| Conan package | no | yes |

**Typical use cases:**

- Numeric APIs that expose both integral and floating-point signatures from one handler.
- String APIs that accept `std::string`, `std::string_view`, and C-string literals through declared overloads.
- Callback registries that need a stable, copyable callable abstraction with controlled conversion behavior.
- Framework code where implicit conversion policy must be explicit and library-owned, not left to the caller.

---

## Quick Start

```cpp
#include <fw/function_wrapper.hpp>
#include <string_view>

int main()
{
    // Multi-signature numeric wrapper
    fw::function_wrapper<int(int, int), float(float, float)> add =
        [](auto a, auto b) { return a + b; };

    const int   sum_i = add(2, 3);
    const float sum_f = add(2.0f, 2.5f);

    // Single-signature string wrapper
    fw::function_wrapper<std::size_t(std::string_view)> size_of =
        [](std::string_view s) { return s.size(); };

    const std::size_t n = size_of("function_wrapper");

    return static_cast<int>(sum_i + sum_f + static_cast<float>(n));
}
```

**CMake consumer:**

```cmake
cmake_minimum_required(VERSION 3.20)
project(example LANGUAGES CXX)

# fw can be added via add_subdirectory, find_package, or Conan —
# the target name is always the same.
add_executable(example main.cpp)
target_link_libraries(example PRIVATE fw::wrapper)
```

---

## Features

- **Header-only** — one include path, no compiled library to link.
- **Multi-signature dispatch** — declare as many `R(Args...)` signatures as needed; the correct one is selected at compile time.
- **Deterministic ranking policy** — exact match wins; then reference/cv binding; then arithmetic promotions and conversions; then string-like conversions; then class hierarchy; then single-step user-defined implicit conversions. Explicit conversions are never used automatically.
- **Small-buffer optimization** — small callables (lambdas, function pointers, small functors) are stored inline with no heap allocation.
- **Value semantics** — copy, move, assign, swap, and reset all work as expected.
- **Null / empty state** — default-constructed wrapper is empty. `has_value()`, `operator bool`, and comparison with `nullptr` are all provided.
- **Target introspection** — `target_type()` and `target<T>()` mirror `std::function`.
- **Exception types** — `fw::bad_call` (empty wrapper called) and `fw::bad_signature_call` (no matching signature) are public and catchable.
- **CMake package** — exports `fw::wrapper` in all integration modes.
- **Conan recipe** — included in the repository.

---

## Installation

### Option 1 — `add_subdirectory` (simplest)

```cmake
add_subdirectory(third_party/fw)
target_link_libraries(my_target PRIVATE fw::wrapper)
```

### Option 2 — Git submodule

```bash
git submodule add <fw-repository-url> third_party/fw
```

```cmake
add_subdirectory(third_party/fw)
target_link_libraries(my_target PRIVATE fw::wrapper)
```

### Option 3 — Installed CMake package

```bash
cmake --preset release
cmake --build --preset release
cmake --install cmake-build-release --prefix /opt/fw
```

```cmake
find_package(fw CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE fw::wrapper)
```

### Option 4 — Conan

```bash
conan create . --build=missing
```

The Conan package exports the same `fw::wrapper` CMake target.

See [Integration Guide](docs/integration.md) for full details on each mode.

---

## API Overview

### Primary headers

```cpp
#include <fw/function_wrapper.hpp>  // function_wrapper, make_function_array
#include <fw/exceptions.hpp>        // bad_call, bad_signature_call
```

### `fw::function_wrapper<Sigs...>`

```cpp
// Construction
fw::function_wrapper<int(int)> w = some_callable;

// Invocation
int result = w(42);          // operator()
int result = w.call(42);     // explicit call member

// State
bool live  = w.has_value();  // true if a callable is stored
w.reset();                   // clear the stored callable
w.swap(other);               // swap with another wrapper

// Introspection
const std::type_info& ti = w.target_type();
int (*fn)(int) = *w.target<int(*)(int)>();
```

### `fw::make_function_array`

Builds a `std::array` of `function_wrapper` objects with a unified signature set deduced from all provided callables:

```cpp
auto handlers = fw::make_function_array(
    [](int x)   { return x * 2; },
    [](int x)   { return x + 10; }
);
```

### Exception types

| Type | Thrown when |
|---|---|
| `fw::bad_call` | An empty wrapper is called |
| `fw::bad_signature_call` | A runtime dispatch finds no matching vtable entry |

---

## Dispatch Policy

When a call could match more than one declared signature, `fw` applies a fixed priority ranking:

1. Exact match (identical types after decay)
2. Reference / cv-qualification binding
3. Integral and floating-point promotions
4. Arithmetic conversions (common numeric target preference for mixed-type calls)
5. C-string to `std::string_view` / `std::string`
6. Class hierarchy (derived-to-base) and pointer hierarchy
7. Single-step user-defined implicit conversion

**Hard boundaries:**

- Explicit conversions are never used implicitly.
- Chained user-defined conversions are not performed.
- Calls that are genuinely ambiguous across declared signatures fail at compile time.

---

## Build and Test

**Requirements:**

- CMake 3.20+
- C++23-capable compiler (GCC 13+, Clang 16+, MSVC 19.35+)
- GoogleTest — fetched automatically for test builds
- Conan 2.x — only needed for Conan packaging
- Clang/LLVM tools — only needed for coverage reports

**Presets:**

| Preset | Purpose |
|---|---|
| `debug` | Development build, tests enabled |
| `release` | Packaging build, tests disabled |

```bash
# Build and run all tests
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Release / install build
cmake --preset release
cmake --build --preset release
cmake --install cmake-build-release --prefix /tmp/fw-install
```

See [Development Guide](docs/development.md) for the full local workflow, IDE setup, coverage, and packaging steps.

---

## Documentation

| Document | Description |
|---|---|
| [Integration Guide](docs/integration.md) | All supported integration modes with CMake and Conan |
| [Examples](docs/examples.md) | Annotated code examples covering the main use cases |
| [API Reference](docs/api.md) | Full public API reference |
| [Development Guide](docs/development.md) | Local workflow, test targets, coverage, packaging |
| [Contributing](CONTRIBUTING.md) | How to contribute: workflow, conventions, review process |

---

## Repository Layout

```text
.
├── CMakeLists.txt          # project definition and package install rules
├── CMakePresets.json       # debug and release presets
├── conanfile.py            # Conan recipe
├── fwConfig.cmake.in       # CMake package config template
├── include/
│   └── fw/
│       ├── function_wrapper.hpp   # primary public header
│       ├── exceptions.hpp         # public exception types
│       └── detail/                # internal implementation headers
├── tests/                  # GoogleTest-based test suite
├── docs/                   # documentation
└── README.md
```

---

## Requirements Summary

| Requirement | Version |
|---|---|
| CMake | 3.20+ |
| C++ standard | C++23 |
| Compiler (GCC) | 13+ |
| Compiler (Clang) | 16+ |
| Compiler (MSVC) | 19.35+ |
| Conan (optional) | 2.x |

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

---

## License

See [LICENSE](LICENSE) for terms.
