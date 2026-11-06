#Requires -Version 5.1
[CmdletBinding()]
param(
    [string] $Configuration = 'Release',
    [string] $Platform      = 'x64',
    [string] $Timestamp     = '2030-01-01 00:00:00',
    [switch] $SkipDriverPack    # skip icon-embed + gen_icon_size.h step
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force

$ProjectRoot = $PSScriptRoot
$ProjectName = 'WinDefCtl'
$BinDir      = Join-Path $ProjectRoot 'bin'
$ObjDir      = Join-Path $ProjectRoot 'obj'

# -- Console helpers ----------------------------------------------------------

function Write-Banner {
    param([string]$Text, [string]$Color = 'Cyan')
    $line = '-' * 60
    Write-Host ''
    Write-Host $line -ForegroundColor DarkGray
    Write-Host "  $Text" -ForegroundColor $Color
    Write-Host $line -ForegroundColor DarkGray
}

function Write-Info    ([string]$m) { Write-Host "    $m" -ForegroundColor Cyan    }
function Write-Step    ([string]$m) { Write-Host "    $m" -ForegroundColor DarkGray }
function Write-Ok      ([string]$m) { Write-Host "    $m" -ForegroundColor Green   }
function Write-Fail    ([string]$m) { Write-Host "    $m" -ForegroundColor Red     }

# -- Timestamp utilities -------------------------------------------------------

function Parse-FixedTimestamp ([string]$Value) {
    $styles = [System.Globalization.DateTimeStyles]::AllowWhiteSpaces -bor
              [System.Globalization.DateTimeStyles]::AssumeLocal
    try {
        return [datetime]::Parse(
            $Value,
            [System.Globalization.CultureInfo]::InvariantCulture,
            $styles)
    } catch {
        throw "Invalid -Timestamp '$Value'.  Example: 2030-01-01 00:00:00"
    }
}

function Set-FixedFileTimestamp {
    param(
        [Parameter(Mandatory)] [string[]] $Paths,
        [Parameter(Mandatory)] [datetime] $Value
    )
    foreach ($p in $Paths) {
        if (Test-Path -LiteralPath $p) {
            $item = Get-Item -LiteralPath $p
            $item.CreationTime   = $Value
            $item.LastWriteTime  = $Value
            $item.LastAccessTime = $Value
        }
    }
}

# -- Locate Visual Studio (vswhere + manual fallback) -------------------------

function Get-LatestVsPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
               'Microsoft Visual Studio\Installer\vswhere.exe'

    if (Test-Path -LiteralPath $vswhere) {
        # Try stable releases first, then prereleases
        foreach ($prerelease in @('', '-prerelease')) {
            $args = @('-products', '*',
                      '-requires', 'Microsoft.Component.MSBuild',
                      '-property', 'installationPath',
                      '-latest') + $(if ($prerelease) { @($prerelease) } else { @() })
            $p = & $vswhere @args 2>$null
            if ($p) { return $p.Trim() }
        }
    }

    # Manual fallback - scan known VS installation directories
    foreach ($ver in @('18', '17', '16')) {
        foreach ($base in @(${env:ProgramFiles}, ${env:ProgramFiles(x86)})) {
            $root = Join-Path $base "Microsoft Visual Studio\$ver"
            if (-not (Test-Path $root)) { continue }
            $edition = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
                       Select-Object -First 1
            if ($edition) { return $edition.FullName }
        }
    }

    throw 'Visual Studio with MSBuild component was not found.'
}

function Get-MSBuildPath ([string]$VsRoot) {
    # Prefer amd64-native MSBuild for speed/correctness
    $candidates = @(
        (Get-ChildItem "$VsRoot\MSBuild" -Filter 'MSBuild.exe' -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'amd64' } |
            Select-Object -First 1 -ExpandProperty FullName),
        (Get-ChildItem "$VsRoot\MSBuild" -Filter 'MSBuild.exe' -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName)
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return $c }
    }
    throw "MSBuild.exe not found under: $VsRoot"
}

