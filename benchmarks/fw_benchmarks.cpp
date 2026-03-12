#include <fw/function_ref.hpp>
#include <fw/function_wrapper.hpp>
#include <fw/member_adapter.hpp>
#include <fw/move_only_function_wrapper.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
using clock_type = std::chrono::steady_clock;

struct benchmark_result
{
    std::string_view name;
    std::size_t iterations;
    double ns_per_op;
    double allocations_per_op;
    double bytes_per_op;
    std::size_t peak_live_bytes;
};

volatile std::uint64_t g_sink = 0;

struct allocation_snapshot
{
    std::size_t allocations{ 0 };
    std::size_t deallocations{ 0 };
    std::size_t requested_bytes{ 0 };
    std::size_t live_bytes{ 0 };
    std::size_t peak_live_bytes{ 0 };
};

struct allocation_header
{
    void* base{ nullptr };
    std::size_t size{ 0 };
};

inline allocation_snapshot g_allocation_stats{};
inline bool g_track_allocations = false;

#if defined(__clang__) || defined(__GNUC__)
#define FW_BENCHMARK_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define FW_BENCHMARK_NOINLINE __declspec(noinline)
#else
#define FW_BENCHMARK_NOINLINE
#endif

template <class T>
inline void do_not_optimize(const T& value)
{
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(value) : "memory");
#else
    g_sink += static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(std::addressof(value)) & 1u);
#endif
}

inline void clobber_memory()
{
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : : "memory");
#endif
}

[[nodiscard]] inline std::uintptr_t align_up(std::uintptr_t value, std::size_t alignment) noexcept
{
    return (value + alignment - 1u) & ~(static_cast<std::uintptr_t>(alignment) - 1u);
}

inline void reset_allocation_stats() noexcept
{
    g_allocation_stats = {};
}

[[nodiscard]] inline allocation_snapshot current_allocation_stats() noexcept
{
    return g_allocation_stats;
}

[[nodiscard]] inline void* tracked_allocate(std::size_t size, std::size_t alignment)
{
    const std::size_t requested_size = size == 0 ? 1 : size;
    const std::size_t effective_alignment = std::max<std::size_t>(alignment, alignof(std::max_align_t));
    const std::size_t total_size = requested_size + effective_alignment - 1u + sizeof(allocation_header);

    void* raw = std::malloc(total_size);
    if (!raw)
    {
        throw std::bad_alloc{};
    }

    const auto raw_address = reinterpret_cast<std::uintptr_t>(raw) + sizeof(allocation_header);
    const auto aligned_address = align_up(raw_address, effective_alignment);
    auto* header = reinterpret_cast<allocation_header*>(aligned_address - sizeof(allocation_header));
    header->base = raw;
    header->size = requested_size;

    if (g_track_allocations)
    {
        ++g_allocation_stats.allocations;
        g_allocation_stats.requested_bytes += requested_size;
        g_allocation_stats.live_bytes += requested_size;
        g_allocation_stats.peak_live_bytes = std::max(g_allocation_stats.peak_live_bytes, g_allocation_stats.live_bytes);
    }

    return reinterpret_cast<void*>(aligned_address);
}

inline void tracked_deallocate(void* ptr) noexcept
{
    if (!ptr)
    {
        return;
    }

    auto* header = reinterpret_cast<allocation_header*>(reinterpret_cast<std::uintptr_t>(ptr) - sizeof(allocation_header));
    if (g_track_allocations)
    {
        ++g_allocation_stats.deallocations;
        g_allocation_stats.live_bytes -= header->size;
    }
    std::free(header->base);
}

inline int next_input(std::uint64_t& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    return static_cast<int>((state >> 16) & 1023u);
}

template <class Sig>
class simple_function_ref;

template <class R, class... Args>
class simple_function_ref<R(Args...)>
{
public:
    simple_function_ref() = default;

    template <class F>
    simple_function_ref(F& callable) noexcept : object_(std::addressof(callable))
    {
        thunk_ = [](void* object, Args... args) -> R {
            auto& fn = *static_cast<F*>(object);
            return fn(std::forward<Args>(args)...);
        };
    }

    R operator()(Args... args) const
    {
        return thunk_(object_, std::forward<Args>(args)...);
    }

private:
    void* object_{ nullptr };
    R (*thunk_)(void*, Args...){ nullptr };
};

struct small_copyable
{
    int bias{ 3 };

    explicit small_copyable(int value = 3) noexcept : bias(value) {}

