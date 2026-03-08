#ifndef FW_STATIC_FUNCTION_HPP
#define FW_STATIC_FUNCTION_HPP

#include <fw/detail/concepts.hpp>
#include <fw/exceptions.hpp>

#include <tuple>
#include <utility>

namespace fw
{
// ── static_function ───────────────────────────────────────────────────────────
// The class stores one raw function pointer per declared signature and resolves
// calls through the same compile-time best-signature selection used elsewhere.

template <class... Sigs>
class static_function
{
    static_assert(sizeof...(Sigs) > 0, "fw::static_function requires at least one signature.");
    static_assert((detail::is_function_signature_v<Sigs> && ...), "All fw::static_function template parameters must be plain function signatures.");

    template <class Sig>
    struct slot;

    // Each declared signature gets its own pointer slot inside the tuple.
    template <class R, class... Args>
    struct slot<R(Args...)>
    {
        using pointer_type = R (*)(Args...);
        pointer_type pointer{ nullptr };
    };

public:
    constexpr static_function() noexcept = default;

    template <class Sig>
    constexpr explicit static_function(typename slot<Sig>::pointer_type pointer) noexcept
    {
        bind<Sig>(pointer);
    }

    template <class Sig>
    constexpr void bind(typename slot<Sig>::pointer_type pointer) noexcept
    {
        std::get<slot<Sig>>(m_slots).pointer = pointer;
    }

    template <class Sig, auto Function>
    static FW_CONSTEVAL static_function make() noexcept
    {
        static_assert(
            std::is_same_v<typename slot<Sig>::pointer_type, detail::remove_cvref_t<decltype(+Function)>>,
            "fw::static_function: Sig must match the provided function pointer.");
        static_function result;
        result.template bind<Sig>(+Function);
        return result;
    }

    template <auto Function>
    static FW_CONSTEVAL static_function make() noexcept
    {
        using signature = detail::fn_sig_t<decltype(Function)>;
        return make<signature, Function>();
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return ((std::get<slot<Sigs>>(m_slots).pointer != nullptr) || ...);
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    template <class Sig>
    [[nodiscard]] constexpr typename slot<Sig>::pointer_type target() const noexcept
    {
        return std::get<slot<Sig>>(m_slots).pointer;
    }

    constexpr void reset() noexcept
    {
        ((std::get<slot<Sigs>>(m_slots).pointer = nullptr), ...);
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

private:
    // Select the best declared signature for this argument list at compile time.
    template <class Self, class... CallArgs>
    static constexpr decltype(auto) dispatch_call_(Self&& self, CallArgs&&... args)
    {
        using best_match = detail::best_signature_t<detail::typelist<Sigs...>, CallArgs...>;

        if constexpr (!best_match::found)
        {
            static_assert(best_match::found, "fw::static_function: no declared signature accepts these arguments.");
        }
        else
        {
            static_assert(!best_match::ambiguous, "fw::static_function: call is ambiguous across declared signatures.");
            return std::forward<Self>(self).template invoke_signature_<typename best_match::type>(std::forward<CallArgs>(args)...);
        }
    }

    template <class Sig, class... CallArgs>
    constexpr decltype(auto) invoke_signature_(CallArgs&&... args) &
    {
        return invoke_slot_<Sig>(std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    constexpr decltype(auto) invoke_signature_(CallArgs&&... args) const&
    {
        return invoke_slot_<Sig>(std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    constexpr decltype(auto) invoke_signature_(CallArgs&&... args) &&
    {
        return invoke_slot_<Sig>(std::forward<CallArgs>(args)...);
    }

    template <class Sig, class... CallArgs>
    constexpr decltype(auto) invoke_slot_(CallArgs&&... args) const
    {
        const auto pointer = std::get<slot<Sig>>(m_slots).pointer;
        if (!pointer)
        {
            throw bad_signature_call{};
        }
        return invoke_pointer_<Sig>(pointer, std::forward<CallArgs>(args)...);
    }

    template <class Sig>
    struct pointer_invoker_;

    template <class R, class... Args>
    struct pointer_invoker_<R(Args...)>
    {
        template <class... CallArgs>
        static constexpr R invoke(typename slot<R(Args...)>::pointer_type pointer, CallArgs&&... args)
        {
            if constexpr (std::is_void_v<R>)
            {
                pointer(static_cast<Args>(std::forward<CallArgs>(args))...);
            }
            else
            {
                return pointer(static_cast<Args>(std::forward<CallArgs>(args))...);
            }
        }
    };

    template <class Sig, class... CallArgs>
    static constexpr decltype(auto) invoke_pointer_(typename slot<Sig>::pointer_type pointer, CallArgs&&... args)
    {
        return pointer_invoker_<Sig>::invoke(pointer, std::forward<CallArgs>(args)...);
    }

    std::tuple<slot<Sigs>...> m_slots{};
};


// ── static_function_ref ───────────────────────────────────────────────────────
// Lightweight single-signature view over a plain function pointer.

template <class Sig>
struct static_function_ref;

template <class R, class... Args>
struct static_function_ref<R(Args...)>
{
    using pointer_type = R (*)(Args...);

    constexpr static_function_ref() noexcept = default;
    constexpr explicit static_function_ref(pointer_type pointer) noexcept : m_pointer(pointer) {}

    template <auto Function>
    static FW_CONSTEVAL static_function_ref make() noexcept
    {
        static_assert(std::is_same_v<detail::remove_cvref_t<decltype(+Function)>, pointer_type>, "fw::static_function_ref: Function must match the declared signature.");
        return static_function_ref(+Function);
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return m_pointer != nullptr;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    constexpr R operator()(Args... args) const
    {
        if (!m_pointer)
        {
            throw bad_call{};
        }
        if constexpr (std::is_void_v<R>)
        {
            m_pointer(std::forward<Args>(args)...);
        }
        else
        {
            return m_pointer(std::forward<Args>(args)...);
        }
    }

private:
    pointer_type m_pointer{ nullptr };
};

template <class Sig>
static_function_ref(Sig*) -> static_function_ref<Sig>;
} // namespace fw

#endif
