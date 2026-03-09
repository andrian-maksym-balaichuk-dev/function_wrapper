#ifndef FW_FUNCTION_REF_HPP
#define FW_FUNCTION_REF_HPP

#include <fw/call_result.hpp>
#include <fw/detail/concepts.hpp>
#include <fw/exceptions.hpp>

#include <array>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

namespace fw
{
// ── function_ref ──────────────────────────────────────────────────────────────
// A nullable, non-owning callable view over a single signature.

template <class Sig>
class function_ref;

template <class R, class... Args>
class function_ref<R(Args...)>
{
    static constexpr std::size_t k_aux_storage_size = sizeof(void*) * 2;

    using mutable_thunk_t = R (*)(const function_ref&, Args...);
    using const_thunk_t = R (*)(const function_ref&, Args...);
    using pointer_type = R (*)(Args...);

    template <class F, class Result, class... CallArgs>
    static constexpr bool callable_lvalue_bindable_v = !std::is_pointer_v<detail::remove_cvref_t<F>> &&
        !std::is_function_v<std::remove_reference_t<F>> && detail::is_exact_invocable_r_v<F, Result, CallArgs...>;

    template <class Object, class Member, class Result, class... CallArgs>
    static constexpr bool member_bindable_v = std::is_member_pointer_v<detail::remove_cvref_t<Member>> &&
        detail::is_exact_invocable_r_v<Member, Result, Object, CallArgs...>;

public:
    // ── construction and assignment ──────────────────────────────────────────

    function_ref() noexcept = default;
    function_ref(std::nullptr_t) noexcept {}

    function_ref(const function_ref&) noexcept = default;
    function_ref(function_ref&&) noexcept = default;
    function_ref& operator=(const function_ref&) noexcept = default;
    function_ref& operator=(function_ref&&) noexcept = default;

    function_ref& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    function_ref(pointer_type pointer) noexcept
    {
        bind(pointer);
    }

#if FW_HAS_CONCEPTS
    template <class F>
        requires(!std::is_const_v<F> && callable_lvalue_bindable_v<F&, R, Args...>)
    function_ref(F& f) noexcept
    {
        bind(f);
    }

    template <class F>
        requires(callable_lvalue_bindable_v<const F&, R, Args...>)
    function_ref(const F& f) noexcept
    {
        bind(f);
    }

    template <class F>
        requires(callable_lvalue_bindable_v<detail::remove_cvref_t<F>&, R, Args...>)
    function_ref(F&&) = delete;

    template <class F>
        requires(callable_lvalue_bindable_v<const detail::remove_cvref_t<F>&, R, Args...>)
    function_ref(const F&&) = delete;

    template <class Object, class Member>
        requires(!std::is_const_v<Object> && member_bindable_v<Object&, Member, R, Args...>)
    function_ref(Object& object, Member member) noexcept
    {
        bind_member(object, member);
    }

    template <class Object, class Member>
        requires(member_bindable_v<const Object&, Member, R, Args...>)
    function_ref(const Object& object, Member member) noexcept
    {
        bind_member(object, member);
    }
#else
    template <class F, std::enable_if_t<!std::is_const_v<F> && callable_lvalue_bindable_v<F&, R, Args...>, int> = 0>
    function_ref(F& f) noexcept
    {
        bind(f);
    }

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const F&, R, Args...>, int> = 0>
    function_ref(const F& f) noexcept
    {
        bind(f);
    }

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<detail::remove_cvref_t<F>&, R, Args...>, int> = 0>
    function_ref(F&&) = delete;

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const detail::remove_cvref_t<F>&, R, Args...>, int> = 0>
    function_ref(const F&&) = delete;

    template <class Object, class Member, std::enable_if_t<!std::is_const_v<Object> && member_bindable_v<Object&, Member, R, Args...>, int> = 0>
    function_ref(Object& object, Member member) noexcept
    {
        bind_member(object, member);
    }

    template <class Object, class Member, std::enable_if_t<member_bindable_v<const Object&, Member, R, Args...>, int> = 0>
    function_ref(const Object& object, Member member) noexcept
    {
        bind_member(object, member);
    }
#endif

