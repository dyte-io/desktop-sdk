#!/bin/sh

exec clang++ \
    mobile.cc \
    -o mobile.cpython-312-x86_64-linux-gnu.so \
    -L"$PWD/releaseShared" \
    -Wl,-R"$PWD/releaseShared" \
    -fsanitize=address \
    -shared-libasan \
    -shared \
    -fPIC \
    $(pkg-config --libs --cflags python3-embed) \
    -lc_malloc_debug \
    -lcwebrtc \
    -lmobilecore \
    -Wall \
    -Wextra \
    -g3
