#!/bin/bash
# MechWarrior 1 Remake alone, for the Mac: "MechWarrior 1 Remake.app" and its DMG, the package the launcher's
# INSTALL / UPDATE downloads from this repo's Releases (DEPLOY.md). mw1 is the app's executable, mw1data/ in
# Contents/Resources (where SDL_GetBasePath points mw1, as in the combined package), the engine's stand-in fonts in
# Resources/assets/fonts, the libraries it loads in Contents/Frameworks with their paths rewritten to @rpath, its
# own bundle id, every binary signed ad hoc. A package build (MW3_PACKAGE: no development game paths). Nothing from
# any game. PM's own tools/macos/make_app.sh is left as it is, so merges from mech3-pm-re never meet this file.
#
#   mw1/tools/package/make_app.sh                    run from the repo root; Homebrew's libraries (not for release)
#   mw1/tools/package/make_app.sh --deps [PREFIX]    the release form: universal (arm64, x86_64), macOS 12 and
#        later, against tools/deps/build_deps.sh mac's libraries (default build-deps/mac/prefix)
#
# Out: build-mw1-mac/dist/MechWarrior 1 Remake.app and build-mw1-mac/dist/MW1-Remake-<mw1-vX.Y.Z>-mac.dmg
set -euo pipefail
cd "$(dirname "$0")/../../.."
DEPS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --deps) if [ $# -gt 1 ] && [ "${2#--}" = "$2" ]; then DEPS="$(cd "$2" && pwd)"; shift 2
            else DEPS="$PWD/build-deps/mac/prefix"; shift; fi ;;
    *) echo "unknown option $1" >&2; exit 2 ;;
  esac
done
if [ -n "$DEPS" ] && [ ! -d "$DEPS/lib" ]; then echo "no $DEPS/lib: run tools/deps/build_deps.sh mac first" >&2; exit 1; fi
if [ -z "$DEPS" ]; then
  echo "note: Homebrew's libraries (this Mac's macOS, arm64 only): not for release" >&2
fi
NAME="MechWarrior 1 Remake"
BUNDLE_ID="com.owenmorgan.mw1remake"  # its own, so it never replaces MW3's or Pirate's Moon's
VER="$(git describe --tags --match 'mw1-v[0-9]*' --always --dirty)"
OUT=build-mw1-mac/dist
APP="$OUT/$NAME.app"

# The build, from scratch.
BUILD=build-mw1-mac/build
rm -rf "$BUILD"
CONF=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DMW3_PACKAGE=ON -DMW3_BUILD_MW1=ON)
if [ -n "$DEPS" ]; then
  CONF+=(-DCMAKE_PREFIX_PATH="$DEPS" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0)
  export PKG_CONFIG_LIBDIR="$DEPS/lib/pkgconfig"
fi
cmake -S . -B "$BUILD" "${CONF[@]}" >/dev/null
cmake --build "$BUILD" --target mw1 > build-mw1-mac/build.log 2>&1 ||
  { grep -E -B2 -A8 "error|FAILED" build-mw1-mac/build.log | tail -40 >&2; echo "mw1 did not build: build-mw1-mac/build.log" >&2; exit 1; }
[ -x "$BUILD/mw1" ] && [ -f "$BUILD/mw1data/zbd/mw1image.zbd" ] || { echo "mw1 or its mw1data was not built in $BUILD" >&2; exit 1; }

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
cp "$BUILD/mw1" "$APP/Contents/MacOS/mw1"
cp -R "$BUILD/mw1data" "$APP/Contents/Resources/mw1data"

# Every library outside the system, followed through the ones they load (as PM's make_app.sh does).
python3 - "$APP" "$DEPS" <<'EOF'
import glob, os, shutil, subprocess, sys
app = sys.argv[1]
prefix = sys.argv[2] if len(sys.argv) > 2 else ''
exes = [os.path.join(app, 'Contents/MacOS', n) for n in ('mw1',)]
exes = [e for e in exes if os.path.exists(e)]
fw = os.path.join(app, 'Contents/Frameworks')
def deps(f):
    out = subprocess.run(['otool', '-L', f], capture_output=True, text=True, check=True).stdout.splitlines()
    # A universal file's list has a "<file> (architecture x):" header per arch.
    return sorted({l.strip().split(' (')[0] for l in out if l.startswith(('\t', ' '))})
def foreign(p):
    if prefix and p.startswith('@rpath/'):  # build_deps.sh's libraries are named @rpath/<file>
        return True
    return not (p.startswith('/System/') or p.startswith('/usr/lib/') or p.startswith('@'))