    function_ref& operator=(pointer_type pointer) noexcept
    {
        bind(pointer);
        return *this;
    }

    template <class F, std::enable_if_t<!std::is_const_v<F> && callable_lvalue_bindable_v<F&, R, Args...>, int> = 0>
    function_ref& operator=(F& f) noexcept
    {
        bind(f);
        return *this;
    }

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const F&, R, Args...>, int> = 0>
    function_ref& operator=(const F& f) noexcept
    {
        bind(f);
        return *this;
    }

#if FW_HAS_CONCEPTS
    template <class F>
        requires(callable_lvalue_bindable_v<detail::remove_cvref_t<F>&, R, Args...>)
    function_ref& operator=(F&&) = delete;

    template <class F>
        requires(callable_lvalue_bindable_v<const detail::remove_cvref_t<F>&, R, Args...>)
    function_ref& operator=(const F&&) = delete;
#else
    template <class F, std::enable_if_t<callable_lvalue_bindable_v<detail::remove_cvref_t<F>&, R, Args...>, int> = 0>
    function_ref& operator=(F&&) = delete;

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const detail::remove_cvref_t<F>&, R, Args...>, int> = 0>
    function_ref& operator=(const F&&) = delete;
#endif

    // ── state inspection ─────────────────────────────────────────────────────

    [[nodiscard]] bool has_value() const noexcept
    {
        return mutable_thunk_ != nullptr || const_thunk_ != nullptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return has_value();
    }

    // ── invocation ───────────────────────────────────────────────────────────

    try_call_result<R> try_call(Args... args) const noexcept(noexcept(mutable_thunk_(*this, std::forward<Args>(args)...)) && noexcept(const_thunk_(*this, std::forward<Args>(args)...)))
    {
        if (mutable_thunk_)
        {
            if constexpr (std::is_void_v<R>)
            {
                mutable_thunk_(*this, std::forward<Args>(args)...);
                return try_call_result<void>::success();
            }
            else
            {
                return try_call_result<R>::success(mutable_thunk_(*this, std::forward<Args>(args)...));
            }
        }
        if (const_thunk_)
        {
            if constexpr (std::is_void_v<R>)
            {
                const_thunk_(*this, std::forward<Args>(args)...);
                return try_call_result<void>::success();
            }
            else
            {
                return try_call_result<R>::success(const_thunk_(*this, std::forward<Args>(args)...));
            }
        }
        return try_call_result<R>::empty();
    }

    R call(Args... args) const
    {
        if (mutable_thunk_)
        {
            return mutable_thunk_(*this, std::forward<Args>(args)...);
        }
        if (const_thunk_)
        {
            return const_thunk_(*this, std::forward<Args>(args)...);
        }
        throw bad_call{};
    }

    R operator()(Args... args) const
    {
        return call(std::forward<Args>(args)...);
    }

    // ── lifecycle helpers ────────────────────────────────────────────────────

    void reset() noexcept
    {
        clear_();
    }

    void swap(function_ref& other) noexcept
    {
        using std::swap;
        swap(object_, other.object_);
        swap(mutable_thunk_, other.mutable_thunk_);
        swap(const_thunk_, other.const_thunk_);
        swap(aux_, other.aux_);
    }

    friend void swap(function_ref& lhs, function_ref& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    [[nodiscard]] friend bool operator==(const function_ref& ref, std::nullptr_t) noexcept
    {
        return !ref.has_value();
    }

#if __cplusplus < 202002L && !(defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
    [[nodiscard]] friend bool operator!=(const function_ref& ref, std::nullptr_t) noexcept
    {
        return ref.has_value();
    }
    [[nodiscard]] friend bool operator==(std::nullptr_t, const function_ref& ref) noexcept
    {
        return !ref.has_value();
    }
    [[nodiscard]] friend bool operator!=(std::nullptr_t, const function_ref& ref) noexcept
    {
        return ref.has_value();
    }
#endif

private:
    // ── erased state ─────────────────────────────────────────────────────────

    void clear_() noexcept
    {
        object_ = nullptr;
        mutable_thunk_ = nullptr;
        const_thunk_ = nullptr;
        aux_.fill(std::byte{});
    }

    // Store inline metadata for cases where a raw object pointer is not enough,
    // such as function-pointer and member-pointer bindings.
    template <class T>
    void store_aux(T value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "fw::function_ref auxiliary payload must be trivially copyable.");
        static_assert(sizeof(T) <= k_aux_storage_size, "fw::function_ref auxiliary payload exceeds inline storage.");
        std::memcpy(aux_.data(), &value, sizeof(T));
    }

