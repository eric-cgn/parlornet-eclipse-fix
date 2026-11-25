"""Eclipse 68k Fix: its patch routine, compiled for the host and run on a synthetic CODE 1 (test/test_patch.c),
the rule that the repository holds none of the game's code, only digests of it, the sources and the build,
and the release files when they have been built (tools/build.sh).

    python3 -m unittest discover -s tests

The game is not in the repository. With ECLIPSE_ORIG_RSRC and ECLIPSE_REF_RSRC set to the resource forks of a
copy of Reversi The Eclipse 1.2 and of a patched copy, the host test also runs on them, and the window the
patcher hashes is read from the copy to check that no run of it appears in the patcher's files, its README or
the built downloads. ECLIPSE_EXTRA_FILES (paths separated by the OS path separator) adds other files to that
check, such as the ParlorNet docs that describe the bug.
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = ROOT
BUILD = os.path.join(ROOT, "build")
SLUG = "eclipse-68k-fix"
sys.path.insert(0, os.path.join(ROOT, "tools"))
# SHA-256 of the 64-byte window (CODE 1 data offset 0x16D2C, branch as shipped), recorded from the copy examined.
WINDOW_SHA256 = "f6a96ba4e21b7cab638c62a095ee2f4f43180b28fdbbb2e3c3d890061d69915e"
PATCH_OFFSET, WINDOW_BEFORE, WINDOW_LEN, CODE1_SIZE = 0x16D56, 0x2A, 64, 0x229E4
IMAGE = "ghcr.io/autc04/retro68@sha256:96ae710e8c80ac92a90da19598115a572ed6a50143b713ef7444fadc0ea0fef9"
# The patcher's own files: the ones checked for credits and for the game's bytes.
SOURCES = ("CMakeLists.txt", "LICENSE", "README.md", "Makefile", "src", "test", "release", "docs")


def code1(fork):
    """Offset and length of CODE 1's data in a raw resource fork."""
    data_off, map_off = int.from_bytes(fork[0:4], "big"), int.from_bytes(fork[4:8], "big")
    m = fork[map_off:]
    types = m[int.from_bytes(m[24:26], "big"):]
    for i in range(int.from_bytes(types[0:2], "big") + 1):
        t = types[2 + 8 * i:10 + 8 * i]
        if t[:4] != b"CODE":
            continue
        for j in range(int.from_bytes(t[4:6], "big") + 1):
            ref = types[int.from_bytes(t[6:8], "big") + 12 * j:]
            if int.from_bytes(ref[0:2], "big", signed=True) == 1:
                off = data_off + (int.from_bytes(ref[4:8], "big") & 0xFFFFFF)
                return off + 4, int.from_bytes(fork[off:off + 4], "big")
    raise ValueError("no CODE 1")


def source_files():
    out = []
    for name in SOURCES:
        path = os.path.join(ROOT, name)
        if os.path.isfile(path):
            out.append(path)
        for d, _, fs in os.walk(path):
            out += [os.path.join(d, f) for f in fs]
    return out


def built(ext):
    path = os.path.join(BUILD, "%s.%s" % (SLUG, ext))
    if not os.path.isfile(path):
        return None
    with open(path, "rb") as f:
        return f.read()


