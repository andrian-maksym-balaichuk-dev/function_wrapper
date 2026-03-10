#ifndef FW_DETAIL_SIGNATURE_INTERFACE_HPP
#define FW_DETAIL_SIGNATURE_INTERFACE_HPP

#include <fw/call_result.hpp>
#include <fw/detail/vtable.hpp>
#include <fw/exceptions.hpp>

namespace fw::detail
{
// ── Signature Interface ───────────────────────────────────────────────────────
// Provides the call operator and call() member function for a given signature, dispatching to the appropriate
// vtable entry based on the value category of the object. The derived class must provide object_ptr() and
// vtable_ptr() member functions to access the underlying object and its vtable.

template <class Derived, class Sig>
struct signature_interface;

template <class Derived, class TL>
struct signature_interface_pack;

template <class Derived, class R, class... Args>
struct signature_interface<Derived, R(Args...)>
{
private:
    using entry_type = signature_vtable_entry<R(Args...)>;

    struct dispatch_state
    {
        const entry_type* entry;
        void* object;
    };

    struct const_dispatch_state
    {
        const entry_type* entry;
        const void* object;
    };

    [[nodiscard]] static dispatch_state load_l_(Derived& self) noexcept
    {
        const auto* vt = self.vtable_ptr();
        return { vt ? &static_cast<const entry_type&>(*vt) : nullptr, self.object_ptr() };
    }

    [[nodiscard]] static const_dispatch_state load_cl_(const Derived& self) noexcept
    {
        const auto* vt = self.vtable_ptr();
        return { vt ? &static_cast<const entry_type&>(*vt) : nullptr, self.object_ptr() };
    }

    [[nodiscard]] static dispatch_state load_r_(Derived& self) noexcept
    {
        const auto* vt = self.vtable_ptr();
        return { vt ? &static_cast<const entry_type&>(*vt) : nullptr, self.object_ptr() };
    }

    static try_call_result<R> try_dispatch_l_(Derived& self, Args... args) noexcept(noexcept(std::declval<typename signature_vtable_entry<R(Args...)>::lcall_t>()(
        self.object_ptr(),
        std::forward<Args>(args)...)))
    {
        const auto state = load_l_(self);
        if (!state.entry || !state.object)
        {
            return try_call_result<R>::empty();
        }

        if (!state.entry->lcall)
        {
            return try_call_result<R>::signature_mismatch();
        }
        if constexpr (!std::is_void_v<R>)
        {
            return try_call_result<R>::success(state.entry->lcall(state.object, std::forward<Args>(args)...));
        }
        else
        {
            state.entry->lcall(state.object, std::forward<Args>(args)...);
            return try_call_result<R>::success();
        }
    }

    static try_call_result<R> try_dispatch_cl_(const Derived& self, Args... args) noexcept(noexcept(std::declval<typename signature_vtable_entry<R(Args...)>::clcall_t>()(
        self.object_ptr(),
        std::forward<Args>(args)...)))
    {
        const auto state = load_cl_(self);
        if (!state.entry || !state.object)
        {
            return try_call_result<R>::empty();
        }

        if (!state.entry->clcall)
        {
            return try_call_result<R>::signature_mismatch();
        }
        if constexpr (!std::is_void_v<R>)
        {
            return try_call_result<R>::success(state.entry->clcall(state.object, std::forward<Args>(args)...));
        }
        else
        {
            state.entry->clcall(state.object, std::forward<Args>(args)...);
            return try_call_result<R>::success();
        }
    }

    static try_call_result<R> try_dispatch_r_(Derived& self, Args... args) noexcept(noexcept(std::declval<typename signature_vtable_entry<R(Args...)>::rcall_t>()(
        self.object_ptr(),
        std::forward<Args>(args)...)))
    {
        const auto state = load_r_(self);
        if (!state.entry || !state.object)
        {
            return try_call_result<R>::empty();
        }

        if (!state.entry->rcall)
        {
            return try_call_result<R>::signature_mismatch();
        }
        if constexpr (!std::is_void_v<R>)
        {
            return try_call_result<R>::success(state.entry->rcall(state.object, std::forward<Args>(args)...));
        }
        else
        {
            state.entry->rcall(state.object, std::forward<Args>(args)...);
            return try_call_result<R>::success();
        }
    }

