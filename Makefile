# Host-side test of the patch routine.
#
#   make test
#
# checks SHA-256 against the FIPS 180-4 examples, the stored digests against the
# values recorded from the copy examined, and the check and the patch on a
# synthetic CODE 1. The game is not in this repository; to test against a real
# copy as well, point ORIG and REF at the resource forks of your own copy and of
# a copy patched by hand (or by the patcher), e.g. the .rsrc/ files of a netatalk share:
#
#   make test ORIG=".rsrc/Reversi • The Eclipse 1.2" REF="patched.rsrc"
#
# The Mac application is built by tools/build.sh (Retro68 in Docker).

CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c99

build/test_patch: test/test_patch.c src/patch.c src/patch.h
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ test/test_patch.c src/patch.c

test: build/test_patch
	@if [ -n "$(ORIG)" ] || [ -n "$(REF)" ]; then \
		test -n "$(ORIG)" -a -n "$(REF)" || { echo "set both ORIG= and REF= (resource forks)"; exit 2; }; \
		build/test_patch "$(ORIG)" "$(REF)"; \
	else \
		build/test_patch; \
	fi

clean:
	rm -f build/test_patch

.PHONY: test clean
