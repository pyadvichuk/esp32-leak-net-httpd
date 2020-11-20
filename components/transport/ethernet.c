#include <stdatomic.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_log.h>
#include <driver/gpio.h>

#include "esp_eth.h"
#include "esp_eth_mac_wallbox.h"
#include "esp_eth_phy_ksz8081.h"
#include "httpd.h"

#ifndef CONFIG_WBOX_USE_INTERNAL_ETHERNET
#error Mandatory definition 'WBOX_USE_INTERNAL_ETHERNET' is not set
#endif
#ifndef CONFIG_WBOX_ETH_PHY_KSZ8081
#error Mandatory definition 'WBOX_ETH_PHY_KSZ8081' is not set
#endif

#define TAG "WBETH"

typedef struct local_handler_t
{
  esp_netif_t* netif;
  httpd_handle_wrapper_t* server;
} local_handler_t;


/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    char buf_ip[16];
    char buf_gw[16];
    char buf_nm[16];
    ip_event_got_ip_t* ip=event_data;

    esp_ip4addr_ntoa(&ip->ip_info.ip,buf_ip,sizeof(buf_ip));
    esp_ip4addr_ntoa(&ip->ip_info.gw,buf_gw,sizeof(buf_gw));
    esp_ip4addr_ntoa(&ip->ip_info.netmask,buf_nm,sizeof(buf_nm));

    ESP_LOGI(TAG, "Ethernet got IP <%s>, mask <%s>, gw <%s>",buf_ip,buf_nm,buf_gw);

    local_handler_t* data=arg;
    httpd_handle_wrapper_t *pServer = data->server;
    start_web_server(pServer);
}

void initialize_ethernet(void *arg)
{
    ESP_LOGI(TAG, "Start initializing....");

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    uint32_t dns=0x08080808U;
    esp_netif_dns_info_t ip_dns;
    IP4_ADDR( &ip_dns.ip.u_addr.ip4,
                ((dns>>24)&0xff),
                ((dns>>16)&0xff),
                ((dns>> 8)&0xff),
                ((dns>> 0)&0xff));
    esp_netif_set_dns_info(eth_netif,ESP_NETIF_DNS_FALLBACK,&ip_dns);

    local_handler_t* args=malloc(sizeof(*args));
    args->server=arg;
    args->netif=eth_netif;

    ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, args));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.smi_mdc_gpio_num = CONFIG_WBOX_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_WBOX_ETH_MDIO_GPIO;

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_WBOX_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_WBOX_ETH_PHY_RST_GPIO;

    esp_eth_mac_t *mac = esp_eth_mac_new_wallbox(&mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz8081(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;

    if (esp_eth_driver_install(&config, &eth_handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "Ethernet cannot be initialized or not present");
    }
    else
    {
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
        ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    }
    ESP_LOGI(TAG, "Initializing finished");
}



