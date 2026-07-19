<#
.SYNOPSIS
    Copies App's vcpkg DLL dependencies into the build output directory.

.DESCRIPTION
    Replaces vcpkg's AppLocalFromInstalled hook, which is broken on the
    VS2026 Insiders toolset (no CopyFilesToOutputDirectory target exists,
    so AfterTargets="CopyFilesToOutputDirectory" never fires).

    Also replaces applocal.ps1 (dumpbin-based dependency walk), which was
    intermittently failing right after link - probably a transient file
    lock on the freshly-linked exe causing dumpbin to silently return
    nothing on the first PostBuildEvent run.

    Instead, this copies a fixed, known list of DLLs straight from the
    vcpkg installed bin dir, with retries to ride out any lock.

.PARAMETER VcpkgBinDir
    vcpkg installed bin directory (debug or release) for the active triplet.

.PARAMETER OutDir
    Build output directory to copy DLLs into.
#>
param(
    [Parameter(Mandatory)] [string]$VcpkgBinDir,
    [Parameter(Mandatory)] [string]$OutDir
)

$dlls = @(
    "PhysXCommon_64.dll",
    "PhysXCooking_64.dll",
    "PhysXFoundation_64.dll",
    "PhysX_64.dll",
    "SDL3.dll",
    "SDL3_mixer.dll",
    "lua.dll",
    "meshoptimizer.dll",
    "gmock.dll",
    "gtest.dll",
    "rmlui.dll",
    "rmlui_debugger.dll",
    "rmlui_lua.dll",
    # RmlUi's [freetype] feature transitively depends on these. Debug builds
    # use a "d"-suffixed name (vcpkg convention); only one of each pair
    # exists per config, the other is silently skipped by the missing-file
    # check below.
    "freetype.dll",
    "freetyped.dll",
    "zlib1.dll",
    "zlibd1.dll",
    "bz2.dll",
    "bz2d.dll",
    "libpng16.dll",
    "libpng16d.dll",
    "brotlicommon.dll",
    "brotlidec.dll",
    "brotlienc.dll",
    # SDL3_mixer's OGG/Vorbis decoding backend.
    "ogg.dll",
    "vorbis.dll",
    "vorbisenc.dll",
    "vorbisfile.dll"
)

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

foreach ($dll in $dlls) {
    $src = Join-Path $VcpkgBinDir $dll
    $dst = Join-Path $OutDir $dll
    if (-not (Test-Path $src)) {
        Write-Warning "copy_vcpkg_dlls: missing $src"
        continue
    }
    if ((Test-Path $dst) -and (Get-Item $dst).LastWriteTimeUtc -ge (Get-Item $src).LastWriteTimeUtc) {
        continue
    }
    $attempts = 0
    while ($true) {
        try {
            Copy-Item -Path $src -Destination $dst -Force
            break
        } catch {
            $attempts++
            if ($attempts -ge 5) { throw }
            Start-Sleep -Milliseconds 200
        }
    }
}
