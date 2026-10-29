/* SPDX-License-Identifier: GPL-3.0-only */
/* USB host on USB0: a class-compliant USB-MIDI controller plugged into the FM-1's own port (an Intech Grid
 * module, a keyboard). HOME held at power-on starts it, for that session only: a plain power-on is the USB-MIDI
 * device as ever (usb.c), so a computer, the web installer and M-UPGRADE always find the FM-1.
 * The register sequence is the AC79 SDK's (cpu.a usb.c, for USB0): usb_h_sie_init (pads pulled down, PHY on,
 * VBUS valid), usb_h_chirp_and_reset (DEVCTL session, FS/LS seen, bus reset), usb_ctlXfer (EP0, host CSR0
 * bits), usb0_h_ep_read (RXTYPE, ReqPkt; RxPktRdy, 0x10, ReqPkt again); enumeration as _usb_host_mount.
 * One device, no hub. All SIE traffic in usbh_poll(), from the TIMER5 ISR at 2 kHz in usb_poll's place, never
 * blocking: each step is a state that looks once a poll. The FM-1's USB-C may not give the controller power
 * (5 V out): a controller that needs it then wants a powered OTG cable (power in, data through), not a hub.
 * What comes in goes to usb.c's MIDI in queue: the UI cannot tell it from the device's or the TRS jack's. */
enum { S_DEVCTL = 15, S_RXTYPE = 26, S_RXINTERVAL = 27 };
enum { H_OFF, H_POWER, H_ATTACH, H_RESET, H_RESET_END, H_ENUM, H_RUN, H_NOMIDI, H_FAIL };
enum { C_SETUP, C_IN, C_OUT_STATUS, C_IN_STATUS };
#define H_ADDR 1u                                    /* the device's address once set */
#define H_TRIES 5u                                   /* enumerations before a pause and a fresh start */

static uint8_t h_ep0[64 + 4] __attribute__((aligned(4)));
static uint8_t h_rx[64 + 4] __attribute__((aligned(4)));
static uint8_t h_desc[256] __attribute__((aligned(4)));

static struct {
    volatile uint8_t on;         /* host mode: usbh_poll runs instead of usb_poll */
    volatile uint8_t st;         /* H_* */
    uint8_t step;                /* the enumeration's step (H_ENUM) */
    uint8_t addr, mps0;          /* the device's address (0 until set) and EP0 packet size */
    uint8_t lowspeed, tries;
    uint8_t cfg;                 /* bConfigurationValue */
    uint8_t ep_in, ep_type, ep_ival;   /* the MIDI streaming IN endpoint: number, bulk 2 / interrupt 3, interval */
    uint8_t hub;                 /* the device was a hub (not supported) */
    uint32_t t;                  /* ms: the state's start, or the wait's end (h_wait) */
    uint32_t se0;                /* polls the bus has been SE0 (D+ and D- low: nothing there) */
    /* the control transfer under way */
    uint8_t cst;
    uint8_t *buf;
    uint16_t len, got;
    uint32_t ct;
    volatile uint32_t connects, rx_pkts, errors;
} usbh;

static void h_wait(uint32_t ms) { usbh.t = fm1_ms + ms; }
static int h_waiting(void) { return (int32_t)(fm1_ms - usbh.t) < 0; }

/* ---------------------------------------------------- the control pipe --- */
static void ctl_start(uint8_t rt, uint8_t req, uint16_t val, uint16_t idx, uint16_t len, uint8_t *buf)
{
    h_ep0[0] = rt;
    h_ep0[1] = req;
    h_ep0[2] = (uint8_t)val;
    h_ep0[3] = (uint8_t)(val >> 8);
    h_ep0[4] = (uint8_t)idx;
    h_ep0[5] = (uint8_t)(idx >> 8);
    h_ep0[6] = (uint8_t)len;
    h_ep0[7] = (uint8_t)(len >> 8);
    usbh.buf = buf;
    usbh.len = len;
    usbh.got = 0;
    sie_wr(S_FADDR, usbh.addr);
    fm1_usb_ep0_send(h_ep0, 8);
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x0A);                            /* SetupPkt | TxPktRdy */
    usbh.cst = C_SETUP;
    usbh.ct = fm1_ms;
}

