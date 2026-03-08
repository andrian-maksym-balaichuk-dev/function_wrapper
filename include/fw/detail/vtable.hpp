#ifndef FW_DETAIL_VTABLE_HPP
#define FW_DETAIL_VTABLE_HPP

#include <fw/detail/concepts.hpp>

#include <functional>
#include <memory>
#include <new>
#include <typeinfo>
#include <utility>

namespace fw::detail
{
// ── storage kind ──────────────────────────────────────────────────────────────
// Indicates whether the wrapper is currently empty, using SBO, or using heap storage.
enum class storage_kind : unsigned char
{
    Empty,
    Small,
    Heap
};


// ── per-signature vtable entry ─────────────────────────────────────────────────
// Three function pointers — one per value category of the stored object.

template <class Sig>
struct signature_vtable_entry;

template <class R, class... Args>
struct signature_vtable_entry<R(Args...)>
{
    using lcall_t = R (*)(void*, Args...);        // lvalue ref
    using clcall_t = R (*)(const void*, Args...); // const lvalue ref
    using rcall_t = R (*)(void*, Args...);        // rvalue ref (ptr itself mutable)

    lcall_t lcall{ nullptr };
    clcall_t clcall{ nullptr };
    rcall_t rcall{ nullptr };
};


// ── forward declaration ────────────────────────────────────────────────────────
// The wrapper_vtable struct is defined after wrapper_storage since it needs to refer to it for the lifecycle function
// pointers, but wrapper_storage needs to refer to wrapper_vtable for its vtable pointer.

template <class... Sigs>
struct wrapper_vtable;

template <class T, class... Args>
T* fw_construct(void* where, Args&&... args);


// ── SBO + heap storage ─────────────────────────────────────────────────────────
// The wrapper_storage struct contains the actual storage for the wrapped object, either in an internal buffer (SBO) or
// on the heap, as well as a pointer to the appropriate vtable for the currently stored type. The wrapper's lifecycle

template <class... Sigs>
struct wrapper_storage
{
    union payload_t
    {
        alignas(std::max_align_t) unsigned char sbo[FW_SBO_SIZE];
        void* heap;
        constexpr payload_t() noexcept : heap(nullptr) {}
    };

    using vtable_type = wrapper_vtable<Sigs...>;

    storage_kind kind{ storage_kind::Empty };
    const vtable_type* vt{ nullptr };
    payload_t payload{};
};


// ── vtable (lifecycle + per-signature call slots) ──────────────────────────────
// The wrapper_vtable struct contains the function pointers for all supported signatures as well as the lifecycle functions (destroy, copy, move) and pointer accessors (get_ptr, get_cptr).

template <class... Sigs>
struct wrapper_vtable : signature_vtable_entry<Sigs>...
{
    using storage_type = wrapper_storage<Sigs...>;

    using destroy_t = void (*)(storage_type&) noexcept;
    using copy_t = void (*)(storage_type&, const storage_type&);
    using move_t = void (*)(storage_type&, storage_type&) noexcept;
    using get_ptr_t = void* (*)(storage_type&) noexcept;
    using get_cptr_t = const void* (*)(const storage_type&) noexcept;

    constexpr wrapper_vtable(signature_vtable_entry<Sigs>... entries, const std::type_info* stored_type, destroy_t d, copy_t c, move_t m, get_ptr_t gp, get_cptr_t gcp) noexcept
    : signature_vtable_entry<Sigs>(entries)..., type(stored_type), destroy(d), copy(c), move(m), get_ptr(gp), get_cptr(gcp)
    {
        // no runtime initialization needed since this is a POD aggregate and all members are initialized by the initializer list
    }

