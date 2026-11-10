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

## Loading
- etaHEN runs an ELF loader (elfldr) on **port 9021**.
- Deploy: `make test PS5_HOST=<ip>` -> `$(PS5_DEPLOY) -h <ip> -p 9021 nami.elf`.
- Each cold boot drops the jailbreak; re-run the exploit chain, then elfldr.

## Firmware
- 13.20 is covered by the public 7.00-13.60 kernel chain (Fargo/"Relapse").
- 14.x is a separate, newer track - not our target.

## Open / to verify on device
- Does a blocking payload stay resident (socket stays open)? -> M1 threading.
- Exact SDL2 prospero build to use for M1.
