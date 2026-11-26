/* DOOM for the Wii U GamePad, on libwup + doomgeneric.
 *
 * Put a WAD in the app folder (/apps/doom/ by default) or pass its path as the
 * app argument (wuplink.py <app.wup> <wad path>). See README.md.
 */
#include "wup.h"
#include "doomgeneric.h"
#include "wup_platform.h"

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <sys/dirent.h>

/* libwup's <sys/dirent.h> declares the types but not these functions */
DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);

WUP_APP("DOOM");

/* screen8 app: we never use the RGB565 framebuffer, so let libwup use all of
 * WUP_BIGMEM for malloc() (the weak symbol in libwup/src/sys.c). */
int wupdoom_full_heap = 1;

extern jmp_buf wup_doom_jmp;
extern char wup_doom_error[512];

/* DOOM's zone heap. The GamePad has ~1.4 MB of heap in screen8 mode; a larger
 * zone will not fit. Keep this at 1 unless you know what you are doing. */
#ifndef WUP_DOOM_MB
#define WUP_DOOM_MB "1"
#endif

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void path_join(char *out, size_t n, const char *dir, const char *name)
{
    if (!dir || !*dir) {
        snprintf(out, n, "%s", name);
    } else if (dir[strlen(dir) - 1] == '/') {
        snprintf(out, n, "%s%s", dir, name);
    } else {
        snprintf(out, n, "%s/%s", dir, name);
    }
}

/* The WAD the player supplied. Argument first, then the usual IWAD names in
 * the app folder, then any .wad file in it. */
static int find_wad(char *out, size_t n)
{
    static const char *names[] = {
        "doom1.wad", "doom.wad", "doom2.wad", "freedoom1.wad",
        "freedoom2.wad", "freedm.wad", "chex.wad", "hacx.wad"
    };
    const char *arg = wup_arg();
    const char *dir = wup_app_dir();
    unsigned i;
    size_t len;
    DIR *d;
    struct dirent *e;

    if (arg && *arg) {
        snprintf(out, n, "%s", arg);
        if (file_exists(out)) return 1;
        path_join(out, n, dir, arg);
        if (file_exists(out)) return 1;
    }

    for (i = 0; i < sizeof names / sizeof names[0]; ++i) {
        path_join(out, n, dir, names[i]);
        if (file_exists(out)) return 1;
        if (file_exists(names[i])) {
            snprintf(out, n, "%s", names[i]);
            return 1;
        }
    }

    d = opendir(dir);
    if (d) {
        while ((e = readdir(d)) != NULL) {
            len = strlen(e->d_name);
            if (len > 4 && e->d_type != DT_DIR &&
                strcasecmp(e->d_name + len - 4, ".wad") == 0) {
                path_join(out, n, dir, e->d_name);
                closedir(d);
                return 1;
            }
        }
        closedir(d);
    }

    out[0] = 0;
    return 0;
}

static void notice(const char *title, const char *line1, const char *line2)
{
    wup_screen();                      /* switch back to the firmware-free RGB565 layer */
    gfx_fill(0, 0, GFX_W, GFX_H, RGB(8, 12, 24));
    gfx_vgrad(0, 0, GFX_W, 140, RGB(90, 20, 24), RGB(16, 12, 28));
    gfx_text_c(&font_big, GFX_W / 2, 88, title, RGB(255, 236, 210));
    gfx_text_c(&font_body, GFX_W / 2, 240, line1, RGB(210, 214, 230));
    gfx_text_c(&font_body, GFX_W / 2, 280, line2, RGB(150, 158, 186));
    gfx_present_all();
}

int main(void)
{
    char wad[256];
    char *argv[8];
    int argc = 0;

    if (!wup_fs_ok()) {
        notice("DOOM", "The GamePad filesystem is not mounted.", "Press HOME to return to the menu.");
        while (!(wup_buttons() & WUP_HOME)) wup_sleep(16);
        return 0;
    }

    if (!find_wad(wad, sizeof wad)) {
        notice("DOOM", "No WAD found in the app folder.",
               "Copy doom1.wad to /apps/doom and try again.");
        while (!(wup_buttons() & WUP_HOME)) wup_sleep(16);
        return 0;
    }

    notice("DOOM", "Loading...", wad);

    argv[argc++] = "doom";
    argv[argc++] = "-mb";
    argv[argc++] = WUP_DOOM_MB;
    argv[argc++] = "-iwad";
    argv[argc++] = wad;
    argv[argc++] = "-nogui";
    argv[argc++] = "-nosound";        /* no audio back end on the pad yet */
    argv[argc] = NULL;

    if (setjmp(wup_doom_jmp)) {
        char msg[600];
        snprintf(msg, sizeof msg, "DOOM stopped: %s", wup_doom_error);
        notice("DOOM", msg, "Press HOME to return to the menu.");
        while (!(wup_buttons() & WUP_HOME)) wup_sleep(16);
        return 0;
    }

    doomgeneric_Create(argc, argv);

    for (;;) {
        if (wup_buttons() & WUP_HOME) break;
        wup_doom_poll_input();
        doomgeneric_Tick();
        wup_sleep(1);                  /* the firmware needs a turn every loop */
    }
    return 0;                          /* libwup fades out and loads the menu */
}
