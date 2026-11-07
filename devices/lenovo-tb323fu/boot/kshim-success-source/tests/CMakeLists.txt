cmake_minimum_required(VERSION 3.16)
project(kshim_host_tests C ASM)
enable_testing()

get_filename_component(KSHIM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(KSHIM_TEST_MENU_STYLE "FLAT" CACHE STRING "Boot menu preset used by host UI tests")
set(KSHIM_TEST_MENU_STYLES FLAT CLASSIC CARDS TERMINAL MINIMAL HIGH_CONTRAST
    FLUENT MATERIAL3 SURFACE CUPERTINO GLASS AURORA SOFT_UI BENTO NEON NORD
    MATERIAL2 MATERIAL2_DARK FLUENT2 FLUENT2_DARK FLUENT_DARK SURFACE_DARK HARMONYOS CLOVER IOS_HIG
    METRO ONE_UI ADWAITA HOLO LVGL_DEFAULT STAR_CHART)
set_property(CACHE KSHIM_TEST_MENU_STYLE PROPERTY STRINGS ${KSHIM_TEST_MENU_STYLES})
if(NOT KSHIM_TEST_MENU_STYLE IN_LIST KSHIM_TEST_MENU_STYLES)
    message(FATAL_ERROR "Unknown KSHIM_TEST_MENU_STYLE: ${KSHIM_TEST_MENU_STYLE}")
endif()
option(KSHIM_TEST_SANITIZERS "Use address and undefined behavior sanitizers" ON)
option(KSHIM_TEST_STANDALONE "Use standalone LVGL heap for host tests" ON)
add_compile_definitions(KSHIM_HOST_TEST=1 CONFIG_QCOM_SPMI=1
    KSHIM_STANDALONE=$<BOOL:${KSHIM_TEST_STANDALONE}>
    CONFIG_KSHIM_LVGL_BUFFER_BYTES=243712)
if(KSHIM_TEST_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
add_compile_options(-g -O1 -ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections)

if(KSHIM_TEST_MENU_STYLE STREQUAL "LVGL_DEFAULT")
    add_compile_definitions(CONFIG_KSHIM_MENU_STYLE_LVGL_DEFAULT=1)
endif()

set(LV_CONF_PATH "${KSHIM_ROOT}/src/include/lv_conf.h" CACHE FILEPATH "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
add_subdirectory("${KSHIM_ROOT}/third_party/lvgl" lvgl)
add_subdirectory("${KSHIM_ROOT}/ports/crdk" crdk)
if(KSHIM_TEST_MENU_STYLE STREQUAL "STAR_CHART")
    set(KSHIM_UI_FONT_BLOB "${KSHIM_ROOT}/src/ui/fonts/lxgw_wenkai_menu.ksf")
else()
    set(KSHIM_UI_FONT_BLOB "${KSHIM_ROOT}/src/ui/fonts/noto_sans_cjk_sc.ksf")
endif()
configure_file("${KSHIM_ROOT}/src/ui/fonts/ui_font_blob.S.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ui_font_blob.S" @ONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/ui_font_blob.S"
    PROPERTIES OBJECT_DEPENDS "${KSHIM_UI_FONT_BLOB}")
add_library(kshim_backdrop STATIC "${KSHIM_ROOT}/src/ui/backdrop.c"
    "${KSHIM_ROOT}/src/ui/font.c" "${CMAKE_CURRENT_BINARY_DIR}/ui_font_blob.S")
target_include_directories(kshim_backdrop PUBLIC "${KSHIM_ROOT}/src/include")
target_link_libraries(kshim_backdrop PUBLIC lvgl)

# 创建带断言和严格告警的主机测试目标。
function(kshim_test Target)
    add_executable(${Target} ${ARGN})
    target_compile_options(${Target} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    target_compile_definitions(${Target} PRIVATE _POSIX_C_SOURCE=200809L)
    target_include_directories(${Target} PRIVATE "${KSHIM_ROOT}/src/include")
    add_test(NAME ${Target} COMMAND ${Target})
endfunction()

kshim_test(qualcomm_test qualcomm_test.c "${KSHIM_ROOT}/src/drivers/keys.c" "${KSHIM_ROOT}/src/dt/keys.c")
target_include_directories(qualcomm_test PRIVATE "${KSHIM_ROOT}/lib/libfdt/include")
kshim_test(spmi_test spmi_test.c "${KSHIM_ROOT}/src/drivers/qcom_keys.c")
target_link_libraries(spmi_test PRIVATE crdk_spmi)
kshim_test(framebuffer_test framebuffer_test.c "${KSHIM_ROOT}/src/ui/framebuffer.c")

set(KSHIM_TEST_UI_SOURCES "${KSHIM_ROOT}/src/ui/lvgl_port.c"
    "${KSHIM_ROOT}/src/ui/framebuffer.c" "${KSHIM_ROOT}/src/drivers/keys.c"
    "${KSHIM_ROOT}/src/core/boot_metrics.c" "${KSHIM_ROOT}/src/core/menu_locale.c")
kshim_test(lvgl_test lvgl_test.c ${KSHIM_TEST_UI_SOURCES} "${KSHIM_ROOT}/src/drivers/touch.c")
kshim_test(lvgl_hdk_test lvgl_test.c ${KSHIM_TEST_UI_SOURCES} "${KSHIM_ROOT}/src/drivers/touch.c")
kshim_test(menu_style_render_test menu_style_render_test.c ${KSHIM_TEST_UI_SOURCES})
kshim_test(menu_modern_test menu_modern_test.c ${KSHIM_TEST_UI_SOURCES})
add_executable(ui_preview_test ui_preview_test.c ${KSHIM_TEST_UI_SOURCES})
target_include_directories(ui_preview_test PRIVATE "${KSHIM_ROOT}/src/include")
foreach(target lvgl_test lvgl_hdk_test menu_style_render_test menu_modern_test ui_preview_test)
    target_link_libraries(${target} PRIVATE lvgl kshim_backdrop m)
    target_compile_definitions(${target} PRIVATE CONFIG_KSHIM_MENU_STYLE_${KSHIM_TEST_MENU_STYLE}=1)
endforeach()
foreach(target lvgl_hdk_test ui_preview_test)
    target_compile_definitions(${target} PRIVATE CONFIG_KSHIM_UEFI_TEST_MENU=1
        "CONFIG_KSHIM_MENU_ENTRY1=\"UEFI A\"" "CONFIG_KSHIM_MENU_ENTRY2=\"UEFI B\"")
endforeach()

kshim_test(qcom_bus_test qcom_bus_test.c)
kshim_test(qcom_touch_platform_test qcom_touch_platform_test.c)
kshim_test(hdk8250_touch_platform_test hdk8250_touch_platform_test.c)
kshim_test(touch_test touch_test.c "${KSHIM_ROOT}/src/drivers/touch.c")
kshim_test(st_ftm5_test st_ftm5_test.c "${KSHIM_ROOT}/src/drivers/st_ftm5.c" "${KSHIM_ROOT}/src/drivers/touch.c")

add_library(kshim_dt STATIC
    "${KSHIM_ROOT}/src/dt/memory_overlay.c"
    "${KSHIM_ROOT}/lib/libfdt/fdt_overlay.c"

    "${KSHIM_ROOT}/src/dt/probe.c"
    "${KSHIM_ROOT}/src/dt/dt.c" "${KSHIM_ROOT}/src/dt/device_info.c" "${KSHIM_ROOT}/src/dt/initrd.c" "${KSHIM_ROOT}/src/dt/handoff.c" "${KSHIM_ROOT}/src/dt/handoff_reservations.c" "${KSHIM_ROOT}/src/dt/spmi.c"
    "${KSHIM_ROOT}/src/dt/gpio.c" "${KSHIM_ROOT}/src/dt/touch.c" "${KSHIM_ROOT}/src/dt/console.c"
    "${KSHIM_ROOT}/src/dt/splash.c"
    "${KSHIM_ROOT}/lib/libfdt/fdt.c" "${KSHIM_ROOT}/lib/libfdt/fdt_check.c"
    "${KSHIM_ROOT}/lib/libfdt/fdt_ro.c" "${KSHIM_ROOT}/lib/libfdt/fdt_rw.c"
    "${KSHIM_ROOT}/lib/libfdt/fdt_sw.c" "${KSHIM_ROOT}/lib/libfdt/fdt_wip.c"
    "${KSHIM_ROOT}/lib/libfdt/fdt_empty_tree.c" "${KSHIM_ROOT}/lib/libfdt/fdt_addresses.c")
target_include_directories(kshim_dt PUBLIC "${KSHIM_ROOT}/src/include" "${KSHIM_ROOT}/lib/libfdt/include")
target_link_libraries(kshim_dt PUBLIC crdk)
foreach(target qualcomm_test qcom_bus_test qcom_touch_platform_test hdk8250_touch_platform_test st_ftm5_test)
    target_link_libraries(${target} PRIVATE kshim_dt)
endforeach()
add_link_options(-Wl,--gc-sections)
kshim_test(dt_test dt_test.c "${KSHIM_ROOT}/src/dt/keys.c" "${KSHIM_ROOT}/src/drivers/keys.c")
target_compile_definitions(dt_test PRIVATE "FIXTURES=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures\"")
target_link_libraries(dt_test PRIVATE kshim_dt)
kshim_test(crdk_poll_test crdk_poll_test.c)
target_link_libraries(crdk_poll_test PRIVATE crdk)
kshim_test(dt_handoff_test dt_handoff_test.c)
target_link_libraries(dt_handoff_test PRIVATE kshim_dt)
kshim_test(initrd_test initrd_test.c)
target_link_libraries(initrd_test PRIVATE kshim_dt)
kshim_test(dt_handoff_reservations_test dt_handoff_reservations_test.c)
target_link_libraries(dt_handoff_reservations_test PRIVATE kshim_dt)

find_program(DTC_EXECUTABLE dtc REQUIRED)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fixtures/kona-resources.dtb
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/fixtures
    COMMAND ${DTC_EXECUTABLE} -q -I dts -O dtb -o ${CMAKE_CURRENT_BINARY_DIR}/fixtures/kona-resources.dtb
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/kona-resources.dts
    DEPENDS fixtures/kona-resources.dts)
add_custom_target(dt_fixtures DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fixtures/kona-resources.dtb)
add_dependencies(dt_test dt_fixtures)
target_compile_definitions(dt_test PRIVATE "GENERATED_FIXTURES=\"${CMAKE_CURRENT_BINARY_DIR}/fixtures\"")
kshim_test(mmu_test mmu_test.c "${KSHIM_ROOT}/src/arch/arm64/mmu.c")
target_include_directories(mmu_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(mmu_test PRIVATE kshim_dt)

set(KSHIM_TEST_MENU_SCALE_Q8 "0" CACHE STRING "Theme design scale; 0=automatic")
option(KSHIM_TEST_MENU_REDUCED_EFFECTS "Reduce theme drawing effects" OFF)
foreach(target lvgl_test lvgl_hdk_test menu_style_render_test menu_modern_test ui_preview_test)
    target_compile_definitions(${target} PRIVATE
        CONFIG_KSHIM_MENU_SCALE_Q8=${KSHIM_TEST_MENU_SCALE_Q8}
        CONFIG_KSHIM_MENU_REDUCED_EFFECTS=$<BOOL:${KSHIM_TEST_MENU_REDUCED_EFFECTS}>)
endforeach()
kshim_test(menu_design_contract_test menu_design_contract_test.c)
target_compile_definitions(menu_design_contract_test PRIVATE
    CONFIG_KSHIM_MENU_STYLE_${KSHIM_TEST_MENU_STYLE}=1
    "EXPECTED_STYLE=\"${KSHIM_TEST_MENU_STYLE}\"")

if(KSHIM_TEST_MENU_STYLE MATCHES "^(METRO|ONE_UI|ADWAITA|HOLO)$")
    kshim_test(menu_new_themes_lvgl_test menu_new_themes_lvgl_test.c ${KSHIM_TEST_UI_SOURCES})
    target_link_libraries(menu_new_themes_lvgl_test PRIVATE lvgl kshim_backdrop m)
    target_compile_definitions(menu_new_themes_lvgl_test PRIVATE
        CONFIG_KSHIM_MENU_STYLE_${KSHIM_TEST_MENU_STYLE}=1
        CONFIG_KSHIM_MENU_SCALE_Q8=${KSHIM_TEST_MENU_SCALE_Q8}
        CONFIG_KSHIM_MENU_REDUCED_EFFECTS=$<BOOL:${KSHIM_TEST_MENU_REDUCED_EFFECTS}>)
endif()

if(KSHIM_TEST_MENU_STYLE STREQUAL "ONE_UI")
    kshim_test(menu_one_ui_identity_test menu_one_ui_identity_test.c)
    target_compile_definitions(menu_one_ui_identity_test PRIVATE CONFIG_KSHIM_MENU_STYLE_ONE_UI=1)
endif()
if(KSHIM_TEST_MENU_STYLE STREQUAL "LVGL_DEFAULT")
    kshim_test(menu_native_theme_test menu_native_theme_test.c ${KSHIM_TEST_UI_SOURCES})
    target_link_libraries(menu_native_theme_test PRIVATE lvgl kshim_backdrop m)
    target_compile_definitions(menu_native_theme_test PRIVATE
        CONFIG_KSHIM_MENU_SCALE_Q8=${KSHIM_TEST_MENU_SCALE_Q8}
        CONFIG_KSHIM_MENU_REDUCED_EFFECTS=$<BOOL:${KSHIM_TEST_MENU_REDUCED_EFFECTS}>)
endif()
# TB323FU key resource regression fixture.
set(TB323FU_KEYS_DTS "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tb323fu-keys.dts")
set(TB323FU_KEYS_DTB "${CMAKE_CURRENT_BINARY_DIR}/fixtures/tb323fu-keys.dtb")

add_custom_command(
    OUTPUT "${TB323FU_KEYS_DTB}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/fixtures"
    COMMAND "${DTC_EXECUTABLE}" -I dts -O dtb -o "${TB323FU_KEYS_DTB}" "${TB323FU_KEYS_DTS}"
    DEPENDS "${TB323FU_KEYS_DTS}"
    VERBATIM)

add_custom_target(tb323fu_fixture DEPENDS "${TB323FU_KEYS_DTB}")

kshim_test(tb323fu_keys_test
    tb323fu_keys_test.c
    ${KSHIM_ROOT}/src/dt/keys.c
    ${KSHIM_ROOT}/src/drivers/keys.c)

target_link_libraries(tb323fu_keys_test PRIVATE kshim_dt)
add_dependencies(tb323fu_keys_test tb323fu_fixture)
target_compile_definitions(tb323fu_keys_test PRIVATE FIXTURE_PATH="${TB323FU_KEYS_DTB}")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fixtures)

set(TB323FU_SPLASH_DTB
    ${CMAKE_CURRENT_BINARY_DIR}/fixtures/tb323fu-splash.dtb)

add_custom_command(
    OUTPUT ${TB323FU_SPLASH_DTB}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/fixtures
    COMMAND ${DTC_EXECUTABLE} -I dts -O dtb -o ${TB323FU_SPLASH_DTB}
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tb323fu-splash.dts
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tb323fu-splash.dts
    VERBATIM)

add_custom_target(tb323fu_splash_fixture DEPENDS ${TB323FU_SPLASH_DTB})

kshim_test(tb323fu_splash_test tb323fu_splash_test.c)
target_link_libraries(tb323fu_splash_test PRIVATE kshim_dt)
add_dependencies(tb323fu_splash_test tb323fu_splash_fixture)
target_compile_definitions(tb323fu_splash_test PRIVATE
    FIXTURE_PATH="${TB323FU_SPLASH_DTB}")

# TB322FC / Sun (DPU12) splash detection; builds its DT in-process with libfdt.
kshim_test(tb322fc_splash_test tb322fc_splash_test.c)
target_link_libraries(tb322fc_splash_test PRIVATE kshim_dt)

# TB710FU / SM8650 splash detection (measured register values); builds its
# DT in-process with libfdt.
kshim_test(tb710fu_splash_test tb710fu_splash_test.c)
target_link_libraries(tb710fu_splash_test PRIVATE kshim_dt)

# Parent index cache unit test: wrap fdt_parent_offset to observe cache hits.
kshim_test(dt_parent_index_test parent_index_test.c)
target_link_libraries(dt_parent_index_test PRIVATE kshim_dt)
target_link_options(dt_parent_index_test PRIVATE -Wl,--wrap=fdt_parent_offset)

# Resource scope lifecycle regression against real src/dt/resources.c.
kshim_test(dt_resource_scope_test resource_scope_test.c "${KSHIM_ROOT}/src/dt/resources.c")
target_include_directories(dt_resource_scope_test BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/scope-include")
target_link_libraries(dt_resource_scope_test PRIVATE kshim_dt)
target_link_options(dt_resource_scope_test PRIVATE -Wl,--wrap=fdt_parent_offset)

kshim_test(dt_compatible_index_test compatible_index_test.c)
target_link_libraries(dt_compatible_index_test PRIVATE kshim_dt)
target_link_options(dt_compatible_index_test PRIVATE -Wl,--wrap=fdt_node_offset_by_compatible)

kshim_test(dt_phandle_scan_test phandle_scan_test.c)
target_link_libraries(dt_phandle_scan_test PRIVATE kshim_dt)
target_link_options(dt_phandle_scan_test PRIVATE -Wl,--wrap=fdt_getprop)

kshim_test(dt_probe_report_test probe_report_test.c)
target_link_libraries(dt_probe_report_test PRIVATE kshim_dt)

kshim_test(dt_diagnostic_report_test dt_diagnostic_report_test.c)
target_link_libraries(dt_diagnostic_report_test PRIVATE kshim_dt)

target_compile_definitions(kshim_dt PRIVATE KSHIM_DIAGNOSTIC_RESOURCES_ONLY=1)

kshim_test(memory_overlay_test memory_overlay_test.c
    "${KSHIM_ROOT}/src/dt/simplefb.c")
target_link_libraries(memory_overlay_test PRIVATE kshim_dt)

kshim_test(device_info_test device_info_test.c)
target_link_libraries(device_info_test PRIVATE kshim_dt)

kshim_test(splash_trace_report_test splash_trace_report_test.c)
target_link_libraries(splash_trace_report_test PRIVATE kshim_dt)

kshim_test(probe_snapshot_slots_test probe_snapshot_slots_test.c)
target_link_libraries(probe_snapshot_slots_test PRIVATE kshim_dt)

kshim_test(splash_snapshot_test splash_snapshot_test.c)
target_link_libraries(splash_snapshot_test PRIVATE kshim_dt)

kshim_test(sspp_v13_test sspp_v13_test.c)
target_link_libraries(sspp_v13_test PRIVATE crdk)

kshim_test(reserved_placeholder_test reserved_placeholder_test.c "${KSHIM_ROOT}/src/arch/arm64/mmu.c")
target_include_directories(reserved_placeholder_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(reserved_placeholder_test PRIVATE kshim_dt)

kshim_test(psci_power_test psci_power_test.c "${KSHIM_ROOT}/src/arch/arm64/psci.c")
target_link_libraries(psci_power_test PRIVATE kshim_dt)

# Compile the real platform loop with scripted peripherals in both profiles.
# platform_menu_test and platform_menu_no_power_test keep the debug console ON
# (explicit here, matching the harness default); platform_menu_console_off_test
# forces it OFF to exercise the TB322FC auto-return gate against the same source.
kshim_test(platform_menu_test platform_menu_test.c)
kshim_test(platform_menu_no_power_test platform_menu_test.c)
kshim_test(platform_menu_console_off_test platform_menu_test.c)
target_compile_definitions(platform_menu_no_power_test PRIVATE CONFIG_KSHIM_MENU_POWER_ACTIONS=0)
target_compile_definitions(platform_menu_test PRIVATE CONFIG_KSHIM_DEBUG_CONSOLE=1)
target_compile_definitions(platform_menu_no_power_test PRIVATE CONFIG_KSHIM_DEBUG_CONSOLE=1)
target_compile_definitions(platform_menu_console_off_test PRIVATE CONFIG_KSHIM_DEBUG_CONSOLE=0)
foreach(target platform_menu_test platform_menu_no_power_test platform_menu_console_off_test)
    target_include_directories(${target} BEFORE PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include" "${KSHIM_ROOT}/lib/libfdt/include")
    target_link_libraries(${target} PRIVATE crdk)
    target_sources(${target} PRIVATE "${KSHIM_ROOT}/src/core/menu_locale.c"
        "${KSHIM_ROOT}/src/core/boot_settings.c")
endforeach()

# Pure boot-settings decision module: no stub, no storage/UFS backend.
kshim_test(boot_settings_test boot_settings_test.c
    "${KSHIM_ROOT}/src/core/boot_settings.c")

# Session-settings entry-point profiles: the same real platform loop, driven
# through kshim_platform_select_localized_settings. The console-off variant
# additionally exercises the new entry point against the TB322FC auto-return
# gate with the debug console forced off.
kshim_test(platform_settings_menu_test platform_menu_test.c)
target_compile_definitions(platform_settings_menu_test PRIVATE
    CONFIG_KSHIM_UFS_SETTINGS=1
    CONFIG_KSHIM_MENU_SESSION_SETTINGS=1
    CONFIG_KSHIM_MENU_LANGUAGE_SWITCH=1
    CONFIG_KSHIM_MENU_ADVANCED_ACTIONS=1
    CONFIG_KSHIM_DEBUG_CONSOLE=1
    CONFIG_KSHIM_MENU_POWER_ACTIONS=1
    CONFIG_KSHIM_MENU_CHINESE=1
    CONFIG_KSHIM_QCOM_REBOOT=1)
target_include_directories(platform_settings_menu_test BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${KSHIM_ROOT}/lib/libfdt/include")
target_link_libraries(platform_settings_menu_test PRIVATE crdk)
target_sources(platform_settings_menu_test PRIVATE
    "${KSHIM_ROOT}/src/core/menu_locale.c" "${KSHIM_ROOT}/src/core/boot_settings.c")

kshim_test(platform_settings_console_off_test platform_menu_test.c)
target_compile_definitions(platform_settings_console_off_test PRIVATE
    CONFIG_KSHIM_MENU_SESSION_SETTINGS=1
    CONFIG_KSHIM_MENU_LANGUAGE_SWITCH=1
    CONFIG_KSHIM_MENU_ADVANCED_ACTIONS=1
    CONFIG_KSHIM_DEBUG_CONSOLE=0
    CONFIG_KSHIM_MENU_POWER_ACTIONS=1
    CONFIG_KSHIM_MENU_CHINESE=1
    CONFIG_KSHIM_QCOM_REBOOT=1)
target_include_directories(platform_settings_console_off_test BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${KSHIM_ROOT}/lib/libfdt/include")
target_link_libraries(platform_settings_console_off_test PRIVATE crdk)
target_sources(platform_settings_console_off_test PRIVATE
    "${KSHIM_ROOT}/src/core/menu_locale.c" "${KSHIM_ROOT}/src/core/boot_settings.c")

# TB323FU uses its own theme and locale without changing the generic suite.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(TB323FU_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/tb323fu-generated")
set(ENGLISH_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/english-generated")
file(MAKE_DIRECTORY "${ENGLISH_GENERATED}")
file(WRITE "${ENGLISH_GENERATED}/.config" "# CONFIG_KSHIM_MENU_CHINESE is not set\nCONFIG_KSHIM_LVGL_BUFFER_BYTES=243712\n")
foreach(profile TB323FU ENGLISH)
    if(profile STREQUAL "TB323FU")
        set(profile_config "${KSHIM_ROOT}/configs/tb323fu_defconfig")
    else()
        set(profile_config "${ENGLISH_GENERATED}/.config")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E env
        "srctree=${KSHIM_ROOT}/src" "ARCH=aarch64" "HOST_ARCH=${CMAKE_HOST_SYSTEM_PROCESSOR}"
        "HOST_INFO=host-test" "BUILD_TIME=test" "VERSION=0.0.1"
        ${Python3_EXECUTABLE} "${KSHIM_ROOT}/tools/configure_kconfig.py"
        --root "${KSHIM_ROOT}" --config "${profile_config}"
        --header "${${profile}_GENERATED}/config.h"
        --cmake "${${profile}_GENERATED}/config.cmake"
        RESULT_VARIABLE profile_status)
    if(NOT profile_status EQUAL 0)
        message(FATAL_ERROR "Failed to generate ${profile} test config")
    endif()
endforeach()
kshim_test(tb323fu_menu_test tb323fu_menu_test.c ${KSHIM_TEST_UI_SOURCES})
add_executable(tb323fu_ui_preview ui_preview_test.c ${KSHIM_TEST_UI_SOURCES})
target_include_directories(tb323fu_ui_preview PRIVATE "${KSHIM_ROOT}/src/include")
foreach(target tb323fu_menu_test tb323fu_ui_preview)
    target_link_libraries(${target} PRIVATE lvgl kshim_backdrop kshim_dt m)
    target_include_directories(${target} BEFORE PRIVATE "${TB323FU_GENERATED}")
endforeach()

kshim_test(menu_text_chinese_test menu_text_config_test.c)
target_sources(menu_text_chinese_test PRIVATE "${KSHIM_ROOT}/src/core/menu_locale.c")
target_include_directories(menu_text_chinese_test BEFORE PRIVATE "${TB323FU_GENERATED}")
target_compile_definitions(menu_text_chinese_test PRIVATE KSHIM_TEST_EXPECT_CHINESE=1)
kshim_test(menu_text_english_test menu_text_config_test.c)
target_sources(menu_text_english_test PRIVATE "${KSHIM_ROOT}/src/core/menu_locale.c")
target_include_directories(menu_text_english_test BEFORE PRIVATE "${ENGLISH_GENERATED}")
target_compile_definitions(menu_text_english_test PRIVATE KSHIM_TEST_EXPECT_CHINESE=0)
kshim_test(framebuffer_rotation_test framebuffer_rotation_test.c "${KSHIM_ROOT}/src/ui/framebuffer.c")
kshim_test(boot_metrics_test boot_metrics_test.c "${KSHIM_ROOT}/src/core/boot_metrics.c")
target_compile_definitions(boot_metrics_test PRIVATE CONFIG_KSHIM_BOOT_TIMING=1)
target_link_libraries(boot_metrics_test PRIVATE kshim_dt)
kshim_test(boot_metrics_disabled_test boot_metrics_test.c "${KSHIM_ROOT}/src/core/boot_metrics.c")
target_link_libraries(boot_metrics_disabled_test PRIVATE kshim_dt)
kshim_test(qcom_reboot_test qcom_reboot_test.c "${KSHIM_ROOT}/src/drivers/qcom_reboot.c")
target_compile_definitions(qcom_reboot_test PRIVATE CONFIG_KSHIM_QCOM_REBOOT=1)
target_link_libraries(qcom_reboot_test PRIVATE kshim_dt crdk_spmi)
kshim_test(qcom_reboot_sdi_test qcom_reboot_test.c "${KSHIM_ROOT}/src/drivers/qcom_reboot.c")
target_compile_definitions(qcom_reboot_sdi_test PRIVATE CONFIG_KSHIM_QCOM_REBOOT=1 CONFIG_KSHIM_EDL_DISABLE_SDI=1)
target_link_libraries(qcom_reboot_sdi_test PRIVATE kshim_dt crdk_spmi)
kshim_test(platform_advanced_menu_test platform_menu_test.c)
target_compile_definitions(platform_advanced_menu_test PRIVATE CONFIG_KSHIM_MENU_ADVANCED_ACTIONS=1 CONFIG_KSHIM_QCOM_REBOOT=1 CONFIG_KSHIM_MENU_CHINESE=1)
target_include_directories(platform_advanced_menu_test BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${KSHIM_ROOT}/lib/libfdt/include")
target_link_libraries(platform_advanced_menu_test PRIVATE crdk)
target_sources(platform_advanced_menu_test PRIVATE "${KSHIM_ROOT}/src/core/menu_locale.c"
    "${KSHIM_ROOT}/src/core/boot_settings.c")

# Runtime menu language switch profile: same real platform loop, scripted peripherals.
kshim_test(platform_language_menu_test platform_menu_test.c)
target_compile_definitions(platform_language_menu_test PRIVATE
    CONFIG_KSHIM_MENU_LANGUAGE_SWITCH=1
    CONFIG_KSHIM_MENU_CHINESE=1
    CONFIG_KSHIM_MENU_ADVANCED_ACTIONS=1
    CONFIG_KSHIM_QCOM_REBOOT=1)
target_include_directories(platform_language_menu_test BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${KSHIM_ROOT}/lib/libfdt/include")
target_link_libraries(platform_language_menu_test PRIVATE crdk)
target_sources(platform_language_menu_test PRIVATE "${KSHIM_ROOT}/src/core/menu_locale.c"
    "${KSHIM_ROOT}/src/core/boot_settings.c")

kshim_test(menu_locale_test menu_locale_test.c "${KSHIM_ROOT}/src/core/menu_locale.c")
target_compile_definitions(menu_locale_test PRIVATE CONFIG_KSHIM_MENU_LANGUAGE_SWITCH=1 CONFIG_KSHIM_MENU_CHINESE=1)

# Compile-time debug-console switch regression: the real src/drivers/console.c
# against stubbed DT validate/probe, GENI poll UART and PL011 edges. The default
# profile uses the generated board Kconfig (CONFIG_KSHIM_DEBUG_CONSOLE defaults
# to y); the disabled profile forces it off with an empty config.h.
set(KSHIM_CONSOLE_TEST_SOURCES console_config_test.c
    "${KSHIM_ROOT}/src/drivers/console.c")
kshim_test(console_default_test ${KSHIM_CONSOLE_TEST_SOURCES})
target_compile_definitions(console_default_test PRIVATE TEST_CONSOLE_DISABLED=0)
target_include_directories(console_default_test BEFORE PRIVATE
    "${TB323FU_GENERATED}" "${KSHIM_ROOT}/lib/libfdt/include")
target_link_libraries(console_default_test PRIVATE crdk)

kshim_test(console_disabled_test ${KSHIM_CONSOLE_TEST_SOURCES})
target_compile_definitions(console_disabled_test PRIVATE
    TEST_CONSOLE_DISABLED=1 CONFIG_KSHIM_DEBUG_CONSOLE=0)
target_include_directories(console_disabled_test BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${KSHIM_ROOT}/lib/libfdt/include")
target_link_libraries(console_disabled_test PRIVATE crdk)

kshim_test(storage_block_test storage_block_test.c "${KSHIM_ROOT}/src/core/storage_block.c")
kshim_test(storage_ab_test storage_ab_test.c "${KSHIM_ROOT}/src/core/storage_ab.c"
    "${KSHIM_ROOT}/src/core/storage_block.c")
kshim_test(storage_config_test storage_config_test.c "${KSHIM_ROOT}/src/core/storage_block.c"
    "${KSHIM_ROOT}/src/core/storage_config.c")
kshim_test(storage_probe_test storage_probe_test.c "${KSHIM_ROOT}/src/drivers/storage_probe.c")
kshim_test(storage_probe_io_test storage_probe_io_test.c "${KSHIM_ROOT}/src/arch/arm64/storage_probe_io.c")
target_compile_definitions(storage_probe_io_test PRIVATE CONFIG_KSHIM_UFS_NATIVE_IO=1)
kshim_test(storage_probe_export_test storage_probe_export_test.c "${KSHIM_ROOT}/src/dt/storage_probe_export.c")
target_link_libraries(storage_probe_export_test PRIVATE kshim_dt)
kshim_test(storage_ufs_test storage_ufs_test.c "${KSHIM_ROOT}/src/drivers/storage_ufs.c"
    "${KSHIM_ROOT}/src/core/storage_block.c")
# Inherited-HIBERN8 handoff regression: fake MMIO hardware around the real
# transport, so the opt-in manage_link contract is exercised end to end
# without any LVGL dependency.
kshim_test(storage_ufs_link_test storage_ufs_link_test.c
    "${KSHIM_ROOT}/src/drivers/storage_ufs.c"
    "${KSHIM_ROOT}/src/core/storage_block.c")
# Opt-in UFS device power preparation: fake MMIO/DMA around the real transport,
# so prepare/wake/verify/restore is exercised end to end without a stub API.
kshim_test(storage_ufs_power_test storage_ufs_power_test.c
    "${KSHIM_ROOT}/src/drivers/storage_ufs.c"
    "${KSHIM_ROOT}/src/core/storage_block.c")
# Bounded read-only UFS diagnostic sequence: the generic claim/nop/open_lun/
# release transport is stubbed by the test, while the real storage_block.c
# supplies the header/tail CRC. GPT lookup and block reads are wrapped so the
# suite can inject partitions, payloads and failures without a virtual disk.
kshim_test(storage_ufs_read_probe_test storage_ufs_read_probe_test.c
    "${KSHIM_ROOT}/src/drivers/storage_ufs_read_probe.c"
    "${KSHIM_ROOT}/src/core/storage_block.c")
target_link_options(storage_ufs_read_probe_test PRIVATE
    -Wl,--wrap=kshim_gpt_find_backup -Wl,--wrap=kshim_block_read)
kshim_test(storage_ufs_read_power_probe_test storage_ufs_read_probe_test.c
    "${KSHIM_ROOT}/src/drivers/storage_ufs_read_probe.c"
    "${KSHIM_ROOT}/src/core/storage_block.c")
target_compile_definitions(storage_ufs_read_power_probe_test PRIVATE CONFIG_KSHIM_UFS_DEVICE_WAKE=1)
target_link_options(storage_ufs_read_power_probe_test PRIVATE
    -Wl,--wrap=kshim_gpt_find_backup -Wl,--wrap=kshim_block_read)
kshim_test(storage_ufs_read_export_test storage_ufs_read_export_test.c
    "${KSHIM_ROOT}/src/dt/storage_ufs_read_export.c")
target_link_libraries(storage_ufs_read_export_test PRIVATE kshim_dt)
# Early UFS diagnostic screen: the pure renderer only, on real framebuffer
# memory with guard bytes. It never links the storage transport.
kshim_test(storage_ufs_read_screen_test storage_ufs_read_screen_test.c
    "${KSHIM_ROOT}/src/ui/storage_ufs_read_screen.c"
    "${KSHIM_ROOT}/src/ui/framebuffer.c")
kshim_test(storage_sdhci_test storage_sdhci_test.c "${KSHIM_ROOT}/src/drivers/storage_sdhci.c"
    "${KSHIM_ROOT}/src/core/storage_block.c")
kshim_test(storage_ufs_dma_test storage_ufs_dma_test.c
    "${KSHIM_ROOT}/src/drivers/storage_ufs_dma.c")
kshim_test(storage_settings_test storage_settings_test.c "${KSHIM_ROOT}/src/core/storage_settings.c"
    "${KSHIM_ROOT}/src/core/storage_block.c" "${KSHIM_ROOT}/src/core/storage_config.c")
# The real menu persistence path shares the production settings and block layers.
foreach(target platform_menu_test platform_menu_no_power_test platform_menu_console_off_test
        platform_settings_menu_test platform_settings_console_off_test platform_advanced_menu_test
        platform_language_menu_test)
    target_sources(${target} PRIVATE "${KSHIM_ROOT}/src/core/storage_settings.c"
        "${KSHIM_ROOT}/src/core/storage_config.c" "${KSHIM_ROOT}/src/core/storage_block.c")
endforeach()

kshim_test(storage_ufs_settings_test storage_ufs_settings_test.c
    "${KSHIM_ROOT}/src/core/storage_settings.c"
    "${KSHIM_ROOT}/src/core/storage_config.c" "${KSHIM_ROOT}/src/core/storage_block.c")

target_sources(storage_ufs_settings_test PRIVATE "${KSHIM_ROOT}/src/drivers/storage_ufs_settings.c")

kshim_test(storage_settings_export_test storage_settings_export_test.c)
target_link_libraries(storage_settings_export_test PRIVATE kshim_dt)

target_sources(storage_settings_export_test PRIVATE "${KSHIM_ROOT}/src/dt/storage_settings_export.c")

kshim_test(storage_settings_native_test storage_settings_native_test.c
    "${KSHIM_ROOT}/src/arch/arm64/storage_settings_native.c")
target_compile_definitions(storage_settings_native_test PRIVATE CONFIG_KSHIM_SETTINGS_SAVE_TEST=1)

kshim_test(android_touch_spi_trial_test android_touch_spi_trial_test.c
    "${KSHIM_ROOT}/src/dt/tb323fu_android_touch.c")
target_link_libraries(android_touch_spi_trial_test PRIVATE kshim_dt)
add_test(NAME android_touch_profile_test
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/android_touch_profile_test.py"
        --cc "${CMAKE_C_COMPILER}" --crdk-include "${CRDK_ROOT}/Include")
