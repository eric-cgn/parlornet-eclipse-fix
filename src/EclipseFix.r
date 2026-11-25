/* Resources for Eclipse 68k Fix. MacRoman characters are written as \0xNN:
   A5 bullet, A9 copyright, C9 ellipsis, D2/D3 curly double quotes. */
#include "Multiverse.r"

#define CR "\0x0D"

resource 'vers' (1) {
	0x01, 0x00, release, 0x00, verUS,
	"1.0",
	"1.0, MIT license"
};

resource 'vers' (2) {
	0x01, 0x00, release, 0x00, verUS,
	"1.0",
	"Fix for Reversi \0xA5 The Eclipse 1.2"
};

resource 'SIZE' (-1) {
	reserved,
	ignoreSuspendResumeEvents,
	reserved,
	cannotBackground,
	needsActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	notHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	768 * 1024,
	512 * 1024
};

resource 'MBAR' (128) { { 128, 129, 130 } };

resource 'MENU' (128) {
	128, textMenuProc, 0x7FFFFFFD, enabled, apple,
	{
		"About Eclipse 68k Fix\0xC9", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain
	}
};

resource 'MENU' (129) {
	129, textMenuProc, 0x7FFFFFFD, enabled, "File",
	{
		"Choose Eclipse\0xC9", noIcon, "O", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Quit", noIcon, "Q", noMark, plain
	}
};

resource 'MENU' (130) {
	130, textMenuProc, 0, enabled, "Edit",
	{
		"Undo", noIcon, "Z", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Cut", noIcon, "X", noMark, plain;
		"Copy", noIcon, "C", noMark, plain;
		"Paste", noIcon, "V", noMark, plain;
		"Clear", noIcon, noKey, noMark, plain
	}
};

/* 128: messages (NoteAlert / StopAlert), text ^0^1 */
resource 'ALRT' (128) {
	{ 0, 0, 132, 380 }, 128,
	{
		OK, visible, sound1;
		OK, visible, sound1;
		OK, visible, sound1;
		OK, visible, sound1
	},
	alertPositionMainScreen
};

resource 'DITL' (128) {
	{
		{ 100, 298, 120, 366 }, Button { enabled, "OK" };
		{ 10, 64, 92, 366 }, StaticText { disabled, "^0^1" }
	}
};

/* 129: confirm (CautionAlert), text ^0^1 */
resource 'ALRT' (129) {
	{ 0, 0, 130, 380 }, 129,
	{
		OK, visible, sound1;
		OK, visible, sound1;
		OK, visible, sound1;
		OK, visible, sound1
	},
	alertPositionMainScreen
};

resource 'DITL' (129) {
	{
		{ 98, 298, 118, 366 }, Button { enabled, "Fix" };
		{ 98, 214, 118, 282 }, Button { enabled, "Cancel" };
		{ 10, 64, 88, 366 }, StaticText { disabled, "^0^1" }
	}
};

/* 130: About (static text items hold at most 255 characters each) */
resource 'ALRT' (130) {
	{ 0, 0, 194, 360 }, 130,
	{
		OK, visible, silent;
		OK, visible, silent;
		OK, visible, silent;
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DITL' (130) {
	{
		{ 162, 278, 182, 346 }, Button { enabled, "OK" };
		{ 12, 14, 30, 346 }, StaticText { disabled, "Eclipse 68k Fix 1.0" };
		{ 38, 14, 120, 346 }, StaticText { disabled,
			"Fixes the host name bug in the 68k code of Reversi \0xA5 The Eclipse 1.2 "
			"by Lars Graulund, published by Freeverse Software (1997). The game is "
			"theirs; it is not included, and this patcher is not affiliated with them."
		};
		{ 128, 14, 146, 346 }, StaticText { disabled, "MIT license." }
	}
};
