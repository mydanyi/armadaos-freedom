cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -shared -static -Wl,--build-id=none -Wl,-Bsymbolic -Wl,--no-dynamic-linker -Wl,--no-undefined -Wl,--gc-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -nostdlib -mno-outline-atomics -g -fno-common -nostartfiles -mstrict-align -march=armv8-a -Os -std=gnu2x -fno-pic -fno-pie -fno-plt -ffunction-sections -fdata-sections -Wall")

# Distinguish windows and linux gcc path
if (WIN32)
    set(CMAKE_MAKE_PROGRAM "E:/JetBrains/CLion 2024.3.3/bin/mingw/bin/mingw32-make.exe")
    set(TARGET_TOOLCHAIN_PREFIX "E:/aarch64-gcc/bin/aarch64-none-elf-")
    set(HOST_C_COMPILER "E:/x86_64-mingw-gcc/bin/x86_64-w64-mingw32-gcc.exe")
    set(CMAKE_C_COMPILER "E:/aarch64-gcc/bin/aarch64-none-elf-gcc.exe")
    set(CMAKE_OBJCOPY "E:/aarch64-gcc/bin/aarch64-none-elf-objcopy.exe")
elseif(UNIX)
    set(HOST_C_COMPILER "/usr/bin/gcc")
#    set(TARGET_TOOLCHAIN_PREFIX "/usr/bin/aarch64-none-elf-")
#    set(CMAKE_C_COMPILER "/usr/bin/aarch64-none-elf-gcc")
#    set(CMAKE_OBJCOPY "/usr/bin/aarch64-none-elf-objcopy")
    set(TARGET_TOOLCHAIN_PREFIX "/usr/bin/aarch64-linux-gnu-")
    set(CMAKE_C_COMPILER "/usr/bin/aarch64-linux-gnu-gcc")
    set(CMAKE_OBJCOPY "/usr/bin/aarch64-linux-gnu-objcopy")
endif()

