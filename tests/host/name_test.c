/* SPDX-License-Identifier: GPL-3.0-only */
/* (1.5.0, STORAGE-1.5.0.md Decisions 2) firmware/src/app/names.h alone: a few names by hand, then (names) each line of
 * stdin as a name field: its width if the device takes it, else "no", for the twins' test (tests/host/container_twins.sh:
 * the same answers from tools/dub1_container.py and web/dub1container.js).
 *   cc -o name_test tests/host/name_test.c && ./name_test [names < LINES] */
#include <stdio.h>
#include <string.h>
#include "../../firmware/src/app/names.h"

static int fits(const char *s)                   /* a C string as a 16-byte field: its width, or -1 */
{
    char f[16];
    uint32_t px;
    if (strlen(s) > sizeof f)
        return -1;
    memset(f, 0, sizeof f);
    memcpy(f, s, strlen(s));
    return name_fits(f, sizeof f, &px) ? (int)px : -1;
}

int main(int argc, char **argv)
{
    int bad = 0;
    if (argc == 2 && !strcmp(argv[1], "names")) {
        static char ln[4096];
        while (fgets(ln, sizeof ln, stdin)) {
            const size_t n = strcspn(ln, "\n");
            char f[16];
            uint32_t px;
            if (n > sizeof f) {
                puts("no");
                continue;
            }
            memset(f, 0, sizeof f);
            memcpy(f, ln, n);
            if (name_fits(f, sizeof f, &px))
                printf("%u\n", px);
            else
                puts("no");
        }
        return 0;
    }
#define CHECK(c, what) do { if (!(c)) { printf("FAIL %s\n", what); bad++; } } while (0)
    CHECK(fits("") == 0, "empty: none, good");
    CHECK(fits("Night Drive") == 66, "Night Drive, 66 px");
    CHECK(fits("Winter Set") > 0, "Winter Set");
    CHECK(fits("Dub Chamber") == 79 && fits("Echospace 12") == 85, "Dub Chamber 79 px, Echospace 12 85 (Decisions 3)");
    CHECK(fits("Woodward Dub") < 0, "Woodward Dub: 92 px, wider than the tag holds");
    CHECK(fits("WWWWWWWW") < 0 && fits("WWWWWWW") == 84, "W: 12 px each");
    CHECK(fits("iiiiiiiiiiiii") < 0 && fits("iiiiiiiiiiii") == 48, "13 characters: no, however narrow");
    CHECK(fits(" Fog") < 0 && fits("Fog ") > 0, "not blank in front");
    CHECK(fits("Fog\xe9") < 0 && fits("Fog*") < 0 && fits("a;b") < 0, "only the letters, numbers and marks");
    CHECK(fits("1111111") == 56 && fits("0") == fits("1"), "the digits tabular, the widest's");
    {
        char f[16] = "Fog";
        uint32_t px;
        f[5] = 'x';                              /* (a character after the NULs) */
        CHECK(!name_fits(f, sizeof f, &px), "nothing after the NULs");
    }
    printf(bad ? "names: %d FAILED\n" : "names: ok\n", bad);
    return bad != 0;
}
