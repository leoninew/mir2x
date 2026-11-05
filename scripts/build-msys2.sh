#!/usr/bin/env bash
set -euo pipefail

if [[ ! -r /etc/msystem ]]; then
    echo "Run this script with MSYS2 bash (UCRT64)." >&2
    exit 1
fi

export MSYSTEM=UCRT64
. /etc/msystem
export PATH="${MINGW_PREFIX}/bin:/usr/bin:$PATH"

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
export CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
if [[ ! ${VCPKG_BINARY_SOURCES+x} ]]; then
    mkdir -p .vcpkg/binary-cache
    export VCPKG_BINARY_SOURCES="clear;files,$(cygpath -m "$PWD/.vcpkg/binary-cache"),readwrite"
fi

exec python3 ./build.py \
    --build-dir=. \
    --triplet=x64-mingw-static \
    --host-triplet=x64-mingw-static \
    --c-compiler=gcc \
    --cxx-compiler=g++ \
    --parallel="${MIR2X_BUILD_JOBS:-8}" \
    "$@"
