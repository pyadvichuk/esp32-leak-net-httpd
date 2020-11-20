#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_sntp.h>
#include <nvs_flash.h>

#include "httpd/httpd.h"

extern void initialize_ethernet(void *arg);
extern void initialize_wifi(void *arg);

EXT_RAM_ATTR httpd_handle_wrapper_t server =
{
    .refs = 0,
    .handler = NULL
};

static uint32_t memory_report(int diff_only,uint32_t caps,const char* capsname,uint32_t prev_free)
{
    static multi_heap_info_t heap_info={0,};

    memset(&heap_info,0,sizeof(heap_info));
    heap_caps_get_info(&heap_info, caps);

    if (0 == diff_only)
    {
        ESP_LOGI("MEMSTAT", "TYPE=%s  free=%zu allocd=%zu largest=%zu btotal=%zu bfree=%zu ballocd=%zu",capsname,
                                heap_info.total_free_bytes,heap_info.total_allocated_bytes,heap_info.largest_free_block,
                                heap_info.total_blocks,heap_info.free_blocks,heap_info.allocated_blocks);
    }
    else if ((prev_free != heap_info.total_free_bytes) && (prev_free != 0))
    {
        int diff = prev_free - heap_info.total_free_bytes;
        if (diff > 0)
        {
            ESP_LOGI("MEMSTAT", "TYPE=%s  +%i  free=%zu", capsname, diff, heap_info.total_free_bytes);
        }
        else
        {
            ESP_LOGI("MEMSTAT", "TYPE=%s  %i  free=%zu", capsname, diff, heap_info.total_free_bytes);
        }
    }

    return heap_info.total_free_bytes;
}

static void time_cb(struct timeval* arg)
{
      ESP_LOGI(__func__,"SNTP time synced");
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    initialize_ethernet(&server);
    // initialize_wifi(&server);

    sntp_set_time_sync_notification_cb(time_cb);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0,"ua.pool.ntp.org");
    sntp_init();

    uint32_t divider = 0;
    uint32_t hp_size = esp_get_free_heap_size();
    uint32_t hp_g32_size = memory_report(1,MALLOC_CAP_32BIT,"g32",0);
    uint32_t hp_g08_size = memory_report(1,MALLOC_CAP_8BIT,"g08",0);
    uint32_t hp_dma_size = memory_report(1,MALLOC_CAP_DMA,"dma",0);
    uint32_t hp_spi_size = memory_report(1,MALLOC_CAP_SPIRAM,"spi",0);
    uint32_t hp_int_size = memory_report(1,MALLOC_CAP_INTERNAL,"int",0);
    uint32_t hp_def_size = memory_report(1,MALLOC_CAP_DEFAULT,"def",0);
    for(;;)
    {
        if (60 == divider)
        {
            hp_g32_size = memory_report(0,MALLOC_CAP_32BIT,"g32",0);
            hp_g08_size = memory_report(0,MALLOC_CAP_8BIT,"g08",0);
            hp_dma_size = memory_report(0,MALLOC_CAP_DMA,"dma",0);
            hp_spi_size = memory_report(0,MALLOC_CAP_SPIRAM,"spi",0);
            hp_int_size = memory_report(0,MALLOC_CAP_INTERNAL,"int",0);
            hp_def_size = memory_report(0,MALLOC_CAP_DEFAULT,"def",0);
        }
        else if (0 == (divider % 5))
        {
            hp_g32_size = memory_report(1,MALLOC_CAP_32BIT,"g32",hp_g32_size);
            hp_g08_size = memory_report(1,MALLOC_CAP_8BIT,"g08",hp_g08_size);
            hp_dma_size = memory_report(1,MALLOC_CAP_DMA,"dma",hp_dma_size);
            hp_spi_size = memory_report(1,MALLOC_CAP_SPIRAM,"spi",hp_spi_size);
            hp_int_size = memory_report(1,MALLOC_CAP_INTERNAL,"int",hp_int_size);
            hp_def_size = memory_report(1,MALLOC_CAP_DEFAULT,"def",hp_def_size);
        }

        uint32_t n_hp_size = esp_get_free_heap_size();
        if (n_hp_size != hp_size)
        {
            printf("HEAP  %c  ==> %d\n", ((n_hp_size < hp_size)?'-':'+'), n_hp_size-4000000);
        }
        hp_size = n_hp_size;
        divider = (60 == divider) ? 0 : (divider+1);
        vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }
}

void sntp_sync_time(struct timeval* tm)
{
    settimeofday(tm,0);
    sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}
