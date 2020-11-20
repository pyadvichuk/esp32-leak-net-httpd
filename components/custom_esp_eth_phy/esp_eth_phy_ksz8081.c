#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_eth.h"
#include "eth_phy_regs_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "ksz8081";
#define PHY_CHECK(a, str, goto_tag, ...)                                          \
    do                                                                            \
    {                                                                             \
        if (!(a))                                                                 \
        {                                                                         \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                        \
        }                                                                         \
    } while (0)

/***************Vendor Specific Register***************/

/**
 * @brief PCR(Page Control Register)
 *
 */
typedef union {
    struct {
        uint32_t register_page_select : 5; /* Select register page, default is 16 */
        uint32_t reserved : 11;            /* Reserved */
    };
    uint32_t val;
} pcr_reg_t;
#define ETH_PHY_PCR_REG_ADDR (0x14)

/**
 * @brief ISR(Interrupt Status Register), Page 16
 *
 */
typedef union {
    struct {
        uint32_t link_changed : 1;   /* Flag to indicate link status change interrupt */
        uint32_t duplex_changed : 1; /* Flag to indicate duplex change interrupt */
        uint32_t speed_changed : 1;  /* Flag to indicate speed change interrupt */
        uint32_t intr_status : 1;    /* Flag to indicate interrupt status */
        uint32_t reserved1 : 4;      /* Reserved */
        uint32_t link_mask : 1;      /* Mask link change interrupt */
        uint32_t duplex_mask : 1;    /* Mask duplex change interrupt */
        uint32_t speed_mask : 1;     /* Mask speed change interrupt */
        uint32_t all_mask : 1;       /* Mask all interrupt */
        uint32_t reserved2 : 3;      /* Reserved */
        uint32_t use_intr_pin : 1;   /* Set high to use INTR and INTR_32 as an interrupt pin */
    };
    uint32_t val;
} isr_reg_t;
#define ETH_PHY_ISR_REG_ADDR (0x11)

/**
 * @brief PHY MDI/MDIX Control and Specific Status Register, Page 16
 *
 */
typedef union {
    struct {
        uint32_t op_mode : 3;    /* Operation Mode Idicator */
        uint32_t force_mdix : 1; /* Force the MDIX channel to be selected */
        uint32_t reserved1 : 4;  /* Reserved */
        uint32_t link_up : 1;    /* Indicate the link status is OK or FAIL */
        uint32_t reserved2 : 7;  /* Reserved */
    };
    uint32_t val;
} cssr_reg_t;
#define ETH_PHY_CSSR_REG_ADDR (0x1E)

/**
 * @brief PSCR(PHY Specific Control Register), Page 1
 *
 */
typedef union {
    struct {
        uint32_t reserved1 : 7;      /* Reserved */
        uint32_t force_link_100 : 1; /* Force Link 100 */
        uint32_t force_link_10 : 1;  /* Force Link 10 */
        uint32_t reserved2 : 7;      /* Reserved */
    };
    uint32_t val;
} pscr_reg_t;
#define ETH_PHY_PSCR_REG_ADDR (0x11)

typedef struct {
    esp_eth_phy_t parent;
    esp_eth_mediator_t *eth;
    uint32_t addr;
    uint32_t reset_timeout_ms;
    uint32_t autonego_timeout_ms;
    eth_link_t link_status;
    int reset_gpio_num;
} phy_ksz8081_t;

