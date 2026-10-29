# C1SlimEsp32Bypass — ESP32-S3 N8R8 固件
# 对 C1 Slim 做 WiFi SoftAP + DNS 劫持 + HTTPS MITM，把 api.mpen.com.cn
# 的 adbAdmit 响应改成 success:true，走 C1Max 已验证的那条绕过链。
#
# 参考 ../C1Max-Capture/capture/mitm_addon.py 与 ../C1Max-Capture/root-implant/README.md
# （设备不校验证书，所以自签就够）。

cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(c1slim_esp32_bypass)
