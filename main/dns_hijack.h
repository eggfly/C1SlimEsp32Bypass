#pragma once
#include <stdint.h>

/* UDP:53 捕获型 DNS：所有 A/AAAA 查询都回我们自己 (AP 网关 IP)。
 * 这样 C1 Slim 查 api.mpen.com.cn 也会落到 ESP32 上。 */
void c1slim_dns_hijack_start(uint32_t self_ip_be);
