#include <gtest/gtest.h>
#include "unpd/syscall.hpp"

TEST(SyscallTest, GatewayStructInitialization) {
    unpd::syscall::UnpdSyscallGateway gateway;
    (void)gateway;
    SUCCEED();
}

TEST(SyscallTest, SyscallPingMockFallback) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    uint32_t vMajor = 0, vMinor = 0;
    bool ok = unpd::syscall::UnpdSyscallGateway::SyscallPing(handle, vMajor, vMinor);
    EXPECT_FALSE(ok);
}

TEST(SyscallTest, SyscallReadCr3InvalidHandle) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    char buffer[64]{};
    uint64_t transferred = 0;
    bool ok = unpd::syscall::UnpdSyscallGateway::SyscallReadCr3(handle, 0x1AA000, 0x7FF600000000, buffer, sizeof(buffer), transferred);
    EXPECT_FALSE(ok);
}

TEST(SyscallTest, SyscallWriteCr3InvalidHandle) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    char buffer[64]{};
    uint64_t transferred = 0;
    bool ok = unpd::syscall::UnpdSyscallGateway::SyscallWriteCr3(handle, 0x1AA000, 0x7FF600000000, buffer, sizeof(buffer), transferred);
    EXPECT_FALSE(ok);
}

TEST(SyscallTest, SyscallAllocateFreeNonPagedInvalidHandle) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    uint64_t allocHandle = 0;
    bool okAlloc = unpd::syscall::UnpdSyscallGateway::SyscallAllocateNonPaged(handle, 1024, allocHandle);
    EXPECT_FALSE(okAlloc);

    bool okFree = unpd::syscall::UnpdSyscallGateway::SyscallFreeNonPaged(handle, 100);
    EXPECT_FALSE(okFree);
}

TEST(SyscallTest, SyscallSimdPatternScanInvalidHandle) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    uint8_t pattern[] = { 0xAA, 0xBB, 0xCC };
    uint64_t matchAddr = 0;
    bool ok = unpd::syscall::UnpdSyscallGateway::SyscallSimdPatternScan(
        handle, 0x7FF600000000, 0x1000, pattern, sizeof(pattern), "xxx", matchAddr
    );
    EXPECT_FALSE(ok);
}

TEST(SyscallTest, SyscallResolveVmtInvalidHandle) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    uint64_t vtableAddr = 0, firstMethod = 0;
    uint32_t count = 0;
    bool ok = unpd::syscall::UnpdSyscallGateway::SyscallResolveVmt(
        handle, 0x7FF600000000, 0x5000, 0x7FF600001000, 0x2000, vtableAddr, count, firstMethod
    );
    EXPECT_FALSE(ok);
}

TEST(SyscallTest, SyscallMoveMouseRelativeInvalidHandle) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    bool ok = unpd::syscall::UnpdSyscallGateway::SyscallMoveMouseRelative(handle, 10, -5);
    EXPECT_FALSE(ok);
}

TEST(SyscallTest, SyscallQueryProcessBaseInvalidHandle) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    uint64_t baseAddr = 0, pebAddr = 0;
    bool ok = unpd::syscall::UnpdSyscallGateway::SyscallQueryProcessBase(handle, 1234, baseAddr, pebAddr);
    EXPECT_FALSE(ok);
}
