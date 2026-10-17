idf_component_register(SRCS "main.c" "epd.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_driver_spi)
