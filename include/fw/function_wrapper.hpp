#ifndef FW_FUNCTION_WRAPPER_HPP
#define FW_FUNCTION_WRAPPER_HPP

#include <fw/detail/concepts.hpp>
#include <fw/member_adapter.hpp>
#include <fw/detail/signature_interface.hpp>
#include <fw/static_function.hpp>
#include <fw/detail/vtable.hpp>

#include <array>
#include <typeinfo>
#include <utility>

namespace fw
{
// ── function_wrapper ───────────────────────────────────────────────────────────
// The class inherits one CRTP signature_interface base per signature.

template <class... Args>
class function_wrapper : public detail::signature_interface_pack<function_wrapper<Args...>, typename detail::wrapper_template_arguments<Args...>::signatures>
{
    using argument_traits = detail::wrapper_template_arguments<Args...>;

    static_assert(detail::tl_size_v<typename argument_traits::signatures> > 0, "fw::function_wrapper requires at least one signature.");
    static_assert(
        detail::tl_all_function_signatures_v<typename argument_traits::signatures>,
        "All fw::function_wrapper template parameters must be function "
        "signatures of the form R(Args...) or R(Args...) noexcept.");
    static_assert(
        !detail::tl_has_conflicting_signatures_v<typename argument_traits::signatures>,
        "fw::function_wrapper does not allow both R(Args...) and R(Args...) noexcept for the same argument list.");

public:
    using self_type = function_wrapper;
    using policy_type = typename argument_traits::policy;
    using signatures_type = typename argument_traits::signatures;
    using single_signature_type = detail::tl_front_or_t<signatures_type, void()>;
    using constexpr_storage_type = typename detail::tl_apply<signatures_type, static_function>::type;
    using storage_type = typename argument_traits::template apply<detail::wrapper_storage>;
    using vtable_type = typename argument_traits::template apply<detail::wrapper_vtable>;

    constexpr function_wrapper() noexcept = default;

#if FW_HAS_CONCEPTS
    template <class F> requires(!detail::is_specialization_of_v<F, function_wrapper>)
    constexpr function_wrapper(F&& f) // NOLINT (Intentionally implicit — mirrors std::function conversion semantics.)
    {
        assign(std::forward<F>(f));
    }
#else
    template <class F, std::enable_if_t<!detail::is_specialization_of_v<F, function_wrapper>, int> = 0>
    constexpr function_wrapper(F&& f) // NOLINT (Intentionally implicit — mirrors std::function conversion semantics.)
    {
        assign(std::forward<F>(f));
    }
#endif

    constexpr function_wrapper(const function_wrapper& other)
        : constexpr_storage_(other.constexpr_storage_),
          constexpr_storage_active_(other.constexpr_storage_active_)
    {
        if (detail::is_constant_evaluated_compat())
        {
            return;
        }

        if (other.uses_trivial_small_relocate_())
        {
            detail::copy_trivial_small_storage(storage_, other.storage_);
        }
        else if (const auto* vt = other.storage_.vtable_ptr())
        {
            vt->copy(storage_, other.storage_);
        }
    }

    constexpr function_wrapper(function_wrapper&& other) noexcept
        : constexpr_storage_(std::move(other.constexpr_storage_)),
          constexpr_storage_active_(other.constexpr_storage_active_)
    {
        other.constexpr_storage_.reset();
        other.constexpr_storage_active_ = false;

        if (detail::is_constant_evaluated_compat())
        {
            return;
        }

        if (other.uses_trivial_small_relocate_())
        {
            detail::move_trivial_small_storage(storage_, other.storage_);
        }
        else if (const auto* vt = other.storage_.vtable_ptr())
        {
            vt->move(storage_, other.storage_);
        }
    }

    constexpr function_wrapper& operator=(const function_wrapper& rhs)
    {
        if (this != &rhs)
        {
            reset();
            constexpr_storage_ = rhs.constexpr_storage_;
            constexpr_storage_active_ = rhs.constexpr_storage_active_;

            if (detail::is_constant_evaluated_compat())
            {
                return *this;
            }

            if (rhs.uses_trivial_small_relocate_())
            {
                detail::copy_trivial_small_storage(storage_, rhs.storage_);
            }
            else if (const auto* vt = rhs.storage_.vtable_ptr())
            {
                vt->copy(storage_, rhs.storage_);
            }
        }
        return *this;
    }

