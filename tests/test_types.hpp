#pragma once

#include <fw/function_wrapper.hpp>
#include <fw/move_only_function_wrapper.hpp>
#include <fw/static_function.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace fw::test_support
{

constexpr int add(int left, int right)
{
    return left + right;
}

constexpr double multiply(double left, double right)
{
    return left * right;
}

struct LargeAdder
{
    std::array<int, 64> padding{};

    int operator()(int left, int right) const
    {
        return left + right + padding[0];
    }
};

struct ConsumeOnce
{
    int value{ 7 };

    int operator()() &&
    {
        return value;
    }
};

struct ReferenceSource
{
    int value{ 4 };

    int& operator()()
    {
        return value;
    }
};

struct JoinText
{
    std::string operator()(std::string left, std::string right) const
    {
        return left + right;
    }
};

struct TextLength
{
    std::size_t operator()(std::string text) const
    {
        return text.size();
    }

    std::size_t operator()(std::string_view text) const
    {
        return text.size() + 100;
    }
};

struct WideTextLength
{
    std::size_t operator()(std::wstring text) const
    {
        return text.size();
    }

    std::size_t operator()(std::wstring_view text) const
    {
        return text.size() + 200;
    }
};

struct Animal
{
    virtual ~Animal() = default;
};

struct Dog : Animal
{
    operator int() const
    {
        return 12;
    }
};

struct AnimalMetric
{
    int operator()(const Animal&) const
    {
        return 1;
    }

    int operator()(int value) const
    {
        return value;
    }
};

struct Label
{
    std::string value;

    Label(std::string_view text) : value(text) {}
};

struct NameView
{
    std::string value{ "converted" };

    operator std::string_view() const
    {
        return value;
    }
};

struct LvalueOnlyFunction
{
    int value{ 3 };

    int operator()() &
    {
        return value;
    }
};

struct InvocationCounter
{
    int* value{ nullptr };

    void operator()() const
    {
        ++*value;
    }
};

struct MoveOnlyAdder
{
    std::unique_ptr<int> bias;

    explicit MoveOnlyAdder(int value) : bias(std::make_unique<int>(value)) {}
    MoveOnlyAdder(MoveOnlyAdder&&) noexcept = default;
    MoveOnlyAdder& operator=(MoveOnlyAdder&&) noexcept = default;
    MoveOnlyAdder(const MoveOnlyAdder&) = delete;
    MoveOnlyAdder& operator=(const MoveOnlyAdder&) = delete;

    int operator()(int left, int right) const
    {
        return left + right + *bias;
    }
};

struct LargeMoveOnlyAdder
{
    std::array<int, 64> padding{};
    std::unique_ptr<int> bias;

    explicit LargeMoveOnlyAdder(int value) : bias(std::make_unique<int>(value)) {}
    LargeMoveOnlyAdder(LargeMoveOnlyAdder&&) noexcept = default;
    LargeMoveOnlyAdder& operator=(LargeMoveOnlyAdder&&) noexcept = default;
    LargeMoveOnlyAdder(const LargeMoveOnlyAdder&) = delete;
    LargeMoveOnlyAdder& operator=(const LargeMoveOnlyAdder&) = delete;

    int operator()(int left, int right) const
    {
        return left + right + padding[0] + *bias;
    }
};

struct MoveOnlyConsumeOnce
{
    std::unique_ptr<int> value;

    explicit MoveOnlyConsumeOnce(int initial = 7) : value(std::make_unique<int>(initial)) {}
    MoveOnlyConsumeOnce(MoveOnlyConsumeOnce&&) noexcept = default;
    MoveOnlyConsumeOnce& operator=(MoveOnlyConsumeOnce&&) noexcept = default;
    MoveOnlyConsumeOnce(const MoveOnlyConsumeOnce&) = delete;
    MoveOnlyConsumeOnce& operator=(const MoveOnlyConsumeOnce&) = delete;

    int operator()() &&
    {
        return *value;
    }
};

struct MoveOnlyNumericTransform
{
    std::unique_ptr<int> bias;

    explicit MoveOnlyNumericTransform(int value) : bias(std::make_unique<int>(value)) {}
    MoveOnlyNumericTransform(MoveOnlyNumericTransform&&) noexcept = default;
    MoveOnlyNumericTransform& operator=(MoveOnlyNumericTransform&&) noexcept = default;
    MoveOnlyNumericTransform(const MoveOnlyNumericTransform&) = delete;
    MoveOnlyNumericTransform& operator=(const MoveOnlyNumericTransform&) = delete;

    int operator()(int left, int right) const
    {
        return left + right + *bias;
    }

    double operator()(double left, double right) const
    {
        return left * right + static_cast<double>(*bias);
    }
};

struct RunningTotal
{
    int total{ 0 };

    int operator()(int value)
    {
        total += value;
        return total;
    }
};

struct ConstIncrement
{
    int delta{ 1 };

    int operator()(int value) const
    {
        return value + delta;
    }
};

struct MemberAdapterTarget
{
    int factor{ 3 };
    int offset{ 5 };

    int scale(int value)
    {
        return value * factor;
    }

    int scale_const(int value) const
    {
        return value + offset;
    }
};

using MixedAddWrapper = fw::function_wrapper<int(int, int), float(float, float)>;
using PromotedIntWrapper = fw::function_wrapper<int(int), long(long)>;
using TextLengthWrapper = fw::function_wrapper<std::size_t(std::string), std::size_t(std::string_view)>;
using WideTextLengthWrapper = fw::function_wrapper<std::size_t(std::wstring), std::size_t(std::wstring_view)>;
using AnimalMetricWrapper = fw::function_wrapper<int(const Animal&), int(int)>;

static_assert(std::is_same_v<decltype(std::declval<MixedAddWrapper&>()(2, 2.5f)), float>);
static_assert(std::is_same_v<decltype(std::declval<PromotedIntWrapper&>()(short{ 7 })), int>);
static_assert(std::is_same_v<decltype(std::declval<TextLengthWrapper&>()("abc")), std::size_t>);
static_assert(std::is_same_v<decltype(std::declval<WideTextLengthWrapper&>()(L"abc")), std::size_t>);
static_assert(std::is_same_v<decltype(std::declval<AnimalMetricWrapper&>()(Dog{})), int>);

} // namespace fw::test_support
