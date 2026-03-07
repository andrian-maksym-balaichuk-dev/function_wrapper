# Examples

This document shows representative uses of `fw::function_wrapper`, ordered from the simplest to the most advanced.

---

## 1. Single-Signature Wrapper

The most basic use case. `fw::function_wrapper` with one signature behaves like `std::function`.

```cpp
#include <fw/function_wrapper.hpp>

int add(int a, int b) { return a + b; }

int main()
{
    fw::function_wrapper<int(int, int)> w = add;
    return w(2, 3); // returns 5
}
```

Use this when you want `std::function`-like behavior but you prefer a single wrapper type throughout your library, or you anticipate needing to add a second signature later without changing the callsites.

---

## 2. Multi-Signature Numeric Dispatch

The primary use case for `fw`. One lambda, two declared signatures, no branching.

```cpp
#include <fw/function_wrapper.hpp>
#include <type_traits>

int main()
{
    fw::function_wrapper<int(int, int), float(float, float)> add =
        [](auto a, auto b) { return a + b; };

    const int   ri = add(2, 2);      // dispatches to int(int, int)
    const float rf = add(2.f, 2.5f); // dispatches to float(float, float)

    // Return types are deduced at compile time.
    static_assert(std::is_same_v<decltype(add(2, 2)),      int>);
    static_assert(std::is_same_v<decltype(add(2.f, 2.5f)), float>);

    return static_cast<int>(ri + rf);
}
```

The dispatch happens entirely at compile time — no `if/else` or `std::visit` at the callsite.

---

## 3. Mixed Arithmetic — Common Target Preference

When arguments mix `int` and `float`, `fw` uses the common numeric target preference to pick the `float` signature rather than failing.

```cpp
#include <fw/function_wrapper.hpp>

int main()
{
    fw::function_wrapper<int(int, int), float(float, float)> add =
        [](auto a, auto b) { return a + b; };

    // int + float — the float signature is the common-target winner.
    const float result = add(2, 2.5f);

    return static_cast<int>(result); // 4
}
```

---

## 4. String-Like Inputs

C-strings, `std::string_view`, and `std::string` can all be routed through declared string-like signatures.

```cpp
#include <fw/function_wrapper.hpp>
#include <string>
#include <string_view>

int main()
{
    fw::function_wrapper<std::size_t(std::string), std::size_t(std::string_view)> length =
        [](auto text) { return text.size(); };

    const std::size_t a = length(std::string{"owned"});  // string  → string sig
    const std::size_t b = length("literal");             // C-string → string_view sig

    return static_cast<int>(a + b);
}
```

---

## 5. User-Defined Implicit Conversions

If a declared parameter type has an implicit converting constructor from the argument type, `fw` will use it.

```cpp
#include <fw/function_wrapper.hpp>
#include <string_view>

struct Label {
    Label(std::string_view text) : value(text) {}  // implicit
    std::string_view value;
};

int main()
{
    fw::function_wrapper<std::size_t(Label)> label_size =
        [](Label l) { return l.value.size(); };

    // "token" → std::string_view → Label (implicit converting constructor)
    return static_cast<int>(label_size("token")); // 5
}
```

---

## 6. Explicit Conversion Boundary

`fw` respects C++ convertibility rules. Explicit constructors are never used implicitly during dispatch. This is a feature, not a limitation — it means your API's explicit conversion boundaries are enforced automatically.

```cpp
#include <fw/function_wrapper.hpp>
#include <string_view>

struct Token {
    explicit Token(std::string_view text) : value(text) {}  // explicit
    std::string_view value;
};

int main()
{
    fw::function_wrapper<void(Token)> consume = [](Token) {};

    // consume("alpha");      // does NOT compile — explicit constructor not used
    consume(Token{"alpha"});  // correct — construct explicitly at callsite
    return 0;
}
```

---

## 7. Lambda Stored by Value

Lambdas with captures are stored by value. Small lambdas use the inline SBO buffer; larger ones are heap-allocated transparently.

```cpp
#include <fw/function_wrapper.hpp>

int main()
{
    int offset = 10;

    fw::function_wrapper<int(int)> shift =
        [offset](int x) { return x + offset; };  // captures offset by value

    return shift(5); // returns 15
}
```

---

## 8. Copy and Move Semantics

`fw::function_wrapper` is copyable and movable. Copies duplicate the stored callable; moves transfer ownership.

