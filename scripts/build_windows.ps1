# Builds the Infinit Lab Windows app and a ready-to-install Setup.exe.
#
# The script installs what is missing instead of failing: Flutter, the Visual
# Studio C++ toolchain, CMake/Ninja and Inno Setup.  Run it from PowerShell:
#
#   powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1
#
# Options:
#   -SkipInstaller   build the app bundle only
#   -Clean           remove previous build folders first
#   -NoBootstrap     never download or install tools, only use what is present

[CmdletBinding()]
param(
    [switch]$SkipInstaller,
    [switch]$Clean,
    [switch]$NoBootstrap
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Root = Split-Path -Parent $PSScriptRoot
$AppDir = Join-Path $Root 'app'
$CoreDir = Join-Path $Root 'core'
$CoreBuild = Join-Path $CoreDir 'build-windows'
$Toolchain = Join-Path $env:LOCALAPPDATA 'infinit-toolchain'
$ReleaseDir = Join-Path $Root 'release'
$VersionLine = Select-String -Path (Join-Path $AppDir 'pubspec.yaml') -Pattern '^version:' |
    Select-Object -First 1
if (-not $VersionLine) { throw 'Could not read the version from app\pubspec.yaml.' }
$Version = ($VersionLine.Line -replace '^version:\s*', '' -replace '\+.*$', '').Trim()
if (-not $Version) { throw 'The version in app\pubspec.yaml is empty.' }

function Write-Step([string]$Text) {
    Write-Host ""
    Write-Host "==> $Text" -ForegroundColor Cyan
}

function Test-Command([string]$Name) {
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Add-ToPath([string]$Directory) {
    if ((Test-Path $Directory) -and ($env:PATH -notlike "*$Directory*")) {
        $env:PATH = "$Directory;$env:PATH"
    }
}

function Get-RemoteJson([string]$Url) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    return Invoke-RestMethod -Uri $Url -UseBasicParsing
}

function Save-File([string]$Url, [string]$Destination) {
    Write-Host "    downloading $Url"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    New-Item -ItemType Directory -Force -Path (Split-Path $Destination) | Out-Null
    Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing
}

function Ensure-WingetPackage([string]$Id, [string]$Override) {
    if ($NoBootstrap) {
        throw "Missing dependency $Id and -NoBootstrap was used."
    }
    if (-not (Test-Command 'winget')) {
        throw "winget is not available. Install '$Id' manually, then rerun."
    }
    Write-Host "    installing $Id with winget (this can take a while)"
    $wingetArgs = @(
        'install', '--id', $Id, '-e', '--silent',
        '--accept-package-agreements', '--accept-source-agreements'
    )
    if ($Override) { $wingetArgs += @('--override', $Override) }
    & winget @wingetArgs
    if ($LASTEXITCODE -ne 0) {
        throw "winget failed to install $Id (exit $LASTEXITCODE). If it mentions elevation, open PowerShell as administrator and rerun."
    }
}

function Ensure-Flutter {
    $flutter = Get-Command flutter -ErrorAction SilentlyContinue
    if ($flutter) { return $flutter.Source }

    $candidate = Join-Path $Toolchain 'flutter\bin\flutter.bat'
    if (Test-Path $candidate) { return $candidate }

    if ($NoBootstrap) { throw 'Flutter is not installed.' }

    Write-Step 'Installing Flutter SDK'
    $releases = Get-RemoteJson 'https://storage.googleapis.com/flutter_infra_release/releases/releases_windows.json'
    $hash = $releases.current_release.stable
    $entry = $releases.releases | Where-Object { $_.hash -eq $hash } | Select-Object -First 1
    if (-not $entry) { throw 'Could not resolve the current Flutter stable release.' }
    $zip = Join-Path $env:TEMP 'flutter-stable.zip'
    # base_url has no trailing slash and archive has no leading one, so a plain
    # concatenation asks for ".../releasesstable/windows/..." and Google answers
    # with NoSuchKey.
    $archive = $entry.archive.TrimStart('/')
    if (-not $archive.EndsWith('.zip')) {
        throw "The Flutter release metadata returned an unexpected archive: $archive"
    }
    $url = "$($releases.base_url.TrimEnd('/'))/$archive"
    Save-File $url $zip
    Write-Host '    extracting Flutter (several minutes)'
    Expand-Archive -Path $zip -DestinationPath $Toolchain -Force
    Remove-Item $zip -Force
    return (Join-Path $Toolchain 'flutter\bin\flutter.bat')
}

function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $path) { return $path.Trim() }
    return $null
}

