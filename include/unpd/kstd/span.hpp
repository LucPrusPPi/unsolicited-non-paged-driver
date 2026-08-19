#pragma once

#ifndef UNPD_KSTD_SPAN_HPP
#define UNPD_KSTD_SPAN_HPP

#include <stdint.h>
#include <stddef.h>

namespace unpd::kstd {

// Freestanding type traits for kernel mode
template <typename U> struct remove_cv { using type = U; };
template <typename U> struct remove_cv<const U> { using type = U; };
template <typename U> struct remove_cv<volatile U> { using type = U; };
template <typename U> struct remove_cv<const volatile U> { using type = U; };
template <typename U> using remove_cv_t = typename remove_cv<U>::type;

/**
 * @brief Zero-overhead type-safe contiguous memory view for Kernel and User modes.
 *
 * @details
 * Compliant with C++20 std::span semantics but works in freestanding /kernel environments
 * without relying on the MSVC C++ standard library runtime.
 *
 * @tparam T Element type (e.g. uint8_t, const char, struct)
 */
template <typename T>
class span {
public:
    using element_type = T;
    using value_type = remove_cv_t<T>;
    using size_type = size_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;

    constexpr span() noexcept : m_data(nullptr), m_size(0) {}

    constexpr span(pointer ptr, size_type count) noexcept 
        : m_data(ptr), m_size(count) {}

    constexpr span(pointer first, pointer last) noexcept 
        : m_data(first), m_size(static_cast<size_type>(last - first)) {}

    template <size_t N>
    constexpr span(T (&arr)[N]) noexcept 
        : m_data(arr), m_size(N) {}

    [[nodiscard]] constexpr pointer data() const noexcept { return m_data; }
    [[nodiscard]] constexpr size_type size() const noexcept { return m_size; }
    [[nodiscard]] constexpr size_type size_bytes() const noexcept { return m_size * sizeof(T); }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

    [[nodiscard]] constexpr reference operator[](size_type idx) const noexcept {
        return m_data[idx];
    }

    [[nodiscard]] constexpr iterator begin() const noexcept { return m_data; }
    [[nodiscard]] constexpr iterator end() const noexcept { return m_data + m_size; }

    [[nodiscard]] constexpr span<T> subspan(size_type offset, size_type count = static_cast<size_type>(-1)) const noexcept {
        if (offset >= m_size) return span<T>();
        size_type available = m_size - offset;
        size_type actual_count = (count < available) ? count : available;
        return span<T>(m_data + offset, actual_count);
    }

private:
    pointer m_data;
    size_type m_size;
};

using byte_span = span<uint8_t>;
using const_byte_span = span<const uint8_t>;

} // namespace unpd::kstd

#endif // UNPD_KSTD_SPAN_HPP
