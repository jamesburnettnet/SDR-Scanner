cmake_minimum_required(VERSION 3.19)
project(SdrScanner VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

# --- librtlsdr discovery -----------------------------------------------
# Debian/Ubuntu (librtlsdr-dev) and Homebrew (macOS) provide plain
# pkg-config + headers/lib with no CMake package config, so that's the
# only discovery path tried on those platforms -- this is what was already
# proven working there; no reason to risk it by adding another path.
#
# vcpkg's "rtlsdr" port is different: per its own usage docs it does NOT
# support plain find_path/find_library at all, only a CMake package config
# (find_package(rtlsdr CONFIG REQUIRED), target rtlsdr::rtlsdr). So on
# Windows specifically, that's what's used instead.
if(WIN32)
    find_package(rtlsdr CONFIG REQUIRED)
    set(RTLSDR_TARGET rtlsdr::rtlsdr)
    message(STATUS "librtlsdr found via CMake package config (rtlsdr::rtlsdr)")
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_RTLSDR QUIET librtlsdr)
    endif()

    find_path(RTLSDR_INCLUDE_DIR
        NAMES rtl-sdr.h
        HINTS ${PC_RTLSDR_INCLUDE_DIRS}
    )
    find_library(RTLSDR_LIBRARY
        NAMES rtlsdr librtlsdr
        HINTS ${PC_RTLSDR_LIBRARY_DIRS}
    )

    if(NOT RTLSDR_INCLUDE_DIR OR NOT RTLSDR_LIBRARY)
        message(FATAL_ERROR "librtlsdr not found. Install librtlsdr-dev (Linux) or 'brew install librtlsdr' (macOS).")
    endif()

    message(STATUS "librtlsdr include: ${RTLSDR_INCLUDE_DIR}")
    message(STATUS "librtlsdr library: ${RTLSDR_LIBRARY}")
endif()

set(APP_SOURCES
    src/main.cpp

    src/core/Frequency.h
    src/core/Frequency.cpp
    src/core/FrequencyListModel.h
    src/core/FrequencyListModel.cpp
    src/core/ScanGroup.h
    src/core/ScanGroup.cpp
    src/core/FirDesign.h
    src/core/FirDesign.cpp
    src/core/Decimator.h
    src/core/Decimator.cpp
    src/core/ChannelProcessor.h
    src/core/ChannelProcessor.cpp
    src/core/Squelch.h
    src/core/Squelch.cpp
    src/core/ScannerState.h
    src/core/ScanEngine.h
    src/core/ScanEngine.cpp
    src/core/AudioOutput.h
    src/core/AudioOutput.cpp
    src/core/FrequencyListStore.h
    src/core/FrequencyListStore.cpp
    src/core/SquelchCalibrator.h
    src/core/SquelchCalibrator.cpp
    src/core/ActivityLogModel.h
    src/core/ActivityLogModel.cpp
    src/core/HumFilter.h
    src/core/HumFilter.cpp

    src/sdr/ISdrDevice.h
    src/sdr/RtlSdrDevice.h
    src/sdr/RtlSdrDevice.cpp
    src/sdr/SdrDeviceManager.h
    src/sdr/SdrDeviceManager.cpp

    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/LcdPanel.h
    src/ui/LcdPanel.cpp
    src/ui/FrequencyTableView.h
    src/ui/FrequencyTableView.cpp
    src/ui/AddFrequencyDialog.h
    src/ui/AddFrequencyDialog.cpp
    src/ui/ExploreDialog.h
    src/ui/ExploreDialog.cpp
    src/ui/DeviceBar.h
    src/ui/DeviceBar.cpp
    src/ui/SignalStripChart.h
    src/ui/SignalStripChart.cpp
    src/ui/ActivityLogWidget.h
    src/ui/ActivityLogWidget.cpp
)

add_executable(sdr-scanner ${APP_SOURCES})

target_include_directories(sdr-scanner PRIVATE src)

target_link_libraries(sdr-scanner PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
)

if(RTLSDR_TARGET)
    target_link_libraries(sdr-scanner PRIVATE ${RTLSDR_TARGET})
else()
    target_include_directories(sdr-scanner PRIVATE ${RTLSDR_INCLUDE_DIR})
    target_link_libraries(sdr-scanner PRIVATE ${RTLSDR_LIBRARY})
endif()

