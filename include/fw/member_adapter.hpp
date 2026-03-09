#ifndef FW_MEMBER_ADAPTER_HPP
#define FW_MEMBER_ADAPTER_HPP

#include <fw/detail/concepts.hpp>

#include <functional>
#include <type_traits>
#include <utility>

namespace fw
{
// ── member_adapter ───────────────────────────────────────────────────────────
// Non-owning callable object that binds an object reference plus a member
// pointer, so owning wrappers can store common member-function/member-object
// cases without forcing a hand-written lambda.

template <class Object, class Member>
class member_adapter
{
    using object_type = std::remove_reference_t<Object>;
    using member_type = detail::remove_cvref_t<Member>;

public:
    constexpr member_adapter(object_type& object, member_type member) noexcept : object_(std::addressof(object)), member_(member) {}

#if FW_HAS_CONCEPTS
    template <class... CallArgs>
        requires std::invocable<const member_type&, object_type&, CallArgs...>
    constexpr decltype(auto) operator()(CallArgs&&... args) const
        noexcept(noexcept(std::invoke(member_, *object_, std::forward<CallArgs>(args)...)))
    {
        return std::invoke(member_, *object_, std::forward<CallArgs>(args)...);
    }
#else
    template <class... CallArgs,
              class = std::void_t<std::invoke_result_t<const member_type&, object_type&, CallArgs...>>>
    constexpr decltype(auto) operator()(CallArgs&&... args) const
        noexcept(noexcept(std::invoke(member_, *object_, std::forward<CallArgs>(args)...)))
    {
        return std::invoke(member_, *object_, std::forward<CallArgs>(args)...);
    }
#endif

    [[nodiscard]] constexpr object_type* object_ptr() const noexcept
    {
        return object_;
    }

    [[nodiscard]] constexpr member_type member_ptr() const noexcept
    {
        return member_;
    }

private:
    object_type* object_{ nullptr };
    member_type member_{};
};

template <class Object, class Member>
[[nodiscard]] constexpr auto member_ref(Object& object, Member member) noexcept
{
    static_assert(std::is_member_pointer_v<detail::remove_cvref_t<Member>>, "fw::member_ref requires a member-function or member-object pointer.");
    return member_adapter<std::remove_reference_t<Object>, detail::remove_cvref_t<Member>>(object, member);
}
} // namespace fw

namespace fw::detail
{
template <class Object, class Member, class = void>
struct member_adapter_signature;

template <class Object, class Member>
struct member_adapter_signature<Object, Member, std::enable_if_t<std::is_member_function_pointer_v<Member>>>
{
    using type = fn_sig_t<Member>;
};

template <class Object, class C, class T>
struct member_adapter_signature<Object, T C::*, std::enable_if_t<!std::is_function_v<T>>>
{
    using reference_type = std::conditional_t<std::is_const_v<Object>, const T&, T&>;
    using type = reference_type() noexcept;
};

template <class Object, class Member>
using member_adapter_signature_t = typename member_adapter_signature<Object, Member>::type;

template <class Object, class Member>
struct fn_sig<fw::member_adapter<Object, Member>, void>
{
    using type = member_adapter_signature_t<Object, Member>;
};
} // namespace fw::detail

#endif // FW_MEMBER_ADAPTER_HPP