    int operator()(int value) const noexcept
    {
        return value + bias;
    }
};

struct large_copyable
{
    std::array<int, 64> padding{};
    int bias{ 3 };

    explicit large_copyable(int value = 3) noexcept : bias(value)
    {
        padding[0] = value & 7;
    }

    int operator()(int value) const noexcept
    {
        return value + bias + padding[0];
    }
};

struct move_only_small
{
    std::unique_ptr<int> bias;

    explicit move_only_small(int value) : bias(std::make_unique<int>(value)) {}
    move_only_small(move_only_small&&) noexcept = default;
    move_only_small& operator=(move_only_small&&) noexcept = default;
    move_only_small(const move_only_small&) = delete;
    move_only_small& operator=(const move_only_small&) = delete;

    int operator()(int value) const noexcept
    {
        return value + *bias;
    }
};

struct move_only_small_noalloc
{
    int bias{ 3 };

    explicit move_only_small_noalloc(int value) noexcept : bias(value) {}
    move_only_small_noalloc(move_only_small_noalloc&&) noexcept = default;
    move_only_small_noalloc& operator=(move_only_small_noalloc&&) noexcept = default;
    move_only_small_noalloc(const move_only_small_noalloc&) = delete;
    move_only_small_noalloc& operator=(const move_only_small_noalloc&) = delete;

    int operator()(int value) const noexcept
    {
        return value + bias;
    }
};

struct move_only_large
{
    std::array<int, 64> padding{};
    std::unique_ptr<int> bias;

    explicit move_only_large(int value) : bias(std::make_unique<int>(value))
    {
        padding[0] = value & 7;
    }
    move_only_large(move_only_large&&) noexcept = default;
    move_only_large& operator=(move_only_large&&) noexcept = default;
    move_only_large(const move_only_large&) = delete;
    move_only_large& operator=(const move_only_large&) = delete;

    int operator()(int value) const noexcept
    {
        return value + *bias + padding[0];
    }
};

struct processor
{
    int scale{ 3 };

    int run(int value) const noexcept
    {
        return value * scale;
    }
};

int increment_free(int value) noexcept
{
    return value + 1;
}

template <class Callable>
FW_BENCHMARK_NOINLINE void invoke_loop(Callable& callable, std::size_t iterations)
{
    std::uint64_t local = 0;
    std::uint64_t state = 0x9e3779b97f4a7c15ULL ^ iterations;
    do_not_optimize(callable);
    for (std::size_t index = 0; index < iterations; ++index)
    {
        const auto result = callable(next_input(state) + static_cast<int>(index & 7u));
        do_not_optimize(result);
        local += static_cast<std::uint64_t>(result);
    }
    clobber_memory();
    g_sink += local + state;
}

template <class Factory>
FW_BENCHMARK_NOINLINE void construct_and_invoke_loop(Factory&& factory, std::size_t iterations)
{
    std::uint64_t local = 0;
    std::uint64_t state = 0x243f6a8885a308d3ULL ^ iterations;
    for (std::size_t index = 0; index < iterations; ++index)
    {
        const auto seed = next_input(state) + static_cast<int>(index & 7u);
        auto callable = factory(seed);
        do_not_optimize(callable);
        const auto result = callable(seed);
        do_not_optimize(result);
        local += static_cast<std::uint64_t>(result);
        clobber_memory();
    }
    g_sink += local + state;
}

template <class Runner>
benchmark_result run_benchmark(std::string_view name, Runner&& runner)
{
    constexpr auto target = std::chrono::milliseconds(1000);
    constexpr std::size_t max_iterations = std::size_t{ 1 } << 24;

    std::size_t iterations = 256;
    for (;;)
    {
        const auto start = clock_type::now();
        runner(iterations);
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - start);
        if (elapsed >= target || iterations >= max_iterations)
        {
            break;
        }
        iterations *= 2;
    }

    double best_ns_per_op = std::numeric_limits<double>::max();
    for (int sample = 0; sample < 5; ++sample)
    {
        const auto start = clock_type::now();
        runner(iterations);
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - start);
        best_ns_per_op = std::min(best_ns_per_op, static_cast<double>(elapsed.count()) / static_cast<double>(iterations));
    }

    reset_allocation_stats();
    g_track_allocations = true;
    runner(iterations);
    g_track_allocations = false;
    const auto memory = current_allocation_stats();

    return { name,
             iterations,
             best_ns_per_op,
             static_cast<double>(memory.allocations) / static_cast<double>(iterations),
             static_cast<double>(memory.requested_bytes) / static_cast<double>(iterations),
             memory.peak_live_bytes };
}

