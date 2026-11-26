# DOOM for the Wii U GamePad

A working port of classic DOOM to the
[wup-homebrew](https://github.com/ravensclawx/wup-homebrew) Homebrew Menu,
built with [libwup](https://github.com/ravensclawx/wup-homebrew/tree/main/libwup)
and the [doomgeneric](https://github.com/ozkl/doomgeneric) engine (Chocolate
Doom core). It runs on the GamePad's own firmware — no console required.

Status: **working**, running Doom Shareware 1.9 on real hardware.

## Layout

| Path | What |
| --- | --- |
| `doom-wup/` | The app: `app.cfg`, `main.c`, `wup_platform.c`, patched engine sources in `engine/`. Full details in [doom-wup/README.md](doom-wup/README.md). |
| `wup-homebrew/` | The SDK clone. `libwup/` is the runtime and build tools; `wup-menu/` is the Homebrew Menu; `wup-emu/` is the firmware emulator. |
| `doomgeneric/` | Upstream engine clone. |
| `doom-wup/doom1.wad` | Local copy of the Shareware 1.9 WAD currently installed on the pad. |
| `DOOM1.WAD` | The original Shareware 1.1 WAD (kept for reference; not compatible with this engine without patches). |

## Requirements

- LLVM 19 with `clang`, `ld.lld`, `llvm-ar`, `llvm-nm`, `llvm-objcopy`
  (default Windows install: `C:\Program Files\LLVM\bin`).
- Python 3 with `meson==1.5.2`, `ninja` and `Pillow`:
  ```
  python -m pip install meson==1.5.2 ninja pillow
  ```
  Meson 1.12 rejects the Windows prefix in libwup's cross build, so pin 1.5.2.
- A GamePad with the Homebrew Menu installed and Wi-Fi joined.

The SDK clones are **not committed** in this repository. Next to `doom-wup/`, fetch:

```
git clone https://github.com/ravensclawx/wup-homebrew.git
```

`doomgeneric/` is optional — its sources are already vendored (and patched) in
`doom-wup/engine/`.

## Build

```
cd doom-wup
build.cmd              # Windows;PowerShell wrapper that fixes PATH
```

or directly:

```
python ../wup-homebrew/libwup/tools/wupbuild.py .
```

Output: `doom-wup/build/apps/doom.wup` (~202 KB). The first build also compiles
picolibc and compiler-rt once (about a minute).

## Install and play

Send the app with the Netloader (press **Y** in the Homebrew Menu):

```
python ..\wup-homebrew\libwup\tools\wuplink.py build\apps\doom.wup -a <pad-ip>
```

or upload `/apps/doom/doom.wup` over FTP (`ftp://<pad-ip>`, passive mode; FTP is
only available while the menu is running). Put your IWAD in the same folder as
`/apps/doom/doom1.wad`. The app accepts an argument path, then known IWAD names,
then the first `*.wad` in its folder.

**Use Shareware 1.9** (or another modern IWAD). Version 1.1 lacks lumps the
engine expects and is no longer used. No WAD is redistributed here.

### Controls

| Action | Button |
| --- | --- |
| Move / turn | D-pad, left stick (move + strafe), right stick (turn) |
| Strafe | L / R, or left stick left/right |
| Fire | ZR (or touch the screen) |
| Run | ZL |
| Use / open doors | X |
| Confirm in menus | A |
| Back | B |
| Automap | Y |
| Pause | + |
| Quit to Homebrew Menu | HOME |

## Publishing to the App Store

`app.cfg` already has the catalogue fields (`name`, `author`, `version`,
`description`). Add a 64×64 `icon.png` to `doom-wup/` and uncomment the
`icon = icon.png` line before packaging.

The Homebrew App Store uses the libget catalogue format produced by
`wup-menu/tools/mkrepo.py`:

- `repo.json` — package metadata generated from each app's `app.cfg`
- `zips/doom.zip` — containing `apps/doom/doom.wup`
- `packages/doom/icon.png` — your icon

Publish the folder over HTTPS (GitHub Pages works). Because the WAD is not
bundled, the package contains only the app; players add their own IWAD to
`/apps/doom/`. See `wup-menu/tools/mkrepo.py` for the exact schema.

## How it works

- The engine renders 320×200 with a 256-colour palette. `wup_platform.c`
  nearest-neighbour scales it to the GamePad's 854×480 8bpp layer, so no RGB565
  framebuffer is allocated and the whole 1.4 MB of `WUP_BIGMEM` is available.
- Engine code is compiled as Thumb-1 `-Os` to fit `WUP_SPARE` (368 KB); the
  small libwup-facing files stay ARM. `app.cfg` uses `large = yes`,
  `screen8 = yes`, `CMAP256`.
- Local changes to the engine: missing-lump compatibility for older WADs,
  title screen without DEMO lumps, and an ending when a trimmed WAD has no
  next level. With a normal 1.9 WAD all of these are inert.
- `libwup/src/sys.c` has one weak-symbol change (`wupdoom_full_heap`) so this
  app can use the full `WUP_BIGMEM` as heap. Other apps are unaffected.
- Sound is currently disabled; music and SFX data in the WAD are untouched.

## Troubleshooting

- **`unable to allocate 0 MiB of RAM for zone`** — rebuild; the heap fix above
  is required for the 1 MB zone.
- **`W_GetNumForName: ... not found`** — use a Shareware 1.9 IWAD, not 1.1.
- **FTP upload times out** — the pad transfers at ~12 KB/s; raise FileZilla's
  timeout to 120–300 s (or use Windows Explorer FTP). FTP only runs from the
  Homebrew Menu, not while an app is open.
- **Netloader: "not enough space"** — the pad needs the app's size free even to
  replace it; delete partial uploads or duplicate apps over FTP first.
- **Logs** — this build has no networking, so `wuplog.py` shows nothing; fatal
  messages are drawn on the pad's screen.

## Licences

The DOOM engine sources are GPL-2.0-or-later (see `doom-wup/engine/DOOM-LICENSE.txt`).
The libwup glue is GPL-3.0-or-later, matching libwup. No Nintendo code or
firmware is included.
