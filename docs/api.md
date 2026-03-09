# API Reference

This document covers the complete public API of `fw`.

---

## Headers

```cpp
#include <fw/call_result.hpp>                  // try_call_status, try_call_result<R>
#include <fw/function_ref.hpp>                 // function_ref<R(Args...)>
#include <fw/member_adapter.hpp>               // member_ref(object, member)
#include <fw/static_function.hpp>              // static_function<Sigs...>, static_function_ref<R(Args...)>
#include <fw/function_wrapper.hpp>            // function_wrapper<Policy, Sigs...>, make_function_array
#include <fw/move_only_function_wrapper.hpp>  // move_only_function_wrapper<Policy, Sigs...>, make_move_only_function_array
#include <fw/exceptions.hpp>                  // bad_call, bad_signature_call
```

Do not include headers under `fw/detail/`. They are internal implementation details with no stability guarantee.

---

## `fw::function_wrapper<Sigs...>` or `fw::function_wrapper<Policy, Sigs...>`

```cpp
template <class Policy, class... Sigs>
class function_wrapper;
```

A type-erased callable wrapper that stores one callable and exposes it through one or more declared call signatures.

If present, `Policy` must be a storage policy such as `fw::policy::sbo<48>`. If omitted, `fw::policy::default_policy` is used.
`Sigs...` must be function signatures of the form `R(Args...)` or `R(Args...) noexcept`. At least one signature is required.
`function_wrapper` rejects signature sets that contain both `R(Args...)` and `R(Args...) noexcept` for the same argument list.

```cpp
fw::function_wrapper<int(int, int)>                              // one signature, default policy
fw::function_wrapper<int(int, int), float(float, float)>         // two signatures, default policy
fw::function_wrapper<int(int) noexcept, double(double)>          // mixed noexcept and non-noexcept
fw::function_wrapper<fw::policy::sbo<48>, int(int), double(double)> // explicit custom policy
```

---

### Construction

#### Default constructor

```cpp
function_wrapper() noexcept;
```

Constructs an empty wrapper. `has_value()` returns `false`. Calling an empty wrapper throws `fw::bad_call`.

#### Callable constructor

```cpp
template <class F>
function_wrapper(F&& f);
```

Stores a copy of `f`. `F` must be copy-constructible and callable through at least one declared signature.
Binding to a declared `noexcept` signature additionally requires `F` to be nothrow-invocable for that exact signature.

Fails to compile if:
- `F` is not copy-constructible.
- `F` is not callable through any of `Sigs...`.

Small callables (function pointers, small lambdas) are stored in the inline SBO buffer. Larger callables are heap-allocated.

#### Copy constructor

```cpp
function_wrapper(const function_wrapper& other);
```

Deep-copies the stored callable from `other`.

#### Move constructor

```cpp
function_wrapper(function_wrapper&& other) noexcept;
```

Transfers the stored callable from `other`. `other` is left empty.

---

### Assignment

#### Copy assignment

```cpp
function_wrapper& operator=(const function_wrapper& rhs);
```

#### Move assignment

```cpp
function_wrapper& operator=(function_wrapper&& rhs) noexcept;
```

#### Callable assignment

```cpp
template <class F>
function_wrapper& operator=(F&& f);
```

Resets the current stored callable and stores a copy of `f`. Same requirements as the callable constructor.

---

### Invocation

#### `operator()`

```cpp
template <class... CallArgs>
decltype(auto) operator()(CallArgs&&... args) &;

template <class... CallArgs>
decltype(auto) operator()(CallArgs&&... args) const&;

template <class... CallArgs>
decltype(auto) operator()(CallArgs&&... args) &&;
```

Dispatches the call to the best matching declared signature. Signature selection happens at compile time. The return type is the return type of the selected signature.

Throws `fw::bad_call` if the wrapper is empty.

Fails to compile if:
- No declared signature can accept `CallArgs...`.
- The call is ambiguous across two or more equally ranked signatures.

#### `call(...)`

```cpp
template <class... CallArgs>
decltype(auto) call(CallArgs&&... args) &;

template <class... CallArgs>
decltype(auto) call(CallArgs&&... args) const&;

template <class... CallArgs>
decltype(auto) call(CallArgs&&... args) &&;
```

