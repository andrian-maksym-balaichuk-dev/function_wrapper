#include <gtest/gtest.h>

#include <fw/exceptions.hpp>
#include <fw/function_wrapper.hpp>

#include <type_traits>
#include <utility>

#include "test_types.hpp"

namespace
{

constexpr auto k_static_add = fw::static_function<int(int, int)>::make<fw::test_support::add>();
static_assert(k_static_add.has_value());
static_assert(k_static_add(2, 3) == 5);

constexpr auto make_mixed_static_function()
{
    fw::static_function<int(int, int), double(double, double)> wrapper;
    wrapper.bind<int(int, int)>(fw::test_support::add);
    wrapper.bind<double(double, double)>(fw::test_support::multiply);
    return wrapper;
}

constexpr auto k_mixed_static = make_mixed_static_function();
static_assert(k_mixed_static(4, 5) == 9);
static_assert(k_mixed_static(2.0, 4.0) == 8.0);

constexpr auto k_static_add_ref = fw::static_function_ref<int(int, int)>::make<fw::test_support::add>();
static_assert(k_static_add_ref.has_value());
static_assert(k_static_add_ref(1, 6) == 7);

} // namespace

TEST(FunctionWrapper, GivenEmptyWrapperWhenInvokedThenBadCallIsThrown)
{
    fw::function_wrapper<int(int, int)> wrapper;
    EXPECT_FALSE(wrapper.has_value());
    EXPECT_THROW(static_cast<void>(wrapper(1, 2)), fw::bad_call);
}

TEST(FunctionWrapper, GivenEmptyWrapperWhenObservedThenStateQueriesStayConsistent)
{
    fw::function_wrapper<int(int, int)> wrapper;

    EXPECT_FALSE(wrapper.has_value());
    EXPECT_FALSE(static_cast<bool>(wrapper));
    EXPECT_EQ(wrapper.target_type(), typeid(void));
    EXPECT_EQ(wrapper.target<int (*)(int, int)>(), nullptr);
    EXPECT_EQ(wrapper.target<fw::test_support::LargeAdder>(), nullptr);
    EXPECT_TRUE(wrapper == nullptr);
    EXPECT_TRUE(nullptr == wrapper);
    EXPECT_FALSE(wrapper != nullptr);
    EXPECT_FALSE(nullptr != wrapper);

    wrapper.reset();
    EXPECT_FALSE(wrapper.has_value());
}

TEST(FunctionWrapper, GivenStoredCallablesWhenQueriedThenTargetsAndTypesArePreserved)
{
    fw::function_wrapper<int(int, int)> small = fw::test_support::add;
    EXPECT_NE(small.target<int (*)(int, int)>(), nullptr);
    EXPECT_EQ(small(3, 4), 7);
    EXPECT_EQ(small.target_type(), typeid(int (*)(int, int)));
    EXPECT_FALSE(small == nullptr);
    EXPECT_TRUE(small != nullptr);
    EXPECT_TRUE(nullptr != small);

    fw::function_wrapper<int(int, int)> large = fw::test_support::LargeAdder{};
    EXPECT_NE(large.target<fw::test_support::LargeAdder>(), nullptr);
    EXPECT_EQ(large(5, 6), 11);
    EXPECT_EQ(large.target_type(), typeid(fw::test_support::LargeAdder));

    const auto& const_small = small;
    const auto& const_large = large;
    EXPECT_NE(const_small.target<int (*)(int, int)>(), nullptr);
    EXPECT_NE(const_large.target<fw::test_support::LargeAdder>(), nullptr);
    EXPECT_EQ(const_large.target<int (*)(int, int)>(), nullptr);
}

TEST(FunctionWrapper, GivenSupportedValueCategoriesWhenInvokedThenDispatchUsesMatchingSlots)
{
    fw::function_wrapper<int(int, int)> wrapper = fw::test_support::add;
    const auto& const_wrapper = wrapper;
    EXPECT_EQ(const_wrapper(2, 3), 5);

    fw::function_wrapper<int()> consume_once = fw::test_support::ConsumeOnce{};
    EXPECT_EQ(std::move(consume_once)(), 7);

    fw::function_wrapper<int&()> reference_wrapper = fw::test_support::ReferenceSource{};
    int& value = reference_wrapper();
    value = 9;
    ASSERT_NE(reference_wrapper.target<fw::test_support::ReferenceSource>(), nullptr);
    EXPECT_EQ(reference_wrapper.target<fw::test_support::ReferenceSource>()->value, 9);
}

