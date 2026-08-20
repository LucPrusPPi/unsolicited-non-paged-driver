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
