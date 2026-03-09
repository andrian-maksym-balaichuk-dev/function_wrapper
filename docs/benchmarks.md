# Benchmark Guide

This repository ships a local microbenchmark executable for comparing `fw` against standard-library callable wrappers and a minimal `function_ref`-style view.

## Coverage

The benchmark suite currently measures:

- small callable invocation
- large callable invocation
- small callable construction plus one call
- move-only callable construction plus one call
- member binding via lambda vs `fw::member_ref`

Compared call surfaces include:

- direct lambda / function pointer baselines
- `std::function`
- `std::move_only_function` when available in the active standard library
- `fw::function_wrapper`
- `fw::move_only_function_wrapper`
- `fw::function_ref`
- a minimal local `simple_function_ref` competitor used as a low-level borrowing baseline

## Build

```bash
cmake --preset debug
cmake --build --preset debug --target fw_benchmarks
```

The benchmark target is built with optimization enabled (`-O3` or `/O2`) even in the debug preset, so you can run it from the normal local workflow without switching build trees.

## Run

```bash
./cmake-build-debug/fw_benchmarks
```

Or use the convenience target:

```bash
cmake --build --preset debug --target fw_run_benchmarks
```

## Reading the output

Each table prints:

- benchmark name
- calibrated iteration count
- best-of-5 `steady_clock` samples in nanoseconds per operation

These are local microbenchmarks, not a portability claim. Compare rows on the same machine, compiler, standard library, and optimization level.

## Sample Results

The following numbers were recorded locally on March 9, 2026 from:

```bash
./cmake-build-debug/fw_benchmarks
```

They are included as one reference point only. Treat them as machine-specific.

### Small callable invocation

| Benchmark | ns/op |
| --- | ---: |
| direct lambda | 0.98 |
| function pointer | 1.06 |
| `std::function` | 1.04 |
| `fw::function_wrapper` | 1.24 |
| `fw::move_only_function_wrapper` | 1.24 |
| `fw::function_ref` | 1.06 |
| `simple_function_ref` | 1.06 |

### Large callable invocation

| Benchmark | ns/op |
| --- | ---: |
| `std::function` large | 1.08 |
| `fw::function_wrapper` large | 1.34 |
| `fw::move_only_function_wrapper` large | 1.31 |

### Small callable construction + call

| Benchmark | ns/op |
| --- | ---: |
| `std::function` small ctor+call | 1.49 |
| `fw::function_wrapper` small ctor+call | 2.23 |
| `fw::move_only_wrapper` small ctor+call | 2.30 |

### Move-only construction + call

| Benchmark | ns/op |
| --- | ---: |
| `fw::move_only_wrapper` move-only ctor+call | 19.30 |
| `fw::move_only_wrapper` large move-only | 38.62 |

### Member adapter invocation

| Benchmark | ns/op |
| --- | ---: |
| `std::function` member via lambda | 1.04 |
| `fw::function_wrapper` member_ref | 1.50 |
| direct `fw::member_ref` | 1.02 |

## Notes

- `std::move_only_function` rows are skipped automatically if the current standard library does not provide it.
- `std::move_only_function` rows were skipped in the sample run above because the local standard library did not provide it.
- The suite is informative only. It is not a CI pass/fail gate.
- The benchmark executable lives in [fw_benchmarks.cpp](/Users/andrian/mycode/function_wrapper/benchmarks/fw_benchmarks.cpp).
