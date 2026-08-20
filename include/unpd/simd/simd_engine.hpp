#pragma once

#ifndef UNPD_SIMD_ENGINE_HPP
#define UNPD_SIMD_ENGINE_HPP

#include "unpd/config.hpp"
#include "unpd/kernel_asm.hpp"

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

namespace unpd::simd {

enum class SimdLevel : uint32_t {
    Scalar = 0,
    SSE42  = 1,
    AVX2   = 2,
    AVX512 = 4
};

/**
 * @brief High-Performance Modular SIMD Engine with CPUID Auto-Dispatching.
 *
 * @details
 * Handles safe Kernel XSAVE state preservation via KeSaveExtendedProcessorState /
 * KeRestoreExtendedProcessorState when invoking AVX2/AVX-512 instructions in Ring-0.
 */
class SimdEngine {
public:
    static SimdEngine& Instance() noexcept {
        static SimdEngine instance;
        return instance;
    }

    SimdEngine() noexcept : m_caps(SimdLevel::Scalar) {
        Initialize();
    }

    void Initialize() noexcept {
#if UNPD_CONFIG_ALLOW_SIMD_ACCELERATION
        uint32_t capsMask = UnpdQueryCpuSimdCapsASM();
        if ((capsMask & 2) != 0) {
            m_caps = SimdLevel::AVX2;
        } else if ((capsMask & 1) != 0) {
            m_caps = SimdLevel::SSE42;
        } else {
            m_caps = SimdLevel::Scalar;
        }
#else
        m_caps = SimdLevel::Scalar;
#endif
    }

    [[nodiscard]] SimdLevel GetActiveLevel() const noexcept {
        return m_caps;
    }

    [[nodiscard]] const char* GetActiveLevelName() const noexcept {
        switch (m_caps) {
            case SimdLevel::AVX2:   return "AVX2 (256-bit Vectorized)";
            case SimdLevel::AVX512: return "AVX-512 (512-bit Vectorized)";
            case SimdLevel::SSE42:  return "SSE4.2 (128-bit Vectorized)";
            default:                return "Scalar (MASM64 Rep-Stos)";
        }
    }

    const void* ScanPattern(const void* base, uint64_t size, const uint8_t* pattern, const char* mask) noexcept {
        if (!base || !pattern || !mask || size == 0) return nullptr;

#if UNPD_CONFIG_ALLOW_SIMD_ACCELERATION && defined(_KERNEL_MODE)
        if (m_caps == SimdLevel::AVX2) {
            KFLOATING_SAVE fpuSave{};
            NTSTATUS status = KeSaveExtendedProcessorState(XSTATE_MASK_GSSE, reinterpret_cast<PXSTATE_SAVE>(&fpuSave));
            const void* result = nullptr;
            if (NT_SUCCESS(status)) {
                result = UnpdScanPatternAVX2ASM(base, size, pattern, mask);
                KeRestoreExtendedProcessorState(reinterpret_cast<PXSTATE_SAVE>(&fpuSave));
                return result;
            }
        }
#endif
        return UnpdScanPatternASM(base, size, pattern, mask);
    }

    void FastZero(void* address, uint64_t size) noexcept {
        if (!address || size == 0) return;

#if UNPD_CONFIG_ALLOW_SIMD_ACCELERATION && defined(_KERNEL_MODE)
        if (m_caps == SimdLevel::AVX2) {
            KFLOATING_SAVE fpuSave{};
            NTSTATUS status = KeSaveExtendedProcessorState(XSTATE_MASK_GSSE, reinterpret_cast<PXSTATE_SAVE>(&fpuSave));
            if (NT_SUCCESS(status)) {
                UnpdFastZeroAVX2ASM(address, size);
                KeRestoreExtendedProcessorState(reinterpret_cast<PXSTATE_SAVE>(&fpuSave));
                return;
            }
        }
#endif
        UnpdZeroMemorySecureASM(address, size);
    }

    void FastCopy(void* dest, const void* src, uint64_t size) noexcept {
        if (!dest || !src || size == 0) return;

#if UNPD_CONFIG_ALLOW_SIMD_ACCELERATION && defined(_KERNEL_MODE)
        if (m_caps == SimdLevel::AVX2) {
            KFLOATING_SAVE fpuSave{};
            NTSTATUS status = KeSaveExtendedProcessorState(XSTATE_MASK_GSSE, reinterpret_cast<PXSTATE_SAVE>(&fpuSave));
            if (NT_SUCCESS(status)) {
                UnpdFastCopyAVX2ASM(dest, src, size);
                KeRestoreExtendedProcessorState(reinterpret_cast<PXSTATE_SAVE>(&fpuSave));
                return;
            }
        }
#endif
        UnpdFastCopy64(dest, src, size / 8);
    }

private:
    SimdLevel m_caps;
};

} // namespace unpd::simd

#endif // UNPD_SIMD_ENGINE_HPP