void print_section(std::string_view title, const std::vector<benchmark_result>& results)
{
    std::cout << "\n" << title << "\n";
    std::cout << std::left << std::setw(38) << "benchmark" << std::right << std::setw(14) << "iterations" << std::setw(12) << "ns/op"
              << std::setw(12) << "alloc/op" << std::setw(12) << "bytes/op" << std::setw(14) << "peak bytes" << "\n";
    std::cout << std::string(102, '-') << "\n";
    for (const auto& result : results)
    {
        std::cout << std::left << std::setw(38) << result.name << std::right << std::setw(14) << result.iterations << std::setw(14) << std::fixed
                  << std::setprecision(2) << result.ns_per_op << std::setw(12) << result.allocations_per_op << std::setw(12) << result.bytes_per_op
                  << std::setw(14) << result.peak_live_bytes << "\n";
    }
}

std::vector<benchmark_result> small_call_results()
{
    std::vector<benchmark_result> results;

    const auto direct_lambda = [](int value) noexcept { return value + 3; };
    small_copyable small{};
    int (*free_fn)(int) noexcept = &increment_free;
    std::function<int(int)> std_function = small;
    fw::function_wrapper<int(int)> fw_function = small;
    fw::move_only_function_wrapper<int(int)> fw_move_only = small_copyable{};
    fw::function_ref<int(int) noexcept> fw_ref = small;
    simple_function_ref<int(int)> simple_ref = small;

    results.push_back(run_benchmark("direct lambda", [&](std::size_t iterations) { invoke_loop(direct_lambda, iterations); }));
    results.push_back(run_benchmark("function pointer", [&](std::size_t iterations) { invoke_loop(free_fn, iterations); }));
    results.push_back(run_benchmark("std::function", [&](std::size_t iterations) { invoke_loop(std_function, iterations); }));
#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
    std::move_only_function<int(int)> std_move_only = small_copyable{};
    results.push_back(run_benchmark("std::move_only_function", [&](std::size_t iterations) { invoke_loop(std_move_only, iterations); }));
#endif
    results.push_back(run_benchmark("fw::function_wrapper", [&](std::size_t iterations) { invoke_loop(fw_function, iterations); }));
    results.push_back(run_benchmark("fw::move_only_function_wrapper", [&](std::size_t iterations) { invoke_loop(fw_move_only, iterations); }));
    results.push_back(run_benchmark("fw::function_ref", [&](std::size_t iterations) { invoke_loop(fw_ref, iterations); }));
    results.push_back(run_benchmark("simple_function_ref", [&](std::size_t iterations) { invoke_loop(simple_ref, iterations); }));

    return results;
}

std::vector<benchmark_result> large_call_results()
{
    std::vector<benchmark_result> results;

    const large_copyable large{};
    std::function<int(int)> std_function = large;
    fw::function_wrapper<int(int)> fw_function = large;
    fw::move_only_function_wrapper<int(int)> fw_move_only = large_copyable{};

    results.push_back(run_benchmark("std::function large", [&](std::size_t iterations) { invoke_loop(std_function, iterations); }));
#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
    std::move_only_function<int(int)> std_move_only = large_copyable{};
    results.push_back(run_benchmark("std::move_only_function large", [&](std::size_t iterations) { invoke_loop(std_move_only, iterations); }));
#endif
    results.push_back(run_benchmark("fw::function_wrapper large", [&](std::size_t iterations) { invoke_loop(fw_function, iterations); }));
    results.push_back(run_benchmark("fw::move_only_function_wrapper large", [&](std::size_t iterations) { invoke_loop(fw_move_only, iterations); }));

    return results;
}

std::vector<benchmark_result> construction_results()
{
    std::vector<benchmark_result> results;

    results.push_back(run_benchmark("std::function small ctor+call", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return std::function<int(int)>(small_copyable{ (seed & 15) + 1 });
            },
            iterations);
    }));

#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
    results.push_back(run_benchmark("std::move_only_function small ctor+call", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return std::move_only_function<int(int)>(small_copyable{ (seed & 15) + 1 });
            },
            iterations);
    }));
