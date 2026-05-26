## ⚡ WinDefCtl v2.0 — Full Defender Engine Kill + RTP/TP Control

**Release: ${DATE}**

---

## 📥 DOWNLOAD

| File | Size | Description |
|------|------|-------------|
| [WinDefCtl.exe](https://github.com/${REPO}/releases/download/${TAG}/WinDefCtl.exe) | ${SIZE_EXE} | Standalone executable — no installer, no dependencies |

Run as **Administrator**.

---

## 🚀 QUICK START

```cmd
WinDefCtl kill          # Kill Defender engine (IFEO + kernel kill, no restart)
WinDefCtl restore       # Restore Defender (remove IFEO + start services)
WinDefCtl rtp off       # Disable Real-Time Protection
WinDefCtl rtp on        # Enable Real-Time Protection
WinDefCtl tp off        # Disable Tamper Protection
WinDefCtl tp on         # Enable Tamper Protection
```

---

## ✅ WHAT'S NEW IN v2.0

**`kill` / `restore` — Full engine kill without restart**

- **IFEO offline hive bypass** — `RegSaveKeyEx` → `RegLoadKey(TempIFEO)` → write `Debugger=systray.exe` → `RegUnLoadKey` → `RegRestoreKey(REG_FORCE_RESTORE)` — bypasses Tamper Protection at kernel level
- **Ring-0 kill via `kvckiller.sys`** — digitally signed driver (service: `wsftprm`), loads without DSE bypass or HVCI restart; IOCTL `0x22201C` on `\\.\Warsaw_PM` terminates `MsMpEng.exe` + `SecurityHealthSystray.exe`; `SecurityHealthService` stopped via SCM
- **Driver embedded in icon** — LZX CAB appended to `.ico` resource, extracted at runtime via FDI in-memory decompression; no file dropped until `kill` is actually called
- **Smart service reuse** — if `wsftprm` already exists with a valid `kvckiller.sys` path (e.g. from KVC), it is reused without overwriting
- **Atomic cleanup** — service stop waits for `SERVICE_STOPPED` state before `DeleteService` (no zombie SCM entries); driver deleted only if WinDefCtl created the service

**Direct2D overlay (replaces console-maximization trick)**

- Full-screen `WS_EX_LAYERED | WS_EX_TOPMOST` window on a dedicated background thread
- DirectWrite: Consolas Bold 80pt, centered, pulsing green (sin-wave cycle ~6s), animated `PLEASE WAIT...` dots
- CRT scanline effect via Direct2D `DrawLine` overlay
- Shown during `rtp`/`tp` UI automation; not shown during `kill`/`restore` (silent operation)

**Build quality**

- `/MT` static CRT — no `vcruntime140.dll` / `MSVCP*.dll` dependency
- No PDB generated in Release build
- `build.ps1` full chain: driver pack → `GenIconSize.h` → MSBuild → timestamp → artifact cleanup

---

## 📋 COMMANDS

```
=== Engine Kill (IFEO + kvckiller.sys) ===
  kill        Terminate Defender engine (IFEO block + kernel kill)
  restore     Re-enable Defender (remove IFEO, start services)

=== Real-Time Protection (UI automation + overlay) ===
  rtp status  Check current RTP status
  rtp on      Enable Real-Time Protection
  rtp off     Disable Real-Time Protection

=== Tamper Protection (UI automation + overlay) ===
  tp status   Check current Tamper Protection state
  tp on       Enable Tamper Protection
  tp off      Disable Tamper Protection
```

---

## 📞 CONTACT & SUPPORT

- **Email**: marek@wesolowski.eu.org
- **Website**: https://kvc.pl
- **GitHub**: https://github.com/${REPO}

---

*Release Date: ${DATE}*
*© WESMAR 2026*
