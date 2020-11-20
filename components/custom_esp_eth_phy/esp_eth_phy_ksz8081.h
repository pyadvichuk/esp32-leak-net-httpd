#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_eth_phy.h"

/**
* @brief Create a PHY instance of KSZ8081
*
* @param[in] config: configuration of PHY
*
* @return
*      - instance: create PHY instance successfully
*      - NULL: create PHY instance failed because some error occurred
*/
esp_eth_phy_t *esp_eth_phy_new_ksz8081(const eth_phy_config_t *config);

#ifdef __cplusplus
}
#endif
