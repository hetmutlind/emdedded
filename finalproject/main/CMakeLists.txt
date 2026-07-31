idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer esp_driver_gpio esp_driver_i2c u8g2
    PRIV_REQUIRES esp_driver_spi
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)