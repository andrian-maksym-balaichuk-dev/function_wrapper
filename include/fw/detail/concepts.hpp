#ifndef FW_DETAIL_CONCEPTS_HPP
#define FW_DETAIL_CONCEPTS_HPP

#include <cstddef>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// ── Feature Detection ────────────────────────────────────────────────────────
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#define FW_HAS_CONCEPTS 1
#include <concepts>
#else
#define FW_HAS_CONCEPTS 0
#endif

#if defined(__cpp_lib_construct_at) && __cpp_lib_construct_at >= 202002L
#define FW_HAS_CONSTRUCT_AT 1
#include <memory>
#else
#define FW_HAS_CONSTRUCT_AT 0
#endif

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#define FW_LIKELY [[likely]]
#define FW_UNLIKELY [[unlikely]]
#define FW_HAS_CXX20 1
#else
#define FW_LIKELY
#define FW_UNLIKELY
#define FW_HAS_CXX20 0
#endif

#if __cplusplus >= 202302L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202302L)
#define FW_HAS_CXX23 1
#else
#define FW_HAS_CXX23 0
#endif

#if FW_HAS_CXX20
#define FW_CXX20_CONSTEXPR constexpr
#else
#define FW_CXX20_CONSTEXPR
#endif

#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
#define FW_HAS_BIT_CAST 1
#include <bit>
#else
#define FW_HAS_BIT_CAST 0
#endif

#if defined(__cpp_consteval) && __cpp_consteval >= 201811L
#define FW_CONSTEVAL consteval
#else
#define FW_CONSTEVAL constexpr
#endif

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#define FW_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#else
#define FW_NODISCARD_MSG(msg) [[nodiscard]]
#endif

#if defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
#define FW_HAS_STD_UNREACHABLE 1
#else
#define FW_HAS_STD_UNREACHABLE 0
#endif

namespace fw::policy
{
template <std::size_t Size>
struct sbo
{
    static_assert(Size > 0, "fw::policy::sbo requires a non-zero storage size.");

    static constexpr std::size_t storage_size = Size;
};

using default_policy = sbo<4 * sizeof(void*)>;
} // namespace fw::policy

namespace fw::detail
{
// ── Utility Traits ───────────────────────────────────────────────────────────
template <class...>
inline constexpr bool always_false_v = false;

template <class T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

constexpr bool is_constant_evaluated_compat() noexcept
{
#if FW_HAS_CXX20
    return std::is_constant_evaluated();
#else
    return false;
#endif
}

template <class To, class From>
FW_CXX20_CONSTEXPR To bit_cast_compat(const From& source) noexcept
{
    static_assert(sizeof(To) == sizeof(From), "fw::detail::bit_cast_compat requires same-size types.");
    static_assert(std::is_trivially_copyable_v<To>, "fw::detail::bit_cast_compat requires trivially copyable destination.");
    static_assert(std::is_trivially_copyable_v<From>, "fw::detail::bit_cast_compat requires trivially copyable source.");

#if FW_HAS_BIT_CAST
    return std::bit_cast<To>(source);
#else
    To destination{};
    std::memcpy(&destination, &source, sizeof(To));
    return destination;
#endif
}

template <class T, template <class...> class Template>
struct is_specialization_of : std::false_type
{};

template <template <class...> class Template, class... Args>
struct is_specialization_of<Template<Args...>, Template> : std::true_type
{};

template <class T, template <class...> class Template>
inline constexpr bool is_specialization_of_v = is_specialization_of<remove_cvref_t<T>, Template>::value;

template <class Policy, class = void>
struct is_storage_policy : std::false_type
{};

template <class Policy>
struct is_storage_policy<Policy, std::void_t<decltype(remove_cvref_t<Policy>::storage_size)>>
: std::bool_constant<std::is_convertible_v<decltype(remove_cvref_t<Policy>::storage_size), std::size_t>>
{};

template <class Policy>
inline constexpr bool is_storage_policy_v = is_storage_policy<Policy>::value;

template <class Policy>
inline constexpr std::size_t sbo_storage_size_v = static_cast<std::size_t>(remove_cvref_t<Policy>::storage_size);


// ── Function Signature Traits ───────────────────────────────────────────────
// True for supported function signatures like R(Args...) and R(Args...) noexcept.
// clang-format off
template <class Sig>
struct is_function_signature : std::false_type {};

template <class R, class... Args>
struct is_function_signature<R(Args...)> : std::true_type {};

template <class R, class... Args>
struct is_function_signature<R(Args...) noexcept> : std::true_type {};
// clang-format on

template <class Sig>
inline constexpr bool is_function_signature_v = is_function_signature<Sig>::value;

#if FW_HAS_CONCEPTS
template <class Sig>
concept FunctionSignature = is_function_signature_v<Sig>;
#endif


template <class Sig>
struct signature_traits;

template <class R, class... Args>
struct signature_traits<R(Args...)>
{
    using return_type = R;
    using base_signature = R(Args...);
    using pointer_type = R (*)(Args...);

