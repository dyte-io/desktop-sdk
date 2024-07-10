#!/bin/sh

set -eu

docker run \
  -v ./third_party/curl:/curl \
  --rm -i quay.io/pypa/manylinux_2_28_x86_64:2024.07.02-0 \
  sh - <<EOF
set -euo pipefail

curl -LO https://www.openssl.org/source/openssl-3.0.14.tar.gz
echo 'eeca035d4dd4e84fc25846d952da6297484afa0650a6f84c682e39df3a4123ca openssl-3.0.14.tar.gz' | sha256sum -c

curl -LO https://github.com/curl/curl/releases/download/curl-8_8_0/curl-8.8.0.tar.xz
echo '0f58bb95fc330c8a46eeb3df5701b0d90c9d9bfcc42bd1cd08791d12551d4400 curl-8.8.0.tar.xz' | sha256sum -c

dnf -y install perl-IPC-Cmd perl-Pod-Html

tar xf openssl-3.0.14.tar.gz
tar xf curl-8.8.0.tar.xz

(
	cd openssl-3.0.14

	./config \
		--prefix=/opt/openssl \
		--libdir=lib

	make
	make install
)

cd curl-8.8.0

./configure \
	--prefix=/ \
	--enable-ipv6 \
	--enable-unix-sockets \
	--enable-symbol-hiding \
	--enable-http \
	--enable-websockets \
	--with-openssl=/opt/openssl \
	--with-pic \
	--with-ca-fallback \
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
	--disable-httpsrr \
	--disable-ech \
	--disable-ftp \
	--disable-file \
	--disable-ldap \
	--disable-ldaps \
	--disable-rtsp \
	--disable-proxy \
	--disable-dict \
	--disable-telnet \
	--disable-tftp \
	--disable-pop3 \
	--disable-imap \
	--disable-smb \
	--disable-smtp \
	--disable-gopher \
	--disable-mqtt \
	--disable-verbose \
	--disable-sspi \
	--disable-kerberos-auth \
	--disable-negotiate-auth \
	--disable-aws \
	--disable-ntlm \
	--disable-tls-srp \
	--disable-socketpair \
	--disable-alt-svc \
	--disable-hsts

make
make DESTDIR="\$PWD/root" install

cp root/lib/libcurl.so.4 /curl/
objcopy --strip-unneeded /curl/libcurl.so.4
EOF
