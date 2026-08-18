cmake_minimum_required(VERSION 3.20)
project(UnsolicitedNonPagedDriver VERSION 1.0.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    src/driver/unpd.rc
)

add_executable(unpd ${DRIVER_SOURCES})
set_target_properties(unpd PROPERTIES
    SUFFIX ".sys"
    OUTPUT_NAME "unpd"
)

target_include_directories(unpd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${WDK_INC_KM}
    ${WDK_INC_SHARED}
    ${WDK_INC_CRT}
)

target_compile_definitions(unpd PRIVATE
    _KERNEL_MODE=1
    _AMD64_=1
    _WIN64=1
    NTDDI_VERSION=0x0A00000C # NTDDI_WIN10_NI / modern Win11
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
set(TEST_SOURCES
    src/tests/test_runner.cpp
    src/tests/test_ioctl.cpp
    src/tests/test_fuzzing.cpp
    src/tests/test_stress.cpp
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

target_link_libraries(unpd_tests PRIVATE
    Advapi32.lib
)

# -----------------------------------------------------------------------------
# Custom Target: sign_driver
# -----------------------------------------------------------------------------
add_custom_target(sign_driver
    COMMAND powershell.exe -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/Sign-Driver.ps1 -DriverPath $<TARGET_FILE:unpd>
    DEPENDS unpd
    COMMENT "Signing driver binary with local test certificate..."
)