    template <class T>
    [[nodiscard]] T load_aux() const noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "fw::function_ref auxiliary payload must be trivially copyable.");
        T value{};
        std::memcpy(&value, aux_.data(), sizeof(T));
        return value;
    }

    // ── erased thunks ────────────────────────────────────────────────────────

    template <class F>
    static R invoke_mutable_callable(const function_ref& self, Args... args)
    {
        auto& fn = *static_cast<F*>(const_cast<void*>(self.object_));
        if constexpr (std::is_void_v<R>)
        {
            std::invoke(fn, std::forward<Args>(args)...);
            return;
        }
        else
        {
            return std::invoke(fn, std::forward<Args>(args)...);
        }
    }

    template <class F>
    static R invoke_const_callable(const function_ref& self, Args... args)
    {
        const auto& fn = *static_cast<const F*>(self.object_);
        if constexpr (std::is_void_v<R>)
        {
            std::invoke(fn, std::forward<Args>(args)...);
            return;
        }
        else
        {
            return std::invoke(fn, std::forward<Args>(args)...);
        }
    }

    static R invoke_function_pointer(const function_ref& self, Args... args)
    {
        const auto pointer = self.template load_aux<pointer_type>();
        if constexpr (std::is_void_v<R>)
        {
            pointer(std::forward<Args>(args)...);
            return;
        }
        else
        {
            return pointer(std::forward<Args>(args)...);
        }
    }

    template <class Object, class Member>
    static R invoke_mutable_member(const function_ref& self, Args... args)
    {
        auto member = self.template load_aux<Member>();
        auto& object = *static_cast<Object*>(const_cast<void*>(self.object_));
        if constexpr (std::is_void_v<R>)
        {
            std::invoke(member, object, std::forward<Args>(args)...);
            return;
        }
        else
        {
            return std::invoke(member, object, std::forward<Args>(args)...);
        }
    }

    template <class Object, class Member>
    static R invoke_const_member(const function_ref& self, Args... args)
    {
        auto member = self.template load_aux<Member>();
        const auto& object = *static_cast<const Object*>(self.object_);
        if constexpr (std::is_void_v<R>)
        {
            std::invoke(member, object, std::forward<Args>(args)...);
            return;
        }
        else
        {
            return std::invoke(member, object, std::forward<Args>(args)...);
        }
    }

    void bind(pointer_type pointer) noexcept
    {
        clear_();
        store_aux(pointer);
        mutable_thunk_ = &invoke_function_pointer;
        const_thunk_ = &invoke_function_pointer;
    }

    template <class F, std::enable_if_t<!std::is_const_v<F> && callable_lvalue_bindable_v<F&, R, Args...>, int> = 0>
    void bind(F& f) noexcept
    {
        clear_();
        object_ = std::addressof(f);
        mutable_thunk_ = &invoke_mutable_callable<F>;
    }

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const F&, R, Args...>, int> = 0>
    void bind(const F& f) noexcept
    {
        clear_();
        object_ = std::addressof(f);
        const_thunk_ = &invoke_const_callable<F>;
    }

    template <class Object, class Member, std::enable_if_t<!std::is_const_v<Object> && member_bindable_v<Object&, Member, R, Args...>, int> = 0>
    void bind_member(Object& object, Member member) noexcept
    {
        clear_();
        object_ = std::addressof(object);
        store_aux(member);
        mutable_thunk_ = &invoke_mutable_member<Object, Member>;
    }

    template <class Object, class Member, std::enable_if_t<member_bindable_v<const Object&, Member, R, Args...>, int> = 0>
    void bind_member(const Object& object, Member member) noexcept
    {
        clear_();
        object_ = std::addressof(object);
        store_aux(member);
        const_thunk_ = &invoke_const_member<Object, Member>;
    }

    const void* object_{ nullptr };
    mutable_thunk_t mutable_thunk_{ nullptr };
    const_thunk_t const_thunk_{ nullptr };
    std::array<std::byte, k_aux_storage_size> aux_{};
};

