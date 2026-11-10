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
- **Update system (TEST THIS):** version shown on XMB, auto-check on launch,
  banner when update available, Settings → Check for Update / Install Update,
  install downloads new UI + relaunches. (Test server: http://192.168.3.35:8099)
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

## 🎯 XMB PHASE (current focus)
- Real original icons in the category row (Games/Payloads/Files/Network/Disc/
  Settings/Power) and small icons on each option. (building now)
- Match multiMAN's section structure (pending PS3 reference photos from user).

## 📋 Planned
- **Home-screen tile**: needs a small fake-app PKG (Elf Arsenal tile_autoinstall
  only tiles PKGs, not raw payloads). Separate build task.
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
