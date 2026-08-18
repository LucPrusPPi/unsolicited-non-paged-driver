#include <gtest/gtest.h>
#include <iostream>
#include "test_client.hpp"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    unpd::test::DriverClient probe;
    if (probe.isMockMode()) {
        std::cout << "[INFO] Running in Mock / CI loopback mode (Driver not loaded into kernel)\n";
    } else {
        std::cout << "[INFO] Running in Real Kernel Mode (Connected to \\\\.\\UnsolicitedNonPagedDriver)\n";
    }

    return RUN_ALL_TESTS();
}
