#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_eth_mac.h"
#include "sdkconfig.h"


#if CONFIG_ETH_USE_ESP32_EMAC
/**
* @brief Create ESP32 Ethernet MAC instance
*
* @param config: Ethernet MAC configuration
*
* @return
*      - instance: create MAC instance successfully
*      - NULL: create MAC instance failed because some error occurred
*/
esp_eth_mac_t *esp_eth_mac_new_wallbox(const eth_mac_config_t *config);
#endif

#ifdef __cplusplus
}
#endif
