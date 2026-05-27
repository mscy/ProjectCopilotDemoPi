idf_component_register(
    SRCS
        "main.c"
        "drivers/i2c_bus.c"
        "drivers/led_pwm.c"
        "drivers/lcd.c"
        "drivers/bme690.c"
        "drivers/bsec2_iaq.c"
        "drivers/bmi270.c"
        "drivers/max98357a.c"
        "drivers/ws2812b.c"
        "drivers/encoder.c"
        "drivers/buttons.c"
        "drivers/sdcard.c"
        "drivers/battery.c"
        "drivers/m5unit_lcd_i2c.c"
        "app/mode.c"
        "app/power.c"
        "app/display.c"
        "app/mp3_player.c"
    INCLUDE_DIRS
        "."
        "drivers"
        "app"
    REQUIRES
        driver
        esp_adc
        esp_lcd
        esp_timer
        nvs_flash
        fatfs
        sdmmc
        spi_flash
        espressif__esp_audio_codec
)

set(BSEC2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../third_party/Bosch-BSEC2-Library/src")
set(BSEC2_LIB  "${BSEC2_ROOT}/esp32s3/libalgobsec.a")

if(EXISTS ${BSEC2_LIB})
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${BSEC2_ROOT}"
        "${BSEC2_ROOT}/inc"
    )
    target_link_libraries(${COMPONENT_LIB} INTERFACE "${BSEC2_LIB}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BSEC2_EXTERNAL_LIB=1)
else()
    message(WARNING "BSEC2 library not found at ${BSEC2_LIB}; IAQ will use fallback estimator")
endif()
