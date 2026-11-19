param(
    [ValidateSet("Debug", "RelWithDebInfo", "Release")]
    [string]$Configuration = "RelWithDebInfo",
    [ValidateSet("Stable", "Beta")]
    [string]$Channel = "Beta",
    [string]$MusicFrameworkPath = (Join-Path $PSScriptRoot "../../music-framework"),
    [int]$Jobs = 4
)

$ErrorActionPreference = "Stop"
$sourceDir = $PSScriptRoot
$distDir = Join-Path $sourceDir "dist"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
$installationPath = if (Test-Path -LiteralPath $vswhere) {
    & $vswhere -latest -products * -property installationPath
}
$clang = if ($installationPath) {
    Join-Path $installationPath "VC/Tools/Llvm/x64/bin/clang-cl.exe"
}
$ninja = if ($installationPath) {
    Join-Path $installationPath "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
}

if (-not $clang -or -not (Test-Path -LiteralPath $clang)) {
    throw "clang-cl was not found in the latest Visual Studio installation"
}
if (-not $ninja -or -not (Test-Path -LiteralPath $ninja)) {
    throw "Ninja was not found in the latest Visual Studio installation"
}
$MusicFrameworkPath = (Resolve-Path -LiteralPath $MusicFrameworkPath -ErrorAction Stop).Path

New-Item -ItemType Directory -Force -Path $distDir | Out-Null

$variants = @(
    @{ Arch = "AVX2"; Output = "AVX2.dll" },
    @{ Arch = "SSE2"; Output = "SSE.dll" }
)

foreach ($variant in $variants) {
    $arch = $variant.Arch
    $buildDir = Join-Path $sourceDir ("cmake-build-{0}-{1}" -f $Configuration.ToLowerInvariant(), $arch.ToLowerInvariant())

    & cmake -S $sourceDir -B $buildDir -G Ninja `
        "-DCMAKE_BUILD_TYPE=$Configuration" `
        "-DCMAKE_CXX_COMPILER=$clang" `
        "-DCMAKE_MAKE_PROGRAM=$ninja" `
        "-DATLAS_CPU_ARCH=$arch" `
        "-DATLAS_TF2_CHANNEL=$Channel" `
        "-DATLAS_MUSIC_FRAMEWORK=$MusicFrameworkPath"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for $arch" }

    & cmake --build $buildDir --target atlas_tf2 --config $Configuration -j $Jobs
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed for $arch" }

    $builtDll = Join-Path $buildDir "atlas-tf2.dll"
    if (-not (Test-Path -LiteralPath $builtDll)) {
        throw "Expected output was not produced: $builtDll"
    }
    Copy-Item -LiteralPath $builtDll -Destination (Join-Path $distDir $variant.Output) -Force
}

Write-Host "Built both variants in $distDir"
Get-ChildItem -LiteralPath $distDir -Filter *.dll | Select-Object Name, Length, LastWriteTime
