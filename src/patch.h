/*
 * patch.h - the patch itself, independent of the Mac Toolbox.
 *
 * Works on the data of resource CODE 1 of "Reversi • The Eclipse 1.2" as the
 * Resource Manager returns it (4-byte segment header, then the 68k code).
 * The same file is compiled into the Mac application and into the host test.
 */
#ifndef ECLIPSE_PATCH_H
#define ECLIPSE_PATCH_H

/* Size of CODE 1 in Eclipse 1.2 (bytes, header included). */
#define kEclCode1Size   0x229E4L

/* Offset of the patched branch in the CODE 1 resource data:
   code offset 0x16D52 plus the 4-byte segment header. */
#define kEclPatchOffset 0x16D56L

/* The window around the branch that is hashed: 0x2A bytes before the branch,
   the branch itself and 0x14 bytes after it, 64 bytes in all. */
#define kEclWindowBefore 0x2AL
#define kEclWindowLen    64L

typedef enum {
	kEclUnknown = 0,    /* not this version of the game (or altered) */
	kEclUnpatched = 1,  /* Eclipse 1.2 as shipped: needs the fix */
	kEclPatched = 2     /* Eclipse 1.2 with the fix already applied */
} EclState;

/* What a CODE 1 resource must match, with the two bytes of the branch taken
   as shipped in both digests. */
typedef struct {
	long size;                    /* size of the resource data */
	unsigned long crc;            /* CRC-32 (IEEE, as zlib) of the whole resource */
	unsigned char window[32];     /* SHA-256 of the 64-byte window */
} EclRef;

/* The values for Eclipse 1.2 as shipped. */
extern const EclRef kEclShipped;

/* Classifies a CODE 1 resource against kEclShipped. Read-only. */
EclState EclCheck(const unsigned char *code, long size);

/* Applies the fix if, and only if, EclCheck says kEclUnpatched.
   Returns the state found before patching; the buffer is changed only
   when the result is kEclUnpatched. */
EclState EclPatch(unsigned char *code, long size);

/* The same against another reference (used by the host test). */
EclState EclCheckRef(const unsigned char *code, long size, const EclRef *ref);
EclState EclPatchRef(unsigned char *code, long size, const EclRef *ref);

/* SHA-256 (FIPS 180-4) of len bytes. */
void EclSHA256(const unsigned char *p, long len, unsigned char digest[32]);

#endif