function Ensure-VisualStudio {
    $vs = Find-VisualStudio
    if ($vs) { return $vs }
    Write-Step 'Installing Visual Studio Build Tools (C++)'
    Ensure-WingetPackage 'Microsoft.VisualStudio.2022.BuildTools' `
        '--wait --quiet --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
    $vs = Find-VisualStudio
    if (-not $vs) { throw 'Visual Studio C++ tools are still missing after installation.' }
    return $vs
}

function Find-CMake([string]$VisualStudio) {
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $bundled = Join-Path $VisualStudio 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path $bundled) { return $bundled }
    return $null
}

function Find-InnoSetup {
    # The installer ships as "Inno Setup 6", "Inno Setup 7" and so on, and the
    # chocolatey package follows the newest one.
    $roots = @(
        ${env:ProgramFiles(x86)},
        $env:ProgramFiles,
        (Join-Path $env:LOCALAPPDATA 'Programs')
    )
    $found = @()
    foreach ($root in $roots) {
        if (-not $root -or -not (Test-Path $root)) { continue }
        $found += Get-ChildItem -Path $root -Filter 'Inno Setup *' -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { Join-Path $_.FullName 'ISCC.exe' } |
            Where-Object { Test-Path $_ }
    }
    if ($found.Count -eq 0) { return $null }
    return $found | Sort-Object -Descending | Select-Object -First 1
}

if ($Clean) {
    Write-Step 'Cleaning previous builds'
    Remove-Item -Recurse -Force (Join-Path $AppDir 'build\windows') -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $CoreBuild -ErrorAction SilentlyContinue
}

Push-Location $Root
try {
    Write-Step 'Checking the C++ toolchain'
    $visualStudio = Ensure-VisualStudio
    $cmake = Find-CMake $visualStudio
    if (-not $cmake) {
        if ($NoBootstrap) { throw 'CMake was not found.' }
        Ensure-WingetPackage 'Kitware.CMake'
        $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
        if (-not $cmakeCommand) { throw 'CMake is still missing after installation.' }
        $cmake = $cmakeCommand.Source
    }

    Write-Step 'Checking Flutter'
    $flutter = Ensure-Flutter
    $flutterBin = Split-Path $flutter
    Add-ToPath $flutterBin
    Add-ToPath (Join-Path (Split-Path (Split-Path $flutter)) 'bin')
    & $flutter --version
    if ($LASTEXITCODE -ne 0) { throw 'flutter --version failed.' }
    & $flutter config --enable-windows-desktop | Out-Null

    Write-Step 'Building the native engine (odecore.dll)'
    & $cmake -S $CoreDir -B $CoreBuild -A x64 `
        -DODE_BUILD_TESTS=OFF -DODE_BUILD_STATIC=OFF
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & $cmake --build $CoreBuild --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Native engine build failed.' }
    $dll = Join-Path $CoreBuild 'lib\Release\odecore.dll'
    if (-not (Test-Path $dll)) {
        $dll = Get-ChildItem -Path $CoreBuild -Filter odecore.dll -Recurse |
            Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $dll) { throw 'odecore.dll was not produced.' }

    Write-Step 'Preparing the Windows runner'
    if (-not (Test-Path (Join-Path $AppDir 'windows\runner\main.cpp'))) {
        throw 'app\windows is missing. Restore it from version control and rerun.'
    }
    Push-Location $AppDir
    try {
        if (-not (Test-Path '.dart_tool')) {
            & $flutter pub get
        }
        Write-Step 'Building the Flutter app'
        & $flutter build windows --release
        if ($LASTEXITCODE -ne 0) { throw 'flutter build windows failed.' }
    }
    finally {
        Pop-Location
    }

    $bundle = Join-Path $AppDir 'build\windows\x64\runner\Release'
    if (-not (Test-Path $bundle)) { throw "Bundle not found: $bundle" }
    if (-not (Test-Path (Join-Path $bundle 'infinit.exe'))) {
        throw 'infinit.exe was not produced.'
    }
    Copy-Item $dll (Join-Path $bundle 'odecore.dll') -Force

    $juliaSource = Join-Path $Root 'julia'
    if (Test-Path $juliaSource) {
        $juliaTarget = Join-Path $bundle 'julia'
        New-Item -ItemType Directory -Force -Path (Join-Path $juliaTarget 'src') | Out-Null
        Copy-Item (Join-Path $juliaSource 'Project.toml') $juliaTarget -Force
        Copy-Item (Join-Path $juliaSource 'src\*.jl') (Join-Path $juliaTarget 'src') -Force
    }

    Write-Step "App bundle ready: $bundle"
    if ($SkipInstaller) { return }

    Write-Step 'Building the installer'
    $iscc = Find-InnoSetup
    if (-not $iscc) {
        Ensure-WingetPackage 'JRSoftware.InnoSetup'
        $iscc = Find-InnoSetup
    }
    if (-not $iscc) { throw 'ISCC.exe (Inno Setup) was not found.' }
    New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null
    & $iscc `
        "/DAppVersion=$Version" `
        "/DSourceDir=$bundle" `
        "/DOutputDir=$ReleaseDir" `
        (Join-Path $PSScriptRoot 'windows\installer.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Inno Setup failed.' }

    $setup = Join-Path $ReleaseDir "InfinitLab-$Version-Setup.exe"
    Write-Step "Installer ready: $setup"
}
finally {
    Pop-Location
}
