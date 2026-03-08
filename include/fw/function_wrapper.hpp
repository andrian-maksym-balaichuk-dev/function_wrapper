#ifndef FW_FUNCTION_WRAPPER_HPP
#define FW_FUNCTION_WRAPPER_HPP

#include <fw/detail/concepts.hpp>
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

template <class... Sigs>
class function_wrapper : public detail::signature_interface<function_wrapper<Sigs...>, Sigs>...
{
    static_assert(sizeof...(Sigs) > 0, "fw::function_wrapper requires at least one signature.");
    static_assert(
        (detail::is_function_signature_v<Sigs> && ...),
        "All fw::function_wrapper template parameters must be function "
        "signatures of the form R(Args...) or R(Args...) noexcept.");
    static_assert(
        !detail::has_conflicting_signatures_v<Sigs...>,
        "fw::function_wrapper does not allow both R(Args...) and R(Args...) noexcept for the same argument list.");

public:
    using self_type = function_wrapper;
    using storage_type = detail::wrapper_storage<Sigs...>;
    using vtable_type = detail::wrapper_vtable<Sigs...>;

    function_wrapper() noexcept = default;

#if FW_HAS_CONCEPTS
    template <class F> requires(!detail::is_specialization_of_v<F, function_wrapper>)
    function_wrapper(F&& f) // NOLINT (Intentionally implicit — mirrors std::function conversion semantics.)
    {
        assign(std::forward<F>(f));
    }
#else
    template <class F, std::enable_if_t<!detail::is_specialization_of_v<F, function_wrapper>, int> = 0>
    function_wrapper(F&& f) // NOLINT (Intentionally implicit — mirrors std::function conversion semantics.)
    {
        assign(std::forward<F>(f));
    }
#endif

    function_wrapper(const function_wrapper& other)
    {
        if (other.storage_.vt)
        {
            other.storage_.vt->copy(storage_, other.storage_);
        }
    }

    function_wrapper(function_wrapper&& other) noexcept
    {
        if (other.storage_.vt)
        {
            other.storage_.vt->move(storage_, other.storage_);
        }
    }

    function_wrapper& operator=(const function_wrapper& rhs)
    {
        if (this != &rhs)
        {
            reset();

            if (rhs.storage_.vt)
            {
                rhs.storage_.vt->copy(storage_, rhs.storage_);
            }
        }
        return *this;
    }

    function_wrapper& operator=(function_wrapper&& rhs) noexcept
    {
        if (this != &rhs)
        {
            reset();

            if (rhs.storage_.vt)
            {
                rhs.storage_.vt->move(storage_, rhs.storage_);
            }
        }
        return *this;
    }

#if FW_HAS_CONCEPTS
    template <class F> requires(!detail::is_specialization_of_v<F, function_wrapper>)
    function_wrapper& operator=(F&& f)
    {
        reset();
        assign(std::forward<F>(f));
        return *this;
    }
#else
    template <class F, std::enable_if_t<!detail::is_specialization_of_v<F, function_wrapper>, int> = 0>
    function_wrapper& operator=(F&& f)
    {
        reset();
        assign(std::forward<F>(f));
        return *this;
    }
#endif

    ~function_wrapper() noexcept
    {
        reset();
    }

    FW_NODISCARD_MSG("check before calling to avoid bad_call") bool has_value() const noexcept
    {
        return storage_.vt != nullptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] const std::type_info& target_type() const noexcept
    {
        return storage_.vt ? *storage_.vt->type : typeid(void);
    }

