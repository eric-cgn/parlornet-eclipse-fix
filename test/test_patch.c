/*
 * test_patch.c - host test of src/patch.c.
 *
 *   test_patch
 *   test_patch <original resource fork> <patched reference resource fork>
 *
 * Without arguments it checks the SHA-256 routine against the FIPS 180-4
 * examples, checks that the stored digest of the window around the branch is
 * the value recorded below, and runs the check and the patch on a synthetic
 * CODE 1 (pseudo-random bytes with the shipped branch at the patch site),
 * whose reference digests it computes itself. The game is not in this
 * repository, and neither is any of its code.
 *
 * With the two resource forks of a real copy (raw resource fork bytes, as kept
 * in .rsrc/ by netatalk-style shares or extracted from a MacBinary) it also
 * finds CODE 1 in the original, checks its window against the recorded digest,
 * runs the patch routine on it in place, and checks that the whole fork then
 * equals the reference byte for byte. Then it checks that the routine refuses
 * the already-patched fork and several altered copies.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/patch.h"

/* SHA-256 of the 64 bytes at CODE 1 data offset 0x16D2C (the window, branch as
   shipped) in the copy of Eclipse 1.2 examined, whose resource fork has SHA-256
   ef6ea99590ec32af41cfbcc4ceb92e3c3a47cf526ec6f927e5d350ef66c7ca2c. */
static const char kShippedWindowHex[] =
	"f6a96ba4e21b7cab638c62a095ee2f4f43180b28fdbbb2e3c3d890061d69915e";
/* CRC-32 of CODE 1 of the same copy. */
#define kShippedCRC 0xE5423745UL

static const unsigned char kOrig[2] = { 0x65, 0x04 };  /* the branch as shipped */
static const unsigned char kFix[2]  = { 0x4E, 0x71 };  /* nop */

static int failures = 0;

#define CHECK(cond, msg) do { \
	if (cond) printf("ok    %s\n", msg); \
	else { printf("FAIL  %s\n", msg); failures++; } \
} while (0)

static void Hex(const unsigned char *d, int n, char *out)
{
	int i;
	for (i = 0; i < n; i++)
		snprintf(out + 2 * i, 3, "%02x", d[i]);
}

static int DigestIs(const unsigned char *p, long len, const char *hex)
{
	unsigned char d[32];
	char s[65];
	EclSHA256(p, len, d);
	Hex(d, 32, s);
	return strcmp(s, hex) == 0;
}

/* Bitwise CRC-32 (IEEE, as zlib), separate from the table code in patch.c. */
static unsigned long Crc32(const unsigned char *p, long len)
{
	unsigned long c = 0xFFFFFFFFUL;
	int k;
	while (len-- > 0) {
		c ^= *p++;
		for (k = 0; k < 8; k++)
			c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
	}
	return (c ^ 0xFFFFFFFFUL) & 0xFFFFFFFFUL;
}

/* The refusals shared by the synthetic and the real test: work is the CODE 1
   data inside a buffer of bufLen bytes starting at base, orig a copy of that
   buffer as shipped. */
static void Refusals(unsigned char *base, unsigned char *orig, unsigned char *scratch, long bufLen,
                     long codeOff, long codeLen, const EclRef *ref)
{
	static const struct { long off; const char *what; } alt[] = {
		{ kEclPatchOffset - kEclWindowBefore, "first window byte altered" },
		{ kEclPatchOffset - kEclWindowBefore + kEclWindowLen - 1, "last window byte altered" },
		{ kEclPatchOffset - 1, "byte before the branch altered" },
		{ kEclPatchOffset + 2, "byte after the branch altered" },
		{ 0x10, "byte far from the site altered (CRC)" },
		{ kEclCode1Size - 1, "last byte of CODE 1 altered (CRC)" },
	};
	size_t k;
	EclState st;

	for (k = 0; k < sizeof alt / sizeof alt[0]; k++) {
		memcpy(base, orig, bufLen);
		base[codeOff + alt[k].off] ^= 0x01;
		memcpy(scratch, base, bufLen);
		st = EclPatchRef(base + codeOff, codeLen, ref);
		CHECK(st == kEclUnknown && memcmp(base, scratch, bufLen) == 0, alt[k].what);
	}
	memcpy(base, orig, bufLen);
	base[codeOff + kEclPatchOffset] = kFix[0];  /* half-patched */
	CHECK(EclPatchRef(base + codeOff, codeLen, ref) == kEclUnknown, "branch byte half-changed: refused");
	memcpy(base, orig, bufLen);
	CHECK(EclPatchRef(base + codeOff, codeLen - 1, ref) == kEclUnknown, "wrong CODE 1 size: refused");
	if (bufLen >= codeOff + codeLen + 2)
		CHECK(EclPatchRef(base + codeOff, codeLen + 2, ref) == kEclUnknown, "longer CODE 1: refused");
	CHECK(memcmp(base, orig, bufLen) == 0, "no change after size refusals");
}