static int ctl_step(void)                            /* 0: under way, 1: done, -1: failed */
{
    uint32_t csr, n, i;
    if (fm1_ms - usbh.ct > 500u)
        return -1;
    sie_wr(S_INDEX, 0);
    csr = sie_rd(S_CSR0);
    if (csr & 0x94u) {                               /* RxStall, Error, NAK timeout */
        sie_wr(S_CSR0, 0);
        return -1;
    }
    switch (usbh.cst) {
    case C_SETUP:
        if (csr & 0x02u)                             /* TxPktRdy: the SETUP not gone yet */
            return 0;
        sie_wr(S_CSR0, usbh.len ? 0x20 : 0x60);      /* ReqPkt (data in), or the status in: StatusPkt | ReqPkt */
        usbh.cst = usbh.len ? C_IN : C_IN_STATUS;
        usbh.ct = fm1_ms;
        return 0;
    case C_IN:
        if (!(csr & 0x01u))                          /* RxPktRdy */
            return 0;
        n = sie_rd(S_COUNT0);
        fm1_usb_rx_sync();
        for (i = 0; i < n && usbh.got < usbh.len; i++)
            usbh.buf[usbh.got++] = h_ep0[i];
        if (n < usbh.mps0 || usbh.got >= usbh.len) {
            sie_wr(S_CSR0, 0);                       /* RxPktRdy off */
            fm1_usb_ep0_send(h_ep0, 0);
            sie_wr(S_CSR0, 0x42);                    /* the status out, zero length: StatusPkt | TxPktRdy */
            usbh.cst = C_OUT_STATUS;
        } else {
            sie_wr(S_CSR0, 0x20);                    /* the next packet */
        }
        usbh.ct = fm1_ms;
        return 0;
    case C_OUT_STATUS:
        return csr & 0x02u ? 0 : 1;
    default:                                         /* C_IN_STATUS */
        if (!(csr & 0x01u))
            return 0;
        sie_wr(S_CSR0, 0);
        return 1;
    }
}

/* the configuration: its first MIDI streaming interface (audio class 1, subclass 3) and that interface's
 * first IN endpoint, bulk or interrupt */
static int parse_cfg(const uint8_t *d, uint32_t n)
{
    uint32_t i = 0;
    int midi = 0;
    usbh.cfg = n > 5u ? d[5] : 1;
    while (i + 2u <= n && d[i] >= 2u && i + d[i] <= n) {
        if (d[i + 1] == 4u && d[i] >= 9u)
            midi = d[i + 5] == 1u && d[i + 6] == 3u;
        else if (d[i + 1] == 5u && d[i] >= 7u && midi && (d[i + 2] & 0x80u) && (d[i + 3] & 3u) >= 2u) {
            usbh.ep_in = d[i + 2] & 15u;
            usbh.ep_type = d[i + 3] & 3u;
            usbh.ep_ival = d[i + 6];
            return 1;
        }
        i += d[i];
    }
    return 0;
}

/* ------------------------------------------------------- the IN pipe --- */
static void rx_arm(void)
{
    sie_wr(S_INDEX, 1);
    sie_wr(S_RXTYPE, (uint32_t)usbh.ep_type << 4 | usbh.ep_in);
    sie_wr(S_RXCSR1, 0x20);                          /* ReqPkt */
}

static void rx_config(void)
{
    fm1_usb_ep_rxbuf(1, h_rx);
    sie_wr(S_FADDR, usbh.addr);
    sie_wr(S_INDEX, 1);
    sie_wr(S_RXMAXP, 0xFF);
    sie_wr(S_RXCSR1, 0x90);                          /* ClrDataTog | FlushFIFO */
    sie_wr(S_RXCSR2, 0);
    sie_wr(S_RXINTERVAL, usbh.ep_type == 3u ? usbh.ep_ival : 0);   /* (bulk: no NAK limit) */
    fm1_usb_ep_enable(1u << 1);
    rx_arm();
}