Identical to `operator()`. Provided for contexts where explicit call syntax is preferred over operator syntax.

#### `try_call(...)`

```cpp
template <class... CallArgs>
auto try_call(CallArgs&&... args) &;

template <class... CallArgs>
auto try_call(CallArgs&&... args) const&;

template <class... CallArgs>
auto try_call(CallArgs&&... args) &&;
```

Dispatches through the same compile-time signature selection as `call(...)`, but returns `fw::try_call_result<R>` instead of throwing `fw::bad_call` or `fw::bad_signature_call`.

- `status() == fw::try_call_status::Success` means the call completed and a value is available.
- `status() == fw::try_call_status::Empty` means the wrapper had no stored callable.
- `status() == fw::try_call_status::SignatureMismatch` means the selected signature had no live slot for the current value category.

If the bound target itself throws through a non-`noexcept` signature, that exception still propagates.

---

### State

#### `has_value`

```cpp
[[nodiscard]] bool has_value() const noexcept;
```

Returns `true` if a callable is stored, `false` if the wrapper is empty.

#### `operator bool`

```cpp
[[nodiscard]] explicit operator bool() const noexcept;
```

Returns `has_value()`.

#### Null comparison

```cpp
[[nodiscard]] friend bool operator==(const function_wrapper& w, std::nullptr_t) noexcept;
```

Returns `!w.has_value()`. Symmetric forms and `operator!=` are provided for pre-C++20 compatibility.

---

### Lifecycle

#### `reset`

```cpp
void reset() noexcept;
```

Destroys the stored callable. The wrapper becomes empty.

#### `swap`

```cpp
void swap(function_wrapper& other) noexcept;
```

Swaps the stored callable with `other`. Self-swap is a no-op.

#### Free `swap`

```cpp
friend void swap(function_wrapper& lhs, function_wrapper& rhs) noexcept;
```

Calls `lhs.swap(rhs)`. Participates in ADL.

#### Destructor

```cpp
~function_wrapper() noexcept;
```

Destroys the stored callable.

---

### Introspection

#### `target_type`

```cpp
[[nodiscard]] const std::type_info& target_type() const noexcept;
```

Returns the `std::type_info` of the stored callable type, or `typeid(void)` if the wrapper is empty.

#### `target<T>`

```cpp
template <class T>
[[nodiscard]] T* target() noexcept;

template <class T>
[[nodiscard]] const T* target() const noexcept;
```

Returns a pointer to the stored callable if its type is exactly `T`, otherwise `nullptr`.

#### Member adapters

```cpp
auto bound = fw::member_ref(object, &Type::member_function);
auto field = fw::member_ref(object, &Type::member_object);
```

`fw::member_ref(...)` returns a small non-owning callable adapter that stores an object pointer plus a member pointer.
It can be passed directly to `function_wrapper`, `move_only_function_wrapper`, `make_function_array`, or used as a normal callable object.

#### `contains_signature<Sig>()`

```cpp
template <class Sig>
[[nodiscard]] static constexpr bool contains_signature() noexcept;
```

Returns `true` if `Sig` appears in the wrapper's declared signature set.

#### `has_bound_signature<Sig>()`

```cpp
template <class Sig>
[[nodiscard]] bool has_bound_signature() const noexcept;
```

Returns `true` if the wrapper currently stores a callable and that callable has at least one live call slot for `Sig`.
Returns `false` for empty wrappers and for undeclared signatures.

#### `bound_signatures()`

```cpp
[[nodiscard]] std::array<bool, N> bound_signatures() const noexcept;
```

Returns one boolean per declared signature, in declaration order. Each entry reports whether the currently stored callable is bound for that signature.

---

## `fw::function_ref<Sig>`

```cpp
template <class Sig>
class function_ref;
```

A nullable, non-owning callable view over one signature. `Sig` may be either `R(Args...)` or `R(Args...) noexcept`.

### Construction and assignment

```cpp
function_ref() noexcept;
function_ref(std::nullptr_t) noexcept;
function_ref(R (*pointer)(Args...)) noexcept;
function_ref(R (*pointer)(Args...) noexcept) noexcept;

template <class F>
function_ref(F& f) noexcept;

template <class F>
function_ref(const F& f) noexcept;
```