function Get-DumpbinPath ([string]$VsRoot) {
    $candidates = @(
        (Get-ChildItem "$VsRoot\VC\Tools\MSVC" -Filter 'dumpbin.exe' -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'Hostx64\\x64' } |
            Select-Object -First 1 -ExpandProperty FullName),
        (Get-ChildItem "$VsRoot" -Filter 'dumpbin.exe' -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName)
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return $c }
    }
    return $null
}

function Test-NoCrtDependencies {
    param(
        [Parameter(Mandatory)] [string]$ExePath,
        [string]$DumpbinPath
    )

    Write-Banner '[4] Verify Zero-CRT Imports (dumpbin)'
    if (-not $DumpbinPath -or -not (Test-Path -LiteralPath $DumpbinPath)) {
        Write-Step 'dumpbin.exe not found - skipping import verification.'
        return $true
    }

    $lines = & $DumpbinPath /dependents "$ExePath" 2>&1
    if (-not $lines) {
        Write-Step 'dumpbin output not captured - skipping import verification.'
        return $true
    }

    $inSection = $false
    $deps = [System.Collections.Generic.List[string]]::new()
    foreach ($line in $lines) {
        if ($line -match 'Image has the following dependencies') { $inSection = $true; continue }
        if ($inSection) {
            if ($line -match '^\s*$' -and $deps.Count -gt 0) { break }
            $dll = $line.Trim()
            if ($dll -and $dll -notmatch '^Summary') { $deps.Add($dll) }
        }
    }

    if ($deps.Count -eq 0) {
        Write-Ok 'dumpbin: no external DLL dependencies.'
        return $true
    }

    $forbidden = @('crt', 'msvcp', 'vcruntime', 'ucrtbase')
    $hasForbidden = $false

    Write-Step 'Imported DLLs:'
    foreach ($d in $deps) {
        $bad = $false
        foreach ($f in $forbidden) {
            if ($d.ToLower().Contains($f)) { $bad = $true; break }
        }
        if ($bad) {
            Write-Fail "  $d (FORBIDDEN CRT DEPENDENCY!)"
            $hasForbidden = $true
        } else {
            Write-Ok "  $d"
        }
    }

    if ($hasForbidden) {
        Write-Fail 'Zero-CRT check failed: CRT library found in import table!'
        return $false
    }

    Write-Ok 'Zero-CRT verified: 100% native Windows OS APIs, 0 CRT dependencies!'
    return $true
}

# -- Step 0: Embed 1 + 2 into icon ----------------------
#
# Output: ICON\WinDefCtl.ico  =  [original icon bytes]  +  [LZX-CAB(1, 2)]
# gen_icon_size.h is regenerated with the exact header byte count so the C++
# runtime knows where to seek for the CAB data.

