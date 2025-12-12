#Requires -RunAsAdministrator
# WinDefCtl.ps1 - Windows Defender Automation & Control Utility
# PowerShell Edition - Real-Time Protection and Tamper Protection Management
# Author: Marek Wesolowski - WESMAR - 2025

param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet('rtp', 'tp')]
    [string]$Command,
    
    [Parameter(Mandatory=$false, Position=1)]
    [ValidateSet('on', 'off', 'status')]
    [string]$Action = 'status'
)

# ============================================================================
# UI Automation Setup
# ============================================================================

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class WinAPI {
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);
    
    [DllImport("user32.dll")]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder text, int count);
    
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);
    
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    
    [DllImport("user32.dll")]
    public static extern bool IsWindow(IntPtr hWnd);
    
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    
    public const uint WM_SYSCOMMAND = 0x0112;
    public const uint SC_CLOSE = 0xF060;
    public const uint WM_CLOSE = 0x0010;
    public const int SW_SHOWMINNOACTIVE = 7;
}
"@

# ============================================================================
# Registry Helper Functions
# ============================================================================

$UAC_REG_PATH = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System"
$VOLATILE_KEY_PATH = "HKCU:\Software\Temp"
$KEY_NOT_EXISTED = 0xFF

function Read-RegistryDword {
    param(
        [string]$Path,
        [string]$Name
    )
    
    try {
        if (Test-Path $Path) {
            $value = Get-ItemProperty -Path $Path -Name $Name -ErrorAction SilentlyContinue
            if ($null -ne $value) {
                return @{
                    Value = $value.$Name
                    Existed = $true
                }
            }
        }
    }
    catch { }
    
    return @{
        Value = 0
        Existed = $false
    }
}

function Write-RegistryDword {
    param(
        [string]$Path,
        [string]$Name,
        [int]$Value
    )
    
    try {
        if (-not (Test-Path $Path)) {
            New-Item -Path $Path -Force | Out-Null
        }
        Set-ItemProperty -Path $Path -Name $Name -Value $Value -Type DWord -Force
        return $true
    }
    catch {
        return $false
    }
}

function Remove-RegistryValue {
    param(
        [string]$Path,
        [string]$Name
    )
    
    try {
        if (Test-Path $Path) {
            Remove-ItemProperty -Path $Path -Name $Name -ErrorAction SilentlyContinue
        }
        return $true
    }
    catch {
        return $false
    }
}

# ============================================================================
# UAC Management Functions
# ============================================================================

function Encode-UACStatus {
    param(
        [int]$CPBA,
        [bool]$CPBAExisted,
        [int]$POSD,
        [bool]$POSDExisted
    )
    
    $cpbaValue = if ($CPBAExisted) { $CPBA -band 0xFF } else { $KEY_NOT_EXISTED }
    $posdValue = if ($POSDExisted) { $POSD -band 0xFF } else { $KEY_NOT_EXISTED }
    
    $encoded = $cpbaValue -bor ($posdValue -shl 8)
    
    return $encoded
}

function Decode-UACStatus {
    param([int]$Encoded)
    
    $cpbaByte = $Encoded -band 0xFF
    $posdByte = ($Encoded -shr 8) -band 0xFF
    
    return @{
        CPBA = if ($cpbaByte -ne $KEY_NOT_EXISTED) { $cpbaByte } else { 0 }
        CPBAExisted = ($cpbaByte -ne $KEY_NOT_EXISTED)
        POSD = if ($posdByte -ne $KEY_NOT_EXISTED) { $posdByte } else { 0 }
        POSDExisted = ($posdByte -ne $KEY_NOT_EXISTED)
    }
}

function Backup-UAC {
    Write-Host "  [*] Backing up and disabling UAC prompts..."
    
    $cpba = Read-RegistryDword -Path $UAC_REG_PATH -Name "ConsentPromptBehaviorAdmin"
    $posd = Read-RegistryDword -Path $UAC_REG_PATH -Name "PromptOnSecureDesktop"
    
    $encoded = Encode-UACStatus -CPBA $cpba.Value -CPBAExisted $cpba.Existed -POSD $posd.Value -POSDExisted $posd.Existed
    
    if (-not (Write-RegistryDword -Path $UAC_REG_PATH -Name "UACStatus" -Value $encoded)) {
        return $false
    }
    
    $success = $true
    $success = $success -and (Write-RegistryDword -Path $UAC_REG_PATH -Name "ConsentPromptBehaviorAdmin" -Value 0)
    $success = $success -and (Write-RegistryDword -Path $UAC_REG_PATH -Name "PromptOnSecureDesktop" -Value 0)
    
    return $success
}

