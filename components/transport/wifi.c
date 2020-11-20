#include <string.h>
#include <stdatomic.h>
#include <esp_wifi.h>
#include <esp_log.h>

#include "httpd.h"

#define WALLBOX_WIFI_STA_SSID CONFIG_WIFI_STA_SSID
#define WALLBOX_WIFI_STA_PASS CONFIG_WIFI_STA_PASSWORD

#define WALLBOX_WIFI_AP_PASS CONFIG_WIFI_AP_PASSWORD
#define WALLBOX_WIFI_AP_MAX_CONN CONFIG_WIFI_AP_MAX_CONNECTIONS

typedef struct local_handler_t
{
  esp_netif_t* netif;
  httpd_handle_wrapper_t* server;
} local_handler_t;

#define TAG "WBWIFI"

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            esp_wifi_connect();
            break;
        default:
            break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t* ip = event_data;
    local_handler_t *data = (local_handler_t*)arg;

    if (NULL != data)
    {
        esp_netif_t* netif = data->netif;
        httpd_handle_wrapper_t* pServer = data->server;

        char buf_ip[16];
        char buf_gw[16];
        char buf_nm[16];
        esp_ip4addr_ntoa(&ip->ip_info.ip,buf_ip,sizeof(buf_ip));
        esp_ip4addr_ntoa(&ip->ip_info.gw,buf_gw,sizeof(buf_gw));
        esp_ip4addr_ntoa(&ip->ip_info.netmask,buf_nm,sizeof(buf_nm));

        ESP_LOGI(TAG, "WIFI STA got IP <%s>, mask <%s>, gw <%s>", buf_ip, buf_nm, buf_gw);

        esp_netif_dns_info_t ip_dns={0,};
        if (ESP_OK == esp_netif_get_dns_info(netif,ESP_NETIF_DNS_MAIN,&ip_dns))
        {
            esp_ip4addr_ntoa(&ip_dns.ip.u_addr.ip4,buf_ip,sizeof(buf_ip));
            ESP_LOGI(TAG, "WIFI STA DNS (main) <%s>", buf_ip);
        }

        if (ESP_OK == esp_netif_get_dns_info(netif,ESP_NETIF_DNS_BACKUP,&ip_dns))
        {
            esp_ip4addr_ntoa(&ip_dns.ip.u_addr.ip4,buf_ip,sizeof(buf_ip));
            ESP_LOGI(TAG, "WIFI STA DNS (backup) <%s>", buf_ip);
        }

        if (ESP_OK == esp_netif_get_dns_info(netif,ESP_NETIF_DNS_FALLBACK,&ip_dns))
        {
            esp_ip4addr_ntoa(&ip_dns.ip.u_addr.ip4,buf_ip,sizeof(buf_ip));
            ESP_LOGI(TAG, "WIFI STA DNS (fallback) <%s>", buf_ip);
        }

        start_web_server(pServer);
    }
    else
    {
        /* code */
    }
}

void initialize_wifi(void *arg)
{
    esp_netif_t * sta_netif = esp_netif_create_default_wifi_sta();

    uint32_t dns=0x08080808U;
    esp_netif_dns_info_t ip_dns;
    IP4_ADDR( &ip_dns.ip.u_addr.ip4,
                ((dns>>24)&0xff),
                ((dns>>16)&0xff),
                ((dns>> 8)&0xff),
                ((dns>> 0)&0xff));
    esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_FALLBACK, &ip_dns);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    local_handler_t* args = malloc(sizeof(*args));
                    args->server = arg;
                    args->netif  = sta_netif;


    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, arg));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, args));

    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = WALLBOX_WIFI_STA_SSID,
            .password = WALLBOX_WIFI_STA_PASS,
        },
    };

    ESP_LOGI(TAG, "Setting WiFi configuration STA SSID %s...", wifi_sta_config.sta.ssid);
    ESP_LOGI(TAG, "Setting WiFi configuration STA password %s...", wifi_sta_config.sta.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
