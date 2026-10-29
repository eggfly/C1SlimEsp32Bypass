/* SoftAP：开热点让 C1 Slim 连进来。
 * 参考 C1Max 根植入链 —— 设备连上我们的 AP → 它向 api.mpen.com.cn 发请求
 * → DNS 劫持到自己 → HTTPS MITM 改响应。 */

#include "wifi_ap.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "wifi_ap";

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *e = data;
        ESP_LOGI(TAG, ">>> client connected: %02x:%02x:%02x:%02x:%02x:%02x aid=%d",
                 e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5],
                 e->aid);
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *e = data;
        ESP_LOGI(TAG, "<<< client left:      %02x:%02x:%02x:%02x:%02x:%02x aid=%d",
                 e->mac[0], e->mac[1], e->mac[2], e->mac[3], e->mac[4], e->mac[5],
                 e->aid);
    }
}

esp_netif_t *c1slim_wifi_start_ap(void)
{
    /* NVS（WiFi 栈要） */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL, NULL));

    wifi_config_t ap = {
        .ap = {
            .ssid            = C1SLIM_AP_SSID,
            .ssid_len        = strlen(C1SLIM_AP_SSID),
            .channel         = C1SLIM_AP_CHANNEL,
            .password        = C1SLIM_AP_PASS,
            .max_connection  = C1SLIM_AP_MAX_CONN,
            /* 老设备兼容：WPA/WPA2 混合、关掉 PMF、强制 CCMP(AES) 加密 */
            .authmode        = WIFI_AUTH_WPA_WPA2_PSK,
            .pairwise_cipher = WIFI_CIPHER_TYPE_CCMP,
            .pmf_cfg         = { .capable = false, .required = false },
            .beacon_interval = 100,
        },
    };
    if (strlen(C1SLIM_AP_PASS) == 0) {
        ap.ap.authmode = WIFI_AUTH_OPEN;
    }

    /* IDF 5.1+ 默认把 AP 的 PMF capable 开着——这里强制关掉再 set_config */
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP up: SSID=\"%s\" PSK=\"%s\" ch=%d",
             C1SLIM_AP_SSID, C1SLIM_AP_PASS, C1SLIM_AP_CHANNEL);
    ESP_LOGI(TAG, "AP IP:  192.168.4.1");
    return ap_netif;
}
