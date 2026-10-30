/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB host (usb_host.c) against a model of USB0's SIE in host mode and a device behind it. The model
 * follows the AC79 SDK's host code for USB0 (cpu.a usb.c): DEVCTL's Session / HostMode / FSDev, POWER's Reset,
 * CSR0's SetupPkt / ReqPkt / StatusPkt / TxPktRdy / RxPktRdy / RxStall / Error, the EP0 DMA buffer, RXTYPE and
 * ReqPkt on EP1, RxPktRdy cleared by 0x10. What it checks is the driver's side: enumeration in order (the
 * requests, the address, the configuration), the MIDI streaming interface found among others (an Intech
 * Grid's CDC data interface has a bulk IN too), the events into the MIDI in queue, an unplug and a second
 * plug, a hub and a keyboard turned away, a stall retried, a device that never answers, an interrupt
 * endpoint, an 8-byte EP0. Not the silicon: a register the SDK uses otherwise than the model would pass here
 * and fail on an FM-1. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")

/* ---- usb.c's, as usb_host.c uses them ---- */
enum { S_FADDR = 0, S_POWER = 1, S_INTRUSB = 6, S_INTRTX1E = 7, S_INTRRX1E = 9, S_INTRUSBE = 11, S_INDEX = 14,
       S_CSR0 = 17, S_RXMAXP = 19, S_RXCSR1 = 20, S_RXCSR2 = 21, S_COUNT0 = 22, S_RXCOUNT1 = 22, S_RXCOUNT2 = 23 };
#define MQ 64u
static uint32_t midi_in_q[MQ];
static volatile uint32_t mi_w, mi_r;
static volatile uint32_t fm1_ms;
static int detached;
static void usb_detach(void) { detached = 1; }

/* ---- the device ---- */
typedef struct {
    const char *name;
    const uint8_t *dev, *cfg;
    uint16_t cfglen;
    uint8_t ep_in;               /* where its MIDI comes from (0: none) */
    int stall_n;                 /* data stages to stall before it answers */
    int mute;                    /* never answers */
} device_t;

static struct {
    const device_t *d;
    int attached, session, configured, cfgval, stall_n;
    uint8_t addr, pend_addr, pend_cfg, setup[8];
    uint8_t faddr, index, power, csr0, rxcsr1, rxtype, rxinterval, intrusb;
    uint16_t count0, rxcount;
    int rx_req;
    uint8_t *ep0p, *rxp;
    uint32_t ep0cnt;
    const uint8_t *in_src;
    uint16_t in_left;
    int resets, setups, bad;
    uint8_t q[256];              /* the device's MIDI to send: 4-byte USB-MIDI packets */
    int qn;
} m;

static void model_plug(const device_t *d)
{
    m.d = d;
    m.attached = 1;
    m.addr = 0;
    m.configured = 0;
    m.stall_n = d->stall_n;
    m.intrusb |= 0x10;           /* Conn */
}
static void model_unplug(void)
{
    m.attached = 0;
    m.configured = 0;
    m.addr = 0;
    m.intrusb |= 0x20;           /* Discon */
}
static void model_send(uint8_t cin, uint8_t a, uint8_t b, uint8_t c)
{
    m.q[m.qn++] = cin, m.q[m.qn++] = a, m.q[m.qn++] = b, m.q[m.qn++] = c;
}

/* ---- the HAL, host side ---- */
static void fm1_usb_reset(void) { m.session = 0, m.power = 0, m.csr0 = 0, m.rxcsr1 = 0, m.rx_req = 0; }
static void fm1_usb_host_attach(void *ep0) { m.ep0p = ep0; }
static void fm1_usb_ep0_send(void *p, uint32_t n) { m.ep0p = p, m.ep0cnt = n; }
static void fm1_usb_rx_sync(void) {}
static void fm1_usb_ep_rxbuf(uint32_t ep, void *p) { (void)ep, m.rxp = p; }
static void fm1_usb_ep_enable(uint32_t eps) { (void)eps; }
static void fm1_usb_low_speed(uint32_t on) { (void)on; }
static uint32_t fm1_usb_se0(void) { return !m.attached; }

static void dev_setup(void)
{
    const uint8_t *s = m.setup;
    uint16_t val = (uint16_t)(s[2] | s[3] << 8), len = (uint16_t)(s[6] | s[7] << 8);
    m.setups++;
    m.in_src = 0, m.in_left = 0;
    if (s[0] == 0x80 && s[1] == 6) {             /* GET_DESCRIPTOR */
        const uint8_t *p = val >> 8 == 1 ? m.d->dev : val >> 8 == 2 ? m.d->cfg : 0;
        uint16_t n = val >> 8 == 1 ? 18 : m.d->cfglen;
        if (!p)
            m.bad++;
        m.in_src = p;
        m.in_left = len < n ? len : n;
    } else if (s[0] == 0x00 && s[1] == 5) {
        m.pend_addr = (uint8_t)val;
    } else if (s[0] == 0x00 && s[1] == 9) {
        m.pend_cfg = (uint8_t)val;
    } else {
        m.bad++;
    }
}

static void csr0_write(uint8_t v)
{
    const int answers = m.attached && !m.d->mute && m.faddr == m.addr && m.session;
    if (v == 0x0A) {                             /* SETUP */
        if (!answers) {
            m.csr0 = 0x10;
            return;
        }
        if (m.ep0cnt != 8)
            m.bad++;
        memcpy(m.setup, m.ep0p, 8);
        dev_setup();
        m.csr0 = 0;
    } else if (v == 0x20) {                      /* IN data */
        uint16_t n;
        if (!answers) {
            m.csr0 = 0x10;
            return;
        }
        if (m.stall_n > 0) {
            m.stall_n--;
            m.csr0 = 0x04;
            return;
        }
        n = m.in_left < m.d->dev[7] ? m.in_left : m.d->dev[7];
        memcpy(m.ep0p, m.in_src, n);
        m.in_src += n, m.in_left = (uint16_t)(m.in_left - n);
        m.count0 = n;
        m.csr0 = 0x01;
    } else if (v == 0x42) {                      /* status out (after IN data): zero length */
        if (m.ep0cnt != 0)
            m.bad++;
        m.csr0 = 0;
    } else if (v == 0x60) {                      /* status in (no data): the request takes effect */
        if (!answers) {
            m.csr0 = 0x10;
            return;
        }
        if (m.setup[1] == 5)
            m.addr = m.pend_addr;
        if (m.setup[1] == 9)
            m.configured = 1, m.cfgval = m.pend_cfg;
        m.count0 = 0;
        m.csr0 = 0x01;
    } else if (v == 0) {
        m.csr0 = 0;
    } else {
        m.bad++;
    }
}

static void rx_tick(void)                        /* the IN token the SIE keeps sending while ReqPkt is set */
{
    int n;
    if (!m.rx_req || !m.attached || !m.configured || m.faddr != m.addr || (m.rxtype & 15) != m.d->ep_in || !m.qn)
        return;
    n = m.qn > 64 ? 64 : m.qn;
    memcpy(m.rxp, m.q, (size_t)n);
    memmove(m.q, m.q + n, (size_t)(m.qn - n));
    m.qn -= n;
    m.rxcount = (uint16_t)n;
    m.rxcsr1 = 0x01;
    m.rx_req = 0;
}

static void sie_wr(uint32_t r, uint32_t v)
{
    switch (r) {
    case S_FADDR: m.faddr = (uint8_t)v; break;
    case S_POWER:
        if ((v & 8) && !(m.power & 8))
            m.resets++, m.addr = 0, m.configured = 0;
        m.power = (uint8_t)v;
        break;
    case S_INDEX: m.index = (uint8_t)v; break;
    case 15: m.session = v & 1; break;           /* DEVCTL */
    case S_CSR0:
        if (m.index == 0)
            csr0_write((uint8_t)v);
        break;
    case S_RXCSR1:
        if (m.index != 1)
            break;
        if (v == 0x20)
            m.rx_req = 1;
        else if (v == 0x10 || v == 0x90 || v == 0)   /* (the SDK's: 0x10 after a packet, 0x90 to configure) */
            m.rxcsr1 = 0, m.rx_req = 0;
        else
            m.bad++;
        break;
    case 26: m.rxtype = (uint8_t)v; break;
    case 27: m.rxinterval = (uint8_t)v; break;
    default: break;
    }
}

static uint32_t sie_rd(uint32_t r)
{
    switch (r) {
    case 15: return m.session ? 0x05u | (m.attached ? 0x40u : 0) : 0;   /* Session, HostMode, FSDev */
    case S_POWER: return m.power;
    case S_INTRUSB: { uint32_t x = m.intrusb; m.intrusb = 0; return x; }
    case S_CSR0: return m.index == 0 ? m.csr0 : 0;
    case 22: return m.index == 0 ? m.count0 & 0xFFu : m.rxcount & 0xFFu;
    case 23: return m.rxcount >> 8;
    case S_RXCSR1: rx_tick(); return m.rxcsr1;
    default: return 0;
    }
}

#include "../../firmware/src/usb_host.c"

/* ---- the devices ---- */
static const uint8_t GRID_DEV[18] = {18, 1, 0, 2, 0xEF, 2, 1, 64, 0x3E, 0x30, 0x02, 0x80, 0, 1, 1, 2, 3, 1};
static const uint8_t GRID_CFG[] = {   /* CDC (its data interface: bulk IN 0x82) + Audio Control + MIDI Streaming */
    9, 2, 0, 0, 4, 1, 0, 0x80, 50,
    8, 0x0B, 0, 2, 2, 2, 0, 0,
    9, 4, 0, 0, 1, 2, 2, 0, 0,  5, 0x24, 0, 0x20, 1,  5, 0x24, 1, 0, 1,  4, 0x24, 2, 2,  5, 0x24, 6, 0, 1,
    7, 5, 0x81, 3, 8, 0, 16,
    9, 4, 1, 0, 2, 0x0A, 0, 0, 0,  7, 5, 0x02, 2, 64, 0, 0,  7, 5, 0x82, 2, 64, 0, 0,
    9, 4, 2, 0, 0, 1, 1, 0, 0,  9, 0x24, 1, 0, 1, 9, 0, 1, 3,
    9, 4, 3, 0, 2, 1, 3, 0, 0,  7, 0x24, 1, 0, 1, 0x25, 0,
    6, 0x24, 2, 1, 1, 0,  6, 0x24, 2, 2, 2, 0,  9, 0x24, 3, 1, 3, 1, 2, 1, 0,  9, 0x24, 3, 2, 4, 1, 1, 1, 0,
    9, 5, 0x03, 2, 64, 0, 0, 0, 0,  5, 0x25, 1, 1, 1,
    9, 5, 0x83, 2, 64, 0, 0, 0, 0,  5, 0x25, 1, 1, 3};
static const uint8_t HUB_DEV[18] = {18, 1, 0, 2, 9, 0, 1, 64, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1};
static const uint8_t KBD_DEV[18] = {18, 1, 0, 2, 0, 0, 0, 8, 1, 2, 3, 4, 0, 1, 1, 2, 0, 1};
/* (a keyboard's configuration says 40 bytes and has 34: the short packet ends the read) */
static const uint8_t KBD_CFG[] = {9, 2, 40, 0, 1, 1, 0, 0xA0, 50,  9, 4, 0, 0, 1, 3, 1, 1, 0,
                                  9, 0x21, 0x11, 1, 0, 1, 0x22, 63, 0,  7, 5, 0x81, 3, 8, 0, 10};
static const uint8_t SMALL_DEV[18] = {18, 1, 0, 2, 0, 0, 0, 8, 1, 2, 3, 4, 0, 1, 1, 2, 0, 1};   /* EP0 of 8 */
static const uint8_t INTR_CFG[] = {   /* MIDI only, its IN an interrupt endpoint, configuration value 2 */
    9, 2, 0, 0, 2, 2, 0, 0x80, 50,
    9, 4, 0, 0, 0, 1, 1, 0, 0,  9, 0x24, 1, 0, 1, 9, 0, 1, 1,
    9, 4, 1, 0, 1, 1, 3, 0, 0,  7, 0x24, 1, 0, 1, 0x0F, 0,
    9, 5, 0x81, 3, 64, 0, 4, 0, 0,  5, 0x25, 1, 1, 1};

static uint8_t grid_cfg[sizeof GRID_CFG], intr_cfg[sizeof INTR_CFG];

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint16_t seen;                            /* the states usbh went through (bit per H_*) */
static void run_ms(uint32_t ms)
{
    while (ms--) {
        usbh_poll();
        seen |= (uint16_t)(1u << usbh.st);
        usbh_poll();
        seen |= (uint16_t)(1u << usbh.st);
        fm1_ms++;
    }
}
static void fresh(void)
{
    memset(&m, 0, sizeof m);
    memset(&usbh, 0, sizeof usbh);
    mi_w = mi_r = 0;
    detached = 0;
    seen = 0;
    usbh_begin();
}
static uint32_t q_take(void) { return mi_r < mi_w ? midi_in_q[mi_r++ % MQ] : 0; }

int main(void)
{
    static const device_t grid = {"grid", GRID_DEV, grid_cfg, sizeof GRID_CFG, 3, 0, 0};
    static const device_t hub = {"hub", HUB_DEV, KBD_CFG, sizeof KBD_CFG, 0, 0, 0};
    static const device_t kbd = {"keyboard", KBD_DEV, KBD_CFG, sizeof KBD_CFG, 0, 0, 0};
    static const device_t stall = {"stall", GRID_DEV, grid_cfg, sizeof GRID_CFG, 3, 2, 0};
    static const device_t mute = {"mute", GRID_DEV, grid_cfg, sizeof GRID_CFG, 3, 0, 1};
    static const device_t intr = {"interrupt", SMALL_DEV, intr_cfg, sizeof INTR_CFG, 1, 0, 0};
    memcpy(grid_cfg, GRID_CFG, sizeof GRID_CFG);
    grid_cfg[2] = (uint8_t)sizeof GRID_CFG;
    memcpy(intr_cfg, INTR_CFG, sizeof INTR_CFG);
    intr_cfg[2] = (uint8_t)sizeof INTR_CFG;

    /* nothing plugged in: it waits, the device driver off */
    fresh();
    run_ms(2000);
    CHECK(detached, "usbh_begin left the device driver on");
    CHECK(usbh.st == H_ATTACH, "nothing there: state %d, not H_ATTACH", usbh.st);
    CHECK(m.setups == 0, "%d SETUPs to nobody", m.setups);

    /* an Intech Grid: enumerated, its MIDI streaming IN found (not the CDC's), CCs through, SysEx left out */
    model_plug(&grid);
    run_ms(1000);
    CHECK(usbh.st == H_RUN, "grid: state %d, not H_RUN", usbh.st);
    CHECK(m.addr == H_ADDR && m.configured && m.cfgval == 1, "grid: addr %d configured %d value %d", m.addr,
          m.configured, m.cfgval);
    CHECK(usbh.ep_in == 3 && usbh.ep_type == 2, "grid: endpoint %d type %d, not 3 bulk", usbh.ep_in, usbh.ep_type);
    CHECK(m.rxtype == 0x23, "grid: RXTYPE %#x, not 0x23", m.rxtype);
    CHECK(m.resets == 1 && m.bad == 0, "grid: %d resets, %d bad requests", m.resets, m.bad);
    model_send(0x0B, 0xB0, 20, 127);             /* CC 20 127 */
    model_send(0x04, 0xF0, 0x7E, 0x00);          /* SysEx: not for the queue */
    model_send(0x07, 0x01, 0x02, 0xF7);
    model_send(0x09, 0x90, 60, 100);             /* a note */
    model_send(0x0F, 0xF8, 0, 0);                /* clock */
    run_ms(5);
    CHECK(mi_w == 3, "grid: %u events in the queue, not 3", (unsigned)mi_w);
    CHECK(q_take() == 0x7F14B00Bu, "grid: the CC changed on the way");
    CHECK(q_take() == 0x643C9009u, "grid: the note changed on the way");
    CHECK(q_take() == 0x0000F80Fu, "grid: the clock changed on the way");
    {   /* a burst bigger than a packet: all of it, in order */
        int i, ok = 1;
        for (i = 0; i < 40; i++)
            model_send(0x0B, 0xB0, 21, (uint8_t)i);
        run_ms(5);
        for (i = 0; i < 40; i++)
            ok &= q_take() == (0x0Bu | 0xB0u << 8 | 21u << 16 | (uint32_t)i << 24);
        CHECK(ok && mi_r == mi_w, "grid: a 40-event burst lost or reordered");
    }
    CHECK(m.bad == 0, "grid: %d writes the SIE would not take", m.bad);

    /* unplugged, and plugged in again: enumerated again */
    model_unplug();
    run_ms(200);
    CHECK(usbh.st == H_ATTACH, "unplugged: state %d, not H_ATTACH", usbh.st);
    model_plug(&grid);
    run_ms(1000);
    CHECK(usbh.st == H_RUN && usbh.connects == 2, "plugged again: state %d, %u connects", usbh.st,
          (unsigned)usbh.connects);
    model_send(0x0B, 0xB0, 1, 64);
    run_ms(5);
    CHECK(q_take() == 0x4001B00Bu, "plugged again: no CC");

    /* a hub, a keyboard: turned away, nothing configured */
    fresh();
    model_plug(&hub);
    run_ms(1000);
    CHECK(usbh.st == H_NOMIDI && usbh.hub, "hub: state %d hub %d", usbh.st, usbh.hub);
    fresh();
    model_plug(&kbd);
    run_ms(1000);
    CHECK(usbh.st == H_NOMIDI && !usbh.hub && !m.configured, "keyboard: state %d configured %d", usbh.st,
          m.configured);

    /* a device that stalls twice: retried (a reset each time), then it works */
    fresh();
    model_plug(&stall);
    run_ms(2000);
    CHECK(usbh.st == H_RUN && m.resets == 3, "stall: state %d, %d resets (3)", usbh.st, m.resets);

    /* a device that never answers: five tries, a pause, then from the start (and no lock-up) */
    fresh();
    model_plug(&mute);
    run_ms(5000);
    CHECK(seen & (1u << H_FAIL), "mute: never gave up");
    CHECK(usbh.errors >= 5u && m.resets >= 6, "mute: %u errors, %d resets", (unsigned)usbh.errors, m.resets);

    /* an interrupt IN endpoint, an 8-byte EP0 (the configuration in several packets), configuration value 2 */
    fresh();
    model_plug(&intr);
    run_ms(1000);
    CHECK(usbh.st == H_RUN && m.cfgval == 2, "interrupt: state %d value %d", usbh.st, m.cfgval);
    CHECK(m.rxtype == 0x31 && m.rxinterval == 4, "interrupt: RXTYPE %#x interval %d", m.rxtype, m.rxinterval);
    model_send(0x0B, 0xB0, 74, 10);
    run_ms(5);
    CHECK(q_take() == 0x0A4AB00Bu, "interrupt: no CC");

    printf(fails ? "usbh: FAILED\n" : "usbh: ok\n");
    return fails != 0;
}
