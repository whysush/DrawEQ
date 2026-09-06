#!/usr/bin/env bash
#
# Builds DrawEQ.msi from an already-built VST3 bundle and standalone exe.
#
#   ./Installer/build-msi.sh <vst3-bundle-dir> <standalone-exe> [output.msi]
#
# The bundle directory is the folder called DrawEQ.vst3 - the folder, not the
# DLL of the same name inside it.
#
# Needs the WiX toolset:
#
#   dotnet tool install --global wix --version 5.0.2
#   wix extension add -g WixToolset.UI.wixext/5.0.2
#
# Windows only. WiX warns that other platforms are unsupported and it means it -
# on Linux it rejects even a plain directory name. This script is therefore
# meant for the Windows CI job or a Windows machine with Git Bash.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

bundle="${1:?usage: build-msi.sh <vst3-bundle-dir> <standalone-exe> [output.msi]}"
standalone="${2:?usage: build-msi.sh <vst3-bundle-dir> <standalone-exe> [output.msi]}"
output="${3:-DrawEQ.msi}"

# The version lives in cmake/Version.cmake so the installer and the plugin
# metadata can never drift apart.
version="$(sed -n 's/^ *set *( *DRAWEQ_VERSION *\([0-9.]*\).*/\1/p' "$here/../cmake/Version.cmake")"
[ -n "$version" ] || { echo "could not read DRAWEQ_VERSION from cmake/Version.cmake" >&2; exit 1; }

for required in "$bundle/Contents/x86_64-win/DrawEQ.vst3" \
                "$bundle/Contents/Resources/moduleinfo.json" \
                "$standalone"; do
    [ -f "$required" ] || { echo "missing: $required" >&2; exit 1; }
done

# wix is a native Windows program, so it cannot read the /d/a/... paths that
# Git Bash hands out. cygpath is what turns them back into C:\... form.
winpath () {
    if command -v cygpath > /dev/null 2>&1; then cygpath -w "$1"; else echo "$1"; fi
}

bundle_abs="$(cd "$bundle" && pwd)"
standalone_abs="$(cd "$(dirname "$standalone")" && pwd)/$(basename "$standalone")"

echo "packaging DrawEQ $version"

wix build "$(winpath "$here/DrawEQ.wxs")" \
    -arch x64 \
    -ext WixToolset.UI.wixext \
    -d "Version=$version" \
    -d "Vst3BundleDir=$(winpath "$bundle_abs")" \
    -d "StandaloneExe=$(winpath "$standalone_abs")" \
    -d "LicenseRtf=$(winpath "$here/License.rtf")" \
    -o "$(winpath "$output")"

echo "built $output ($(du -h "$output" | cut -f1))"
