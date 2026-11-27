idf_component_register(
    SRCS "app_main.c" "ble_midi.c" "boot_button.c" "commands.c" "display_reset.c" "looper.c"
         "ui.c" "ui_main.c" "ui_pad.c" "ui_setup.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash esp_timer esp_driver_gpio freertos esp32_s3_touch_amoled_1_8)