    static constexpr bool is_noexcept = false;
    static constexpr std::size_t arity = sizeof...(Args);
};

template <class R, class... Args>
struct signature_traits<R(Args...) noexcept>
{
    using return_type = R;
    using base_signature = R(Args...);
    using pointer_type = R (*)(Args...) noexcept;

    static constexpr bool is_noexcept = true;
    static constexpr std::size_t arity = sizeof...(Args);
};

template <class Sig>
using signature_return_t = typename signature_traits<Sig>::return_type;

template <class Sig>
using base_signature_t = typename signature_traits<Sig>::base_signature;

template <class Sig>
using signature_pointer_t = typename signature_traits<Sig>::pointer_type;

template <class Sig>
inline constexpr bool is_noexcept_signature_v = signature_traits<Sig>::is_noexcept;

template <class Sig>
inline constexpr std::size_t signature_arity_v = signature_traits<Sig>::arity;

template <class LeftSig, class RightSig>
inline constexpr bool conflicting_signature_pair_v =
    std::is_same_v<base_signature_t<LeftSig>, base_signature_t<RightSig>> && (is_noexcept_signature_v<LeftSig> != is_noexcept_signature_v<RightSig>);

template <class... Sigs>
struct has_conflicting_signatures : std::false_type
{};

template <class Sig, class... Rest>
struct has_conflicting_signatures<Sig, Rest...>
: std::bool_constant<((conflicting_signature_pair_v<Sig, Rest>) || ...) || has_conflicting_signatures<Rest...>::value>
{};

template <class Sig>
struct has_conflicting_signatures<Sig> : std::false_type
{};

template <>
struct has_conflicting_signatures<> : std::false_type
{};

template <class... Sigs>
inline constexpr bool has_conflicting_signatures_v = has_conflicting_signatures<Sigs...>::value;


// ── Exact Invocability ───────────────────────────────────────────────────────
// True iff Fn(Args...) is well-formed and returns exactly R.

template <class Void, class Fn, class R, class... Args>
struct is_exact_invocable_r_impl : std::false_type
{};

template <class Fn, class R, class... Args>
struct is_exact_invocable_r_impl<std::void_t<std::invoke_result_t<Fn, Args...>>, Fn, R, Args...>
: std::is_same<std::invoke_result_t<Fn, Args...>, R>
{};

template <class Fn, class R, class... Args>
inline constexpr bool is_exact_invocable_r_v = is_exact_invocable_r_impl<void, Fn, R, Args...>::value;

template <class Void, class Fn, class R, class... Args>
struct is_exact_nothrow_invocable_r_impl : std::false_type
{};

template <class Fn, class R, class... Args>
struct is_exact_nothrow_invocable_r_impl<std::void_t<std::invoke_result_t<Fn, Args...>>, Fn, R, Args...>
: std::bool_constant<std::is_same_v<std::invoke_result_t<Fn, Args...>, R> &&
                     noexcept(std::invoke(std::declval<Fn>(), std::declval<Args>()...))>
{};

template <class Fn, class R, class... Args>
inline constexpr bool is_exact_nothrow_invocable_r_v = is_exact_nothrow_invocable_r_impl<void, Fn, R, Args...>::value;

#if FW_HAS_CONCEPTS
template <class Fn, class R, class... Args>
concept ExactInvocableR = std::invocable<Fn, Args...> && std::same_as<std::invoke_result_t<Fn, Args...>, R>;

template <class Fn, class R, class... Args>
concept ExactNothrowInvocableR = ExactInvocableR<Fn, R, Args...> && noexcept(std::invoke(std::declval<Fn>(), std::declval<Args>()...));
#endif


// ── SBO Eligibility ─────────────────────────────────────────────────────────

template <class Policy, class T>
inline constexpr bool fits_in_sbo_v =
    sizeof(T) <= sbo_storage_size_v<Policy> && alignof(T) <= alignof(std::max_align_t) && std::is_nothrow_move_constructible_v<T>;


// ── Signature Deduction ─────────────────────────────────────────────────────
// Supports free functions, non-generic lambdas, and functors with one concrete operator().

template <class T, class = void>
struct fn_sig
{
    static_assert(
        always_false_v<T>,
        "fw: only free functions, non-generic lambdas, and functors with "
        "exactly one concrete operator() support signature deduction.");
};

template <class R, class... Args>
struct fn_sig<R (*)(Args...), void>
{
    using type = R(Args...);
};

template <class R, class... Args>
struct fn_sig<R (*)(Args...) noexcept, void>
{
    using type = R(Args...) noexcept;
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...), void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) noexcept, void>
{
    using type = R(Args...) noexcept;
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) const, void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) const noexcept, void>
{
    using type = R(Args...) noexcept;
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...)&, void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) & noexcept, void>
{
    using type = R(Args...) noexcept;
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) const&, void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) const& noexcept, void>
{
    using type = R(Args...) noexcept;
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...)&&, void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) && noexcept, void>
{
    using type = R(Args...) noexcept;
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) const&&, void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) const&& noexcept, void>
{
    using type = R(Args...) noexcept;
};

