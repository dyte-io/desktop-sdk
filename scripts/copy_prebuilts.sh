#!/bin/sh

set -eu

rm -f \
	third_party/libcwebrtc/*.so \
	third_party/libmobilecore/*.so \
	third_party/libmobilecore/*.h

LIBCWEBRTC_PATH="$HOME/libwebrtc/src/out/Release"
LIBMOBILECORE_PATH="$HOME/native/mobile-core/shared/build/bin/linuxX64/releaseShared"

copy_and_split() {
	base="$(basename "$2")"

	cp "$2" "$1/"

	objcopy --only-keep-debug "$1/$base" "$1/$base.dbg"
	objcopy --strip-unneeded "$1/$base"
	objcopy --add-gnu-debuglink="$1/$base.dbg" "$1/$base"
}

cp "$LIBMOBILECORE_PATH/libmobilecore_api.h" third_party/libmobilecore

copy_and_split third_party/libcwebrtc "$LIBCWEBRTC_PATH/libcwebrtc.so"
copy_and_split third_party/libmobilecore "$LIBMOBILECORE_PATH/libmobilecore.so"
