# Third-Party Notices

Nami is licensed under **GPL-3.0-or-later** (see `LICENSE`).

## Adapted launch backend (`src/gpl/`, `src/bootstrap.c`)

These files are derived from **ps5-payload-dev/websrv** (via the
Rufidj/Nativehbl adaptation) and carry their original copyright headers.

- Original author: **John Törnblom**
- Upstream: https://github.com/ps5-payload-dev/websrv
- License: **GPL-3.0-or-later**
- Files: `src/gpl/pt.{c,h}`, `src/gpl/elfldr.{c,h}`, `src/gpl/hbldr.{c,h}`,
  `src/gpl/fs.h`, `src/gpl/fs_readfile.c`, `src/bootstrap.c`

These implement the "bigapp" homebrew launch path: `bootstrap.elf` elevates
its own credentials and `hbldr_launch`es the Nami UI ELF into a graphics-
capable app context (video-out + pad). This path requires the Homebrew
Launcher base app (PPSA26344) installed on the console.

## Installed-title launch (`src/nami_ui.c`)

The `LncAppParam` struct layout and the `sceLncUtilLaunchApp` launch sequence
(foreground user + skip-update flag) are derived from **LightningMods/Itemzflow**
(GPL-3.0), https://github.com/LightningMods/Itemzflow.

## Bundled font

`assets/nami.ttf` is **DejaVuSans** (DejaVu Fonts, a Bitstream Vera / Arev
derivative), used under the permissive DejaVu Fonts License (free to use,
bundle and redistribute). It is loaded at runtime via SDL2_ttf; if absent the
UI falls back to the built-in 8x8 debug font.

## Toolchain / libraries

Built with the **ps5-payload-dev SDK** and its pacbrew library set (SDL2,
SDL2_ttf, freetype, jansson, etc.), each under its own license.

When distributing Nami binaries, distribute the corresponding source for the
exact build, per GPL-3.
