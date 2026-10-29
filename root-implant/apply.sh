#!/bin/sh
# 装 C1 Slim 永久 root
# 做法：取消 /etc/init.d/S90usb 第 34 行的 "#/etc/init.d/usb/adb" 注释。
# 幂等：已装过再跑不会重复植入。
set -e

TARGET=/etc/init.d/S90usb
BAK=/storage/rootbak
mkdir -p $BAK

if [ ! -f "$TARGET" ]; then
    echo "ERR: $TARGET not found. 不是 C1 Slim?" >&2
    exit 1
fi

if [ ! -f "$BAK/S90usb.orig" ]; then
    cp "$TARGET" "$BAK/S90usb.orig"
    echo "[+] backup -> $BAK/S90usb.orig"
else
    echo "[=] backup already at $BAK/S90usb.orig (skip)"
fi

# 探查当前状态
if grep -q '^[[:space:]]*/etc/init.d/usb/adb[[:space:]]*\$1' "$TARGET"; then
    echo "[=] already patched (adb boots)"
else
    echo "[*] mount rw /"
    mount -o remount,rw /
    echo "[*] sed patch $TARGET"
    sed -i 's|^\t#/etc/init.d/usb/adb\t\$1|\t/etc/init.d/usb/adb\t\$1|' "$TARGET"
    if ! grep -q '^[[:space:]]*/etc/init.d/usb/adb[[:space:]]*\$1' "$TARGET"; then
        echo "ERR: sed 没匹配到预期行。手工检查 $TARGET" >&2
        mount -o remount,ro /
        exit 2
    fi
    sync
    mount -o remount,ro /
    echo "[+] patched. 重启后 adbd 会随 S90usb 自启"
fi

echo "---"
echo "下一步： adb reboot ；等 15-20s ； adb devices 应直接看到 root 设备"
