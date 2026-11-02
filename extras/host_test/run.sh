#!/usr/bin/env bash
# Build and run the host tests: timing engine, driver against a fake register file,
# and the algorithms on synthetic PPG.
#
#   extras/host_test/run.sh
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$DIR/../../src"
"${CXX:-c++}" -std=c++11 -O1 -Wall -Wextra -Wno-unused-parameter \
    -I "$DIR/stub" -I "$SRC" \
    "$DIR/host_test.cpp" "$DIR/stub/stubs.cpp" \
    "$SRC/protocentral_afe44xx.cpp" "$SRC/protocentral_afe44xx_legacy.cpp" "$SRC/Protocentral_AFE44xx_Algorithms.cpp" \
    -o "$DIR/host_test"
"$DIR/host_test"