function Restore-UAC {
    Write-Host "  [*] Restoring original UAC settings..."
    
    $backup = Read-RegistryDword -Path $UAC_REG_PATH -Name "UACStatus"
    
    if (-not $backup.Existed) {
        return $false
    }
    
    $decoded = Decode-UACStatus -Encoded $backup.Value
    
    if ($decoded.CPBAExisted) {
        Write-RegistryDword -Path $UAC_REG_PATH -Name "ConsentPromptBehaviorAdmin" -Value $decoded.CPBA | Out-Null
    }
    else {
        Remove-RegistryValue -Path $UAC_REG_PATH -Name "ConsentPromptBehaviorAdmin" | Out-Null
    }
    
    if ($decoded.POSDExisted) {
        Write-RegistryDword -Path $UAC_REG_PATH -Name "PromptOnSecureDesktop" -Value $decoded.POSD | Out-Null
    }
    else {
        Remove-RegistryValue -Path $UAC_REG_PATH -Name "PromptOnSecureDesktop" | Out-Null
    }
    
    Remove-RegistryValue -Path $UAC_REG_PATH -Name "UACStatus" | Out-Null
    return $true
}

function Test-UACBackupExists {
    $backup = Read-RegistryDword -Path $UAC_REG_PATH -Name "UACStatus"
    return $backup.Existed
}

function Recover-UACIfNeeded {
    if (Test-UACBackupExists) {
        Write-Host "  [RECOVERY] Found incomplete UAC backup, restoring..."
        return Restore-UAC
    }
    return $true
}

# ============================================================================
# Cold Boot Detection (Volatile Registry Marker)
# ============================================================================

function Test-ColdBoot {
    # Volatile key in HKCU:\Software\Temp - disappears on logout/reboot
    try {
        $marker = Get-ItemProperty -Path "$VOLATILE_KEY_PATH" -Name "WinDefCtl_Warmed" -ErrorAction SilentlyContinue
        return ($null -eq $marker)
    }
    catch {
        return $true
    }
}

function Set-WarmMarker {
    try {
        # Create volatile registry key - will disappear on session end
        if (-not (Test-Path $VOLATILE_KEY_PATH)) {
            New-Item -Path $VOLATILE_KEY_PATH -Force | Out-Null
        }
        
        # Unfortunately PowerShell doesn't support REG_OPTION_VOLATILE directly
        # We'll use reg.exe for true volatile key creation
        & reg add "HKCU\Software\Temp" /v "WinDefCtl_Warmed" /t REG_DWORD /d 1 /f | Out-Null
        
        return $true
    }
    catch {
        return $false
    }
}

# ============================================================================
# Window Management Functions
# ============================================================================

function Find-SecurityWindow {
    param([int]$MaxRetries = 10)
    
    $script:foundWindow = $null
    
    for ($i = 0; $i -lt $MaxRetries; $i++) {
        $callback = [WinAPI+EnumWindowsProc] {
            param($hwnd, $lParam)
            
            $className = New-Object System.Text.StringBuilder 256
            [WinAPI]::GetClassName($hwnd, $className, 256) | Out-Null
            
            if ($className.ToString() -eq "ApplicationFrameWindow" -and [WinAPI]::IsWindowVisible($hwnd)) {
                $script:foundWindow = $hwnd
                return $false
            }
            return $true
        }
        
        [WinAPI]::EnumWindows($callback, [IntPtr]::Zero) | Out-Null
        
        if ($script:foundWindow) {
            return $script:foundWindow
        }
        
        Start-Sleep -Milliseconds 100
    }
    
    return $null
}