static esp_err_t ksz8081_page_select(phy_ksz8081_t *ksz8081, uint32_t page)
{
    esp_eth_mediator_t *eth = ksz8081->eth;
    pcr_reg_t pcr = {
        .register_page_select = page
    };
    PHY_CHECK(eth->phy_reg_write(eth, ksz8081->addr, ETH_PHY_PCR_REG_ADDR, pcr.val) == ESP_OK, "write PCR failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t ksz8081_update_link_duplex_speed(phy_ksz8081_t *ksz8081)
{
    esp_eth_mediator_t *eth = ksz8081->eth;
    eth_speed_t speed = ETH_SPEED_10M;
    eth_duplex_t duplex = ETH_DUPLEX_HALF;
    cssr_reg_t cssr;
    bmsr_reg_t bmsr;
    PHY_CHECK(ksz8081_page_select(ksz8081, 16) == ESP_OK, "select page 16 failed", err);
    PHY_CHECK(eth->phy_reg_read(eth, ksz8081->addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)) == ESP_OK,
              "read BMSR failed", err);
    PHY_CHECK(eth->phy_reg_read(eth, ksz8081->addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)) == ESP_OK,
              "read BMSR failed", err);
    eth_link_t link = bmsr.link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
    /* check if link status changed */
    if (ksz8081->link_status != link) {
        /* when link up, read negotiation result */
        if (link == ETH_LINK_UP) {
            PHY_CHECK(eth->phy_reg_read(eth, ksz8081->addr, ETH_PHY_CSSR_REG_ADDR, &(cssr.val)) == ESP_OK,
                      "read CSSR failed", err);
            switch (cssr.op_mode) {
            case 1: //10M Half
                speed = ETH_SPEED_10M;
                duplex = ETH_DUPLEX_HALF;
                break;
            case 2: //100M Half
                speed = ETH_SPEED_100M;
                duplex = ETH_DUPLEX_HALF;
                break;
            case 5: //10M Full
                speed = ETH_SPEED_10M;
                duplex = ETH_DUPLEX_FULL;
                break;
            case 6: //100M Full
                speed = ETH_SPEED_100M;
                duplex = ETH_DUPLEX_FULL;
                break;
            default:
                break;
            }
            PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed) == ESP_OK,
                      "change speed failed", err);
            PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex) == ESP_OK,
                      "change duplex failed", err);
        }
        PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link) == ESP_OK,
                  "chagne link failed", err);
        ksz8081->link_status = link;
    }
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t ksz8081_set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth)
{
    PHY_CHECK(eth, "can't set mediator to null", err);
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    ksz8081->eth = eth;
    return ESP_OK;
err:
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t ksz8081_get_link(esp_eth_phy_t *phy)
{
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    /* Updata information about link, speed, duplex */
    PHY_CHECK(ksz8081_update_link_duplex_speed(ksz8081) == ESP_OK, "update link duplex speed failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t ksz8081_reset(esp_eth_phy_t *phy)
{
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    ksz8081->link_status = ETH_LINK_DOWN;
    esp_eth_mediator_t *eth = ksz8081->eth;
    bmcr_reg_t bmcr = {.reset = 1};
    PHY_CHECK(eth->phy_reg_write(eth, ksz8081->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val) == ESP_OK,
              "write BMCR failed", err);
    /* wait for reset complete */
    uint32_t to = 0;
    for (to = 0; to < ksz8081->reset_timeout_ms / 10; to++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        PHY_CHECK(eth->phy_reg_read(eth, ksz8081->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK,
                  "read BMCR failed", err);
        if (!bmcr.reset) {
            break;
        }
    }
    PHY_CHECK(to < ksz8081->reset_timeout_ms / 10, "reset timeout", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t ksz8081_reset_hw(esp_eth_phy_t *phy)
{
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    if (ksz8081->reset_gpio_num >= 0) {
        gpio_pad_select_gpio(ksz8081->reset_gpio_num);
        gpio_set_direction(ksz8081->reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(ksz8081->reset_gpio_num, 0);
        gpio_set_level(ksz8081->reset_gpio_num, 1);
    }
    return ESP_OK;
}

static esp_err_t ksz8081_negotiate(esp_eth_phy_t *phy)
{
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    esp_eth_mediator_t *eth = ksz8081->eth;
    /* Restart auto negotiation */
    bmcr_reg_t bmcr = {
        .speed_select = 1,     /* 100Mbps */
        .duplex_mode = 1,      /* Full Duplex */
        .en_auto_nego = 1,     /* Auto Negotiation */
        .restart_auto_nego = 1 /* Restart Auto Negotiation */
    };
    PHY_CHECK(eth->phy_reg_write(eth, ksz8081->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val) == ESP_OK,
              "write BMCR failed", err);
    /* Wait for auto negotiation complete */
    bmsr_reg_t bmsr;
    uint32_t to = 0;
    for (to = 0; to < ksz8081->autonego_timeout_ms / 10; to++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        PHY_CHECK(eth->phy_reg_read(eth, ksz8081->addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)) == ESP_OK,
                  "read BMSR failed", err);
        if (bmsr.auto_nego_complete) {
            break;
        }
    }
    /* Auto negotiation failed, maybe no network cable plugged in, so output a warning */
    if (to >= ksz8081->autonego_timeout_ms / 10) {
        ESP_LOGW(TAG, "auto negotiation timeout");
    }
    /* Updata information about link, speed, duplex */
    PHY_CHECK(ksz8081_update_link_duplex_speed(ksz8081) == ESP_OK, "update link duplex speed failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t ksz8081_pwrctl(esp_eth_phy_t *phy, bool enable)
{
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    esp_eth_mediator_t *eth = ksz8081->eth;
    bmcr_reg_t bmcr;
    PHY_CHECK(eth->phy_reg_read(eth, ksz8081->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK,
              "read BMCR failed", err);
    if (!enable) {
        /* Enable IEEE Power Down Mode */
        bmcr.power_down = 1;
    } else {
        /* Disable IEEE Power Down Mode */
        bmcr.power_down = 0;
    }
    PHY_CHECK(eth->phy_reg_write(eth, ksz8081->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val) == ESP_OK,
              "write BMCR failed", err);
    PHY_CHECK(eth->phy_reg_read(eth, ksz8081->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK,
              "read BMCR failed", err);
    if (!enable) {
        PHY_CHECK(bmcr.power_down == 1, "power down failed", err);
    } else {
        PHY_CHECK(bmcr.power_down == 0, "power up failed", err);
    }
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t ksz8081_set_addr(esp_eth_phy_t *phy, uint32_t addr)
{
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    ksz8081->addr = addr;
    return ESP_OK;
}

static esp_err_t ksz8081_get_addr(esp_eth_phy_t *phy, uint32_t *addr)
{
    PHY_CHECK(addr, "addr can't be null", err);
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    *addr = ksz8081->addr;
    return ESP_OK;
err:
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t ksz8081_del(esp_eth_phy_t *phy)
{
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    free(ksz8081);
    return ESP_OK;
}

static esp_err_t ksz8081_init(esp_eth_phy_t *phy)
{
    phy_ksz8081_t *ksz8081 = __containerof(phy, phy_ksz8081_t, parent);
    esp_eth_mediator_t *eth = ksz8081->eth;
    /* Power on Ethernet PHY */
    PHY_CHECK(ksz8081_pwrctl(phy, true) == ESP_OK, "power control failed", err);
    /* Reset Ethernet PHY */
    PHY_CHECK(ksz8081_reset(phy) == ESP_OK, "reset failed", err);
    /* Check PHY ID */
    phyidr1_reg_t id1;
    phyidr2_reg_t id2;
    PHY_CHECK(eth->phy_reg_read(eth, ksz8081->addr, ETH_PHY_IDR1_REG_ADDR, &(id1.val)) == ESP_OK, "read ID1 failed", err);
    PHY_CHECK(eth->phy_reg_read(eth, ksz8081->addr, ETH_PHY_IDR2_REG_ADDR, &(id2.val)) == ESP_OK, "read ID2 failed", err);
    PHY_CHECK(id1.oui_msb == 0x22 && id2.oui_lsb == 0x05 && id2.vendor_model == 0x16, "wrong chip ID", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t ksz8081_deinit(esp_eth_phy_t *phy)
{
    /* Power off Ethernet PHY */
    PHY_CHECK(ksz8081_pwrctl(phy, false) == ESP_OK, "power control failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

esp_eth_phy_t *esp_eth_phy_new_ksz8081(const eth_phy_config_t *config)
{
    PHY_CHECK(config, "can't set phy config to null", err);
    phy_ksz8081_t *ksz8081 = calloc(1, sizeof(phy_ksz8081_t));
    PHY_CHECK(ksz8081, "calloc ksz8081 failed", err);
    ksz8081->addr = config->phy_addr;
    ksz8081->reset_timeout_ms = config->reset_timeout_ms;
    ksz8081->reset_gpio_num = config->reset_gpio_num;
    ksz8081->link_status = ETH_LINK_DOWN;
    ksz8081->autonego_timeout_ms = config->autonego_timeout_ms;
    ksz8081->parent.reset = ksz8081_reset;
    ksz8081->parent.reset_hw = ksz8081_reset_hw;
    ksz8081->parent.init = ksz8081_init;
    ksz8081->parent.deinit = ksz8081_deinit;
    ksz8081->parent.set_mediator = ksz8081_set_mediator;
    ksz8081->parent.negotiate = ksz8081_negotiate;
    ksz8081->parent.get_link = ksz8081_get_link;
    ksz8081->parent.pwrctl = ksz8081_pwrctl;
    ksz8081->parent.get_addr = ksz8081_get_addr;
    ksz8081->parent.set_addr = ksz8081_set_addr;
    ksz8081->parent.del = ksz8081_del;

    return &(ksz8081->parent);
err:
    return NULL;
}