template <class R, class... Args>
class function_ref<R(Args...) noexcept>
{
    static constexpr std::size_t k_aux_storage_size = sizeof(void*) * 2;

    using mutable_thunk_t = R (*)(const function_ref&, Args...) noexcept;
    using const_thunk_t = R (*)(const function_ref&, Args...) noexcept;
    using pointer_type = R (*)(Args...) noexcept;

    template <class F, class Result, class... CallArgs>
    static constexpr bool callable_lvalue_bindable_v = !std::is_pointer_v<detail::remove_cvref_t<F>> &&
        !std::is_function_v<std::remove_reference_t<F>> && detail::is_exact_nothrow_invocable_r_v<F, Result, CallArgs...>;

    template <class Object, class Member, class Result, class... CallArgs>
    static constexpr bool member_bindable_v = std::is_member_pointer_v<detail::remove_cvref_t<Member>> &&
        detail::is_exact_nothrow_invocable_r_v<Member, Result, Object, CallArgs...>;

public:
    function_ref() noexcept = default;
    function_ref(std::nullptr_t) noexcept {}

    function_ref(const function_ref&) noexcept = default;
    function_ref(function_ref&&) noexcept = default;
    function_ref& operator=(const function_ref&) noexcept = default;
    function_ref& operator=(function_ref&&) noexcept = default;

    function_ref& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    function_ref(pointer_type pointer) noexcept
    {
        bind(pointer);
    }

#if FW_HAS_CONCEPTS
    template <class F>
        requires(!std::is_const_v<F> && callable_lvalue_bindable_v<F&, R, Args...>)
    function_ref(F& f) noexcept
    {
        bind(f);
    }

    template <class F>
        requires(callable_lvalue_bindable_v<const F&, R, Args...>)
    function_ref(const F& f) noexcept
    {
        bind(f);
    }

    template <class F>
        requires(callable_lvalue_bindable_v<detail::remove_cvref_t<F>&, R, Args...>)
    function_ref(F&&) = delete;

    template <class F>
        requires(callable_lvalue_bindable_v<const detail::remove_cvref_t<F>&, R, Args...>)
    function_ref(const F&&) = delete;

    template <class Object, class Member>
        requires(!std::is_const_v<Object> && member_bindable_v<Object&, Member, R, Args...>)
    function_ref(Object& object, Member member) noexcept
    {
        bind_member(object, member);
    }

    template <class Object, class Member>
        requires(member_bindable_v<const Object&, Member, R, Args...>)
    function_ref(const Object& object, Member member) noexcept
    {
        bind_member(object, member);
    }
#else
    template <class F, std::enable_if_t<!std::is_const_v<F> && callable_lvalue_bindable_v<F&, R, Args...>, int> = 0>
    function_ref(F& f) noexcept
    {
        bind(f);
    }

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const F&, R, Args...>, int> = 0>
    function_ref(const F& f) noexcept
    {
        bind(f);
    }

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<detail::remove_cvref_t<F>&, R, Args...>, int> = 0>
    function_ref(F&&) = delete;

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const detail::remove_cvref_t<F>&, R, Args...>, int> = 0>
    function_ref(const F&&) = delete;

    template <class Object, class Member, std::enable_if_t<!std::is_const_v<Object> && member_bindable_v<Object&, Member, R, Args...>, int> = 0>
    function_ref(Object& object, Member member) noexcept
    {
        bind_member(object, member);
    }

    template <class Object, class Member, std::enable_if_t<member_bindable_v<const Object&, Member, R, Args...>, int> = 0>
    function_ref(const Object& object, Member member) noexcept
    {
        bind_member(object, member);
    }
#endif

    function_ref& operator=(pointer_type pointer) noexcept
    {
        bind(pointer);
        return *this;
    }