function Close-SecurityWindow {
    param([IntPtr]$WindowHandle)
    
    if ($WindowHandle -eq [IntPtr]::Zero -or -not [WinAPI]::IsWindow($WindowHandle)) {
        return
    }
    
    # Try SetForegroundWindow + SC_CLOSE
    [WinAPI]::SetForegroundWindow($WindowHandle) | Out-Null
    Start-Sleep -Milliseconds 100
    [WinAPI]::SendMessage($WindowHandle, [WinAPI]::WM_SYSCOMMAND, [IntPtr][WinAPI]::SC_CLOSE, [IntPtr]::Zero) | Out-Null
    
    # Wait for window to close
    $closed = $false
    for ($i = 0; $i -lt 30; $i++) {
        if (-not [WinAPI]::IsWindow($WindowHandle)) {
            $closed = $true
            break
        }
        Start-Sleep -Milliseconds 100
    }
    
    # Fallback to WM_CLOSE if needed
    if (-not $closed) {
        [WinAPI]::SendMessage($WindowHandle, [WinAPI]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 1000
    }
}

# ============================================================================
# Pre-Warming for Cold Boot
# ============================================================================

function Invoke-PreWarmDefender {
    Write-Host "  [*] Cold boot detected - pre-warming Windows Defender..."
    
    Start-Process "windowsdefender://threatsettings" -WindowStyle Hidden
    Start-Sleep -Milliseconds 800
    
    $hwnd = Find-SecurityWindow -MaxRetries 10
    
    if ($hwnd) {
        Write-Host "  [*] Pre-warm window found, waiting for full initialization..."
        Start-Sleep -Milliseconds 800
        
        Write-Host "  [*] Closing pre-warm window..."
        Close-SecurityWindow -WindowHandle $hwnd
        
        Set-WarmMarker | Out-Null
        Write-Host "  [*] Pre-warm complete"
        return $true
    }
    
    Write-Host "  [WARN] Pre-warm window not found, continuing anyway..."
    return $false
}

# ============================================================================
# UI Automation Functions
# ============================================================================

function Wait-UILoaded {
    param(
        [System.Windows.Automation.AutomationElement]$RootElement,
        [int]$MaxRetries = 50
    )
    
    for ($i = 0; $i -lt $MaxRetries; $i++) {
        try {
            $descendants = $RootElement.FindAll(
                [System.Windows.Automation.TreeScope]::Descendants,
                [System.Windows.Automation.Condition]::TrueCondition
            )
            
            if ($descendants.Count -gt 10) {
                return $true
            }
        }
        catch { }
        
        Start-Sleep -Milliseconds 100
    }
    
    return $false
}

function Get-ElementCount {
    param([System.Windows.Automation.AutomationElement]$RootElement)
    
    try {
        $descendants = $RootElement.FindAll(
            [System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.Condition]::TrueCondition
        )
        return $descendants.Count
    }
    catch {
        return 0
    }
}

function Wait-StructureChange {
    param(
        [System.Windows.Automation.AutomationElement]$RootElement,
        [int]$BaselineCount,
        [bool]$ExpectIncrease,
        [int]$TimeoutSeconds = 10
    )
    
    Write-Host "  [*] Waiting for UI update..." -NoNewline
    $maxLoops = $TimeoutSeconds * 10
    
    for ($i = 0; $i -lt $maxLoops; $i++) {
        $currentCount = Get-ElementCount -RootElement $RootElement
        
        $structureChanged = if ($ExpectIncrease) { 
            $currentCount -gt $BaselineCount 
        } else { 
            $currentCount -lt $BaselineCount 
        }
        
        if ($structureChanged) {
            Start-Sleep -Milliseconds 200
            $recheckCount = Get-ElementCount -RootElement $RootElement
            
            $stable = if ($ExpectIncrease) { 
                $recheckCount -gt $BaselineCount 
            } else { 
                $recheckCount -lt $BaselineCount 
            }
            
            if ($stable) {
                Write-Host " [OK]"
                return $true
            }
        }
        
        Start-Sleep -Milliseconds 100
    }
    
    Write-Host " [WARN] Timeout."
    return $false
}

function Find-FirstToggleSwitch {
    param([System.Windows.Automation.AutomationElement]$RootElement)
    
    $condition = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        [System.Windows.Automation.ControlType]::Button
    )
    
    $buttons = $RootElement.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)
    
    foreach ($button in $buttons) {
        try {
            $togglePattern = $button.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
            if ($togglePattern) {
                return $button
            }
        }
        catch { }
    }
    
    return $null
}

