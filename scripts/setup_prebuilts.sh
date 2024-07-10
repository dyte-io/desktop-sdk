#!/usr/bin/env sh

set -eu

rm -rf build_ext

cmake -B build_ext \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_SKIP_BUILD_RPATH=ON \
	-DPYTHON_EXECUTABLE="$(command -v python3)"

cmake --build build_ext

rm -rf prebuilts prebuilts-debuginfo
mkdir prebuilts prebuilts-debuginfo

cp \
	build_ext/*.so \
	third_party/curl/libcurl.so.4 \
	third_party/libcwebrtc/libcwebrtc.so \
	third_party/libmobilecore/libmobilecore.so \
	prebuilts/
