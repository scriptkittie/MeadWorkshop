#!/usr/bin/env bash
# Builds Mead Workshop and packs it for Windows. Run it in an MSYS2 UCRT64 shell (the GitHub workflow and the local
# installer script both do):
#   dist/MeadWorkshop-<version>-Setup.exe             installer (no admin rights needed; Start menu + desktop shortcuts)
#   dist/MeadWorkshop-<version>-windows-portable.zip  unzip anywhere and run MeadWorkshop.exe
# Everything the app needs (Qt, QML, the C++ runtime) is copied in, so the person installing it needs nothing else.
set -euo pipefail
cd "$(dirname "$0")/../.."
SRC="$(pwd)"
VERSION=$(sed -n 's/^project(MeadWorkshop VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
BUILD="$(cygpath -u "$LOCALAPPDATA")/MeadWorkshop/package-build-$(basename "$SRC")"
DIST="$SRC/dist"
APP="$DIST/MeadWorkshop"

echo "== Building version $VERSION (release)"
cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD"

echo "== Checking the brewing math and advice before packing"
"$BUILD/engine_tests.exe" | tail -1
"$BUILD/advisor_tests.exe" | tail -1
"$BUILD/consistency_tests.exe" | tail -1

echo "== Gathering the app and everything it needs"
rm -rf "$DIST"
mkdir -p "$APP"
cp "$BUILD/MeadWorkshop.exe" "$APP/"
for f in LICENSE README.md SOURCES.md THIRD-PARTY-NOTICES.md; do
  if [ -f "$f" ]; then cp "$f" "$APP/"; fi
done
DEPLOY=$(command -v windeployqt6 || command -v windeployqt-qt6 || command -v windeployqt || true)
if [ -z "$DEPLOY" ]; then
  echo "windeployqt wasn't found. Install it with: pacman -S mingw-w64-ucrt-x86_64-qt6-tools"
  exit 1
fi
"$DEPLOY" --release --qmldir qml --no-translations --compiler-runtime "$APP/MeadWorkshop.exe"
# Copy any remaining MSYS2 libraries the app or the Qt plugins load (repeat until nothing new turns up).
for pass in 1 2 3 4 5 6; do
  added=0
  while read -r dll; do
    [ -n "$dll" ] || continue
    if [ ! -f "$APP/$(basename "$dll")" ]; then cp "$dll" "$APP/"; added=1; fi
  done < <(find "$APP" \( -name '*.exe' -o -name '*.dll' \) -print0 | xargs -0 -n1 ldd 2>/dev/null | awk '/\/ucrt64\// {print $3}' | sort -u)
  [ "$added" = 1 ] || break
done

echo "== Adding the licenses of the bundled libraries"
LIC="$APP/licenses"
mkdir -p "$LIC"
: > "$LIC/PACKAGES.txt"
{
  echo mingw-w64-ucrt-x86_64-qt6-base
  echo mingw-w64-ucrt-x86_64-qt6-declarative
  find "$APP" -maxdepth 1 -name '*.dll' -printf '%f\n' | while read -r dll; do
    pacman -Qqo "/ucrt64/bin/$dll" 2>/dev/null || true
  done
} | sort -u | while read -r pkg; do
  [ -n "$pkg" ] || continue
  pacman -Q "$pkg" >> "$LIC/PACKAGES.txt" 2>/dev/null || echo "$pkg" >> "$LIC/PACKAGES.txt"
  name="${pkg#mingw-w64-ucrt-x86_64-}"
  if [ -d "/ucrt64/share/licenses/$name" ]; then cp -r "/ucrt64/share/licenses/$name" "$LIC/"; fi
done
echo "   $(wc -l < "$LIC/PACKAGES.txt") packages, $(find "$LIC" -type f | wc -l) license files"

echo "== Packing"
(cd "$DIST" && zip -qr "MeadWorkshop-$VERSION-windows-portable.zip" MeadWorkshop)
makensis -V2 \
  -DVERSION="$VERSION" \
  -DSRCDIR="$(cygpath -w "$APP")" \
  -DOUTFILE="$(cygpath -w "$DIST/MeadWorkshop-$VERSION-Setup.exe")" \
  -DICON="$(cygpath -w "$SRC/packaging/icons/meadworkshop.ico")" \
  packaging/windows/installer.nsi

echo "== Done:"
ls -1 "$DIST"
