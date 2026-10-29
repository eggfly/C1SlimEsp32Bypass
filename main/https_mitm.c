/* HTTPS/HTTP MITM 服务器
 *
 * 核心复刻 C1Max-Capture/capture/mitm_addon.py 里的 adbAdmit 分支：
 *   if "adbAdmit" in q.pretty_url or b"adb_allow" in body[:2048].lower():
 *       obj["data"]["success"]    = True
 *       obj["data"]["adb_allow"]  = True
 *       obj["data"]["adbAllow"]   = True
 *       obj["data"]["allow"]      = True
 *
 * C1Max README：设备不校验证书 —— 所以自签 cert 可以直接用。
 *
 * 四条监听（80/443/8080/8443）配合 DNS 全量劫持 + 捕获门户放行，提高命中率。
 */

#include "https_mitm.h"
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "mitm";

/* 自签 cert / key —— 构建时 embed；详见 certs/README.md 生成命令 */
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");
extern const uint8_t server_cert_pem_end[]   asm("_binary_server_cert_pem_end");
extern const uint8_t server_key_pem_start[]  asm("_binary_server_key_pem_start");
extern const uint8_t server_key_pem_end[]    asm("_binary_server_key_pem_end");

/* ------------- 响应模板 ------------- */

/* adbAdmit 命中：所有变体 true，哪种拼写都认 */
static const char *RESP_ADMIT =
    "{\"errorCode\":\"200\",\"errorMsg\":\"Success\","
    "\"data\":{"
        "\"success\":true,"
        "\"adb_allow\":true,"
        "\"adbAllow\":true,"
        "\"allow\":true,"
        "\"adb\":true,"
        "\"adbAdmit\":true,"
        "\"enabled\":true"
    "},"
    "\"code\":200,\"status\":\"ok\",\"result\":\"success\"}";

/* 其他接口：空成功，避免设备侧抛网络错 */
static const char *RESP_OK =
    "{\"errorCode\":\"200\",\"errorMsg\":\"Success\",\"data\":{},"
    "\"code\":200,\"status\":\"ok\"}";

/* ------------- 工具 ------------- */

static int contains_ci(const char *hay, const char *needle)
{
    if (!hay || !needle) return 0;
    size_t nl = strlen(needle);
    for (const char *p = hay; *p; p++) {
        if (strncasecmp(p, needle, nl) == 0) return 1;
    }
    return 0;
}

/* 读完请求 body（最多 2 KiB），用于匹配 adb_allow 关键字 —— 和 mitm_addon.py 一致 */
static int peek_body(httpd_req_t *req, char *buf, int cap)
{
    int remaining = req->content_len;
    if (remaining <= 0) { buf[0] = 0; return 0; }
    if (remaining > cap - 1) remaining = cap - 1;
    int off = 0;
    while (off < remaining) {
        int r = httpd_req_recv(req, buf + off, remaining - off);
        if (r <= 0) break;
        off += r;
    }
    buf[off] = 0;
    return off;
}

/* ------------- 统一 handler ------------- */

static esp_err_t any_handler(httpd_req_t *req)
{
    char host[128] = {0};
    httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));
    const char *ua_hint = "";

    char body[2048];
    int blen = peek_body(req, body, sizeof(body));

    int hit = contains_ci(req->uri, "adbAdmit")
           || contains_ci(req->uri, "adb_allow")
           || contains_ci(req->uri, "adbAllow")
           || (blen > 0 && contains_ci(body, "adb_allow"))
           || (blen > 0 && contains_ci(body, "adbAdmit"));

    /* 捕获门户探测：各家 OS 用来判有没有联网 —— 原样放行 204 */
    int captive =
        contains_ci(req->uri, "generate_204") ||
        contains_ci(req->uri, "gen_204") ||
        contains_ci(req->uri, "connectivitycheck") ||
        contains_ci(req->uri, "hotspot-detect") ||
        contains_ci(req->uri, "ncsi.txt");

    ESP_LOGI(TAG, "%s %s%s  host=%s%s%s",
             (req->method == HTTP_GET) ? "GET"  :
             (req->method == HTTP_POST) ? "POST" :
             (req->method == HTTP_PUT) ? "PUT"  : "???",
             host, req->uri,
             host[0] ? "" : "(no host)",
             hit ? "  <-- adbAdmit HIT" : "",
             captive ? "  (captive probe)" : "");
    (void)ua_hint;

    if (captive) {
        httpd_resp_set_status(req, "204 No Content");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, NULL, 0);
    }

    /* 全部返回 JSON（API 期望）；非 API 页面的浏览器探测也无所谓 */
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Connection", "close");
    const char *payload = hit ? RESP_ADMIT : RESP_OK;
    return httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
}

/* 注册通配 URI：ESP-IDF 需要 wildcard_match 配合 */
static void register_wildcard(httpd_handle_t s)
{
    static const httpd_method_t methods[] = {
        HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE, HTTP_PATCH, HTTP_HEAD, HTTP_OPTIONS,
    };
    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
        httpd_uri_t u = {
            .uri      = "/*",
            .method   = methods[i],
            .handler  = any_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(s, &u);
    }
}

/* ------------- 启动四条监听 ------------- */

static void start_http(uint16_t port)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port       = port;
    cfg.ctrl_port         = 32768 + port;   /* 多 server 要不同控制端口 */
    cfg.uri_match_fn      = httpd_uri_match_wildcard;
    cfg.max_uri_handlers  = 10;
    cfg.max_open_sockets  = 3;              /* 省 LwIP socket */
    cfg.lru_purge_enable  = true;
    cfg.stack_size        = 8192;

    httpd_handle_t s = NULL;
    esp_err_t err = httpd_start(&s, &cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "HTTP :%u fail %s", port, esp_err_to_name(err)); return; }
    register_wildcard(s);
    ESP_LOGI(TAG, "HTTP  listening on :%u", port);
}

static void start_https(uint16_t port)
{
    httpd_ssl_config_t cfg = HTTPD_SSL_CONFIG_DEFAULT();
    cfg.servercert       = server_cert_pem_start;
    cfg.servercert_len   = server_cert_pem_end - server_cert_pem_start;
    cfg.prvtkey_pem      = server_key_pem_start;
    cfg.prvtkey_len      = server_key_pem_end  - server_key_pem_start;
    cfg.port_secure      = port;
    cfg.port_insecure    = 0;                       /* 别开，上面已经单独 HTTP 了 */
    cfg.httpd.ctrl_port  = 32768 + port;
    cfg.httpd.uri_match_fn      = httpd_uri_match_wildcard;
    cfg.httpd.max_uri_handlers  = 10;
    cfg.httpd.max_open_sockets  = 3;        /* 省 LwIP socket */
    cfg.httpd.lru_purge_enable  = true;
    cfg.httpd.stack_size        = 10240;

    httpd_handle_t s = NULL;
    esp_err_t err = httpd_ssl_start(&s, &cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "HTTPS :%u fail %s", port, esp_err_to_name(err)); return; }
    register_wildcard(s);
    ESP_LOGI(TAG, "HTTPS listening on :%u", port);
}

void c1slim_https_mitm_start(void)
{
    /* 只开 80 + 443。LwIP socket 池有限，别浪费 */
    start_http(80);
    start_https(443);
}