`function_ref` binds lvalue callables only. Temporary callable objects are rejected to avoid dangling references.
For `function_ref<R(Args...) noexcept>`, the bound callable or member adapter must be nothrow-invocable.

The class is copyable and movable as a cheap view type. `reset()`, `swap()`, `has_value()`, `operator bool`, and comparison with `nullptr` are provided.

### Invocation

```cpp
auto try_call(Args... args) const;
R operator()(Args... args) const;
R call(Args... args) const;
```

`try_call(...)` returns `fw::try_call_result<R>` and reports an empty reference as `fw::try_call_status::Empty`.
`operator()` and `call(...)` dispatch directly to the bound callable and throw `fw::bad_call` on an empty reference.

### Member adapters

```cpp
fw::function_ref<int(int)> ref(object, &Type::member_function);
fw::function_ref<int&()> member_ref(object, &Type::member_object);
```

These constructors dispatch through `std::invoke` without allocating or materializing a wrapper lambda.

### Safety

`function_ref` does not extend lifetimes. If the bound callable or object goes out of scope, the view becomes invalid.
Even for `noexcept` signatures, `function_ref` remains nullable, so an empty call still throws `fw::bad_call`.

---

## `fw::move_only_function_wrapper<Sigs...>` or `fw::move_only_function_wrapper<Policy, Sigs...>`

```cpp
template <class Policy, class... Sigs>
class move_only_function_wrapper;
```

A type-erased callable wrapper with the same dispatch, SBO, null-state, and introspection model as `fw::function_wrapper`, but with move-only ownership semantics.

If present, `Policy` must be a storage policy such as `fw::policy::sbo<48>`. If omitted, `fw::policy::default_policy` is used.
`Sigs...` must be function signatures of the form `R(Args...)` or `R(Args...) noexcept`. At least one signature is required.
`move_only_function_wrapper` rejects signature sets that contain both `R(Args...)` and `R(Args...) noexcept` for the same argument list.

### Construction and assignment

```cpp
move_only_function_wrapper() noexcept;

template <class F>
move_only_function_wrapper(F&& f);

move_only_function_wrapper(move_only_function_wrapper&& other) noexcept;
move_only_function_wrapper& operator=(move_only_function_wrapper&& rhs) noexcept;
```

The callable constructor stores `std::decay_t<F>`. The stored callable must be move-constructible and callable through at least one declared signature.
Binding to a declared `noexcept` signature additionally requires a nothrow-invocable target.

Copy construction and copy assignment are deleted.

### Invocation, state, lifecycle, and introspection

`move_only_function_wrapper` provides the same member surface as `function_wrapper` for:

- `operator()` / `call(...)`
- `try_call(...)`
- `has_value()` / `operator bool`
- comparison with `nullptr`
- `reset()` / `swap()`
- `target_type()` / `target<T>()`
- `contains_signature<Sig>()`
- `has_bound_signature<Sig>()`
- `bound_signatures()`

Behavior is the same as `function_wrapper`, except ownership transfer is move-only.

```cpp
#include <memory>

fw::move_only_function_wrapper<int(int)> w =
    [state = std::make_unique<int>(5)](int x) { return x + *state; };

int result = w(7); // 12
```

---

## `fw::make_function_array`

```cpp
template <class Policy = fw::policy::default_policy, class... Fs>
auto make_function_array(Fs&&... fs);
```

Constructs a `std::array<function_wrapper<Policy, Sigs...>, N>` where:

- `N` is `sizeof...(Fs)`.
- `Sigs...` is the deduplicated union of the deduced signature of each `F` in `Fs...`.

The deduction guide for `function_wrapper` is used on each callable to determine its signature.

```cpp
auto arr = fw::make_function_array(
    [](int x) { return x * 2; },
    [](int x) { return x + 1; }
);
// arr has type std::array<fw::function_wrapper<int(int)>, 2>
```

Requires at least one callable. Fails to compile if no callables are provided.

---

## `fw::make_move_only_function_array`

```cpp
template <class Policy = fw::policy::default_policy, class... Fs>
auto make_move_only_function_array(Fs&&... fs);
```

