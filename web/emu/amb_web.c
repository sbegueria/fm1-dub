/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1 in the browser: the host simulator (host/amb_host.c: the whole app, UI and instrument,
 * against a simulated FM-1) compiled to WebAssembly and run in an AudioWorklet (from X0X's). The worklet asks
 * for audio; producing it advances the device's clock one millisecond at a time, which runs the
 * engine every 256 samples (the I2S half buffer) and the UI every 16 ms, as on the FM-1. The page
 * sends input, and reads the screen, the lights and the saved objects back through these exports.
 *
 *   web/emu/build.sh   ->  build/emu/amb.wasm (+ the page) */
#include <stdint.h>

static void web_audio(const int32_t *blk, uint32_t n);
#include "../../host/amb_host.c"

#undef __attribute__

/* output ring, stereo float, filled by run_ms, drained by web_render */
#define RING 4096u
static float ring[RING * 2];
static uint32_t ring_w, ring_r;
static void web_audio(const int32_t *blk, uint32_t n)
{
    uint32_t k;
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_w++ % RING) * 2u;
        ring[i] = (float)blk[2 * k] / 8388608.0f;
        ring[i + 1] = (float)blk[2 * k + 1] / 8388608.0f;
    }
}

static float out_l[1024], out_r[1024];
__attribute__((used, visibility("default"))) float *web_out_l(void) { return out_l; }
__attribute__((used, visibility("default"))) float *web_out_r(void) { return out_r; }

/* n frames (<= 1024) into out_l / out_r: the device runs until they exist */
__attribute__((used, visibility("default"))) void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u)
        n = 1024u;
    while (ring_w - ring_r < n)
        run_ms(1);
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1];
    }
}

/* a screen reader (fm1-reader): the page writes a SysEx frame (the bytes between F0 and F7) to web_sysex_buf and
 * calls web_sysex(n); what the firmware says back, whole F0..F7 frames, from web_said_buf (web_said_len bytes;
 * web_said_clear when read) */
static uint8_t said_buf[4096], sx_in[64];
static uint32_t said_n;
static void web_said(const uint8_t *p, uint32_t n)
{
    if (said_n + n <= sizeof said_buf) {
        memcpy(said_buf + said_n, p, n);
        said_n += n;
    }
}
__attribute__((used, visibility("default"))) uint8_t *web_said_buf(void) { return said_buf; }
__attribute__((used, visibility("default"))) uint32_t web_said_len(void) { return said_n; }
__attribute__((used, visibility("default"))) void web_said_clear(void) { said_n = 0; }
__attribute__((used, visibility("default"))) uint8_t *web_sysex_buf(void) { return sx_in; }
__attribute__((used, visibility("default"))) void web_sysex(uint32_t n) { ann_frame(sx_in, n > sizeof sx_in ? sizeof sx_in : n, now_ms); }

__attribute__((used, visibility("default"))) void web_boot(void) { boot(); }
__attribute__((used, visibility("default"))) void web_buttons(uint32_t m) { held_btn = m; }
__attribute__((used, visibility("default"))) void web_keys(uint32_t m) { held_keys = m; }
__attribute__((used, visibility("default"))) void web_enc(int role, int32_t n)
{
    if (role >= 0 && role < NE)
        enc_acc[role] += n;
}
__attribute__((used, visibility("default"))) void web_master(uint32_t v) { master = v > 4096u ? 4096u : v; }
/* MIDI in (the page's Web MIDI): one message, as the FM-1's USB gets it (a USB-MIDI packet, cable 0), into the queue
 * plat_midi_in drains, as the host's "midi" command */
__attribute__((used, visibility("default"))) void web_midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    const uint32_t cin = st >= 0xF0u ? 0x0Fu : st >> 4;
    if (mi_w - mi_r < MQ)
        min_q[mi_w++ % MQ] = cin | (st & 0xFFu) << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24;
}
/* (1.5.0, step 5) a set to hear first: the page writes a .dub1 SET's bytes to web_set_buf and calls web_set_load(n):
 * taken into the slots as the FM-1 takes a set loaded (APPLY's yes: its pieces into slots 1..n, WORK saved), the
 * engine on its first piece, said and shown on the card. 0 loaded (1: not saved), < 0 refused (project.c's D1C_*) */
