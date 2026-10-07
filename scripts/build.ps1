<#
.SYNOPSIS
  Builds Rockstar Table Tennis for PC from your own Xbox 360 ISO.

.DESCRIPTION
  1. Checks the build tools (Git, CMake, Ninja, LLVM, Visual Studio Build Tools).
  2. Downloads the ReXGlue SDK at a pinned commit and applies this project's patch.
  3. Extracts your ISO (read-only) with extract-xiso.
  4. Recompiles the game and puts the result in dist\Rockstar Table Tennis.

  Your ISO is never modified. No game files are part of this repository.

.PARAMETER Iso
  Path to your Rockstar Table Tennis ISO (title ID 545407DF). A file picker opens if omitted.
#>
param(
  [string]$Iso
)

$ErrorActionPreference = 'Stop'

$SdkRepo = 'https://github.com/rexglue/rexglue-sdk.git'
$SdkCommit = 'c94f5ebdcb3c9d1a460ca48e04f9758448f8d518'
$XisoUrl = 'https://github.com/XboxDev/extract-xiso/releases/download/build-202609111233/extract-xiso-Win64_Release.zip'

$Root = Split-Path -Parent $PSScriptRoot
$Work = Join-Path $Root 'build'
$SdkDir = Join-Path $Work 'rexglue-sdk'
$SdkInstall = Join-Path $SdkDir 'out\install\win-amd64'
$ProjDir = Join-Path $Work 'project'
$ToolsDir = Join-Path $Work 'tools'
$Dist = Join-Path $Root 'dist\Rockstar Table Tennis'

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }

function Refresh-Path {
  $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
              [Environment]::GetEnvironmentVariable('Path', 'User')
  if (Test-Path 'C:\Program Files\LLVM\bin') { $env:Path = 'C:\Program Files\LLVM\bin;' + $env:Path }
}

function Find-VsInstall {
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) { return $null }
  $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if ($path) { return $path.Trim() }
  return $null
}

# Runs a command inside the Visual Studio x64 developer environment.
function Invoke-InVsEnv([string]$Command) {
  $vs = Find-VsInstall
  $bat = Join-Path $Work 'vsenv_cmd.bat'
  @"
@echo off
call "$vs\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set PATH=C:\Program Files\LLVM\bin;%PATH%
$Command
"@ | Set-Content -Encoding ascii $bat
  cmd /c "`"$bat`""
  if ($LASTEXITCODE -ne 0) { throw "Command failed: $Command" }
}

# Git on Windows checks symlinks out as small text files; replace them by copies.
function Repair-Symlinks([string]$RepoDir) {
  Push-Location $RepoDir
  try {
    $links = git ls-files -s | Where-Object { $_ -match '^120000 ' } | ForEach-Object { ($_ -split "`t", 2)[1] }
    foreach ($link in $links) {
      if (-not (Test-Path -LiteralPath $link -PathType Leaf)) { continue }
      # Already repaired (real content) or not a link stub: skip.
      if ((Get-Item -LiteralPath $link).Length -gt 512) { continue }
      $target = (Get-Content -LiteralPath $link -Raw)
      if (-not $target -or $target.Trim() -match '[\r\n<>|"*?]') { continue }
      $target = $target.Trim()
      $parent = Split-Path -Parent $link
      if (-not $parent) { $parent = '.' }
      $src = Join-Path $parent $target
      if (Test-Path -LiteralPath $src -PathType Leaf) {
        Copy-Item -LiteralPath $src -Destination $link -Force
      } elseif (Test-Path -LiteralPath $src -PathType Container) {
        Remove-Item -LiteralPath $link -Force
        Copy-Item -LiteralPath $src -Destination $link -Recurse
      }
    }
  } finally { Pop-Location }
}

# --- 0. ISO -------------------------------------------------------------------
if (-not $Iso) {
  Add-Type -AssemblyName System.Windows.Forms
  $dlg = New-Object System.Windows.Forms.OpenFileDialog
  $dlg.Title = 'Select your Rockstar Table Tennis ISO (Xbox 360)'
  $dlg.Filter = 'Xbox 360 disc image (*.iso)|*.iso'
  if ($dlg.ShowDialog() -ne 'OK') { throw 'No ISO selected.' }
  $Iso = $dlg.FileName
}
if (-not (Test-Path -LiteralPath $Iso)) { throw "ISO not found: $Iso" }
New-Item -ItemType Directory -Force $Work, $ToolsDir | Out-Null

