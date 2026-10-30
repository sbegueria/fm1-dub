/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 platform on the FM-1: plat.h over Felucca's HAL, panel map, USB rings and storage.
 * Part of the unity build (amb.c), after the HAL, usb.c, storage.c and panel.c. */

uint32_t plat_ms(void) { return fm1_ms; }

uint32_t plat_buttons(void)
{
    uint32_t raw = fm1_in.buttons, out = 0, i;
    for (i = 0; i < NB; i++)
        if (raw & (1u << panel.btn[i]))
            out |= 1u << i;
    return out;
}

uint32_t plat_keys(void) { return fm1_in.notes & ((1u << NKEYS) - 1u); }

int32_t plat_enc(int role)
{
    if (role < 0 || role >= NE)
        return 0;
    return fm1_enc_take(panel.enc[role]) * panel.dir[role];
}

static volatile uint32_t master_q12 = 2048;
uint32_t plat_master(void) { return master_q12; }

/* LEDs: the picture is built off-line and copied one byte per column (Felucca: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker) */
static uint8_t led_pos[FM1_NKEY];
static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < FM1_NKEY; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
/* Glow: every button not lit is lit dim, on one scan frame in GLOW_DIV (the HAL's fm1_led_dim) */
#define GLOW_DIV 4u
static uint8_t glow_on = 1;
void plat_glow(int on) { glow_on = (uint8_t)(on != 0); }
/* PLAY has two LEDs: its red at the button's own position, a green at the keyless matrix position
 * column 8, row PA5 (found on the device). Green, unless recording. */
static uint8_t play_red;
void plat_play_red(int red) { play_red = (uint8_t)(red != 0); }
void plat_leds(uint32_t buttons, uint32_t keys)
{
    uint8_t nl[FM1_NCOL] = {0}, dl[FM1_NCOL] = {0};
    uint32_t i, c;
    for (i = 0; i < NB + NKEYS; i++) {
        uint32_t id = i < NB ? panel.btn[i] : 14u + (i - NB);
        int on = i < NB ? (buttons >> i) & 1u : (keys >> (i - NB)) & 1u;
        uint8_t q = led_pos[id];
        if (i == B_PLAY && !play_red)            /* PLAY's green LED instead (keyless position col 8, row PA5) */
            q = (uint8_t)((8u << 3) | 1u);
        if (q == 0xFF)
            continue;
        if (on)
            nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
        else if (i < NB && glow_on)
            dl[q >> 3] |= (uint8_t)(1u << (q & 7u));
    }
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led[c] = nl[c];
        fm1_led_dim[c] = dl[c];
    }
    fm1_led_dim_div = glow_on ? GLOW_DIV : 0u;
}

/* MIDI: Felucca's rings carry 4-byte USB-MIDI event packets, byte 0 = cable / CIN */
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = midi_in_q[mi_r % MQ];
    mi_r++;
    return 1;
}
void plat_midi_out(uint32_t pkt) { midi_out_event(pkt); }
void plat_midi_send(uint32_t pkt)                /* (the main loop's: the audio ISR fills the same ring, so it waits) */
{
    fm1_irq_off();
    midi_out_event(pkt);
#if EN_USBH
    usbh_out(pkt);
#endif
    fm1_irq_on();
}
/* the engine's MIDI out (from the render): a USB-MIDI packet, cable 0, CIN = the status's high nibble */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

int plat_store_load(uint32_t obj, void *dst, uint32_t max) { return flash_ok ? st_load(obj, dst, max) : -1; }
int plat_store_save(uint32_t obj, const void *src, uint32_t len) { return flash_ok ? st_save(obj, src, len) : -9; }

static volatile uint32_t audio_cpu_pct, audio_xruns, audio_cpu_peak;
uint32_t plat_cpu_pct(void) { return audio_cpu_pct; }
uint32_t plat_cpu_peak(void) { return audio_cpu_peak; }
static int32_t batt_raw = -1;
int32_t plat_battery(void) { return batt_raw; }
int plat_usb_power(void) { return usb.config && !usb.suspended; }
uint32_t plat_xruns(void) { return audio_xruns; }
uint32_t plat_clock_mhz(void)                  /* (once: the cycle counter against TIMER4, 20 ms) */
{
    static uint32_t mhz, done;
    if (!done) {
        uint32_t c0, c1, t0, t1;
        done = 1;
        c0 = fm1_perf_cycles(), t0 = fm1_ticks();
        while ((uint32_t)(fm1_ticks() - t0) < 20000u * FM1_TICKS_PER_US)
            ;
        c1 = fm1_perf_cycles(), t1 = fm1_ticks();
        if (c1 != c0 && t1 != t0)
            mhz = ((c1 - c0) * FM1_TICKS_PER_US + (t1 - t0) / 2u) / (t1 - t0);   /* (32 bits: 7.2 M cycles x 24) */
    }
    return mhz;
}
void plat_clock_regs(uint32_t r[3]) { fm1_clock_regs(r); }
__attribute__((section(".fast_text"))) uint32_t en_prof_ticks(void) { return fm1_ticks(); }   /* (amb.c's stages: TIMER4, 24 MHz; in RAM) */
static volatile uint32_t prof_peak[PLAT_NPROF], prof_mean[PLAT_NPROF];   /* (render_block's: TIMER4 ticks) */
void plat_prof(uint32_t peak[PLAT_NPROF], uint32_t mean[PLAT_NPROF])
{
    int i;
    for (i = 0; i < PLAT_NPROF; i++)
        peak[i] = prof_peak[i] / FM1_TICKS_PER_US, mean[i] = prof_mean[i] / FM1_TICKS_PER_US;
}   /* (hal/fm1_clock.h; stock: 6, 0940022B, 0750310C) */
