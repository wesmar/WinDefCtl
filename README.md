# WinDefCtl - Windows Defender Automation & Control Utility

**Automated Real-Time Protection and Tamper Protection Management**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)]()
[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()

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
- **Reliable Operation Confirmation** - Structural density detection for UI changes

### Technical Implementation
- **UI Automation API** - No registry or service manipulation
- **Multi-layer Window Hiding** - Opacity control, DWM cloaking, off-screen positioning
- **Smart Timeout Mechanisms** - Extended wait times for slow hardware (10 seconds)
- **Atomic Operations** - Complete success or automatic rollback
- **UAC Recovery System** - Automatic restoration on crash or interruption
- **Multi-language Support** - Should work with non-English Windows installations (tested on Polish Windows)

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

---

## ⚙️ How It Works

### Stealth Window Management ("Ghost Mode")

WinDefCtl opens Windows Security interface completely invisibly using multiple techniques:

1. **Opacity Hack** - Sets window alpha to 0 (invisible)
2. **DWM Cloak** - Hides from window manager and taskbar
3. **Logical Teleport** - Hijacks restore position to off-screen coordinates
4. **Physical Teleport** - Moves window to (-4000, -4000) immediately
5. **Show Without Activate** - Window remains active for automation but hidden

**Note:** Window may briefly flash during operation despite cloaking techniques.

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
- **Compiler:** Visual Studio 2026 (C++20/latest)

---

## ⚠️ Important Notes

### Operation Behavior

- Opens `windowsdefender://threatsettings` URI
- Window is minimized and hidden immediately (40 retries × 250ms = 10 sec timeout)
- UI loading timeout: 100 retries × 100ms = 10 seconds
- All operations are logged to console (DEBUG_LOGGING_ENABLED = 1)

### UAC Registry Keys

Located at `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System`:
- `ConsentPromptBehaviorAdmin` - UAC prompt behavior
- `PromptOnSecureDesktop` - Secure desktop setting
- `UACStatus` - Backup storage (custom key)

### Limitations

- Requires active user session (no headless execution)
- Cannot run from Windows PE or Safe Mode
- System restart may be required for some changes to take full effect

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