function Build-DriverIcon {
    param(
        [Parameter(Mandatory)] [string] $Path1,
        [Parameter(Mandatory)] [string] $Path2,
        [Parameter(Mandatory)] [string] $BaseIcon,
        [Parameter(Mandatory)] [string] $OutIcon
    )

    foreach ($f in $Path1, $Path2, $BaseIcon) {
        if (-not (Test-Path -LiteralPath $f)) {
            Write-Fail "Required file not found: $f"
            return $false
        }
    }

    $iconBytes   = [System.IO.File]::ReadAllBytes($BaseIcon)
    $headerSize  = $iconBytes.Length

    $tmpDir = [System.IO.Path]::Combine(
        [System.IO.Path]::GetTempPath(),
        "wdc_pack_$([System.IO.Path]::GetRandomFileName())")
    New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null

    try {
        $tmp1 = Join-Path $tmpDir '1'
        $tmp2 = Join-Path $tmpDir '2'
        Copy-Item -LiteralPath $Path1 -Destination $tmp1 -Force
        Copy-Item -LiteralPath $Path2 -Destination $tmp2 -Force

        # DDF: LZX compression, both drivers in one cabinet
        $ddf = @"
.Set CabinetNameTemplate=drivers.cab
.Set DiskDirectoryTemplate=$tmpDir
.Set CompressionType=LZX
.Set CompressionMemory=21
.Set MaxCabinetSize=0
.Set Cabinet=on
.Set Compress=on
"$tmp1" 1
"$tmp2" 2
"@
        $ddfPath = Join-Path $tmpDir 'pack.ddf'
        [System.IO.File]::WriteAllText($ddfPath, $ddf, [System.Text.Encoding]::ASCII)

        $p = Start-Process makecab.exe `
                 -ArgumentList '/F', "`"$ddfPath`"" `
                 -Wait -PassThru -NoNewWindow -WorkingDirectory $tmpDir
        if ($p.ExitCode -ne 0) {
            Write-Fail "makecab.exe failed (exit $($p.ExitCode))"
            return $false
        }

        $cabPath = Join-Path $tmpDir 'drivers.cab'
        if (-not (Test-Path -LiteralPath $cabPath)) {
            Write-Fail "CAB not produced at expected path: $cabPath"
            return $false
        }

        $cabBytes = [System.IO.File]::ReadAllBytes($cabPath)
        $combined = [byte[]]::new($headerSize + $cabBytes.Length)
        [Array]::Copy($iconBytes, 0, $combined, 0,          $headerSize)
        [Array]::Copy($cabBytes,  0, $combined, $headerSize, $cabBytes.Length)

        $outDir = Split-Path -Parent $OutIcon
        if (-not (Test-Path -LiteralPath $outDir)) {
            New-Item -ItemType Directory -Force -Path $outDir | Out-Null
        }
        [System.IO.File]::WriteAllBytes($OutIcon, $combined)

        $size1 = (Get-Item $Path1).Length
        $size2 = (Get-Item $Path2).Length
        Write-Step "1         : $size1 B"
        Write-Step "2         : $size2 B"
        Write-Step "CAB       : $($cabBytes.Length) B (LZX:21)"
        Write-Step "Icon      : $headerSize B header + $($cabBytes.Length) B CAB = $($combined.Length) B total"
        Write-Step "Output    : $OutIcon"

        $genH = @"
// Auto-generated by build.ps1 - do not edit manually.
// Byte count of IcoBuilder\WinDefCtl.ico; CAB data begins at this offset.
#pragma once
#define ICON_HEADER_SIZE $headerSize
"@
        $genHPath = Join-Path $ProjectRoot 'src\GenIconSize.h'
        [System.IO.File]::WriteAllText($genHPath, $genH)
        Write-Step "GenIconSize.h : ICON_HEADER_SIZE = $headerSize"

        return $true
    }
    finally {
        Remove-Item $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ============================================================================
# MAIN
# ============================================================================

$fixedTs    = Parse-FixedTimestamp -Value $Timestamp
$fixedTsStr = $fixedTs.ToString('yyyy-MM-dd HH:mm:ss',
                  [System.Globalization.CultureInfo]::InvariantCulture)
$epoch      = [DateTimeOffset]::new($fixedTs).ToUnixTimeSeconds()
$env:SOURCE_DATE_EPOCH = [string]$epoch

Write-Banner "$ProjectName v2.1 - full build chain"
Write-Step "Configuration : $Configuration | $Platform"
Write-Step "Timestamp     : $fixedTsStr (epoch $epoch)"

$startTime = Get-Date
$ok        = $true

# -- Step 0: Package driver ----------------------------------------------------

if (-not $SkipDriverPack) {
    Write-Banner '[0] Package 1 + 2 into icon'
    $ok = Build-DriverIcon `
        -Path1      (Join-Path $ProjectRoot 'IcoBuilder\1') `
        -Path2      (Join-Path $ProjectRoot 'IcoBuilder\2') `
        -BaseIcon   (Join-Path $ProjectRoot 'IcoBuilder\WinDefCtl.ico') `
        -OutIcon    (Join-Path $ProjectRoot 'ICON\WinDefCtl.ico')
} else {
    Write-Banner '[0] Driver pack SKIPPED (-SkipDriverPack)'
    if (-not (Test-Path (Join-Path $ProjectRoot 'ICON\WinDefCtl.ico'))) {
        Write-Fail 'ICON\WinDefCtl.ico missing - run without -SkipDriverPack first'
        exit 1
    }
}

# -- Step 1: Locate toolchain --------------------------------------------------

if ($ok) {
    Write-Banner '[1] Locate Visual Studio'
    try {
        $vsPath  = Get-LatestVsPath
        $msbuild = Get-MSBuildPath -VsRoot $vsPath
        $dumpbin = Get-DumpbinPath -VsRoot $vsPath
        Write-Step "VS     : $vsPath"
        Write-Step "MSBuild: $msbuild"
        if ($dumpbin) { Write-Step "Dumpbin: $dumpbin" }
    } catch {
        Write-Fail $_
        $ok = $false
    }
}

# -- Step 2: Clean previous obj\ -----------------------------------------------

if ($ok -and (Test-Path -LiteralPath $ObjDir)) {
    Remove-Item $ObjDir -Recurse -Force
    Write-Banner '[2] Cleaned obj\'
} else {
    Write-Banner '[2] Clean obj\ (nothing to remove)'
}

# -- Step 3: MSBuild /t:Rebuild ------------------------------------------------

if ($ok) {
    Write-Banner '[3] MSBuild Rebuild'

    if (-not (Test-Path -LiteralPath $BinDir)) {
        New-Item -ItemType Directory -Path $BinDir | Out-Null
    }

    Push-Location $ProjectRoot
    try {
        & $msbuild "src\$ProjectName.vcxproj" `
            /p:Configuration=$Configuration `
            /p:Platform=$Platform `
            /p:SolutionDir="$ProjectRoot\" `
            /p:SOURCE_DATE_EPOCH=$epoch `
            /t:Rebuild /m /nologo /v:minimal
        if ($LASTEXITCODE -ne 0) {
            Write-Fail "MSBuild exited $LASTEXITCODE"
            $ok = $false
        }
    }
    finally {
        Pop-Location
    }
}

# -- Step 4: Verify Zero-CRT imports (dumpbin) ---------------------------------

if ($ok) {
    $exePath = Join-Path $BinDir "$ProjectName.exe"
    $ok = Test-NoCrtDependencies -ExePath $exePath -DumpbinPath $dumpbin
}

# -- Step 5: Stamp output timestamps -------------------------------------------

if ($ok) {
    Write-Banner '[5] Stamp output'
    $exePath = Join-Path $BinDir "$ProjectName.exe"
    Set-FixedFileTimestamp -Paths @($exePath) -Value $fixedTs
    Write-Step "Timestamps set to: $fixedTsStr"
}

# -- Step 6: Post-build clean --------------------------------------------------

if ($ok) {
    Write-Banner '[6] Post-build clean'

    foreach ($d in @($ObjDir, (Join-Path $ProjectRoot 'x64'))) {
        if (-not (Test-Path -LiteralPath $d)) { continue }
        $removed = $false
        for ($i = 0; $i -lt 5; $i++) {
            try {
                Remove-Item $d -Recurse -Force -ErrorAction Stop
                Write-Step "Removed: $d"
                $removed = $true
                break
            } catch {
                Start-Sleep -Milliseconds 500
            }
        }
        if (-not $removed) { Write-Info "Could not remove $d (locked) - safe to delete manually" }
    }
}

# -- Result --------------------------------------------------------------------

$elapsed = (Get-Date) - $startTime
Write-Host ''
if ($ok) {
    $exePath = Join-Path $BinDir "$ProjectName.exe"
    Write-Host ("  BUILD OK  {0:F2}s  ->  {1}" -f $elapsed.TotalSeconds, $exePath) `
        -ForegroundColor Green
    if (Test-Path -LiteralPath $exePath) {
        $sz = (Get-Item $exePath).Length
        $szKb = [math]::Round($sz / 1KB, 1)
        Write-Host "  Size: $sz B  ($szKb KB)" -ForegroundColor Gray
    }
} else {
    Write-Host "  BUILD FAILED ($([math]::Round($elapsed.TotalSeconds,2))s)" -ForegroundColor Red
    exit 1
}