    constexpr function_wrapper& operator=(function_wrapper&& rhs) noexcept
    {
        if (this != &rhs)
        {
            reset();
            constexpr_storage_ = std::move(rhs.constexpr_storage_);
            constexpr_storage_active_ = rhs.constexpr_storage_active_;
            rhs.constexpr_storage_.reset();
            rhs.constexpr_storage_active_ = false;

            if (detail::is_constant_evaluated_compat())
            {
                return *this;
            }

            if (rhs.uses_trivial_small_relocate_())
            {
                detail::move_trivial_small_storage(storage_, rhs.storage_);
            }
            else if (const auto* vt = rhs.storage_.vtable_ptr())
            {
                vt->move(storage_, rhs.storage_);
            }
        }
        return *this;
    }

#if FW_HAS_CONCEPTS
    template <class F> requires(!detail::is_specialization_of_v<F, function_wrapper>)
    constexpr function_wrapper& operator=(F&& f)
    {
        reset();
        assign(std::forward<F>(f));
        return *this;
    }
#else
    template <class F, std::enable_if_t<!detail::is_specialization_of_v<F, function_wrapper>, int> = 0>
    constexpr function_wrapper& operator=(F&& f)
    {
        reset();
        assign(std::forward<F>(f));
        return *this;
    }
#endif

    FW_CXX20_CONSTEXPR ~function_wrapper() noexcept
    {
        reset();
    }

    FW_NODISCARD_MSG("check before calling to avoid bad_call") constexpr bool has_value() const noexcept
    {
        if (detail::is_constant_evaluated_compat())
        {
            return constexpr_storage_.has_value();
        }

        return constexpr_storage_active_ || !storage_.is_empty();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] const std::type_info& target_type() const noexcept
    {
        const auto* vt = storage_.vtable_ptr();
        return vt ? *vt->type : typeid(void);
    }

    template <class Sig>
    [[nodiscard]] static constexpr bool contains_signature() noexcept
    {
        return detail::tl_contains_v<signatures_type, Sig>;
    }

    template <class Sig>
    [[nodiscard]] constexpr bool has_bound_signature() const noexcept
    {
        if constexpr (!contains_signature<Sig>())
        {
            return false;
        }
        else if (use_constexpr_storage_())
        {
            return constexpr_storage_.template has_bound_signature<Sig>();
        }
        else
        {
            return detail::vtable_has_bound_signature<Sig>(storage_.vtable_ptr());
        }
    }

    [[nodiscard]] constexpr auto bound_signatures() const noexcept
    {
        if (use_constexpr_storage_())
        {
            return constexpr_storage_.bound_signatures();
        }
        return bound_signatures_impl_(signatures_type{});
    }

    template <class Sig>
    [[nodiscard]] constexpr auto target_signature() const noexcept -> detail::signature_pointer_t<Sig>
    {
        if constexpr (!contains_signature<Sig>())
        {
            return nullptr;
        }
        else if (use_constexpr_storage_())
        {
            return constexpr_storage_.template target<Sig>();
        }
        else if (const auto* pointer = target<detail::signature_pointer_t<Sig>>())
        {
            return *pointer;
        }
        else
        {
            return nullptr;
        }
    }

