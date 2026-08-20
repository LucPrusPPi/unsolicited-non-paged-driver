#pragma once

#ifndef UNPD_MMU_VMT_RESOLVER_HPP
#define UNPD_MMU_VMT_RESOLVER_HPP

#include "unpd/common.h"
#include "unpd/simd/simd_engine.hpp"

namespace unpd::mmu {

/**
 * @brief Zero-RTTI Kernel-Compliant Virtual Method Table (VMT / VTable) Resolver.
 *
 * @details
 * Performs fast RTTI-free scanning of C++ VTables in memory modules (.rdata -> .text references).
 * Entirely compatible with /GR- disabled kernel builds.
 */
class VmtResolver {
public:
    struct VmtInfo {
        uint64_t VtableAddress;
        uint32_t MethodCount;
        uint64_t FirstMethodAddress;
    };

    /**
     * @brief Resolves a VTable structure in memory without relying on C++ RTTI descriptors.
     */
    static bool ResolveVtable(
        const void* moduleBase,
        uint64_t moduleSize,
        uint64_t codeSectionStart,
        uint64_t codeSectionSize,
        VmtInfo& outInfo
    ) noexcept {
        if (!moduleBase || moduleSize < 16 || codeSectionSize == 0) return false;

        const auto* basePtr = static_cast<const uint8_t*>(moduleBase);
        const uint64_t codeEnd = codeSectionStart + codeSectionSize;

        // Scan 8-byte aligned pointers pointing into executable .text code range
        for (uint64_t offset = 0; offset <= moduleSize - 16; offset += 8) {
            const auto* candidate = reinterpret_cast<const uint64_t*>(basePtr + offset);
            uint64_t funcPtr = candidate[0];

            if (funcPtr >= codeSectionStart && funcPtr < codeEnd) {
                // Verify subsequent pointers in table
                uint32_t count = 1;
                while ((offset + count * 8) < moduleSize) {
                    uint64_t nextFunc = candidate[count];
                    if (nextFunc >= codeSectionStart && nextFunc < codeEnd) {
                        count++;
                    } else {
                        break;
                    }
                }

                if (count >= 2) { // Valid VTable threshold
                    outInfo.VtableAddress = reinterpret_cast<uint64_t>(candidate);
                    outInfo.MethodCount = count;
                    outInfo.FirstMethodAddress = funcPtr;
                    return true;
                }
            }
        }
        return false;
    }
};

} // namespace unpd::mmu

#endif // UNPD_MMU_VMT_RESOLVER_HPP
