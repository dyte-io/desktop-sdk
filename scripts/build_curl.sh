#!/bin/sh

set -eu

cd third_party/curl

rm -rf curl-8.8.0
tar xf curl-8.8.0.tar.xz

cd curl-8.8.0

./configure \
	--prefix=/ \
	--enable-ipv6 \
	--enable-unix-sockets \
	--enable-symbol-hiding \
	--enable-http \
	--enable-websockets \
	--with-openssl \
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
make DESTDIR="$PWD/root" install

cp root/lib/libcurl.so.4 ..
objcopy --strip-unneeded ../libcurl.so.4
