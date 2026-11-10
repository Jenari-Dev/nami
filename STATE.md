# Nami - STATE

_Verified vs claimed is tracked explicitly. "Verified" means the operator
ran it on the 13.20 console and confirmed the stated result._

## Target
- Console firmware: **13.20** (inside the 7.00-13.60 public kernel chain)
- SDK: ps5-payload-dsk at `~/dev/emu/ps5-tools/ps5-payload-sdk`
- Loader: etaHEN elfldr, port 9021
- Build host: CachyOS, bundled prospero clang

## Milestones
| ID | What | Status |
|----|------|--------|
| M0 | toast + HTTP status page to PC | **built, NOT yet device-verified** |
| M1 | SDL2 window + pad input | not started |
| M2 | title list + launch | not started |
| M3 | settings | not started |
| M4 | desktop / file manager | not started |
| M5 | full web UI | not started |

## M0 detail
- `src/main.c` - sends on-TV toast, then runs HTTP server (blocks).
- `src/http_server.c` - single static page on :8080, close-per-request.
- Build: `make` -> `nami.elf`. Links `-lSceNotification`.
- **Open question (device):** does elfldr keep a blocking payload resident
  so the socket stays up? If not, M1 moves the server onto its own thread.

## Decisions
- UI stack: **SDL2 + Dear ImGui** first, skin into true XMB look later.
- Home-screen presence is a homebrew-enabler stub tile (cannot replace the
  system dashboard); our app IS the XMB once that tile launches it.
- Game launch (M2) uses the system app-launch path - firmware-sensitive,
  the main RE effort of the project.