function Find-LastToggleSwitch {
    param([System.Windows.Automation.AutomationElement]$RootElement)
    
    $condition = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        [System.Windows.Automation.ControlType]::Button
    )
    
    $buttons = $RootElement.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)
    $lastToggle = $null
    
    foreach ($button in $buttons) {
        try {
            $togglePattern = $button.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
            if ($togglePattern) {
                $lastToggle = $button
            }
        }
        catch { }
    }
    
    return $lastToggle
}

# ============================================================================
# Real-Time Protection Functions
# ============================================================================

function Get-RTPStatus {
    param([System.Windows.Automation.AutomationElement]$RootElement)
    
    $button = Find-FirstToggleSwitch -RootElement $RootElement
    if (-not $button) {
        return $null
    }
    
    try {
        $togglePattern = $button.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
        $state = $togglePattern.Current.ToggleState
        $isEnabled = ($state -eq [System.Windows.Automation.ToggleState]::On)
        
        Write-Host "  [*] RTP Status: $(if ($isEnabled) { 'ENABLED' } else { 'DISABLED' })"
        return $isEnabled
    }
    catch {
        return $null
    }
}

function Enable-RTP {
    param([System.Windows.Automation.AutomationElement]$RootElement)
    
    if (-not (Backup-UAC)) {
        return $false
    }
    
    $button = Find-FirstToggleSwitch -RootElement $RootElement
    if (-not $button) {
        Restore-UAC | Out-Null
        return $false
    }
    
    try {
        $togglePattern = $button.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
        $state = $togglePattern.Current.ToggleState
        
        if ($state -eq [System.Windows.Automation.ToggleState]::Off) {
            $baseline = Get-ElementCount -RootElement $RootElement
            $togglePattern.Toggle()
            $result = Wait-StructureChange -RootElement $RootElement -BaselineCount $baseline -ExpectIncrease $false
        }
        else {
            Write-Host "  [*] RTP already enabled"
            $result = $true
        }
        
        Restore-UAC | Out-Null
        return $result
    }
    catch {
        Restore-UAC | Out-Null
        return $false
    }
}

function Disable-RTP {
    param([System.Windows.Automation.AutomationElement]$RootElement)
    
    if (-not (Backup-UAC)) {
        return $false
    }
    
    $button = Find-FirstToggleSwitch -RootElement $RootElement
    if (-not $button) {
        Restore-UAC | Out-Null
        return $false
    }
    
    try {
        $togglePattern = $button.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
        $state = $togglePattern.Current.ToggleState
        
        if ($state -eq [System.Windows.Automation.ToggleState]::On) {
            $baseline = Get-ElementCount -RootElement $RootElement
            $togglePattern.Toggle()
            $result = Wait-StructureChange -RootElement $RootElement -BaselineCount $baseline -ExpectIncrease $true
        }
        else {
            Write-Host "  [*] RTP already disabled"
            $result = $true
        }
        
        Restore-UAC | Out-Null
        return $result
    }
    catch {
        Restore-UAC | Out-Null
        return $false
    }
}

# ============================================================================
# Tamper Protection Functions
# ============================================================================

function Get-TPStatus {
    param([System.Windows.Automation.AutomationElement]$RootElement)
    
    $button = Find-LastToggleSwitch -RootElement $RootElement
    if (-not $button) {
        return $null
    }
    
    try {
        $togglePattern = $button.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
        $state = $togglePattern.Current.ToggleState
        $isEnabled = ($state -eq [System.Windows.Automation.ToggleState]::On)
        
        Write-Host "  [*] Tamper Protection Status: $(if ($isEnabled) { 'ENABLED' } else { 'DISABLED' })"
        return $isEnabled
    }
    catch {
        return $null
    }
}

