---
layout: post
title: How to create CA(x.509)
category: Crypto
comments: true
---

# How to create CA(x.509)

This guide shows how to create an X.509 root CA, an intermediate CA and signed server certificates with OpenSSL. The generated PEM files can be embedded into the [TLS](TLS.md) configuration (`ServerCerts`, `CasCerts` and `ServerKey` in `TLS.json`).

References:

 * https://zhuanlan.zhihu.com/p/492475360 : how to create x.509
 * OpenSSL config example: `C:/Anaconda3/Library/ssl/openssl.cnf`

### How to create ROOT CA

```sh
mkdir astrust
cd astrust
mkdir -p CA/private newcerts
touch index.txt
echo 01020304050607 > serial
echo abcdefghijklmn > crlnumber
openssl req -x509 -newkey rsa:2048 -sha256 -days 7300 -nodes -keyout AS_Root_CA.key -out AS_Root_CA.crt -subj "/C=US/ST=TX/L=DAL/O=Security/OU=IT Department/CN=AS Root CA" -addext keyUsage=critical,cRLSign,keyCertSign,digitalSignature -addext basicConstraints=critical,CA:true,pathlen:3
```

### How to create Intermediate CA

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

### Use the certificates with TLS

Export the server certificate, the CA chain and the server private key as PEM files, place them next to `TLS.json` (for example in a `Cert/` folder) and reference them from the TLS configuration. The TLS generator embeds the PEM content as C strings, so no file system is needed on the target. See [TLS](TLS.md) for details.