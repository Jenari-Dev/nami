# 00 - Toolchain & loading (13.20)

## SDK
- `ps5-payload-dev/sdk` at `~/dev/emu/ps5-tools/ps5-payload-sdk`.
- Bundled clang/clang++/lld under `bin/`; make rules at
  `toolchain/prospero.mk` (defines `$(CC)`, `$(PS5_DEPLOY)`, ldscripts).
- Builds **ELF payloads**, not signed .pkg apps. A real home-screen app
  comes from installing a homebrew-enabler stub package that launches our
  ELF into a bigapp (full-GPU) context.
- Samples used as references: `notify` (sceNotificationSend +
  -lSceNotification), `hello_world`/`hello_cmake` (build flow),
  `list_files`, `install_app`, `http2_get`.

## Loading (VERIFIED mechanism on this console, 2026-11-10)
- This console runs **Elf Arsenal v1.6.23** (etaHEN stack). It does **NOT**
  expose the ps5-payload-dev network ELF loader on 9021 - that port is always
  closed here, which broke `prospero-deploy`. Open ports seen: 2121 (FTP,
  `ftpsrv`), 3232 (debugger - accepts but hangs, not an ELF loader).
- **Deploy = FTP upload + on-console launch:**
  - `make deploy PS5_HOST=<ip>` uploads `nami.elf` to
    `/data/elf-arsenal/payloadmgr/` over FTP (:2121, anonymous).
  - Register in `/data/elf-arsenal/managed-payloads.json` (entry mirrors the
    existing "Local upload (no GitHub source)" ps5_pup_decrypt example:
    name/description/asset_glob/install_filename, auto_update_on_boot:false).
  - Launch from the Elf Arsenal menu on the console (or its web UI).
- FTP is full read/write to the PS5 root (useful for file-manager milestones).
- Each cold boot drops the jailbreak; re-run the exploit chain + etaHEN.

## Firmware
- 13.20 is covered by the public 7.00-13.60 kernel chain (Fargo/"Relapse").
- 14.x is a separate, newer track - not our target.

## Open / to verify on device
- Does a blocking payload stay resident (socket stays open)? -> M1 threading.
- Exact SDL2 prospero build to use for M1.
