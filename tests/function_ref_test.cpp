#include <gtest/gtest.h>

#include <fw/exceptions.hpp>
#include <fw/function_ref.hpp>

#include <type_traits>
#include <utility>

#include "test_types.hpp"

namespace
{

using UnaryFunctionRef = fw::function_ref<int(int)>;
using BinaryFunctionRef = fw::function_ref<int(int, int)>;

fw::test_support::ConstIncrement g_increment{ 2 };

static_assert(std::is_copy_constructible_v<UnaryFunctionRef>);
static_assert(std::is_copy_assignable_v<UnaryFunctionRef>);
static_assert(std::is_move_constructible_v<UnaryFunctionRef>);
static_assert(std::is_move_assignable_v<UnaryFunctionRef>);
static_assert(!std::is_constructible_v<UnaryFunctionRef, fw::test_support::RunningTotal&&>);
static_assert(!std::is_constructible_v<UnaryFunctionRef, const fw::test_support::ConstIncrement&&>);
static_assert(!std::is_assignable_v<UnaryFunctionRef&, fw::test_support::RunningTotal&&>);
static_assert(!std::is_assignable_v<UnaryFunctionRef&, const fw::test_support::ConstIncrement&&>);
static_assert(std::is_same_v<decltype(fw::function_ref{ &fw::test_support::add }), fw::function_ref<int(int, int)>>);
static_assert(std::is_same_v<decltype(fw::function_ref{ g_increment }), fw::function_ref<int(int)>>);

} // namespace

TEST(FunctionRef, GivenEmptyReferenceWhenObservedThenStateQueriesStayConsistent)
{
    BinaryFunctionRef ref;
    BinaryFunctionRef other = nullptr;

    EXPECT_FALSE(ref.has_value());
    EXPECT_FALSE(static_cast<bool>(ref));
    EXPECT_TRUE(ref == nullptr);
    EXPECT_TRUE(nullptr == ref);
    EXPECT_FALSE(ref != nullptr);
    EXPECT_FALSE(nullptr != ref);
    EXPECT_THROW(static_cast<void>(ref(1, 2)), fw::bad_call);

    ref = &fw::test_support::add;
    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(ref(2, 3), 5);

    ref.swap(other);
    EXPECT_FALSE(ref.has_value());
    EXPECT_TRUE(other.has_value());
    EXPECT_EQ(other(3, 4), 7);

    other.reset();
    EXPECT_FALSE(other.has_value());
}

TEST(FunctionRef, GivenFreeFunctionWhenBoundThenExplicitAndDeducedFormsCallItDirectly)
{
    BinaryFunctionRef explicit_ref = fw::test_support::add;
    auto deduced_ref = fw::function_ref{ &fw::test_support::add };

    EXPECT_EQ(explicit_ref(4, 5), 9);
    EXPECT_EQ(deduced_ref(6, 7), 13);
}

TEST(FunctionRef, GivenMutableCallableWhenBoundThenCallsAliasTheOriginalObject)
{
    fw::test_support::RunningTotal total;
    UnaryFunctionRef ref = total;

    EXPECT_EQ(ref(2), 2);
    EXPECT_EQ(ref(5), 7);
    EXPECT_EQ(total.total, 7);
}

TEST(FunctionRef, GivenConstCallableWhenBoundThenConstDispatchRemainsAvailable)
{
    const fw::test_support::ConstIncrement increment{ 4 };
    UnaryFunctionRef ref = increment;

    EXPECT_EQ(ref(3), 7);
}

TEST(FunctionRef, GivenWrapperLikeCallablesWhenBoundThenReferencesCanViewExistingOwners)
{
    fw::function_wrapper<int(int, int)> wrapper = fw::test_support::add;
    fw::move_only_function_wrapper<int(int, int)> move_only = fw::test_support::MoveOnlyAdder{ 1 };
    const auto static_ref = fw::static_function_ref<int(int, int)>::make<fw::test_support::add>();

    BinaryFunctionRef wrapper_ref = wrapper;
    BinaryFunctionRef move_only_ref = move_only;
    BinaryFunctionRef static_ref_view = static_ref;

    EXPECT_EQ(wrapper_ref(1, 2), 3);
    EXPECT_EQ(move_only_ref(2, 3), 6);
    EXPECT_EQ(static_ref_view(4, 5), 9);
}

TEST(FunctionRef, GivenMemberAdaptersWhenBoundThenFunctionsAndObjectsAreInvokedThroughStdInvoke)
{
    fw::test_support::MemberAdapterTarget target{ .factor = 4, .offset = 6 };
    const fw::test_support::MemberAdapterTarget const_target{ .factor = 5, .offset = 7 };

    fw::function_ref<int(int)> mutable_member_ref(target, &fw::test_support::MemberAdapterTarget::scale);
    fw::function_ref<int(int)> const_member_ref(const_target, &fw::test_support::MemberAdapterTarget::scale_const);
    fw::function_ref<int&()> member_object_ref(target, &fw::test_support::MemberAdapterTarget::offset);
    fw::function_ref<const int&()> const_member_object_ref(const_target, &fw::test_support::MemberAdapterTarget::offset);

    EXPECT_EQ(mutable_member_ref(3), 12);
    EXPECT_EQ(const_member_ref(3), 10);
    EXPECT_EQ(member_object_ref(), 6);
    member_object_ref() = 9;
    EXPECT_EQ(target.offset, 9);
    EXPECT_EQ(const_member_object_ref(), 7);
}

TEST(FunctionRef, GivenCopiedReferencesWhenInvokedThenTheyAliasTheSameTarget)
{
    fw::test_support::RunningTotal total;
    UnaryFunctionRef first = total;
    UnaryFunctionRef second = first;

    EXPECT_EQ(second(3), 3);
    EXPECT_EQ(first(4), 7);
    EXPECT_EQ(total.total, 7);
}
