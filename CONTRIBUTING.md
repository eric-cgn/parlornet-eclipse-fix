# Contributing to Eclipse 68k Fix

Contributions are welcome: a fix, a test, a report of the patcher on a Mac or a system it has not been tried on, or a correction to the docs. Eclipse 68k Fix is part of [ParlorNet](https://github.com/eric-cgn/parlornet); changes to the server, its docs site or the patcher's page there go to the ParlorNet repository, and its [CONTRIBUTING.md](https://github.com/eric-cgn/parlornet/blob/main/CONTRIBUTING.md) applies here too.

Pull requests go to the `dev` branch; `main` takes only merges from `dev`, as releases (see Branches and releases in the README). Before you send a change, build it with `tools/build.sh` and run `python3 -m unittest discover -s tests`. A change to what the patcher writes needs a test in `test/test_patch.c`, and a run against a real copy of the game (see Testing in the README).

## Names, trademarks and terms

Reversi The Eclipse is someone else's program, and its name belongs to its owners. The patcher uses the name only to say which program it fixes. Please do the same in anything you contribute: describe the game and the bug in your own words, and don't copy the game's artwork or look.

Many period programs also came with terms: a license, a shareware notice, a service's terms of use. Some of those terms rule out the kind of study or change that working with ParlorNet would need, and some programs can't be part of ParlorNet at all because of them. Before working on the game, read the terms it came with, and work only in ways those terms allow. Don't accept terms on someone else's behalf, and don't contribute anything learned in a way the terms forbid.

This project won't accept contributions that infringe trademarks, break agreements, or otherwise run afoul of legal or social obligations, including the respect owed to the people and communities who made and still keep these games alive. Where exactly those lines fall depends on the program, the terms and the circumstances, and the project's maintainers make the final call. If you're not sure, ask before you start, or leave it out. A contribution can be declined or removed later if it turns out to cross one of these lines.

## Other people's software

The patcher never hosts, bundles or redistributes the game, and it never includes code, files or artwork from it. It holds only the digests it checks a copy against and the two bytes it changes, and it changes the player's own copy. Nothing that carries the game's bytes will be accepted: no copy of the game, no patched copy, no table of its code, in the source, the tests, the docs or the release files. Code under an open-source license may be read for facts about a file format, but the patcher's code is its own, under its MIT license.
