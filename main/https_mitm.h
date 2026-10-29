#pragma once

/* 同时起 4 个服务器：
 *   HTTP  80 / 8080
 *   HTTPS 443 / 8443
 * 对任意 URL：
 *   - 路径含 adbAdmit 或 adb_allow 的 → 返回 success:true 的 JSON
 *   - 其他 → 返回一个 "errorCode":"200" 的空成功，避免设备侧抛错
 *
 * 所有命中会打到 UART / USB-CDC，方便你盯着烧完之后设备到底连了没。
 */
void c1slim_https_mitm_start(void);