static void rx_service(void)
{
    uint32_t csr, n, i;
    sie_wr(S_INDEX, 1);
    csr = sie_rd(S_RXCSR1);
    if (csr & 0x01u) {                               /* RxPktRdy */
        n = sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8);
        if (n > 64u)
            n = 64u;
        fm1_usb_rx_sync();
        for (i = 0; i + 3u < n; i += 4u) {           /* USB-MIDI event packets, as ep1_rx takes them */
            uint32_t cin = h_rx[i] & 15u, pkt = (uint32_t)h_rx[i] | (uint32_t)h_rx[i + 1] << 8 |
                                                (uint32_t)h_rx[i + 2] << 16 | (uint32_t)h_rx[i + 3] << 24;
            if (((cin >= 8u && cin <= 0xEu) || (cin == 0xFu && h_rx[i + 1] >= 0xF8u)) && mi_w - mi_r < MQ) {
                midi_in_q[mi_w % MQ] = pkt;
                RING_PUBLISH();
                mi_w++;
            }
        }
        usbh.rx_pkts++;
        sie_wr(S_RXCSR1, 0x10);
        rx_arm();
    } else if (csr & 0x44u) {                        /* RxStall, or Error (three tries unanswered) */
        usbh.errors++;
        sie_wr(S_RXCSR1, 0);
        usbh.tries = 0;
        usbh.st = H_RESET;                           /* enumerate again */
    } else if (csr & 0x08u) {                        /* DataError / NAK timeout: ask again */
        sie_wr(S_RXCSR1, 0);
        rx_arm();
    }
}

/* ------------------------------------------------------- enumeration --- */
static void enum_fail(void)
{
    usbh.errors++;
    if (++usbh.tries >= H_TRIES) {
        usbh.st = H_FAIL;
        h_wait(2000);
    } else {
        usbh.st = H_RESET;
    }
}

static void enum_service(void)
{
    int r;
    if (h_waiting())
        return;
    if (usbh.step & 1u) {                            /* odd steps: a transfer under way */
        r = ctl_step();
        if (r < 0) {
            enum_fail();
            return;
        }
        if (!r)
            return;
    }
    switch (usbh.step) {
    case 0:                                          /* the device descriptor's first 8 bytes, at address 0 */
        ctl_start(0x80, 6, 0x0100, 0, 8, h_desc);
        break;
    case 1:
        usbh.mps0 = h_desc[7] >= 8u && h_desc[7] <= 64u ? h_desc[7] : 8u;
        if (h_desc[4] == 9u) {                       /* a hub */
            usbh.hub = 1;
            usbh.st = H_NOMIDI;
            return;
        }
        h_wait(20);
        break;
    case 2:
        ctl_start(0x00, 5, H_ADDR, 0, 0, 0);         /* SET_ADDRESS */
        break;
    case 3:
        usbh.addr = H_ADDR;
        h_wait(20);
        break;
    case 4:
        ctl_start(0x80, 6, 0x0200, 0, 9, h_desc);    /* the configuration's first 9 bytes: its length */
        break;
    case 5:
        break;
    case 6: {
        uint32_t n = (uint32_t)h_desc[2] | (uint32_t)h_desc[3] << 8;
        ctl_start(0x80, 6, 0x0200, 0, (uint16_t)(n < 9u ? 9u : n > sizeof h_desc ? sizeof h_desc : n), h_desc);
        break;
    }
    case 7:
        if (!parse_cfg(h_desc, usbh.got)) {          /* no MIDI in it */
            usbh.st = H_NOMIDI;
            return;
        }
        break;
    case 8:
        ctl_start(0x00, 9, usbh.cfg, 0, 0, 0);       /* SET_CONFIGURATION */
        break;
    default:                                         /* 9: configured */
        rx_config();
        usbh.tries = 0;
        usbh.connects++;
        usbh.st = H_RUN;
        return;
    }
    usbh.step++;
}

