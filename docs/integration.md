# Integration Guide

This document describes all supported ways to consume `fw` in a C++ project.

Every integration mode exposes exactly one CMake target:

```cmake
fw::wrapper
```

Your downstream `target_link_libraries` call is always the same regardless of how `fw` was brought into the build.

---

## Requirements

| Requirement | Version |
|---|---|
| CMake | 3.20+ |
| C++ standard | C++23 |
| Conan (optional) | 2.x |

---

## Option 1 — `add_subdirectory`

The simplest mode. Use this when `fw` lives inside your source tree as a direct subdirectory, submodule, or vendored copy.

**Project layout:**

```text
your_project/
├── CMakeLists.txt
├── src/
└── third_party/
    └── fw/          ← fw source tree here
```

**Consumer `CMakeLists.txt`:**

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_project LANGUAGES CXX)

add_subdirectory(third_party/fw)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE fw::wrapper)
```

**When to use:**

- You control the full source tree and want one configure step.
- You do not need external package management.
- You want zero installation steps in CI.

---

## Option 2 — Git submodule

Track `fw` at a pinned commit inside your repository.

```bash
git submodule add <fw-repository-url> third_party/fw
git submodule update --init --recursive
```

**Consumer `CMakeLists.txt`:**

```cmake
add_subdirectory(third_party/fw)
target_link_libraries(my_app PRIVATE fw::wrapper)
```

**When to use:**

- You want dependency versions tracked and auditable in Git history.
- You want reproducible source-based integration across machines and CI.
- Your team already uses Git submodules for third-party C++ libraries.

---

## Option 3 — Vendored copy

Copy the `fw` source into your repository manually. This is common in organizations that require a security review before ingesting third-party code.

**Project layout:**

```text
your_project/
├── CMakeLists.txt
├── src/
└── vendor/
    └── fw/
```

**Consumer `CMakeLists.txt`:**

```cmake
add_subdirectory(vendor/fw)
target_link_libraries(my_app PRIVATE fw::wrapper)
```

**When to use:**

- You want a fully self-contained repository with no network dependencies at build time.
- Dependency updates are handled as explicit, reviewed imports.
- Your organization has a formal vendoring or import policy.

---

## Option 4 — Installed CMake package

Install `fw` to a prefix directory and consume it through `find_package`. This is the standard approach for system-wide or CI-managed installations.

**Install `fw`:**

```bash
cmake --preset release
cmake --build --preset release
cmake --install cmake-build-release --prefix /opt/fw
```

**Consumer `CMakeLists.txt`:**

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_project LANGUAGES CXX)

find_package(fw CONFIG REQUIRED)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE fw::wrapper)
```

**Configure the consumer build:**

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/fw
cmake --build build
```

**Installed package contents:**

```text
<prefix>/
├── include/
│   └── fw/
│       ├── function_wrapper.hpp
│       ├── exceptions.hpp
│       └── detail/
└── lib/
    └── cmake/
        └── fw/
            ├── fwConfig.cmake
            ├── fwConfigVersion.cmake
            └── fwTargets.cmake
```

**When to use:**

- You want a clean separation between the `fw` producer build and downstream consumer builds.
- CI or release jobs install libraries into a shared prefix or Docker layer.
- Multiple projects on the same machine or in the same CI runner consume the same `fw` install.

---

## Option 5 — Conan package

Use Conan to manage `fw` as an external package with profile-based compiler and platform control.

**Create the Conan package from the `fw` repository:**

```bash
conan create . --build=missing
```

**In the consumer project**, add `fw` to your `conanfile.txt` or `conanfile.py` and run the normal Conan install step. The generated CMake integration exposes the same `fw::wrapper` target.

**When to use:**

- Your organization standardizes on Conan for C++ dependency management.
- You want profile-driven compiler, settings, and platform management.
- You want dependency resolution entirely outside the source tree.

---

## Public headers

The public include pattern is always:

```cpp
#include <fw/function_wrapper.hpp>  // function_wrapper, make_function_array
#include <fw/exceptions.hpp>        // bad_call, bad_signature_call
```

Do not include headers under `fw/detail/` directly — they are internal implementation details and may change between versions.

---

## CMake target contract

The target name `fw::wrapper` is stable across all integration modes. This means a project can switch between `add_subdirectory`, `find_package`, and Conan without changing any downstream `target_link_libraries` calls.

```cmake
# This line is always correct regardless of how fw was integrated.
target_link_libraries(my_target PRIVATE fw::wrapper)
```

This stability is intentional and maintained as a first-class compatibility guarantee.
