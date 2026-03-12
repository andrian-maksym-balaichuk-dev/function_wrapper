# Benchmark Guide

This repository ships a local microbenchmark executable for comparing `fw` against standard-library callable wrappers and a minimal `function_ref`-style view.

## Coverage

The benchmark suite currently measures:

- small callable invocation
- large callable invocation
- small callable construction plus one call
- move-only callable construction plus one call
- move-only construction plus one call without callable-owned allocation
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
- allocations per operation from a separate tracked pass
- requested bytes per operation from the same tracked pass
- peak live bytes observed during the tracked pass

These are local microbenchmarks, not a portability claim. Compare rows on the same machine, compiler, standard library, and optimization level.

## Sample Results

The following numbers were recorded locally on March 12, 2026 from:

```bash
./cmake-build-debug/fw_benchmarks
```

They are included as one reference point only. Treat them as machine-specific.

### Small callable invocation

| Benchmark | ns/op | alloc/op | bytes/op | peak bytes |
| --- | ---: | ---: | ---: | ---: |
| direct lambda | 0.95 | 0.00 | 0.00 | 0 |
| `std::function` | 1.01 | 0.00 | 0.00 | 0 |
| `fw::function_wrapper` | 1.03 | 0.00 | 0.00 | 0 |
| `fw::move_only_function_wrapper` | 1.03 | 0.00 | 0.00 | 0 |
| `fw::function_ref` | 1.03 | 0.00 | 0.00 | 0 |

### Large callable invocation

| Benchmark | ns/op | alloc/op | bytes/op | peak bytes |
| --- | ---: | ---: | ---: | ---: |
| `std::function` large | 1.05 | 0.00 | 0.00 | 0 |
| `fw::function_wrapper` large | 1.05 | 0.00 | 0.00 | 0 |
| `fw::move_only_function_wrapper` large | 1.05 | 0.00 | 0.00 | 0 |

### Small callable construction + call

| Benchmark | ns/op | alloc/op | bytes/op | peak bytes |
| --- | ---: | ---: | ---: | ---: |
| `std::function` small ctor+call | 1.47 | 0.00 | 0.00 | 0 |
| `fw::function_wrapper` small ctor+call | 1.21 | 0.00 | 0.00 | 0 |
| `fw::move_only_wrapper` small ctor+call | 1.10 | 0.00 | 0.00 | 0 |

### Move-only construction + call

| Benchmark | ns/op | alloc/op | bytes/op | peak bytes |
| --- | ---: | ---: | ---: | ---: |
| `fw::move_only_wrapper` noalloc | 1.11 | 0.00 | 0.00 | 0 |
| `fw::move_only_wrapper` move-only ctor+call | 19.13 | 1.00 | 4.00 | 4 |
| `fw::move_only_wrapper` large move-only | 36.38 | 2.00 | 268.00 | 268 |

### Member adapter invocation

| Benchmark | ns/op | alloc/op | bytes/op | peak bytes |
| --- | ---: | ---: | ---: | ---: |
| `std::function` member via lambda | 1.05 | 0.00 | 0.00 | 0 |
| `fw::function_wrapper` member_ref | 1.04 | 0.00 | 0.00 | 0 |
| direct `fw::member_ref` | 1.02 | 0.00 | 0.00 | 0 |

## Notes

- `std::move_only_function` rows are skipped automatically if the current standard library does not provide it.
- `std::move_only_function` rows were skipped in the sample run above because the local standard library did not provide it.
- `fw::function_wrapper member_ref` uses a dedicated internal member-adapter dispatch path in this build; it now matches `std::function`'s lambda-based binding within measurement noise.
- `fw::move_only_wrapper noalloc` is the cleaner wrapper-overhead row for move-only ownership. The `move-only ctor+call` and `large move-only` rows above also include the cost of constructing callables that allocate `std::unique_ptr` storage on every iteration.
- Allocation tracking is measured in a separate pass after timing calibration, but the benchmark binary still includes global allocation hooks. Treat the absolute `ns/op` values as relative to this benchmark build, not as canonical library-wide timings.
- The suite is informative only. It is not a CI pass/fail gate.
- The benchmark executable lives in [fw_benchmarks.cpp](/Users/andrian/mycode/function_wrapper/benchmarks/fw_benchmarks.cpp).
