# DOOM for the Wii U GamePad

A port of the classic DOOM engine to the [wup-homebrew](https://github.com/ravensclawx/wup-homebrew)
runtime, using [doomgeneric](https://github.com/ozkl/doomgeneric) as the engine
and `libwup` for the screen, input, clock and filesystem.

The engine renders at 320x200 with a 256-colour palette; `wup_platform.c`
scales that to the GamePad's 854x480 8bpp layer (no RGB565 framebuffer is
allocated, so the heap has the whole 1.4 MB of `WUP_BIGMEM`).

## What you need

* The `libwup` SDK (this repository's `../wup-homebrew/libwup`).
* LLVM 19 (clang, lld, llvm-ar, llvm-nm, llvm-objcopy), Python 3 with
  `meson==1.5.2`, `ninja` and `Pillow`. Newer Meson (1.12) rejects the Windows
  prefix in libwup's cross build, so pin the older one:
  `python -m pip install meson==1.5.2 ninja pillow`.
* **A WAD you supply yourself.** Nothing is bundled. A small IWAD works best
  (shareware `doom1.wad`, `freedoom1.wad`, `chex.wad`). Large WADs such as
  `doom2.wad` may exhaust the zone heap; see *Memory* below.
* Currently installed on the pad: **Doom Shareware 1.9**, untrimmed. The engine
  is Chocolate Doom-based, so 1.9 (not 1.1) is the compatible IWAD version.
* `shrink_wad.py` is optional: it can make a smaller WAD (E1M1 only, all music
  kept, demos dropped). `engine/d_main.c` and `engine/g_game.c` are patched so
  a trimmed WAD still shows a title screen and plays an ending. With the full
  1.9 WAD those patches are inert. To make a trimmed copy:
  `python shrink_wad.py <input.wad> <output.wad>`.

## Build

From this folder:

```
python ../wup-homebrew/libwup/tools/wupbuild.py .
```

On Windows, use `python` (not the WindowsApps `python3` stub) and make sure the
LLVM `bin` directory and Python's `Scripts` directory are on `PATH`. The
included `build.cmd` / `build.ps1` does that for you:

```
build.cmd
```

The first build takes a minute: it builds picolibc and compiler-rt once. The
result is `build/apps/doom.wup` (about 200 KB). The engine is compiled as
Thumb-1 `-Os` to fit `WUP_SPARE`; a successful link reports roughly:

```
DOOM (large app): ... code 327 KB at 0xda000, 40 KB free; .bss 238 KB at
0x190000, 1210 KB left in WUP_BIGMEM
```

## Install and run

Copy `doom.wup` and your WAD into one folder on the pad:

```
/apps/doom/doom.wup
/apps/doom/doom1.wad          # Shareware 1.9, untrimmed
```

Then either copy the folder over FTP (`ftp://<pad-ip>`), or send the app with
the Netloader (press **Y** in the Homebrew Menu, then run):

```
python ../wup-homebrew/libwup/tools/wuplink.py build/apps/doom.wup
```

The WAD is looked up in this order:

1. the app argument (`wuplink.py app.wup <path>` / the menu's "open with");
2. `doom1.wad`, `doom.wad`, `doom2.wad`, `freedoom1.wad`, `freedoom2.wad`,
   `freedm.wad`, `chex.wad`, `hacx.wad` in the app folder;
3. the first `*.wad` in the app folder.

If none is found the app says so on screen and waits for **HOME**.

## Controls

| Action | Button |
| --- | --- |
| Move / turn | D-pad, or left stick (move + strafe) + right stick (turn) |
| Strafe | L / R, or left stick left/right |
| Fire | ZR (or touch the screen) |
| Run | ZL |
| Use / open doors | X |
| Confirm in menus | A |
| Back / pause | B |
| Automap | Y |
| Pause | + |
| Quit to the Homebrew Menu | HOME |

Hold **HOME** to leave at any time.

## Memory

`app.cfg` uses `screen8 = yes` and `large = yes`: code lives in `WUP_SPARE`
(368 KB), `.bss` and the heap share the 1.4 MB `WUP_BIGMEM`. The zone heap is
set with `-mb 1` in `main.c` (`WUP_DOOM_MB`), leaving room for the 64 KB
framebuffer and malloc overhead. Raise it only if the build reports enough
free heap and your WAD needs more; lower it if the GamePad runs out of memory.

`libwup/src/sys.c` has one small change for this app: a weak global
`wupdoom_full_heap` lets `sbrk()` use the whole of `WUP_BIGMEM` as heap. Without
it libwup only uses the part *below* the RGB565 framebuffer, which is ~340 KB
here and cannot satisfy a 1 MB zone (`unable to allocate 0 MiB of RAM for
zone`). Other apps are unaffected: the symbol is weak and defaults to 0.

Music is disabled (the engine's platform layer has no sound back end yet),
which saves both RAM and CPU. Sound effects are likewise silent.

## Testing without a pad

The repository's `wup-emu` can run `doom.wup`, but it needs **your own**
`bootrom.bin` and `flash.bin` dumps (not included in git). With those in place:

```
cd ../wup-homebrew/wup-emu
make
python ../wup-homebrew/wup-menu/tools/mkflash.py <your flash.bin> build/wup.bin /apps/doom/doom.wup=build/apps/doom.wup
./wup-emu --flash build/wup.bin --scale 2
```

This build was compiled and linked successfully here; it has not been run on
real GamePad hardware.

## Licence

The DOOM engine sources in `engine/` are GPL-2.0-or-later (see
`engine/DOOM-LICENSE.txt`). The libwup glue in `main.c` and `wup_platform.c`
is GPL-3.0-or-later, matching libwup.
