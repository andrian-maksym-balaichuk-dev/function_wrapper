# Development Guide

This document describes the local development workflow for `fw`, including build setup, test execution, coverage, and packaging.

---

## Tooling Baseline

| Tool | Version | Required |
|---|---|---|
| CMake | 3.20+ | Always |
| C++ compiler | C++23 capable | Always |
| GoogleTest | fetched via CMake | Test builds |
| Conan | 2.x | Packaging only |
| Clang/LLVM | any | Coverage only |

GoogleTest is fetched automatically by CMake when tests are enabled. No manual installation is required.

---

## Repository Layout

```text
.
├── CMakeLists.txt          # project definition, install rules
├── CMakePresets.json       # preset definitions
├── conanfile.py            # Conan recipe
├── fwConfig.cmake.in       # CMake package config template
├── include/
│   └── fw/
│       ├── function_wrapper.hpp   # primary public header
│       ├── exceptions.hpp         # public exception types
│       └── detail/                # internal implementation (do not include directly)
├── tests/                  # GoogleTest-based test suite
└── docs/                   # documentation
```

---

## CMake Presets

The project exposes two presets:

| Preset | Purpose | Tests enabled |
|---|---|---|
| `debug` | Local development | Yes |
| `release` | Packaging and install validation | No |

---

## Standard Workflow

### Configure and build (debug)

```bash
cmake --preset debug
cmake --build --preset debug
```

### Run all tests

```bash
ctest --preset debug
```

Tests are organized into focused executables. CTest discovers and runs them all.

### Release build

```bash
cmake --preset release
cmake --build --preset release
```

---

## Test Targets

The test suite is split into focused executables. Each can be run directly from the IDE or from the command line.

| Target | Covers |
|---|---|
| `fw_concepts_tests` | Concept constraints and type trait correctness |
| `fw_exceptions_tests` | Exception message stability and throw behavior |
| `fw_function_wrapper_tests` | Core wrapper construction, copy/move, call dispatch |
| `fw_signature_interface_tests` | Signature selection and dispatch ranking policy |
| `fw_vtable_tests` | Vtable construction and lifecycle |

There is also an aggregate build target:

```bash
cmake --build --preset debug --target fw_check
```

`fw_check` builds all test targets in one step. It does not run them — use `ctest` for execution.

### Running a specific test binary

After building:

```bash
./cmake-build-debug/tests/fw_function_wrapper_tests
./cmake-build-debug/tests/fw_signature_interface_tests --gtest_filter="*Dispatch*"
```

GoogleTest filter syntax applies. Use `--gtest_list_tests` to enumerate all test names.

---

## Test Naming Convention

All tests use Given/When/Then naming:

```
GivenEmptyWrapper_WhenCalled_ThrowsBadCall
GivenIntFloatSignatures_WhenCalledWithFloat_DispatchesToFloat
```

This keeps CTest and IDE test runner output readable without additional annotation.

---

## IDE Setup (CLion)

1. Open the repository root in CLion.
2. Import the project using the shipped CMake presets (CLion detects `CMakePresets.json` automatically).
3. Select the `debug` profile for normal development.
4. Run individual test targets from the Run/Debug configurations panel.

The split test targets keep the IDE run surface clear. Prefer running `fw_function_wrapper_tests` during active feature work and `fw_check` before committing.

---

## Coverage

Coverage is optional and requires a Clang/LLVM toolchain.

**Enable at configure time:**

```bash
cmake --preset debug -DFW_ENABLE_COVERAGE=ON
```

**Build the coverage report:**

```bash
cmake --build --preset debug --target fw_wrapper_coverage
```

The HTML report is generated in `cmake-build-debug/coverage/`.

Coverage is not required for contributions but is useful when investigating untested paths.

---

## Packaging

### Install CMake package locally

```bash
cmake --preset release
cmake --build --preset release
cmake --install cmake-build-release --prefix /tmp/fw-install
```

Verify the installed layout:

```text
/tmp/fw-install/
├── include/fw/
│   ├── function_wrapper.hpp
│   ├── exceptions.hpp
│   └── detail/
└── lib/cmake/fw/
    ├── fwConfig.cmake
    ├── fwConfigVersion.cmake
    └── fwTargets.cmake
```

### Create a Conan package

```bash
conan create . --build=missing
```

Test the consumer integration by pointing a test project at the installed package or local Conan cache.

---

## Code Style

The repository ships a `.clang-format` file. Format all changed files before committing:

```bash
clang-format -i include/fw/function_wrapper.hpp
clang-format -i tests/test_function_wrapper.cpp
```

Key conventions:

- 4-space indentation, no tabs.
- Opening braces on the line following the declaration (Allman style).
- `[[nodiscard]]` on all query methods.
- No `using namespace` in headers.
- Internal implementation lives under `fw/detail/` and is never part of the public API.

---

## Maintenance Checklist

Before releasing a new version:

- [ ] All test targets pass under `ctest --preset debug`.
- [ ] `cmake --preset release && cmake --build --preset release` succeeds cleanly.
- [ ] `cmake --install` produces the expected layout.
- [ ] `conan create . --build=missing` succeeds.
- [ ] Public API changes are reflected in `README.md`, `docs/api.md`, and `docs/examples.md`.
- [ ] The exported CMake target name `fw::wrapper` has not changed.
- [ ] No machine-specific toolchain policy has been added to project CMake files.
