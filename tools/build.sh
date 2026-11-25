#!/bin/sh
# Builds Eclipse 68k Fix with Retro68 in Docker, as the release workflow does (.github/workflows/release.yml),
# into build/ (git-ignored):
#
#   build/eclipse-68k-fix.bin .hqx .dsk     the application as MacBinary, BinHex and an 800K HFS disk image
#   build/release/                          the release files: those three, eclipse-68k-fix-readme.txt
#                                           (release/), and SHA256SUMS and MD5SUMS of all four
#
# The .bin and the .dsk come from Retro68's Rez, with their dates set to the release date by
# tools/patcher_dates.py, so that the same source always gives the same files; the .hqx is made from the .bin
# by tools/binhex4.py.
# RETRO68_IMAGE overrides the toolchain image (pinned by digest: the one the releases are built with).
set -eu
cd "$(dirname "$0")/.."
IMAGE="${RETRO68_IMAGE:-ghcr.io/autc04/retro68@sha256:96ae710e8c80ac92a90da19598115a572ed6a50143b713ef7444fadc0ea0fef9}"
TC=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
OUT=build
SLUG=eclipse-68k-fix
APP="Eclipse 68k Fix"
DAY=2026-11-21          # the release date of 1.0
mkdir -p "$OUT"

rm -rf "$OUT/$SLUG"
docker run --rm --platform linux/amd64 -u "$(id -u):$(id -g)" -v "$PWD:/src" -w /src "$IMAGE" sh -c "
	set -e
	cmake -S . -B $OUT/$SLUG -DCMAKE_TOOLCHAIN_FILE=$TC -DCMAKE_BUILD_TYPE=MinSizeRel >/dev/null
	cmake --build $OUT/$SLUG >/dev/null
"
cp "$OUT/$SLUG/$APP.bin" "$OUT/$SLUG.bin"
cp "$OUT/$SLUG/$APP.dsk" "$OUT/$SLUG.dsk"
python3 tools/patcher_dates.py "$DAY" "$OUT/$SLUG.bin" "$OUT/$SLUG.dsk"
python3 tools/binhex4.py "$OUT/$SLUG.bin" "$OUT/$SLUG.hqx"

rel="$OUT/release"
rm -rf "$rel"
mkdir -p "$rel"
cp "$OUT/$SLUG.hqx" "$OUT/$SLUG.bin" "$OUT/$SLUG.dsk" "release/$SLUG-readme.txt" "$rel/"
(
	cd "$rel"
	files="$SLUG.hqx $SLUG.bin $SLUG.dsk $SLUG-readme.txt"
	python3 - $files <<'EOF'
import hashlib, sys
for algo, out in (("sha256", "SHA256SUMS"), ("md5", "MD5SUMS")):
    with open(out, "w") as f:
        for name in sys.argv[1:]:
            with open(name, "rb") as g:
                f.write("%s  %s\n" % (hashlib.new(algo, g.read()).hexdigest(), name))
EOF
)
ls -l "$rel"
cat "$rel/SHA256SUMS"
