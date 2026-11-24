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
python3 tools/gen_sets.py sets build/gen/first_sets.h >/dev/null   # (1.5.0: the sets the FM-1 comes with)
# (-D_DEFAULT_SOURCE: on Linux, glibc's -std=c99 hides POSIX's clock_gettime and popen without it; macOS ignores it)
# (1.5.0) twice: amb_host as the release (DUB1_CUES 0: the cues kept, none played or shown), amb_host_cues with them
# played (-DDUB1_CUES=1: the cue player's tests, so it doesn't rot; a scenario's "# (build: DUB1_CUES=1)" line picks it)
rm -f build/host/amb_host build/host/amb_host_cues   # (a failed one not left as it was)
for v in 0 1; do
    o=build/host/amb_host; [ $v = 1 ] && o=build/host/amb_host_cues
    $CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -D_DEFAULT_SOURCE \
        -DEN_HOST -DDUB1_CUES=$v -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen -o $o host/amb_host.c host/stems.c \
        firmware/src/dsp/amb.c -lm &
done
wait
[ -x build/host/amb_host ] && [ -x build/host/amb_host_cues ]
echo "host: build/host/amb_host, build/host/amb_host_cues"