template <class F>
struct fn_sig<F, std::void_t<decltype(&std::decay_t<F>::operator())>> : fn_sig<decltype(&std::decay_t<F>::operator())>
{};

template <class F>
using fn_sig_t = typename fn_sig<std::decay_t<F>>::type;


// ── Typelist Utilities ──────────────────────────────────────────────────────
// A simple typelist and some basic operations on it, used for signature selection.

template <class... Ts>
struct typelist
{};

template <class T, class TL>
struct tl_prepend;

template <class T, class... Ts>
struct tl_prepend<T, typelist<Ts...>>
{
    using type = typelist<T, Ts...>;
};

template <class List, class T>
struct tl_contains;

template <class T, class... Ts>
struct tl_contains<typelist<Ts...>, T> : std::bool_constant<(std::is_same_v<T, Ts> || ...)>
{};

template <class TL, class T>
inline constexpr bool tl_contains_v = tl_contains<TL, T>::value;

template <class TL>
struct tl_size;

template <class... Ts>
struct tl_size<typelist<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)>
{};

template <class TL>
inline constexpr std::size_t tl_size_v = tl_size<TL>::value;

template <class TL>
struct tl_front;

template <class T, class... Ts>
struct tl_front<typelist<T, Ts...>>
{
    using type = T;
};

template <class TL>
using tl_front_t = typename tl_front<TL>::type;

template <class TL, class Default>
struct tl_front_or
{
    using type = Default;
};

template <class T, class... Ts, class Default>
struct tl_front_or<typelist<T, Ts...>, Default>
{
    using type = T;
};

template <class TL, class Default>
using tl_front_or_t = typename tl_front_or<TL, Default>::type;

template <class... Ts>
struct all_same : std::true_type
{};

template <class T, class U, class... Ts>
struct all_same<T, U, Ts...> : std::bool_constant<std::is_same_v<T, U> && all_same<U, Ts...>::value>
{};

