#include <gtest/gtest.h>

#include <fw/detail/concepts.hpp>

#include <string>
#include <string_view>
#include <type_traits>

#include "test_types.hpp"

namespace
{
struct ExplicitOnly
{
    explicit operator int() const
    {
        return 9;
    }
};

struct NotConvertible
{};

template <class Source, class Target>
int conversion_rank()
{
    using rank_fn = int (*)();
    rank_fn volatile rank = &fw::detail::implicit_conversion_rank<Source, Target>;
    return rank();
}

} // namespace

TEST(Concepts, GivenCallableTraitsWhenQueriedThenTheyReportSupportedShapes)
{
    using unique_types = fw::detail::unique_tl<int, int, double>;

    EXPECT_TRUE((fw::detail::is_function_signature_v<int(int)>));
    EXPECT_TRUE((fw::detail::is_function_signature_v<int(int) noexcept>));
    EXPECT_FALSE((fw::detail::is_function_signature_v<int>));
    EXPECT_TRUE((fw::detail::is_exact_invocable_r_v<decltype(&fw::test_support::add), int, int, int>));
    EXPECT_TRUE((fw::detail::is_exact_nothrow_invocable_r_v<decltype(&fw::test_support::add_noexcept), int, int, int>));
    EXPECT_FALSE((fw::detail::is_exact_nothrow_invocable_r_v<decltype(&fw::test_support::add), int, int, int>));
    EXPECT_TRUE((fw::detail::fits_in_sbo_v<fw::policy::default_policy, int (*)(int, int)>));
    EXPECT_FALSE((fw::detail::fits_in_sbo_v<fw::policy::default_policy, fw::test_support::LargeAdder>));
    EXPECT_TRUE((std::is_same_v<fw::detail::fn_sig_t<decltype(&fw::test_support::add)>, int(int, int)>));
    EXPECT_TRUE((std::is_same_v<fw::detail::fn_sig_t<decltype(&fw::test_support::add_noexcept)>, int(int, int) noexcept>));
    EXPECT_TRUE((std::is_same_v<fw::detail::base_signature_t<int(int) noexcept>, int(int)>));
    EXPECT_TRUE((std::is_same_v<unique_types, fw::detail::typelist<int, double>>));
    EXPECT_TRUE((fw::detail::has_conflicting_signatures_v<int(int), int(int) noexcept>));
    EXPECT_FALSE((fw::detail::has_conflicting_signatures_v<int(int), long(long) noexcept>));
}

TEST(Concepts, GivenConvertibleTypesWhenRankedThenLibraryPolicyIsApplied)
{
    EXPECT_EQ((conversion_rank<int, int>()), 0);
    EXPECT_EQ((conversion_rank<int&, int>()), 0);
    EXPECT_EQ((conversion_rank<int&, int&>()), 0);
    EXPECT_EQ((conversion_rank<int&&, int&>()), 1);
    EXPECT_EQ((conversion_rank<float, double>()), 1);
    EXPECT_EQ((conversion_rank<short, int>()), 1);
    EXPECT_EQ((conversion_rank<int, float>()), 2);
    EXPECT_EQ((conversion_rank<int, short>()), 6);
    EXPECT_EQ((conversion_rank<double, float>()), 6);
    EXPECT_EQ((conversion_rank<const char(&)[4], std::string_view>()), 3);
    EXPECT_EQ((conversion_rank<fw::test_support::Dog, fw::test_support::Animal>()), 3);
    EXPECT_EQ((conversion_rank<fw::test_support::Dog*, fw::test_support::Animal*>()), 3);
    EXPECT_EQ((conversion_rank<const char(&)[4], std::string>()), 4);
    EXPECT_EQ((conversion_rank<fw::test_support::NameView, std::string_view>()), 5);
    EXPECT_EQ((conversion_rank<double, int>()), 6);
    EXPECT_EQ((conversion_rank<ExplicitOnly, int>()), fw::detail::conversion_not_viable);
    EXPECT_EQ((conversion_rank<NotConvertible, int>()), fw::detail::conversion_not_viable);
}

TEST(Concepts, GivenCandidateSignaturesWhenSelectedThenBestMatchWinsOrReportsAmbiguity)
{
    using empty_selection = fw::detail::best_signature_t<fw::detail::typelist<>, int>;
    using exact_candidate = fw::detail::signature_candidate<int(int), int>;
    using wrong_arity_candidate = fw::detail::signature_candidate<int(int, int), int>;
    using better_than_missing = fw::detail::signature_is_better_than<int(int), fw::detail::no_matching_signature, int>;
    using better_numeric = fw::detail::signature_is_better_than<int(int), long(long), short>;
    using common_numeric_false = fw::detail::signature_matches_common_numeric_target<int()>;
    using common_numeric_true = fw::detail::signature_matches_common_numeric_target<float(float, float), int, float>;
    using numeric_winner = fw::detail::best_signature_t<fw::detail::typelist<int(int, int), float(float, float)>, int, float>;
    using ambiguous_selection = fw::detail::best_signature_t<fw::detail::typelist<int(float), int(double)>, int>;

    EXPECT_FALSE(empty_selection::found);
    EXPECT_FALSE(empty_selection::ambiguous);
    EXPECT_TRUE(exact_candidate::viable);
    EXPECT_FALSE(wrong_arity_candidate::viable);
    EXPECT_TRUE(better_than_missing::value);
    EXPECT_TRUE(better_numeric::value);
    EXPECT_FALSE(common_numeric_false::value);
    EXPECT_TRUE(common_numeric_true::value);
    EXPECT_TRUE(numeric_winner::found);
    EXPECT_FALSE(numeric_winner::ambiguous);
    EXPECT_TRUE((std::is_same_v<typename numeric_winner::type, float(float, float)>));
    EXPECT_TRUE(ambiguous_selection::found);
    EXPECT_TRUE(ambiguous_selection::ambiguous);
}
