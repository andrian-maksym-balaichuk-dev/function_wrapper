# Examples

This document shows representative uses of `fw::function_wrapper`.

## Single-Signature Wrapper

```cpp
#include <fw/function_wrapper.hpp>

int add(int left, int right) {
    return left + right;
}

int main() {
    fw::function_wrapper<int(int, int)> wrapper = add;
    return wrapper(2, 3);
}
```

Use this when you want `std::function`-like behavior but you prefer the same
wrapper type the rest of your library already exposes.

## Multi-Signature Numeric Dispatch

```cpp
#include <fw/function_wrapper.hpp>

#include <type_traits>

int main() {
    fw::function_wrapper<int(int, int), float(float, float)> add =
        [](auto left, auto right) {
            return left + right;
        };

    const int exact = add(2, 2);
    const float mixed = add(2, 2.5f);

    static_assert(std::is_same_v<decltype(add(2, 2)), int>);
    static_assert(std::is_same_v<decltype(add(2, 2.5f)), float>);

    return exact + static_cast<int>(mixed);
}
```

This is one of the main library use cases: one callable, multiple public
signatures, and deterministic selection between them.

## String-Like Inputs

```cpp
#include <fw/function_wrapper.hpp>

#include <string>
#include <string_view>

int main() {
    fw::function_wrapper<std::size_t(std::string), std::size_t(std::string_view)> length =
        [](auto text) {
            return text.size();
        };

    const std::size_t owned = length(std::string{"alpha"});
    const std::size_t viewed = length("beta");

    return static_cast<int>(owned + viewed);
}
```

String literals can be routed through declared string-like signatures with the
library ranking policy.

## User-Defined Implicit Conversions

```cpp
#include <fw/function_wrapper.hpp>

#include <string_view>

struct Label {
    Label(std::string_view text) : value(text) {}
    std::string_view value;
};

int main() {
    fw::function_wrapper<std::size_t(Label)> label_size =
        [](Label label) {
            return label.value.size();
        };

    return static_cast<int>(label_size("token"));
}
```

This works because `Label` has an implicit converting constructor from
`std::string_view`.

## Explicit Conversion Boundary

```cpp
#include <fw/function_wrapper.hpp>

#include <string_view>

struct Token {
    explicit Token(std::string_view text) : value(text) {}
    std::string_view value;
};

int main() {
    fw::function_wrapper<void(Token)> consume = [](Token) {};

    // consume("alpha");      // does not compile
    consume(Token{"alpha"});  // correct
}
```

`fw` follows normal C++ convertibility rules. Explicit conversions are not used
implicitly during dispatch.

## Empty Wrapper Behavior

```cpp
#include <fw/exceptions.hpp>
#include <fw/function_wrapper.hpp>

int main() {
    fw::function_wrapper<int(int)> wrapper;

    try {
        return wrapper(7);
    } catch (const fw::bad_call&) {
        return 0;
    }
}
```

An empty wrapper throws `fw::bad_call`.

## Target Introspection

```cpp
#include <fw/function_wrapper.hpp>

int add(int left, int right) {
    return left + right;
}

int main() {
    fw::function_wrapper<int(int, int)> wrapper = add;

    if (auto* target = wrapper.target<int(*)(int, int)>()) {
        return (*target)(1, 2);
    }

    return 0;
}
```

This is useful when library code needs to inspect the stored callable type.
