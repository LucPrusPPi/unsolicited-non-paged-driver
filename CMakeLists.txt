cmake_minimum_required(VERSION 3.20)
project(UnsolicitedNonPagedDriver VERSION 1.0.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UNPD_BUILD_TESTS "Build usermode test suite" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# -----------------------------------------------------------------------------
# WDK Detection & Configuration
# -----------------------------------------------------------------------------
set(WDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
file(GLOB WDK_VERSIONS RELATIVE "${WDK_ROOT}/Include" "${WDK_ROOT}/Include/10.*")
list(SORT WDK_VERSIONS ORDER DESCENDING)

if(WDK_VERSIONS)
    list(GET WDK_VERSIONS 0 WDK_VERSION)
    message(STATUS "Detected Windows Kit / WDK version: ${WDK_VERSION}")
else()
    set(WDK_VERSION "10.0.28000.0")
    message(WARNING "Could not automatically detect WDK version, defaulting to: ${WDK_VERSION}")
endif()

set(WDK_INC_KM "${WDK_ROOT}/Include/${WDK_VERSION}/km")
set(WDK_INC_SHARED "${WDK_ROOT}/Include/${WDK_VERSION}/shared")
set(WDK_INC_UCRT "${WDK_ROOT}/Include/${WDK_VERSION}/ucrt")
set(WDK_INC_CRT "${WDK_ROOT}/Include/${WDK_VERSION}/km/crt")

set(WDK_LIB_KM "${WDK_ROOT}/Lib/${WDK_VERSION}/km/x64")
set(WDK_LIB_UCRT "${WDK_ROOT}/Lib/${WDK_VERSION}/ucrt/x64")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# -----------------------------------------------------------------------------
# Target: unpd.sys (Kernel Driver)
# -----------------------------------------------------------------------------
set(DRIVER_SOURCES
    src/driver/driver_entry.cpp
    src/driver/device_control.cpp
    src/driver/ioctl_handler.cpp
    src/driver/memory_manager.cpp
    src/driver/page_engine.cpp
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
)

target_compile_definitions(unpd PRIVATE
    _KERNEL_MODE=1
    _AMD64_=1
    _WIN64=1
    NTDDI_VERSION=0x0A00000C
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    POOL_ZERO_DOWN_LEVEL_SUPPORT=1
)

target_compile_options(unpd PRIVATE
    /kernel
    /GR-
    /GS
    /Gy
    /GF
    /Zc:wchar_t
    /Zc:forScope
    /Zc:inline
    /fp:precise
    /W4
    /WX
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(unpd PRIVATE
        -Wno-ignored-pragma-intrinsic
        -Wno-pragma-pack
        -Wno-macro-redefined
        -Wno-invalid-token-paste
        -Wno-inconsistent-dllimport
        -Wno-nonportable-include-path
        -Wno-unknown-pragmas
        -Wno-microsoft-enum-forward-reference
        -Wno-ignored-attributes
        -Wno-sizeof-array-div
        -Wno-unused-but-set-variable
        -Wno-unused-variable
        -Wno-microsoft-include
        -Wno-c++11-narrowing
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

# -----------------------------------------------------------------------------
# Target: unpd_tests.exe (Usermode Test Harness)
# -----------------------------------------------------------------------------
if(UNPD_BUILD_TESTS)
    set(TEST_SOURCES
        src/tests/test_runner.cpp
        src/tests/test_ioctl.cpp
        src/tests/test_fuzzing.cpp
        src/tests/test_stress.cpp
        src/tests/test_page_engine.cpp
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
        /W4
        /permissive-
        /EHsc
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(unpd_tests PRIVATE
            -Wno-unused-lambda-capture
            -Wno-nonportable-include-path
            -Wno-c++11-narrowing
        )
    endif()

    target_link_libraries(unpd_tests PRIVATE
        Advapi32.lib
    )
endif()

# -----------------------------------------------------------------------------
# Custom Target: sign_driver
# -----------------------------------------------------------------------------
add_custom_target(sign_driver
    COMMAND powershell.exe -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/Sign-Driver.ps1 -DriverPath $<TARGET_FILE:unpd>
    DEPENDS unpd
    COMMENT "Signing driver binary with local test certificate..."
)

# -----------------------------------------------------------------------------
# Installation & CMake Package Configuration (vcpkg compatibility)
# -----------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS unpd
    EXPORT unpdTargets
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
)

install(EXPORT unpdTargets
    FILE unpdTargets.cmake
    NAMESPACE unpd::
    DESTINATION lib/cmake/unpd
)

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
