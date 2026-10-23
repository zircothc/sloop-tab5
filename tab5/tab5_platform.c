/* SPDX-License-Identifier: GPL-3.0-only
 *
 * SLOOP on a touch-screen host: platform layer (step 1 of the M5Stack Tab5 port).
 *
 * The firmware (SLOOP, based on FELUCCA, (C) 2026 Leo Kuroshita / Hügelton Instruments, GPL-3.0) is
 * compiled as ONE unit exactly like src/felucca.c does, but every register-level HAL header is replaced
 * by the small doubles below. The design follows the browser build of FELUCCA (web/emu/felucca_web.c,
 * itself after charlesvestal/fm1-x0x): one thread, the device clock driven by the audio that is asked
 * for. Nothing in firmware/src is modified.
 *
 *   - clock    : fm1_ms / TIMER4 ticks advance 1 ms at a time (tab5_step_ms)
 *   - audio    : the real audio ISR (audio.c fm1_alnk0_irq) renders each I2S half when it is due
 *   - panel    : buttons / keys as bit masks, encoders as step counters, MASTER as a 0..1023 "ADC"
 *   - LEDs     : fm1_led / fm1_led_dim / fm1_led_bg read back through the key matrix
 *   - screen   : lcd_fill / lcd_blit into a 240 x 240 RGB565 frame (the panel's byte order)
 *   - flash    : a 1 MiB NOR image behind storage.c's hooks (settings, projects, user presets)
 *   - not here : USB, TRS MIDI, the editor, OTA, console, crash / UBOOT paths
 *
 * Build for the PC with tab5/build_host.sh; the same file is the one to build inside ESP-IDF later
 * (tab5_ticks_hook() is where a real timer goes).
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(TAB5_HOST) || defined(TAB5_STRIP_ATTR)
#define __attribute__(x)                         /* the JieLi section / alignment attributes mean nothing here */
#endif

/* ---- the build configuration (what src/felucca.c defaults to, minus the FM-1's USB, UART, OTA) ---- */
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 0
#define FELUCCA_OTA_DRYRUN 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#define FELUCCA_UART 0
#define FELUCCA_ICONS 1
#define FELUCCA_ARRANGER 1
#ifndef FELUCCA_ID
#define FELUCCA_ID "TAB5"
#endif
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "TAB5"
#endif

#define FM1_TICKS_PER_US 24u                     /* TIMER4: 24 MHz, as the firmware's time arithmetic assumes */
#define FM1_INLINE static inline
#define FM1_NCOL 11u                             /* the key matrix is how LEDs are addressed (fm1_input.h) */
#define FM1_NKEY 41u
#define FM1_NENC 7u

#include "felucca_tables.h"

/* ------------------------------------------------------------------ time --- */
static uint32_t tab5_ticks;
uint32_t (*tab5_ticks_hook)(void);               /* NULL: the deterministic device clock (tests, goldens) */
static uint32_t fm1_ticks(void) { return tab5_ticks_hook ? tab5_ticks_hook() : tab5_ticks; }
static uint32_t fm1_micros(void) { return fm1_ticks() / FM1_TICKS_PER_US; }
static void fm1_delay_ms(uint32_t ms) { (void)ms; }       /* no busy waits: the host never blocks */
static void fm1_delay_us(uint32_t us) { (void)us; }
static void fm1_time_init(void) {}

/* ------------------------------------------------- libc and the display --- */
#define memset felucca_memset                    /* the firmware's own mem* (libc.c) must not clash with the PC's */
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "libc.c"
#undef memset
#undef memcpy
#undef memcmp

static void fm1_irq_off(void) {}                 /* one thread: nothing to mask */
static void fm1_irq_on(void) {}

static uint16_t tab5_fb[240 * 240];              /* RGB565 as the panel gets it (high byte first) */
static uint32_t tab5_draws;                      /* bumped on every draw: repaint the scaled copy when it moves */
static void lcd_sync(void) {}
static void lcd_init(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sw = (uint16_t)((c >> 8) | (c << 8));
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            tab5_fb[(y + j) * 240u + x + i] = sw;
    tab5_draws++;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            tab5_fb[(y + j) * 240u + x + i] = p[j * w + i];
    tab5_draws++;
}
#include "gfx.c"

