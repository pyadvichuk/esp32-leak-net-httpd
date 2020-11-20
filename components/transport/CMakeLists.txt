idf_component_register(
    SRCS
        "wifi.c" "ethernet.c"
    INCLUDE_DIRS
        "." ".."
    PRIV_REQUIRES
        custom_esp_eth_phy
        httpd
)
