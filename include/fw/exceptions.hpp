#ifndef FW_EXCEPTIONS_HPP
#define FW_EXCEPTIONS_HPP

#include <exception>

namespace fw
{
// ── Exceptions ───────────────────────────────────────────────────────────────
// Thrown by function_wrapper when an invalid call is attempted. bad_call is thrown when the wrapper is empty,
// and bad_signature_call is thrown when the wrapper contains a callable but it doesn't match

struct bad_call final : std::exception
{
    [[nodiscard]] const char* what() const noexcept override
    {
        return "fw::bad_call: called an empty function_wrapper";
    }
};

struct bad_signature_call final : std::exception
{
    [[nodiscard]] const char* what() const noexcept override
    {
        return "fw::bad_signature_call: stored callable does not match this signature";
    }
};
} // namespace fw

#endif // FW_EXCEPTIONS_HPP
