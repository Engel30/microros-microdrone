#!/usr/bin/env bash
# Test su host del modulo motor_spinup (nessuna dipendenza ESP-IDF).
set -e
cd "$(dirname "$0")/.."
out=$(mktemp -d)
gcc -std=c11 -Wall -Wextra -Werror -I include -I ../common/include \
    test/test_spinup.c motor_spinup.c -lm -o "$out/test_spinup"
"$out/test_spinup"