function Enable-TP {
    param([System.Windows.Automation.AutomationElement]$RootElement)
    
    if (-not (Backup-UAC)) {
        return $false
    }
    
    $button = Find-LastToggleSwitch -RootElement $RootElement
    if (-not $button) {
        Restore-UAC | Out-Null
        return $false
    }
    
    try {
        $togglePattern = $button.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
        $state = $togglePattern.Current.ToggleState
        
        if ($state -eq [System.Windows.Automation.ToggleState]::Off) {
            $baseline = Get-ElementCount -RootElement $RootElement
            $togglePattern.Toggle()
            $result = Wait-StructureChange -RootElement $RootElement -BaselineCount $baseline -ExpectIncrease $false
        }
        else {
            Write-Host "  [*] Tamper Protection already enabled"
            $result = $true
        }
        
        Restore-UAC | Out-Null
        return $result
    }
    catch {
        Restore-UAC | Out-Null
        return $false
    }
}

function Disable-TP {
    param([System.Windows.Automation.AutomationElement]$RootElement)
    
    if (-not (Backup-UAC)) {
        return $false
    }
    
    $button = Find-LastToggleSwitch -RootElement $RootElement
    if (-not $button) {
        Restore-UAC | Out-Null
        return $false
    }
    
    try {
        $togglePattern = $button.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
        $state = $togglePattern.Current.ToggleState
        
        if ($state -eq [System.Windows.Automation.ToggleState]::On) {
            $baseline = Get-ElementCount -RootElement $RootElement
            $togglePattern.Toggle()
            $result = Wait-StructureChange -RootElement $RootElement -BaselineCount $baseline -ExpectIncrease $true
        }
        else {
            Write-Host "  [*] Tamper Protection already disabled"
            $result = $true
        }
        
        Restore-UAC | Out-Null
        return $result
    }
    catch {
        Restore-UAC | Out-Null
        return $false
    }
}

# ============================================================================
# Main Execution Flow
# ============================================================================

Write-Host ""
Write-Host "=== Windows Defender $(if ($Command -eq 'rtp') { 'RTP' } else { 'Tamper Protection' }) Control ===" -ForegroundColor Cyan
Write-Host ""

# Check for incomplete UAC backup from previous crash
Recover-UACIfNeeded | Out-Null

Write-Host "  [*] Opening Windows Defender..."

# Pre-warming on cold boot
if (Test-ColdBoot) {
    Invoke-PreWarmDefender | Out-Null
    Start-Sleep -Milliseconds 800
}

# Open Windows Security
Start-Process "windowsdefender://threatsettings" -WindowStyle Hidden
$hwndSecurity = Find-SecurityWindow -MaxRetries 10

if (-not $hwndSecurity) {
    Write-Host "  [ERROR] Failed to find Windows Security window" -ForegroundColor Red
    exit 1
}

# Get UI Automation root element
try {
    $rootElement = [System.Windows.Automation.AutomationElement]::FromHandle($hwndSecurity)
}
catch {
    Write-Host "  [ERROR] Failed to get automation element" -ForegroundColor Red
    Close-SecurityWindow -WindowHandle $hwndSecurity
    exit 1
}

# Wait for UI to load
if (-not (Wait-UILoaded -RootElement $rootElement -MaxRetries 50)) {
    Write-Host "  [ERROR] Failed to load UI (Timeout on slow system)" -ForegroundColor Red
    Close-SecurityWindow -WindowHandle $hwndSecurity
    exit 1
}

# Execute requested action
$result = $false

if ($Command -eq 'rtp') {
    switch ($Action) {
        'status' { 
            $result = (Get-RTPStatus -RootElement $rootElement) -ne $null
        }
        'on' { 
            $result = Enable-RTP -RootElement $rootElement
        }
        'off' { 
            $result = Disable-RTP -RootElement $rootElement
        }
    }
}
elseif ($Command -eq 'tp') {
    switch ($Action) {
        'status' { 
            $result = (Get-TPStatus -RootElement $rootElement) -ne $null
        }
        'on' { 
            $result = Enable-TP -RootElement $rootElement
        }
        'off' { 
            $result = Disable-TP -RootElement $rootElement
        }
    }
}

# Close security window
Close-SecurityWindow -WindowHandle $hwndSecurity

Write-Host ""
Write-Host "  [*] Operation completed." -ForegroundColor $(if ($result) { 'Green' } else { 'Yellow' })
Write-Host ""

exit $(if ($result) { 0 } else { 1 })