```cpp
#include <fw/function_wrapper.hpp>

int main()
{
    fw::function_wrapper<int(int, int)> original =
        [](int a, int b) { return a + b; };

    auto copy  = original;           // deep copy of the stored callable
    auto moved = std::move(original); // original is now empty

    return copy(1, 2) + moved(3, 4); // 3 + 7 = 10
}
```

---

## 9. Empty Wrapper and `bad_call`

A default-constructed `fw::function_wrapper` holds no callable. Calling it throws `fw::bad_call`.

```cpp
#include <fw/function_wrapper.hpp>
#include <fw/exceptions.hpp>

int main()
{
    fw::function_wrapper<int(int)> w;  // empty

    if (!w.has_value()) {
        // Safe — guard before calling.
        return 0;
    }

    try {
        return w(7);
    } catch (const fw::bad_call& e) {
        return 0;
    }
}
```

Use `has_value()` or `operator bool` to guard before calling when the wrapper may be empty.

---

## 10. Target Introspection

Inspect the type and value of the stored callable using `target_type()` and `target<T>()`.

```cpp
#include <fw/function_wrapper.hpp>
#include <cassert>
#include <typeinfo>

int add(int a, int b) { return a + b; }

int main()
{
    fw::function_wrapper<int(int, int)> w = add;

    // Type check
    assert(w.target_type() == typeid(int(*)(int, int)));

    // Value access
    if (auto* fn = w.target<int(*)(int, int)>()) {
        return (*fn)(1, 2); // returns 3
    }

    return 0;
}
```

---

## 11. `reset` and `swap`

```cpp
#include <fw/function_wrapper.hpp>

int main()
{
    fw::function_wrapper<int(int)> a = [](int x) { return x * 2; };
    fw::function_wrapper<int(int)> b = [](int x) { return x + 100; };

    a.swap(b);             // a now doubles, b now adds 100
    swap(a, b);            // swap back using the free function overload

    a.reset();             // a is now empty
    assert(!a.has_value());

    return b(5); // returns 10
}
```

---

## 12. `make_function_array`

Build a `std::array` of `function_wrapper` objects with a unified signature deduced from all callables.

```cpp
#include <fw/function_wrapper.hpp>

int main()
{
    auto handlers = fw::make_function_array(
        [](int x) { return x * 2; },
        [](int x) { return x + 10; },
        [](int x) { return -x; }
    );

    int total = 0;
    for (auto& h : handlers) {
        total += h(5);
    }

    return total; // (5*2) + (5+10) + (-5) = 10 + 15 - 5 = 20
}
```

---

## 13. Storing in a Container

Because `fw::function_wrapper` has value semantics and is copyable, it can be stored in standard containers.

```cpp
#include <fw/function_wrapper.hpp>
#include <vector>

int main()
{
    using Handler = fw::function_wrapper<int(int)>;

    std::vector<Handler> pipeline;
    pipeline.push_back([](int x) { return x * 2; });
    pipeline.push_back([](int x) { return x + 1; });
    pipeline.push_back([](int x) { return x - 3; });

    int value = 5;
    for (auto& step : pipeline) {
        value = step(value);
    }

    return value; // ((5*2)+1)-3 = 8
}
```

---

## 14. Class Method Stored via Lambda

Free functions and member function pointers require a wrapping lambda. The wrapper stores the lambda by value.

```cpp
#include <fw/function_wrapper.hpp>

struct Processor {
    int scale;
    int run(int x) const { return x * scale; }
};

int main()
{
    Processor p{3};

    fw::function_wrapper<int(int)> w =
        [&p](int x) { return p.run(x); };

    return w(7); // returns 21
}
```

---

## Real-World Patterns

The following examples show how `fw::function_wrapper` appears in production-style library and application code.

---

### Math Library — Unified Numeric API

A math utility that exposes the same operation for `int`, `float`, and `double` through one stable type.

```cpp
#include <fw/function_wrapper.hpp>

// One type, three call surfaces.
using MathOp = fw::function_wrapper<
    int(int, int),
    float(float, float),
    double(double, double)
>;

MathOp make_adder()
{
    return [](auto a, auto b) { return a + b; };
}

MathOp make_multiplier()
{
    return [](auto a, auto b) { return a * b; };
}

int main()
{
    MathOp add = make_adder();
    MathOp mul = make_multiplier();

    const int    i = add(2, 3);          // 5
    const float  f = add(1.5f, 2.5f);   // 4.0f
    const double d = mul(2.0, 3.14159); // 6.28...

    return static_cast<int>(i + f + d); // 15
}
```

