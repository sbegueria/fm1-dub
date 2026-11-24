/* SPDX-License-Identifier: GPL-3.0-only */
/* (1.5.0, format 37; STORAGE-1.5.0.md Decisions 2 and 3) a name as the device takes it: a piece's or a set's, from a
 * container. Letters, digits, space and a few marks (the fonts have them, the reader says them), not blank in front,
 * up to NAME_CHARS of them (a backstop), and no wider on the screen than NAME_PX: the widest name the header's tag holds
 * with the phrase beside it whole (its word, the widest of FULL BREAK GROOVE THIN INTRO OUTRO, and its eight cells at
 * the smaller size), the unsaved disk shown and the battery not (Decisions 3: the battery icon steps aside while a name
 * wider than NAME_PX_BATT needs its room): 232 - 15 - 6 = 211 for the phrase's end, 4 + 12 + 10 before it, GROOVE
 * 38 + 6, the cells 54 (ui.c's draw_header and hdr_phrase; the host's "namecheck" works it out again from them).
 * The width is NAME_W's: the header's font (FONT_B) by character, the digits at the widest digit's (a build with libraqm gives the digits one width, one without their own: the widest holds for both).
 * tools/gen_name_w.py makes the table from the font and checks its twins' copies (tools/dub1_container.py,
 * web/dub1container.js): a name the web or a script accepts is never cut here. Pure: no other file needed (the
 * names' test, tests/host/name_test.c, includes it alone) */
#pragma once
#include <stdint.h>

#define NAME_CHARS 12                            /* characters at most */
#define NAME_PX 87                               /* the widest, in pixels (tools/gen_name_w.py's) */
#define NAME_PX_BATT 68                          /* the widest with the battery shown too (19 px: the icon and its gap) */
/* FONT_B's advances, ' ' to '~', the digits tabular (tools/gen_name_w.py) */
static const uint8_t NAME_W[95] = {3, 5, 5, 10, 8, 12, 10, 3, 5, 5, 6, 7, 4, 6, 4, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 4, 7, 7, 7, 7,
    12, 9, 8, 8, 8, 8, 7, 8, 8, 4, 8, 8, 7, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8, 12, 8, 8, 7, 6, 7, 6, 7, 7, 3, 7, 8, 7, 8, 7,
    5, 7, 7, 4, 4, 7, 3, 11, 7, 8, 8, 8, 5, 7, 5, 7, 7, 10, 7, 7, 6, 5, 3, 5, 8};

static int name_char(char c)                     /* one of a name's characters: 1 */
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' ||
           c == '.' || c == '\'' || c == '&' || c == '!' || c == '?' || c == '/' || c == '+' || c == '#' || c == '(' ||
           c == ')' || c == ':' || c == ',' || c == '_';
}

/* a name field (n bytes: the name, then NULs to its end) as the device takes it: 1. Empty is good (none: the recipe's
 * name); *px its width (0 if not good) */
static int name_fits(const char *f, uint32_t n, uint32_t *px)
{
    uint32_t i, w = 0, end = 0;
    *px = 0;
    for (i = 0; i < n; i++) {
        const char c = f[i];
        if (!c) {
            end = 1;
        } else if (end || i >= NAME_CHARS || !name_char(c)) {
            return 0;
        } else {
            w += NAME_W[(uint8_t)c - 32u];
        }
    }
    if (f[0] == ' ' || w > NAME_PX)              /* (not blank in front; no wider than the tag's room) */
        return 0;
    *px = w;
    return 1;
}