    template <class... CallArgs>
    decltype(auto) call(CallArgs&&... args) &
    {
        return dispatch_call_(*this, std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    decltype(auto) call(CallArgs&&... args) const&
    {
        return dispatch_call_(*this, std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    decltype(auto) call(CallArgs&&... args) &&
    {
        return dispatch_call_(std::move(*this), std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    decltype(auto) operator()(CallArgs&&... args) &
    {
        return call(std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    decltype(auto) operator()(CallArgs&&... args) const&
    {
        return call(std::forward<CallArgs>(args)...);
    }

    template <class... CallArgs>
    decltype(auto) operator()(CallArgs&&... args) &&
    {
        return std::move(*this).call(std::forward<CallArgs>(args)...);
    }

    template <class T>
    [[nodiscard]] T* target() noexcept
    {
        if (!storage_.vt || *storage_.vt->type != typeid(T))
        {
            return nullptr; // Not a pointer to T stored. Could be empty, or could be a different type.
        }
        return static_cast<T*>(storage_.vt->get_ptr(storage_));
    }

    template <class T>
    [[nodiscard]] const T* target() const noexcept
    {
        if (!storage_.vt || *storage_.vt->type != typeid(T))
        {
            return nullptr; // Not a pointer to T stored. Could be empty, or could be a different type.
        }
        return static_cast<const T*>(storage_.vt->get_cptr(storage_));
    }

    // Used by signature_interface::dispatch_() to reach the vtable and object.
    [[nodiscard]] const vtable_type* vtable_ptr() const noexcept
    {
        return storage_.vt;
    }

    [[nodiscard]] void* object_ptr() noexcept
    {
        return storage_.vt ? storage_.vt->get_ptr(storage_) : nullptr;
    }
    [[nodiscard]] const void* object_ptr() const noexcept
    {
        return storage_.vt ? storage_.vt->get_cptr(storage_) : nullptr;
    }

    void reset() noexcept
    {
        if (storage_.vt)
        {
            storage_.vt->destroy(storage_);
        }
    }

    void swap(function_wrapper& other) noexcept
    {
        if (this == &other)
        {
            return; // Self-swap is a no-op, and also avoids clobbering our storage before we can move it.
        }

        storage_type tmp{};

        if (storage_.vt)
        {
            storage_.vt->move(tmp, storage_);
        }
        if (other.storage_.vt)
        {
            other.storage_.vt->move(storage_, other.storage_);
        }
        if (tmp.vt)
        {
            tmp.vt->move(other.storage_, tmp);
        }
    }

    friend void swap(function_wrapper& lhs, function_wrapper& rhs) noexcept
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
    template <class Self, class... CallArgs>
    static decltype(auto) dispatch_call_(Self&& self, CallArgs&&... args)
    {
        using best_match = detail::best_signature_t<detail::typelist<Sigs...>, CallArgs...>;

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
    void assign(F&& f)
    {
        using stored_type = std::decay_t<F>;

        static_assert(std::is_copy_constructible_v<stored_type>, "fw::function_wrapper requires a copy-constructible callable.");
        static_assert(detail::supports_any_signature_v<stored_type, Sigs...>, "fw::function_wrapper: callable does not match any declared signature.");

        if constexpr (detail::fits_in_sbo_v<stored_type>)
        {
            detail::fw_construct<stored_type>(storage_.payload.sbo, std::forward<F>(f));
            storage_.kind = detail::storage_kind::Small;
        }
        else
        {
            storage_.payload.heap = new stored_type(std::forward<F>(f));
            storage_.kind = detail::storage_kind::Heap;
        }
        storage_.vt = detail::vtable_instance<stored_type, Sigs...>::get();
    }

    storage_type storage_{};
};

template <class... Sigs>
function_wrapper<Sigs...> static_function<Sigs...>::to_function_wrapper() const&
{
    return function_wrapper<Sigs...>(*this);
}

template <class... Sigs>
function_wrapper<Sigs...> static_function<Sigs...>::to_function_wrapper() &&
{
    return function_wrapper<Sigs...>(std::move(*this));
}

// ── Deduction guide ────────────────────────────────────────────────────────────
// function_wrapper f = my_free_fn;  →  deduces function_wrapper<R(Args...)>

template <class F>
function_wrapper(F) -> function_wrapper<detail::fn_sig_t<F>>;

// ── make_function_array ────────────────────────────────────────────────────────
// Builds a std::array<function_wrapper<UnionOfSigs...>, N> from N callables.
// Signatures are deduplicated so each appears only once in the vtable.

template <class... Fs>
auto make_function_array(Fs&&... fs)
{
    static_assert(sizeof...(Fs) > 0, "fw::make_function_array requires at least one callable.");
    using sigs_tl = detail::unique_tl<detail::fn_sig_t<Fs>...>;
    using wrapper_t = typename detail::tl_apply<sigs_tl, function_wrapper>::type;
    return std::array<wrapper_t, sizeof...(Fs)>{ wrapper_t{ std::forward<Fs>(fs) }... };
}

// ── Sanity check ───────────────────────────────────────────────────────────────
// Verify that the function_wrapper template is small enough to fit the expected number of signatures in the SBO buffer.
// If this static_assert fails, it means either FW_SBO_SIZE is too small or the vtable layout has unexpectedly changed
// and become larger, and you should investigate which case it is and adjust FW_SBO_SIZE if appropriate.

static_assert(sizeof(function_wrapper<int(int, int)>) <= sizeof(void*) * 8, "fw::function_wrapper is unexpectedly large; check FW_SBO_SIZE.");
} // namespace fw

#endif // FW_FUNCTION_WRAPPER_HPP
