#include <gtest/gtest.h>

#include <fw/exceptions.hpp>

#include <cstring>

TEST(Exceptions, GivenPublicExceptionsWhenWhatIsCalledThenStableMessagesAreReturned)
{
    fw::bad_call bad_call;
    fw::bad_signature_call bad_signature_call;

    EXPECT_STREQ(bad_call.what(), "fw::bad_call: called an empty function_wrapper");
    EXPECT_STREQ(bad_signature_call.what(), "fw::bad_signature_call: stored callable does not match this signature");
    EXPECT_GT(std::strlen(bad_call.what()), 0U);
    EXPECT_GT(std::strlen(bad_signature_call.what()), 0U);
}