---

### String Processing — Unified Text API

A text utility that accepts `const char*`, `std::string_view`, and `std::string` through a single stored handler.

```cpp
#include <fw/function_wrapper.hpp>
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>

// Accepts all three common string types through one declared surface.
using TextTransform = fw::function_wrapper<
    std::string(std::string),
    std::string(std::string_view),
    std::string(const char*)
>;

TextTransform make_uppercase()
{
    return [](auto input) {
        std::string result(input);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        return result;
    };
}

TextTransform make_trimmer()
{
    return [](auto input) {
        std::string s(input);
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            [](unsigned char c){ return !std::isspace(c); }));
        s.erase(std::find_if(s.rbegin(), s.rend(),
            [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
        return s;
    };
}

int main()
{
    TextTransform upper = make_uppercase();
    TextTransform trim  = make_trimmer();

    const std::string r1 = upper("hello world");    // "HELLO WORLD" — C-string
    const std::string r2 = upper(std::string_view{"fw library"});  // "FW LIBRARY"
    const std::string r3 = trim(std::string{"  padded  "});        // "padded"

    return static_cast<int>(r1.size() + r2.size() + r3.size()); // 11+10+6 = 27
}
```

---

### Feature Flag Dispatch — Pluggable Behavior

A feature flag system where behavior changes at initialization time but the callsite never branches.

```cpp
#include <fw/function_wrapper.hpp>
#include <string_view>

// Score calculator that can be swapped per feature flag.
using ScoreFn = fw::function_wrapper<double(int clicks, int views)>;

ScoreFn make_legacy_scorer()
{
    // Old algorithm: raw clicks.
    return [](int clicks, int /*views*/) -> double {
        return static_cast<double>(clicks);
    };
}

ScoreFn make_v2_scorer()
{
    // New algorithm: click-through rate.
    return [](int clicks, int views) -> double {
        if (views == 0) return 0.0;
        return static_cast<double>(clicks) / static_cast<double>(views);
    };
}

ScoreFn load_scorer(bool use_v2)
{
    return use_v2 ? make_v2_scorer() : make_legacy_scorer();
}

int main()
{
    // Decision made once at startup; callsite is always the same.
    ScoreFn scorer = load_scorer(true);

    const double score = scorer(120, 1000); // 0.12 — v2 CTR

    return static_cast<int>(score * 100); // 12
}
```

---

### Callback Registry — Heterogeneous Handlers

A named event registry that stores handlers with multiple supported argument types.

```cpp
#include <fw/function_wrapper.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

// Handlers can accept an event name as string, string_view, or C-string.
using EventHandler = fw::function_wrapper<
    void(std::string),
    void(std::string_view),
    void(const char*)
>;

class EventBus {
public:
    void subscribe(std::string event, EventHandler handler)
    {
        handlers_[std::move(event)] = std::move(handler);
    }

    void emit(const std::string& event, std::string_view payload) const
    {
        if (auto it = handlers_.find(event); it != handlers_.end()) {
            it->second(payload); // routed through string_view signature
        }
    }

private:
    std::unordered_map<std::string, EventHandler> handlers_;
};

int main()
{
    EventBus bus;

    std::string last_payload;
    bus.subscribe("user.login", [&last_payload](auto payload) {
        last_payload = std::string(payload);
    });

    bus.emit("user.login", "user_id=42");

    return static_cast<int>(last_payload.size()); // 10
}
```

---

### Math Pipeline — Chained Numeric Transforms

A sequence of numeric transforms stored uniformly and applied in order.

```cpp
#include <fw/function_wrapper.hpp>
#include <vector>
#include <cmath>

using NumericTransform = fw::function_wrapper<
    double(double),
    float(float)
>;

int main()
{
    std::vector<NumericTransform> pipeline;

    pipeline.push_back([](auto x) { return x * 2.0;     });  // scale
    pipeline.push_back([](auto x) { return x + 1.0;     });  // shift
    pipeline.push_back([](auto x) { return x * x;       });  // square
    pipeline.push_back([](auto x) { return std::sqrt(x);});  // sqrt

    double value = 3.0;
    for (auto& step : pipeline) {
        value = step(value); // always dispatches to double(double)
    }

    // (3*2+1)^2 = 49, sqrt(49) = 7
    return static_cast<int>(std::round(value)); // 7
}
```
