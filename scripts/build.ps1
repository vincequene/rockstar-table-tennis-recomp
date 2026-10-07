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
  $dlg.Title = 'Choisis ton ISO de Rockstar Table Tennis (Xbox 360)'
  $dlg.Filter = 'Image disque Xbox 360 (*.iso)|*.iso'
  if ($dlg.ShowDialog() -ne 'OK') { throw 'Aucun ISO choisi.' }
  $Iso = $dlg.FileName
}
if (-not (Test-Path -LiteralPath $Iso)) { throw "ISO introuvable : $Iso" }
New-Item -ItemType Directory -Force $Work, $ToolsDir | Out-Null

# --- 1. Tools -----------------------------------------------------------------
Step 'Verification des outils'
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
  Write-Host 'Outils manquants :' ($missing.id -join ', ') $(if ($needVs) { 'Visual Studio Build Tools (C++)' })
  $answer = Read-Host 'Les installer maintenant avec winget ? (o/n)'
  if ($answer -notmatch '^[oOyY]') { throw 'Installe les outils manquants puis relance le script.' }
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
  Step 'Telechargement du ReXGlue SDK'
  if (-not (Test-Path $SdkDir)) {
    git clone $SdkRepo $SdkDir
  }
  Push-Location $SdkDir
  try {
    git checkout -q $SdkCommit
    git submodule update --init --recursive
    Step 'Application des correctifs du projet sur le SDK'
    $patch = Join-Path $Root 'patches\rexglue-sdk.patch'
    $ErrorActionPreference = 'Continue'
    git apply --reverse --check --ignore-whitespace $patch 2>$null
    if ($LASTEXITCODE -ne 0) {
      git apply --ignore-whitespace --whitespace=nowarn $patch
      if ($LASTEXITCODE -ne 0) { throw 'Le correctif du SDK ne s''applique pas.' }
    }
    $ErrorActionPreference = 'Stop'
  } finally { Pop-Location }
  Repair-Symlinks $SdkDir
  $subs = git -C $SdkDir submodule foreach --recursive --quiet 'echo $displaypath'
  foreach ($s in $subs) { Repair-Symlinks (Join-Path $SdkDir $s) }

  Step 'Compilation du SDK (10 a 30 minutes)'
  Invoke-InVsEnv "cd /d `"$SdkDir`" && cmake --preset win-amd64 && cmake --build out/build/win-amd64 --config Release --target install"
}
$rexglue = Join-Path $SdkInstall 'bin\rexglue.exe'

# --- 3. Game files ------------------------------------------------------------
Step 'Preparation du projet'
New-Item -ItemType Directory -Force $ProjDir | Out-Null
Copy-Item -Path (Join-Path $Root 'project\*') -Destination $ProjDir -Recurse -Force
$assets = Join-Path $ProjDir 'assets'
if (-not (Test-Path (Join-Path $assets 'default.xex'))) {
  Step "Extraction de l'ISO (lecture seule)"
  $xiso = Join-Path $ToolsDir 'artifacts\extract-xiso.exe'
  if (-not (Test-Path $xiso)) {
    $zip = Join-Path $ToolsDir 'extract-xiso.zip'
    Invoke-WebRequest $XisoUrl -OutFile $zip
    Expand-Archive -Force $zip $ToolsDir
  }
  & $xiso -x -d $assets $Iso
  if (-not (Test-Path (Join-Path $assets 'default.xex'))) { throw "default.xex introuvable apres extraction : est-ce le bon ISO ?" }
}

# --- 4. Recompile ---------------------------------------------------------------
Step 'Traduction du code Xbox 360 en C++'
Push-Location $ProjDir
try {
  & $rexglue codegen tabletennis_manifest.toml
  if ($LASTEXITCODE -ne 0) { throw 'La traduction du code a echoue (rexglue codegen).' }
} finally { Pop-Location }

Step 'Compilation du jeu'
Invoke-InVsEnv "cd /d `"$ProjDir`" && cmake --preset win-amd64-release && cmake --build --preset win-amd64-release"

# --- 5. Package -----------------------------------------------------------------
Step 'Creation du dossier du jeu'
$out = Join-Path $ProjDir 'out\build\win-amd64-release'
New-Item -ItemType Directory -Force $Dist | Out-Null
Copy-Item (Join-Path $out 'Rockstar Table Tennis.exe') $Dist -Force
Copy-Item (Join-Path $out '*.dll') $Dist -Force
Copy-Item (Join-Path $SdkInstall 'bin\*.dll') $Dist -Force
if (-not (Test-Path (Join-Path $Dist 'tabletennis.toml'))) {
  Copy-Item (Join-Path $Root 'config\tabletennis.toml') $Dist
}
$game = Join-Path $Dist 'game'
if (-not (Test-Path (Join-Path $game 'default.xex'))) {
  robocopy $assets $game /E /MOVE /NFL /NDL /NJH /NJS | Out-Null
}

Write-Host "`nTermine ! Lance : $Dist\Rockstar Table Tennis.exe" -ForegroundColor Green