#endif

    results.push_back(run_benchmark("fw::function_wrapper small ctor+call", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return fw::function_wrapper<int(int)>(small_copyable{ (seed & 15) + 1 });
            },
            iterations);
    }));

    results.push_back(run_benchmark("fw::move_only_wrapper small ctor+call", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return fw::move_only_function_wrapper<int(int)>(small_copyable{ (seed & 15) + 1 });
            },
            iterations);
    }));

    return results;
}

std::vector<benchmark_result> move_only_results()
{
    std::vector<benchmark_result> results;

#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
    results.push_back(run_benchmark("std::move_only_function noalloc", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return std::move_only_function<int(int)>(move_only_small_noalloc{ (seed & 15) + 1 });
            },
            iterations);
    }));

    results.push_back(run_benchmark("std::move_only_function move-only ctor+call", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return std::move_only_function<int(int)>(move_only_small{ (seed & 15) + 1 });
            },
            iterations);
    }));

    results.push_back(run_benchmark("std::move_only_function large move-only", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return std::move_only_function<int(int)>(move_only_large{ (seed & 15) + 1 });
            },
            iterations);
    }));
#endif

    results.push_back(run_benchmark("fw::move_only_wrapper noalloc", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return fw::move_only_function_wrapper<int(int)>(move_only_small_noalloc{ (seed & 15) + 1 });
            },
            iterations);
    }));

    results.push_back(run_benchmark("fw::move_only_wrapper move-only ctor+call", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return fw::move_only_function_wrapper<int(int)>(move_only_small{ (seed & 15) + 1 });
            },
            iterations);
    }));

    results.push_back(run_benchmark("fw::move_only_wrapper large move-only", [&](std::size_t iterations) {
        construct_and_invoke_loop(
            [](int seed) {
                return fw::move_only_function_wrapper<int(int)>(move_only_large{ (seed & 15) + 1 });
            },
            iterations);
    }));

    return results;
}

std::vector<benchmark_result> member_adapter_results()
{
    std::vector<benchmark_result> results;

    processor proc{ 4 };
    std::function<int(int)> std_member = [&proc](int value) { return proc.run(value); };
    fw::function_wrapper<int(int)> fw_member = fw::member_ref(proc, &processor::run);

    results.push_back(run_benchmark("std::function member via lambda", [&](std::size_t iterations) { invoke_loop(std_member, iterations); }));
    results.push_back(run_benchmark("fw::function_wrapper member_ref", [&](std::size_t iterations) { invoke_loop(fw_member, iterations); }));
    results.push_back(run_benchmark("fw::member_ref direct", [&](std::size_t iterations) {
        const auto adapter = fw::member_ref(proc, &processor::run);
        invoke_loop(adapter, iterations);
    }));

    return results;
}
} // namespace

void* operator new(std::size_t size)
{
    return tracked_allocate(size, alignof(std::max_align_t));
}

void* operator new[](std::size_t size)
{
    return tracked_allocate(size, alignof(std::max_align_t));
}

void* operator new(std::size_t size, std::align_val_t alignment)
{
    return tracked_allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return tracked_allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return tracked_allocate(size, alignof(std::max_align_t));
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return tracked_allocate(size, alignof(std::max_align_t));
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try
    {
        return tracked_allocate(size, static_cast<std::size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try
    {
        return tracked_allocate(size, static_cast<std::size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(void* ptr) noexcept
{
    tracked_deallocate(ptr);
}

void operator delete[](void* ptr) noexcept
{
    tracked_deallocate(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    tracked_deallocate(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    tracked_deallocate(ptr);
}

void operator delete(void* ptr, std::align_val_t) noexcept
{
    tracked_deallocate(ptr);
}

void operator delete[](void* ptr, std::align_val_t) noexcept
{
    tracked_deallocate(ptr);
}

void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept
{
    tracked_deallocate(ptr);
}

void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept
{
    tracked_deallocate(ptr);
}

int main()
{
    std::cout << "fw benchmark suite\n";
    std::cout << "All numbers are best-of-5 steady_clock samples in ns/op.\n";
    std::cout << "Compiler/library optimize these heavily; compare relative overhead on the same machine.\n";

    print_section("small callable invocation", small_call_results());
    print_section("large callable invocation", large_call_results());
    print_section("small callable construction + call", construction_results());
    print_section("move-only construction + call", move_only_results());
    print_section("member adapter invocation", member_adapter_results());

#if !(defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L)
    std::cout << "\nstd::move_only_function is unavailable in this standard library, so those rows were skipped.\n";
#endif

    std::cout << "\nbenchmark sink: " << g_sink << "\n";
    return 0;
}