# --- 1. Tools -----------------------------------------------------------------
Step 'Checking build tools'
Refresh-Path
$missing = @()
foreach ($t in @(
    @{ cmd = 'git';   id = 'Git.Git' },
    @{ cmd = 'cmake'; id = 'Kitware.CMake' },
    @{ cmd = 'ninja'; id = 'Ninja-build.Ninja' },
    @{ cmd = 'clang'; id = 'LLVM.LLVM' })) {
  if (-not (Get-Command $t.cmd -ErrorAction SilentlyContinue)) { $missing += $t }
}
$needVs = -not (Find-VsInstall)
if ($missing.Count -or $needVs) {
  Write-Host 'Missing tools:' ($missing.id -join ', ') $(if ($needVs) { 'Visual Studio Build Tools (C++)' })
  $answer = Read-Host 'Install them now with winget? (y/n)'
  if ($answer -notmatch '^[oOyY]') { throw 'Install the missing tools, then run the script again.' }
  foreach ($t in $missing) {
    winget install --id $t.id -e --accept-package-agreements --accept-source-agreements
  }
  if ($needVs) {
    winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements `
      --accept-source-agreements --override '--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
  }
  Refresh-Path
}

# --- 2. ReXGlue SDK -----------------------------------------------------------
if (-not (Test-Path (Join-Path $SdkInstall 'bin\rexglue.exe'))) {
  Step 'Downloading the ReXGlue SDK'
  if (-not (Test-Path $SdkDir)) {
    git clone $SdkRepo $SdkDir
  }
  Push-Location $SdkDir
  try {
    git checkout -q $SdkCommit
    git submodule update --init --recursive
    Step 'Applying this project''s patch to the SDK'
    $patch = Join-Path $Root 'patches\rexglue-sdk.patch'
    $ErrorActionPreference = 'Continue'
    git apply --reverse --check --ignore-whitespace $patch 2>$null
    if ($LASTEXITCODE -ne 0) {
      git apply --ignore-whitespace --whitespace=nowarn $patch
      if ($LASTEXITCODE -ne 0) { throw 'The SDK patch does not apply.' }
    }
    $ErrorActionPreference = 'Stop'
  } finally { Pop-Location }
  Repair-Symlinks $SdkDir
  $subs = git -C $SdkDir submodule foreach --recursive --quiet 'echo $displaypath'
  foreach ($s in $subs) { Repair-Symlinks (Join-Path $SdkDir $s) }

  Step 'Building the SDK (10 to 30 minutes)'
  Invoke-InVsEnv "cd /d `"$SdkDir`" && cmake --preset win-amd64 && cmake --build out/build/win-amd64 --config Release --target install"
}
$rexglue = Join-Path $SdkInstall 'bin\rexglue.exe'

# --- 3. Game files ------------------------------------------------------------
Step 'Preparing the project'
New-Item -ItemType Directory -Force $ProjDir | Out-Null
Copy-Item -Path (Join-Path $Root 'project\*') -Destination $ProjDir -Recurse -Force
$assets = Join-Path $ProjDir 'assets'
if (-not (Test-Path (Join-Path $assets 'default.xex'))) {
  Step 'Extracting the ISO (read-only)'
  $xiso = Join-Path $ToolsDir 'artifacts\extract-xiso.exe'
  if (-not (Test-Path $xiso)) {
    $zip = Join-Path $ToolsDir 'extract-xiso.zip'
    Invoke-WebRequest $XisoUrl -OutFile $zip
    Expand-Archive -Force $zip $ToolsDir
  }
  & $xiso -x -d $assets $Iso
  if (-not (Test-Path (Join-Path $assets 'default.xex'))) { throw "default.xex not found after extraction: is this the right ISO?" }
}

# --- 4. Recompile ---------------------------------------------------------------
Step 'Translating the Xbox 360 code to C++'
Push-Location $ProjDir
try {
  & $rexglue codegen tabletennis_manifest.toml
  if ($LASTEXITCODE -ne 0) { throw 'Code translation failed (rexglue codegen).' }
} finally { Pop-Location }

Step 'Building the game'
$buildCmd = "cd /d `"$ProjDir`" && cmake --preset win-amd64-release && cmake --build --preset win-amd64-release"
Invoke-InVsEnv $buildCmd

# --- 4b. Game icon --------------------------------------------------------------
# The icon is not in the repository: take it from your own copy of the game.
$out = Join-Path $ProjDir 'out\build\win-amd64-release'
$ico = Join-Path $ProjDir 'res\tabletennis.ico'
$customIco = Join-Path $Root 'tabletennis.ico'
if (Test-Path $customIco) { Copy-Item $customIco $ico -Force }
if (-not (Test-Path $ico)) {
  Step 'Extracting the game icon'
  $png = Join-Path $Work 'title_icon.png'
  Copy-Item (Join-Path $SdkInstall 'bin\*.dll') $out -Force
  $proc = Start-Process -FilePath (Join-Path $out 'Rockstar Table Tennis.exe') -WorkingDirectory $out `
    -ArgumentList "--export_icon=`"$png`"", '--fullscreen=false', '--discord_enabled=false' -PassThru
  if (-not $proc.WaitForExit(60000)) { $proc.Kill() }
  if (Test-Path $png) {
    Add-Type -AssemblyName System.Drawing
    $src = [System.Drawing.Image]::FromFile($png)
    $sizes = 256, 64, 48, 32, 16
    $images = @()
    foreach ($size in $sizes) {
      $bmp = New-Object System.Drawing.Bitmap $size, $size
      $g = [System.Drawing.Graphics]::FromImage($bmp)
      $g.InterpolationMode = 'HighQualityBicubic'
      $g.DrawImage($src, 0, 0, $size, $size)
      $g.Dispose()
      $ms = New-Object System.IO.MemoryStream
      $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
      $images += , ($ms.ToArray())
      $bmp.Dispose()
    }
    $src.Dispose()
    $stream = New-Object System.IO.MemoryStream
    $w = New-Object System.IO.BinaryWriter $stream
    $w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
      $b = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
      $w.Write([byte]$b); $w.Write([byte]$b); $w.Write([byte]0); $w.Write([byte]0)
      $w.Write([uint16]1); $w.Write([uint16]32)
      $w.Write([uint32]$images[$i].Length); $w.Write([uint32]$offset)
      $offset += $images[$i].Length
    }
    foreach ($img in $images) { $w.Write($img) }
    [System.IO.File]::WriteAllBytes($ico, $stream.ToArray())
    Invoke-InVsEnv $buildCmd
  } else {
    Write-Host 'Could not extract the icon; the game will build without one.' -ForegroundColor Yellow
  }
}

# --- 5. Package -----------------------------------------------------------------
Step 'Creating the game folder'
$out = Join-Path $ProjDir 'out\build\win-amd64-release'
New-Item -ItemType Directory -Force $Dist | Out-Null
Copy-Item (Join-Path $out 'Rockstar Table Tennis.exe') $Dist -Force
Copy-Item (Join-Path $out '*.dll') $Dist -Force
Copy-Item (Join-Path $SdkInstall 'bin\*.dll') $Dist -Force
if (-not (Test-Path (Join-Path $Dist 'tabletennis.toml'))) {
  Copy-Item (Join-Path $Root 'config\tabletennis.toml') $Dist
}
$fonts = Join-Path $Root 'fonts'
if (Test-Path $fonts) { Copy-Item $fonts $Dist -Recurse -Force }
$game = Join-Path $Dist 'game'
if (-not (Test-Path (Join-Path $game 'default.xex'))) {
  robocopy $assets $game /E /MOVE /NFL /NDL /NJH /NJS | Out-Null
}

Write-Host "`nDone! Run: $Dist\Rockstar Table Tennis.exe" -ForegroundColor Green
