Eclipse 68k Fix 1.0
===================

Eclipse 68k Fix is a small classic Macintosh application that fixes a bug in
your copy of Reversi - The Eclipse 1.2 (Lars Graulund, published by Freeverse
Software, 1997).

On 68k Macs the game cannot connect to an Internet Othello Server (IOS): it
turns any host name shorter than 32 characters into repeats of its first
letter, so "parlornet.org" is looked up as "ppppppppppppp". A real 68k Mac
does this too. PowerPC Macs are not affected; their code copies the name
correctly.

The patcher changes two bytes in the game's 68k code (resource CODE 1). The
game is not included; the patcher changes the copy you already have.

Web page: http://parlornet.org/eclipse-68k-fix.htm


Requirements
------------

A 68k Mac with System 7 or later. It also runs on PowerPC Macs, as a 68k
application, though they do not need the fix.


Using it
--------

1. Duplicate "Reversi - The Eclipse 1.2" in the Finder and keep the original.
2. Open Eclipse 68k Fix. Its window explains what it does.
3. Click "Choose Eclipse..." and select the copy. The file dialog lists only
   applications with the game's creator code (LPRV).
4. The patcher reads resource CODE 1 and then says one of:
   - the copy is already fixed (nothing changed);
   - it is not Reversi - The Eclipse 1.2 as shipped (nothing changed);
   - or it asks you to confirm, writes the fix, reads it back from disk and
     reports the result.

It refuses to write, and says why, if the file is locked, the disk is locked
or read-only, the game is running, or the file is in use.


What it changes
---------------

resource   CODE 1 ("Eclipse source"), 141,796 bytes
place      code offset $16D52 (offset $16D56 in the resource data)
bytes      65 04 (bcs.b) -> 4E 71 (nop)
unchanged  data fork (PowerPC code), Finder info, every other resource

Before writing, the patcher checks the size of CODE 1, a SHA-256 hash of the
64 bytes around the branch, and a CRC-32 of the whole resource. Any other
version or altered copy is left alone. The patcher holds only these digests,
none of the game's code.

The game's 68k code turns the host name from a C string into a Pascal string
in place, with memcpy(buf + 1, buf, n). CodeWarrior's 68k compiler expands
that memcpy inline: 32 bytes or more go to _BlockMoveData, which handles
overlapping blocks, but shorter lengths use a forward byte loop, which copies
the first letter all the way along. With the branch replaced by nop, every
length takes the _BlockMoveData path.


Files
-----

eclipse-68k-fix.hqx
eclipse-68k-fix.bin
eclipse-68k-fix.dsk


The game
--------

Reversi - The Eclipse is by Lars Graulund and was published by Freeverse
Software. This patcher is not affiliated with either and contains none of the
game.


License
-------

MIT License

Copyright (c) 2026 the ParlorNet authors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
