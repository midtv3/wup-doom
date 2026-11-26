/* doomgeneric <-> libwup: 8bpp framebuffer, GamePad buttons/sticks and the clock.
 *
 * The engine renders 320x200 indexed pixels into DG_ScreenBuffer (CMAP256 is
 * enabled in app.cfg), I_FinishUpdate copies it there and calls DG_DrawFrame;
 * we nearest-neighbour scale it onto the firmware's own 854x480 8bpp layer.
 * No RGB565 framebuffer is used, so all of WUP_BIGMEM stays available to the
 * engine's heap (app.cfg: screen8 = yes).
 */
#include "wup.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "wup_platform.h"

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

/* --- fatal error reporting ------------------------------------------------ */

jmp_buf wup_doom_jmp;
char wup_doom_error[512];

void wup_doom_fail(const char *msg)
{
    if (msg) {
        size_t n = strlen(msg);
        if (n >= sizeof wup_doom_error) n = sizeof wup_doom_error - 1;
        memcpy(wup_doom_error, msg, n);
        wup_doom_error[n] = 0;
    } else {
        wup_doom_error[0] = 0;
    }
    wup_log("DOOM fatal: ");
    wup_log(wup_doom_error);
    longjmp(wup_doom_jmp, 1);          /* back to main(), then to the menu */
}

/* --- video ---------------------------------------------------------------- */

static const uint8_t default_palette[768] = {
    /* a plain grey ramp; I_SetPalette replaces it as soon as the WAD loads */
    0, 1, 2, 3, 4, 5, 6, 7
};

void DG_SetPalette(const uint8_t *palette)
{
    int i;
    for (i = 0; i < 256; ++i)
        wup_palette(i, ((uint32_t)palette[i * 3] << 16) |
                       ((uint32_t)palette[i * 3 + 1] << 8) |
                        (uint32_t)palette[i * 3 + 2]);
}

void DG_Init(void)
{
    wup_screen8();                     /* firmware's 8bpp layer, no extra RAM */
    DG_SetPalette(default_palette);
    wup_present8(0, GFX_H);
}

void DG_DrawFrame(void)
{
    int y, x;
    uint32_t xf, yf, xstep, ystep;
    uint8_t *dst = wup_screen8();
    uint32_t stride = wup_stride8();
    const uint8_t *src = (const uint8_t *)DG_ScreenBuffer;

    if (!src) return;

    xstep = ((uint32_t)DOOMGENERIC_RESX << 16) / GFX_W;
    ystep = ((uint32_t)DOOMGENERIC_RESY << 16) / GFX_H;
    yf = 0;
    for (y = 0; y < GFX_H; ++y) {
        const uint8_t *row = src + (yf >> 16) * DOOMGENERIC_RESX;
        uint8_t *out = dst + (uint32_t)y * stride;
        xf = 0;
        for (x = 0; x < GFX_W; ++x) {
            out[x] = row[xf >> 16];
            xf += xstep;
        }
        yf += ystep;
    }
    wup_present8(0, GFX_H);
}

/* --- clock and sleeping --------------------------------------------------- */

uint32_t DG_GetTicksMs(void)
{
    return wup_us() / 1000;            /* wraps about every 71 minutes */
}

void DG_SleepMs(uint32_t ms)
{
    if (ms > 1000) ms = 1000;
    wup_sleep((int)ms);                /* lets the firmware keep running */
}

void DG_SetWindowTitle(const char *title)
{
    if (title) wup_log(title);
}

/* --- input ---------------------------------------------------------------- */

#define QUEUE_LEN 64
static struct {
    int pressed;
    unsigned char key;
} queue[QUEUE_LEN];
static int q_head, q_tail;

static const unsigned char tracked[] = {
    KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW,
    KEY_STRAFE_L, KEY_STRAFE_R, KEY_USE, KEY_FIRE, KEY_ENTER,
    KEY_ESCAPE, KEY_RSHIFT, KEY_TAB, KEY_PAUSE, KEY_F1, KEY_F2
};
static unsigned char held[256], want[256];

static void queue_key(int pressed, unsigned char key)
{
    int next = (q_tail + 1) % QUEUE_LEN;
    if (next == q_head) return;        /* full: drop it */
    queue[q_tail].pressed = pressed;
    queue[q_tail].key = key;
    q_tail = next;
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    if (q_head == q_tail) return 0;
    *pressed = queue[q_head].pressed;
    *key = queue[q_head].key;
    q_head = (q_head + 1) % QUEUE_LEN;
    return 1;
}

static void setkey(unsigned char key, int on)
{
    want[key] = (unsigned char)(on ? 1 : 0);
}

void wup_doom_poll_input(void)
{
    unsigned b = wup_buttons();
    unsigned i;
    int sx, sy, tx, ty;
    int touched, tx_screen, ty_screen;
    static const int DEAD = 300;       /* WUP_STICK_MAX is 1100 */

    memset(want, 0, sizeof want);

    /* D-pad: turn and move, the classic DOOM layout */
    setkey(KEY_LEFTARROW,  b & WUP_LEFT);
    setkey(KEY_RIGHTARROW, b & WUP_RIGHT);
    setkey(KEY_UPARROW,    b & WUP_UP);
    setkey(KEY_DOWNARROW,  b & WUP_DOWN);

    /* Face buttons: A confirm/use, B back/pause, X use, Y automap */
    setkey(KEY_ENTER,  b & WUP_A);
    setkey(KEY_ESCAPE, b & WUP_B);
    setkey(KEY_USE,    b & WUP_X);
    setkey(KEY_TAB,    b & WUP_Y);

    /* Shoulders: fire, run, strafe */
    setkey(KEY_FIRE,     b & WUP_ZR);
    setkey(KEY_RSHIFT,   b & WUP_ZL);
    setkey(KEY_STRAFE_L, b & WUP_L);
    setkey(KEY_STRAFE_R, b & WUP_R);

    setkey(KEY_PAUSE, b & WUP_PLUS);
    setkey(KEY_F1,    b & WUP_MINUS);  /* nothing bound in vanilla; handy */

    /* Left stick: strafe / move. Right stick: turn. */
    sx = wup_stick(0, 0);
    sy = wup_stick(0, 1);
    tx = wup_stick(1, 0);
    ty = wup_stick(1, 1);
    (void)ty;
    if (sx < -DEAD) setkey(KEY_STRAFE_L, 1);
    if (sx >  DEAD) setkey(KEY_STRAFE_R, 1);
    if (sy < -DEAD) setkey(KEY_DOWNARROW, 1);
    if (sy >  DEAD) setkey(KEY_UPARROW, 1);
    if (tx < -DEAD) setkey(KEY_LEFTARROW, 1);
    if (tx >  DEAD) setkey(KEY_RIGHTARROW, 1);

    /* Touching the screen fires, like a touchscreen DOOM. */
    touched = wup_touch(&tx_screen, &ty_screen);
    if (touched) setkey(KEY_FIRE, 1);

    for (i = 0; i < sizeof tracked; ++i) {
        unsigned char k = tracked[i];
        if (want[k] && !held[k]) queue_key(1, k);
        else if (!want[k] && held[k]) queue_key(0, k);
        held[k] = want[k];
    }
}
