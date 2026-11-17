set(FINDER_BLE_SOURCE "ble_scan.c")
set(FINDER_EXTRA_REQUIRES "bt")
if(CONFIG_FINDER_EMULATOR_BUILD)
    set(FINDER_BLE_SOURCE "ble_scan_sim.c")
    set(FINDER_EXTRA_REQUIRES "")
endif()

idf_component_register(
    SRCS "main.c"
         "signal_filter.c"
         ${FINDER_BLE_SOURCE}
         "finder_model.c"
         "finder_storage.c"
         "finder_audio.c"
         "finder_audio_synth.c"
         "finder_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support esp_timer nvs_flash ${FINDER_EXTRA_REQUIRES}
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/finder_font_14.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/finder_font_20.c"
)