# If host system is linux and `tput colors` reports less than 256, switch to aquatic style.
set(menuconfig_style "\"path=bg:#51589e,fg:#E6DFF5 list=bg:#B5D7F8,fg:#505E73 separator=bg:#6684DC selection=fg:#1F1A20,bg:#A4C2F2,bold,underline frame=bg:#494f8f body=bg:#6169be help=bg:#51589e,fg:#E6DFF5\"")
if(UNIX)
    find_program(TPUT_EXEC tput)
    if(TPUT_EXEC)
        execute_process(
            COMMAND ${TPUT_EXEC} colors
            OUTPUT_VARIABLE TPUT_COLORS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(TPUT_COLORS LESS 256)
            message(WARNING "Terminal does not support truecolor.")
            message(WARNING "Please re-run cmake setup in a truecolor terminal.")
            set(menuconfig_style "aquatic")
        endif()
    endif()
endif()


# Output file name
set(OUTPUT_NAME kShimC)
set(KSHIMC_VERSION "0.0.1")
project(${OUTPUT_NAME} C ASM)

option(KSHIM_TB323FU_ANDROID_TOUCH_4M8_TRIAL "Trial only: limit original Android NVT controller to 4.8 MHz at handoff" OFF)
option(KSHIM_STANDALONE "Boot directly as an ARM64 Image instead of an attached manifest runtime" OFF)
option(KSHIM_DIAGNOSTIC_BASE_ONLY "Diagnostic only: hand off to the packed Linux base image before any DT/console/peripheral work; no menu and no peripheral probes; incompatible with KSHIM_STANDALONE" OFF)
option(KSHIM_DIAGNOSTIC_RESOURCES_ONLY "Diagnostic only: probe DT/console/resources, then hand off to the packed Linux base image after quiescing the console; no menu and no post-checkpoint peripheral probes; incompatible with KSHIM_STANDALONE and KSHIM_DIAGNOSTIC_BASE_ONLY" OFF)
option(KSHIM_RAMOOPS_RECOVERY_ONLY "Recovery only: read-only ramoops recovery entry after runtime DT validation; no console/resources/UI/Linux; incompatible with KSHIM_STANDALONE and the diagnostic handoff switches" OFF)
option(KSHIM_DIAGNOSTIC_DT_ONLY "Diagnostic only: pure DT parsing (validation, device info, layout-only ramoops capture, memory snapshot) then an immediate handoff to the packed Linux base image; no console, resources, peripherals, MMU or UI; incompatible with KSHIM_STANDALONE and the other diagnostic/recovery switches" OFF)
if(KSHIM_DIAGNOSTIC_BASE_ONLY AND KSHIM_STANDALONE)
    message(FATAL_ERROR "KSHIM_DIAGNOSTIC_BASE_ONLY is forbidden together with KSHIM_STANDALONE")
endif()
if(KSHIM_DIAGNOSTIC_RESOURCES_ONLY AND KSHIM_STANDALONE)
    message(FATAL_ERROR "KSHIM_DIAGNOSTIC_RESOURCES_ONLY is forbidden together with KSHIM_STANDALONE")
endif()
if(KSHIM_DIAGNOSTIC_RESOURCES_ONLY AND KSHIM_DIAGNOSTIC_BASE_ONLY)
    message(FATAL_ERROR "KSHIM_DIAGNOSTIC_RESOURCES_ONLY is forbidden together with KSHIM_DIAGNOSTIC_BASE_ONLY")
endif()
if(KSHIM_RAMOOPS_RECOVERY_ONLY AND KSHIM_STANDALONE)
    message(FATAL_ERROR "KSHIM_RAMOOPS_RECOVERY_ONLY is forbidden together with KSHIM_STANDALONE")
endif()
if(KSHIM_RAMOOPS_RECOVERY_ONLY AND KSHIM_DIAGNOSTIC_BASE_ONLY)
    message(FATAL_ERROR "KSHIM_RAMOOPS_RECOVERY_ONLY is forbidden together with KSHIM_DIAGNOSTIC_BASE_ONLY")
endif()
if(KSHIM_RAMOOPS_RECOVERY_ONLY AND KSHIM_DIAGNOSTIC_RESOURCES_ONLY)
    message(FATAL_ERROR "KSHIM_RAMOOPS_RECOVERY_ONLY is forbidden together with KSHIM_DIAGNOSTIC_RESOURCES_ONLY")
endif()
if(KSHIM_DIAGNOSTIC_DT_ONLY AND (KSHIM_STANDALONE OR KSHIM_DIAGNOSTIC_BASE_ONLY OR KSHIM_DIAGNOSTIC_RESOURCES_ONLY OR KSHIM_RAMOOPS_RECOVERY_ONLY))
    message(FATAL_ERROR "KSHIM_DIAGNOSTIC_DT_ONLY is forbidden together with KSHIM_STANDALONE, KSHIM_DIAGNOSTIC_BASE_ONLY, KSHIM_DIAGNOSTIC_RESOURCES_ONLY or KSHIM_RAMOOPS_RECOVERY_ONLY")
endif()
set(KSHIM_LINK_ADDRESS "0x40200000" CACHE STRING "Runtime link address")
set(KSHIM_RUNTIME_SLOT_SIZE "0x400000" CACHE STRING "Reserved runtime bytes (4 MiB for manifest protocol)")
set(KSHIM_UEFI_PAYLOAD "" CACHE FILEPATH "Optional raw UEFI FD for standalone test menu")
add_compile_definitions(KSHIM_STANDALONE=$<BOOL:${KSHIM_STANDALONE}>
    KSHIM_DIAGNOSTIC_BASE_ONLY=$<BOOL:${KSHIM_DIAGNOSTIC_BASE_ONLY}>
    KSHIM_DIAGNOSTIC_RESOURCES_ONLY=$<BOOL:${KSHIM_DIAGNOSTIC_RESOURCES_ONLY}>
    KSHIM_DIAGNOSTIC_DT_ONLY=$<BOOL:${KSHIM_DIAGNOSTIC_DT_ONLY}>
    KSHIM_RAMOOPS_RECOVERY_ONLY=$<BOOL:${KSHIM_RAMOOPS_RECOVERY_ONLY}>
    KSHIM_TB323FU_ANDROID_TOUCH_4M8_TRIAL=$<BOOL:${KSHIM_TB323FU_ANDROID_TOUCH_4M8_TRIAL}>
    KSHIM_LINK_ADDRESS=${KSHIM_LINK_ADDRESS} KSHIM_RUNTIME_SLOT_SIZE=${KSHIM_RUNTIME_SLOT_SIZE})

# Generate common variables pass to Kconfig
set(HOST_ARCH "${CMAKE_HOST_SYSTEM_PROCESSOR}")
string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S" UTC)
get_filename_component(COMPILER_BASENAME "${CMAKE_C_COMPILER}" NAME)
set(HOST_INFO "$ENV{USER}$ENV{USERNAME}@$ENV{HOSTNAME}$ENV{COMPUTERNAME} ${COMPILER_BASENAME} (${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION})")
set(KCONFIG_ENV "srctree=${CMAKE_SOURCE_DIR}/src" "HOST_ARCH=${HOST_ARCH}" "HOST_INFO=${HOST_INFO}"
                "BUILD_TIME=${BUILD_TIME}" "ARCH=${CMAKE_SYSTEM_PROCESSOR}" "VERSION=${KSHIMC_VERSION}"
)

find_program(MENUCONFIG_EXECUTABLE menuconfig)
find_program(GUICONFIG_EXECUTABLE guiconfig)
find_program(GENCONFIG_EXECUTABLE genconfig)

if(NOT GENCONFIG_EXECUTABLE)
    message(FATAL_ERROR "genconfig (Python kconfiglib) is required")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_BINARY_DIR}/.config" "${CMAKE_SOURCE_DIR}/Kconfig"
    "${CMAKE_SOURCE_DIR}/src/Kconfig" "${CMAKE_SOURCE_DIR}/src/drivers/Kconfig"
    "${CMAKE_SOURCE_DIR}/tools/configure_kconfig.py")
