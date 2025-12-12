# WinDefCtl - Windows Defender Automation & Control Utility

**Automated Real-Time Protection and Tamper Protection Management**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2011-blue)]()
[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()

---

## 📦 Available Versions

### PowerShell Script Version

By popular request from the [MyDigitalLife (MDL) community](https://forums.mydigitallife.net/threads/command-line-utility-to-turn-on-off-windows-defender-and-tamper-protection.89900/), a PowerShell script version is now available: **WinDefCtl.ps1**

This script version provides the same core functionality as the compiled utility, allowing users who prefer script-based solutions or need to review the source code directly to manage Windows Defender settings with full transparency.

Both versions offer very similar capabilities for controlling Real-Time Protection and Tamper Protection settings.

---

## 📋 Overview

WinDefCtl is a command-line utility that provides automated control over Windows Defender's Real-Time Protection (RTP) and Tamper Protection settings through UI Automation API. It operates with stealth execution capabilities, making security configuration changes invisible to the user.

**Author:** Marek Wesołowski - WESMAR  
**Contact:** marek@wesolowski.eu.org | +48 607-440-283  
**Website:** https://kvc.pl 
**GitHub:** https://github.com/wesmar/WinDefCtl/

---

## ✨ Key Features

### Core Capabilities
- **Real-Time Protection Control** - Enable/disable/check RTP status
- **Tamper Protection Control** - Enable/disable/check Tamper Protection status
- **Stealth Execution** - Invisible window management using DWM cloaking
- **Automatic UAC Handling** - Temporary UAC suppression with automatic restoration
- **Cold Boot Detection** - Intelligent pre-warming on first run after login
- **Reliable Operation Confirmation** - Structural density detection for UI changes

### Technical Implementation
- **UI Automation API** - No registry or service manipulation
- **Multi-layer Window Hiding** - Opacity control, DWM cloaking, off-screen positioning
- **Smart Timeout Mechanisms** - Extended wait times for slow hardware (10 seconds)
- **Session-Aware Pre-Warming** - Volatile registry markers for optimal performance
- **Atomic Operations** - Complete success or automatic rollback
- **UAC Recovery System** - Automatic restoration on crash or interruption

---

## 🚀 Usage

### Basic Commands

```cmd
# Real-Time Protection
WinDefCtl rtp status        # Check current RTP status
WinDefCtl rtp on            # Enable Real-Time Protection
WinDefCtl rtp off           # Disable Real-Time Protection

# Tamper Protection
WinDefCtl tp status         # Check current Tamper Protection status
WinDefCtl tp on             # Enable Tamper Protection
WinDefCtl tp off            # Disable Tamper Protection
```

### Example Workflow

```cmd
# Check current status
WinDefCtl rtp status
WinDefCtl tp status

# Disable protection for maintenance
WinDefCtl rtp off
WinDefCtl tp off

# Re-enable protection after maintenance
WinDefCtl tp on
WinDefCtl rtp on
```

### First Run After Login (Cold Boot)

On the first execution after user login or logout/login, WinDefCtl performs an automatic pre-warming phase:

```cmd
=== Windows Defender Tamper Protection Control ===
  [*] Opening Windows Defender...
  [*] Cold boot detected - pre-warming Windows Defender...
  [*] Pre-warm window found, waiting for full initialization...
  [*] Closing pre-warm window...
  [*] Retry close with PostMessage...
  [*] Pre-warm complete
  [*] Backing up and disabling UAC prompts...
  [*] Waiting for UI update... [OK]
  [*] Restoring original UAC settings...
  [*] Operation completed.
```

This is normal behavior and ensures reliable operation. Subsequent executions within the same login session will skip the pre-warm phase.

---

## ⚙️ How It Works

### Cold Boot Detection & Pre-Warming

**Why Pre-Warming is Necessary:**

On the first launch after user login, Windows Security UI components are not loaded into memory. While the window appears visually ready, internal components (message loop, event handlers) may not be fully initialized. This causes close messages to be ignored, preventing proper automation.

**Pre-Warming Solution:**

1. **Session Detection** - Checks volatile registry key at `HKCU\Software\WinDefCtl\WinDefCtl_Warmed`
2. **First-Run Detection** - If key doesn't exist, this is the first run after login (cold boot)
3. **Component Loading** - Opens Windows Security window, waits for full initialization (~5 seconds)
4. **Graceful Close** - Closes window using multiple strategies (WM_SYSCOMMAND, PostMessage fallback)
5. **Session Marker** - Sets volatile registry flag (auto-deleted on logout)
6. **Subsequent Runs** - Marker exists = components already in memory = skip pre-warm

This ensures that all Windows Security components are loaded and responsive before actual automation begins.

### Stealth Window Management ("Ghost Mode")

WinDefCtl opens Windows Security interface completely invisibly using multiple techniques:

1. **Opacity Hack** - Sets window alpha to 0 (invisible)
2. **DWM Cloak** - Hides from window manager and taskbar
3. **Logical Teleport** - Hijacks restore position to off-screen coordinates
4. **Physical Teleport** - Moves window to (-4000, -4000) immediately
5. **Show Without Activate** - Window remains active for automation but hidden

### UAC Manipulation

Temporarily modifies registry to suppress UAC prompts:

- **Backup** - Saves original `ConsentPromptBehaviorAdmin` and `PromptOnSecureDesktop` values
- **Disable** - Sets both values to 0 (no prompts)
- **Restore** - Automatically restores original values after operation
- **Recovery** - Detects incomplete operations on startup and auto-restores UAC

### UI Automation Strategy

Uses "Structural Density" approach for reliable operation:

1. **Element Counting** - Counts all UI elements in the window
2. **Baseline Capture** - Records element count before toggle action
3. **Structure Change Detection** - Waits for element count change (warning dialogs appear/disappear)
4. **Confirmation** - Verifies stable state after change

### Toggle Switch Detection

- **First Toggle** - Real-Time Protection (top switch in UI)
- **Last Toggle** - Tamper Protection (bottom switch in UI)
- Uses `IUIAutomationTogglePattern` to interact with switches
- Detects current state before toggling to avoid unnecessary actions

---

## 🛠️ Technical Requirements

- **OS:** Windows 11 (with modern Windows Security interface)
- **Privileges:** Administrator rights required
- **Dependencies:** UI Automation API, DWM API
- **Compiler:** Visual Studio 2022 (C++20)

---

## ⚠️ Important Notes

### Operation Behavior

- Opens `windowsdefender://threatsettings` URI
- Window is minimized and hidden immediately (40 retries × 250ms = 10 sec timeout)
- UI loading timeout: 100 retries × 100ms = 10 seconds
- Cold boot detection adds ~5-7 seconds on first run after login
- All operations are logged to console (DEBUG_LOGGING_ENABLED = 1)

### Registry Keys

**UAC Backup** (HKLM):
Located at `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System`:
- `ConsentPromptBehaviorAdmin` - UAC prompt behavior
- `PromptOnSecureDesktop` - Secure desktop setting
- `UACStatus` - Backup storage (custom key)

**Session Marker** (HKCU):
Located at `HKCU\Software\WinDefCtl`:
- `WinDefCtl_Warmed` - Volatile flag (auto-deleted on logout)
- Used for cold boot detection and pre-warm skip logic

### Limitations

- Requires active user session (no headless execution)
- Cannot run from Windows PE or Safe Mode
- System restart may be required for some changes to take full effect
- Pre-warming adds 5-7 seconds to first execution after login

---

## 📞 Support & Contribution

### Professional Services
For custom modifications, enterprise support, or security consulting:
- **Email:** marek@wesolowski.eu.org
- **Phone:** +48 607-440-283

### Donations
Support this project:
- **PayPal:** paypal.me/ext1
- **Revolut:** revolut.me/marekb92

### Source Code
- **GitHub:** https://github.com/wesmar/WinDefCtl/

---

## 📄 License

This project is released under the MIT License. See LICENSE file for details.

---

## ⚖️ Legal & Ethical Notice

**Intended for authorized security testing and system administration only.**

- User assumes full legal responsibility for all actions performed
- Ensure proper authorization before using on any system
- This tool modifies system security settings - use responsibly
- Misuse may violate computer crime laws in your jurisdiction

**By using this tool, you acknowledge understanding and accept full responsibility.**

---

**Copyright © 2025 Marek Wesołowski - WESMAR. All rights reserved.**
