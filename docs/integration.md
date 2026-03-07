# Integration Guide

This document describes the supported ways to consume `fw` in a real project.
Every supported mode exposes the same public target:

```cmake
fw::wrapper
```

## Requirements

- CMake 3.20 or newer
- a compiler with C++23 support
- access to the `fw` source tree, installed package, or Conan package

## Option 1: Add The Repository With `add_subdirectory`

This is the simplest approach when `fw` is part of the same source tree.

Project layout:

```text
your_project/
├── CMakeLists.txt
├── src/
└── third_party/
    └── fw/
```

Consumer `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_project LANGUAGES CXX)

add_subdirectory(third_party/fw)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE fw::wrapper)
```

Use this when:

- you control the full source tree
- you want one configure step for all dependencies
- you do not need external package management

## Option 2: Add `fw` As A Git Submodule

Add the repository:

```bash
git submodule add <your-fw-repository-url> third_party/fw
git submodule update --init --recursive
```

Then consume it the same way:

```cmake
add_subdirectory(third_party/fw)
target_link_libraries(my_app PRIVATE fw::wrapper)
```

Use this when:

- you want dependency versions tracked in Git
- you want reproducible source-based integration
- your team already uses submodules for third-party C++ libraries

## Option 3: Vendor A Copy Of The Source

Some companies prefer to copy reviewed third-party code directly into the
repository rather than rely on Git submodules.

Example layout:

```text
your_project/
├── CMakeLists.txt
├── src/
└── vendor/
    └── fw/
```

Consumer `CMakeLists.txt`:

```cmake
add_subdirectory(vendor/fw)
target_link_libraries(my_app PRIVATE fw::wrapper)
```

Use this when:

- you want fully self-contained source control
- dependency updates are handled by manual import
- your organization has a vendoring policy

## Option 4: Consume An Installed CMake Package

Install `fw` first:

```bash
cmake --preset release
cmake --build --preset release
cmake --install cmake-build-release --prefix /opt/fw
```

Consumer `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_project LANGUAGES CXX)

find_package(fw CONFIG REQUIRED)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE fw::wrapper)
```

Configure the consumer:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/fw
cmake --build build
```

Use this when:

- you want package-style separation between producer and consumer
- CI or release jobs install libraries into a shared prefix
- you want downstream projects to consume a stable exported target

## Option 5: Consume Through Conan

Create the package:

```bash
conan create . --build=missing
```

In the consumer project, use Conan in the normal way for your environment.
The package still exposes the same CMake package contract and target name.

Use this when:

- your organization standardizes on Conan
- you want dependency resolution outside the source tree
- you want profile-based compiler and platform management

## Consumer Include Pattern

The public include form is:

```cpp
#include <fw/function_wrapper.hpp>
```

If only the public exceptions are needed:

```cpp
#include <fw/exceptions.hpp>
```

## Public CMake Contract

The integration mode should not change how downstream code links the library.
The stable contract is:

```cmake
target_link_libraries(my_target PRIVATE fw::wrapper)
```

That consistency is intentional. It keeps local source builds, installed
packages, and Conan-based builds aligned.