static uint8_t web_set[PLAT_FS_MAX];
__attribute__((used, visibility("default"))) uint8_t *web_set_buf(void) { return web_set; }
__attribute__((used, visibility("default"))) int web_set_load(uint32_t n)
{
    int rc;
    if (n > sizeof web_set || n < 44u)
        return -1;
    if ((rc = project_set_mem(web_set, n)) < 0)
        return rc;
    project_apply();
    en_regrow();
    ui.prog_from = en_queue_w();
    ui.cue_from = -1;
    ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
    if (rc)
        mark_dirty();
    else
        ui.dirty = 0;
    say("SET LOADED", proj.name[0] ? proj.name : "");
    card_show("SET LOADED", proj.name[0] ? proj.name : "LOADED", K_ACC, 0);
    return rc;
}
/* (step 7b) a few things to watch, for the tests (tests/host/emu_cues_test.mjs: a set's cues heard here as on the FM-1):
 * 0 the 16ths since PLAY (en_bars), 1 the parts muted (ui.mutes), 2 playing, 3 the parts soloed, 4 the playing piece's
 * cues, 5 the piece, 6 the moves the cues hold, 7 the DJ filter */
__attribute__((used, visibility("default"))) int web_peek(uint32_t i)
{
    switch (i) {
    case 0: return (int)en_bars;
    case 1: return (int)ui.mutes;
    case 2: return en_playing;
    case 3: return (int)ui.solo;
    case 4: return cue.n;
    case 5: return proj.piece;
    case 6: return (int)cue.moves;
    case 7: return ui.mac[0];
    default: return 0;
    }
}
__attribute__((used, visibility("default"))) uint32_t web_lit_buttons(void) { return lit_btn; }
__attribute__((used, visibility("default"))) uint32_t web_lit_keys(void) { return lit_keys; }
__attribute__((used, visibility("default"))) uint16_t *web_fb(void) { return fb; }
__attribute__((used, visibility("default"))) uint32_t web_blits(void) { return blits; }
__attribute__((used, visibility("default"))) uint32_t web_now(void) { return now_ms; }
__attribute__((used, visibility("default"))) uint32_t web_cpu(void) { return cpu_pct; }

/* saved objects: the page keeps them (IndexedDB since 1.5.0, step 5; localStorage before) and gives them back before boot. (1.5.0) The flash is a simulated
 * NOR now (amb_host.c, the firmware's storage.c on it): objects 0 and 1 (OBJ_PROJ, OBJ_PANEL) go through a buffer, as
 * payloads (an older page's saves still load: 1.4's project then migrated into WORK at the boot, as on a device);
 * object 2 is the frame store's region itself, 0x97000..0xDAFFF (WORK and the sets; the page keeps it in IndexedDB) */
#define WEB_FRAMES OBJ_NOBJ
static uint8_t web_stage[OBJ_NOBJ][PLAT_STORE_MAX];
__attribute__((used, visibility("default"))) uint8_t *web_store(uint32_t obj)
{
    host_nor_init();
    return obj < OBJ_NOBJ ? web_stage[obj] : obj == WEB_FRAMES ? host_nor + FS_BASE : 0;
}
__attribute__((used, visibility("default"))) int web_store_len(uint32_t obj)
{
    if (obj == WEB_FRAMES)
        return (int)(FS_N * FS_SIZE);
    return obj < OBJ_NOBJ ? plat_store_load(obj, web_stage[obj], PLAT_STORE_MAX) : -1;
}
__attribute__((used, visibility("default"))) void web_store_set_len(uint32_t obj, int n)   /* (before the boot) */
{
    if (obj < OBJ_NOBJ && n > 0)
        st_save(obj, web_stage[obj], n > (int)PLAT_STORE_MAX ? PLAT_STORE_MAX : (uint32_t)n);
}
__attribute__((used, visibility("default"))) uint32_t web_store_writes(void) { return store_writes; }
__attribute__((used, visibility("default"))) uint32_t web_nobj(void) { return OBJ_NOBJ + 1u; }
__attribute__((used, visibility("default"))) uint32_t web_store_max(void) { return PLAT_STORE_MAX; }
__attribute__((used, visibility("default"))) uint32_t web_store_cap(uint32_t obj)   /* (1.5.0) an object's bytes at most */
{
    return obj < OBJ_NOBJ ? PLAT_STORE_MAX : obj == WEB_FRAMES ? FS_N * FS_SIZE : 0u;
}