execute_process(COMMAND ${CMAKE_COMMAND} -E env ${KCONFIG_ENV}
    ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/configure_kconfig.py
    --root ${CMAKE_SOURCE_DIR} --config ${CMAKE_BINARY_DIR}/.config
    --header ${CMAKE_BINARY_DIR}/generated/include/config.h
    --cmake ${CMAKE_BINARY_DIR}/generated/config.cmake
    RESULT_VARIABLE CONFIG_STATUS)
if(NOT CONFIG_STATUS EQUAL 0)
    message(FATAL_ERROR "Kconfig evaluation failed")
endif()
include(${CMAKE_BINARY_DIR}/generated/config.cmake)
add_custom_target(generate_config)

# Build Picolibc for the existing kShimC runtime and startup environment.
# Before MMU is enabled, libc runs in a fixed-alignment environment, so alignment-safe code is required here.
# -mstrict-align does not constrain handwritten asm, which can still emit unaligned state on paths before the handoff.
# __PREFER_SIZE_OVER_SPEED selects the generic byte path instead of the alignment-assuming optimized path.
set(__PREFER_SIZE_OVER_SPEED ON CACHE BOOL "" FORCE)
set(__THREAD_LOCAL_STORAGE OFF CACHE BOOL "" FORCE)
set(__THREAD_LOCAL_STORAGE_API OFF CACHE BOOL "" FORCE)
set(__GLOBAL_ERRNO ON CACHE BOOL "" FORCE)
set(__SINGLE_THREAD ON CACHE BOOL "" FORCE)
set(ENABLE_MALLOC ON CACHE BOOL "" FORCE)
set(POSIX_CONSOLE OFF CACHE BOOL "" FORCE)
set(__IO_FLOAT OFF CACHE BOOL "" FORCE)
set(__IO_DEFAULT l CACHE STRING "" FORCE)
set(__IO_LONG_DOUBLE OFF CACHE BOOL "" FORCE)
set(__IO_POS_ARGS OFF CACHE BOOL "" FORCE)
set(__MB_CAPABLE OFF CACHE BOOL "" FORCE)
set(TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/picolibc ${CMAKE_BINARY_DIR}/lib/picolibc)