    template <class F, std::enable_if_t<!std::is_const_v<F> && callable_lvalue_bindable_v<F&, R, Args...>, int> = 0>
    function_ref& operator=(F& f) noexcept
    {
        bind(f);
        return *this;
    }

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const F&, R, Args...>, int> = 0>
    function_ref& operator=(const F& f) noexcept
    {
        bind(f);
        return *this;
    }

#if FW_HAS_CONCEPTS
    template <class F>
        requires(callable_lvalue_bindable_v<detail::remove_cvref_t<F>&, R, Args...>)
    function_ref& operator=(F&&) = delete;

    template <class F>
        requires(callable_lvalue_bindable_v<const detail::remove_cvref_t<F>&, R, Args...>)
    function_ref& operator=(const F&&) = delete;
#else
    template <class F, std::enable_if_t<callable_lvalue_bindable_v<detail::remove_cvref_t<F>&, R, Args...>, int> = 0>
    function_ref& operator=(F&&) = delete;

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const detail::remove_cvref_t<F>&, R, Args...>, int> = 0>
    function_ref& operator=(const F&&) = delete;
#endif

    [[nodiscard]] bool has_value() const noexcept
    {
        return mutable_thunk_ != nullptr || const_thunk_ != nullptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return has_value();
    }

    try_call_result<R> try_call(Args... args) const noexcept
    {
        if (mutable_thunk_)
        {
            if constexpr (std::is_void_v<R>)
            {
                mutable_thunk_(*this, std::forward<Args>(args)...);
                return try_call_result<void>::success();
            }
            else
            {
                return try_call_result<R>::success(mutable_thunk_(*this, std::forward<Args>(args)...));
            }
        }
        if (const_thunk_)
        {
            if constexpr (std::is_void_v<R>)
            {
                const_thunk_(*this, std::forward<Args>(args)...);
                return try_call_result<void>::success();
            }
            else
            {
                return try_call_result<R>::success(const_thunk_(*this, std::forward<Args>(args)...));
            }
        }
        return try_call_result<R>::empty();
    }

    R call(Args... args) const
    {
        if (mutable_thunk_)
        {
            return mutable_thunk_(*this, std::forward<Args>(args)...);
        }
        if (const_thunk_)
        {
            return const_thunk_(*this, std::forward<Args>(args)...);
        }
        throw bad_call{};
    }

    R operator()(Args... args) const
    {
        return call(std::forward<Args>(args)...);
    }

    void reset() noexcept
    {
        clear_();
    }

    void swap(function_ref& other) noexcept
    {
        using std::swap;
        swap(object_, other.object_);
        swap(mutable_thunk_, other.mutable_thunk_);
        swap(const_thunk_, other.const_thunk_);
        swap(aux_, other.aux_);
    }

    friend void swap(function_ref& lhs, function_ref& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    [[nodiscard]] friend bool operator==(const function_ref& ref, std::nullptr_t) noexcept
    {
        return !ref.has_value();
    }

#if __cplusplus < 202002L && !(defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
    [[nodiscard]] friend bool operator!=(const function_ref& ref, std::nullptr_t) noexcept
    {
        return ref.has_value();
    }
    [[nodiscard]] friend bool operator==(std::nullptr_t, const function_ref& ref) noexcept
    {
        return !ref.has_value();
    }
    [[nodiscard]] friend bool operator!=(std::nullptr_t, const function_ref& ref) noexcept
    {
        return ref.has_value();
    }
#endif

private:
    void clear_() noexcept
    {
        object_ = nullptr;
        mutable_thunk_ = nullptr;
        const_thunk_ = nullptr;
        aux_.fill(std::byte{});
    }

    // Store inline metadata for cases where a raw object pointer is not enough,
    // such as function-pointer and member-pointer bindings.
    template <class T>
    void store_aux(T value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "fw::function_ref auxiliary payload must be trivially copyable.");
        static_assert(sizeof(T) <= k_aux_storage_size, "fw::function_ref auxiliary payload exceeds inline storage.");
        std::memcpy(aux_.data(), &value, sizeof(T));
    }