TEST(FunctionWrapper, GivenLiveWrappersWhenCopiedMovedAndSwappedThenStateRemainsValid)
{
    fw::function_wrapper<int(int, int)> first = fw::test_support::add;
    fw::function_wrapper<int(int, int)> second = fw::test_support::LargeAdder{};

    auto copy = first;
    EXPECT_EQ(copy(8, 9), 17);

    auto moved = std::move(copy);
    EXPECT_EQ(moved(10, 11), 21);
    EXPECT_FALSE(copy.has_value());

    swap(first, second);
    EXPECT_EQ(first(1, 2), 3);
    EXPECT_EQ(second(1, 2), 3);

    first.swap(first);
    EXPECT_EQ(first(2, 2), 4);
}

TEST(FunctionWrapper, GivenAssignmentsWhenStateChangesThenObserversAndCallsRemainCorrect)
{
    fw::function_wrapper<int(int, int)> wrapper;
    fw::function_wrapper<int(int, int)> empty;

    wrapper = fw::test_support::add;
    EXPECT_EQ(wrapper(4, 5), 9);

    wrapper = fw::test_support::LargeAdder{};
    EXPECT_NE(wrapper.target<fw::test_support::LargeAdder>(), nullptr);

    fw::function_wrapper<int(int, int)> heap_copy;
    heap_copy = wrapper;
    EXPECT_NE(heap_copy.target<fw::test_support::LargeAdder>(), nullptr);
    EXPECT_EQ(heap_copy(2, 3), 5);

    fw::function_wrapper<int(int, int)> heap_move;
    heap_move = std::move(heap_copy);
    EXPECT_NE(heap_move.target<fw::test_support::LargeAdder>(), nullptr);
    EXPECT_FALSE(heap_copy.has_value());

    heap_move = empty;
    EXPECT_FALSE(heap_move.has_value());

    wrapper = std::move(empty);
    EXPECT_FALSE(wrapper.has_value());

    wrapper = fw::test_support::add;
    wrapper = wrapper;
    EXPECT_EQ(wrapper(1, 2), 3);

    wrapper = fw::test_support::LargeAdder{};
    wrapper = std::move(wrapper);
    EXPECT_NE(wrapper.target<fw::test_support::LargeAdder>(), nullptr);

    fw::function_wrapper<int(int, int)> full = fw::test_support::add;
    full.swap(empty);
    EXPECT_FALSE(full.has_value());
    EXPECT_EQ(empty(6, 7), 13);

    empty.reset();
    EXPECT_FALSE(empty.has_value());
}

TEST(FunctionWrapper, GivenCallableArrayWhenBuiltThenUniqueSignaturesDispatchCorrectly)
{
    auto noop = [] {};
    auto wrappers = fw::make_function_array(fw::test_support::add, fw::test_support::multiply, noop);

    EXPECT_EQ(wrappers[0](1, 2), 3);
    EXPECT_DOUBLE_EQ(wrappers[1](1.5, 4.0), 6.0);
    EXPECT_NO_THROW(wrappers[2]());
    EXPECT_THROW(static_cast<void>(wrappers[0]()), fw::bad_signature_call);
    EXPECT_THROW(static_cast<void>(wrappers[2](1, 2)), fw::bad_signature_call);
}

TEST(FunctionWrapper, GivenMixedArithmeticWhenInvokedThenCommonNumericSignatureIsPreferred)
{
    fw::test_support::MixedAddWrapper wrapper = [](auto left, auto right) { return left + right; };

    EXPECT_EQ(wrapper(2, 2), 4);
    EXPECT_FLOAT_EQ(wrapper(2, 2.5f), 4.5f);
}

TEST(FunctionWrapper, GivenIntegralPromotionWhenInvokedThenPromotedSignatureIsSelected)
{
    fw::test_support::PromotedIntWrapper wrapper = [](auto value) { return value + value; };

    EXPECT_EQ(wrapper(short{ 7 }), 14);
    static_assert(std::is_same_v<decltype(wrapper(short{ 7 })), int>);
}