    template <class... CallArgs>
    constexpr auto try_call(CallArgs&&... args) &
    {
        return dispatch_try_call_(*this, std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    constexpr auto try_call(CallArgs&&... args) const&
    {
        return dispatch_try_call_(*this, std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    constexpr auto try_call(CallArgs&&... args) &&
    {
        return dispatch_try_call_(std::move(*this), std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    constexpr decltype(auto) call(CallArgs&&... args) &
    {
        return dispatch_call_(*this, std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    constexpr decltype(auto) call(CallArgs&&... args) const&
    {
        return dispatch_call_(*this, std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    constexpr decltype(auto) call(CallArgs&&... args) &&
    {
        return dispatch_call_(std::move(*this), std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    constexpr decltype(auto) operator()(CallArgs&&... args) &
    {
        return call(std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    constexpr decltype(auto) operator()(CallArgs&&... args) const&
    {
        return call(std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    constexpr decltype(auto) operator()(CallArgs&&... args) &&
    {
        return std::move(*this).call(std::forward<CallArgs>(args)...);
    }

    template <class T>
    [[nodiscard]] T* target() noexcept
    {
        const auto* vt = storage_.vtable_ptr();
        if (!vt || *vt->type != typeid(T))
        {
            return nullptr; // Not a pointer to T stored. Could be empty, or could be a different type.
        }
        return static_cast<T*>(storage_.obj);
    }

    template <class T>
    [[nodiscard]] const T* target() const noexcept
    {
        const auto* vt = storage_.vtable_ptr();
        if (!vt || *vt->type != typeid(T))
        {
            return nullptr; // Not a pointer to T stored. Could be empty, or could be a different type.
        }
        return static_cast<const T*>(storage_.obj);
    }

    // Used by signature_interface::dispatch_() to reach the vtable and object.
    [[nodiscard]] const vtable_type* vtable_ptr() const noexcept
    {
        return storage_.vtable_ptr();
    }

    [[nodiscard]] void* object_ptr() noexcept
    {
        return storage_.obj;
    }
    [[nodiscard]] const void* object_ptr() const noexcept
    {
        return storage_.obj;
    }

    constexpr void reset() noexcept
    {
        constexpr_storage_.reset();
        constexpr_storage_active_ = false;

        if (detail::is_constant_evaluated_compat())
        {
            return;
        }

        if (uses_trivial_small_destroy_())
        {
            detail::destroy_trivial_small_storage(storage_);
        }
        else if (const auto* vt = storage_.vtable_ptr())
        {
            vt->destroy(storage_);
        }
    }

    constexpr void swap(function_wrapper& other) noexcept
    {
        if (this == &other)
        {
            return; // Self-swap is a no-op, and also avoids clobbering our storage before we can move it.
        }

        using std::swap;
        swap(constexpr_storage_, other.constexpr_storage_);
        swap(constexpr_storage_active_, other.constexpr_storage_active_);

        if (detail::is_constant_evaluated_compat())
        {
            return;
        }

        storage_type tmp{};

        if (const auto* vt = storage_.vtable_ptr())
        {
            vt->move(tmp, storage_);
        }
        if (const auto* vt = other.storage_.vtable_ptr())
        {
            vt->move(storage_, other.storage_);
        }
        if (const auto* vt = tmp.vtable_ptr())
        {
            vt->move(other.storage_, tmp);
        }
    }

    friend constexpr void swap(function_wrapper& lhs, function_wrapper& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    [[nodiscard]] friend bool operator==(const function_wrapper& w, std::nullptr_t) noexcept
    {
        return !w.has_value();
    }

#if __cplusplus < 202002L && !(defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
    [[nodiscard]] friend bool operator!=(const function_wrapper& w, std::nullptr_t) noexcept
    {
        return w.has_value();
    }
    [[nodiscard]] friend bool operator==(std::nullptr_t, const function_wrapper& w) noexcept
    {
        return !w.has_value();
    }
    [[nodiscard]] friend bool operator!=(std::nullptr_t, const function_wrapper& w) noexcept
    {
        return w.has_value();
    }
#endif

private:
    template <class... DeclaredSigs>
    [[nodiscard]] constexpr auto bound_signatures_impl_(detail::typelist<DeclaredSigs...>) const noexcept
    {
        return std::array<bool, sizeof...(DeclaredSigs)>{ has_bound_signature<DeclaredSigs>()... };
    }

    [[nodiscard]] bool uses_trivial_small_destroy_() const noexcept
    {
        return storage_.has_trivial_small_destroy();
    }

    [[nodiscard]] bool uses_trivial_small_relocate_() const noexcept
    {
        return storage_.has_trivial_small_relocate();
    }

    template <class Self, class... CallArgs>
    static constexpr auto dispatch_try_call_(Self&& self, CallArgs&&... args)
    {
        if (self.use_constexpr_storage_())
        {
            return std::forward<Self>(self).constexpr_storage_.try_call(std::forward<CallArgs>(args)...);
        }

        if constexpr (detail::tl_size_v<signatures_type> == 1)
        {
            return std::forward<Self>(self).template try_invoke_signature_<single_signature_type>(std::forward<CallArgs>(args)...);
        }
        else
        {
            using best_match = detail::best_signature_t<signatures_type, CallArgs...>;

            if constexpr (!best_match::found)
            {
                static_assert(best_match::found, "fw::function_wrapper: no declared signature accepts these arguments.");
            }
            else
            {
                static_assert(!best_match::ambiguous, "fw::function_wrapper: call is ambiguous across declared signatures.");
                return std::forward<Self>(self).template try_invoke_signature_<typename best_match::type>(std::forward<CallArgs>(args)...);
            }
        }
    }

    template <class Self, class... CallArgs>
    static constexpr decltype(auto) dispatch_call_(Self&& self, CallArgs&&... args)
    {
        if (self.use_constexpr_storage_())
        {
            return std::forward<Self>(self).constexpr_storage_.call(std::forward<CallArgs>(args)...);
        }

        if constexpr (detail::tl_size_v<signatures_type> == 1)
        {
            return std::forward<Self>(self).template invoke_signature_<single_signature_type>(std::forward<CallArgs>(args)...);
        }
        else
        {
            using best_match = detail::best_signature_t<signatures_type, CallArgs...>;

            if constexpr (!best_match::found)
            {
                static_assert(best_match::found, "fw::function_wrapper: no declared signature accepts these arguments.");
            }
            else
            {
                static_assert(!best_match::ambiguous, "fw::function_wrapper: call is ambiguous across declared signatures.");
                return std::forward<Self>(self).template invoke_signature_<typename best_match::type>(std::forward<CallArgs>(args)...);
            }
        }
    }

    template <class Sig, class... CallArgs>
    auto try_invoke_signature_(CallArgs&&... args) &
    {
        auto& base = static_cast<detail::signature_interface<function_wrapper, Sig>&>(*this);
        return detail::invoke_signature_try_call<Sig>(base, std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    auto try_invoke_signature_(CallArgs&&... args) const&
    {
        const auto& base = static_cast<const detail::signature_interface<function_wrapper, Sig>&>(*this);
        return detail::invoke_signature_try_call<Sig>(base, std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    auto try_invoke_signature_(CallArgs&&... args) &&
    {
        auto&& base = static_cast<detail::signature_interface<function_wrapper, Sig>&&>(*this);
        return detail::invoke_signature_try_call<Sig>(std::move(base), std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    decltype(auto) invoke_signature_(CallArgs&&... args) &
    {
        auto& base = static_cast<detail::signature_interface<function_wrapper, Sig>&>(*this);
        return detail::invoke_signature_call<Sig>(base, std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    decltype(auto) invoke_signature_(CallArgs&&... args) const&
    {
        const auto& base = static_cast<const detail::signature_interface<function_wrapper, Sig>&>(*this);
        return detail::invoke_signature_call<Sig>(base, std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    decltype(auto) invoke_signature_(CallArgs&&... args) &&
    {
        auto&& base = static_cast<detail::signature_interface<function_wrapper, Sig>&&>(*this);
        return detail::invoke_signature_call<Sig>(std::move(base), std::forward<CallArgs>(args)...);
    }

    template <class F>
    constexpr void assign(F&& f)
    {
        if (detail::is_constant_evaluated_compat())
        {
            assign_constexpr_(std::forward<F>(f));
        }
        else
        {
            assign_runtime_(std::forward<F>(f));
        }
    }

    [[nodiscard]] constexpr bool use_constexpr_storage_() const noexcept
    {
        if (detail::is_constant_evaluated_compat())
        {
            return true;
        }

        return constexpr_storage_active_ && storage_.is_empty();
    }

    template <class F>
    constexpr void assign_constexpr_(F&& f)
    {
        using stored_type = std::decay_t<F>;

        if constexpr (std::is_same_v<stored_type, constexpr_storage_type>)
        {
            constexpr_storage_ = std::forward<F>(f);
            constexpr_storage_active_ = constexpr_storage_.has_value();
        }
        else if constexpr (constexpr_bindable_callable_v<stored_type>)
        {
            constexpr_storage_ = {};
            constexpr_storage_active_ =
                bind_constexpr_callable_(constexpr_storage_, std::forward<F>(f), signatures_type{});
        }
        else
        {
            throw bad_call{};
        }
    }

    template <class F>
    void assign_runtime_(F&& f)
    {
        using stored_type = std::decay_t<F>;
        using resolved_vtable = typename detail::vtable_instance_from_list<stored_type, policy_type, signatures_type>::type;
        const auto* vt = resolved_vtable::get();

        static_assert(std::is_copy_constructible_v<stored_type>, "fw::function_wrapper requires a copy-constructible callable.");
        static_assert(
            detail::supports_any_signature_in_list<stored_type, signatures_type>::value,
            "fw::function_wrapper: callable does not match any declared signature.");

        if constexpr (detail::fits_in_sbo_v<policy_type, stored_type>)
        {
            detail::emplace_small_storage<stored_type>(storage_, vt, std::forward<F>(f));
        }
        else
        {
            detail::emplace_heap_storage<stored_type>(storage_, vt, std::forward<F>(f));
        }
    }

    template <class Stored>
    static constexpr bool constexpr_bindable_callable_v = self_type::template constexpr_bindable_callable_impl_<Stored>(signatures_type{});

    template <class Stored, class... DeclaredSigs>
    static constexpr bool constexpr_bindable_callable_impl_(detail::typelist<DeclaredSigs...>)
    {
        return (std::is_convertible_v<Stored, detail::signature_pointer_t<DeclaredSigs>> || ...);
    }

    template <class Sig, class F>
    static constexpr bool bind_constexpr_signature_(constexpr_storage_type& storage, F&& f)
    {
        using pointer_type = detail::signature_pointer_t<Sig>;

        if constexpr (std::is_convertible_v<std::decay_t<F>, pointer_type>)
        {
            storage.template bind<Sig>(static_cast<pointer_type>(f));
            return true;
        }
        else
        {
            return false;
        }
    }

    template <class F, class... DeclaredSigs>
    static constexpr bool bind_constexpr_callable_(
        constexpr_storage_type& storage,
        F&& f,
        detail::typelist<DeclaredSigs...>)
    {
        bool any = false;
        ((any = bind_constexpr_signature_<DeclaredSigs>(storage, std::forward<F>(f)) || any), ...);
        return any;
    }

    constexpr_storage_type constexpr_storage_{};
    bool constexpr_storage_active_{false};
    storage_type storage_{};
};

template <class... Sigs>
constexpr function_wrapper<Sigs...> static_function<Sigs...>::to_function_wrapper() const&
{
    return function_wrapper<Sigs...>(*this);
}

template <class... Sigs>
template <class Policy>
constexpr function_wrapper<Policy, Sigs...> static_function<Sigs...>::to_function_wrapper() const&
{
    return function_wrapper<Policy, Sigs...>(*this);
}

template <class... Sigs>
constexpr function_wrapper<Sigs...> static_function<Sigs...>::to_function_wrapper() &&
{
    return function_wrapper<Sigs...>(std::move(*this));
}

template <class... Sigs>
template <class Policy>
constexpr function_wrapper<Policy, Sigs...> static_function<Sigs...>::to_function_wrapper() &&
{
    return function_wrapper<Policy, Sigs...>(std::move(*this));
}

// ── Deduction guide ────────────────────────────────────────────────────────────
// function_wrapper f = my_free_fn;  →  deduces function_wrapper<R(Args...)>

template <class F>
function_wrapper(F) -> function_wrapper<policy::default_policy, detail::fn_sig_t<F>>;

// ── make_function_array ────────────────────────────────────────────────────────
// Builds a std::array<function_wrapper<UnionOfSigs...>, N> from N callables.
// Signatures are deduplicated so each appears only once in the vtable.

template <class Policy = policy::default_policy, class... Fs>
constexpr auto make_function_array(Fs&&... fs)
{
    static_assert(sizeof...(Fs) > 0, "fw::make_function_array requires at least one callable.");
    using sigs_tl = detail::unique_tl<detail::fn_sig_t<Fs>...>;
    using policy_and_sigs_tl = typename detail::tl_prepend<Policy, sigs_tl>::type;
    using wrapper_t = typename detail::tl_apply<policy_and_sigs_tl, function_wrapper>::type;
    return std::array<wrapper_t, sizeof...(Fs)>{ wrapper_t{ std::forward<Fs>(fs) }... };
}

// ── Sanity check ───────────────────────────────────────────────────────────────
// Verify that the function_wrapper template is small enough to fit the expected number of signatures in the SBO buffer.
// If this static_assert fails, the default policy's SBO size or the wrapper layout has changed.

static_assert(
    sizeof(function_wrapper<policy::default_policy, int(int, int)>) <= sizeof(void*) * 8,
    "fw::function_wrapper is unexpectedly large; check fw::policy::default_policy.");
} // namespace fw

#endif // FW_FUNCTION_WRAPPER_HPP
