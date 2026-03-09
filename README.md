# function_wrapper

**A header-only C++ library for type-erased callables with multiple declared signatures.**

`fw::function_wrapper` stores one callable and exposes it through any number of declared call signatures — with deterministic, library-defined dispatch. Declared signatures may be either `R(Args...)` or `R(Args...) noexcept`. Owning wrappers use `fw::policy::default_policy` when no policy is specified, and still accept an explicit leading policy such as `fw::policy::sbo<48>` when you want custom SBO sizing. `fw::move_only_function_wrapper` provides the same dispatch model for move-only callables, and `fw::function_ref` adds a zero-allocation non-owning callable view.

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

| Need | `std::function` | `fw` |
|---|---|---|
| Single call signature | yes | yes |
| Multiple call signatures | no | yes |
| Copyable ownership | yes | `fw::function_wrapper` |
| Move-only ownership | no | `fw::move_only_function_wrapper` |
| Non-owning callable view | no | `fw::function_ref` |
| Small-buffer optimization | implementation-defined | yes, tunable |
| Deterministic multi-sig dispatch | n/a | yes |
| Explicit conversion boundary | n/a | yes |
| Null-check / empty state | yes | yes |
| `noexcept` signature declarations | no | yes |
| Target introspection | yes | yes |
| Header-only | no | yes |
| CMake package | no | yes |
| Conan package | no | yes |

**Typical use cases:**

- Numeric APIs that expose both integral and floating-point signatures from one handler.
- String APIs that accept `std::string`, `std::string_view`, and C-string literals through declared overloads.
- Callback registries that need a stable, copyable callable abstraction with controlled conversion behavior.
- Async or resource-owning callbacks that capture `std::unique_ptr`, sockets, file handles, or coroutine state.
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
- **Multi-signature dispatch** — declare as many `R(Args...)` or `R(Args...) noexcept` signatures as needed; the correct one is selected at compile time.
- **Deterministic ranking policy** — exact match wins; then reference/cv binding; then arithmetic promotions and conversions; then string-like conversions; then class hierarchy; then single-step user-defined implicit conversions. Explicit conversions are never used automatically.
- **Strict `noexcept` binding** — a `noexcept` signature only accepts nothrow-invocable targets, and `R(Args...)` / `R(Args...) noexcept` may not coexist for the same argument list.
- **Small-buffer optimization** — small callables (lambdas, function pointers, small functors) are stored inline with no heap allocation.
- **Three callable roles** — `function_wrapper` is copyable, `move_only_function_wrapper` stores move-only callables, and `function_ref` is a non-owning view.
- **Per-wrapper SBO policy** — omit the policy to use `fw::policy::default_policy`, or pass one explicitly such as `fw::policy::sbo<48>`.
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
#include <fw/function_ref.hpp>                 // function_ref
#include <fw/member_adapter.hpp>               // member_ref
#include <fw/function_wrapper.hpp>            // function_wrapper, make_function_array
#include <fw/move_only_function_wrapper.hpp>  // move_only_function_wrapper, make_move_only_function_array
#include <fw/exceptions.hpp>                  // bad_call, bad_signature_call
```

### `fw::function_ref<R(Args...)>`

```cpp
fw::function_ref<int(int)> ref = some_lvalue_callable;
int result = ref(42);
```

`function_ref` is a non-owning view. It does not extend lifetimes and rejects temporary callable objects.

`fw::function_ref<R(Args...) noexcept>` is also supported for nothrow-callable lvalues and member adapters.

### `fw::function_wrapper<Sigs...>` or `fw::function_wrapper<Policy, Sigs...>`

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
bool declares = decltype(w)::contains_signature<int(int)>();
bool bound = w.has_bound_signature<int(int)>();
auto slots = w.bound_signatures(); // std::array<bool, N> in declared-signature order
```

Declared signatures may be either `R(Args...)` or `R(Args...) noexcept`. For the same base signature, you must choose one or the other.

### `fw::move_only_function_wrapper<Sigs...>` or `fw::move_only_function_wrapper<Policy, Sigs...>`

```cpp
#include <memory>

fw::move_only_function_wrapper<int(int)> w =
    [state = std::make_unique<int>(4)](int x) { return x + *state; };

int result = w(8);
```

`fw::static_function` and `fw::static_function_ref` also accept `noexcept` function signatures:

```cpp
#include <fw/static_function.hpp>

int add_noexcept(int a, int b) noexcept { return a + b; }

constexpr auto sf = fw::static_function<int(int, int) noexcept>::make<add_noexcept>();
constexpr auto sr = fw::static_function_ref<int(int, int) noexcept>::make<add_noexcept>();

static_assert(decltype(sf)::contains_signature<int(int, int) noexcept>());
static_assert(sf.has_bound_signature<int(int, int) noexcept>());
```

`try_call(...)` is available on `function_wrapper`, `move_only_function_wrapper`, `function_ref`, `static_function`, `static_function_ref`, and the exact-signature interface bases. It returns `fw::try_call_result<R>` instead of throwing `fw::bad_call` or `fw::bad_signature_call`:

```cpp
fw::function_wrapper<int(int)> w;
auto result = w.try_call(7);

if (!result && result.status() == fw::try_call_status::Empty)
{
    // wrapper had no stored callable
}
```

`fw::member_ref(object, &Type::member)` builds a small non-owning callable adapter for member functions and member objects, so owning wrappers can bind common method cases without a hand-written lambda.

### `fw::make_function_array`

Builds a `std::array` of `function_wrapper` objects with a unified signature set deduced from all provided callables:

```cpp
auto handlers = fw::make_function_array(
    [](int x)   { return x * 2; },
    [](int x)   { return x + 10; }
);
```

### `fw::make_move_only_function_array`

Builds a `std::array` of `move_only_function_wrapper` objects with the same deduplicated-signature behavior, but accepts move-only callables.

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
- `R(Args...)` and `R(Args...) noexcept` are treated as distinct declared signatures and cannot be mixed for the same `R`/`Args...`.

## `noexcept` Notes

- A declared `noexcept` signature only accepts a target that is nothrow-invocable with that exact signature.
- `function_wrapper`, `move_only_function_wrapper`, `function_ref`, and `static_function_ref` remain nullable. Empty-state calls still throw `fw::bad_call` or `fw::bad_signature_call`, so their public call operators are not themselves `noexcept`.
- `try_call(...)` converts those library-generated empty/signature-mismatch failures into `fw::try_call_status` values. If the bound target itself throws through a non-`noexcept` signature, that exception still propagates.

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
│       ├── function_ref.hpp        # non-owning callable view
│       ├── function_wrapper.hpp   # primary public header
│       ├── move_only_function_wrapper.hpp
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
