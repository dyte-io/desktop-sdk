#!/bin/sh

set -eu

rm -f \
	third_party/libcwebrtc/*.so \
	third_party/libmobilecore/*.so \
	third_party/libmobilecore/*.h

LIBCWEBRTC_PATH="$HOME/libwebrtc/src/out/Release"
LIBMOBILECORE_PATH="$HOME/native/mobile-core/shared/build/bin/linuxX64/releaseShared"

cp "$LIBMOBILECORE_PATH/libmobilecore_api.h" third_party/libmobilecore
cp "$LIBMOBILECORE_PATH/libmobilecore.so" third_party/libmobilecore
cp "$LIBCWEBRTC_PATH/libcwebrtc.so" third_party/libcwebrtc
