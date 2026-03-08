#ifndef FW_DETAIL_CONCEPTS_HPP
#define FW_DETAIL_CONCEPTS_HPP

#include <cstddef>
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
#else
#define FW_LIKELY
#define FW_UNLIKELY
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

// Small-buffer optimization capacity used by the wrapper.
#ifndef FW_SBO_SIZE
#define FW_SBO_SIZE (3 * sizeof(void*))
#endif

namespace fw::detail
{
// ── Utility Traits ───────────────────────────────────────────────────────────
template <class...>
inline constexpr bool always_false_v = false;

template <class T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;


// ── Function Signature Traits ───────────────────────────────────────────────
// True only for plain function signatures like R(Args...).
// clang-format off
template <class Sig>
struct is_function_signature : std::false_type {};

template <class R, class... Args>
struct is_function_signature<R(Args...)> : std::true_type {};
// clang-format on

template <class Sig>
inline constexpr bool is_function_signature_v = is_function_signature<Sig>::value;

#if FW_HAS_CONCEPTS
template <class Sig>
concept FunctionSignature = is_function_signature_v<Sig>;
#endif


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

#if FW_HAS_CONCEPTS
template <class Fn, class R, class... Args>
concept ExactInvocableR = std::invocable<Fn, Args...> && std::same_as<std::invoke_result_t<Fn, Args...>, R>;
#endif


// ── SBO Eligibility ─────────────────────────────────────────────────────────

template <class T>
inline constexpr bool fits_in_sbo_v =
    sizeof(T) <= FW_SBO_SIZE && alignof(T) <= alignof(std::max_align_t) && std::is_nothrow_move_constructible_v<T>;


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

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...), void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) const, void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...)&, void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) const&, void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...)&&, void>
{
    using type = R(Args...);
};

template <class C, class R, class... Args>
struct fn_sig<R (C::*)(Args...) const&&, void>
{
    using type = R(Args...);
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

template <class List, class T>
struct tl_contains;

template <class T, class... Ts>
struct tl_contains<typelist<Ts...>, T> : std::bool_constant<(std::is_same_v<T, Ts> || ...)>
{};

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

template <class R, class... Args, class... CallArgs>
struct signature_candidate<R(Args...), CallArgs...>
: signature_candidate_impl<sizeof...(Args) == sizeof...(CallArgs), R(Args...), CallArgs...>
{};

template <class LeftSig, class RightSig, class... CallArgs>
struct signature_is_better_than : std::false_type
{};

template <class LeftSig, class... CallArgs>
struct signature_is_better_than<LeftSig, no_matching_signature, CallArgs...>
: std::bool_constant<signature_candidate<LeftSig, CallArgs...>::viable>
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

template <class LeftR, class... LeftArgs, class RightR, class... RightArgs, class... CallArgs>
struct signature_is_better_than<LeftR(LeftArgs...), RightR(RightArgs...), CallArgs...>
: signature_is_better_than_impl<sizeof...(LeftArgs) == sizeof...(RightArgs) && sizeof...(LeftArgs) == sizeof...(CallArgs), LeftR(LeftArgs...), RightR(RightArgs...), CallArgs...>
{};

template <class Sig, class... CallArgs>
struct signature_matches_common_numeric_target;

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
: signature_matches_common_numeric_target_impl<(sizeof...(CallArgs) > 0), Sig, CallArgs...>
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
    static R invoke(Self&& self, CallArgs&&... args)
    {
        return std::forward<Self>(self).call(static_cast<Args>(std::forward<CallArgs>(args))...);
    }
};

template <class Sig, class Base, class... CallArgs>
decltype(auto) invoke_signature_call(Base&& base, CallArgs&&... args)
{
    return signature_invoker<Sig>::invoke(std::forward<Base>(base), std::forward<CallArgs>(args)...);
}

} // namespace fw::detail

#endif // FW_DETAIL_CONCEPTS_HPP