    static R dispatch_l_(Derived& self, Args... args)
    {
        const auto state = load_l_(self);
        if (!state.entry || !state.object)
        {
            FW_UNLIKELY throw bad_call{};
        }

        if (!state.entry->lcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return state.entry->lcall(state.object, std::forward<Args>(args)...);
        }
        else
        {
            state.entry->lcall(state.object, std::forward<Args>(args)...);
            return;
        }
    }

    static R dispatch_cl_(const Derived& self, Args... args)
    {
        const auto state = load_cl_(self);
        if (!state.entry || !state.object)
        {
            FW_UNLIKELY throw bad_call{};
        }

        if (!state.entry->clcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return state.entry->clcall(state.object, std::forward<Args>(args)...);
        }
        else
        {
            state.entry->clcall(state.object, std::forward<Args>(args)...);
            return;
        }
    }

    static R dispatch_r_(Derived& self, Args... args)
    {
        const auto state = load_r_(self);
        if (!state.entry || !state.object)
        {
            FW_UNLIKELY throw bad_call{};
        }

        if (!state.entry->rcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return state.entry->rcall(state.object, std::forward<Args>(args)...);
        }
        else
        {
            state.entry->rcall(state.object, std::forward<Args>(args)...);
            return;
        }
    }

public:
    // clang-format off
    try_call_result<R> try_call(Args... args) & noexcept(noexcept(try_dispatch_l_(static_cast<Derived&>(*this), std::forward<Args>(args)...)))
    {
        return try_dispatch_l_(static_cast<Derived&>(*this), std::forward<Args>(args)...);
    }
    try_call_result<R> try_call(Args... args) const& noexcept(noexcept(try_dispatch_cl_(static_cast<const Derived&>(*this), std::forward<Args>(args)...)))
    {
        return try_dispatch_cl_(static_cast<const Derived&>(*this), std::forward<Args>(args)...);
    }
    try_call_result<R> try_call(Args... args) && noexcept(noexcept(try_dispatch_r_(static_cast<Derived&>(*this), std::forward<Args>(args)...)))
    {
        return try_dispatch_r_(static_cast<Derived&>(*this), std::forward<Args>(args)...);
    }

    R call(Args... args) & { return dispatch_l_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }
    R call(Args... args) const& { return dispatch_cl_(static_cast<const Derived&>(*this), std::forward<Args>(args)...); }
    R call(Args... args) && { return dispatch_r_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }

    R operator()(Args... args) & { return dispatch_l_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }
    R operator()(Args... args) const& { return dispatch_cl_(static_cast<const Derived&>(*this), std::forward<Args>(args)...); }
    R operator()(Args... args) && { return dispatch_r_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }
    // clang-format on
};

template <class Derived, class R, class... Args>
struct signature_interface<Derived, R(Args...) noexcept>
{
private:
    using entry_type = signature_vtable_entry<R(Args...) noexcept>;

    struct dispatch_state
    {
        const entry_type* entry;
        void* object;
    };

    struct const_dispatch_state
    {
        const entry_type* entry;
        const void* object;
    };

    [[nodiscard]] static dispatch_state load_l_(Derived& self) noexcept
    {
        const auto* vt = self.vtable_ptr();
        return { vt ? &static_cast<const entry_type&>(*vt) : nullptr, self.object_ptr() };
    }

    [[nodiscard]] static const_dispatch_state load_cl_(const Derived& self) noexcept
    {
        const auto* vt = self.vtable_ptr();
        return { vt ? &static_cast<const entry_type&>(*vt) : nullptr, self.object_ptr() };
    }

    [[nodiscard]] static dispatch_state load_r_(Derived& self) noexcept
    {
        const auto* vt = self.vtable_ptr();
        return { vt ? &static_cast<const entry_type&>(*vt) : nullptr, self.object_ptr() };
    }