class EclipseFix(unittest.TestCase):

    def run_host_test(self, *forks):
        cc = shutil.which(os.environ.get("CC", "cc"))
        if not cc:
            self.skipTest("no C compiler")
        with tempfile.TemporaryDirectory() as tmp:
            exe = os.path.join(tmp, "test_patch")
            subprocess.run([cc, "-O2", "-Wall", "-Wextra", "-std=c99", "-o", exe,
                            os.path.join(BASE, "test", "test_patch.c"), os.path.join(BASE, "src", "patch.c")],
                           check=True)
            r = subprocess.run([exe, *forks], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertTrue(r.stdout.rstrip().endswith("all passed"), r.stdout)
        self.assertNotIn("FAIL", r.stdout)
        return r.stdout

    def test_host_test_synthetic(self):
        out = self.run_host_test()
        for line in ("ok    SHA-256 of \"abc\"", "ok    stored window digest is the recorded value",
                     "ok    synthetic: exactly the two bytes changed", "ok    first window byte altered"):
            self.assertIn(line, out)

    def test_stored_digest(self):
        """patch.c stores the window's SHA-256 (the recorded value), the CRC-32 and the two bytes of the change,
        and no table of the game's bytes."""
        with open(os.path.join(BASE, "src", "patch.c")) as f:
            src = f.read()
        shipped = src.split("kEclShipped = {")[1].split("};")[0]
        stored = "".join(re.findall(r"0x([0-9A-F]{2})\b", shipped.split("{", 1)[1]))
        self.assertEqual(stored.lower(), WINDOW_SHA256)
        self.assertIn("0xE5423745UL", src)
        self.assertIn("{ 0x65, 0x04 }", src)
        self.assertIn("{ 0x4E, 0x71 }", src)
        self.assertNotIn("kContext", src)
        with open(os.path.join(BASE, "test", "test_patch.c")) as f:
            self.assertIn('"%s"' % WINDOW_SHA256, f.read())

    @unittest.skipUnless(os.environ.get("ECLIPSE_ORIG_RSRC") and os.environ.get("ECLIPSE_REF_RSRC"),
                         "set ECLIPSE_ORIG_RSRC and ECLIPSE_REF_RSRC to run on a real copy")
    def test_real_copy(self):
        orig_path, ref_path = os.environ["ECLIPSE_ORIG_RSRC"], os.environ["ECLIPSE_REF_RSRC"]
        out = self.run_host_test(orig_path, ref_path)
        self.assertIn("ok    patched fork equals the reference byte for byte", out)
        with open(orig_path, "rb") as f:
            fork = f.read()
        off, size = code1(fork)
        self.assertEqual(size, CODE1_SIZE)
        start = off + PATCH_OFFSET - WINDOW_BEFORE
        window = fork[start:start + WINDOW_LEN]
        self.assertEqual(hashlib.sha256(window).hexdigest(), WINDOW_SHA256)
        # No run of 6 bytes of the window, as bytes or as hex text, in the patcher, its docs or its downloads.
        runs = {window[i:i + 6] for i in range(WINDOW_LEN - 5)}
        hex_runs = {r.hex() for r in runs}
        paths = source_files()
        paths += [os.path.join(BUILD, "%s.%s" % (SLUG, e)) for e in ("bin", "hqx", "dsk") if built(e)]
        paths += [p for p in os.environ.get("ECLIPSE_EXTRA_FILES", "").split(os.pathsep) if p]
        for path in paths:
            with open(path, "rb") as f:
                data = f.read()
            text = re.sub(rb"0x|\$|[^0-9a-fA-F]", b"", data).lower().decode()
            for r in runs:
                self.assertNotIn(r, data, path)
            for h in hex_runs:
                self.assertNotIn(h, text, path)


class Sources(unittest.TestCase):

    def test_the_patcher_sources(self):
        """The patcher's source, with its license and no credits; the build script uses the pinned Retro68 image,
        and the release workflow runs it and the tests on a push to main with a new version."""
        for f in ("CMakeLists.txt", "LICENSE", "README.md", "Makefile", "src/main.c", "src/patch.c",
                  "src/patch.h", "src/EclipseFix.r", "test/test_patch.c", "release/%s-readme.txt" % SLUG):
            self.assertTrue(os.path.isfile(os.path.join(ROOT, f)), f)
        for path in source_files():
            with open(path, "rb") as fh:
                data = fh.read()
            for credit in (b"Eric", b"Claude", b"Anthropic", b"Credits", b"Thaller"):
                self.assertNotIn(credit, data, path)
        with open(os.path.join(ROOT, "tools", "build.sh")) as f:
            script = f.read()
        self.assertIn(IMAGE, script)
        with open(os.path.join(ROOT, ".github", "workflows", "release.yml")) as f:
            workflow = f.read()
        self.assertIn("tools/build.sh", workflow)
        self.assertLess(workflow.index("tools/build.sh"), workflow.index("gh release create"))
        self.assertIn("unittest discover -s tests", workflow)
        self.assertIn("branches: [main]", workflow)
        self.assertIn('gh release view "v$v"', workflow)
        for ext in ("bin", "hqx", "dsk"):            # built, never committed
            self.assertFalse(os.path.exists(os.path.join(ROOT, "release", "%s.%s" % (SLUG, ext))))

    def test_version(self):
        """The version in the 'vers' resources and the window, and the README's title, agree."""
        with open(os.path.join(ROOT, "src", "EclipseFix.r")) as f:
            r = f.read()
        version = re.search(r'"([\d.]+), MIT license"', r).group(1)
        self.assertIn('"Eclipse 68k Fix %s"' % version, r)
        with open(os.path.join(ROOT, "release", "%s-readme.txt" % SLUG)) as f:
            readme = f.read()
        self.assertTrue(readme.startswith("Eclipse 68k Fix %s\n" % version))
        self.assertIn("MIT License", readme)
        self.assertTrue(all(len(l) <= 78 for l in readme.splitlines()))
        for ext in ("hqx", "bin", "dsk"):
            self.assertIn("%s.%s" % (SLUG, ext), readme)


class Built(unittest.TestCase):
    """The release files, when tools/build.sh has built them."""

    def test_downloads(self):
        import binhex4
        files = {e: built(e) for e in ("hqx", "bin", "dsk")}
        if not all(files.values()):
            self.skipTest("not built (tools/build.sh)")
        hqx, mb = binhex4.decode(files["hqx"]), binhex4.read_macbinary(files["bin"])
        self.assertEqual(hqx, mb)
        self.assertEqual((hqx["name"], hqx["type"], hqx["creator"]), (b"Eclipse 68k Fix", b"APPL", b"EcFx"))
        self.assertEqual(binhex4.encode(mb), files["hqx"])
        self.assertEqual(files["dsk"][1024:1026], b"BD")
        sums = os.path.join(BUILD, "release", "SHA256SUMS")
        if os.path.isfile(sums):
            with open(sums) as f:
                listed = dict(reversed(l.split("  ", 1)) for l in f.read().splitlines())
            for ext, data in files.items():
                self.assertEqual(listed["%s.%s" % (SLUG, ext)], hashlib.sha256(data).hexdigest())


class BinHex(unittest.TestCase):
    """tools/binhex4.py: runs, 0x90 bytes and CRCs."""

    def test_round_trip(self):
        import binhex4
        info = {"name": b"Test", "type": b"TEXT", "creator": b"ttxt", "flags": 0x0100,
                "data": b"\x90" * 7 + b"abc" + b"\0" * 600 + b"\x90x\x90\x90", "rsrc": bytes(range(256)) * 3}
        out = binhex4.encode(info)
        self.assertEqual(binhex4.decode(out), info)
        self.assertLess(len(out), 2000)  # the run of 600 zeros is packed
        bad = bytearray(out)
        bad[-10] = ord("!") if bad[-10] != ord("!") else ord("#")
        with self.assertRaises(ValueError):
            binhex4.decode(bytes(bad))


if __name__ == "__main__":
    unittest.main()
