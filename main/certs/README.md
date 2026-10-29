# 自签证书

C1Max README 的原话：**设备不校验证书**。所以这里给的是一张随手签的 CN=api.mpen.com.cn
cert，`https_mitm.c` 构建时 embed 进固件。

需要重新生成（比如要换 CN、换有效期）：

```bash
openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
  -subj "/C=CN/O=C1SlimBypass/CN=api.mpen.com.cn" \
  -addext "subjectAltName=DNS:api.mpen.com.cn,DNS:*.mpen.com.cn,DNS:*.com.cn,IP:192.168.4.1" \
  -keyout server_key.pem -out server_cert.pem
```

如果真机上事后发现它居然校证书了（不太可能，但有的话固件会在 UART 打
`tls_failed`），再从 `/etc/ssl/certs` 抓出真正的 chain 并自签对应的 intermediate。
