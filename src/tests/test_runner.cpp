#include "test_client.hpp"
#include <iostream>
#include <vector>
#include <functional>
#include <string>
#include <chrono>

struct TestCase {
    std::string name;
    std::function<bool(unpd::test::DriverClient&)> fn;
};

extern void RegisterIoctlTests(std::vector<TestCase>& tests);
extern void RegisterFuzzingTests(std::vector<TestCase>& tests);
extern void RegisterStressTests(std::vector<TestCase>& tests);
extern void RegisterPageEngineTests(std::vector<TestCase>& tests);

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "========================================================\n";
    std::cout << " Unsolicited Non-Paged Driver (UNPD) Usermode Test Suite\n";
    std::cout << "========================================================\n\n";

    unpd::test::DriverClient client;
    if (!client.open()) {
        std::cerr << "[!] Error: Driver device handle could not be opened.\n";
        std::cerr << "    Ensure unpd.sys is loaded and running (e.g. via 'sc start unpd').\n";
        return 1;
    }

    std::wcout << L"[+] Successfully connected to driver device: " << UNPD_USERMODE_PATH_W << L"\n\n";

    std::vector<TestCase> tests;
    RegisterIoctlTests(tests);
    RegisterFuzzingTests(tests);
    RegisterStressTests(tests);
    RegisterPageEngineTests(tests);

    size_t passed = 0;
    size_t failed = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < tests.size(); ++i) {
        std::cout << "[" << (i + 1) << "/" << tests.size() << "] RUN:  " << tests[i].name << "... ";
        std::cout.flush();

        auto t0 = std::chrono::high_resolution_clock::now();
        bool ok = false;
        try {
            ok = tests[i].fn(client);
        } catch (const std::exception& ex) {
            std::cerr << "\n    Exception caught: " << ex.what();
            ok = false;
        } catch (...) {
            std::cerr << "\n    Unknown exception caught.";
            ok = false;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

        if (ok) {
            std::cout << "PASS (" << durationUs << " us)\n";
            passed++;
        } else {
            std::cout << "FAIL (" << durationUs << " us)\n";
            failed++;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "\n--------------------------------------------------------\n";
    std::cout << "Test Summary: " << passed << " Passed, " << failed << " Failed, " << tests.size() << " Total\n";
    std::cout << "Total Elapsed Time: " << totalMs << " ms\n";
    std::cout << "========================================================\n";

    return (failed == 0) ? 0 : 1;
}