    static try_call_result<R> try_dispatch_l_(Derived& self, Args... args) noexcept
    {
        const auto state = load_l_(self);
        if (!state.entry || !state.object)
        {
            return try_call_result<R>::empty();
        }

        if (!state.entry->lcall)
        {
            return try_call_result<R>::signature_mismatch();
        }
        if constexpr (!std::is_void_v<R>)
        {
            return try_call_result<R>::success(state.entry->lcall(state.object, std::forward<Args>(args)...));
        }
        else
        {
            state.entry->lcall(state.object, std::forward<Args>(args)...);
            return try_call_result<R>::success();
        }
    }

    static try_call_result<R> try_dispatch_cl_(const Derived& self, Args... args) noexcept
    {
        const auto state = load_cl_(self);
        if (!state.entry || !state.object)
        {
            return try_call_result<R>::empty();
        }

        if (!state.entry->clcall)
        {
            return try_call_result<R>::signature_mismatch();
        }
        if constexpr (!std::is_void_v<R>)
        {
            return try_call_result<R>::success(state.entry->clcall(state.object, std::forward<Args>(args)...));
        }
        else
        {
            state.entry->clcall(state.object, std::forward<Args>(args)...);
            return try_call_result<R>::success();
        }
    }

    static try_call_result<R> try_dispatch_r_(Derived& self, Args... args) noexcept
    {
        const auto state = load_r_(self);
        if (!state.entry || !state.object)
        {
            return try_call_result<R>::empty();
        }

        if (!state.entry->rcall)
        {
            return try_call_result<R>::signature_mismatch();
        }
        if constexpr (!std::is_void_v<R>)
        {
            return try_call_result<R>::success(state.entry->rcall(state.object, std::forward<Args>(args)...));
        }
        else
        {
            state.entry->rcall(state.object, std::forward<Args>(args)...);
            return try_call_result<R>::success();
        }
    }

    static R dispatch_l_(Derived& self, Args... args)
    {
        const auto state = load_l_(self);
        if (!state.entry || !state.object)
        {
            FW_UNLIKELY throw bad_call{};
        }

        if (!state.entry->lcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return state.entry->lcall(state.object, std::forward<Args>(args)...);
        }
        else
        {
            state.entry->lcall(state.object, std::forward<Args>(args)...);
            return;
        }
    }

    static R dispatch_cl_(const Derived& self, Args... args)
    {
        const auto state = load_cl_(self);
        if (!state.entry || !state.object)
        {
            FW_UNLIKELY throw bad_call{};
        }

        if (!state.entry->clcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return state.entry->clcall(state.object, std::forward<Args>(args)...);
        }
        else
        {
            state.entry->clcall(state.object, std::forward<Args>(args)...);
            return;
        }
    }

    static R dispatch_r_(Derived& self, Args... args)
    {
        const auto state = load_r_(self);
        if (!state.entry || !state.object)
        {
            FW_UNLIKELY throw bad_call{};
        }

        if (!state.entry->rcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return state.entry->rcall(state.object, std::forward<Args>(args)...);
        }
        else
        {
            state.entry->rcall(state.object, std::forward<Args>(args)...);
            return;
        }
    }

public:
    // clang-format off
    try_call_result<R> try_call(Args... args) & noexcept { return try_dispatch_l_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }
    try_call_result<R> try_call(Args... args) const& noexcept { return try_dispatch_cl_(static_cast<const Derived&>(*this), std::forward<Args>(args)...); }
    try_call_result<R> try_call(Args... args) && noexcept { return try_dispatch_r_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }

    R call(Args... args) & { return dispatch_l_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }
    R call(Args... args) const& { return dispatch_cl_(static_cast<const Derived&>(*this), std::forward<Args>(args)...); }
    R call(Args... args) && { return dispatch_r_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }

    R operator()(Args... args) & { return dispatch_l_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }
    R operator()(Args... args) const& { return dispatch_cl_(static_cast<const Derived&>(*this), std::forward<Args>(args)...); }
    R operator()(Args... args) && { return dispatch_r_(static_cast<Derived&>(*this), std::forward<Args>(args)...); }
    // clang-format on
};

template <class Derived, class... Sigs>
struct signature_interface_pack<Derived, typelist<Sigs...>> : signature_interface<Derived, Sigs>...
{};
} // namespace fw::detail

#endif // FW_DETAIL_SIGNATURE_INTERFACE_HPP
