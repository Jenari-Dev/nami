# Nami - STATE

_Verified vs claimed is tracked explicitly. "Verified" means the operator
ran it on the 13.20 console and confirmed the stated result._

## Target
- Console firmware: **13.20** (inside the 7.00-13.60 public kernel chain)
- SDK: ps5-payload-dsk at `~/dev/emu/ps5-tools/ps5-payload-sdk`
- Loader: **Elf Arsenal v1.6.23** (etaHEN stack). No 9021 network loader;
  deploy via FTP (:2121) into /data/elf-arsenal/payloadmgr/ + launch on
  console. `make deploy PS5_HOST=<ip>`. (prospero-deploy/9021 does NOT work here.)
- Console IP: 192.168.3.28 (this build host: 192.168.3.35)
- Build host: CachyOS, bundled prospero clang

## Milestones
| ID | What | Status |
|----|------|--------|
| M0 | toast + HTTP status page to PC | **VERIFIED on 13.20 (2026-11-10)** |
| M1 | SDL2 window + pad input | **VERIFIED on 13.20 (2026-11-10)** - first frame + HUD on TV, launched via bigapp from Elf Arsenal |
| M2 | XMB nav (strip + items + HUD) | **built + deployed, awaiting TV check** |
| M3 | title list + launch | not started |
| M3 | settings | not started |
| M4 | desktop / file manager | not started |
| M5 | full web UI | not started |

## M0 detail
- `src/main.c` - sends on-TV toast, then runs HTTP server (blocks).
- `src/http_server.c` - single static page on :8080, close-per-request.
- Build: `make` -> `nami.elf`. Links `-lSceNotification`.
- **Resolved (device):** an Elf-Arsenal-launched blocking payload stays
  resident; the socket stayed up across repeated requests (sub-ms 200s).
  M1 still moves the server to its own thread so the SDL2 UI loop can run
  alongside it.

## Decisions
- UI stack: **SDL2 + Dear ImGui** first, skin into true XMB look later.
- Home-screen presence is a homebrew-enabler stub tile (cannot replace the
  system dashboard); our app IS the XMB once that tile launches it.
- Game launch (M2) uses the system app-launch path - firmware-sensitive,
  the main RE effort of the project.