def licences(real, name):
    dst = os.path.join(app, 'Contents/Resources/licenses', name)
    if prefix:
        # From the source tree it was built from (build-deps/mac/work/src/<name>-<version>).
        stem = name.split('.')[0].removeprefix('lib')
        srcs = [d for d in glob.glob(os.path.join(os.path.dirname(prefix), 'work/src/*'))
                if os.path.basename(d).lower().startswith(stem.lower() + '-')]
        if stem.startswith('av') or stem.startswith('sw'):
            srcs = glob.glob(os.path.join(os.path.dirname(prefix), 'work/src/ffmpeg-*'))
        if stem == 'SDL3_ttf':  # FreeType is built into it
            srcs += glob.glob(os.path.join(os.path.dirname(prefix), 'work/src/freetype-*'))
        for d in srcs:
            for n in os.listdir(d):
                if n.upper().startswith(('LICEN', 'COPYING')) and os.path.isfile(os.path.join(d, n)):
                    sub = os.path.join(dst, os.path.basename(d))
                    os.makedirs(sub, exist_ok=True)
                    shutil.copy2(os.path.join(d, n), sub)
            # FreeType's LICENSE.TXT points to the FTL it is used under.
            ftl = os.path.join(d, 'docs', 'FTL.TXT')
            if os.path.isfile(ftl):
                shutil.copy2(ftl, os.path.join(dst, os.path.basename(d)))
        return
    # Its licence files, from its keg (Cellar/<formula>/<version>/).
    keg = os.path.dirname(os.path.dirname(real))
    lic = os.path.join(app, 'Contents/Resources/licenses', os.path.basename(os.path.dirname(keg)))
    os.makedirs(lic, exist_ok=True)
    for n in os.listdir(keg):
        if n.upper().startswith(('LICEN', 'COPYING')):
            shutil.copy2(os.path.join(keg, n), lic)
# Homebrew's own libraries can name each other @rpath/<file> (brotli does):
# found beside the library that loads them, or in Homebrew's lib.
origin = {}
def brew_rpath(f, d):
    if prefix or not d.startswith('@rpath/'):
        return None
    for lib in (origin.get(f, ''), '/opt/homebrew/lib', '/usr/local/lib'):
        c = os.path.join(lib, os.path.basename(d)) if lib else ''
        if c and os.path.exists(c):
            return c
    return None
copied, todo = {}, list(exes)
while todo:
    f = todo.pop()
    for d in deps(f):
        found = brew_rpath(f, d)
        if (foreign(d) or found) and d not in copied:
            name = os.path.basename(d)
            dst = os.path.join(fw, name)
            if not os.path.exists(dst):
                src = found or (os.path.join(prefix, 'lib', name) if d.startswith('@rpath/') else d)
                if not os.path.exists(src):
                    sys.exit(f'{d}: not in {prefix}/lib')
                real = os.path.realpath(src)
                shutil.copy2(real, dst)
                origin[dst] = os.path.dirname(real)
                os.chmod(dst, 0o644)
                licences(real, name)
            copied[d] = name
            todo.append(dst)
for f in exes + [os.path.join(fw, n) for n in set(copied.values())]:
    args = []
    for d in deps(f):
        if d in copied:
            args += ['-change', d, '@rpath/' + copied[d]]
    if f not in exes:
        args += ['-id', '@rpath/' + os.path.basename(f)]
    if args:
        subprocess.run(['install_name_tool'] + args + [f], check=True, stderr=subprocess.DEVNULL)
# Only the bundle's rpath: a build against a prefix keeps its absolute lib path.
for exe in exes:
    lc = subprocess.run(['otool', '-l', exe], capture_output=True, text=True, check=True).stdout.split('\n')
    stale = {lc[i + 2].strip().split(' ')[1] for i, l in enumerate(lc) if 'cmd LC_RPATH' in l}  # each arch lists it
    for path in sorted(p for p in stale):
        subprocess.run(['install_name_tool', '-delete_rpath', path, exe], check=True)
    subprocess.run(['install_name_tool', '-add_rpath', '@executable_path/../Frameworks', exe], check=True)
def outside(p):
    return not (p.startswith('/System/') or p.startswith('/usr/lib/') or p.startswith('@'))
left = [d for f in exes + [os.path.join(fw, n) for n in set(copied.values())] for d in deps(f) if outside(d)]
if left:
    sys.exit('still outside the bundle: ' + ', '.join(sorted(set(left))))
print(f'{len(set(copied.values()))} libraries bundled:', ' '.join(sorted(set(copied.values()))))
EOF

# The icon: the launcher's MW1 badge at each iconset size.
ICONSET="build-mw1-mac/mw1.iconset"
rm -rf "$ICONSET" && mkdir -p "$ICONSET"
for n in 16 32 128 256 512; do
  sips -z "$n" "$n" assets/launcher/badge_mw1.png --out "$ICONSET/icon_${n}x${n}.png" >/dev/null
  sips -z "$((2 * n))" "$((2 * n))" assets/launcher/badge_mw1.png --out "$ICONSET/icon_${n}x${n}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/mw1.icns"

cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>$NAME</string>
  <key>CFBundleDisplayName</key><string>$NAME</string>
  <key>CFBundleDevelopmentRegion</key><string>en</string>
  <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
  <key>CFBundleExecutable</key><string>mw1</string>
  <key>CFBundleIconFile</key><string>mw1</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VER</string>
  <key>CFBundleVersion</key><string>$VER</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>LSMinimumSystemVersion</key><string>$(otool -l "$APP/Contents/MacOS/mw1" | awk '/LC_BUILD_VERSION/{b=1} b&&/minos/{print $2; exit}')</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSArchitecturePriority</key><array><string>arm64</string></array>
  <key>NSHumanReadableCopyright</key><string>A fan remake; MechWarrior is a trademark of its owners. Needs your own copies of MechWarrior (1989), MechWarrior 3 and Pirate's Moon.</string>
</dict>
</plist>
EOF

# The read-me and the notices: MW1's own, and the engine's (its libraries and fonts).
cp mw1/dist/README-MW1.txt "$APP/Contents/Resources/README.txt"
cp mw1/dist/THIRD-PARTY-MW1.txt "$APP/Contents/Resources/THIRD-PARTY-MW1.txt"
cp tools/macos/THIRD-PARTY.txt "$APP/Contents/Resources/THIRD-PARTY.txt"
if [ -f LICENSE ]; then cp LICENSE "$APP/Contents/Resources/LICENSE.txt"; fi
mkdir -p "$APP/Contents/Resources/assets/fonts" "$APP/Contents/Resources/licenses/liberation" "$APP/Contents/Resources/licenses/anton"
cp assets/fonts/*.ttf "$APP/Contents/Resources/assets/fonts/"
cp assets/fonts/Liberation-OFL.txt "$APP/Contents/Resources/licenses/liberation/OFL.txt"
cp assets/fonts/Anton-OFL.txt "$APP/Contents/Resources/licenses/anton/OFL.txt"
# The engine's find screen (mw1 --setup) is drawn from its document: the launcher's art (the engine's own) in Resources, SDL's
# base path.
mkdir -p "$APP/Contents/Resources/assets/launcher/ui" "$APP/Contents/Resources/licenses/saira-condensed" "$APP/Contents/Resources/licenses/rmlui"
cp assets/launcher/ui/* "$APP/Contents/Resources/assets/launcher/ui/"
cp assets/launcher/*.png assets/launcher/font.ttf assets/launcher/font_body.ttf "$APP/Contents/Resources/assets/launcher/"
cp assets/launcher/font-OFL.txt "$APP/Contents/Resources/licenses/saira-condensed/OFL.txt"
cp third_party/rmlui/LICENSE.txt "$APP/Contents/Resources/licenses/rmlui/"
cp third_party/rmlui/Include/RmlUi/Core/Containers/LICENSE.txt "$APP/Contents/Resources/licenses/rmlui/LICENSE-containers.txt"
if [ -n "$DEPS" ]; then
  [ -d "$DEPS/share/licenses" ] || { echo "no licences in $DEPS/share/licenses: run tools/deps/build_deps.sh mac again" >&2; exit 1; }
  cp -R "$DEPS/share/licenses/." "$APP/Contents/Resources/licenses/"
fi

# Signing, ad hoc (no identity, so no name in the signature): the libraries, the program, then the app.
for f in "$APP"/Contents/Frameworks/*.dylib; do
  codesign --force --sign - "$f"
done
codesign --force --sign - --entitlements tools/macos/mw3.entitlements "$APP/Contents/MacOS/mw1"
codesign --force --sign - --entitlements tools/macos/mw3.entitlements "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$APP/Contents/Info.plist")" = mw1 ] || { echo "the app does not start mw1" >&2; exit 1; }
[ -f "$APP/Contents/Resources/mw1data/zbd/mw1image.zbd" ] || { echo "no mw1data in the app" >&2; exit 1; }
case "$VER" in *-dirty) echo "warning: $VER: a dirty tree, not for release" >&2 ;; esac

# The DMG: the app and a link to Applications.
DMG="$OUT/MW1-Remake-$VER-mac.dmg"
STAGE="build-mw1-mac/dmg"
rm -rf "$STAGE" "$DMG" && mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
cp mw1/dist/README-MW1.txt "$STAGE/Read me.txt"
# No local path or user name in anything shipped.
python3 -I tools/deps/check_paths.py "$STAGE" || exit 1
hdiutil create -volname "$NAME" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
echo "App: $APP"
echo "DMG: $DMG"