/* the SPI NOR (1 MiB, erased = 0xFF). The caller owns the memory and hands it over with tab5_set_nor() before
 * tab5_power_on(); the user sample slots are read straight out of it (eng_sample.c reads XIP). */
#define TAB5_NOR_SIZE 0x100000u
static uint8_t *tab5_nor;
static uint8_t tab5_nor_dirty_map[TAB5_NOR_SIZE / 4096u / 8u];       /* one bit per 4 KB sector written since taken */
#define SMP_USER_XIP(k) ((const uint8_t *)tab5_nor + SMP_USER_BASE + (k) * SMP_USER_SIZE)

#include "core.h"
#include "engines.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"
#include "fx.c"

/* ------------------------------------------------------ interrupts, system --- */
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
typedef struct { uint32_t magic, count, vec, pc, emu, dbg, rets, early; } fm1_crash_t;
static fm1_crash_t fm1_crash;
static struct { uint8_t p3_rst, wdt_con; uint32_t rst_src; } fm1_boot;
static void fm1_wdt_feed(void) {}
static void fm1_wdt_arm(uint32_t v) { (void)v; }
static void fm1_reboot(void) {}
static void fm1_enter_uboot(void) {}
static void fm1_reset_reason(void) {}
static void fm1_mailbox_clear(void) {}
static void fm1_guard_lock_top(void) {}
static void fm1_guard_enable(uint32_t m) { (void)m; }
static void fm1_irq_enable_all(void) {}
static void fm1_irq_init(void) {}
static void fm1_timer5_ack(void) {}
static void fm1_timer5_start(void (*isr)(void), uint32_t prio) { (void)isr; (void)prio; }
void isr_timer5(void) {}
enum { FM1_GUARD_STACK = 1, FM1_GUARD_WRITE = 2, FM1_GUARD_BUS = 4, FM1_GUARD_PC = 8 };
uint32_t _data_start[1], _data_end[1], _data_load[1], _bss_start[1], _bss_end[1];
uint32_t _pool_start[1], _pool_end[1], _rt_start[1], _rt_end[1], _rt_load[1];

/* ---------------------------------------------------------------- panel --- */
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {  /* key id at (row bit, column), as hal/fm1_input.h */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static volatile struct { uint32_t notes, buttons; } fm1_in;      /* bit per note key 0..26 / button 0..13 */
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_bg[FM1_NCOL];
static volatile uint16_t fm1_led_bg_ns;
static uint32_t tab5_pressed, tab5_released, tab5_notes_pressed;
static int32_t tab5_enc_steps[FM1_NENC];
static uint32_t tab5_master_adc = 700;           /* MASTER, 0..1023 as the ADC reads it */
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
/* the touch task writes these, the device (audio task, maybe the other core) reads and clears them: atomic, so a
 * press that lands between the read and the clear is not lost */
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = __atomic_exchange_n(&tab5_pressed, 0u, __ATOMIC_ACQ_REL);
    uint32_t r = __atomic_exchange_n(&tab5_released, 0u, __ATOMIC_ACQ_REL);
    if (released)
        *released = r;
    return p;
}
static uint32_t fm1_input_note_edges(void) { return __atomic_exchange_n(&tab5_notes_pressed, 0u, __ATOMIC_ACQ_REL); }
static int32_t fm1_enc_take(uint32_t e) { return __atomic_exchange_n(&tab5_enc_steps[e % FM1_NENC], 0, __ATOMIC_ACQ_REL); }
static void fm1_input_init(void) {}
static void fm1_input_tick(void) {}
static void fm1_input_scan(void) {}
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? (int32_t)tab5_master_adc : 620; }
static void fm1_adc_init(void) {}
static void fm1_led_key(uint32_t id, int on)     /* LED of key id (button 0..13 or note key 14..40) */
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                if (on)
                    fm1_led[p] |= (uint8_t)(1u << r);
                else
                    fm1_led[p] &= (uint8_t)~(1u << r);
            }
}

/* ----------------------------------------------------------------- USB --- */
/* usb.c is the FM-1's device-side USB stack. It is compiled for what lives in it besides the registers (the
 * MIDI rings and parser every other file uses); nothing starts it, so its register calls never run. */
#include "fm1_usb.h"
#include "usb.c"
#define usb_poll() ((void)0)
#define usb_start() ((void)0)
#define usb_retry(ms) ((void)(ms))
#define usb_detach() ((void)0)

#include "arranger.c"
#include "seq.c"

