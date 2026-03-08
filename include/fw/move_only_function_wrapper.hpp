#ifndef FW_MOVE_ONLY_FUNCTION_WRAPPER_HPP
#define FW_MOVE_ONLY_FUNCTION_WRAPPER_HPP

#include <fw/detail/concepts.hpp>
#include <fw/detail/signature_interface.hpp>
#include <fw/detail/vtable.hpp>

#include <array>
#include <typeinfo>
#include <utility>

namespace fw
{
// ── move_only_function_wrapper ────────────────────────────────────────────────
// The class mirrors function_wrapper but only supports move semantics.

template <class... Sigs>
class move_only_function_wrapper : public detail::signature_interface<move_only_function_wrapper<Sigs...>, Sigs>...
{
    static_assert(sizeof...(Sigs) > 0, "fw::move_only_function_wrapper requires at least one signature.");
    static_assert(
        (detail::is_function_signature_v<Sigs> && ...),
        "All fw::move_only_function_wrapper template parameters must be plain "
        "function signatures of the form R(Args...).");

public:
    using self_type = move_only_function_wrapper;
    using storage_type = detail::wrapper_storage<Sigs...>;
    using vtable_type = detail::wrapper_vtable<Sigs...>;

    move_only_function_wrapper() noexcept = default;

#if FW_HAS_CONCEPTS
    template <class F>
        requires(!std::is_same_v<std::decay_t<F>, self_type> && std::constructible_from<std::decay_t<F>, F&&>)
    move_only_function_wrapper(F&& f) // NOLINT (Intentionally implicit — mirrors std::move_only_function conversion semantics.)
    {
        assign(std::forward<F>(f));
    }
#else
    template <class F,
              std::enable_if_t<!std::is_same_v<std::decay_t<F>, self_type> && std::is_constructible_v<std::decay_t<F>, F&&>, int> = 0>
    move_only_function_wrapper(F&& f) // NOLINT (Intentionally implicit — mirrors std::move_only_function conversion semantics.)
    {
        assign(std::forward<F>(f));
    }
#endif

    move_only_function_wrapper(const move_only_function_wrapper&) = delete;
    move_only_function_wrapper& operator=(const move_only_function_wrapper&) = delete;

    move_only_function_wrapper(move_only_function_wrapper&& other) noexcept
    {
        if (other.storage_.vt)
        {
            other.storage_.vt->move(storage_, other.storage_);
        }
    }

    move_only_function_wrapper& operator=(move_only_function_wrapper&& rhs) noexcept
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
    template <class F>
        requires(!std::is_same_v<std::decay_t<F>, self_type> && std::constructible_from<std::decay_t<F>, F&&>)
    move_only_function_wrapper& operator=(F&& f)
    {
        reset();
        assign(std::forward<F>(f));
        return *this;
    }
#else
    template <class F,
              std::enable_if_t<!std::is_same_v<std::decay_t<F>, self_type> && std::is_constructible_v<std::decay_t<F>, F&&>, int> = 0>
    move_only_function_wrapper& operator=(F&& f)
    {
        reset();
        assign(std::forward<F>(f));
        return *this;
    }
#endif

    ~move_only_function_wrapper() noexcept
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
            return nullptr;
        }
        return static_cast<T*>(storage_.vt->get_ptr(storage_));
    }

    template <class T>
    [[nodiscard]] const T* target() const noexcept
    {
        if (!storage_.vt || *storage_.vt->type != typeid(T))
        {
            return nullptr;
        }
        return static_cast<const T*>(storage_.vt->get_cptr(storage_));
    }

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

    void swap(move_only_function_wrapper& other) noexcept
    {
        if (this == &other)
        {
            return;
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

    friend void swap(move_only_function_wrapper& lhs, move_only_function_wrapper& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    [[nodiscard]] friend bool operator==(const move_only_function_wrapper& w, std::nullptr_t) noexcept
    {
        return !w.has_value();
    }

#if __cplusplus < 202002L && !(defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
    [[nodiscard]] friend bool operator!=(const move_only_function_wrapper& w, std::nullptr_t) noexcept
    {
        return w.has_value();
    }
    [[nodiscard]] friend bool operator==(std::nullptr_t, const move_only_function_wrapper& w) noexcept
    {
        return !w.has_value();
    }
    [[nodiscard]] friend bool operator!=(std::nullptr_t, const move_only_function_wrapper& w) noexcept
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
            static_assert(best_match::found, "fw::move_only_function_wrapper: no declared signature accepts these arguments.");
        }
        else
        {
            static_assert(!best_match::ambiguous, "fw::move_only_function_wrapper: call is ambiguous across declared signatures.");
            return std::forward<Self>(self).template invoke_signature_<typename best_match::type>(std::forward<CallArgs>(args)...);
        }
    }

    template <class Sig, class... CallArgs>
    decltype(auto) invoke_signature_(CallArgs&&... args) &
    {
        auto& base = static_cast<detail::signature_interface<move_only_function_wrapper, Sig>&>(*this);
        return detail::invoke_signature_call<Sig>(base, std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    decltype(auto) invoke_signature_(CallArgs&&... args) const&
    {
        const auto& base = static_cast<const detail::signature_interface<move_only_function_wrapper, Sig>&>(*this);
        return detail::invoke_signature_call<Sig>(base, std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    decltype(auto) invoke_signature_(CallArgs&&... args) &&
    {
        auto&& base = static_cast<detail::signature_interface<move_only_function_wrapper, Sig>&&>(*this);
        return detail::invoke_signature_call<Sig>(std::move(base), std::forward<CallArgs>(args)...);
    }

    template <class F>
    void assign(F&& f)
    {
        using stored_type = std::decay_t<F>;

        static_assert(std::is_move_constructible_v<stored_type>, "fw::move_only_function_wrapper requires a move-constructible callable.");
        static_assert(detail::supports_any_signature_v<stored_type, Sigs...>, "fw::move_only_function_wrapper: callable does not match any declared signature.");

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

template <class F>
move_only_function_wrapper(F) -> move_only_function_wrapper<detail::fn_sig_t<F>>;

template <class... Fs>
auto make_move_only_function_array(Fs&&... fs)
{
    static_assert(sizeof...(Fs) > 0, "fw::make_move_only_function_array requires at least one callable.");
    using sigs_tl = detail::unique_tl<detail::fn_sig_t<Fs>...>;
    using wrapper_t = typename detail::tl_apply<sigs_tl, move_only_function_wrapper>::type;
    return std::array<wrapper_t, sizeof...(Fs)>{ wrapper_t{ std::forward<Fs>(fs) }... };
}

static_assert(
    sizeof(move_only_function_wrapper<int(int, int)>) <= sizeof(void*) * 8,
    "fw::move_only_function_wrapper is unexpectedly large; check FW_SBO_SIZE.");
} // namespace fw

#endif // FW_MOVE_ONLY_FUNCTION_WRAPPER_HPP
