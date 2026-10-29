/* 捕获式 DNS 劫持
 *
 * 对任意 A 查询回我们自己的 IP；AAAA 回空 answer（强制走 v4）；其他回
 * NOTIMP。要点是极简解析足够应付 C1 Slim 这种只做标准查询的客户端。
 * 不走 lwip 的 getaddrinfo，直接裸 socket 处理，避免占用系统 resolver。
 */

#include "dns_hijack.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "dns";

#define DNS_PORT 53

/* 标准 DNS 头（RFC 1035） */
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd, an, ns, ar;
} dns_hdr_t;

#define QTYPE_A    1
#define QTYPE_AAAA 28

/* 把问题名读出来（只用来打印）；回包里我们用 c0 0c 指针引用问题名。 */
static int read_qname(const uint8_t *buf, int len, int off, char *out, int outsz)
{
    int p = off, w = 0;
    while (p < len) {
        uint8_t l = buf[p++];
        if (l == 0) break;
        if ((l & 0xc0) == 0xc0) { p++; break; }   /* 压缩指针（罕见于 query 段） */
        if (p + l > len || w + l + 1 >= outsz) return -1;
        if (w) out[w++] = '.';
        memcpy(out + w, buf + p, l);
        w += l;
        p += l;
    }
    out[w] = 0;
    return p;
}

static void dns_task(void *arg)
{
    uint32_t self_be = (uint32_t)(uintptr_t)arg;

    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) { ESP_LOGE(TAG, "socket: %d", errno); vTaskDelete(NULL); }

    struct sockaddr_in sa = {
        .sin_family = AF_INET,
        .sin_port = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s, (void *)&sa, sizeof(sa)) < 0) {
        ESP_LOGE(TAG, "bind:53 failed: %d (占用?)", errno);
        close(s); vTaskDelete(NULL);
    }
    char ipbuf[16];
    esp_ip4addr_ntoa((const esp_ip4_addr_t *)&self_be, ipbuf, sizeof(ipbuf));
    ESP_LOGI(TAG, "DNS hijack listening on UDP:53, A -> %s", ipbuf);

    uint8_t buf[512];
    while (1) {
        struct sockaddr_in src;
        socklen_t sl = sizeof(src);
        int n = recvfrom(s, buf, sizeof(buf), 0, (void *)&src, &sl);
        if (n < (int)sizeof(dns_hdr_t) + 5) continue;

        dns_hdr_t *h = (dns_hdr_t *)buf;
        uint16_t qd = ntohs(h->qd);
        if (qd == 0) continue;

        char name[256];
        int qend = read_qname(buf, n, sizeof(dns_hdr_t), name, sizeof(name));
        if (qend < 0 || qend + 4 > n) continue;
        uint16_t qtype = (buf[qend] << 8) | buf[qend + 1];

        /* 构造回包：QR=1, AA=1, RA=1 */
        h->flags = htons(0x8580);
        h->an = htons((qtype == QTYPE_A) ? 1 : 0);
        h->ns = 0;
        h->ar = 0;

        int rlen = qend + 4;        /* 到 QCLASS 结束 */
        if (qtype == QTYPE_A) {
            uint8_t *w = buf + rlen;
            *w++ = 0xc0; *w++ = 0x0c;              /* name 指针回到问题 */
            *w++ = 0; *w++ = 1;                    /* TYPE  A */
            *w++ = 0; *w++ = 1;                    /* CLASS IN */
            *w++ = 0; *w++ = 0; *w++ = 0; *w++ = 60; /* TTL 60s */
            *w++ = 0; *w++ = 4;                    /* RDLENGTH */
            memcpy(w, &self_be, 4); w += 4;
            rlen = w - buf;
            ESP_LOGI(TAG, "A %s -> self", name);
        } else {
            /* AAAA / 其他：空应答，让客户端回退到 A */
            ESP_LOGI(TAG, "qt=%u %s -> empty", qtype, name);
        }

        sendto(s, buf, rlen, 0, (void *)&src, sl);
    }
}

void c1slim_dns_hijack_start(uint32_t self_ip_be)
{
    xTaskCreate(dns_task, "dns_hijack", 4096,
                (void *)(uintptr_t)self_ip_be, 5, NULL);
}
