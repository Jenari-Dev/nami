# Nami — Feature Tracker

Living status of every feature. Updated as we go. See STATE.md for milestone
history and research/notes/ for deep dives.

## ✅ Working (verified on device)
- Pipeline: build → Elf Arsenal FTP deploy → bigapp launch (bootstrap→UI).
- XMB shell: category strip, D-pad nav, item lists, IP/port HUD.
- Launch installed games (sceLncUtilLaunchApp + LncAppParam) — CUSA01780 + Yōtei.
- Launch payloads/homebrew (bootstrap request → hbldr).
- Real title names (app.db) + cover art (icon0.png).
- File browser (read/navigate), file ops copy/move/delete (single-window toolbar).
- Desktop: open folder windows, drag, taskbar, start menu, trash delete.

## 🔨 Built — awaiting on-device test
- **Update system (TEST THIS):** version on XMB, auto-check on launch (now
  threaded = no freeze, shows Checking.../Downloading...), banner when an update
  exists, Settings Check/Install, install downloads the new UI from the GitHub
  release + relaunches. About Nami opens an overlay (version/notes/repo).
  Source: github.com/Jenari-Dev/nami ; manifest = raw .../main/version.json ;
  current release v0.1.1 (console ships v0.1.0 so an update always shows for the test).
- TTF typography (DejaVuSans) across the UI.
- Desktop rewrite: click-select/click-open, min/max/close + taskbar buttons,
  resize, item + desktop right-click menus, /data/nami/desktop location,
  touchpad mouse. Original Nami wave bg + logo + icons, Linux-style cursor.
- XMB L1/R1 layout cycling.

## 🐛 Broken / to fix (from user feedback 2026-11-10) — DESKTOP PHASE
- New Folder goes to the DESKTOP instead of the focused window (focus bug);
  folder made in an open window (e.g. PS5 Root) doesn't appear.
- Can't fully test cut/paste because of the above.
- Delete inside Trash doesn't work (removing a trashed item).
- Right-click on a desktop folder shows no menu (copy/cut/rename).
- Can't deselect by clicking whitespace in the explorer window.
- Cursor still looks like a cheap triangle → want a proper arrow.
- Start button: icon only, drop the "NAMI" text.
- USB icon shows a folder, should be a drive.
- Explorer per-window toolbar is cheesy / not needed (use right-click instead).
- Min/Max/Close buttons look cheesy (text) → want real icons.
- Desktop place labels (PS5 Root/data/user/usb0/Trash) are left-aligned →
  center them; Trash should be at TOP; allow moving/reorganizing icons.
- L1/R1 on XMB only moves rows, doesn't visibly change layout (revisit).
- In multiMAN, L1/R1 on the DESKTOP returns to the XMB — adopt that.
- Use the PS5 system on-screen keyboard (sceImeDialog) for naming instead of
  Nami's own keyboard (it already pops up).
- Make text documents on desktop / in any folder (New Text File).

## 🎯 XMB PHASE (current focus) — plan from multiMAN photos (2026-11-10)
multiMAN structure learned: horizontal category row (mmCM/Settings/Photo/Music/
Video/Game/Favorites/Retro/Web), each item has icon + description line; L1/R1 =
"Next/Prev Mode" cycles the GAME-LIST DISPLAY MODE (not the whole layout);
Settings rows show a right-side value; game-info view = cover + title + options +
button-hint bar (X Load / O Exit / [] Game Settings / ^ System Menu / L1 R1 Mode);
bottom bar shows HDD + date. Desktop matched ours (place/app icons, file windows,
mmCM-as-Start, right-click Rename/Delete/Open-in-HEX/Create-Folder).
Build (original icons only):
- [ ] Original category-row icons: GAMES(pad) PAYLOADS(chip) FILES(folder)
      NETWORK(globe) DISC(disc) SETTINGS(gear) POWER(power); later FAVORITES(star),
      RETRO/EMUS(original pixel creature).
- [ ] Small per-option icon by kind (game/payload/folder/setting/update/...).
- [ ] Description line under the selected item.
- [ ] L1/R1 -> list / grid / cover-flow display modes for GAMES/PAYLOADS.
- [ ] Button-hint bar (X Launch / O Back / ^ Options / [] Settings / L1 R1 Mode).
- [ ] Settings rows: right-side value.
- [ ] Bottom status: version + IP + storage + clock.
  SHOTS NEEDED (for layout/structure only — icons will be original):
  1. Main XMB with the FULL category row visible (all columns, in order).
  2. Each category selected, showing its vertical option list (one per category).
  3. Each XMB LAYOUT that L1/R1 cycles through (one shot per layout).
  4. The game-info panel (cover + title + description + button hints).
  5. Any context/sub menus (e.g. the Refresh/Create-Folder popup).
  6. The Settings section(s) with their options.
  7. The file-manager / mMM desktop mode.

## 📋 Planned
- **Home-screen tile / make Nami a PKG (like multiMAN)**: multiMAN was a PKG;
  the PS5 equivalent is a fake-signed app PKG installed via etaHEN/Elf Arsenal,
  which gives a real home-screen icon and launches Nami as its own app (instead
  of borrowing the bigapp slot). Needs PKG tooling (param.json/sfo, eboot,
  icon0, fake-sign). This is the single task that delivers both "home-screen
  shortcut" and "it's a PKG, not an ELF". Next up.
- **PKG Manager** (like PS3 multiMAN/Rebug): browse & install .pkg from USB and
  the PS5 HDD, with select/queue. Install via sceAppInstUtil / the system pkg
  installer (etaHEN/Elf Arsenal already install pkgs). Three sources like Rebug:
  "Standard Package Location", "Package Folder", "PS5 HDD folder".
  - As a Nami page/app: very doable.
  - USER PREFERENCE: reachable as an option inside the PS5 SETTINGS menu (and
    a home-screen page) — NOT a home-screen shortcut/tile. The Settings entry
    (and a real top tab) need SceShellUI (dashboard) plugin injection — advanced.
  - Order: build AFTER the XMB design + desktop are finished.
  - Later: ISO folders (PS2/PSP/PS3 ISO) that list games and launch via the
    system emulators (Rebug-style) — much later, emulator integration.
- **Nami Payload Store** (like multiMAN's bewholeogy/pkg store): a manifest/repo
  of payloads (name, GitHub repo, asset glob, version, changelog, description) +
  an in-Nami browser that downloads from GitHub Releases over HTTPS and installs
  (+ makes home-screen shortcuts). libcurl + OpenSSL are available, so HTTPS to
  GitHub works. No single canonical PS5 "store" site exists today; we'd host the
  manifest ourselves and pull assets from each dev's GitHub releases.
- Disc-drive offline activation (research track — see research/notes/01).
- Web browser in desktop (needs a real engine — research).

## Notes
- All artwork is original; bundled font is DejaVuSans (permissive license).
- Version compare is simple strcmp (fine for x.y.z same-width); improve later.
