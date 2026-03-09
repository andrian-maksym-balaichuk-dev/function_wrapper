#include <gtest/gtest.h>

#include <fw/exceptions.hpp>
#include <fw/move_only_function_wrapper.hpp>

#include <array>
#include <memory>
#include <type_traits>
#include <utility>

#include "test_types.hpp"

namespace
{
using BinaryMoveOnlyWrapper = fw::move_only_function_wrapper<int(int, int)>;
using MixedMoveOnlyWrapper = fw::move_only_function_wrapper<int(int, int), double(double, double)>;

static_assert(std::is_move_constructible_v<BinaryMoveOnlyWrapper>);
static_assert(std::is_move_assignable_v<BinaryMoveOnlyWrapper>);
static_assert(!std::is_copy_constructible_v<BinaryMoveOnlyWrapper>);
static_assert(!std::is_copy_assignable_v<BinaryMoveOnlyWrapper>);
static_assert(!std::is_constructible_v<BinaryMoveOnlyWrapper, fw::test_support::MoveOnlyAdder&>);
static_assert((std::is_same_v<typename fw::move_only_function_wrapper<int(int, int)>::policy_type, fw::policy::default_policy>));
static_assert((MixedMoveOnlyWrapper::contains_signature<int(int, int)>()));
static_assert((!MixedMoveOnlyWrapper::contains_signature<void()>()));
static_assert((fw::detail::supports_signature<fw::test_support::NothrowIncrement, int(int) noexcept>::value));

} // namespace

TEST(MoveOnlyFunctionWrapper, GivenEmptyWrapperWhenObservedThenStateQueriesStayConsistent)
{
    BinaryMoveOnlyWrapper wrapper;

    EXPECT_FALSE(wrapper.has_value());
    EXPECT_FALSE(static_cast<bool>(wrapper));
    EXPECT_EQ(wrapper.target_type(), typeid(void));
    EXPECT_EQ(wrapper.target<fw::test_support::MoveOnlyAdder>(), nullptr);
    EXPECT_TRUE(wrapper == nullptr);
    EXPECT_TRUE(nullptr == wrapper);
    EXPECT_FALSE(wrapper != nullptr);
    EXPECT_FALSE(nullptr != wrapper);
    EXPECT_THROW(static_cast<void>(wrapper(1, 2)), fw::bad_call);
}

TEST(MoveOnlyFunctionWrapper, GivenMoveOnlyCallableWhenStoredThenTargetsAndTypesArePreserved)
{
    BinaryMoveOnlyWrapper wrapper = fw::test_support::MoveOnlyAdder{ 3 };

    ASSERT_TRUE(wrapper.has_value());
    EXPECT_EQ(wrapper(2, 4), 9);
    EXPECT_NE(wrapper.target<fw::test_support::MoveOnlyAdder>(), nullptr);
    EXPECT_EQ(wrapper.target_type(), typeid(fw::test_support::MoveOnlyAdder));

    const auto& const_wrapper = wrapper;
    EXPECT_EQ(const_wrapper(1, 5), 9);
    EXPECT_NE(const_wrapper.target<fw::test_support::MoveOnlyAdder>(), nullptr);
}

TEST(MoveOnlyFunctionWrapper, GivenWrappersWhenIntrospectedThenDeclaredAndBoundSignaturesAreReported)
{
    MixedMoveOnlyWrapper empty;
    EXPECT_TRUE(MixedMoveOnlyWrapper::contains_signature<int(int, int)>());
    EXPECT_TRUE(MixedMoveOnlyWrapper::contains_signature<double(double, double)>());
    EXPECT_FALSE(MixedMoveOnlyWrapper::contains_signature<void()>());
    EXPECT_FALSE(empty.has_bound_signature<int(int, int)>());
    EXPECT_FALSE(empty.has_bound_signature<double(double, double)>());
    EXPECT_FALSE(empty.has_bound_signature<void()>());
    EXPECT_EQ(empty.bound_signatures(), (std::array<bool, 2>{ false, false }));

    MixedMoveOnlyWrapper wrapper = fw::test_support::MoveOnlyNumericTransform{ 2 };
    EXPECT_TRUE(wrapper.has_bound_signature<int(int, int)>());
    EXPECT_TRUE(wrapper.has_bound_signature<double(double, double)>());
    EXPECT_FALSE(wrapper.has_bound_signature<void()>());
    EXPECT_EQ(wrapper.bound_signatures(), (std::array<bool, 2>{ true, true }));
}

TEST(MoveOnlyFunctionWrapper, GivenTryCallWhenStateVariesThenStatusesReportEmptyAndSignatureMismatch)
{
    BinaryMoveOnlyWrapper empty;
    const auto empty_result = empty.try_call(1, 2);
    EXPECT_EQ(empty_result.status(), fw::try_call_status::Empty);
    EXPECT_FALSE(empty_result);

    fw::move_only_function_wrapper<int()> consume_once = fw::test_support::MoveOnlyConsumeOnce{ 9 };
    const auto mismatch = static_cast<const fw::move_only_function_wrapper<int()>&>(consume_once).try_call();
    EXPECT_EQ(mismatch.status(), fw::try_call_status::SignatureMismatch);
    EXPECT_FALSE(mismatch);

    auto success = std::move(consume_once).try_call();
    ASSERT_TRUE(success);
    EXPECT_EQ(success.value(), 9);
}

