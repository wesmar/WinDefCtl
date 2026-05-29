# WinDefCtl — Windows Defender Automation & Control Utility v2.0

**Full Defender engine kill + RTP/TP slider control via UI Automation**

[![Platform](https://img.shields.io/badge/platform-Windows%2011-blue)]()
[![Build](https://img.shields.io/badge/build-Release%20x64-brightgreen)]()
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)

---

## ⚡ What's New in v2.0

v2.0 introduces full Defender **engine kill** without restart — no PowerShell, no WMI, no third-party tools.  
Combines offline IFEO hive manipulation with ring-0 kernel kill via `kvckiller.sys`.

| Command | What it does |
|---------|-------------|
| `WinDefCtl kill` | IFEO block + kernel kill of `MsMpEng.exe` + `SecurityHealthSystray.exe` + SCM stop of `SecurityHealthService` |
| `WinDefCtl restore` | Remove IFEO entries + start `WinDefend` + `SecurityHealthService` + relaunch `SecurityHealthSystray.exe` |
| `WinDefCtl rtp off\|on\|status` | Real-Time Protection toggle via UI Automation (overlay) |
| `WinDefCtl tp off\|on\|status` | Tamper Protection toggle via UI Automation (overlay) |

> **Bonus:** v2.0 ships an **optional PowerShell script edition** — same command palette
> in a single self-contained `.ps1` (~65 KB) with the signed Topaz driver embedded as
> base64 LZX CAB. See [PowerShell Edition](#-powershell-edition-addon) below.

---

## 📥 Download

**[Latest Release → v2.0.0](https://github.com/wesmar/WinDefCtl/releases/tag/v2.0.0)**

Single standalone executable — no installer, no dependencies, no runtime DLLs.  
Run as **Administrator**.

PowerShell edition: [`data/WinDefCtl-v2.ps1`](data/WinDefCtl-v2.ps1) (~65 KB, no compiler needed).

---

## 🚀 Usage

```cmd
WinDefCtl kill              # Kill Defender engine (no restart required)
WinDefCtl restore           # Re-enable Defender engine

WinDefCtl rtp status        # Check Real-Time Protection state
WinDefCtl rtp off           # Disable Real-Time Protection
WinDefCtl rtp on            # Enable Real-Time Protection

WinDefCtl tp status         # Check Tamper Protection state
WinDefCtl tp off            # Disable Tamper Protection
WinDefCtl tp on             # Enable Tamper Protection
```

### Typical Workflow

```cmd
# Kill engine completely (blocks MsMpEng restart via IFEO)
WinDefCtl kill

# ... do your thing ...

# Restore everything
WinDefCtl restore

# Or: just disable RTP for a moment
WinDefCtl rtp off
WinDefCtl rtp on
```

---

## ⚙️ How It Works

### `kill` — Engine Kill (v2.0)

Two-stage, no restart required:

**Stage 1 — IFEO offline hive bypass:**

Direct registry write to `Image File Execution Options` is blocked by Tamper Protection.  
WinDefCtl bypasses this using an offline hive cycle:

1. `RegSaveKeyEx` — dump live `IFEO` key to `%TEMP%\Ifeo.hiv`
2. `RegLoadKey(HKLM, TempIFEO, Ifeo.hiv)` — mount as `HKLM\TempIFEO`
3. Write `Debugger=systray.exe` to `TempIFEO\MsMpEng.exe` (and two secondary targets)
4. `RegUnLoadKey(TempIFEO)` — unmount
5. `RegRestoreKey(IFEO, Ifeo.hiv, REG_FORCE_RESTORE)` — kernel-level force-replace

`REG_FORCE_RESTORE` operates below the Tamper Protection filter. Requires `SE_BACKUP_NAME` + `SE_RESTORE_NAME`.

**Stage 2 — Ring-0 process kill via `kvckiller.sys`:**

`kvckiller.sys` carries a valid **digital signature** (service name: `wsftprm`).  
Loads without DSE bypass, without HVCI restart, without any unsigned-driver prerequisites.

- Driver extracted from embedded LZX CAB (appended to the `.ico` resource at `ICON_HEADER_SIZE` offset)
- Written to `System32\drivers\kvckiller.sys`
- Service registered as `wsftprm` (Microsoft validates this service name)
- IOCTL `0x22201C` on `\\.\Warsaw_PM` → terminates `MsMpEng.exe` + `SecurityHealthSystray.exe`
- `SecurityHealthService` stopped via SCM
- Service cleaned up after use (stop → wait for `SERVICE_STOPPED` → delete)

**Smart service reuse:** if `wsftprm` already exists (e.g. from KVC installation) and points to a valid `kvckiller.sys`, the existing service is reused without overwriting.

### `rtp` / `tp` — UI Automation (v1 behavior, unchanged)

Opens `windowsdefender://threatsettings` with `SW_SHOWMINNOACTIVE` (minimized, not activated).  
Full-screen **Direct2D overlay** covers the desktop immediately — hiding the minimized window from view.  
Toggles the switch via `IUIAutomationTogglePattern`.

**Overlay technical details:**
- `WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE` fullscreen popup, fully opaque black
- Direct2D + DirectWrite rendering: Consolas Bold 80pt, pulsing green, animated dots, CRT scanlines
- Background thread — non-blocking, no window activation
- Released on `Hide()` via `PostMessage(WM_CLOSE)`

### UAC Bypass

Before UI automation, UAC prompts are suppressed:
- Backup `ConsentPromptBehaviorAdmin` + `PromptOnSecureDesktop` to `HKLM\...\UACStatus`
- Set both to 0 (no prompts)
- Automatically restored after operation (or on next run if interrupted)

### Cold Boot Pre-Warming

First run after login: Windows Security components are not yet loaded in memory.  
WinDefCtl detects this via a volatile registry marker at `HKCU\Software\WinDefCtl\WinDefCtl_Warmed`  
and pre-loads the Security window before automation begins (~5-7 seconds, first run only).

---

## 📜 PowerShell Edition (Addon)

[`data/WinDefCtl-v2.ps1`](data/WinDefCtl-v2.ps1) — same command palette as the C++ binary,
in a single self-contained PowerShell script (~65 KB). No compiler, no Visual Studio.
Drop the file anywhere, run from elevated PowerShell.

```powershell
.\WinDefCtl-v2.ps1                  # show help (also: /?, -?, -h, --help, "help")
.\WinDefCtl-v2.ps1 status           # read Defender state (read-only)
.\WinDefCtl-v2.ps1 kill             # IFEO block + BYOVD kernel kill
.\WinDefCtl-v2.ps1 restore          # remove IFEO + start WinDefend
.\WinDefCtl-v2.ps1 rtp off          # Real-Time Protection toggle off
.\WinDefCtl-v2.ps1 tp  on           # Tamper Protection toggle on
```

### How it works

| Layer | Implementation |
|-------|---------------|
| Driver embedding | `kvckiller.sys` packed with `makecab.exe` (LZX) → base64-chunked → inline in `$DriverCabB64` |
| Driver deployment | `expand.exe` decompresses CAB directly to `%SystemRoot%\System32\drivers\kvckiller.sys` |
| SCM lifecycle | `sc.exe create wsftprm type= kernel start= demand` → `sc.exe start` → `sc.exe stop`/`delete` |
| Kernel kill | P/Invoke `CreateFileW(\\.\Warsaw_PM)` + `DeviceIoControl(0x22201C, <PID buffer>)` |
| IFEO bypass | `reg.exe save` → `reg.exe load HKLM\TempIFEO` → `reg.exe add Debugger=systray.exe` → `reg.exe unload` → `reg.exe restore /f` (`REG_FORCE_RESTORE`) |
| UI automation | `System.Windows.Automation` — `TogglePattern` on Real-Time / Tamper / Dev Drive protection toggles |
| UAC bypass | Both `ConsentPromptBehaviorAdmin` and `PromptOnSecureDesktop` packed into a single `UACStatus` DWORD, restored after the toggle |
| Overlay | Fullscreen multi-monitor `System.Windows.Forms.Form`, pulsing "PLEASE WAIT" label (sine-wave grey-to-white, 25 FPS) |
| Cold boot detect | Volatile marker `HKCU\Software\Temp\WinDefCtl_Warmed` |

### Zero trace after `kill`

After the kill flow finishes, all of the following are gone:

- `wsftprm` service (stopped + `DeleteService`)
- `%SystemRoot%\System32\drivers\kvckiller.sys` (deleted)
- `%TEMP%\kk.cab` and `%TEMP%\Ifeo.hiv*` (deleted)

Only IFEO entries remain (intentional — that **is** the active block; `restore` clears them).

### Build the script

```powershell
# Source driver: data\kvckiller.sys
.\generator\build-ps.ps1
# Output: out\WinDefCtl-v2.ps1
```

The generator runs `makecab.exe`, base64-encodes the CAB into 76-char lines and
substitutes into `generator\template.ps1`. No external NuGet packages, no .NET
build tools — pure inbox Windows.

### Limits vs. C++ binary

- Slower startup (~1.5 s PowerShell + .NET cold load vs. ~200 ms native exe).
- Requires `-ExecutionPolicy Bypass` or a signed copy.
- Defender RTP **may** flag the script on disk (CAB-encoded signed driver inside a PS1).
  The compiled C++ binary embeds the same driver as an ICO resource so it sits below most heuristics.
- No D2D overlay — uses WinForms.

---

## 🛠️ Build from Source

**Requirements:** Visual Studio 2022 (v145 toolset), Windows SDK 10.0+

```powershell
# Full chain: embed driver → compile → stamp timestamps → clean artifacts
.\build.ps1

# Skip driver embedding (use existing ICON\WinDefCtl.ico)
.\build.ps1 -SkipDriverPack

# Custom timestamp (for reproducible builds)
.\build.ps1 -Timestamp "2030-01-01 00:00:00"
```

**Build chain:**
1. `Build-DriverIcon` — `makecab` LZX compresses `IcoBuilder\kvckiller.sys`, appends CAB to `IcoBuilder\WinDefCtl.ico` → `ICON\WinDefCtl.ico`
2. Regenerates `src\GenIconSize.h` with exact icon header byte count
3. MSBuild `Release|x64`: `/MT` (static CRT), LTCG, no PDB
4. Timestamp stamp → `bin\WinDefCtl.exe`
5. Cleans `obj\`

**Output:** `bin\WinDefCtl.exe` (~380 KB, no external dependencies)

---

## 📋 Technical Requirements

- **OS:** Windows 11 (tested on 24H2 / 26H1)
- **Privileges:** Administrator
- **Architecture:** x64
- **Dependencies:** none (static CRT, inbox D2D1/DWrite via system DLLs)

---

## ⚠️ Notes

- `kill` does NOT unload `WinDefend` service — IFEO prevents `MsMpEng.exe` from starting on service start
- `restore` removes the IFEO block then starts `WinDefend` + `SecurityHealthService`
- UI Automation (`rtp`/`tp`) requires an active user session
- Cannot run from Safe Mode or Windows PE
- kvckiller.sys is digitally signed — no HVCI workaround needed

---

## 📞 Contact & Support

| | |
|-|-|
| **Email** | marek@wesolowski.eu.org |
| **Phone** | +48 607-440-283 |
| **Website** | https://kvc.pl |
| **GitHub** | https://github.com/wesmar/WinDefCtl |

**Donations:**
- PayPal: paypal.me/ext1
- Revolut: revolut.me/marekb92

---

## ⚖️ Legal Notice

For **authorized security testing and system administration only**.  
User assumes full legal responsibility for all actions performed.  
Misuse may violate computer crime laws in your jurisdiction.

---

*© 2026 Marek Wesołowski — WESMAR*