template <class... Ts>
inline constexpr bool all_same_v = all_same<Ts...>::value;

template <class List, class... Ts>
struct tl_unique_impl;

template <class... Out>
struct tl_unique_impl<typelist<Out...>>
{
    using type = typelist<Out...>;
};

template <class... Out, class T, class... Rest>
struct tl_unique_impl<typelist<Out...>, T, Rest...>
: std::conditional_t<tl_contains<typelist<Out...>, T>::value, tl_unique_impl<typelist<Out...>, Rest...>, tl_unique_impl<typelist<Out..., T>, Rest...>>
{};

template <class... Ts>
using unique_tl = typename tl_unique_impl<typelist<>, Ts...>::type;

template <class TL, template <class...> class Z>
struct tl_apply;

template <template <class...> class Z, class... Ts>
struct tl_apply<typelist<Ts...>, Z>
{
    using type = Z<Ts...>;
};

template <class TL>
struct tl_all_function_signatures;

template <class... Sigs>
struct tl_all_function_signatures<typelist<Sigs...>> : std::bool_constant<(is_function_signature_v<Sigs> && ...)>
{};

template <class TL>
inline constexpr bool tl_all_function_signatures_v = tl_all_function_signatures<TL>::value;

template <class TL>
struct tl_has_conflicting_signatures;

template <class... Sigs>
struct tl_has_conflicting_signatures<typelist<Sigs...>> : std::bool_constant<has_conflicting_signatures_v<Sigs...>>
{};

template <class TL>
inline constexpr bool tl_has_conflicting_signatures_v = tl_has_conflicting_signatures<TL>::value;

template <class... Args>
struct wrapper_template_arguments;

template <>
struct wrapper_template_arguments<>
{
    using policy = fw::policy::default_policy;
    using signatures = typelist<>;

    template <template <class, class...> class Z>
    using apply = Z<policy>;
};

template <class First, class... Rest>
struct wrapper_template_arguments<First, Rest...>
{
    static constexpr bool has_explicit_policy = is_storage_policy_v<First>;

    using policy = std::conditional_t<has_explicit_policy, remove_cvref_t<First>, fw::policy::default_policy>;
    using signatures = std::conditional_t<has_explicit_policy, typelist<Rest...>, typelist<First, Rest...>>;

    template <template <class, class...> class Z>
    using apply = typename tl_apply<typename tl_prepend<policy, signatures>::type, Z>::type;
};


// ── Signature Selection Helpers ─────────────────────────────────────────────
// The machinery for selecting the best-matching signature from a list of candidates
// based on the types of the call arguments, mimicking overload resolution rules as closely as possible.

struct no_matching_signature
{};

inline constexpr int conversion_not_viable = 1024;

template <class From>
using unary_plus_result_t = decltype(+std::declval<From>());

template <class T>
struct is_basic_string : std::false_type
{};

template <class CharT, class Traits, class Alloc>
struct is_basic_string<std::basic_string<CharT, Traits, Alloc>> : std::true_type
{};

template <class T>
inline constexpr bool is_basic_string_v = is_basic_string<remove_cvref_t<T>>::value;

template <class T>
struct is_basic_string_view : std::false_type
{};

template <class CharT, class Traits>
struct is_basic_string_view<std::basic_string_view<CharT, Traits>> : std::true_type
{};

template <class T>
inline constexpr bool is_basic_string_view_v = is_basic_string_view<remove_cvref_t<T>>::value;

template <class T>
struct string_character;

template <class CharT, class Traits, class Alloc>
struct string_character<std::basic_string<CharT, Traits, Alloc>>
{
    using type = CharT;
};

template <class CharT, class Traits>
struct string_character<std::basic_string_view<CharT, Traits>>
{
    using type = CharT;
};

template <class T>
using string_character_t = typename string_character<remove_cvref_t<T>>::type;