# Add compile flags for a freestanding environment

# Add include directories
set(INCLUDE_DIRS
    ${CMAKE_BINARY_DIR}/generated/
    ${CMAKE_BINARY_DIR}/generated/include/
    ${CMAKE_SOURCE_DIR}/src/include/
    ${CMAKE_SOURCE_DIR}/lib/libfdt/include/
)

include_directories(${INCLUDE_DIRS})

# Define the executable target, including assembly and C source files
set(SRC_FILES
    src/dt/tb323fu_android_touch.c
    src/dt/memory_overlay.c
    src/dt/simplefb.c
    lib/libfdt/fdt_overlay.c

    src/core/main.c src/core/platform.c src/core/uefi_boot.c src/core/picoport.c src/core/boot_metrics.c
    src/core/menu_locale.c src/core/boot_settings.c
    src/dt/dt.c src/dt/device_info.c src/dt/initrd.c src/dt/handoff.c src/dt/handoff_reservations.c src/dt/console.c src/dt/resources.c src/dt/probe.c
    src/dt/tb323fu_ramoops.c src/dt/tb323fu_ramoops_export.c
    src/drivers/console.c src/drivers/pl011.c src/drivers/keys.c src/drivers/qcom_reboot.c
    src/arch/arm64/exception.c src/arch/arm64/relocation.c src/arch/arm64/mmu.c
    src/arch/arm64/psci.c src/arch/arm64/timer.c src/arch/arm64/gicv3.c
    src/arch/arm64/startup.S src/arch/arm64/arm64.S
    src/arch/arm64/psci.S src/arch/arm64/timer.S src/arch/arm64/smc.S src/arch/arm64/gicv3.S
    lib/libfdt/fdt.c lib/libfdt/fdt_check.c lib/libfdt/fdt_ro.c lib/libfdt/fdt_addresses.c
    lib/libfdt/fdt_rw.c lib/libfdt/fdt_sw.c lib/libfdt/fdt_wip.c lib/libfdt/fdt_empty_tree.c lib/libfdt/fdt_strerror.c)

if(CONFIG_KSHIM_STORAGE_ENTRY_PROBE OR CONFIG_KSHIM_UFS_NATIVE_IO)
    list(APPEND SRC_FILES src/drivers/storage_probe.c src/drivers/storage_ufs_dma.c
        src/dt/storage_probe_export.c
        src/arch/arm64/storage_probe_io.c src/arch/arm64/storage_probe_load.S)
    if(CONFIG_KSHIM_UFS_NATIVE_IO)
        list(APPEND SRC_FILES src/arch/arm64/storage_ufs_native_io.c
            src/arch/arm64/storage_ufs_store.S)
        if(CONFIG_KSHIM_UFS_SETTINGS)
            list(APPEND SRC_FILES src/arch/arm64/storage_settings_native.c
                src/drivers/storage_ufs_settings.c src/dt/storage_settings_export.c)
        endif()
        if(CONFIG_KSHIM_UFS_READ_PROBE)
            list(APPEND SRC_FILES src/arch/arm64/storage_ufs_read_native.c
                src/drivers/storage_ufs_read_probe.c
                src/dt/storage_ufs_read_export.c)
            if(CONFIG_KSHIM_UFS_READ_SCREEN)
                list(APPEND SRC_FILES src/ui/storage_ufs_read_screen.c
                    src/arch/arm64/storage_ufs_read_screen.c)
            endif()
        endif()
    endif()
endif()
# Storage formats are shared by the later hardware backends.
list(APPEND SRC_FILES src/core/storage_block.c src/core/storage_ab.c src/core/storage_config.c
    src/core/storage_settings.c src/drivers/storage_ufs.c src/drivers/storage_sdhci.c)