/* ---------------------------------------------------------------- audio --- */
static uint32_t tab5_half;                       /* the half the next render fills */
#define FM1_AUDIO_HALF 0x80u
static uint8_t fm1_audio_pending(void) { return FM1_AUDIO_HALF; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return tab5_half; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_init(int32_t *buf, uint32_t words, void (*isr)(void), uint32_t prio)
{ (void)buf; (void)words; (void)isr; (void)prio; }
static void fm1_audio_stop(void) {}
void isr_alnk0(void) {}
#include "audio.c"

#include "panel.c"
#include "ui.c"
#include "ui_song.c"
#include "ui_studio.c"
#include "icons.c"
#include "ui_draw.c"
#include "ui_vis.c"
#include "ui_layers.c"
#include "ui_menu.c"
#include "ui_input.c"

/* ---------------------------------------------------------------- flash --- */
#define irq_save hw_irq_save                     /* the real header defines these against registers: */
#define irq_restore hw_irq_restore               /* renamed, never called; ours are above and below */
#define fl_jedec_ram hw_fl_jedec_ram
#define fl_plain_window_init hw_fl_plain_window_init
#include "fm1_flash.h"                           /* the layout constants (FL_*); its register code never runs */
#undef irq_save
#undef irq_restore
#undef fl_jedec_ram
#undef fl_plain_window_init
static uint32_t tab5_flash_writes;
static uint8_t flash_ok;
#undef FL_FAR
#define FL_FAR(fn) (fn)
static uint32_t fl_jedec_ram(void) { return 0x856014u; }          /* the FM-1's part: what persist_boot expects */
static void fl_plain_window_init(void) {}
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off >= TAB5_NOR_SIZE || n > TAB5_NOR_SIZE - off)
        return -1;
    memcpy(dst, tab5_nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (!FL_STORE_OK(off, 0x1000u))
        return -8;
    memset(tab5_nor + (off & ~4095u), 0xFF, 4096);
    tab5_nor_dirty_map[(off >> 12) >> 3] |= (uint8_t)(1u << ((off >> 12) & 7u));
    tab5_flash_writes++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)     /* NOR: a write only clears bits */
{
    const uint8_t *s = src;
    uint32_t i;
    if (!FL_STORE_OK(off, n))
        return -8;
    for (i = 0; i < n; i++)
        tab5_nor[off + i] &= s[i];
    for (i = off >> 12; i <= (off + (n ? n - 1u : 0u)) >> 12; i++)
        tab5_nor_dirty_map[i >> 3] |= (uint8_t)(1u << (i & 7u));
    tab5_flash_writes++;
    return 0;
}
#include "storage.c"
#include "upreset.c"
#include "project.c"

#include "splash.c"
#include "main.c"

/* ================================================================ the device === */
static uint32_t tab5_booted, tab5_boot_ms, tab5_last_frame;
static uint32_t tab5_samples_due;
#define TAB5_RING 8192u
static int32_t tab5_ring[TAB5_RING * 2];         /* stereo, 24 bit left-justified (the codec's format) */
static uint32_t tab5_ring_w, tab5_ring_r;

/* main.c fm1_main(), from the top down to its loop (no USB, UART, guards, OCT+/OCT- setup) */
void tab5_power_on(void)
{
    persist_boot();
    settings_init();
    lcd_init();
    sloop_splash();
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    felucca_dbg.magic = DBG_MAGIC;
    felucca_dbg.boots++;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    felucca_init();
    audio_init();
    tab5_boot_ms = fm1_ms + 930u;                /* the logo stays a moment (main.c: 30 + 900 ms) */
    tab5_booted = 1;
}

/* one pass of main.c's loop body (what a device without USB, editor or UBOOT does) */
static void tab5_frame(void)
{
    static int32_t knob = 512 * 16;
    {
        int32_t b = fm1_adc_read(FM1_ADC_BATT);
        if (b > 0)
            song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
    }
    {
        int32_t a = fm1_adc_read(FM1_ADC_MASTER);
        if (a >= 0) {
            uint32_t k10;
            knob += (a * 16 - knob) / 8;
            k10 = (uint32_t)(knob / 16);
            song.master_q12 = (k10 * k10) >> 8;
        }
    }
    felucca_dbg.ui_frames++;
    felucca_dbg.page = ui.page;
    felucca_dbg.home = ui.home;
    ui_input();
    ui_leds();
    ui_draw();
    autosave_tick();
    sections_flush();
}