template <class T, class CharT>
inline constexpr bool is_c_string_source_v =
    (std::is_pointer_v<std::remove_reference_t<T>> &&
     std::is_same_v<remove_cvref_t<std::remove_pointer_t<std::remove_reference_t<T>>>, CharT>) ||
    (std::is_array_v<std::remove_reference_t<T>> &&
     std::is_same_v<remove_cvref_t<std::remove_extent_t<std::remove_reference_t<T>>>, CharT>);

template <class Source, class Target>
struct is_c_string_to_string_view : std::false_type
{};

template <class Source, class CharT, class Traits>
struct is_c_string_to_string_view<Source, std::basic_string_view<CharT, Traits>>
: std::bool_constant<is_c_string_source_v<Source, CharT>>
{};

template <class Source, class Target>
inline constexpr bool is_c_string_to_string_view_v = is_c_string_to_string_view<Source, remove_cvref_t<Target>>::value;

template <class Source, class Target>
struct is_c_string_to_string : std::false_type
{};

template <class Source, class CharT, class Traits, class Alloc>
struct is_c_string_to_string<Source, std::basic_string<CharT, Traits, Alloc>>
: std::bool_constant<is_c_string_source_v<Source, CharT>>
{};

template <class Source, class Target>
inline constexpr bool is_c_string_to_string_v = is_c_string_to_string<Source, remove_cvref_t<Target>>::value;

template <class Source, class Target>
inline constexpr bool is_class_hierarchy_conversion_v = std::is_class_v<remove_cvref_t<Source>> &&
    std::is_class_v<remove_cvref_t<Target>> && std::is_base_of_v<remove_cvref_t<Target>, remove_cvref_t<Source>>;

template <class Source, class Target>
inline constexpr bool is_pointer_hierarchy_conversion_v =
    std::is_pointer_v<remove_cvref_t<Source>> && std::is_pointer_v<remove_cvref_t<Target>> &&
    std::is_class_v<remove_cvref_t<std::remove_pointer_t<remove_cvref_t<Source>>>> &&
    std::is_class_v<remove_cvref_t<std::remove_pointer_t<remove_cvref_t<Target>>>> &&
    std::is_base_of_v<remove_cvref_t<std::remove_pointer_t<remove_cvref_t<Target>>>, remove_cvref_t<std::remove_pointer_t<remove_cvref_t<Source>>>>;

template <class Source, class Target>
inline constexpr bool is_integral_promotion_v =
    std::is_integral_v<remove_cvref_t<Source>> && std::is_integral_v<remove_cvref_t<Target>> &&
    std::is_same_v<remove_cvref_t<Target>, remove_cvref_t<unary_plus_result_t<remove_cvref_t<Source>>>>;

template <class Source, class Target>
constexpr int implicit_conversion_rank()
{
    using source_type = remove_cvref_t<Source>;
    using target_type = remove_cvref_t<Target>;

    constexpr bool same_reference_category = std::is_lvalue_reference_v<Source> == std::is_lvalue_reference_v<Target>;

    // Lower is better. The goal is to preserve a deterministic subset of
    // overload-resolution behavior after type erasure. :)
    if constexpr (std::is_same_v<Source, Target>)
    {
        return 0;
    }

    if constexpr (std::is_same_v<source_type, target_type>)
    {
        return (!std::is_reference_v<Target> || same_reference_category) ? 0 : 1;
    }

    if constexpr (std::is_arithmetic_v<source_type> && std::is_arithmetic_v<target_type>)
    {
        if constexpr (std::is_same_v<source_type, float> && std::is_same_v<target_type, double>)
        {
            return 1;
        }

        if constexpr (is_integral_promotion_v<Source, Target>)
        {
            return 1;
        }

        if constexpr (std::is_floating_point_v<source_type> && std::is_integral_v<target_type>)
        {
            return 6;
        }

        return std::is_same_v<target_type, std::common_type_t<source_type, target_type>> ? 2 : 6;
    }

    if constexpr (is_c_string_to_string_view_v<Source, Target>)
    {
        return 3;
    }

    if constexpr (is_c_string_to_string_v<Source, Target>)
    {
        return 4;
    }

    if constexpr (is_class_hierarchy_conversion_v<Source, Target> || is_pointer_hierarchy_conversion_v<Source, Target>)
    {
        return 3;
    }

    return std::is_convertible_v<Source, Target> ? 5 : conversion_not_viable;
}

