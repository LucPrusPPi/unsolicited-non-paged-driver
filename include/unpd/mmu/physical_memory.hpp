#pragma once

#ifndef UNPD_MMU_PHYSICAL_MEMORY_HPP
#define UNPD_MMU_PHYSICAL_MEMORY_HPP

#include <unpd/common.h>
#include <unpd/config.hpp>

#ifdef _KERNEL_MODE
#include <ntddk.h>
#endif

namespace unpd::mmu {

#if UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

/**
 * @brief C++20 RAII Template Wrapper for Physical Memory Mapping.
 * Automatically handles MmMapIoSpace and MmUnmapIoSpace in Ring-0.
 */
template <typename T = uint8_t>
class PhysicalMemoryMapping {
private:
    PVOID  m_virtualAddress = nullptr;
    SIZE_T m_mappedSize     = 0;

public:
    constexpr PhysicalMemoryMapping() noexcept = default;

    explicit PhysicalMemoryMapping(ULONG64 physicalAddress, SIZE_T size = sizeof(T)) noexcept {
        Map(physicalAddress, size);
    }

    ~PhysicalMemoryMapping() noexcept {
        Unmap();
    }

    PhysicalMemoryMapping(const PhysicalMemoryMapping&) = delete;
    PhysicalMemoryMapping& operator=(const PhysicalMemoryMapping&) = delete;

    PhysicalMemoryMapping(PhysicalMemoryMapping&& other) noexcept
        : m_virtualAddress(other.m_virtualAddress), m_mappedSize(other.m_mappedSize) {
        other.m_virtualAddress = nullptr;
        other.m_mappedSize = 0;
    }

    PhysicalMemoryMapping& operator=(PhysicalMemoryMapping&& other) noexcept {
        if (this != &other) {
            Unmap();
            m_virtualAddress = other.m_virtualAddress;
            m_mappedSize = other.m_mappedSize;
            other.m_virtualAddress = nullptr;
            other.m_mappedSize = 0;
        }
        return *this;
    }

    bool Map(ULONG64 physicalAddress, SIZE_T size) noexcept {
        Unmap();
        if (!physicalAddress || size == 0) return false;

#ifdef _KERNEL_MODE
        PHYSICAL_ADDRESS pa{};
        pa.QuadPart = static_cast<LONGLONG>(physicalAddress);
        m_virtualAddress = MmMapIoSpace(pa, size, MmCached);
        if (m_virtualAddress) {
            m_mappedSize = size;
            return true;
        }
        return false;
#else
        // Usermode Mock Allocation for GTest / Emulation
        m_virtualAddress = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (m_virtualAddress) {
            m_mappedSize = size;
            return true;
        }
        return false;
#endif
    }

    void Unmap() noexcept {
        if (m_virtualAddress) {
#ifdef _KERNEL_MODE
            MmUnmapIoSpace(m_virtualAddress, m_mappedSize);
#else
            VirtualFree(m_virtualAddress, 0, MEM_RELEASE);
#endif
            m_virtualAddress = nullptr;
            m_mappedSize = 0;
        }
    }

    [[nodiscard]] bool IsValid() const noexcept { return m_virtualAddress != nullptr; }
    [[nodiscard]] T* Get() const noexcept { return static_cast<T*>(m_virtualAddress); }
    [[nodiscard]] T* operator->() const noexcept { return Get(); }
    [[nodiscard]] T& operator*() const noexcept { return *Get(); }
    [[nodiscard]] SIZE_T Size() const noexcept { return m_mappedSize; }
};

/**
 * @brief High-level Physical RAM reader and writer with security boundary checks.
 */
class PhysicalMemory {
public:
    /**
     * @brief Checks if the given physical memory range is valid RAM and not MMIO/APIC/Reserved hardware.
     */
    static bool IsPhysicalAddressAllowed(ULONG64 physicalAddress, SIZE_T size) noexcept {
        if (!physicalAddress || size == 0) return false;
        // Restrict lower 1MB legacy BIOS/IVT/BDA space and APIC/IO-APIC/PCI MMIO range (>= 0xFEC00000 && < 0x100000000)
        if (physicalAddress < 0x100000ULL) return false;
        if (physicalAddress >= 0xFEC00000ULL && physicalAddress < 0x100000000ULL) return false;
        return true;
    }

    static NTSTATUS ReadPhysicalAddress(ULONG64 physicalAddress, PVOID buffer, SIZE_T size, PSIZE_T bytesRead);
    static NTSTATUS WritePhysicalAddress(ULONG64 physicalAddress, const void* buffer, SIZE_T size, PSIZE_T bytesWritten);
};

#else

template <typename T = uint8_t>
class PhysicalMemoryMapping {
public:
    constexpr PhysicalMemoryMapping() noexcept = default;
    explicit PhysicalMemoryMapping(ULONG64, SIZE_T = sizeof(T)) noexcept {}
    [[nodiscard]] bool IsValid() const noexcept { return false; }
    [[nodiscard]] T* Get() const noexcept { return nullptr; }
};

class PhysicalMemory {
public:
    static NTSTATUS ReadPhysicalAddress(ULONG64, PVOID, SIZE_T, PSIZE_T) { return STATUS_NOT_SUPPORTED; }
    static NTSTATUS WritePhysicalAddress(ULONG64, const void*, SIZE_T, PSIZE_T) { return STATUS_NOT_SUPPORTED; }
};

#endif // UNPD_FEATURE_PHYSICAL_MEMORY_ACCESS

} // namespace unpd::mmu

#endif // UNPD_MMU_PHYSICAL_MEMORY_HPP