/* ONE millisecond of the device: the clock, the audio halves due by now, the main loop */
/* USB-MIDI event packets (4 bytes: cable/CIN, status, d1, d2, as a little-endian uint32) from another task or core.
 * One producer (the input task); the device drains the ring on ITS thread, so the firmware's own MIDI ring keeps
 * its single producer and single consumer. */
#define T5_MQ 256u
static uint32_t t5_midi_q[T5_MQ];
static uint32_t t5_midi_w, t5_midi_r;
void tab5_midi_in(uint32_t pkt)
{
    uint32_t w = __atomic_load_n(&t5_midi_w, __ATOMIC_RELAXED);
    if (w - __atomic_load_n(&t5_midi_r, __ATOMIC_ACQUIRE) >= T5_MQ)
        return;                                                  /* full: drop (the firmware drops too) */
    t5_midi_q[w % T5_MQ] = pkt;
    __atomic_store_n(&t5_midi_w, w + 1u, __ATOMIC_RELEASE);
}
static void t5_midi_drain(void)
{
    uint32_t r = t5_midi_r;
    while (r != __atomic_load_n(&t5_midi_w, __ATOMIC_ACQUIRE)) {
        midi_in_event(t5_midi_q[r % T5_MQ]);
        r++;
        __atomic_store_n(&t5_midi_r, r, __ATOMIC_RELEASE);
    }
}

void tab5_step_ms(void)
{
    t5_midi_drain();
    fm1_ms++;
    tab5_ticks += 1000u * FM1_TICKS_PER_US;
    tab5_samples_due += FS;
    while (tab5_samples_due >= 1000u * HALF_FRAMES) {          /* an I2S half is due: the audio ISR renders it */
        uint32_t k;
        const int32_t *o = &abuf[tab5_half * HALF_WORDS];
        tab5_samples_due -= 1000u * HALF_FRAMES;
        fm1_alnk0_irq();
        for (k = 0; k < HALF_FRAMES; k++) {
            uint32_t i = (tab5_ring_w++ % TAB5_RING) * 2u;
            tab5_ring[i] = o[2u * k];
            tab5_ring[i + 1] = o[2u * k + 1u];
        }
        tab5_half ^= 1u;
    }
    if (!tab5_booted || (int32_t)(fm1_ms - tab5_boot_ms) < 0)
        return;
    if (tab5_boot_ms) {                                         /* the splash goes, the loop starts */
        tab5_boot_ms = 0;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        tab5_last_frame = fm1_ms - 15u;
    }
    if (fm1_ms - tab5_last_frame >= 15u) {                      /* a frame, then ui_input until 15 ms are over */
        tab5_last_frame = fm1_ms;
        tab5_frame();
    } else {
        ui_input();
    }
}

/* n stereo frames (24 bit left-justified int32, L R L R ...): the device runs until they exist */
void tab5_audio_read(int32_t *dst, uint32_t n)
{
    uint32_t k;
    while (tab5_ring_w - tab5_ring_r < n)
        tab5_step_ms();
    for (k = 0; k < n; k++) {
        uint32_t i = (tab5_ring_r++ % TAB5_RING) * 2u;
        dst[2u * k] = tab5_ring[i];
        dst[2u * k + 1u] = tab5_ring[i + 1u];
    }
}

/* ------------------------------------------------------------- the panel --- */
/* buttons by their printed labels: bit b = B_FX .. B_OCTUP (panel.c); keys: bit k = key k, F3 .. G5 */
void tab5_buttons(uint32_t labels)
{
    uint32_t m = 0, b, old = fm1_in.buttons;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    __atomic_fetch_or(&tab5_pressed, m & ~old, __ATOMIC_ACQ_REL);
    __atomic_fetch_or(&tab5_released, old & ~m, __ATOMIC_ACQ_REL);
    fm1_in.buttons = m;
}
void tab5_keys(uint32_t mask)
{
    uint32_t old = fm1_in.notes;
    mask &= (1u << 27) - 1u;
    __atomic_fetch_or(&tab5_notes_pressed, mask & ~old, __ATOMIC_ACQ_REL);
    fm1_in.notes = mask;
}
void tab5_enc(uint32_t role, int32_t n)                         /* role: EN_SELECT .. EN_K4; + = clockwise */
{
    if (role < NE)
        __atomic_fetch_add(&tab5_enc_steps[panel.enc[role] % FM1_NENC], n * panel.dir[role], __ATOMIC_ACQ_REL);
}
void tab5_master(uint32_t v) { tab5_master_adc = v > 1023u ? 1023u : v; }

