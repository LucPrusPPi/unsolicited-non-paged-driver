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
    static SimdLevel GetActiveLevel() noexcept {
#if UNPD_CONFIG_ALLOW_SIMD_ACCELERATION
        uint32_t capsMask = UnpdQueryCpuSimdCapsASM();
        if ((capsMask & 2) != 0) return SimdLevel::AVX2;
        if ((capsMask & 1) != 0) return SimdLevel::SSE42;
#endif
        return SimdLevel::Scalar;
    }

    static const char* GetActiveLevelName() noexcept {
        switch (GetActiveLevel()) {
            case SimdLevel::AVX2:   return "AVX2 (256-bit Vectorized)";
            case SimdLevel::AVX512: return "AVX-512 (512-bit Vectorized)";
            case SimdLevel::SSE42:  return "SSE4.2 (128-bit Vectorized)";
            default:                return "Scalar (MASM64 Rep-Stos)";
        }
    }

    static const void* ScanPatternAVX512Emulated(const void* base, uint64_t size, const uint8_t* pattern, const char* mask) noexcept {
        // AVX-512 Software Emulation: Splitting 64-byte ZMM vector strides into dual 256-bit AVX2 YMM iterations
        if (!base || !pattern || !mask || size == 0) return nullptr;

        const auto* bytes = static_cast<const uint8_t*>(base);
        for (uint64_t i = 0; i < size; i += 64) {
            uint64_t currentChunk = (size - i >= 64) ? 64 : (size - i);
            const void* match = ScanPattern(bytes + i, currentChunk, pattern, mask);
            if (match) return match;
        }
        return nullptr;
    }

    static const void* ScanPattern(const void* base, uint64_t size, const uint8_t* pattern, const char* mask) noexcept {
        if (!base || !pattern || !mask || size == 0) return nullptr;

#if UNPD_CONFIG_ALLOW_SIMD_ACCELERATION && defined(_KERNEL_MODE)
        SimdLevel level = GetActiveLevel();
        if (level == SimdLevel::AVX512) {
            KFLOATING_SAVE fpuSave{};
            // XSTATE_MASK_AVX512 = XSTATE_MASK_GSSE (0x4) | XSTATE_MASK_AVX512 (0xE0)
            NTSTATUS status = KeSaveExtendedProcessorState(0xE4, reinterpret_cast<PXSTATE_SAVE>(&fpuSave));
            const void* result = nullptr;
            if (NT_SUCCESS(status)) {
                result = UnpdScanPatternAVX512ASM(base, size, pattern, mask);
                KeRestoreExtendedProcessorState(reinterpret_cast<PXSTATE_SAVE>(&fpuSave));
                return result;
            }
        } else if (level == SimdLevel::AVX2) {
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

    static void FastZero(void* address, uint64_t size) noexcept {
        if (!address || size == 0) return;

#if UNPD_CONFIG_ALLOW_SIMD_ACCELERATION && defined(_KERNEL_MODE)
        if (GetActiveLevel() == SimdLevel::AVX2) {
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

    static void FastCopy(void* dest, const void* src, uint64_t size) noexcept {
        if (!dest || !src || size == 0) return;

#if UNPD_CONFIG_ALLOW_SIMD_ACCELERATION && defined(_KERNEL_MODE)
        if (GetActiveLevel() == SimdLevel::AVX2) {
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
};

} // namespace unpd::simd

#endif // UNPD_SIMD_ENGINE_HPP
