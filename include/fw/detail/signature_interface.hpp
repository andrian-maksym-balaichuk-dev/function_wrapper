#ifndef FW_DETAIL_SIGNATURE_INTERFACE_HPP
#define FW_DETAIL_SIGNATURE_INTERFACE_HPP

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
    static R dispatch_l_(Derived& self, Args... args)
    {
        const auto* vt = self.vtable_ptr();
        if (!vt)
        {
            FW_UNLIKELY throw bad_call{};
        }

        const auto& entry = static_cast<const signature_vtable_entry<R(Args...)>&>(*vt);
        if (!entry.lcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return entry.lcall(self.object_ptr(), std::forward<Args>(args)...);
        }
        else
        {
            entry.lcall(self.object_ptr(), std::forward<Args>(args)...);
            return;
        }
    }

    static R dispatch_cl_(const Derived& self, Args... args)
    {
        const auto* vt = self.vtable_ptr();
        if (!vt)
        {
            FW_UNLIKELY throw bad_call{};
        }

        const auto& entry = static_cast<const signature_vtable_entry<R(Args...)>&>(*vt);
        if (!entry.clcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return entry.clcall(self.object_ptr(), std::forward<Args>(args)...);
        }
        else
        {
            entry.clcall(self.object_ptr(), std::forward<Args>(args)...);
            return;
        }
    }

    static R dispatch_r_(Derived& self, Args... args)
    {
        const auto* vt = self.vtable_ptr();
        if (!vt)
        {
            FW_UNLIKELY throw bad_call{};
        }

        const auto& entry = static_cast<const signature_vtable_entry<R(Args...)>&>(*vt);
        if (!entry.rcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return entry.rcall(self.object_ptr(), std::forward<Args>(args)...);
        }
        else
        {
            entry.rcall(self.object_ptr(), std::forward<Args>(args)...);
            return;
        }
    }

public:
    // clang-format off
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
    static R dispatch_l_(Derived& self, Args... args)
    {
        const auto* vt = self.vtable_ptr();
        if (!vt)
        {
            FW_UNLIKELY throw bad_call{};
        }

        const auto& entry = static_cast<const signature_vtable_entry<R(Args...) noexcept>&>(*vt);
        if (!entry.lcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return entry.lcall(self.object_ptr(), std::forward<Args>(args)...);
        }
        else
        {
            entry.lcall(self.object_ptr(), std::forward<Args>(args)...);
            return;
        }
    }

    static R dispatch_cl_(const Derived& self, Args... args)
    {
        const auto* vt = self.vtable_ptr();
        if (!vt)
        {
            FW_UNLIKELY throw bad_call{};
        }

        const auto& entry = static_cast<const signature_vtable_entry<R(Args...) noexcept>&>(*vt);
        if (!entry.clcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return entry.clcall(self.object_ptr(), std::forward<Args>(args)...);
        }
        else
        {
            entry.clcall(self.object_ptr(), std::forward<Args>(args)...);
            return;
        }
    }

    static R dispatch_r_(Derived& self, Args... args)
    {
        const auto* vt = self.vtable_ptr();
        if (!vt)
        {
            FW_UNLIKELY throw bad_call{};
        }

        const auto& entry = static_cast<const signature_vtable_entry<R(Args...) noexcept>&>(*vt);
        if (!entry.rcall)
        {
            FW_UNLIKELY throw bad_signature_call{};
        }
        if constexpr (!std::is_void_v<R>)
        {
            return entry.rcall(self.object_ptr(), std::forward<Args>(args)...);
        }
        else
        {
            entry.rcall(self.object_ptr(), std::forward<Args>(args)...);
            return;
        }
    }

public:
    // clang-format off
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
