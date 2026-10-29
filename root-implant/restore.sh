#!/bin/sh
# 恢复 C1 Slim 系统分区到植入前
# 优先用 /storage/rootbak/S90usb.orig，没有的话手动加回注释。
set -e

TARGET=/etc/init.d/S90usb
BAK=/storage/rootbak/S90usb.orig

mount -o remount,rw /

if [ -f "$BAK" ]; then
    cp "$BAK" "$TARGET"
    echo "[+] restored from $BAK"
else
    # Fallback：把 adb $1 行加回注释（匹配第 34 行的 start 分支）
    # 这里只处理 start 分支那一处（stop/allstart 不动）
    awk 'NR==34 && /^\t\/etc\/init.d\/usb\/adb\t\$1/ {print "\t#" substr($0,2); next} 1' \
        "$TARGET" > "$TARGET.new"
    mv "$TARGET.new" "$TARGET"
    chmod 755 "$TARGET"
    echo "[+] 没找到备份，用 awk 加回了注释"
fi

sync
mount -o remount,ro /
echo "[+] done. 下次重启就不再自动起 adbd。当前已跑的 adbd 不受影响。"