Constructs a `std::array<move_only_function_wrapper<Policy, Sigs...>, N>` where:

- `N` is `sizeof...(Fs)`.
- `Sigs...` is the deduplicated union of the deduced signature of each `F` in `Fs...`.

Each callable is forwarded into its destination wrapper, so move-only callables are supported.

```cpp
#include <memory>

auto arr = fw::make_move_only_function_array(
    [state = std::make_unique<int>(1)](int x) { return x + *state; },
    [state = std::make_unique<double>(2.0)](double x) { return x * *state; }
);
```

Requires at least one callable. Fails to compile if no callables are provided.

---

## Exception Types

## `fw::try_call_status`

```cpp
enum class try_call_status : unsigned char
{
    Success,
    Empty,
    SignatureMismatch
};
```

The status code returned by the library's non-throwing `try_call(...)` APIs.

## `fw::try_call_result<R>`

```cpp
template <class R>
class try_call_result;
```

Returned by `try_call(...)` on wrappers, refs, and static callables.

Common observers:

- `status()`
- `has_value()`
- `operator bool`
- `value_ptr()`
- `value()`

`try_call_result<void>` reports status only.

### `fw::bad_call`

```cpp
class bad_call : public std::exception;
```

Thrown when `operator()` or `call()` is invoked on an empty wrapper.

`what()` returns a stable string describing the error.

```cpp
fw::function_wrapper<int(int)> w;
try {
    w(5);
} catch (const fw::bad_call& e) {
    // e.what() → "fw::function_wrapper: called on an empty wrapper"
}
```

### `fw::bad_signature_call`

```cpp
class bad_signature_call : public std::exception;
```

Thrown when a runtime dispatch through the vtable finds no callable entry for the selected signature. This is an internal safety net — in normal usage the compile-time signature check prevents this.

---

## Dispatch Policy

When a call could match more than one declared signature, `fw` applies the following ranking, in order of preference:

| Rank | Conversion category |
|---|---|
| 1 | Exact match (identical decayed types) |
| 2 | Reference or cv-qualification binding |
| 3 | Integral and floating-point promotions |
| 4 | Arithmetic conversions (common numeric target preference) |
| 5 | C-string to `std::string_view` / `std::string` |
| 6 | Class hierarchy (derived-to-base) and pointer hierarchy |
| 7 | Single-step user-defined implicit conversion |

**Boundaries:**

- Explicit conversions are never applied implicitly.
- Chained user-defined conversions are not performed.
- If two signatures rank equally, the call is ambiguous and fails at compile time.

---

## Deduction Guide

```cpp
template <class Sig>
function_ref(Sig*) -> function_ref<Sig>;

template <class F>
function_ref(F&) -> function_ref<detail::fn_sig_t<F>>;

template <class F>
function_wrapper(F) -> function_wrapper<fw::policy::default_policy, detail::fn_sig_t<F>>;

template <class F>
move_only_function_wrapper(F) -> move_only_function_wrapper<fw::policy::default_policy, detail::fn_sig_t<F>>;
```

Allows the callable view/wrapper types to deduce their signature from a plain function pointer or a lambda with a single, non-overloaded `operator()`:

```cpp
#include <memory>

int add(int a, int b) { return a + b; }
fw::function_ref ref = &add;
// deduces fw::function_ref<int(int, int)>

fw::function_wrapper w = add;
// deduces fw::function_wrapper<int(int, int)>

fw::move_only_function_wrapper mw =
    [state = std::make_unique<int>(3)](int x) { return x + *state; };
// deduces fw::move_only_function_wrapper<int(int)>
```

Note: generic lambdas with `auto` parameters cannot be deduced this way because their `operator()` is a template. Provide explicit signatures in that case.

For free functions and non-generic callables, deduction preserves `noexcept`.

---

## SBO Size

Small callables are stored in an inline buffer without heap allocation. The buffer is sized to hold a function pointer plus pointer-sized captured state. Larger callables are heap-allocated transparently.

Owning wrappers take their SBO size from the leading policy type. `fw::policy::default_policy` preserves the library default, and custom per-wrapper policies can be declared with `fw::policy::sbo<N>`.
