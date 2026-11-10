# Nami - TASKS

## Now (M0 verify)
- [ ] Operator: `make test PS5_HOST=<ps5-ip>`, confirm toast appears on TV.
- [ ] Operator: open `http://<ps5-ip>:8080` from PC, confirm Nami page loads.
- [ ] Record result in STATE.md (verified / failed + logs).

## Next (M1) - findings 2026-11-10 (ref: Rufidj/Nativehbl, cloned to
##   ~/dev/emu/ps5-tools/Nativehbl-ref)
Mechanism learned:
- UI renders with SDL2 (software renderer). SDK ships an empty homebrew
  prefix (target/user/homebrew/{lib,include} both count 0) - SDL2 stack must
  be installed via ps5-payload-dev **pacbrew** (pacman repo). This box is
  CachyOS (pacman present) but pacbrew is NOT configured yet.
- A plain Elf Arsenal payload CANNOT open a video-out window. Nativehbl uses a
  TWO-ELF model: `bootstrap.elf` elevates ucred (kernel_set_ucred_*, needs
  ps5/kernel.h - present) then `hbldr_launch`es the UI elf into a BIGAPP
  context that has SceVideoOut/ScePad. We need the same model.
- `hbldr_launch` + elfldr/pt come from John Tornblom's GPL-3 code (gpl/ in
  Nativehbl). SDK does not provide them. => **Nami must be GPL-3** to reuse,
  which fits (etaHEN, ps5-payload-dev, hbldr all GPL-3). DECISION PENDING.
- Link set (from Nativehbl): libSDL2[_image/_mixer/_ttf], freetype, jansson,
  png/jpeg/webp/vorbis/ogg + -lSceSystemService -lSceUserService -lSceVideoOut
  -lSceAudioOut -lScePad -lSceKeyboard -lSceImeDialog -lkernel_sys -lm -lz ...

Steps:
- [ ] DECISION: license Nami GPL-3 (required for hbldr bigapp launch).
- [ ] Provision SDL2 stack into the SDK via pacbrew (add pacman repo / fetch).
- [ ] Vendor/adapt bigapp bootstrap (bootstrap.elf: elevate + hbldr_launch).
- [ ] Nami UI elf: open full-screen SDL2 window, clear to a colour on the TV.
- [ ] Deploy both elfs via Elf Arsenal; launch bootstrap -> UI shows.
- [ ] Wire controller input (ScePad via SDL_GameController) -> events.
- [ ] Dear ImGui over SDL2; draw a placeholder XMB row + the IP/port HUD box.
- [ ] Move HTTP server onto its own thread so UI + web run together.

## Research backlog
- [ ] How Nativehbl/websrv launch an installed title on recent FW (M2).
- [ ] etaHEN service surface we can call (PKG install, FTP) vs reimplement.
- [ ] Reading the installed-title list (appinfo / app.db path on 13.20).
