/*
 * patch.c - identify and fix the host name bug in Reversi • The Eclipse 1.2.
 *
 * In the 68k code, MyTelnet's constructor turns the host name from a C string
 * into a Pascal string in place with an inlined memcpy(buf + 1, buf, n).
 * For n < 32 the inline code is a forward byte loop, which smears the first
 * character over the whole name; for n >= 32 it calls _BlockMoveData, which
 * handles overlapping blocks. At code offset 0x16D52, after the comparison of
 * n with 32, a bcs.b sends short lengths to the byte loop; the instruction
 * after it is the _BlockMoveData call. The fix replaces that branch with a
 * NOP, so every length goes through _BlockMoveData.
 *
 * A file is accepted only if CODE 1 has exactly the expected size, the branch
 * holds either the shipped bytes or the fix, a SHA-256 of the 64 bytes around
 * the branch matches, and a CRC-32 of the whole resource matches the shipped
 * game (both digests with the two patch bytes taken as shipped). The game's
 * own bytes are not stored here, only their digests.
 */
#include <stdint.h>

#include "patch.h"

const EclRef kEclShipped = {
	kEclCode1Size,
	0xE5423745UL,
	{
		0xF6, 0xA9, 0x6B, 0xA4, 0xE2, 0x1B, 0x7C, 0xAB,
		0x63, 0x8C, 0x62, 0xA0, 0x95, 0xEE, 0x2F, 0x4F,
		0x43, 0x18, 0x0B, 0x28, 0xFD, 0xBB, 0xB2, 0xE3,
		0xC3, 0xD8, 0x90, 0x06, 0x1D, 0x69, 0x91, 0x5E
	}
};

static const unsigned char kOrig[2] = { 0x65, 0x04 };  /* bcs.b *+6 */
static const unsigned char kFix[2]  = { 0x4E, 0x71 };  /* nop */

/* ---------------------------------------------------------------- CRC-32 */

static unsigned long gCRCTable[256];
static int gCRCReady = 0;

static void CRCInit(void)
{
	unsigned long c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (unsigned long)n;
		for (k = 0; k < 8; k++)
			c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
		gCRCTable[n] = c;
	}
	gCRCReady = 1;
}

static unsigned long CRCUpdate(unsigned long crc, const unsigned char *p, long len)
{
	while (len-- > 0)
		crc = gCRCTable[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	return crc & 0xFFFFFFFFUL;
}

/* --------------------------------------------------------------- SHA-256 */

static const uint32_t kSHAK[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void SHABlock(uint32_t h[8], const unsigned char *p)
{
	uint32_t w[64], a, b, c, d, e, f, g, k, t1, t2;
	int i;

	for (i = 0; i < 16; i++, p += 4)
		w[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
	for (; i < 64; i++)
		w[i] = (ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10)) + w[i - 7]
			+ (ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 16];
	a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; k = h[7];
	for (i = 0; i < 64; i++) {
		t1 = k + (ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25)) + ((e & f) ^ (~e & g)) + kSHAK[i] + w[i];
		t2 = (ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		k = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}

void EclSHA256(const unsigned char *p, long len, unsigned char digest[32])
{
	uint32_t h[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};
	unsigned char last[128];
	uint32_t bitsHi = (uint32_t)((unsigned long)len >> 29), bitsLo = (uint32_t)((unsigned long)len << 3);
	long rest, n, i;

	for (rest = len; rest >= 64; rest -= 64, p += 64)
		SHABlock(h, p);
	for (i = 0; i < rest; i++)
		last[i] = p[i];
	last[rest] = 0x80;
	n = (rest < 56) ? 64 : 128;
	for (i = rest + 1; i < n - 8; i++)
		last[i] = 0;
	for (i = 0; i < 4; i++) {
		last[n - 8 + i] = (unsigned char)(bitsHi >> (24 - 8 * i));
		last[n - 4 + i] = (unsigned char)(bitsLo >> (24 - 8 * i));
	}
	SHABlock(h, last);
	if (n == 128)
		SHABlock(h, last + 64);
	for (i = 0; i < 32; i++)
		digest[i] = (unsigned char)(h[i / 4] >> (24 - 8 * (i % 4)));
}

/* ------------------------------------------------------------ the check */

static int Same(const unsigned char *a, const unsigned char *b, long len)
{
	while (len-- > 0)
		if (*a++ != *b++)
			return 0;
	return 1;
}

EclState EclCheckRef(const unsigned char *code, long size, const EclRef *ref)
{
	const unsigned char *site;
	unsigned char window[kEclWindowLen];
	unsigned char digest[32];
	unsigned long crc;
	EclState state;
	long i;

	if (code == 0 || ref == 0 || size != ref->size || size != kEclCode1Size)
		return kEclUnknown;

	site = code + kEclPatchOffset;
	if (Same(site, kOrig, 2))
		state = kEclUnpatched;
	else if (Same(site, kFix, 2))
		state = kEclPatched;
	else
		return kEclUnknown;

	/* The 64 bytes around the branch, with the branch as shipped. */
	for (i = 0; i < kEclWindowLen; i++)
		window[i] = site[i - kEclWindowBefore];
	window[kEclWindowBefore] = kOrig[0];
	window[kEclWindowBefore + 1] = kOrig[1];
	EclSHA256(window, kEclWindowLen, digest);
	if (!Same(digest, ref->window, 32))
		return kEclUnknown;

	/* The whole resource, with the branch as shipped. */
	if (!gCRCReady)
		CRCInit();
	crc = 0xFFFFFFFFUL;
	crc = CRCUpdate(crc, code, kEclPatchOffset);
	crc = CRCUpdate(crc, kOrig, 2);
	crc = CRCUpdate(crc, site + 2, size - kEclPatchOffset - 2);
	crc ^= 0xFFFFFFFFUL;
	if (crc != ref->crc)
		return kEclUnknown;

	return state;
}

EclState EclPatchRef(unsigned char *code, long size, const EclRef *ref)
{
	EclState state = EclCheckRef(code, size, ref);

	if (state == kEclUnpatched) {
		code[kEclPatchOffset] = kFix[0];
		code[kEclPatchOffset + 1] = kFix[1];
	}
	return state;
}

EclState EclCheck(const unsigned char *code, long size)
{
	return EclCheckRef(code, size, &kEclShipped);
}

EclState EclPatch(unsigned char *code, long size)
{
	return EclPatchRef(code, size, &kEclShipped);
}
