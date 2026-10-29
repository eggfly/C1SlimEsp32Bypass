idf_component_register(
    SRCS
        "main.c"
        "wifi_ap.c"
        "dns_hijack.c"
        "https_mitm.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES
        "certs/server_cert.pem"
        "certs/server_key.pem"
    REQUIRES
        nvs_flash esp_wifi esp_netif esp_event esp_http_server esp_https_server
        lwip esp_timer driver
)
