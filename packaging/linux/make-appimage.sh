#!/usr/bin/env bash
# Builds Mead Workshop and packs it as an AppImage: one file that runs on most Linux systems
# (make it executable and double-click it, or run ./MeadWorkshop-*.AppImage).
#
# Needs the build tools once. CachyOS / Arch:
#   sudo pacman -S --needed qt6-base qt6-declarative cmake ninja gcc curl
# The first run downloads linuxdeploy and its Qt plugin (open source) into ~/.cache/meadworkshop/tools.
set -euo pipefail
cd "$(dirname "$0")/../.."
SRC="$(pwd)"
VERSION=$(sed -n 's/^project(MeadWorkshop VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/meadworkshop"
BUILD="$CACHE/appimage-build-$(basename "$SRC")"
TOOLS="$CACHE/tools"
mkdir -p "$TOOLS" "$SRC/dist"

echo "== Building version $VERSION (release)"
cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD"

echo "== Checking the brewing math and advice before packing"
"$BUILD/engine_tests" | tail -1
"$BUILD/advisor_tests" | tail -1
"$BUILD/consistency_tests" | tail -1

echo "== Gathering the app"
rm -rf "$BUILD/AppDir"
DESTDIR="$BUILD/AppDir" cmake --install "$BUILD"

# The licenses of the bundled libraries (see THIRD-PARTY-NOTICES.md).
DOC="$BUILD/AppDir/usr/share/licenses/meadworkshop"
mkdir -p "$DOC"
cp LICENSE THIRD-PARTY-NOTICES.md "$DOC/"
for d in /usr/share/licenses/qt6-base /usr/share/licenses/qt6-declarative "${QT_ROOT_DIR:-/nonexistent}/../../Licenses"; do
  if [ -d "$d" ]; then cp -r "$d" "$DOC/qt-$(basename "$d")"; fi
done

for tool in linuxdeploy linuxdeploy-plugin-qt; do
  file="$TOOLS/$tool-x86_64.AppImage"
  if [ ! -x "$file" ]; then
    echo "== Downloading $tool"
    curl -fsSL -o "$file" "https://github.com/linuxdeploy/$tool/releases/download/continuous/$tool-x86_64.AppImage"
    chmod +x "$file"
  fi
done

echo "== Packing (with every Qt library and QML module the app uses)"
export PATH="$TOOLS:$PATH"
export QML_SOURCES_PATHS="$SRC/qml"
export QMAKE="$(command -v qmake6 || command -v qmake-qt6 || command -v qmake || true)"
export APPIMAGE_EXTRACT_AND_RUN=1          # works even where FUSE isn't set up
export OUTPUT="$SRC/dist/MeadWorkshop-$VERSION-x86_64.AppImage"
"$TOOLS/linuxdeploy-x86_64.AppImage" --appdir "$BUILD/AppDir" --plugin qt --output appimage \
  --desktop-file "$BUILD/AppDir/usr/share/applications/meadworkshop.desktop" \
  --icon-file "$SRC/packaging/icons/meadworkshop.png"

echo "== Done: $OUTPUT"