TEST(MoveOnlyFunctionWrapper, GivenLargeMoveOnlyCallableWhenMovedThenHeapStorageRemainsValid)
{
    BinaryMoveOnlyWrapper wrapper = fw::test_support::LargeMoveOnlyAdder{ 5 };
    ASSERT_NE(wrapper.target<fw::test_support::LargeMoveOnlyAdder>(), nullptr);
    EXPECT_EQ(wrapper(3, 4), 12);

    BinaryMoveOnlyWrapper moved = std::move(wrapper);
    EXPECT_FALSE(wrapper.has_value());
    ASSERT_NE(moved.target<fw::test_support::LargeMoveOnlyAdder>(), nullptr);
    EXPECT_EQ(moved(6, 7), 18);
}

TEST(MoveOnlyFunctionWrapper, GivenConsumeOnceCallableWhenInvokedThenOnlyRvalueDispatchSucceeds)
{
    fw::move_only_function_wrapper<int()> wrapper = fw::test_support::MoveOnlyConsumeOnce{ 11 };
    const auto& const_wrapper = wrapper;

    EXPECT_THROW(static_cast<void>(wrapper()), fw::bad_signature_call);
    EXPECT_THROW(static_cast<void>(const_wrapper()), fw::bad_signature_call);
    EXPECT_EQ(std::move(wrapper)(), 11);
}

TEST(MoveOnlyFunctionWrapper, GivenAssignmentsSwapAndResetWhenOwnershipMovesThenStateRemainsValid)
{
    BinaryMoveOnlyWrapper first = fw::test_support::MoveOnlyAdder{ 1 };
    BinaryMoveOnlyWrapper second = fw::test_support::MoveOnlyAdder{ 10 };

    swap(first, second);
    EXPECT_EQ(first(1, 2), 13);
    EXPECT_EQ(second(1, 2), 4);

    BinaryMoveOnlyWrapper empty;
    second.swap(empty);
    EXPECT_FALSE(second.has_value());
    EXPECT_EQ(empty(2, 3), 6);

    first = fw::test_support::LargeMoveOnlyAdder{ 4 };
    ASSERT_NE(first.target<fw::test_support::LargeMoveOnlyAdder>(), nullptr);
    first = std::move(first);
    EXPECT_EQ(first(2, 5), 11);

    second = std::move(first);
    EXPECT_FALSE(first.has_value());
    EXPECT_EQ(second(4, 5), 13);

    second.reset();
    EXPECT_FALSE(second.has_value());
}

TEST(MoveOnlyFunctionWrapper, GivenMixedMoveOnlyCallableWhenInvokedThenSignatureSelectionMatchesFunctionWrapper)
{
    MixedMoveOnlyWrapper wrapper = fw::test_support::MoveOnlyNumericTransform{ 2 };

    static_assert(std::is_same_v<decltype(wrapper(1, 2)), int>);
    EXPECT_EQ(wrapper(1, 2), 5);
    EXPECT_DOUBLE_EQ(wrapper(1.5, 2.0), 5.0);
}

TEST(MoveOnlyFunctionWrapper, GivenMoveOnlyFunctionArrayWhenBuiltThenElementsOwnDistinctCallables)
{
    auto wrappers = fw::make_move_only_function_array(
        [bias = std::make_unique<int>(3)](int value) { return value + *bias; },
        [scale = std::make_unique<double>(2.0)](double value) { return value * *scale; });

    using wrapper_array_type = decltype(wrappers);
    using wrapper_element_type = typename wrapper_array_type::value_type;

    static_assert(std::tuple_size_v<wrapper_array_type> == 2);
    static_assert(std::is_same_v<typename wrapper_element_type::policy_type, fw::policy::default_policy>);

    EXPECT_EQ(wrappers[0](4), 7);
    EXPECT_DOUBLE_EQ(wrappers[1](1.5), 3.0);
    EXPECT_THROW(static_cast<void>(wrappers[0](1.5)), fw::bad_signature_call);
    EXPECT_THROW(static_cast<void>(wrappers[1](4)), fw::bad_signature_call);
}

TEST(MoveOnlyFunctionWrapper, GivenNoexceptSignatureWhenStoredThenMoveOnlyTargetsCanStillDispatch)
{
    fw::move_only_function_wrapper<int(int) noexcept> wrapper =
        [bias = std::make_unique<int>(4)](int value) noexcept { return value + *bias; };

    EXPECT_EQ(wrapper(3), 7);
}

TEST(MoveOnlyFunctionWrapper, GivenMixedNoexceptSignaturesWhenInvokedThenSelectionMatchesDeclaredKinds)
{
    fw::move_only_function_wrapper<int(int) noexcept, double(double)> wrapper =
        [bias = std::make_unique<int>(2)](auto value) noexcept(noexcept(value + value)) {
            using value_type = decltype(value);
            if constexpr (std::is_same_v<value_type, int>)
            {
                return value + *bias;
            }
            else
            {
                return value * 2.0;
            }
        };

    EXPECT_EQ(wrapper(5), 7);
    EXPECT_DOUBLE_EQ(wrapper(1.5), 3.0);
}
