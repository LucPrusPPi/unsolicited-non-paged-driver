#pragma once

#ifndef UNPD_KSTD_UNIQUE_PTR_HPP
#define UNPD_KSTD_UNIQUE_PTR_HPP

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

#include <stdint.h>

namespace unpd::kstd {

#ifdef _KERNEL_MODE
template <ULONG Tag = 'DPNU'>
struct KernelPoolDeleter {
    void operator()(void* ptr) const noexcept {
        if (ptr) {
            ExFreePoolWithTag(ptr, Tag);
        }
    }
};
#else
template <uint32_t Tag = 0>
struct KernelPoolDeleter {
    void operator()(void* ptr) const noexcept {
        if (ptr) {
            HeapFree(GetProcessHeap(), 0, ptr);
        }
    }
};
#endif

/**
 * @brief Zero-overhead RAII Unique Pointer for Windows Kernel Pool Allocations.
 */
template <typename T, typename Deleter = KernelPoolDeleter<'DPNU'>>
class unique_ptr {
public:
    using pointer = T*;
    using element_type = T;
    using deleter_type = Deleter;

    constexpr unique_ptr() noexcept : m_ptr(nullptr) {}
    constexpr explicit unique_ptr(pointer p) noexcept : m_ptr(p) {}

    ~unique_ptr() noexcept {
        reset();
    }

    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    constexpr unique_ptr(unique_ptr&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    pointer release() noexcept {
        pointer tmp = m_ptr;
        m_ptr = nullptr;
        return tmp;
    }

    void reset(pointer p = nullptr) noexcept {
        if (m_ptr) {
            Deleter{}(m_ptr);
        }
        m_ptr = p;
    }

    [[nodiscard]] pointer get() const noexcept { return m_ptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return m_ptr != nullptr; }

    T& operator*() const noexcept { return *m_ptr; }
    pointer operator->() const noexcept { return m_ptr; }

private:
    pointer m_ptr;
};

#ifdef _KERNEL_MODE
template <typename T, ULONG Tag = 'DPNU', typename... Args>
[[nodiscard]] unique_ptr<T, KernelPoolDeleter<Tag>> make_kernel_unique(Args&&... args) {
    void* memory = UnpdAllocatePool(sizeof(T), Tag);
    if (!memory) return unique_ptr<T, KernelPoolDeleter<Tag>>();
    T* object = new (memory) T(static_cast<Args&&>(args)...);
    return unique_ptr<T, KernelPoolDeleter<Tag>>(object);
}
#endif

} // namespace unpd::kstd

#endif // UNPD_KSTD_UNIQUE_PTR_HPP
