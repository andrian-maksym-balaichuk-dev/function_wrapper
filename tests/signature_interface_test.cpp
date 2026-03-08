#include <gtest/gtest.h>

#include <fw/detail/signature_interface.hpp>
#include <fw/function_wrapper.hpp>

#include "test_types.hpp"

namespace
{

using BinaryWrapper = fw::function_wrapper<int(int, int)>;
using BinaryInterface = fw::detail::signature_interface<BinaryWrapper, int(int, int)>;
using NullaryWrapper = fw::function_wrapper<int()>;
using NullaryInterface = fw::detail::signature_interface<NullaryWrapper, int()>;
using NoexceptUnaryWrapper = fw::function_wrapper<int(int) noexcept>;
using NoexceptUnaryInterface = fw::detail::signature_interface<NoexceptUnaryWrapper, int(int) noexcept>;

} // namespace

TEST(SignatureInterface, GivenWrapperBaseWhenCalledThenOperatorsForwardAcrossValueCategories)
{
    BinaryWrapper wrapper = fw::test_support::add;

    EXPECT_EQ(static_cast<BinaryInterface&>(wrapper)(1, 2), 3);
    EXPECT_EQ(static_cast<const BinaryInterface&>(wrapper)(2, 3), 5);
    EXPECT_EQ(static_cast<BinaryInterface&&>(wrapper)(3, 4), 7);
}

TEST(SignatureInterface, GivenEmptyWrapperWhenDispatchedThenEveryPathThrowsBadCall)
{
    BinaryWrapper wrapper;

    EXPECT_THROW(static_cast<void>(static_cast<BinaryInterface&>(wrapper)(1, 2)), fw::bad_call);
    EXPECT_THROW(static_cast<void>(static_cast<const BinaryInterface&>(wrapper)(1, 2)), fw::bad_call);
    EXPECT_THROW(static_cast<void>(static_cast<BinaryInterface&&>(wrapper)(1, 2)), fw::bad_call);
}

TEST(SignatureInterface, GivenMissingSlotsWhenConstOrRvalueCalledThenBadSignatureCallIsThrown)
{
    NullaryWrapper wrapper = fw::test_support::LvalueOnlyFunction{};

    EXPECT_EQ(static_cast<NullaryInterface&>(wrapper)(), 3);
    EXPECT_THROW(static_cast<void>(static_cast<const NullaryInterface&>(wrapper)()), fw::bad_signature_call);
    EXPECT_THROW(static_cast<void>(static_cast<NullaryInterface&&>(wrapper)()), fw::bad_signature_call);
}

TEST(SignatureInterface, GivenConstCallableWhenConstInvokedThenConstSlotSucceeds)
{
    using ConstNullaryInterface = fw::detail::signature_interface<fw::function_wrapper<int()>, int()>;

    fw::function_wrapper<int()> wrapper = [] { return 11; };
    const auto& const_wrapper = wrapper;

    EXPECT_EQ(const_wrapper(), 11);
    EXPECT_EQ((static_cast<const ConstNullaryInterface&>(wrapper)()), 11);
}

TEST(SignatureInterface, GivenVoidSignatureWhenInvokedThenConstAndRvalueSlotsDispatch)
{
    int calls = 0;
    fw::function_wrapper<void()> wrapper = fw::test_support::InvocationCounter{ &calls };

    const auto& const_wrapper = wrapper;
    const_wrapper();
    std::move(wrapper)();

    EXPECT_EQ(calls, 2);
}

TEST(SignatureInterface, GivenNoexceptDeclaredSignatureWhenCalledThenTheMatchingBaseStillDispatches)
{
    NoexceptUnaryWrapper wrapper = fw::test_support::NothrowIncrement{ 3 };

    static_assert(!noexcept(std::declval<NoexceptUnaryInterface&>()(1)));
    EXPECT_EQ(static_cast<NoexceptUnaryInterface&>(wrapper)(4), 7);
}
