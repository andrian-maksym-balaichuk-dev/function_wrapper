# fw

`fw` is a header-only C++ library centered on `fw::function_wrapper`, a
type-erased callable wrapper that can expose one stored callable through
multiple declared signatures.

The library is designed for library and application code that needs:

- value-semantic ownership of callables
- small-buffer optimization for small callable objects
- deterministic dispatch across multiple public signatures
- explicit library policy for implicit conversions
- one stable CMake target in every integration mode

The exported target is always:

```cmake
fw::wrapper
```

## Why `fw` Exists

`std::function` models one callable signature. `fw::function_wrapper` is built
for the cases where a library wants one stored callable but more than one
declared call surface.

Typical examples:

- numeric APIs that expose both integral and floating-point signatures
- string-facing APIs that accept `std::string`, `std::string_view`, and
  string literals through declared overloads
- framework code that wants a stable callable abstraction with library-defined
  conversion and dispatch behavior

## Feature Summary

- header-only library
- deterministic multi-signature dispatch
- exact-match preference before weaker conversion paths
- support for arithmetic, string-like, inheritance, pointer, and user-defined
  implicit conversions
- `const`, lvalue, and rvalue call path support
- public exception types for empty-call and missing-signature cases
- CMake package support
- Conan package recipe

## Quick Start

```cpp
#include <fw/function_wrapper.hpp>

#include <string_view>

int main() {
    fw::function_wrapper<int(int, int), float(float, float)> add =
        [](auto left, auto right) {
            return left + right;
        };

    const int sum_int = add(2, 3);
    const float sum_float = add(2, 2.5f);

    fw::function_wrapper<std::size_t(std::string_view)> text_size =
        [](std::string_view text) {
            return text.size();
        };

    const std::size_t size = text_size("fw");
    return sum_int + static_cast<int>(sum_float) + static_cast<int>(size);
}
```

Minimal consumer `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(example LANGUAGES CXX)

# Assumes fw has already been added through add_subdirectory(...),
# find_package(...), or Conan-generated package configuration.
add_executable(example main.cpp)
target_link_libraries(example PRIVATE fw::wrapper)
```

## Installation And Consumption

`fw` is intended to work cleanly in the common integration patterns used in
production C++ projects.

Supported consumption modes:

1. source integration with `add_subdirectory(...)`
2. Git submodule
3. vendored copy inside a monorepo
4. installed CMake package via `find_package(fw CONFIG REQUIRED)`
5. Conan package

The integration details are documented here:

- [Integration Guide](/Users/andrian/mycode/function_wrapper/docs/integration.md)
- [Examples](/Users/andrian/mycode/function_wrapper/docs/examples.md)
- [Development Guide](/Users/andrian/mycode/function_wrapper/docs/development.md)

## API Overview

Primary public headers:

- [include/fw/function_wrapper.hpp](/Users/andrian/mycode/function_wrapper/include/fw/function_wrapper.hpp)
- [include/fw/exceptions.hpp](/Users/andrian/mycode/function_wrapper/include/fw/exceptions.hpp)

Primary public types:

- `fw::function_wrapper<Sigs...>`
- `fw::bad_call`
- `fw::bad_signature_call`

Useful public operations on `fw::function_wrapper`:

- construction from a compatible callable
- copy and move semantics
- `operator()`
- `call(...)`
- `has_value()`
- `reset()`
- `swap(...)`
- `target_type()`
- `target<T>()`

## Dispatch Policy

When more than one declared signature could accept the call, `fw` applies a
deterministic library-defined ranking. The policy covers:

- exact matches
- reference and cv-compatible binding
- promotions and arithmetic conversions
- common numeric target preference for mixed arithmetic calls
- C-string to `std::string_view` and `std::string`
- class hierarchy and pointer hierarchy conversions
- single-step user-defined implicit conversions

Intentional boundary:

- explicit conversions are not used automatically
- chained user-defined conversions are not performed
- truly ambiguous calls remain ambiguous and fail at compile time

## Build And Test

Project requirements:

- CMake 3.20 or newer
- a C++23-capable compiler
- GoogleTest is resolved automatically for local test builds
- Conan 2.x if you package with Conan
- Clang/LLVM tools if you enable coverage

Local presets:

- `debug`: development build with tests enabled
- `release`: packaging-oriented build with tests disabled

Standard local workflow:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Release validation:

```bash
cmake --preset release
cmake --build --preset release
```

## Packaging

Create the Conan package from the repository root:

```bash
conan create . --build=missing
```

Install the CMake package locally:

```bash
cmake --install cmake-build-release --prefix /tmp/fw-install
```

The installed package exports:

- `fwConfig.cmake`
- `fwConfigVersion.cmake`
- `fwTargets.cmake`
- public headers under `include/fw`

## Repository Layout

```text
.
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── conanfile.py
├── docs/
├── fwConfig.cmake.in
├── include/
│   └── fw/
└── tests/
```

## Documentation Map

- [Integration Guide](/Users/andrian/mycode/function_wrapper/docs/integration.md)
- [Examples](/Users/andrian/mycode/function_wrapper/docs/examples.md)
- [Development Guide](/Users/andrian/mycode/function_wrapper/docs/development.md)
