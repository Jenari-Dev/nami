# Nami - TASKS

## Now (M0 verify)
- [ ] Operator: `make test PS5_HOST=<ps5-ip>`, confirm toast appears on TV.
- [ ] Operator: open `http://<ps5-ip>:8080` from PC, confirm Nami page loads.
- [ ] Record result in STATE.md (verified / failed + logs).

## Next (M1)
- [ ] Cross-compile / locate an SDL2 prospero build (ps5-payload SDL2 port).
- [ ] Open a full-screen SDL2 window, clear to a colour on the TV.
- [ ] Wire controller input (pad -> events).
- [ ] Dear ImGui over SDL2; draw a placeholder XMB row.
- [ ] Move HTTP server onto its own thread so UI + web run together.

## Research backlog
- [ ] How Nativehbl/websrv launch an installed title on recent FW (M2).
- [ ] etaHEN service surface we can call (PKG install, FTP) vs reimplement.
- [ ] Reading the installed-title list (appinfo / app.db path on 13.20).
