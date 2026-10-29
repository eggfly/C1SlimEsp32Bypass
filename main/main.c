/* C1 Slim ESP32-S3 N8R8 bypass firmware
 *
 *   SoftAP ──► DNS 劫持 ──► HTTP(S) MITM ──► adbAdmit 响应改 true
 *
 * 启动后在 UART / USB-CDC 打印状态，等 C1 Slim 连上来并请求 adbAdmit。
 * 流程参考 ../../C1Max-Capture/root-implant/README.md 的 Max 攻击链。
 */

#include "wifi_ap.h"
#include "dns_hijack.h"
#include "https_mitm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/inet.h"

static const char *TAG = "c1slim";

static void banner(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=========================================================");
    ESP_LOGI(TAG, " C1 Slim ESP32-S3 bypass  ——  adbAdmit MITM over SoftAP");
    ESP_LOGI(TAG, "=========================================================");
    ESP_LOGI(TAG, " 1. 用 C1 Slim 连 SSID=\"%s\" 密码 \"%s\"",
             C1SLIM_AP_SSID, C1SLIM_AP_PASS);
    ESP_LOGI(TAG, " 2. 设备 关于本机 连点系统版本进隐藏菜单 -> 打开 ADB");
    ESP_LOGI(TAG, " 3. 盯这里：出现 adbAdmit HIT 说明响应已改");
    ESP_LOGI(TAG, " 4. macOS 侧 'adb devices' 应能看到设备（root）");
    ESP_LOGI(TAG, "---------------------------------------------------------");
}

void app_main(void)
{
    banner();

    esp_netif_t *ap = c1slim_wifi_start_ap();

    esp_netif_ip_info_t ip = {0};
    esp_netif_get_ip_info(ap, &ip);
    char ipb[16], gwb[16], mkb[16];
    esp_ip4addr_ntoa(&ip.ip,      ipb, sizeof(ipb));
    esp_ip4addr_ntoa(&ip.gw,      gwb, sizeof(gwb));
    esp_ip4addr_ntoa(&ip.netmask, mkb, sizeof(mkb));
    ESP_LOGI(TAG, "AP netif ip=%s gw=%s mask=%s", ipb, gwb, mkb);

    /* DNS 把所有 A 查询都指回我们自己 */
    c1slim_dns_hijack_start(ip.ip.addr);

    /* 起 HTTP+HTTPS 四条监听 */
    c1slim_https_mitm_start();

    ESP_LOGI(TAG, "ready — 等 C1 Slim 连进来");

    /* 空转，让日志任务跑 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
        wifi_sta_list_t sta;
        if (esp_wifi_ap_get_sta_list(&sta) == ESP_OK && sta.num > 0) {
            ESP_LOGI(TAG, "clients=%d", sta.num);
        }
    }
}
