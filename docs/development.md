# Development Guide

This document describes the local development workflow for `fw`.

## Tooling Baseline

The repository is configured as a modern CMake project with:

- CMake presets for local workflows
- one exported CMake package target
- GoogleTest-based test coverage for the library
- Conan packaging support
- project formatting policy in `.clang-format`
- baseline editor settings in `.editorconfig`

## Local Presets

The repository keeps the preset surface intentionally small:

- `debug`
- `release`

`debug` enables tests.

`release` disables tests and is intended for packaging and install validation.

## Standard Local Workflow

Configure and build the debug tree:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Build the release tree:

```bash
cmake --preset release
cmake --build --preset release
```

## CLion Workflow

Recommended CLion usage:

- import the project with the shipped CMake presets
- use the `debug` preset for normal development
- run one of the focused test executables directly from the IDE

The project intentionally exposes several focused test targets:

- `fw_concepts_tests`
- `fw_exceptions_tests`
- `fw_function_wrapper_tests`
- `fw_signature_interface_tests`
- `fw_vtable_tests`

There is also an aggregate build target:

- `fw_check`

This keeps the IDE run/debug surface clearer than a single monolithic test
binary.

## Test Scope

The library tests cover:

- conversion ranking policy
- declared-signature selection
- empty wrapper behavior
- ownership and state transitions
- const, lvalue, and rvalue dispatch
- exception message stability
- storage and vtable lifecycle behavior

Test names use Given/When/Then form so CTest and IDE runners remain readable.

## Coverage

Coverage is optional and intended for Clang/LLVM-based development.

Enable coverage at configure time:

```bash
cmake --preset debug -DFW_ENABLE_COVERAGE=ON
cmake --build --preset debug --target fw_wrapper_coverage
```

## Packaging Workflow

Install locally:

```bash
cmake --install cmake-build-release --prefix /tmp/fw-install
```

Create the Conan package:

```bash
conan create . --build=missing
```

## Repository Structure

```text
.
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── conanfile.py
├── docs/
├── fwConfig.cmake.in
├── include/fw/
└── tests/
```

## Maintenance Notes

- keep public API changes reflected in the root README and docs examples
- keep the exported CMake target name stable as `fw::wrapper`
- avoid machine-specific toolchain policy in project CMake
- keep local machine compiler policy in user environment or Conan profiles
