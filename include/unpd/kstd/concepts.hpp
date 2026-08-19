#pragma once

#ifndef UNPD_KSTD_CONCEPTS_HPP
#define UNPD_KSTD_CONCEPTS_HPP

#include <unpd/common.h>

namespace unpd::kstd {

// ============================================================================
// Freestanding Type Traits
// ============================================================================
template <typename T, typename U>
struct is_same { static constexpr bool value = false; };

template <typename T>
struct is_same<T, T> { static constexpr bool value = true; };

template <typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

template <typename T>
struct is_integral { static constexpr bool value = false; };

template <> struct is_integral<bool> { static constexpr bool value = true; };
template <> struct is_integral<char> { static constexpr bool value = true; };
template <> struct is_integral<signed char> { static constexpr bool value = true; };
template <> struct is_integral<unsigned char> { static constexpr bool value = true; };
template <> struct is_integral<short> { static constexpr bool value = true; };
template <> struct is_integral<unsigned short> { static constexpr bool value = true; };
template <> struct is_integral<int> { static constexpr bool value = true; };
template <> struct is_integral<unsigned int> { static constexpr bool value = true; };
template <> struct is_integral<long> { static constexpr bool value = true; };
template <> struct is_integral<unsigned long> { static constexpr bool value = true; };
template <> struct is_integral<long long> { static constexpr bool value = true; };
template <> struct is_integral<unsigned long long> { static constexpr bool value = true; };

template <typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;

template <typename T>
struct is_pointer { static constexpr bool value = false; };

template <typename T>
struct is_pointer<T*> { static constexpr bool value = true; };

template <typename T>
inline constexpr bool is_pointer_v = is_pointer<T>::value;

// ============================================================================
// C++20 Concepts for Freestanding Kernel Subset
// ============================================================================
template <typename T, typename U>
concept same_as = is_same_v<T, U> && is_same_v<U, T>;

template <typename T>
concept integral = is_integral_v<T>;

template <typename T>
concept pointer = is_pointer_v<T>;

template <typename T>
concept trivially_copyable = __is_trivially_copyable(T);

template <typename F, typename... Args>
concept invocable = requires(F&& f, Args&&... args) {
    f(static_cast<Args&&>(args)...);
};

} // namespace unpd::kstd

#endif // UNPD_KSTD_CONCEPTS_HPP
