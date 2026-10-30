#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the AMB-1 host simulator (the whole app on this machine) into build/host/amb_host.
# Same sources as the firmware, -ffp-contract=off like the device (no fused multiply-add); host/stems.c the UI a
# second time, on its own, for the film's stems.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
mkdir -p build/gen build/host
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
python3 tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
# (-D_DEFAULT_SOURCE: on Linux, glibc's -std=c99 hides POSIX's clock_gettime and popen without it; macOS ignores it)
$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -D_DEFAULT_SOURCE \
    -DEN_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen -o build/host/amb_host host/amb_host.c host/stems.c \
    firmware/src/dsp/amb.c -lm
echo "host: build/host/amb_host"
