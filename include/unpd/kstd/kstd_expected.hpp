#pragma once

#ifndef UNPD_KSTD_EXPECTED_HPP
#define UNPD_KSTD_EXPECTED_HPP

#include <stdint.h>

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
using NTSTATUS = LONG;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace unpd::kstd {

/**
 * @brief Freestanding C++20 expected<T, NTSTATUS> for modern kernel error handling without C++ exceptions.
 *
 * @tparam T Success value type
 * @tparam E Error type (defaults to NTSTATUS)
 */
template <typename T, typename E = NTSTATUS>
class expected {
public:
    constexpr expected(const T& val) noexcept 
        : m_hasValue(true), m_value(val), m_error(STATUS_SUCCESS) {}

    constexpr expected(T&& val) noexcept 
        : m_hasValue(true), m_value(static_cast<T&&>(val)), m_error(STATUS_SUCCESS) {}

    struct error_tag {};

    constexpr expected(error_tag, E err) noexcept 
        : m_hasValue(false), m_value{}, m_error(err) {}

    static constexpr expected<T, E> error(E err) noexcept {
        return expected<T, E>(error_tag{}, err);
    }

    [[nodiscard]] constexpr bool has_value() const noexcept { return m_hasValue; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return m_hasValue; }

    [[nodiscard]] constexpr T& value() noexcept { return m_value; }
    [[nodiscard]] constexpr const T& value() const noexcept { return m_value; }

    [[nodiscard]] constexpr E error() const noexcept { return m_error; }

    [[nodiscard]] constexpr T value_or(T defaultVal) const noexcept {
        return m_hasValue ? m_value : defaultVal;
    }

private:
    bool m_hasValue;
    T m_value;
    E m_error;
};

// Specialization for void
template <typename E>
class expected<void, E> {
public:
    constexpr expected() noexcept 
        : m_hasValue(true), m_error(STATUS_SUCCESS) {}

    struct error_tag {};

    constexpr expected(error_tag, E err) noexcept 
        : m_hasValue(false), m_error(err) {}

    static constexpr expected<void, E> success() noexcept {
        return expected<void, E>();
    }

    static constexpr expected<void, E> error(E err) noexcept {
        return expected<void, E>(error_tag{}, err);
    }

    [[nodiscard]] constexpr bool has_value() const noexcept { return m_hasValue; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return m_hasValue; }
    [[nodiscard]] constexpr E error() const noexcept { return m_error; }

private:
    bool m_hasValue;
    E m_error;
};

} // namespace unpd::kstd

#endif // UNPD_KSTD_EXPECTED_HPP