/* ---------------------------------------------------------- the poll --- */
static void usbh_poll(void)                          /* TIMER5 ISR, 2 kHz (in usb_poll's place) */
{
    uint32_t dc;
    if (usbh.st >= H_ENUM) {                         /* gone: the bus SE0 for 50 ms, or the SIE says so */
        usbh.se0 = fm1_usb_se0() ? usbh.se0 + 1u : 0;
        if (usbh.se0 > 100u || (sie_rd(S_INTRUSB) & 0x20u)) {
            usbh.se0 = 0;
            usbh.step = 0;
            usbh.st = H_POWER;
        }
    }
    switch (usbh.st) {
    case H_POWER:                                    /* the pads pulled down, the PHY on as host (usb_h_sie_init) */
        if (usbh.step == 0) {                        /* whatever was there off first, 25 ms (as usb_start) */
            fm1_usb_reset();
            usbh.addr = 0;
            usbh.lowspeed = 0;
            usbh.hub = 0;
            usbh.tries = 0;
            usbh.step = 1;
            h_wait(25);
            break;
        }
        if (h_waiting())
            break;
        fm1_usb_host_attach(h_ep0);
        usbh.st = H_ATTACH;
        usbh.step = 0;
        h_wait(10);
        break;
    case H_ATTACH:                                   /* a session; the device's pull-up: FS or LS (100 ms steady) */
        if (h_waiting())
            break;
        if (usbh.step == 0) {
            sie_wr(S_POWER, 0x60);
            sie_wr(S_INTRUSBE, 0);
            sie_wr(S_INTRTX1E, 0);
            sie_wr(S_INTRRX1E, 0);
            sie_wr(S_DEVCTL, 3);                     /* Session | HostReq */
            usbh.step = 1;
            usbh.ct = fm1_ms;
            usbh.t = fm1_ms;
            break;
        }
        dc = sie_rd(S_DEVCTL);
        if (!(dc & 0x04u) || !(dc & 0x60u)) {        /* not host yet, or nothing there */
            usbh.ct = fm1_ms;
            if (fm1_ms - usbh.t > 1000u) {           /* ask for the session again now and then */
                sie_wr(S_DEVCTL, 3);
                usbh.t = fm1_ms;
            }
        } else if (fm1_ms - usbh.ct >= 100u) {
            usbh.lowspeed = !(dc & 0x40u);
            usbh.st = H_RESET;
        }
        break;
    case H_RESET:                                    /* bus reset, 20 ms and 10 more a retry (the SDK's) */
        fm1_usb_low_speed(usbh.lowspeed);
        sie_wr(S_FADDR, 0);
        sie_wr(S_POWER, (sie_rd(S_POWER) & ~8u) | 8u);
        h_wait(20u + 10u * usbh.tries);
        usbh.st = H_RESET_END;
        break;
    case H_RESET_END:
        if (h_waiting())
            break;
        sie_wr(S_POWER, sie_rd(S_POWER) & ~8u);
        usbh.addr = 0;
        usbh.mps0 = 8;
        usbh.step = 0;
        usbh.se0 = 0;
        h_wait(20u + 10u * usbh.tries);
        usbh.st = H_ENUM;
        break;
    case H_ENUM:
        enum_service();
        break;
    case H_RUN:
        rx_service();
        break;
    case H_FAIL:                                     /* five tries failed: a pause, then from the start */
        if (!h_waiting()) {
            usbh.step = 0;
            usbh.st = H_POWER;
        }
        break;
    default:                                         /* H_NOMIDI: wait for it to go (the SE0 check above) */
        break;
    }
}

static void usbh_begin(void)                         /* (main loop, at boot) the device off, the host on */
{
    usb_detach();
    usbh.step = 0;
    usbh.st = H_POWER;
    RING_PUBLISH();
    usbh.on = 1;
}
