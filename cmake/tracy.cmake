set(TRACY_PATH ${CMAKE_CURRENT_LIST_DIR}/..)
set(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../")

include(FindPkgConfig)
include(${TRACY_PATH}/cmake/config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

option(NO_FILESELECTOR "Disable the file selector" OFF)
option(GTK_FILESELECTOR "Use the GTK file selector on Linux instead of the xdg-portal one" OFF)
option(LEGACY "Instead of Wayland, use the legacy X11 backend on Linux" OFF)
option(NO_ISA_EXTENSIONS "Disable ISA extensions (don't pass -march=native or -mcpu=native to the compiler)" OFF)
option(NO_STATISTICS "Disable calculation of statistics" OFF)
option(SELF_PROFILE "Enable self-profiling" OFF)

if(SELF_PROFILE)
    add_definitions(-DTRACY_ENABLE)
    add_compile_options(-g -O3 -fno-omit-frame-pointer)
endif()

set(SERVER_FILES
    TracyColor.cpp
    TracyEventDebug.cpp
    TracyFileselector.cpp
    TracyMicroArchitecture.cpp
    TracyProtoHistory.cpp
    TracyStorage.cpp
    TracyWeb.cpp
)
list(TRANSFORM SERVER_FILES PREPEND "${TRACY_PATH}/profiler/src/profiler/")

set(PROFILER_FILES
        src/ConnectionHistory.cpp
        src/HttpRequest.cpp
        src/ini.c
        src/IsElevated.cpp
        src/ResolvService.cpp
        src/RunQueue.cpp
        src/WindowPosition.cpp
        src/winmain.cpp
        src/winmainArchDiscovery.cpp
)
list(TRANSFORM PROFILER_FILES PREPEND "${TRACY_PATH}/profiler/")

pkg_check_modules(CAPSTONE capstone)
if(CAPSTONE_FOUND AND NOT DOWNLOAD_CAPSTONE)
    message(STATUS "Capstone found: ${CAPSTONE}")
    add_library(TracyCapstone INTERFACE)
    target_include_directories(TracyCapstone INTERFACE ${CAPSTONE_INCLUDE_DIRS})
    target_link_libraries(TracyCapstone INTERFACE ${CAPSTONE_LINK_LIBRARIES})
else()
    CPMAddPackage(
            NAME capstone
            GITHUB_REPOSITORY capstone-engine/capstone
            GIT_TAG 6.0.0-Alpha1
            OPTIONS
            "CAPSTONE_X86_ATT_DISABLE ON"
            "CAPSTONE_ALPHA_SUPPORT OFF"
            "CAPSTONE_HPPA_SUPPORT OFF"
            "CAPSTONE_LOONGARCH_SUPPORT OFF"
            "CAPSTONE_M680X_SUPPORT OFF"
            "CAPSTONE_M68K_SUPPORT OFF"
            "CAPSTONE_MIPS_SUPPORT OFF"
            "CAPSTONE_MOS65XX_SUPPORT OFF"
            "CAPSTONE_PPC_SUPPORT OFF"
            "CAPSTONE_SPARC_SUPPORT OFF"
            "CAPSTONE_SYSTEMZ_SUPPORT OFF"
            "CAPSTONE_XCORE_SUPPORT OFF"
            "CAPSTONE_TRICORE_SUPPORT OFF"
            "CAPSTONE_TMS320C64X_SUPPORT OFF"
            "CAPSTONE_M680X_SUPPORT OFF"
            "CAPSTONE_EVM_SUPPORT OFF"
            "CAPSTONE_WASM_SUPPORT OFF"
            "CAPSTONE_BPF_SUPPORT OFF"
            "CAPSTONE_RISCV_SUPPORT OFF"
            "CAPSTONE_SH_SUPPORT OFF"
            "CAPSTONE_XTENSA_SUPPORT OFF"
            "CAPSTONE_BUILD_MACOS_THIN ON"
            EXCLUDE_FROM_ALL TRUE
    )
    add_library(TracyCapstone INTERFACE)
    target_include_directories(TracyCapstone INTERFACE ${capstone_SOURCE_DIR}/include/capstone)
    target_link_libraries(TracyCapstone INTERFACE capstone)
endif()


set(ZSTD_DIR "${ROOT_DIR}/zstd")

set(ZSTD_SOURCES
        decompress/zstd_ddict.c
        decompress/zstd_decompress_block.c
        decompress/huf_decompress.c
        decompress/zstd_decompress.c
        common/zstd_common.c
        common/error_private.c
        common/xxhash.c
        common/entropy_common.c
        common/debug.c
        common/threading.c
        common/pool.c
        common/fse_decompress.c
        compress/zstd_ldm.c
        compress/zstd_compress_superblock.c
        compress/zstd_opt.c
        compress/zstd_compress_sequences.c
        compress/fse_compress.c
        compress/zstd_double_fast.c
        compress/zstd_compress.c
        compress/zstd_compress_literals.c
        compress/hist.c
        compress/zstdmt_compress.c
        compress/zstd_lazy.c
        compress/huf_compress.c
        compress/zstd_fast.c
        dictBuilder/zdict.c
        dictBuilder/cover.c
        dictBuilder/divsufsort.c
        dictBuilder/fastcover.c
)

list(TRANSFORM ZSTD_SOURCES PREPEND "${ZSTD_DIR}/")

set_property(SOURCE ${ZSTD_DIR}/decompress/huf_decompress_amd64.S APPEND PROPERTY COMPILE_OPTIONS "-x" "assembler-with-cpp")

add_library(TracyZstd STATIC EXCLUDE_FROM_ALL ${ZSTD_SOURCES})
target_include_directories(TracyZstd PUBLIC ${ZSTD_DIR})
target_compile_definitions(TracyZstd PRIVATE ZSTD_DISABLE_ASM)

if (NOT NO_FILESELECTOR AND NOT EMSCRIPTEN)
    set(NFD_DIR "${ROOT_DIR}/nfd")

    if (WIN32)
        set(NFD_SOURCES "${NFD_DIR}/nfd_win.cpp")
    elseif (APPLE)
        set(NFD_SOURCES "${NFD_DIR}/nfd_cocoa.m")
    else()
        if (GTK_FILESELECTOR)
            set(NFD_SOURCES "${NFD_DIR}/nfd_gtk.cpp")
        else()
            set(NFD_SOURCES "${NFD_DIR}/nfd_portal.cpp")
        endif()
    endif()

    file(GLOB_RECURSE NFD_HEADERS CONFIGURE_DEPENDS ${NFD_DIR} "*.h")
    add_library(TracyNfd STATIC EXCLUDE_FROM_ALL ${NFD_SOURCES} ${NFD_HEADERS})
    target_include_directories(TracyNfd PUBLIC ${NFD_DIR})

    if (APPLE)
        find_library(APPKIT_LIBRARY AppKit)
        find_library(UNIFORMTYPEIDENTIFIERS_LIBRARY UniformTypeIdentifiers)
        target_link_libraries(TracyNfd PUBLIC ${APPKIT_LIBRARY} ${UNIFORMTYPEIDENTIFIERS_LIBRARY})
    elseif (UNIX)
        if (GTK_FILESELECTOR)
            pkg_check_modules(GTK3 gtk+-3.0)
            if (NOT GTK3_FOUND)
                message(FATAL_ERROR "GTK3 not found. Please install it or set GTK_FILESELECTOR to OFF.")
            endif()
            add_library(TracyGtk3 INTERFACE)
            target_include_directories(TracyGtk3 INTERFACE ${GTK3_INCLUDE_DIRS})
            target_link_libraries(TracyGtk3 INTERFACE ${GTK3_LINK_LIBRARIES})
            target_link_libraries(TracyNfd PUBLIC TracyGtk3)
        else()
            pkg_check_modules(DBUS dbus-1)
            if (NOT DBUS_FOUND)
                message(FATAL_ERROR "D-Bus not found. Please install it or set GTK_FILESELECTOR to ON.")
            endif()
            add_library(TracyDbus INTERFACE)
            target_include_directories(TracyDbus INTERFACE ${DBUS_INCLUDE_DIRS})
            target_link_libraries(TracyDbus INTERFACE ${DBUS_LINK_LIBRARIES})
            target_link_libraries(TracyNfd PUBLIC TracyDbus)
        endif()
    endif()
endif()

if(NOT EMSCRIPTEN)
    list(APPEND TRACY_LIBS TracyNfd)
endif()