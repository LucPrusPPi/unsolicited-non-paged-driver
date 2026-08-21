#include "unpd/page_engine.hpp"
#include "unpd/mmu/paging_engine.hpp"
#include "unpd/kernel_raii.hpp"

static const uint32_t g_SlabSizes[4] = { 64, 256, 1024, 4096 };

static PVOID SafeMmMapLockedPages(PMDL mdl) {
    PVOID userVa = nullptr;
    __try {
        userVa = MmMapLockedPagesSpecifyCache(
            mdl,
            UserMode,
            MmCached,
            nullptr,
            FALSE,
            NormalPagePriority | MdlMappingNoExecute
        );
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        userVa = nullptr;
    }
    return userVa;
}

static VOID SafeMmUnmapLockedPages(PVOID userVa, PMDL mdl) {
    if (userVa == nullptr || mdl == nullptr) {
        return;
    }
    __try {
        MmUnmapLockedPages(userVa, mdl);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safe swallow
    }
}

NTSTATUS UnpdInitPageEngine(PUNPD_PAGE_ENGINE engine) {
    if (engine == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(engine, sizeof(UNPD_PAGE_ENGINE));
    KeInitializeSpinLock(&engine->EngineLock);
    InitializeListHead(&engine->SessionListHead);
    engine->NextSessionHandle = 1;

    for (int i = 0; i < 4; ++i) {
        PUNPD_SLAB_CACHE slab = &engine->Slabs[i];
        slab->BlockSize = g_SlabSizes[i];
        KeInitializeSpinLock(&slab->CacheLock);

        SIZE_T cacheSize = 64 * 1024; // 64 KB per slab class
        slab->PageBacking = UnpdAllocatePool(cacheSize, UNPD_SLAB_TAG);
        if (slab->PageBacking != nullptr) {
            RtlZeroMemory(slab->PageBacking, cacheSize);
            uint32_t blockCount = static_cast<uint32_t>(cacheSize / slab->BlockSize);
            slab->TotalBlocks = blockCount;
            slab->FreeBlocks = blockCount;

            auto* current = static_cast<uint8_t*>(slab->PageBacking);
            for (uint32_t b = 0; b < blockCount; ++b) {
                auto* block = reinterpret_cast<PUNPD_SLAB_BLOCK>(current);
                block->Next = slab->FreeListHead;
                slab->FreeListHead = block;
                current += slab->BlockSize;
            }
        }
    }

    return STATUS_SUCCESS;
}

VOID UnpdCleanupPageEngine(PUNPD_PAGE_ENGINE engine) {
    if (engine == nullptr) {
        return;
    }

    {
        unpd::SpinlockGuard guard(&engine->EngineLock);

        while (!IsListEmpty(&engine->SessionListHead)) {
            PLIST_ENTRY entry = RemoveHeadList(&engine->SessionListHead);
            auto* session = CONTAINING_RECORD(entry, UNPD_SHARED_SESSION, ListEntry);

            if (session->UserVa != nullptr && session->Mdl != nullptr) {
                SafeMmUnmapLockedPages(session->UserVa, session->Mdl);
                session->UserVa = nullptr;
            }

            if (session->KernelVa != nullptr) {
                ExFreePoolWithTag(session->KernelVa, UNPD_PAGE_TAG);
                session->KernelVa = nullptr;
            }

            if (session->Mdl != nullptr) {
                IoFreeMdl(session->Mdl);
                session->Mdl = nullptr;
            }

            ExFreePoolWithTag(session, UNPD_POOL_TAG);
        }
        engine->ActiveSessions = 0;
    }

    for (int i = 0; i < 4; ++i) {
        PUNPD_SLAB_CACHE slab = &engine->Slabs[i];
        if (slab->PageBacking != nullptr) {
            ExFreePoolWithTag(slab->PageBacking, UNPD_SLAB_TAG);
            slab->PageBacking = nullptr;
            slab->FreeListHead = nullptr;
        }
    }
}

NTSTATUS UnpdCreateSharedSession(
    PUNPD_PAGE_ENGINE engine,
    uint32_t pageCount,
    uint64_t* outSessionHandle,
    PVOID* outUserVa,
    SIZE_T* outTotalBytes
) {
    if (engine == nullptr || pageCount == 0 || pageCount > 256) {
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T totalBytes = static_cast<SIZE_T>(pageCount) * PAGE_SIZE;

    PVOID kernelVa = UnpdAllocatePool(totalBytes, UNPD_PAGE_TAG);
    if (kernelVa == nullptr) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(kernelVa, totalBytes);

    PMDL mdl = IoAllocateMdl(kernelVa, static_cast<ULONG>(totalBytes), FALSE, FALSE, nullptr);
    if (mdl == nullptr) {
        UnpdFreePool(kernelVa, UNPD_PAGE_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    MmBuildMdlForNonPagedPool(mdl);

    PVOID userVa = SafeMmMapLockedPages(mdl);
    if (userVa == nullptr) {
        IoFreeMdl(mdl);
        UnpdFreePool(kernelVa, UNPD_PAGE_TAG);
        return STATUS_UNSUCCESSFUL;
    }

    auto* session = static_cast<PUNPD_SHARED_SESSION>(UnpdAllocatePool(sizeof(UNPD_SHARED_SESSION), UNPD_POOL_TAG));
    if (session == nullptr) {
        SafeMmUnmapLockedPages(userVa, mdl);
        IoFreeMdl(mdl);
        UnpdFreePool(kernelVa, UNPD_PAGE_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(session, sizeof(UNPD_SHARED_SESSION));
    session->Mdl = mdl;
    session->KernelVa = kernelVa;
    session->UserVa = userVa;
    session->Process = IoGetCurrentProcess();
    session->ByteCount = totalBytes;
    session->PageCount = pageCount;
    session->ActiveBufferIndex = 0;
    session->TotalSwaps = 0;
    KeInitializeSpinLock(&session->SessionLock);

    uint64_t handle = 0;
    {
        unpd::SpinlockGuard guard(&engine->EngineLock);
        handle = engine->NextSessionHandle++;
        session->SessionHandle = handle;
        InsertTailList(&engine->SessionListHead, &session->ListEntry);
        engine->ActiveSessions++;
    }

    *outSessionHandle = handle;
    *outUserVa = userVa;
    *outTotalBytes = totalBytes;

    return STATUS_SUCCESS;
}

NTSTATUS UnpdDestroySharedSession(
    PUNPD_PAGE_ENGINE engine,
    uint64_t sessionHandle
) {
    if (engine == nullptr || sessionHandle == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    PUNPD_SHARED_SESSION target = nullptr;
    {
        unpd::SpinlockGuard guard(&engine->EngineLock);

        PLIST_ENTRY curr = engine->SessionListHead.Flink;
        while (curr != &engine->SessionListHead) {
            auto* item = CONTAINING_RECORD(curr, UNPD_SHARED_SESSION, ListEntry);
            if (item->SessionHandle == sessionHandle) {
                RemoveEntryList(&item->ListEntry);
                target = item;
                engine->ActiveSessions--;
                break;
            }
            curr = curr->Flink;
        }
    }

    if (target == nullptr) {
        return STATUS_NOT_FOUND;
    }

    if (target->UserVa != nullptr && target->Mdl != nullptr) {
        if (target->Process != nullptr) {
            unpd::mmu::ProcessAttachmentGuard attachGuard(reinterpret_cast<PEPROCESS>(target->Process));
            SafeMmUnmapLockedPages(target->UserVa, target->Mdl);
        } else {
            SafeMmUnmapLockedPages(target->UserVa, target->Mdl);
        }
        target->UserVa = nullptr;
    }

    if (target->KernelVa != nullptr) {
        ExFreePoolWithTag(target->KernelVa, UNPD_PAGE_TAG);
        target->KernelVa = nullptr;
    }

    if (target->Mdl != nullptr) {
        IoFreeMdl(target->Mdl);
        target->Mdl = nullptr;
    }

    ExFreePoolWithTag(target, UNPD_POOL_TAG);
    return STATUS_SUCCESS;
}

NTSTATUS UnpdSwapSessionBuffers(
    PUNPD_PAGE_ENGINE engine,
    uint64_t sessionHandle,
    uint32_t* outActiveIndex,
    uint32_t* outStandbyIndex,
    uint64_t* outTotalSwaps
) {
    if (engine == nullptr || sessionHandle == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    PUNPD_SHARED_SESSION target = nullptr;
    {
        unpd::SpinlockGuard guard(&engine->EngineLock);

        PLIST_ENTRY curr = engine->SessionListHead.Flink;
        while (curr != &engine->SessionListHead) {
            auto* item = CONTAINING_RECORD(curr, UNPD_SHARED_SESSION, ListEntry);
            if (item->SessionHandle == sessionHandle) {
                target = item;
                break;
            }
            curr = curr->Flink;
        }
    }

    if (target == nullptr) {
        return STATUS_NOT_FOUND;
    }

    LONG oldIndex = InterlockedExchange(&target->ActiveBufferIndex, target->ActiveBufferIndex == 0 ? 1 : 0);
    LONG newIndex = oldIndex == 0 ? 1 : 0;

    uint64_t swaps = InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&target->TotalSwaps));
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&engine->TotalSwaps));

    *outActiveIndex = static_cast<uint32_t>(newIndex);
    *outStandbyIndex = static_cast<uint32_t>(oldIndex);
    *outTotalSwaps = swaps;

    return STATUS_SUCCESS;
}

NTSTATUS UnpdSlabAllocate(
    PUNPD_PAGE_ENGINE engine,
    uint32_t blockClass,
    PVOID* outAddress,
    uint32_t* outBlockSize
) {
    if (engine == nullptr || blockClass >= 4) {
        return STATUS_INVALID_PARAMETER;
    }

    PUNPD_SLAB_CACHE slab = &engine->Slabs[blockClass];
    PUNPD_SLAB_BLOCK block = nullptr;

    {
        unpd::SpinlockGuard guard(&slab->CacheLock);
        if (slab->FreeListHead != nullptr) {
            block = slab->FreeListHead;
            slab->FreeListHead = block->Next;
            slab->FreeBlocks--;
        }
    }

    if (block == nullptr) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(block, slab->BlockSize);
    *outAddress = static_cast<PVOID>(block);
    *outBlockSize = slab->BlockSize;

    return STATUS_SUCCESS;
}

NTSTATUS UnpdSlabFree(
    PUNPD_PAGE_ENGINE engine,
    uint32_t blockClass,
    PVOID address
) {
    if (engine == nullptr || blockClass >= 4 || address == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    PUNPD_SLAB_CACHE slab = &engine->Slabs[blockClass];
    auto* block = static_cast<PUNPD_SLAB_BLOCK>(address);

    {
        unpd::SpinlockGuard guard(&slab->CacheLock);
        block->Next = slab->FreeListHead;
        slab->FreeListHead = block;
        slab->FreeBlocks++;
    }

    return STATUS_SUCCESS;
}
