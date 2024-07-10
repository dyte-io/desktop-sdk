#!/bin/sh

set -eu

docker run \
	-v ./third_party/curl:/curl \
	--rm -i quay.io/pypa/manylinux_2_28_x86_64:2024.07.02-0 \
	sh - <<EOF
set -euo pipefail

dnf install -y vim patch

mkdir /build && cd /build

curl -LO https://curl.se/ca/cacert.pem
curl -L https://curl.se/ca/cacert.pem.sha256 | sha256sum -c

printf '\0' >> cacert.pem
xxd -i cacert.pem > cacert.h

curl -LO https://github.com/Mbed-TLS/mbedtls/releases/download/v3.6.0/mbedtls-3.6.0.tar.bz2
echo '3ecf94fcfdaacafb757786a01b7538a61750ebd85c4b024f56ff8ba1490fcd38 mbedtls-3.6.0.tar.bz2' | sha256sum -c

curl -LO https://github.com/curl/curl/releases/download/curl-8_8_0/curl-8.8.0.tar.xz
echo '0f58bb95fc330c8a46eeb3df5701b0d90c9d9bfcc42bd1cd08791d12551d4400 curl-8.8.0.tar.xz' | sha256sum -c

tar xf mbedtls-3.6.0.tar.bz2
tar xf curl-8.8.0.tar.xz

(
	cd mbedtls-3.6.0
	CFLAGS="-fPIC" make DESTDIR=/opt/mbedtls install
)

cd curl-8.8.0
cp ../cacert.h lib/

patch -p1 < /curl/embed-cacert.patch

./configure \
	--prefix=/ \
	--enable-ipv6 \
	--enable-unix-sockets \
	--enable-symbol-hiding \
	--enable-http \
	--enable-websockets \
	--with-pic \
	--with-mbedtls=/opt/mbedtls \
	--without-ca-bundle \
	--without-ca-path \
	--without-hyper \
	--without-librtmp \
	--without-libpsl \
	--without-libidn2 \
	--without-openssl-quic \
	--without-nghttp2 \
	--without-ngtcp2 \
	--without-nghttp3 \
	--without-brotli \
	--without-zstd \
	--disable-docs \
	--disable-ares \
	--disable-ech \
	--disable-ftp \
	--disable-file \
	--disable-ldap \
	--disable-ldaps \
	--disable-rtsp \
	--disable-dict \
	--disable-telnet \
	--disable-tftp \
	--disable-pop3 \
	--disable-imap \
	--disable-smb \
	--disable-smtp \
	--disable-gopher \
	--disable-mqtt \
	--disable-sspi \
	--disable-kerberos-auth \
	--disable-negotiate-auth \
	--disable-aws \
	--disable-ntlm \
	--disable-alt-svc

make
make DESTDIR="\$PWD/root" install

cp root/lib/libcurl.so.4 /curl/
objcopy --strip-unneeded /curl/libcurl.so.4
EOF
