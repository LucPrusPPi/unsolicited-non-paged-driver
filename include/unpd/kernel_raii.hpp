#pragma once

#ifndef UNPD_KERNEL_RAII_HPP
#define UNPD_KERNEL_RAII_HPP

#ifdef _KERNEL_MODE

#include <ntddk.h>

#ifdef __cplusplus

namespace unpd {

class SpinlockGuard {
public:
    explicit SpinlockGuard(PKSPIN_LOCK lock) noexcept
        : m_lock(lock), m_oldIrql(PASSIVE_LEVEL) {
        KeAcquireSpinLock(m_lock, &m_oldIrql);
    }

    ~SpinlockGuard() noexcept {
        if (m_lock != nullptr) {
            KeReleaseSpinLock(m_lock, m_oldIrql);
        }
    }

    SpinlockGuard(const SpinlockGuard&) = delete;
    SpinlockGuard& operator=(const SpinlockGuard&) = delete;

    SpinlockGuard(SpinlockGuard&& other) noexcept
        : m_lock(other.m_lock), m_oldIrql(other.m_oldIrql) {
        other.m_lock = nullptr;
    }

    SpinlockGuard& operator=(SpinlockGuard&& other) noexcept {
        if (this != &other) {
            if (m_lock != nullptr) {
                KeReleaseSpinLock(m_lock, m_oldIrql);
            }
            m_lock = other.m_lock;
            m_oldIrql = other.m_oldIrql;
            other.m_lock = nullptr;
        }
        return *this;
    }

private:
    PKSPIN_LOCK m_lock;
    KIRQL m_oldIrql;
};

class FastMutexGuard {
public:
    explicit FastMutexGuard(PFAST_MUTEX mutex) noexcept
        : m_mutex(mutex) {
        ExAcquireFastMutex(m_mutex);
    }

    ~FastMutexGuard() noexcept {
        if (m_mutex != nullptr) {
            ExReleaseFastMutex(m_mutex);
        }
    }

    FastMutexGuard(const FastMutexGuard&) = delete;
    FastMutexGuard& operator=(const FastMutexGuard&) = delete;

private:
    PFAST_MUTEX m_mutex;
};

template <typename T>
class NonPagedAllocation {
public:
    explicit NonPagedAllocation(SIZE_T byteCount, ULONG tag = 'DPNU') noexcept
        : m_ptr(nullptr), m_size(byteCount), m_tag(tag) {
#if (NTDDI_VERSION >= NTDDI_WIN10_VB)
        m_ptr = static_cast<T*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, byteCount, tag));
#else
        m_ptr = static_cast<T*>(ExAllocatePoolWithTag(NonPagedPoolNx, byteCount, tag));
#endif
        if (m_ptr != nullptr) {
            RtlZeroMemory(m_ptr, byteCount);
        }
    }

    ~NonPagedAllocation() noexcept {
        reset();
    }

    NonPagedAllocation(const NonPagedAllocation&) = delete;
    NonPagedAllocation& operator=(const NonPagedAllocation&) = delete;

    NonPagedAllocation(NonPagedAllocation&& other) noexcept
        : m_ptr(other.m_ptr), m_size(other.m_size), m_tag(other.m_tag) {
        other.m_ptr = nullptr;
        other.m_size = 0;
    }

    NonPagedAllocation& operator=(NonPagedAllocation&& other) noexcept {
        if (this != &other) {
            reset();
            m_ptr = other.m_ptr;
            m_size = other.m_size;
            m_tag = other.m_tag;
            other.m_ptr = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    [[nodiscard]] T* get() const noexcept {
        return m_ptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    [[nodiscard]] SIZE_T size() const noexcept {
        return m_size;
    }

    T* release() noexcept {
        T* temp = m_ptr;
        m_ptr = nullptr;
        m_size = 0;
        return temp;
    }

    void reset(T* newPtr = nullptr, SIZE_T newSize = 0) noexcept {
        if (m_ptr != nullptr) {
            ExFreePoolWithTag(m_ptr, m_tag);
        }
        m_ptr = newPtr;
        m_size = newSize;
    }

private:
    T* m_ptr;
    SIZE_T m_size;
    ULONG m_tag;
};

} // namespace unpd

#endif // __cplusplus
#endif // _KERNEL_MODE
#endif // UNPD_KERNEL_RAII_HPP