if(CONFIG_KSHIM_SMP)
    list(APPEND SRC_FILES src/arch/arm64/smp.c
        src/arch/arm64/smp_selftest.c src/arch/arm64/mpstartup.S)
endif()
if(CONFIG_QCOM_KEYS OR CONFIG_QCOM_SPMI)
    list(APPEND SRC_FILES src/dt/spmi.c)
endif()
if(CONFIG_QCOM_KEYS OR CONFIG_KSHIM_TOUCH)
    list(APPEND SRC_FILES src/dt/gpio.c)
endif()
if(CONFIG_QCOM_KEYS)
    list(APPEND SRC_FILES src/dt/keys.c src/drivers/qcom_keys.c)
endif()
if(CONFIG_KSHIM_TOUCH)
    list(APPEND SRC_FILES src/dt/touch.c src/drivers/st_ftm5.c)
endif()
if(CONFIG_KSHIM_FRAMEBUFFER)
    list(APPEND SRC_FILES src/dt/splash.c)
endif()
if(CONFIG_KSHIM_FRAMEBUFFER OR CONFIG_KSHIM_LVGL)
    list(APPEND SRC_FILES src/ui/framebuffer.c)
endif()
if(CONFIG_KSHIM_TOUCH OR CONFIG_KSHIM_LVGL)
    list(APPEND SRC_FILES src/drivers/touch.c)
endif()
if(CONFIG_KSHIM_LVGL)
    list(APPEND SRC_FILES src/ui/backdrop.c src/ui/font.c src/ui/lvgl_port.c src/ui/lvgl_os.c)
    if(CONFIG_KSHIM_MENU_STYLE_STAR_CHART)
        set(KSHIM_UI_FONT_BLOB "${CMAKE_SOURCE_DIR}/src/ui/fonts/lxgw_wenkai_menu.ksf")
    else()
        set(KSHIM_UI_FONT_BLOB "${CMAKE_SOURCE_DIR}/src/ui/fonts/noto_sans_cjk_sc.ksf")
    endif()
    configure_file(src/ui/fonts/ui_font_blob.S.in ${CMAKE_BINARY_DIR}/ui_font_blob.S @ONLY)
    set_source_files_properties(${CMAKE_BINARY_DIR}/ui_font_blob.S PROPERTIES
        OBJECT_DEPENDS "${KSHIM_UI_FONT_BLOB}")
    list(APPEND SRC_FILES ${CMAKE_BINARY_DIR}/ui_font_blob.S)
endif()
set(CRDK_SPMI ${CONFIG_QCOM_SPMI} CACHE BOOL "" FORCE)
set(CRDK_GPIO ${CONFIG_QCOM_KEYS} CACHE BOOL "" FORCE)
set(CRDK_GENI ${CONFIG_KSHIM_TOUCH} CACHE BOOL "" FORCE)
set(CRDK_DISPLAY ${CONFIG_KSHIM_FRAMEBUFFER} CACHE BOOL "" FORCE)
# Console type is selected from DT, so UART support remains a capability in every profile.
set(CRDK_UART ON CACHE BOOL "" FORCE)

if(CONFIG_KSHIM_LVGL)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_PATH "${CMAKE_SOURCE_DIR}/src/include/lv_conf.h" CACHE FILEPATH "" FORCE)
add_subdirectory(third_party/lvgl EXCLUDE_FROM_ALL)
include(${CMAKE_SOURCE_DIR}/ports/lvgl/CMakeLists.txt)
kshim_enable_lvgl_smp_adapter(lvgl "${CMAKE_SOURCE_DIR}/third_party/lvgl")
target_link_libraries(lvgl PRIVATE c)

set(KSHIM_UI_LIBRARIES lvgl)
endif()
add_subdirectory(ports/crdk)
target_link_libraries(crdk PRIVATE c)