static void SelfTest(void)
{
	static const char abc56[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	static const char abc112[] = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
	                             "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
	unsigned char *code, *orig, *scratch, *million;
	char hex[65];
	unsigned long seed = 12345;
	long i, diffs;
	EclRef ref;
	EclState st;

	/* 1. SHA-256 against the FIPS 180-4 examples */
	CHECK(DigestIs((const unsigned char *)"", 0,
	               "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), "SHA-256 of \"\"");
	CHECK(DigestIs((const unsigned char *)"abc", 3,
	               "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "SHA-256 of \"abc\"");
	CHECK(DigestIs((const unsigned char *)abc56, 56,
	               "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"), "SHA-256, 448 bits");
	CHECK(DigestIs((const unsigned char *)abc112, 112,
	               "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"), "SHA-256, 896 bits");
	million = malloc(1000000);
	memset(million, 'a', 1000000);
	CHECK(DigestIs(million, 1000000,
	               "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"), "SHA-256, a million a");
	free(million);

	/* 2. the stored values are the ones recorded from the real copy */
	Hex(kEclShipped.window, 32, hex);
	CHECK(strcmp(hex, kShippedWindowHex) == 0, "stored window digest is the recorded value");
	CHECK(kEclShipped.crc == kShippedCRC && kEclShipped.size == kEclCode1Size,
	      "stored CRC-32 and size are the recorded values");

	/* 3. a synthetic CODE 1 with its own reference */
	code = malloc(kEclCode1Size + 2);
	orig = malloc(kEclCode1Size + 2);
	scratch = malloc(kEclCode1Size + 2);
	for (i = 0; i < kEclCode1Size + 2; i++) {
		seed = (seed * 1103515245UL + 12345UL) & 0xFFFFFFFFUL;
		code[i] = (unsigned char)(seed >> 16);
	}
	code[kEclPatchOffset] = kOrig[0];
	code[kEclPatchOffset + 1] = kOrig[1];
	memcpy(orig, code, kEclCode1Size + 2);
	ref.size = kEclCode1Size;
	ref.crc = Crc32(code, kEclCode1Size);
	EclSHA256(code + kEclPatchOffset - kEclWindowBefore, kEclWindowLen, ref.window);

	CHECK(EclCheck(code, kEclCode1Size) == kEclUnknown, "synthetic: not the shipped game");
	CHECK(EclCheckRef(code, kEclCode1Size, &ref) == kEclUnpatched, "synthetic: recognized as unpatched");
	st = EclPatchRef(code, kEclCode1Size, &ref);
	CHECK(st == kEclUnpatched, "synthetic: patch reports it applied the fix");
	diffs = 0;
	for (i = 0; i < kEclCode1Size + 2; i++)
		if (code[i] != orig[i]) diffs++;
	CHECK(diffs == 2 && memcmp(code + kEclPatchOffset, kFix, 2) == 0, "synthetic: exactly the two bytes changed");
	CHECK(EclCheckRef(code, kEclCode1Size, &ref) == kEclPatched, "synthetic: result recognized as patched");
	memcpy(scratch, code, kEclCode1Size + 2);
	CHECK(EclPatchRef(code, kEclCode1Size, &ref) == kEclPatched
	      && memcmp(code, scratch, kEclCode1Size + 2) == 0, "synthetic: already patched: refused, no change");
	Refusals(code, orig, scratch, kEclCode1Size + 2, 0, kEclCode1Size, &ref);
	free(code);
	free(orig);
	free(scratch);
}

static unsigned char *ReadFile(const char *path, long *len)
{
	FILE *f = fopen(path, "rb");
	unsigned char *buf;
	long n;

	if (!f) { perror(path); exit(2); }
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc(n ? n : 1);
	if (!buf || fread(buf, 1, n, f) != (size_t)n) { perror(path); exit(2); }
	fclose(f);
	*len = n;
	return buf;
}

static unsigned long Be32(const unsigned char *p)
{
	return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static unsigned Be16(const unsigned char *p)
{
	return (p[0] << 8) | p[1];
}

/* Locates resource (type, id) in a raw resource fork; returns the offset of
   its data (after the 4-byte length) and stores the length. -1 if absent. */
static long FindResource(const unsigned char *fork, long forkLen, const char *type, int id, long *len)
{
	unsigned long dataOff, mapOff;
	const unsigned char *map, *typeList;
	unsigned nTypes, i, j;

	if (forkLen < 16) return -1;
	dataOff = Be32(fork);
	mapOff = Be32(fork + 4);
	if (mapOff + 30 > (unsigned long)forkLen) return -1;
	map = fork + mapOff;
	typeList = map + Be16(map + 24);
	nTypes = Be16(typeList) + 1;
	for (i = 0; i < nTypes; i++) {
		const unsigned char *t = typeList + 2 + 8 * i;
		if (memcmp(t, type, 4) != 0) continue;
		for (j = 0; j <= Be16(t + 4); j++) {
			const unsigned char *ref = typeList + Be16(t + 6) + 12 * j;
			if ((short)Be16(ref) == id) {
				unsigned long off = dataOff + (Be32(ref + 4) & 0xFFFFFF);
				*len = (long)Be32(fork + off);
				return (long)off + 4;
			}
		}
	}
	return -1;
}

static void RealTest(const char *origPath, const char *refPath)
{
	unsigned char *orig, *ref, *work, window[kEclWindowLen];
	long origLen, refLen, codeLen, codeOff, refCodeLen;
	long i, diffs;
	EclState st;

	orig = ReadFile(origPath, &origLen);
	ref = ReadFile(refPath, &refLen);

	codeOff = FindResource(orig, origLen, "CODE", 1, &codeLen);
	CHECK(codeOff > 0, "CODE 1 found in the original fork");
	if (codeOff <= 0) return;
	printf("      CODE 1 data at fork offset 0x%lx, %ld bytes; patch site at fork offset 0x%lx\n",
	       codeOff, codeLen, codeOff + kEclPatchOffset);
	CHECK(FindResource(ref, refLen, "CODE", 1, &refCodeLen) == codeOff && refCodeLen == codeLen,
	      "reference has CODE 1 at the same place");
	CHECK(codeLen == kEclCode1Size && DigestIs(orig + codeOff + kEclPatchOffset - kEclWindowBefore,
	                                           kEclWindowLen, kShippedWindowHex),
	      "window of the original has the recorded SHA-256");
	CHECK(codeLen == kEclCode1Size && Crc32(orig + codeOff, codeLen) == kShippedCRC,
	      "CODE 1 of the original has the recorded CRC-32");
	memcpy(window, ref + codeOff + kEclPatchOffset - kEclWindowBefore, kEclWindowLen);
	CHECK(memcmp(window + kEclWindowBefore, kFix, 2) == 0, "reference holds the fix at the site");
	memcpy(window + kEclWindowBefore, kOrig, 2);
	CHECK(DigestIs(window, kEclWindowLen, kShippedWindowHex), "window of the reference, branch restored, matches");

	/* 1. the original is recognized and patched to exactly the reference */
	work = malloc(origLen);
	memcpy(work, orig, origLen);
	CHECK(EclCheck(work + codeOff, codeLen) == kEclUnpatched, "original is recognized as unpatched");
	st = EclPatch(work + codeOff, codeLen);
	CHECK(st == kEclUnpatched, "patch reports it applied the fix");
	diffs = 0;
	for (i = 0; i < origLen; i++)
		if (work[i] != orig[i]) diffs++;
	CHECK(diffs == 2, "exactly two bytes changed");
	CHECK(origLen == refLen && memcmp(work, ref, refLen) == 0, "patched fork equals the reference byte for byte");

	/* 2. the result (and the reference) is recognized as patched; patching again changes nothing */
	CHECK(EclCheck(work + codeOff, codeLen) == kEclPatched, "result is recognized as patched");
	CHECK(EclCheck(ref + codeOff, codeLen) == kEclPatched, "reference is recognized as patched");
	st = EclPatch(ref + codeOff, codeLen);
	CHECK(st == kEclPatched && memcmp(work, ref, refLen) == 0, "already patched: refused, no change");

	/* 3. altered copies are refused and left alone (ref reused as scratch) */
	Refusals(work, orig, ref, origLen, codeOff, codeLen, &kEclShipped);
}

int main(int argc, char **argv)
{
	if (argc != 1 && argc != 3) {
		fprintf(stderr, "usage: %s [<original rsrc fork> <patched reference rsrc fork>]\n", argv[0]);
		return 2;
	}
	SelfTest();
	if (argc == 3)
		RealTest(argv[1], argv[2]);
	printf(failures ? "%d FAILED\n" : "all passed\n", failures);
	return failures ? 1 : 0;
}
