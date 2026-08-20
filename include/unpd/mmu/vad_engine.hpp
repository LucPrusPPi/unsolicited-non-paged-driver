#pragma once

#ifndef UNPD_MMU_VAD_ENGINE_HPP
#define UNPD_MMU_VAD_ENGINE_HPP

#include "unpd/config.hpp"
#include "unpd/common.h"
#include "unpd/nt/native_structs.hpp"

namespace unpd::mmu {

/**
 * @brief High-Performance Modular Ring-0 VAD (Virtual Address Descriptor) Engine.
 *
 * @details
 * Walks the process RTL_AVL_TREE VadRoot to inspect or modify memory page protection flags (PAGE_READWRITE, PAGE_EXECUTE_READWRITE)
 * without calling VirtualProtect or triggering EDR / Anti-Cheat protection hooks.
 */
class VadEngine {
public:
    static NTSTATUS FindVadNode(uint32_t processId, uint64_t virtualAddress, nt::PUNPD_MMVAD_SHORT& outVadNode) noexcept {
        outVadNode = nullptr;
#if UNPD_FEATURE_VAD_ENGINE && defined(_KERNEL_MODE)
        PEPROCESS process = nullptr;
        NTSTATUS status = PsLookupProcessByProcessId(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(processId)), &process);
        if (!NT_SUCCESS(status) || !process) {
            return STATUS_NOT_FOUND;
        }

        auto offsets = nt::NtBuildOffsets::GetCurrentBuildOffsets();
        auto* vadRoot = reinterpret_cast<nt::PUNPD_RTL_AVL_TREE>(reinterpret_cast<uint8_t*>(process) + offsets.VadRootOffset);

        if (!vadRoot || !vadRoot->Root) {
            ObDereferenceObject(process);
            return STATUS_NOT_FOUND;
        }

        const ULONG vpn = static_cast<ULONG>(virtualAddress >> 12);
        nt::PUNPD_RTL_BALANCED_NODE currentNode = vadRoot->Root;

        while (currentNode) {
            auto* vadNode = CONTAINING_RECORD(currentNode, nt::UNPD_MMVAD_SHORT, VadNode);

            if (vpn >= vadNode->StartingVpn && vpn <= vadNode->EndingVpn) {
                outVadNode = vadNode;
                ObDereferenceObject(process);
                return STATUS_SUCCESS;
            }

            if (vpn < vadNode->StartingVpn) {
                currentNode = currentNode->Children[0];
            } else {
                currentNode = currentNode->Children[1];
            }
        }

        ObDereferenceObject(process);
        return STATUS_NOT_FOUND;
#else
        UNREFERENCED_PARAMETER(processId);
        UNREFERENCED_PARAMETER(virtualAddress);
        return STATUS_NOT_SUPPORTED;
#endif
    }

    static NTSTATUS ModifyVadProtection(uint32_t processId, uint64_t virtualAddress, ULONG newProtection) noexcept {
#if UNPD_FEATURE_VAD_ENGINE && defined(_KERNEL_MODE)
        nt::PUNPD_MMVAD_SHORT vadNode = nullptr;
        NTSTATUS status = FindVadNode(processId, virtualAddress, vadNode);
        if (!NT_SUCCESS(status) || !vadNode) {
            return status;
        }

        vadNode->u.VadFlags.Protection = newProtection & 0x1F;
        return STATUS_SUCCESS;
#else
        UNREFERENCED_PARAMETER(processId);
        UNREFERENCED_PARAMETER(virtualAddress);
        UNREFERENCED_PARAMETER(newProtection);
        return STATUS_NOT_SUPPORTED;
#endif
    }
};

} // namespace unpd::mmu

#endif // UNPD_MMU_VAD_ENGINE_HPP
