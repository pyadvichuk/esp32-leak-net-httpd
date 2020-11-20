set(custom_esp_eth_srcs "esp_eth_mac_wallbox.c"
                        "esp_eth_phy_ksz8081.c")

idf_component_register(SRCS "${custom_esp_eth_srcs}"
                       INCLUDE_DIRS "."
                       REQUIRES "esp_event"
                       PRIV_REQUIRES "esp_netif" "driver" "log")