template <class Sig, class... CallArgs>
struct signature_candidate;

template <bool SameArity, class Sig, class... CallArgs>
struct signature_candidate_impl;

template <class R, class... Args, class... CallArgs>
struct signature_candidate_impl<false, R(Args...), CallArgs...>
{
    static constexpr bool viable = false;
};

template <class R, class... Args, class... CallArgs>
struct signature_candidate_impl<true, R(Args...), CallArgs...>
{
    static constexpr bool viable = (true && ... && std::is_convertible_v<CallArgs&&, Args>);
};

template <class Sig, class... CallArgs>
struct signature_candidate
: signature_candidate_impl<signature_arity_v<Sig> == sizeof...(CallArgs), base_signature_t<Sig>, CallArgs...>
{};

template <class LeftSig, class RightSig, class... CallArgs>
struct signature_is_better_than;

template <class LeftSig, class... CallArgs>
struct signature_is_better_than<LeftSig, no_matching_signature, CallArgs...>
: std::bool_constant<signature_candidate<LeftSig, CallArgs...>::viable>
{};

template <class RightSig, class... CallArgs>
struct signature_is_better_than<no_matching_signature, RightSig, CallArgs...> : std::false_type
{};

template <bool SameArity, class LeftSig, class RightSig, class... CallArgs>
struct signature_is_better_than_impl : std::false_type
{};

// The rules for determining whether one viable candidate is better than another are as follows:
// 1. If each argument has an implicit conversion sequence of the same rank, the tie is resolved as if by overload resolution rules
//         (i.e., a conversion that preserves cv-qualification and reference category is better than one that doesn't, and a conversion to a common arithmetic type is worse than one to the original type).
// 2. Otherwise, the candidate with the better (i.e., lower-ranked) implicit conversion sequence for at least one argument is better than the other.
template <class LeftR, class... LeftArgs, class RightR, class... RightArgs, class... CallArgs>
struct signature_is_better_than_impl<true, LeftR(LeftArgs...), RightR(RightArgs...), CallArgs...>
: std::bool_constant<
      signature_candidate<LeftR(LeftArgs...), CallArgs...>::viable && signature_candidate<RightR(RightArgs...), CallArgs...>::viable &&
      ((implicit_conversion_rank<CallArgs&&, LeftArgs>() <= implicit_conversion_rank<CallArgs&&, RightArgs>()) && ...) &&
      ((implicit_conversion_rank<CallArgs&&, LeftArgs>() < implicit_conversion_rank<CallArgs&&, RightArgs>()) || ...)>
{};

template <class LeftSig, class RightSig, class... CallArgs>
struct signature_is_better_than
: signature_is_better_than_impl<signature_arity_v<LeftSig> == signature_arity_v<RightSig> && signature_arity_v<LeftSig> == sizeof...(CallArgs),
                                base_signature_t<LeftSig>,
                                base_signature_t<RightSig>,
                                CallArgs...>
{};

template <class Sig, class... CallArgs>
struct signature_matches_common_numeric_target;

template <class... CallArgs>
struct signature_matches_common_numeric_target<no_matching_signature, CallArgs...> : std::false_type
{};

template <bool HasArguments, class Sig, class... CallArgs>
struct signature_matches_common_numeric_target_impl : std::false_type
{};

template <class R, class Target, class... Rest, class... CallArgs>
struct signature_matches_common_numeric_target_impl<true, R(Target, Rest...), CallArgs...>
: std::bool_constant<
      all_same_v<Target, Rest...> && std::is_arithmetic_v<remove_cvref_t<Target>> && (std::is_arithmetic_v<remove_cvref_t<CallArgs>> && ...) &&
      std::is_same_v<remove_cvref_t<Target>, std::common_type_t<remove_cvref_t<CallArgs>...>>>
{};

