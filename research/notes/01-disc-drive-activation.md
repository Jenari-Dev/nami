# 01 - Disc drive activation (offline, never-activated) - RESEARCH

Status: **OPEN RESEARCH PROBLEM. Not a promised feature.** No public tool is
known to activate a never-paired PS5 disc drive offline (confirmed 2026-11-10).

## The problem
User's PS5 (13.20 JB, IP .28) has a disc drive physically installed but it was
never activated (console never signed in to PSN). Goal: activate/pair the drive
offline and fold the capability into Nami.

## Why it's hard (what "activation" actually is)
- The PS5 Blu-ray drive has its own MCU/firmware and is **cryptographically
  paired** to the console's main SoC. The drive won't operate until paired.
- Normal activation contacts **Sony's servers**, which validate the console +
  drive and provision/confirm pairing data that gets stored locally (nvs-class
  storage). This is the same reason a swapped drive needs internet to re-pair.
- Offline, we don't have that server-provisioned blob. To activate without Sony
  we must either (a) complete a pairing handshake that is actually console-local
  (feasible) or (b) forge/replay a Sony-signed blob (needs keys we likely don't
  have -> possibly a hard blocker).

## What exists (grounding)
- John Tornblom's **OffAct** = offline *account* activation (NOT drive). Shows
  offline-activation RE is tractable for *accounts*; drive is a different,
  harder subsystem. (anecdotal: activating account may break PS4 game loading.)
- One anecdotal report: newer Slim CFI-2116 drives may not need the online step;
  older models do. Our console's model/drive unknown - CHECK.
- No verified offline *drive* pairing method found.

## Investigation plan (needs JB kernel access we have)
1. **Characterize current state** (low risk, do first): with Nami/JB, inspect
   what the system says about the unpaired drive - /dev nodes, SceShell/USB
   messages, Syscon/BD error codes, dmesg/klog. Identify the model + drive rev.
2. **Locate the activation code path** in firmware: the module(s) that perform
   drive auth / the online pairing exchange (RE via IDA/ida-pro MCP). Understand
   what the online step sends/receives and what it stores where.
3. **Classify the blocker**: is stored pairing data a Sony-signed blob (hard) or
   a locally-derivable handshake result (implementable)? This decides feasibility.
4. If locally derivable -> implement as a Nami payload (reproduce the handshake,
   write the pairing data). If Sony-signed -> document the wall; likely blocked
   offline without key material.

## Risk / honesty
- May be **unsolvable offline** if pairing requires Sony-signed material.
- Touches low-level drive/syscon state -> investigate read-only first; no writes
  to drive/nvs until the path is fully understood and reversible.
- This is multi-session RE with an uncertain outcome but high payoff if solved.

## Sources
- OffAct (offline account activation, John Tornblom) - X/@goldengames7890 thread.
- glitched.online: "PS5 disc drive doesn't need internet to activate anymore"
  (CFI-2116 Slim, anecdotal; older models still need it).
