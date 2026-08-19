cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(UnsolicitedNonPagedDriver VERSION 1.4.0 LANGUAGES C CXX ASM_MASM RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strip Runtime Checks (/RTC1) from Debug flags for kernel compatibility
string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

option(UNPD_BUILD_TESTS "Build usermode test suite" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-unknown-warning-option>
        $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-character-conversion>
        $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-deprecated-declarations>
    )
endif()

# -----------------------------------------------------------------------------
# WDK & SDK Detection & Configuration
# -----------------------------------------------------------------------------
set(KIT_ROOT "C:/Program Files (x86)/Windows Kits/10")

file(GLOB WDK_VERSIONS RELATIVE "${KIT_ROOT}/Include" "${KIT_ROOT}/Include/10.*")
list(SORT WDK_VERSIONS ORDER DESCENDING)

if(WDK_VERSIONS)
    list(GET WDK_VERSIONS 0 WDK_VERSION)
else()
    set(WDK_VERSION "10.0.28000.0")
endif()

set(WDK_INC_KM "${KIT_ROOT}/Include/${WDK_VERSION}/km")
set(WDK_INC_SHARED "${KIT_ROOT}/Include/${WDK_VERSION}/shared")
set(WDK_INC_CRT "${KIT_ROOT}/Include/${WDK_VERSION}/km/crt")
set(WDK_LIB_KM "${KIT_ROOT}/Lib/${WDK_VERSION}/km/x64")

# Fallback SDK headers for specstrings.h
set(SDK_INC_SHARED "${KIT_ROOT}/Include/10.0.26100.0/shared")
set(SDK_INC_UM "${KIT_ROOT}/Include/10.0.26100.0/um")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# -----------------------------------------------------------------------------
# Target: unpd.sys (Kernel Driver)
# -----------------------------------------------------------------------------
if(EXISTS "${WDK_INC_KM}")
    set(DRIVER_SOURCES
        src/driver/driver_entry.cpp
        src/driver/device_control.cpp
        src/driver/ioctl_handler.cpp
        src/driver/memory_manager.cpp
        src/driver/page_engine.cpp
        src/driver/memory/universal_memory.cpp
        src/driver/mmu/paging_engine.cpp
        src/driver/mmu/physical_memory.cpp
        src/driver/mmu/cr3_walker.cpp
        src/driver/stealth/piddb.cpp
        src/driver/stealth/unloaded_drivers.cpp
        src/driver/comm/shared_memory.cpp
        src/driver/exec/apc.cpp
        src/driver/kernel_asm.asm
        src/driver/unpd.rc
    )

    add_executable(unpd ${DRIVER_SOURCES})
    set_target_properties(unpd PROPERTIES
        SUFFIX ".sys"
        OUTPUT_NAME "unpd"
    )

    target_include_directories(unpd PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_include_directories(unpd PRIVATE
        ${WDK_INC_KM}
        ${WDK_INC_SHARED}
        ${WDK_INC_CRT}
        ${SDK_INC_SHARED}
        ${SDK_INC_UM}
    )

    target_compile_definitions(unpd PRIVATE
        $<$<COMPILE_LANGUAGE:CXX,C>:_KERNEL_MODE=1>
        $<$<COMPILE_LANGUAGE:CXX,C>:_AMD64_=1>
        $<$<COMPILE_LANGUAGE:CXX,C>:_WIN64=1>
        $<$<COMPILE_LANGUAGE:CXX,C>:NTDDI_VERSION=0x0A00000C>
        $<$<COMPILE_LANGUAGE:CXX,C>:WINVER=0x0A00>
        $<$<COMPILE_LANGUAGE:CXX,C>:_WIN32_WINNT=0x0A00>
        $<$<COMPILE_LANGUAGE:CXX,C>:POOL_ZERO_DOWN_LEVEL_SUPPORT=1>
    )

    target_compile_options(unpd PRIVATE
        $<$<COMPILE_LANGUAGE:CXX,C>:/kernel>
        $<$<COMPILE_LANGUAGE:CXX,C>:/GR->
        $<$<COMPILE_LANGUAGE:CXX,C>:/GS>
        $<$<COMPILE_LANGUAGE:CXX,C>:/Gy>
        $<$<COMPILE_LANGUAGE:CXX,C>:/GF>
        $<$<COMPILE_LANGUAGE:CXX,C>:/Zc:wchar_t>
        $<$<COMPILE_LANGUAGE:CXX,C>:/Zc:forScope>
        $<$<COMPILE_LANGUAGE:CXX,C>:/Zc:inline>
        $<$<COMPILE_LANGUAGE:CXX,C>:/fp:precise>
        $<$<COMPILE_LANGUAGE:CXX,C>:/W4>
        $<$<COMPILE_LANGUAGE:CXX,C>:/WX>
        $<$<COMPILE_LANGUAGE:CXX,C>:/wd4005>
        $<$<COMPILE_LANGUAGE:CXX,C>:/wd4083>
        $<$<COMPILE_LANGUAGE:CXX,C>:/wd4201>
        $<$<AND:$<COMPILE_LANGUAGE:CXX,C>,$<CONFIG:Debug>>:/Od>
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(unpd PRIVATE
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-unknown-warning-option>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-ignored-pragma-intrinsic>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-pragma-pack>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-macro-redefined>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-invalid-token-paste>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-inconsistent-dllimport>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-nonportable-include-path>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-unknown-pragmas>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-microsoft-enum-forward-reference>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-ignored-attributes>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-sizeof-array-div>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-unused-but-set-variable>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-unused-variable>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-microsoft-include>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-c++11-narrowing>
        )
    endif()

    target_link_options(unpd PRIVATE
        /DRIVER
        /ENTRY:DriverEntry
        /SUBSYSTEM:NATIVE
        /NODEFAULTLIB
        /MANIFEST:NO
        /GUARD:CF
        /DYNAMICBASE
        /NXCOMPAT
        /OPT:REF
        /OPT:ICF
        /SECTION:.rdata,DR
        /LIBPATH:${WDK_LIB_KM}
    )

    target_link_libraries(unpd PRIVATE
        "${WDK_LIB_KM}/ntoskrnl.lib"
        "${WDK_LIB_KM}/hal.lib"
        "${WDK_LIB_KM}/wdm.lib"
        "${WDK_LIB_KM}/BufferOverflowK.lib"
    )

    add_custom_target(sign_driver
        COMMAND powershell.exe -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/Sign-Driver.ps1 -DriverPath $<TARGET_FILE:unpd>
        DEPENDS unpd
        COMMENT "Signing driver binary with local test certificate..."
    )

    install(TARGETS unpd
        EXPORT unpdTargets
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
    )
endif()

# -----------------------------------------------------------------------------
# Target: unpd_tests.exe (GoogleTest Suite)
# -----------------------------------------------------------------------------
if(UNPD_BUILD_TESTS)
    include(FetchContent)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)

    set(TEST_SOURCES
        src/tests/main.cpp
        src/tests/test_ioctl.cpp
        src/tests/test_page_engine.cpp
        src/tests/test_fuzzing.cpp
        src/tests/test_stress.cpp
        src/tests/test_mmu.cpp
        src/tests/test_stealth.cpp
        src/tests/test_cr3_walker.cpp
        src/tests/test_virtual_mmu.cpp
        src/tests/test_kstd.cpp
        src/tests/test_shared_memory.cpp
        src/tests/emulator/virtual_mmu.cpp
        src/driver/comm/shared_memory.cpp
        src/driver/kernel_asm.asm
    )

    add_executable(unpd_tests ${TEST_SOURCES})

    target_include_directories(unpd_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tests
    )

    target_compile_definitions(unpd_tests PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )

    target_compile_options(unpd_tests PRIVATE
        $<$<COMPILE_LANGUAGE:CXX,C>:/W4>
        $<$<COMPILE_LANGUAGE:CXX,C>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX,C>:/EHsc>
        $<$<COMPILE_LANGUAGE:CXX,C>:/wd4201>
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(unpd_tests PRIVATE
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-unknown-warning-option>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-unused-lambda-capture>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-nonportable-include-path>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-c++11-narrowing>
            $<$<COMPILE_LANGUAGE:CXX,C>:-Wno-character-conversion>
        )
    endif()

    target_link_libraries(unpd_tests PRIVATE
        gtest
        Advapi32.lib
    )

    include(CTest)
    add_test(NAME UnpdGoogleTests COMMAND unpd_tests)
endif()

# -----------------------------------------------------------------------------
# Installation & CMake Package Configuration (vcpkg compatibility)
# -----------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(TARGET unpd)
    install(EXPORT unpdTargets
        FILE unpdTargets.cmake
        NAMESPACE unpd::
        DESTINATION lib/cmake/unpd
    )
endif()

configure_package_config_file(
    cmake/unpdConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/unpdConfig.cmake
    INSTALL_DESTINATION lib/cmake/unpd
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/unpdConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/unpdConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/unpdConfigVersion.cmake
    DESTINATION lib/cmake/unpd
)