template <class Sig, class... CallArgs>
struct signature_matches_common_numeric_target
: signature_matches_common_numeric_target_impl<(sizeof...(CallArgs) > 0), base_signature_t<Sig>, CallArgs...>
{};

template <class TL, class... CallArgs>
struct best_signature_selector;

template <class... CallArgs>
struct best_signature_selector<typelist<>, CallArgs...>
{
    using type = no_matching_signature;

    static constexpr bool found = false;
    static constexpr bool ambiguous = false;
};

template <class Sig, class... Rest, class... CallArgs>
struct best_signature_selector<typelist<Sig, Rest...>, CallArgs...>
{
private:
    using remaining = best_signature_selector<typelist<Rest...>, CallArgs...>;
    using current = signature_candidate<Sig, CallArgs...>;

    static constexpr bool current_is_better = current::viable &&
        (!remaining::found || signature_is_better_than<Sig, typename remaining::type, CallArgs...>::value);

    static constexpr bool remaining_is_better = current::viable && remaining::found &&
        signature_is_better_than<typename remaining::type, Sig, CallArgs...>::value;

    static constexpr bool current_wins_numeric_tie = current::viable && remaining::found && !current_is_better &&
        !remaining_is_better && signature_matches_common_numeric_target<Sig, CallArgs...>::value &&
        !signature_matches_common_numeric_target<typename remaining::type, CallArgs...>::value;

    static constexpr bool remaining_wins_numeric_tie = current::viable && remaining::found && !current_is_better &&
        !remaining_is_better && signature_matches_common_numeric_target<typename remaining::type, CallArgs...>::value &&
        !signature_matches_common_numeric_target<Sig, CallArgs...>::value;

public:
    using type = std::conditional_t<(current_is_better || current_wins_numeric_tie), Sig, typename remaining::type>;

    static constexpr bool found = current::viable || remaining::found;

    static constexpr bool ambiguous = (current::viable && remaining::found && !current_is_better &&
                                       !remaining_is_better && !current_wins_numeric_tie && !remaining_wins_numeric_tie) ||
        (!(current_is_better || current_wins_numeric_tie) && remaining::ambiguous);
};

template <class TL, class... CallArgs>
using best_signature_t = best_signature_selector<TL, CallArgs...>;

template <class Sig>
struct signature_invoker;

template <class R, class... Args>
struct signature_invoker<R(Args...)>
{
    template <class Self, class... CallArgs>
    static constexpr R invoke(Self&& self, CallArgs&&... args)
    {
        return std::forward<Self>(self).call(static_cast<Args>(std::forward<CallArgs>(args))...);
    }
};

template <class R, class... Args>
struct signature_invoker<R(Args...) noexcept>
{
    template <class Self, class... CallArgs>
    static constexpr R invoke(Self&& self, CallArgs&&... args) noexcept(noexcept(std::forward<Self>(self).call(static_cast<Args>(std::forward<CallArgs>(args))...)))
    {
        return std::forward<Self>(self).call(static_cast<Args>(std::forward<CallArgs>(args))...);
    }
};

template <class Sig, class Base, class... CallArgs>
constexpr decltype(auto) invoke_signature_call(Base&& base, CallArgs&&... args) noexcept(noexcept(signature_invoker<Sig>::invoke(std::forward<Base>(base), std::forward<CallArgs>(args)...)))
{
    return signature_invoker<Sig>::invoke(std::forward<Base>(base), std::forward<CallArgs>(args)...);
}

template <class Sig, class Base, class... CallArgs>
constexpr auto invoke_signature_try_call(Base&& base, CallArgs&&... args) noexcept(noexcept(std::forward<Base>(base).try_call(std::forward<CallArgs>(args)...)))
{
    return std::forward<Base>(base).try_call(std::forward<CallArgs>(args)...);
}

} // namespace fw::detail

#endif // FW_DETAIL_CONCEPTS_HPP