    const std::type_info* type;
    destroy_t destroy;
    copy_t copy;
    move_t move;
    get_ptr_t get_ptr;
    get_cptr_t get_cptr;
};


// ── SBO pointer helpers ────────────────────────────────────────────────────────
// Get a typed pointer to the object stored in the SBO buffer, given a wrapper_storage instance. The caller must ensure that the storage kind is Small.

template <class Storage, class T>
[[nodiscard]] T* small_ptr(Storage& s) noexcept
{
    return std::launder(reinterpret_cast<T*>(s.payload.sbo));
}

template <class Storage, class T>
[[nodiscard]] const T* small_ptr(const Storage& s) noexcept
{
    return std::launder(reinterpret_cast<const T*>(s.payload.sbo));
}


// ── signature support check ────────────────────────────────────────────────────
// Checks whether a given stored type supports being called with a particular signature in any of the three value
// categories. Used to determine which call slots to fill in the vtable and whether a signature is supported at all.

template <class Stored, class Sig>
struct supports_signature;

template <class Stored, class R, class... Args>
struct supports_signature<Stored, R(Args...)>
: std::bool_constant<is_exact_invocable_r_v<Stored&, R, Args...> || is_exact_invocable_r_v<const Stored&, R, Args...> || is_exact_invocable_r_v<Stored&&, R, Args...>>
{};

template <class Stored, class... Sigs>
inline constexpr bool supports_any_signature_v = (supports_signature<Stored, Sigs>::value || ...);

// ── invoke thunks (void* → typed call) ────────────────────────────────────────
// These functions are used as the call slots in the vtable. They take a void* pointer to the stored object and the call arguments,
//  cast the pointer to the correct type, and invoke the function. There is one for each value category of the stored object.

template <class Stored, class R, class... Args>
R invoke_l(void* ptr, Args... args)
{
    auto& fn = *static_cast<Stored*>(ptr);
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

template <class Stored, class R, class... Args>
R invoke_cl(const void* ptr, Args... args)
{
    const auto& fn = *static_cast<const Stored*>(ptr);
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

template <class Stored, class R, class... Args>
R invoke_r(void* ptr, Args... args)
{
    if constexpr (std::is_void_v<R>)
    {
        std::invoke(std::move(*static_cast<Stored*>(ptr)), std::forward<Args>(args)...);
        return;
    }
    else
    {
        return std::invoke(std::move(*static_cast<Stored*>(ptr)), std::forward<Args>(args)...);
    }
}

// ── vtable entry factory ───────────────────────────────────────────────────────
// Fills call slots at compile time — only assigns slots the stored type supports.

template <class Stored, class Sig>
struct signature_entry_factory;

template <class Stored, class R, class... Args>
struct signature_entry_factory<Stored, R(Args...)>
{
    static signature_vtable_entry<R(Args...)> make() noexcept
    {
        signature_vtable_entry<R(Args...)> e{};

        if constexpr (is_exact_invocable_r_v<Stored&, R, Args...>)
        {
            e.lcall = &invoke_l<Stored, R, Args...>;
        }
        if constexpr (is_exact_invocable_r_v<const Stored&, R, Args...>)
        {
            e.clcall = &invoke_cl<Stored, R, Args...>;
        }
        if constexpr (is_exact_invocable_r_v<Stored&&, R, Args...>)
        {
            e.rcall = &invoke_r<Stored, R, Args...>;
        }

        return e;
    }
};

// ── vtable singleton ───────────────────────────────────────────────────────────
// The vtable_instance struct provides the actual function implementations for the lifecycle and call slots in the vtable for a
// given stored type. It is instantiated once per stored type and signature set, and its get() function returns a pointer to the corresponding vtable.

template <class Stored, class... Sigs>
struct vtable_instance
{
    using storage_type = wrapper_storage<Sigs...>;
    using vtable_type = wrapper_vtable<Sigs...>;

    static void destroy(storage_type& s) noexcept
    {
        switch (s.kind)
        {
        case storage_kind::Small: {
            std::destroy_at(small_ptr<storage_type, Stored>(s));
            break;
        }
        case storage_kind::Heap: {
            delete static_cast<Stored*>(s.payload.heap);
            s.payload.heap = nullptr;
            break;
        }
        case storage_kind::Empty: {
            break;
        }
        default: {
            return;
        }
        }

        s.kind = storage_kind::Empty;
        s.vt = nullptr;
    }

    static void copy(storage_type& dst, const storage_type& src)
    {
        static_assert(std::is_copy_constructible_v<Stored>, "fw::detail::vtable_instance::copy requires a copy-constructible stored type.");

        switch (src.kind)
        {
        case storage_kind::Small: {
            const Stored& obj = *small_ptr<storage_type, Stored>(src);
            fw_construct<Stored>(dst.payload.sbo, obj);
            dst.kind = storage_kind::Small;
            break;
        }
        case storage_kind::Heap: {
            dst.payload.heap = new Stored(*static_cast<const Stored*>(src.payload.heap));
            dst.kind = storage_kind::Heap;
            break;
        }
        case storage_kind::Empty: {
            dst.kind = storage_kind::Empty;
            break;
        }
        default: {
            return;
        }
        }

        dst.vt = src.vt;
    }

    [[nodiscard]] static constexpr typename vtable_type::copy_t copy_fn() noexcept
    {
        if constexpr (std::is_copy_constructible_v<Stored>)
        {
            return &copy;
        }
        else
        {
            return nullptr;
        }
    }

    static void move(storage_type& dst, storage_type& src) noexcept
    {
        switch (src.kind)
        {
        case storage_kind::Small: {
            Stored& obj = *small_ptr<storage_type, Stored>(src);
            fw_construct<Stored>(dst.payload.sbo, std::move(obj));
            std::destroy_at(&obj);
            dst.kind = storage_kind::Small;
            break;
        }
        case storage_kind::Heap: {
            dst.payload.heap = src.payload.heap;
            src.payload.heap = nullptr;
            dst.kind = storage_kind::Heap;
            break;
        }
        case storage_kind::Empty: {
            dst.kind = storage_kind::Empty;
            break;
        }
        default: {
            return;
        }
        }

        dst.vt = src.vt;
        src.kind = storage_kind::Empty;
        src.vt = nullptr;
    }

    [[nodiscard]] static void* get_ptr(storage_type& s) noexcept
    {
        switch (s.kind)
        {
        case storage_kind::Small: return small_ptr<storage_type, Stored>(s);
        case storage_kind::Heap: return s.payload.heap;
        default: return nullptr;
        }
    }

    [[nodiscard]] static const void* get_cptr(const storage_type& s) noexcept
    {
        switch (s.kind)
        {
        case storage_kind::Small: return small_ptr<storage_type, Stored>(s);
        case storage_kind::Heap: return s.payload.heap;
        default: return nullptr;
        }
    }

    [[nodiscard]] static const vtable_type* get() noexcept
    {
        static const vtable_type table{ signature_entry_factory<Stored, Sigs>::make()...,
                                        &typeid(Stored),
                                        &destroy,
                                        copy_fn(),
                                        &move,
                                        &get_ptr,
                                        &get_cptr };
        return &table;
    }
};

// ── SBO construct helper ───────────────────────────────────────────────────────

template <class T, class... Args>
T* fw_construct(void* where, Args&&... args)
{
#if FW_HAS_CONSTRUCT_AT
    return std::construct_at(static_cast<T*>(where), std::forward<Args>(args)...);
#else
    return ::new (where) T(std::forward<Args>(args)...);
#endif
}
} // namespace fw::detail

#endif // FW_DETAIL_VTABLE_HPP
