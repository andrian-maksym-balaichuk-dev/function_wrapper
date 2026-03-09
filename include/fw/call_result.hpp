#ifndef FW_CALL_RESULT_HPP
#define FW_CALL_RESULT_HPP

#include <optional>
#include <type_traits>
#include <utility>

namespace fw
{
// ── try_call status/result ───────────────────────────────────────────────────
// Used by the non-throwing try_call(...) APIs to report whether dispatch
// succeeded, hit an empty callable, or found no live slot for the signature.

enum class try_call_status : unsigned char
{
    Success,
    Empty,
    SignatureMismatch
};

template <class R>
class try_call_result
{
public:
    using value_type = R;

    [[nodiscard]] static constexpr try_call_result success(R value) noexcept(std::is_nothrow_move_constructible_v<R>)
    {
        return try_call_result(try_call_status::Success, std::move(value));
    }

    [[nodiscard]] static constexpr try_call_result empty() noexcept
    {
        return try_call_result(try_call_status::Empty);
    }

    [[nodiscard]] static constexpr try_call_result signature_mismatch() noexcept
    {
        return try_call_result(try_call_status::SignatureMismatch);
    }

    [[nodiscard]] constexpr try_call_status status() const noexcept
    {
        return status_;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return status_ == try_call_status::Success;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] constexpr value_type* value_ptr() noexcept
    {
        return value_ ? std::addressof(*value_) : nullptr;
    }

    [[nodiscard]] constexpr const value_type* value_ptr() const noexcept
    {
        return value_ ? std::addressof(*value_) : nullptr;
    }

    [[nodiscard]] constexpr value_type& value() & noexcept
    {
        return *value_;
    }

    [[nodiscard]] constexpr const value_type& value() const& noexcept
    {
        return *value_;
    }

    [[nodiscard]] constexpr value_type&& value() && noexcept
    {
        return std::move(*value_);
    }

private:
    constexpr explicit try_call_result(try_call_status status) noexcept : status_(status) {}

    constexpr try_call_result(try_call_status status, R value) noexcept(std::is_nothrow_move_constructible_v<R>)
    : status_(status), value_(std::move(value))
    {}

    try_call_status status_{ try_call_status::Empty };
    std::optional<R> value_{};
};

template <class R>
class try_call_result<R&>
{
public:
    using value_type = R;

    [[nodiscard]] static constexpr try_call_result success(R& value) noexcept
    {
        return try_call_result(try_call_status::Success, std::addressof(value));
    }

    [[nodiscard]] static constexpr try_call_result empty() noexcept
    {
        return try_call_result(try_call_status::Empty);
    }

    [[nodiscard]] static constexpr try_call_result signature_mismatch() noexcept
    {
        return try_call_result(try_call_status::SignatureMismatch);
    }

    [[nodiscard]] constexpr try_call_status status() const noexcept
    {
        return status_;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return status_ == try_call_status::Success;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] constexpr value_type* value_ptr() const noexcept
    {
        return pointer_;
    }

    [[nodiscard]] constexpr R& value() const noexcept
    {
        return *pointer_;
    }

private:
    constexpr explicit try_call_result(try_call_status status) noexcept : status_(status) {}

    constexpr try_call_result(try_call_status status, value_type* pointer) noexcept : status_(status), pointer_(pointer) {}

    try_call_status status_{ try_call_status::Empty };
    value_type* pointer_{ nullptr };
};

template <class R>
class try_call_result<R&&>
{
public:
    using value_type = R;

    [[nodiscard]] static constexpr try_call_result success(R&& value) noexcept
    {
        return try_call_result(try_call_status::Success, std::addressof(value));
    }

    [[nodiscard]] static constexpr try_call_result empty() noexcept
    {
        return try_call_result(try_call_status::Empty);
    }

    [[nodiscard]] static constexpr try_call_result signature_mismatch() noexcept
    {
        return try_call_result(try_call_status::SignatureMismatch);
    }

    [[nodiscard]] constexpr try_call_status status() const noexcept
    {
        return status_;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return status_ == try_call_status::Success;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] constexpr value_type* value_ptr() const noexcept
    {
        return pointer_;
    }

    [[nodiscard]] constexpr R&& value() const noexcept
    {
        return static_cast<R&&>(*pointer_);
    }

private:
    constexpr explicit try_call_result(try_call_status status) noexcept : status_(status) {}

    constexpr try_call_result(try_call_status status, value_type* pointer) noexcept : status_(status), pointer_(pointer) {}

    try_call_status status_{ try_call_status::Empty };
    value_type* pointer_{ nullptr };
};

template <>
class try_call_result<void>
{
public:
    [[nodiscard]] static constexpr try_call_result success() noexcept
    {
        return try_call_result(try_call_status::Success);
    }

    [[nodiscard]] static constexpr try_call_result empty() noexcept
    {
        return try_call_result(try_call_status::Empty);
    }

    [[nodiscard]] static constexpr try_call_result signature_mismatch() noexcept
    {
        return try_call_result(try_call_status::SignatureMismatch);
    }

    [[nodiscard]] constexpr try_call_status status() const noexcept
    {
        return status_;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return status_ == try_call_status::Success;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

private:
    constexpr explicit try_call_result(try_call_status status) noexcept : status_(status) {}

    try_call_status status_{ try_call_status::Empty };
};
} // namespace fw

#endif // FW_CALL_RESULT_HPP
