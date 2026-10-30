/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 platform interface (from X0X's): everything the app needs from the hardware.
 *
 * The device implementation (plat_fm1.c) wraps Felucca's HAL and panel map; the
 * host implementation (host/amb_host.c) is a simulator fed by a script, which is
 * what lets the whole app — UI, sequencer, engines — run and be tested on a Mac.
 * Drawing goes through Felucca's lcd_fill / lcd_blit (lcd.c on the device, a
 * framebuffer on the host) and gfx.c on top. */
#pragma once
#include <stdint.h>

/* big zero-initialised buffers go in the .pool RAM region on the device */
#ifdef EN_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif

/* panel controls, by printed label (Felucca's panel.c map turns these into matrix ids; Felucca
 * calls the second button SCL, the panel prints SEL) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };

#define NKEYS 27                       /* F3 .. G5 */

uint32_t plat_ms(void);
uint32_t plat_buttons(void);           /* held: bit B_* */
uint32_t plat_keys(void);              /* held: bit k = key k (0 = F3) */
int32_t plat_enc(int role);            /* detents since the last call, + = clockwise; EN_* */
uint32_t plat_master(void);            /* MASTER pot, 0..4096 gain Q12 */
void plat_leds(uint32_t buttons, uint32_t keys);   /* lit: bit B_*, bit key */
void plat_glow(int on);                /* the unlit buttons glow dimly, readable in the dark (call before plat_leds) */
void plat_play_red(int red);           /* PLAY lights green; red (its other LED) while recording (before plat_leds) */

/* USB MIDI, 4-byte USB-MIDI event packets (cable 0) */
int plat_midi_in(uint32_t *pkt);       /* 1 = got one (called from the audio ISR) */
void plat_midi_out(uint32_t pkt);      /* queue one (audio ISR or main loop) */

/* persistent storage (Felucca storage.c objects: A/B sectors, CRC, torn-write safe) */
int plat_store_load(uint32_t obj, void *dst, uint32_t max);   /* bytes loaded, < 0 = none / bad */
int plat_store_save(uint32_t obj, const void *src, uint32_t len);   /* 0 = ok */
#define PLAT_STORE_MAX 3840u           /* bytes per object */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_GRID, OBJ_NOBJ };   /* (OBJ_GRID: the Intech Grid controllers' map, ui.c) */
/* flash: OBJ_PROJ 0xFC000, OBJ_PANEL 0xDC000 (2 sectors each, inside Felucca's data area) */

/* audio load in % of the render budget, and xruns since boot (device: from the ISR) */
uint32_t plat_cpu_pct(void);
uint32_t plat_xruns(void);
uint32_t plat_cpu_peak(void);            /* the heaviest block of the last ~2 s, % of its time */
/* (1.4.0) the CPU's clock, for the Setup page: measured once (the core's cycles against the 24 MHz crystal over
 * 20 ms; 0 if the cycle counter doesn't run), and the clock controller's registers as read (source, PLL1's two
 * words): stock is M-VAVE's bootloader's 360 MHz (PLL1 540 MHz x 2/3), m-bootloader 1.2 leaves 320 */
uint32_t plat_clock_mhz(void);
void plat_clock_regs(uint32_t r[3]);
/* (1.4.0) the audio's time by stage (amb.h's EN_PROF stages, PLAT_NPROF of them), in microseconds: the heaviest block
 * of the last ~2 s, and the mean block of the same window (zeros where there is no profile: the host) */
#define PLAT_NPROF 13
void plat_prof(uint32_t peak[PLAT_NPROF], uint32_t mean[PLAT_NPROF]);
int32_t plat_battery(void);              /* the battery divider's reading, raw (0..1023, smoothed); -1: none */
int plat_usb_power(void);                /* a USB host is there (it powers, and charges, the FM-1) */
