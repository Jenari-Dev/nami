# Nami (波)

A multiMAN-style homebrew shell for the PlayStation 5. Launches from a
home-screen tile into a full-screen PS3/XMB-style menu: browse and launch
games, change settings, and open a "desktop" with a file manager, start
menu, browsing and editing. It also serves the same desktop as a web UI
reachable from a PC on the LAN.

**Status:** M0 (pipeline bring-up). Target firmware: **13.20**.

## Architecture

```
core/   filesystem, settings, title list, launch, dumping   (shared C/C++)
shell/  SDL2 full-screen XMB + desktop UI                    -> the TV
web/    embedded HTTP server + web UI                        -> the PC
pkg/    homebrew-enabler stub -> home-screen tile
```

Both front-ends drive the same `core`, so the on-TV shell and the PC web UI
stay in lockstep.

## Build

Requires the [ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk)
with `PS5_PAYLOAD_SDK` exported (already set on this box:
`~/dev/emu/ps5-tools/ps5-payload-sdk`).

```
make                          # build nami.elf
make test PS5_HOST=<ps5-ip>   # deploy to etaHEN elfldr (:9021)
```

Then on the PC, open `http://<ps5-ip>:8080`.

## Roadmap

- **M0** pipeline: on-TV toast + status page served to PC  <- here
- **M1** SDL2 window + controller input (XMB column/row nav)
- **M2** installed-title list + launch (on-device RE)
- **M3** settings
- **M4** desktop: file browser (copy/move/delete/edit), start menu
- **M5** full web UI mirroring the desktop
- **M6+** FTP, dumping/backup, hex editor

## Credits / references

- ps5-payload-dev (SDK, websrv launch path), etaHEN (services, elfldr),
  Rufidj/Nativehbl (SDL2 native launcher reference), LightningMods/Itemzflow.
