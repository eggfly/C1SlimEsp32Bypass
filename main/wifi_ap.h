#pragma once
#include "esp_netif.h"

#define C1SLIM_AP_SSID      "C1SLIM-LAB"
#define C1SLIM_AP_PASS      "12345678"
#define C1SLIM_AP_CHANNEL   6
#define C1SLIM_AP_MAX_CONN  4

/* 启动 SoftAP。返回 ESP32 自己的 netif，用来喂 DNS/HTTP 服务器。
 * AP 子网固定 192.168.4.1/24，DHCP 自动分配。 */
esp_netif_t *c1slim_wifi_start_ap(void);
