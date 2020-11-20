#include <stdatomic.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_https_server.h>

#include "httpd.h"

#define PROJECT_HTTP_SERVER_NAME	    "Server httpd 0.1"
#define PROJECT_HTTP_MAX_URI_HANDLERS	50
#define WB_HTTPD_BUFOUT_SIZE	        4096
#define TAG                             "WBHTTP"

// main.js
#include "http_static.inc"
esp_err_t http_handler__get_file_js(httpd_req_t *req)
{
    httpd_resp_set_type(req, httpmime_file_js);
    httpd_resp_set_hdr(req, "Server", PROJECT_HTTP_SERVER_NAME);
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=604800, immutable");
    httpd_resp_set_hdr(req, "ETag", digest_file_js);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, httpdoc_file_js, httpsize_file_js);
    return ESP_OK;
}

static const httpd_uri_t httphandler_get_file_js =
{
    .uri       = "/main.js",
    .method    = HTTP_GET,
    .handler   = http_handler__get_file_js
};

//-----------------------------------------------------------------------------

static httpd_handle_t start_webserver(void)
{
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
                       conf.httpd.stack_size       = 16384;
                       conf.httpd.max_uri_handlers = PROJECT_HTTP_MAX_URI_HANDLERS;
                       conf.httpd.max_open_sockets = 8;
                       conf.httpd.backlog_conn     = 5;
                       conf.transport_mode         = HTTPD_SSL_TRANSPORT_INSECURE;
                       conf.httpd.uri_match_fn     = httpd_uri_match_wildcard;
                       conf.port_insecure          = 80;

    ESP_LOGI(TAG, "Starting server");

    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "Error starting server %d!",ret);
        return NULL;
    }

    httpd_register_uri_handler(server, &httphandler_get_file_js);

    return server;
}

static void stop_webserver(httpd_handle_t server)
{
    httpd_ssl_stop(server);
}

void start_web_server(httpd_handle_wrapper_t* pWrapper)
{
    if (0 == pWrapper->refs)
    {
        pWrapper->handler = start_webserver();
        ESP_LOGI(TAG, "Web server started");
    }
    pWrapper->refs++;
}

void stop_web_server(httpd_handle_wrapper_t* pWrapper)
{
    pWrapper->refs -= (pWrapper->refs > 0) ? 1 : 0;
    if ((pWrapper->refs == 0) && (NULL != pWrapper->handler))
    {
        stop_webserver(pWrapper->handler);
        pWrapper->handler = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}
