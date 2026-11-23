#!/bin/bash
# Builds Lumenwake on a Mac: VST3, Audio Unit and the standalone app, each one
# a universal binary (Apple Silicon and Intel).
#
# Needs: Xcode (or the Xcode command line tools), CMake 3.22 or newer, git.
# Run it from this folder:   bash build-mac.sh
# Add "install" to also copy the plugins into your own plug-in folders:
#                            bash build-mac.sh install
set -euo pipefail
cd "$(dirname "$0")"

JUCE_TAG=9.0.6

if [ ! -f libs/JUCE/CMakeLists.txt ]; then
    echo "Fetching JUCE $JUCE_TAG..."
    git clone --depth 1 --branch "$JUCE_TAG" https://github.com/juce-framework/JUCE.git libs/JUCE
fi

cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release
cmake --build build-mac --config Release --target Lumenwake_VST3 Lumenwake_AU Lumenwake_Standalone -j "$(sysctl -n hw.ncpu)"

OUT=dist-mac
mkdir -p "$OUT"

for name in Lumenwake.vst3 Lumenwake.component Lumenwake.app; do
    found="$(find build-mac/Lumenwake_artefacts -maxdepth 4 -name "$name" | head -n 1)"

    if [ -z "$found" ]; then
        echo "Not built: $name" >&2
        exit 1
    fi

    # ditto keeps the bundle structure and signature intact.
    ditto "$found" "$OUT/$name"

    # An ad-hoc signature is the least Apple Silicon will load. To hand the
    # plugin to other people without a security warning it must be signed with
    # a Developer ID and notarised instead; see the README.
    codesign --force --deep --sign - "$OUT/$name"
    lipo -archs "$(find "$OUT/$name/Contents/MacOS" -type f | head -n 1)"
done

echo
echo "Built into $OUT/"

if [ "${1:-}" = "install" ]; then
    mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"
    ditto "$OUT/Lumenwake.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/Lumenwake.vst3"
    ditto "$OUT/Lumenwake.component" "$HOME/Library/Audio/Plug-Ins/Components/Lumenwake.component"
    echo "Installed for this user. Checking the Audio Unit:"
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    auval -v aufx Lmw1 Lmwk || echo "auval reported a problem with the Audio Unit (see above)."
fi
