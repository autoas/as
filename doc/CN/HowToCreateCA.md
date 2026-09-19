---
layout: post
title: 如何创建 CA(x.509)
category: Crypto
comments: true
---

# 如何创建 CA(x.509)

本文介绍如何使用 OpenSSL 创建 X.509 根 CA、中间 CA 以及签发的服务器证书。生成的 PEM 文件可嵌入 [TLS](TLS.md) 配置（`TLS.json` 中的 `ServerCerts`、`CasCerts` 和 `ServerKey` 字段）。

参考资料：

 * https://zhuanlan.zhihu.com/p/492475360 ：如何创建 x.509
 * OpenSSL 配置示例：`C:/Anaconda3/Library/ssl/openssl.cnf`

### 如何创建根 CA（ROOT CA）

```sh
mkdir astrust
cd astrust
mkdir -p CA/private newcerts
touch index.txt
echo 01020304050607 > serial
echo abcdefghijklmn > crlnumber
openssl req -x509 -newkey rsa:2048 -sha256 -days 7300 -nodes -keyout AS_Root_CA.key -out AS_Root_CA.crt -subj "/C=US/ST=TX/L=DAL/O=Security/OU=IT Department/CN=AS Root CA" -addext keyUsage=critical,cRLSign,keyCertSign,digitalSignature -addext basicConstraints=critical,CA:true,pathlen:3
```

### 如何创建中间 CA（Intermediate CA）

```sh
mkdir astrust-rsa-ica1
cd astrust-rsa-ica1
mkdir -pv CA/private newcerts
touch index.txt
echo 00 > serial
echo 00 > crlnumber

openssl genrsa -out AS_RSA_ICA1.key 2048

openssl req -new -sha256 -key AS_RSA_ICA1.key -out AS_RSA_ICA1.csr -subj="/C=US/O=Security/CN=AS Secure RSA ICA1"

openssl req -text -noout -in AS_RSA_ICA1.csr

openssl ca -days 1825 -in AS_RSA_ICA1.csr -out AS_RSA_ICA1.crt -cert ../AS_Root_CA.crt -keyfile ../AS_Root_CA.key -create_serial -policy policy_anything

openssl x509 -text -noout -in AS_RSA_ICA1.crt

openssl x509 -in AS_RSA_ICA1.crt -outform der -out AS_RSA_ICA1.der

openssl x509 -in AS_Root_CA.crt -outform der -out AS_Root_CA.der

openssl verify -trusted ../AS_Root_CA.crt AS_RSA_ICA1.crt
```

### 在 TLS 中使用证书

把服务器证书、CA 证书链和服务器私钥导出为 PEM 文件，放在 `TLS.json` 旁边（例如 `Cert/` 目录），并在 TLS 配置中引用它们。TLS 生成器会把 PEM 内容以C 字符串形式嵌入，目标机无需文件系统。详见 [TLS](TLS.md)。