TEST(FunctionWrapper, GivenStringLikeInputsWhenInvokedThenImplicitConversionsRemainDeterministic)
{
    fw::function_wrapper<std::string(std::string, std::string)> join = fw::test_support::JoinText{};
    EXPECT_EQ(join("a", "b"), "ab");

    fw::test_support::TextLengthWrapper text_length = fw::test_support::TextLength{};
    EXPECT_EQ(text_length(std::string{ "abc" }), 3U);
    EXPECT_EQ(text_length(std::string_view{ "abc" }), 103U);
    EXPECT_EQ(text_length("abc"), 103U);

    fw::test_support::WideTextLengthWrapper wide_text_length = fw::test_support::WideTextLength{};
    EXPECT_EQ(wide_text_length(std::wstring{ L"abc" }), 3U);
    EXPECT_EQ(wide_text_length(std::wstring_view{ L"abc" }), 203U);
    EXPECT_EQ(wide_text_length(L"abc"), 203U);
}

TEST(FunctionWrapper, GivenUserDefinedTypesWhenInvokedThenImplicitConversionsAreSupported)
{
    fw::test_support::AnimalMetricWrapper animal_metric = fw::test_support::AnimalMetric{};
    EXPECT_EQ(animal_metric(fw::test_support::Dog{}), 1);

    fw::function_wrapper<std::size_t(fw::test_support::Label)> label_size = [](fw::test_support::Label label) {
        return label.value.size();
    };
    EXPECT_EQ(label_size(std::string_view{ "token" }), 5U);

    fw::function_wrapper<std::size_t(std::string_view)> view_size = [](std::string_view text) { return text.size(); };
    fw::test_support::NameView name_view{};
    EXPECT_EQ(view_size(name_view), name_view.value.size());
}

TEST(FunctionWrapper, GivenStaticFunctionWhenInvokedThenConstexprAndRuntimeCallsMatch)
{
    constexpr auto wrapper = fw::static_function<int(int, int)>::make<fw::test_support::add>();
    static_assert(wrapper(8, 2) == 10);
    EXPECT_TRUE(wrapper.has_value());
    EXPECT_EQ(wrapper(3, 9), 12);
    EXPECT_NE(wrapper.target<int(int, int)>(), nullptr);
}

TEST(FunctionWrapper, GivenStaticFunctionWithMultipleSignaturesWhenInvokedThenBestSignatureIsSelected)
{
    constexpr auto wrapper = [] {
        fw::static_function<int(int, int), double(double, double)> result;
        result.bind<int(int, int)>(fw::test_support::add);
        result.bind<double(double, double)>(fw::test_support::multiply);
        return result;
    }();

    static_assert(std::is_same_v<decltype(wrapper(1, 2)), int>);
    static_assert(std::is_same_v<decltype(wrapper(1.5, 2.0)), double>);
    EXPECT_EQ(wrapper(6, 7), 13);
    EXPECT_DOUBLE_EQ(wrapper(1.5, 3.0), 4.5);
}

TEST(FunctionWrapper, GivenStaticFunctionRefWhenInvokedThenPointerStyleConstexprWrapperWorks)
{
    constexpr auto wrapper = fw::static_function_ref<int(int, int)>::make<fw::test_support::add>();
    static_assert(wrapper(10, 5) == 15);
    EXPECT_TRUE(wrapper.has_value());
    EXPECT_EQ(wrapper(4, 8), 12);
}

TEST(FunctionWrapper, GivenStaticFunctionWhenConvertedThenFunctionWrapperPreservesDispatch)
{
    constexpr auto static_wrapper = [] {
        fw::static_function<int(int, int), double(double, double)> result;
        result.bind<int(int, int)>(fw::test_support::add);
        result.bind<double(double, double)>(fw::test_support::multiply);
        return result;
    }();

    const auto wrapper = static_wrapper.to_function_wrapper();
    ASSERT_TRUE(wrapper.has_value());
    EXPECT_EQ(wrapper(2, 5), 7);
    EXPECT_DOUBLE_EQ(wrapper(1.5, 4.0), 6.0);
}

TEST(FunctionWrapper, GivenStaticFunctionWhenExplicitlyConvertedThenFunctionWrapperCanStoreIt)
{
    fw::static_function<int(int, int)> static_wrapper;
    static_wrapper.bind<int(int, int)>(fw::test_support::add);

    auto wrapper = static_cast<fw::function_wrapper<int(int, int)>>(std::move(static_wrapper));
    ASSERT_TRUE(wrapper.has_value());
    EXPECT_EQ(wrapper(9, 3), 12);
}