/* the LEDs, one bit per control: buttons bit b = the button labelled b, keys bit k = key k */
static uint32_t tab5_led_on(const uint8_t *l, uint32_t id)
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id)
                return (l[c] >> r) & 1u;
    return 0;
}
static uint32_t tab5_led_buttons(const uint8_t *l)
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        m |= tab5_led_on(l, panel.btn[b]) << b;
    return m;
}
static uint32_t tab5_led_keys(const uint8_t *l)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        m |= tab5_led_on(l, 14u + k) << k;
    return m;
}
uint32_t tab5_lit_buttons(void) { return tab5_led_buttons(fm1_led); }
uint32_t tab5_lit_keys(void) { return tab5_led_keys(fm1_led); }
uint32_t tab5_glow_buttons(void) { return tab5_led_buttons(fm1_led_dim); }
uint32_t tab5_glow_keys(void) { return tab5_led_keys(fm1_led_dim); }
uint32_t tab5_back_buttons(void) { return tab5_led_buttons(fm1_led_bg); }
uint32_t tab5_back_keys(void) { return tab5_led_keys(fm1_led_bg); }
uint32_t tab5_backlight_ns(void) { return fm1_led_bg_ns; }

/* ------------------------------------------------------------- the rest --- */
const uint16_t *tab5_screen(void) { return tab5_fb; }           /* 240 x 240 RGB565, byte-swapped (high byte first) */
uint32_t tab5_screen_draws(void) { return tab5_draws; }
uint32_t tab5_now_ms(void) { return fm1_ms; }
uint32_t tab5_playing(void) { return song.playing; }
uint32_t tab5_sample_rate(void) { return FS; }
uint32_t tab5_half_frames(void) { return HALF_FRAMES; }
uint32_t tab5_cpu_q8(void) { return song.cpu_q8; }
void tab5_set_nor(uint8_t *mem) { tab5_nor = mem; }
uint8_t *tab5_nor_image(void) { return tab5_nor; }
uint32_t tab5_nor_size(void) { return TAB5_NOR_SIZE; }
/* the next 4 KB sector written since the last call (its index), or -1: persist these to the flash partition */
int32_t tab5_nor_take_dirty(void)
{
    uint32_t i;
    for (i = 0; i < TAB5_NOR_SIZE / 4096u; i++)
        if (tab5_nor_dirty_map[i >> 3] & (1u << (i & 7u))) {
            tab5_nor_dirty_map[i >> 3] &= (uint8_t)~(1u << (i & 7u));
            return (int32_t)i;
        }
    return -1;
}
uint32_t tab5_flash_write_count(void) { return tab5_flash_writes; }

/* a count of the programmed steps on the three synth tracks (the persistence test reads it) */
uint32_t tab5_pattern_steps(void)
{
    uint32_t k, i, n = 0;
    for (k = 0; k < NPART; k++)
        for (i = 0; i < NSTEP; i++)
            n += trk[k].step[i].n > 0;
    return n;
}

/* ---- text with the firmware's own pixel font (capitals only), for a touch UI to label its controls ---------------
 * big = 0: 16 px high, 1: 32 px (the same glyphs at 2x). Draws into the caller's NATIVE-endian RGB565 buffer. */
int tab5_text_width(const char *s, int big) { return text_w(big ? &FONT_L : &FONT_S, s); }

int tab5_text_draw(uint16_t *dst, int dw, int dh, int x, int y, const char *s, uint16_t color, int big)
{
    const felucca_font_t *f = big ? &FONT_L : &FONT_S;
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), gx, gy, w = f->bw[gi], sw = w >> f->sh;
        const uint8_t *gd = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t i = (gy >> f->sh) * sw + (gx >> f->sh);
                int px = x - (int)f->pad + (int)gx, py = y + (int)gy;
                if (((gd[i >> 3] >> (7u - (i & 7u))) & 1u) && px >= 0 && py >= 0 && px < dw && py < dh)
                    dst[py * dw + px] = color;
            }
        x += f->adv[gi];
    }
    return x;
}