    template <class T>
    [[nodiscard]] T load_aux() const noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "fw::function_ref auxiliary payload must be trivially copyable.");
        T value{};
        std::memcpy(&value, aux_.data(), sizeof(T));
        return value;
    }

    template <class F>
    static R invoke_mutable_callable(const function_ref& self, Args... args) noexcept
    {
        auto& fn = *static_cast<F*>(const_cast<void*>(self.object_));
        if constexpr (std::is_void_v<R>)
        {
            std::invoke(fn, std::forward<Args>(args)...);
            return;
        }
        else
        {
            return std::invoke(fn, std::forward<Args>(args)...);
        }
    }

    template <class F>
    static R invoke_const_callable(const function_ref& self, Args... args) noexcept
    {
        const auto& fn = *static_cast<const F*>(self.object_);
        if constexpr (std::is_void_v<R>)
        {
            std::invoke(fn, std::forward<Args>(args)...);
            return;
        }
        else
        {
            return std::invoke(fn, std::forward<Args>(args)...);
        }
    }

    static R invoke_function_pointer(const function_ref& self, Args... args) noexcept
    {
        const auto pointer = self.template load_aux<pointer_type>();
        if constexpr (std::is_void_v<R>)
        {
            pointer(std::forward<Args>(args)...);
            return;
        }
        else
        {
            return pointer(std::forward<Args>(args)...);
        }
    }

    template <class Object, class Member>
    static R invoke_mutable_member(const function_ref& self, Args... args) noexcept
    {
        auto member = self.template load_aux<Member>();
        auto& object = *static_cast<Object*>(const_cast<void*>(self.object_));
        if constexpr (std::is_void_v<R>)
        {
            std::invoke(member, object, std::forward<Args>(args)...);
            return;
        }
        else
        {
            return std::invoke(member, object, std::forward<Args>(args)...);
        }
    }

    template <class Object, class Member>
    static R invoke_const_member(const function_ref& self, Args... args) noexcept
    {
        auto member = self.template load_aux<Member>();
        const auto& object = *static_cast<const Object*>(self.object_);
        if constexpr (std::is_void_v<R>)
        {
            std::invoke(member, object, std::forward<Args>(args)...);
            return;
        }
        else
        {
            return std::invoke(member, object, std::forward<Args>(args)...);
        }
    }

    void bind(pointer_type pointer) noexcept
    {
        clear_();
        store_aux(pointer);
        mutable_thunk_ = &invoke_function_pointer;
        const_thunk_ = &invoke_function_pointer;
    }

    template <class F, std::enable_if_t<!std::is_const_v<F> && callable_lvalue_bindable_v<F&, R, Args...>, int> = 0>
    void bind(F& f) noexcept
    {
        clear_();
        object_ = std::addressof(f);
        mutable_thunk_ = &invoke_mutable_callable<F>;
    }

    template <class F, std::enable_if_t<callable_lvalue_bindable_v<const F&, R, Args...>, int> = 0>
    void bind(const F& f) noexcept
    {
        clear_();
        object_ = std::addressof(f);
        const_thunk_ = &invoke_const_callable<F>;
    }

    template <class Object, class Member, std::enable_if_t<!std::is_const_v<Object> && member_bindable_v<Object&, Member, R, Args...>, int> = 0>
    void bind_member(Object& object, Member member) noexcept
    {
        clear_();
        object_ = std::addressof(object);
        store_aux(member);
        mutable_thunk_ = &invoke_mutable_member<Object, Member>;
    }

    template <class Object, class Member, std::enable_if_t<member_bindable_v<const Object&, Member, R, Args...>, int> = 0>
    void bind_member(const Object& object, Member member) noexcept
    {
        clear_();
        object_ = std::addressof(object);
        store_aux(member);
        const_thunk_ = &invoke_const_member<Object, Member>;
    }

    const void* object_{ nullptr };
    mutable_thunk_t mutable_thunk_{ nullptr };
    const_thunk_t const_thunk_{ nullptr };
    std::array<std::byte, k_aux_storage_size> aux_{};
};

template <class Sig>
function_ref(Sig*) -> function_ref<Sig>;

template <class F>
function_ref(F&) -> function_ref<detail::fn_sig_t<F>>;

template <class F>
function_ref(const F&) -> function_ref<detail::fn_sig_t<F>>;
} // namespace fw

#endif // FW_FUNCTION_REF_HPP
