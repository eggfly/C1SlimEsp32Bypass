# C1 Slim 永久 Root 植入

> 对应参考：`../../C1-Slim/C1Max-Capture/root-implant/` 是 C1 Max 的做法。
> C1 Slim 走的是另一套 init（Buildroot + busybox sysv），所以植入点不一样，
> 这里是 Slim 专版，已在真机验证开机自动 adbd。

## 攻击链回顾

1. ESP32-S3 SoftAP → DNS 全量劫持 → HTTPS MITM 把 `*.mpen.com.cn/...?action=adbAdmit`
   响应改成 `success:true`（本仓库的 `main/` 固件）
2. 设备上「关于本机」→ 连点系统版本进隐藏菜单 → 点 ADB 开关
3. 设备应用调一次 `/etc/init.d/usb/adb start` 把 adbd 起起来 → 这时 host 侧
   `adb devices` 立刻能看到 MagicPen-xxxxxx (root)
4. **但是**：出厂 `/etc/init.d/S90usb` 把开机启 adb 的那一行**注释掉**了，
   所以重启就没了。永久化 = 把这行注释去掉

## 装 / 恢复

进了 adb root 之后：

```bash
# 装
adb push apply.sh /tmp/
adb shell 'sh /tmp/apply.sh'

# 恢复（取消永久化）
adb push restore.sh /tmp/
adb shell 'sh /tmp/restore.sh'
```

然后 `adb reboot`，等 15–20 秒，`adb devices` 应该直接看到 root 设备。

## 植入点

- 改：`/etc/init.d/S90usb` 第 34 行 `#/etc/init.d/usb/adb $1` → 去掉 `#`
- 备份：`/storage/rootbak/S90usb.orig`（持久分区 `mmcblk0p8`，重刷/OTA 后用于回滚）
- `/` 是 ext4 ro，需要 `mount -o remount,rw /` 再改，改完 `mount -o remount,ro /`

OTA 升级会覆盖系统分区 → `/etc/init.d/S90usb` 回滚、root 丢失。
重跑 ESP32 绕过拿到 shell 后，再 `apply.sh` 一次即可。