if(WIN32)
    target_compile_definitions(sdr-scanner PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# --- Install ---------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS sdr-scanner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX AND NOT APPLE)
    install(FILES resources/sdr-scanner.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/sdr-scanner.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME sdr-scanner.svg)
endif()

if(WIN32)
    # windeployqt bundles the Qt DLLs (and librtlsdr.dll/libusb, since
    # they're picked up as PE dependencies too) next to the executable so
    # the NSIS installer is self-contained -- run at install time so it
    # sees the final staged binary.
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --release --no-translations \
                      \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/sdr-scanner.exe\")")
    endif()
    # librtlsdr/libusb ship as separate runtime DLLs on Windows (e.g. via
    # vcpkg) rather than being statically linked in; grab every DLL out of
    # vcpkg's own installed-triplet bin directory (VCPKG_INSTALLED_DIR/
    # VCPKG_TARGET_TRIPLET are set by vcpkg's toolchain file) so the
    # installer is self-contained regardless of which discovery path found
    # librtlsdr above.
    if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        file(GLOB VCPKG_RUNTIME_DLLS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/*.dll")
        if(VCPKG_RUNTIME_DLLS)
            install(FILES ${VCPKG_RUNTIME_DLLS} DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()
    endif()
endif()

# --- Packaging (CPack) ------------------------------------------------------
# OS_TAG identifies the target OS in package filenames (deb13, ub2604,
# fed44, win) -- CI passes it via -DOS_TAG=..., defaults to "local" for
# plain developer `cpack` runs.
if(NOT DEFINED OS_TAG)
    set(OS_TAG "local")
endif()

# APP_VERSION overrides the package version independently of the
# project()-declared version above (which is just the source-tree
# fallback for local dev builds). The release workflow passes
# -DAPP_VERSION=<tag, minus the leading v> so package filenames match the
# git tag that triggered the build, e.g. v0.0.1 -> 0.0.1.
if(NOT DEFINED APP_VERSION)
    set(APP_VERSION "${PROJECT_VERSION}")
endif()

set(CPACK_PACKAGE_NAME "sdr-scanner")
set(CPACK_PACKAGE_VENDOR "SDR-Scanner Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Analog-style scanner for RTL-SDR and compatible USB SDR dongles")
set(CPACK_PACKAGE_DESCRIPTION
    "SDR Scanner turns an RTL-SDR (or Nooelec NESDR) dongle into a traditional \
analog-style radio scanner: frequency grouping for fast simultaneous squelch \
checking across a 2.4MHz capture window, FM/NFM/AM support, auto/manual \
squelch with a one-shot Auto Tune calibration, an activity log, and a \
mains-hum notch filter.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/JamesBurnettNET/SDR-Scanner")
set(CPACK_PACKAGE_CONTACT "James Burnett")
set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_EXECUTABLES "sdr-scanner" "SDR Scanner")

if(UNIX AND NOT APPLE)
    if(NOT DEFINED CPACK_GENERATOR)
        set(CPACK_GENERATOR "DEB")
    endif()

    if(CPACK_GENERATOR STREQUAL "DEB")
        # Debian package architecture (amd64) differs from
        # CMAKE_SYSTEM_PROCESSOR (x86_64) -- ask dpkg directly.
        execute_process(COMMAND dpkg --print-architecture
                         OUTPUT_VARIABLE DEB_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${DEB_ARCH}")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
        set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
        set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
        # Auto-detects the real shared-library deps (Qt6, librtlsdr, the
        # Qt Multimedia FFmpeg backend, etc.) via dpkg-shlibdeps instead of
        # us hand-maintaining a version-pinned list.
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        # Standard Debian naming (<pkg>_<version>-<revision>_<arch>.deb)
        # with the OS tag folded into the revision, e.g.
        # sdr-scanner_0.1.0-1deb13_amd64.deb
        set(CPACK_DEBIAN_FILE_NAME
            "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}-1${OS_TAG}_${DEB_ARCH}.deb")
    endif()

    if(CPACK_GENERATOR STREQUAL "RPM")
        set(CPACK_RPM_PACKAGE_LICENSE "GPLv3+")
        set(CPACK_RPM_PACKAGE_GROUP "Applications/Communications")
        set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
        # Auto-detects shared-library deps via rpmbuild's find-requires,
        # same rationale as CPACK_DEBIAN_PACKAGE_SHLIBDEPS above.
        set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
        set(CPACK_RPM_FILE_NAME
            "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-1.${OS_TAG}.${CMAKE_SYSTEM_PROCESSOR}.rpm")
    endif()
endif()

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "SDR Scanner")
    set(CPACK_NSIS_DISPLAY_NAME "SDR Scanner")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${OS_TAG}")
endif()

include(CPack)
