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

## Scope — the AIO PS5 toolbox

Nami aims to be the multiMAN of the PS5 jailbreak: **one app** that replaces
the pile of single-purpose ELFs you currently run one at a time. Everything
multiMAN had, plus the modern JB toolbox:

- Package **manager + PKG install queue** (install/background queue/track)
- **Game dumping** and **backup/restore**
- **Payload launcher** (run other ELFs from inside Nami)
- **FTP** server built in
- **Desktop**: file browser (copy/move/delete/edit), start menu, browsing
- **All settings** in one place (fan, kstuff, patches, etc.)
- **XMB-style shell** launched from a home-screen tile
- **On-screen HUD box** (bottom-right, PS3-JB style) showing IP + port
- Everything also reachable from the **PC web UI**
- **Disc-drive activation** (offline, never-paired) -- RESEARCH TRACK, not yet
  known to be solvable; see `research/notes/01-disc-drive-activation.md`

## Roadmap

- **M0** pipeline: on-TV toast + status page served to PC  -- DONE (verified)
- **M1** SDL2 full-screen window + controller input; HTTP server on own thread
- **M2** XMB shell nav (columns/rows) + the IP/port HUD box
- **M3** installed-title list + launch (on-device RE)
- **M4** payload launcher (run other ELFs from inside Nami)
- **M5** settings surface
- **M6** desktop: file browser (copy/move/delete/edit), start menu
- **M7** FTP server + PKG install queue + package manager
- **M8** game dumping + backup/restore
- **M9** full web UI mirroring the desktop
- **M10+** hex editor, long tail, optional dashboard-injected overlay

## Credits / references

- ps5-payload-dev (SDK, websrv launch path), etaHEN (services, elfldr),
  Rufidj/Nativehbl (SDL2 native launcher reference), LightningMods/Itemzflow.
