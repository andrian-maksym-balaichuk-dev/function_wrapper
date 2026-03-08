# API Reference

This document covers the complete public API of `fw`.

---

## Headers

```cpp
#include <fw/function_wrapper.hpp>            // function_wrapper<Sigs...>, make_function_array
#include <fw/move_only_function_wrapper.hpp>  // move_only_function_wrapper<Sigs...>, make_move_only_function_array
#include <fw/exceptions.hpp>                  // bad_call, bad_signature_call
```

Do not include headers under `fw/detail/`. They are internal implementation details with no stability guarantee.

---

## `fw::function_wrapper<Sigs...>`

```cpp
template <class... Sigs>
class function_wrapper;
```

A type-erased callable wrapper that stores one callable and exposes it through one or more declared call signatures.

`Sigs...` must be plain function signatures of the form `R(Args...)`. At least one signature is required.

```cpp
fw::function_wrapper<int(int, int)>                              // one signature
fw::function_wrapper<int(int, int), float(float, float)>         // two signatures
fw::function_wrapper<int(int), float(float), double(double)>     // three signatures
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

---

## `fw::move_only_function_wrapper<Sigs...>`

```cpp
template <class... Sigs>
class move_only_function_wrapper;
```

A type-erased callable wrapper with the same dispatch, SBO, null-state, and introspection model as `fw::function_wrapper`, but with move-only ownership semantics.

`Sigs...` must be plain function signatures of the form `R(Args...)`. At least one signature is required.

### Construction and assignment

```cpp
move_only_function_wrapper() noexcept;

template <class F>
move_only_function_wrapper(F&& f);

move_only_function_wrapper(move_only_function_wrapper&& other) noexcept;
move_only_function_wrapper& operator=(move_only_function_wrapper&& rhs) noexcept;
```

The callable constructor stores `std::decay_t<F>`. The stored callable must be move-constructible and callable through at least one declared signature.

Copy construction and copy assignment are deleted.

### Invocation, state, lifecycle, and introspection

`move_only_function_wrapper` provides the same member surface as `function_wrapper` for:

- `operator()` / `call(...)`
- `has_value()` / `operator bool`
- comparison with `nullptr`
- `reset()` / `swap()`
- `target_type()` / `target<T>()`

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
template <class... Fs>
auto make_function_array(Fs&&... fs);
```

Constructs a `std::array<function_wrapper<Sigs...>, N>` where:

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
template <class... Fs>
auto make_move_only_function_array(Fs&&... fs);
```

Constructs a `std::array<move_only_function_wrapper<Sigs...>, N>` where:

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
template <class F>
function_wrapper(F) -> function_wrapper<detail::fn_sig_t<F>>;

template <class F>
move_only_function_wrapper(F) -> move_only_function_wrapper<detail::fn_sig_t<F>>;
```

Allows both wrapper types to deduce their signature from a plain function pointer or a lambda with a single, non-overloaded `operator()`:

```cpp
#include <memory>

int add(int a, int b) { return a + b; }
fw::function_wrapper w = add;
// deduces fw::function_wrapper<int(int, int)>

fw::move_only_function_wrapper mw =
    [state = std::make_unique<int>(3)](int x) { return x + *state; };
// deduces fw::move_only_function_wrapper<int(int)>
```

Note: generic lambdas with `auto` parameters cannot be deduced this way because their `operator()` is a template. Provide explicit signatures in that case.

---

## SBO Size

Small callables are stored in an inline buffer without heap allocation. The buffer is sized to hold a function pointer plus pointer-sized captured state. Larger callables are heap-allocated transparently.

The SBO size is controlled by `FW_SBO_SIZE`. The default ensures that both `function_wrapper<int(int, int)>` and `move_only_function_wrapper<int(int, int)>` stay within `sizeof(void*) * 8`. Static assertions enforce this at build time.
