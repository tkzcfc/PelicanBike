# CI / local build + package for PelicanBike with the official axmol release/2.x cmdline.
# Usage: pwsh tools/ci/build.ps1 -Platform win32|osx|android|wasm|linux [-Arch x64|arm64] [-Out dist]
# Needs $env:AX_ROOT (axmol clone, setup.ps1 run). Android signing: env ANDROID_KEYSTORE_FILE /
# ANDROID_KEYSTORE_PASSWORD / ANDROID_KEY_ALIAS / ANDROID_KEY_PASSWORD (optional; debug key otherwise).
param(
  [Parameter(Mandatory = $true)][ValidateSet('win32', 'osx', 'android', 'wasm', 'linux')][string]$Platform,
  [string]$Arch = '',
  [string]$Out = 'dist'
)
$ErrorActionPreference = 'Stop'
$proj = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$env:AX_ROOT) { throw 'AX_ROOT is not set (clone axmol release/2.x and run setup.ps1)' }
$axmol = Join-Path $env:AX_ROOT 'tools/cmdline/axmol.ps1'

# engine modules the game does not use (smaller + faster builds); the WASM build is single-threaded (no COOP/COEP needed)
$off = '3D', '3D_PHYSICS', 'PHYSICS', 'NAVMESH', 'HTTP', 'WEBSOCKET', 'OPUS', 'EXT_ASSETMANAGER', 'EXT_COCOSTUDIO', 'EXT_DRAGONBONES',
  'EXT_FAIRYGUI', 'EXT_GUI', 'EXT_IMGUI', 'EXT_INSPECTOR', 'EXT_JSONDEFAULT', 'EXT_LUA', 'EXT_PARTICLE3D', 'EXT_PHYSICS_NODE',
  'EXT_SDFGEN', 'EXT_SPINE', 'EXT_VIDEO', 'EXT_EFFEKSEER', 'EXT_LIVE2D', 'VLC_MEDIA'
$xc = @($off | ForEach-Object { "-DAX_ENABLE_$_=OFF" })
$bdir = "build_ci_$Platform"
if ($Platform -ne 'android') { $xc = @("-B$bdir") + $xc }
if ($Platform -eq 'wasm') { $xc += '-DAX_WASM_THREADS=0', '-DAX_WASM_ENABLE_DEVTOOLS=OFF', '-DAX_WASM_INITIAL_MEMORY=256MB' }
if ($Platform -eq 'android' -and $env:ANDROID_KEYSTORE_FILE) {
  $xc += "-PKEY_STORE_FILE=$($env:ANDROID_KEYSTORE_FILE)", "-PKEY_STORE_PASSWORD=$($env:ANDROID_KEYSTORE_PASSWORD)",
  "-PKEY_ALIAS=$($env:ANDROID_KEY_ALIAS)", "-PKEY_PASSWORD=$($env:ANDROID_KEY_PASSWORD)"
}
$args1 = @('-p', $Platform, '-O3', '-d', $proj, '-xc', ($xc -join ','))
if ($Arch) { $args1 += '-a', $Arch }
Write-Host "axmol $($args1 -join ' ')"
& $axmol @args1
if ($LASTEXITCODE) { throw "axmol build failed ($LASTEXITCODE)" }

New-Item -ItemType Directory -Force -Path (Join-Path $proj $Out) | Out-Null
$dist = (Resolve-Path (Join-Path $proj $Out)).Path
$bin = Join-Path $proj "$bdir/bin/PelicanBike"
switch ($Platform) {
  'win32' {
    $exe = Get-ChildItem -Path $bin -Recurse -Filter 'PelicanBike.exe' | Select-Object -First 1
    if (!$exe) { throw "PelicanBike.exe not found under $bin" }
    $stage = Join-Path $dist 'PelicanBike-windows-x64'; Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    Copy-Item $exe.Directory.FullName $stage -Recurse
    Get-ChildItem $stage -Recurse -Include '*.pdb', '*.ilk', '*.exp', '*.lib' | Remove-Item -Force
    Compress-Archive -Path $stage -DestinationPath (Join-Path $dist 'PelicanBike-windows-x64.zip') -Force
  }
  'osx' {
    $app = Get-ChildItem -Path $bin -Recurse -Directory -Filter 'PelicanBike.app' | Select-Object -First 1
    if (!$app) { throw "PelicanBike.app not found under $bin" }
    Push-Location $app.Parent.FullName
    & ditto -c -k --sequesterRsrc --keepParent 'PelicanBike.app' (Join-Path $dist 'PelicanBike-macos.zip')
    Pop-Location
  }
  'android' {
    $apk = Get-ChildItem -Path (Join-Path $proj 'proj.android/app/build/outputs/apk') -Recurse -Filter '*.apk' |
      Where-Object { $_.FullName -match 'release' } | Select-Object -First 1
    if (!$apk) { throw 'release APK not found' }
    Copy-Item $apk.FullName (Join-Path $dist 'PelicanBike-android.apk') -Force
  }
  'wasm' {
    $stage = Join-Path $dist 'PelicanBike-web'; Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $stage | Out-Null
    Copy-Item (Join-Path $bin 'PelicanBike.*') $stage
    Copy-Item (Join-Path $stage 'PelicanBike.html') (Join-Path $stage 'index.html')
    Copy-Item (Join-Path $proj 'tools/wasm_serve.py') $stage
    Compress-Archive -Path $stage -DestinationPath (Join-Path $dist 'PelicanBike-web.zip') -Force
  }
  'linux' {
    $stage = Join-Path $dist 'PelicanBike-linux-x64'; Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    Copy-Item $bin $stage -Recurse
    & tar -czf (Join-Path $dist 'PelicanBike-linux-x64.tar.gz') -C $dist 'PelicanBike-linux-x64'
  }
}
Get-ChildItem $dist | Format-Table Name, Length
