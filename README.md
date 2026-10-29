# C1SlimEsp32Bypass

把 C1 Max 已跑通的 adb 绕过链（见 `../C1Max-Capture/root-implant/README.md` 和
`../C1Max-Capture/capture/mitm_addon.py`）压到一块 **ESP32-S3 N8R8** 上。

设备连 ESP32 发射的 SoftAP → ESP32 做 DNS 全量劫持 → 自己起 HTTP/HTTPS →
对 `?action=adbAdmit` 的响应改成 `success:true` → C1 Slim 调 `enable_adb.sh true`
→ `adb shell` = root。

C1Max README 原话：**设备不校验证书**，所以 ESP32 用一张自签 cert 就够了，
不需要 CA 植入。

## 攻击链（与 C1 Max 对齐）

1. **WiFi** —— ESP32 开 `C1SLIM-LAB / c1slimlab2026` SoftAP
2. **DNS** —— UDP:53 全量劫持（含 `api.mpen.com.cn`、CDN、captive 探测），都回 `192.168.4.1`
3. **TLS** —— HTTPS 443/8443 用 `main/certs/server_cert.pem` 自签 cert 应答
4. **MITM 响应** —— 任意包含 `adbAdmit` / `adb_allow` / `adbAllow` 的 URL 或 body，
   回这段 JSON（和 `mitm_addon.py` 对齐，字段同时写 true 以兼容各种 key 名）：

   ```json
   {"errorCode":"200","errorMsg":"Success","data":{
      "success":true,"adb_allow":true,"adbAllow":true,
      "allow":true,"adb":true,"adbAdmit":true,"enabled":true},
    "code":200,"status":"ok","result":"success"}
   ```

5. **其他接口** —— 一律 `errorCode:200` 空成功，设备不会抛网络错，自然流程继续到隐藏菜单
6. **串口** —— 每一笔 HTTP/HTTPS 命中都在 UART0 / USB-JTAG CDC 打印，`adbAdmit HIT`
   出现 = 响应已改

## 目录

```
C1SlimEsp32Bypass/
├── CMakeLists.txt
├── sdkconfig.defaults        # ESP32-S3 + 8MB flash + Octal PSRAM(N8R8)
└── main/
    ├── CMakeLists.txt
    ├── main.c                # app_main：起 AP → DNS → MITM
    ├── wifi_ap.{c,h}         # SoftAP
    ├── dns_hijack.{c,h}      # 捕获式 DNS：所有 A -> 自己
    ├── https_mitm.{c,h}      # 四条监听 (HTTP 80/8080, HTTPS 443/8443)
    └── certs/
        ├── server_cert.pem   # 自签 cert（embed 进固件）
        ├── server_key.pem
        └── README.md
```

## 构建 & 烧录（宿主真机的终端）

需要 ESP-IDF **v5.1+**（带 `esp_https_server` 和 `httpd_uri_match_wildcard`）。

```bash
. $IDF_PATH/export.sh           # or: get_idf
cd C1SlimEsp32Bypass
idf.py set-target esp32s3
idf.py build

# N8R8 开发板一般走板载 USB-OTG（内置 USB-CDC/JTAG）或 CP2102/CH343。
# 第一次插上，先看口号：
ls /dev/cu.usb* /dev/cu.wchusb*
idf.py -p /dev/cu.usbmodem14101 flash monitor
```

退 monitor：`Ctrl-]`。

## 用

1. 烧完 ESP32 保持接着 USB（当 serial log 用）
2. 把 **C1 Slim** 从设置里连 WiFi：
   - SSID: `C1SLIM-LAB`
   - 密码: `12345678`
3. 设备上**关于本机** → 连点系统版本进入隐藏调试菜单 → 打开 ADB
4. 盯 ESP32 的 serial log：
   ```
   GET api.mpen.com.cn/v1/pens/XXXX?action=adbAdmit  host=api.mpen.com.cn  <-- adbAdmit HIT
   ```
5. macOS 上 `adb devices` 应该看到设备；`adb shell` 就是 root
6. **立刻** 把永久化植入打上去（Slim 版，已真机验证）：
   ```bash
   adb push root-implant/apply.sh /tmp/
   adb shell 'sh /tmp/apply.sh'
   adb reboot && sleep 20 && adb devices    # 应直接看到 root 设备
   ```
   详见 [`root-implant/README.md`](root-implant/README.md)。核心是 patch
   `/etc/init.d/S90usb` 第 34 行，把 `#/etc/init.d/usb/adb $1` 的注释去掉，
   备份进 `/storage/rootbak/`。Slim 系统是 Buildroot + busybox sysv，和 Max
   的 Android init / usbconfig.sh 走的是**完全不同**的路径。

植入完以后 ESP32 不再是必需品；设备重启后 adbd 永远在。

## 提高命中率的技巧（都已经做了）

- **DNS 全量劫持**：不管查什么域名都回 `192.168.4.1`，确保设备发往 CDN / backup
  域的请求也落到我们这边
- **HTTP + HTTPS 双栈 + 多端口**：80 / 443 / 8080 / 8443 都监听
- **URI 和 body 双匹配**：子串匹配 `adbAdmit` / `adb_allow` / `adbAllow` 不分大小写
- **响应字段冗余**：`success / adb_allow / adbAllow / allow / adb / adbAdmit / enabled`
  同时为 true，覆盖所有可能的 key 名
- **非目标接口兜底 200**：防止设备因 `/v1/foo` 之类返回 404 而拒绝继续
- **captive 探测放行 204**：避免手机 OS 风格的"无网络"弹窗（设备如果有的话）
- **TLS Alert 不断握手**：全部走 `MBEDTLS_SSL_IN_CONTENT_LEN=8192` + dynamic buffer

## 调试清单

| 现象 | 排查 |
| --- | --- |
| 设备连不上 AP | `wifi_ap.c` 里换 OPEN 认证试试；或 ch=1/6/11 换 |
| 连上但 UART 没任何 HTTP log | DNS 没中招：UART 应有 `A api.mpen.com.cn -> self` |
| 看到 `tls_failed` | 设备在校验证书。按 C1Max HANDOFF，把真 CA 塞进 `certs/` 并重签 |
| `adbAdmit HIT` 出现但 adbd 没起 | 设备 enable_adb.sh 可能看别的字段 —— `menuconfig` 之前先在 UART 看 request body 猜 key，再追字段 |

## 和 C1Max 的差异

| 项 | C1Max | C1Slim（本固件） |
| --- | --- | --- |
| MITM 位置 | Ubuntu VM + USB WiFi 网卡 + mitmproxy | 一块 ESP32-S3 板子 |
| HTTPS 解密 | mitmproxy 自签 CA | 自签 cert（设备不校验） |
| 响应改写 | `mitm_addon.py` 的 Python 对象改 | C 字符串直出 |
| init 系统 | Android init + property + usbconfig.sh | Buildroot busybox sysv + `/etc/init.d/S90usb` |
| 永久化点 | 改 `usbconfig.sh`（mtp 分支也挂 adb） | 改 `S90usb` 取消 adb 行注释 |
| 备份位置 | `/storage/rootbak/` | `/storage/rootbak/` (`mmcblk0p8`) |