# Specify the linker script
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/src/arch/arm64/aarch64.lds")
set(PROCESSED_LINKER_SCRIPT "${CMAKE_BINARY_DIR}/aarch64.lds")
file(GLOB LINKER_HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/include/ld/*")
add_custom_command(
    OUTPUT ${PROCESSED_LINKER_SCRIPT}
    COMMAND ${CMAKE_C_COMPILER} -E -P -x c ${LINKER_SCRIPT} -o ${PROCESSED_LINKER_SCRIPT} -I${CMAKE_SOURCE_DIR}/src/include/ -D__LD_SCRIPT__ -DKSHIM_LINK_ADDRESS=${KSHIM_LINK_ADDRESS} -DKSHIM_RUNTIME_SLOT_SIZE=${KSHIM_RUNTIME_SLOT_SIZE}
    DEPENDS ${LINKER_SCRIPT} ${LINKER_HEADERS}
    COMMENT "Preprocessing linker script"
)

add_custom_target(preprocess_linker_script ALL DEPENDS ${PROCESSED_LINKER_SCRIPT})
add_executable(${OUTPUT_NAME}.elf ${SRC_FILES})
# Expose the build version to C as a quoted string literal (e.g. "0.0.1"), not
# a bare preprocessing token. VERSION is only an environment variable fed into
# Kconfig, so it never reaches the C compiler as a definition on its own.
target_compile_definitions(${OUTPUT_NAME}.elf PRIVATE KSHIM_BUILD_VERSION="${KSHIMC_VERSION}")
if(KSHIM_UEFI_PAYLOAD)
    get_filename_component(KSHIM_UEFI_PAYLOAD "${KSHIM_UEFI_PAYLOAD}" ABSOLUTE)
    configure_file(src/arch/arm64/uefi_payload.S.in ${CMAKE_BINARY_DIR}/uefi_payload.S @ONLY)
    set_source_files_properties(${CMAKE_BINARY_DIR}/uefi_payload.S PROPERTIES OBJECT_DEPENDS "${KSHIM_UEFI_PAYLOAD}")
    target_sources(${OUTPUT_NAME}.elf PRIVATE ${CMAKE_BINARY_DIR}/uefi_payload.S)
    target_compile_definitions(${OUTPUT_NAME}.elf PRIVATE KSHIM_HAS_UEFI_PAYLOAD=1)
endif()
add_dependencies(${OUTPUT_NAME}.elf preprocess_linker_script)
target_link_libraries(${OUTPUT_NAME}.elf PRIVATE
    "-Wl,--start-group"
    ${KSHIM_UI_LIBRARIES}
    crdk
    c
    gcc
    "-Wl,--end-group"
)
set_target_properties(${OUTPUT_NAME}.elf PROPERTIES LINK_FLAGS "-T${PROCESSED_LINKER_SCRIPT}")

# After building the ELF file, generate a binary file (main.bin) using objcopy
add_custom_command(TARGET ${OUTPUT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${OUTPUT_NAME}.elf ${OUTPUT_NAME}.bin
    COMMENT "Generating executable binary"
)

if(MENUCONFIG_EXECUTABLE AND GENCONFIG_EXECUTABLE)
    add_custom_target(menuconfig
        COMMAND ${CMAKE_COMMAND} -E env "MENUCONFIG_STYLE=${menuconfig_style}" ${KCONFIG_ENV} ${MENUCONFIG_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Kconfig"
        COMMAND ${CMAKE_COMMAND} -E env ${KCONFIG_ENV} ${GENCONFIG_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Kconfig" "--header-path" "${CMAKE_BINARY_DIR}/generated/include/config.h"
        COMMENT "Starting Menu Config"
    )
endif()

if(GUICONFIG_EXECUTABLE AND GENCONFIG_EXECUTABLE)
    add_custom_target(guiconfig
        COMMAND ${CMAKE_COMMAND} -E env ${KCONFIG_ENV} ${GUICONFIG_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Kconfig"
        COMMAND ${CMAKE_COMMAND} -E env ${KCONFIG_ENV} ${GENCONFIG_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Kconfig" "--header-path" "${CMAKE_BINARY_DIR}/generated/include/config.h"
        COMMENT "Starting GUI Config"
    )
endif()
