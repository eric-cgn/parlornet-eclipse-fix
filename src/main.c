/*
 * Eclipse 68k Fix - fixes the host name bug in the 68k code of the user's
 * own copy of "Reversi • The Eclipse 1.2" (Lars Graulund / Freeverse, 1997).
 *
 * A window explains what it does; "Choose Eclipse…" picks the game with
 * StandardGetFile, the resource fork's CODE 1 is checked by src/patch.c, and
 * the two bytes are written back through the Resource Manager.
 *
 * 68k, System 7 or later (StandardGetFile, FSSpec calls, WaitNextEvent).
 * Strings are MacRoman: \xA5 is the bullet, \xC9 the ellipsis,
 * \xD2/\xD3 the curly double quotes, \xD5 the apostrophe.
 */
#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <Resources.h>
#include <Files.h>
#include <StandardFile.h>
#include <Processes.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include <Devices.h>
#include <SegLoad.h>
#include <Gestalt.h>
#include <string.h>

#include "patch.h"

#ifndef afpAccessDenied
#define afpAccessDenied (-5000)
#endif

enum {
	kMenuApple = 128, kMenuFile = 129, kMenuEdit = 130,
	kAppleAbout = 1,
	kFileChoose = 1, kFileQuit = 3,
	kAlertNote = 128,     /* ^0^1, OK */
	kAlertConfirm = 129,  /* ^0^1, Patch / Cancel */
	kAlertAbout = 130,
	kConfirmPatch = 1
};

#define kWinW 440
#define kWinH 232
#define kMargin 14
#define kBtnW 132
#define kBtnH 20

static WindowPtr gWin;
static ControlHandle gChoose, gQuit;
static short gAppRes;
static Boolean gDone;

static const char kIntro[] =
	"Reversi \xA5 The Eclipse 1.2 has a bug in its 68k code: before it connects "
	"to the Internet Othello Server it turns any host name shorter than 32 characters "
	"into repeats of its first letter (\xD2parlornet.org\xD3 becomes "
	"\xD2ppppppppppppp\xD3), so on a 68k Mac it can never connect.\r\r"
	"This program fixes your own copy of the game. It changes two bytes in the "
	"68k code (CODE resource 1) so that the host name is copied correctly, and "
	"nothing else. The PowerPC code is not touched; PowerPC Macs do not need the fix. "
	"Only an unchanged Eclipse 1.2 is accepted; anything else is left alone.\r\r"
	"Before you start, duplicate the game in the Finder and keep the original. "
	"Then click \xD2" "Choose Eclipse\xC9\xD3 and select the copy.";

/* ---------------------------------------------------------------- strings */

static void CToP(const char *c, Str255 p)
{
	size_t n = strlen(c);
	if (n > 255) n = 255;
	p[0] = (unsigned char)n;
	memcpy(p + 1, c, n);
}

static void PCat(Str255 dst, const unsigned char *src)
{
	short n = src[0];
	if (dst[0] + n > 255) n = 255 - dst[0];
	memcpy(dst + 1 + dst[0], src + 1, n);
	dst[0] += n;
}

static void PCatC(Str255 dst, const char *c)
{
	Str255 p;
	CToP(c, p);
	PCat(dst, p);
}

static void PCatNum(Str255 dst, long n)
{
	Str255 p;
	NumToString(n, p);
	PCat(dst, p);
}

/* Shows ^0^1 in the note alert (or the stop alert for errors). */
static void Message(Boolean isError, const unsigned char *s0, const unsigned char *s1)
{
	ParamText(s0, s1 ? s1 : (const unsigned char *)"\p", (const unsigned char *)"\p", (const unsigned char *)"\p");
	InitCursor();
	if (isError)
		StopAlert(kAlertNote, NULL);
	else
		NoteAlert(kAlertNote, NULL);
}

static void MessageC(Boolean isError, const char *a, const unsigned char *name, const char *b)
{
	Str255 s0, s1;
	CToP(a, s0);
	if (name) {
		PCatC(s0, "\xD2");
		PCat(s0, name);
		PCatC(s0, "\xD3");
	}
	CToP(b ? b : "", s1);
	Message(isError, s0, s1);
}

static void ErrorWithCode(const char *what, const unsigned char *name, OSErr err)
{
	Str255 s0, s1;
	CToP(what, s0);
	PCatC(s0, " \xD2");
	PCat(s0, name);
	PCatC(s0, "\xD3");
	CToP(" (Mac OS error ", s1);
	PCatNum(s1, err);
	PCatC(s1, "). The file was not changed.");
	Message(true, s0, s1);
}

/* ------------------------------------------------------------- the checks */

static Boolean SameFile(const FSSpec *a, const FSSpec *b)
{
	return a->vRefNum == b->vRefNum && a->parID == b->parID
		&& EqualString(a->name, b->name, false, true);
}

/* Is the chosen file a running application? */
static Boolean IsRunning(const FSSpec *spec)
{
	ProcessSerialNumber psn = { 0, kNoProcess };
	ProcessInfoRec info;
	FSSpec appSpec;

	while (GetNextProcess(&psn) == noErr) {
		memset(&info, 0, sizeof info);
		info.processInfoLength = sizeof info;
		info.processName = NULL;
		info.processAppSpec = &appSpec;
		if (GetProcessInformation(&psn, &info) == noErr && SameFile(spec, &appSpec))
			return true;
	}
	return false;
}

/* Explains why the file cannot be written, before any write is tried.
   Returns true if a message was shown. */
static Boolean CannotWrite(const FSSpec *spec)
{
	CInfoPBRec cpb;
	HParamBlockRec vpb;
	Str255 name;

	memset(&vpb, 0, sizeof vpb);
	vpb.volumeParam.ioVRefNum = spec->vRefNum;
	vpb.volumeParam.ioVolIndex = 0;
	vpb.volumeParam.ioNamePtr = NULL;
	if (PBHGetVInfoSync(&vpb) == noErr && (vpb.volumeParam.ioVAtrb & 0x8080)) {
		MessageC(true, "The disk that holds ", spec->name,
			" is locked or read-only. Copy the game to a disk you can write to, then try again.");
		return true;
	}

	memset(&cpb, 0, sizeof cpb);
	memcpy(name, spec->name, spec->name[0] + 1);
	cpb.hFileInfo.ioNamePtr = name;
	cpb.hFileInfo.ioVRefNum = spec->vRefNum;
	cpb.hFileInfo.ioDirID = spec->parID;
	cpb.hFileInfo.ioFDirIndex = 0;
	if (PBGetCatInfoSync(&cpb) == noErr && (cpb.hFileInfo.ioFlAttrib & 0x01)) {
		MessageC(true, "", spec->name,
			" is locked. Select it in the Finder, choose Get Info from the File menu, "
			"turn off \xD2Locked\xD3, then try again.");
		return true;
	}

	if (IsRunning(spec)) {
		MessageC(true, "", spec->name,
			" is running. Quit the game, then try again.");
		return true;
	}
	return false;
}

static void OpenError(const FSSpec *spec, OSErr err, Boolean forWriting)
{
	switch (err) {
	case opWrErr:
	case fBsyErr:
		MessageC(true, "", spec->name,
			" is in use (open in another program, or running). Quit that program, then try again.");
		break;
	case permErr:
	case afpAccessDenied:
		if (forWriting) {
			MessageC(true, "You do not have permission to change ", spec->name,
				". If it is on a file server, copy it to your own disk first.");
			break;
		}
		/* fall through */
	case fLckdErr:
	case wPrErr:
	case vLckdErr:
		MessageC(true, "", spec->name,
			" or its disk is locked. Unlock it (Get Info in the Finder) or copy it to "
			"a disk you can write to, then try again.");
		break;
	case eofErr:
	case mapReadErr:
		if (!forWriting) {
			MessageC(true, "", spec->name,
				" has no resources the patcher can read. It is not Reversi \xA5 The Eclipse 1.2.");
			break;
		}
		/* fall through */
	default:
		if (forWriting)
			ErrorWithCode("Could not write the fix. The file may be locked, on a locked or "
				"read-only disk, or in use:", spec->name, err);
		else
			ErrorWithCode("Could not open", spec->name, err);
		break;
	}
}

/* Opens the resource fork without loading its preloaded resources. */
static short OpenFork(const FSSpec *spec, SignedByte perm, OSErr *err)
{
	short ref;

	SetResLoad(false);
	ref = FSpOpenResFile((FSSpecPtr)spec, perm);
	SetResLoad(true);
	*err = (ref == -1) ? ResError() : noErr;
	if (ref == -1 && *err == noErr)
		*err = resFNotFound;
	UseResFile(gAppRes);
	return ref;
}

static void CloseFork(short ref)
{
	/* Never close a file the patcher did not open itself. */
	if (ref != -1 && ref != gAppRes && ref != 0 && ref != 1 && ref != 2)
		CloseResFile(ref);
	UseResFile(gAppRes);
}

/* Reads CODE 1 from an open fork and classifies it. */
static EclState CheckFork(short ref, Handle *codeOut, OSErr *err)
{
	Handle h;
	EclState st = kEclUnknown;

	*err = noErr;
	UseResFile(ref);
	h = Get1Resource('CODE', 1);
	*err = ResError();
	UseResFile(gAppRes);
	if (h != NULL && *h != NULL) {
		SignedByte hs = HGetState(h);
		HLock(h);
		st = EclCheck((const unsigned char *)*h, GetHandleSize(h));
		HSetState(h, hs);
	} else if (*err == noErr || *err == resNotFound) {
		*err = noErr;
	}
	if (codeOut)
		*codeOut = h;
	return st;
}

static Boolean ConfirmPatch(const FSSpec *spec)
{
	Str255 s0, s1;

	CToP("Fix \xD2", s0);
	PCat(s0, spec->name);
	PCatC(s0, "\xD3? This changes two bytes in its 68k code.");
	CToP(" Keep a copy of the original game in case you need it.", s1);
	ParamText(s0, s1, (const unsigned char *)"\p", (const unsigned char *)"\p");
	InitCursor();
	return CautionAlert(kAlertConfirm, NULL) == kConfirmPatch;
}

/* Writes the fix. Returns noErr or the first error. */
static OSErr WriteFix(const FSSpec *spec)
{
	OSErr err, err2;
	short ref, attrs;
	Handle h;
	EclState st;
	ParamBlockRec fpb;
	SignedByte hs;

	ref = OpenFork(spec, fsRdWrPerm, &err);
	if (ref == -1)
		return err;

	st = CheckFork(ref, &h, &err);
	if (st != kEclUnpatched || h == NULL) {
		CloseFork(ref);
		return err ? err : paramErr;
	}

	/* Patch the copy in memory while the patcher's own file is current:
	   between UseResFile(ref) and UseResFile(gAppRes) below only traps run,
	   so no segment of the patcher can be loaded from the game's file. */
	hs = HGetState(h);
	HLock(h);
	EclPatch((unsigned char *)*h, GetHandleSize(h));
	HSetState(h, hs);

	UseResFile(ref);
	/* CODE 1 is protected (it cannot be changed while that bit is set):
	   clear it for the write, then put the attributes back as they were. */
	attrs = GetResAttrs(h);
	SetResAttrs(h, attrs & ~resProtected);
	err = ResError();
	if (err == noErr) {
		ChangedResource(h);
		err = ResError();
		if (err == noErr) {
			WriteResource(h);
			err = ResError();
		}
	}
	SetResAttrs(h, attrs & ~resChanged);
	err2 = ResError();
	if (err == noErr)
		err = err2;
	if (err == noErr) {
		UpdateResFile(ref);
		err = ResError();
	}
	UseResFile(gAppRes);
	CloseFork(ref);
	memset(&fpb, 0, sizeof fpb);
	fpb.volumeParam.ioVRefNum = spec->vRefNum;
	PBFlushVolSync(&fpb);
	return err;
}

static pascal Boolean EclipseFilter(CInfoPBPtr pb)
{
	/* true hides the item: keep folders, show only applications by Eclipse's creator */
	if (pb->hFileInfo.ioFlAttrib & 0x10)
		return false;
	return pb->hFileInfo.ioFlFndrInfo.fdCreator != 'LPRV';
}

static void ChooseEclipse(void)
{
	StandardFileReply reply;
	SFTypeList types;
	FSSpec spec;
	OSErr err;
	short ref;
	EclState st;

	types[0] = 'APPL';
	StandardGetFile(NewFileFilterUPP(EclipseFilter), 1, types, &reply);
	if (!reply.sfGood)
		return;
	spec = reply.sfFile;

	SetCursor(*GetCursor(watchCursor));
	ref = OpenFork(&spec, fsRdPerm, &err);
	if (ref == -1) {
		OpenError(&spec, err, false);
		return;
	}
	if (ref == gAppRes) {
		MessageC(false, "", spec.name, " is this patcher, not Reversi \xA5 The Eclipse 1.2. Nothing was changed.");
		return;
	}
	st = CheckFork(ref, NULL, &err);
	CloseFork(ref);
	InitCursor();

	if (err != noErr && st == kEclUnknown) {
		ErrorWithCode("Could not read CODE 1 of", spec.name, err);
		return;
	}
	if (st == kEclPatched) {
		MessageC(false, "", spec.name, " is already fixed. Nothing was changed.");
		return;
	}
	if (st != kEclUnpatched) {
		MessageC(false, "", spec.name,
			" is not Reversi \xA5 The Eclipse 1.2 as shipped (or it was changed some other way). "
			"Nothing was changed.");
		return;
	}

	if (CannotWrite(&spec))
		return;
	if (!ConfirmPatch(&spec))
		return;

	SetCursor(*GetCursor(watchCursor));
	err = WriteFix(&spec);
	InitCursor();
	if (err != noErr) {
		OpenError(&spec, err, true);
		return;
	}

	/* Read it back from disk. */
	SetCursor(*GetCursor(watchCursor));
	ref = OpenFork(&spec, fsRdPerm, &err);
	st = (ref == -1) ? kEclUnknown : CheckFork(ref, NULL, &err);
	CloseFork(ref);
	InitCursor();
	if (st == kEclPatched)
		MessageC(false, "Done. ", spec.name,
			" is fixed (checked on disk) and can now connect from a 68k Mac.");
	else
		ErrorWithCode("The fix was written, but reading it back did not show it. Check", spec.name,
			err ? err : ioErr);
}

/* ----------------------------------------------------------------- window */

static void DrawDefaultOutline(ControlHandle c)
{
	Rect r = (**c).contrlRect;
	PenState ps;

	GetPenState(&ps);
	PenSize(3, 3);
	if ((**c).contrlHilite == 255)
		PenPat(&qd.gray);
	InsetRect(&r, -4, -4);
	FrameRoundRect(&r, 16, 16);
	SetPenState(&ps);
}

static void DrawWin(void)
{
	Rect r;

	SetPort(gWin);
	EraseRect(&gWin->portRect);

	TextFont(kFontIDGeneva);
	TextSize(10);
	TextFace(0);
	SetRect(&r, kMargin, kMargin, kWinW - kMargin, kWinH - kMargin - kBtnH - 12);
	TETextBox((Ptr)kIntro, sizeof kIntro - 1, &r, teFlushLeft);

	TextFont(systemFont);
	TextSize(12);
	DrawControls(gWin);
	DrawDefaultOutline(gChoose);
}

static void MakeWindow(void)
{
	Rect r, sb = qd.screenBits.bounds;
	short left = (sb.right - sb.left - kWinW) / 2;
	short top = GetMBarHeight() + 20 + ((sb.bottom - sb.top - GetMBarHeight() - 20 - kWinH) / 3);

	if (top < GetMBarHeight() + 20) top = GetMBarHeight() + 20;
	SetRect(&r, left, top, left + kWinW, top + kWinH);
	gWin = NewWindow(NULL, &r, (ConstStringPtr)"\pEclipse 68k Fix", true, noGrowDocProc, (WindowPtr)-1, true, 0);
	SetPort(gWin);

	SetRect(&r, kWinW - kMargin - kBtnW, kWinH - kMargin - kBtnH, kWinW - kMargin, kWinH - kMargin);
	gChoose = NewControl(gWin, &r, (ConstStringPtr)"\pChoose Eclipse\xC9", true, 0, 0, 1, pushButProc, 0);
	OffsetRect(&r, -(kBtnW + 20), 0);
	r.left = r.right - 72;
	gQuit = NewControl(gWin, &r, (ConstStringPtr)"\pQuit", true, 0, 0, 1, pushButProc, 0);
}

static void FlashButton(ControlHandle c)
{
	long t;
	HiliteControl(c, inButton);
	Delay(8, &t);
	HiliteControl(c, 0);
}

/* ------------------------------------------------------------------ menus */

static void About(void)
{
	InitCursor();
	Alert(kAlertAbout, NULL);
}

static void DoMenu(long choice)
{
	short menu = HiWord(choice), item = LoWord(choice);
	Str255 name;

	switch (menu) {
	case kMenuApple:
		if (item == kAppleAbout)
			About();
		else {
			GetMenuItemText(GetMenuHandle(kMenuApple), item, name);
			OpenDeskAcc(name);
		}
		break;
	case kMenuFile:
		if (item == kFileChoose)
			ChooseEclipse();
		else if (item == kFileQuit)
			gDone = true;
		break;
	case kMenuEdit:
		SystemEdit(item - 1);
		break;
	}
	HiliteMenu(0);
}

/* ----------------------------------------------------------------- events */

static void DoMouseDown(EventRecord *ev)
{
	WindowPtr w;
	ControlHandle c;
	Point p;
	short part = FindWindow(ev->where, &w);

	switch (part) {
	case inMenuBar:
		DoMenu(MenuSelect(ev->where));
		break;
	case inSysWindow:
		SystemClick(ev, w);
		break;
	case inDrag:
		DragWindow(w, ev->where, &qd.screenBits.bounds);
		break;
	case inGoAway:
		if (TrackGoAway(w, ev->where))
			gDone = true;
		break;
	case inContent:
		if (w != FrontWindow()) {
			SelectWindow(w);
			break;
		}
		p = ev->where;
		SetPort(w);
		GlobalToLocal(&p);
		if (FindControl(p, w, &c) && TrackControl(c, p, NULL)) {
			if (c == gChoose)
				ChooseEclipse();
			else if (c == gQuit)
				gDone = true;
		}
		break;
	}
}

static void DoKey(EventRecord *ev)
{
	char ch = ev->message & charCodeMask;

	if (ev->modifiers & cmdKey) {
		if (ch == '.') {
			FlashButton(gQuit);
			gDone = true;
		} else
			DoMenu(MenuKey(ch));
		return;
	}
	if (ch == '\r' || ch == 0x03) {
		FlashButton(gChoose);
		ChooseEclipse();
	} else if (ch == 0x1B) {
		FlashButton(gQuit);
		gDone = true;
	}
}

static void DoActivate(Boolean on)
{
	short h = on ? 0 : 255;
	Rect r = (**gChoose).contrlRect;

	HiliteControl(gChoose, h);
	HiliteControl(gQuit, h);
	SetPort(gWin);
	InsetRect(&r, -4, -4);
	EraseRect(&r);
	DrawControls(gWin);
	DrawDefaultOutline(gChoose);
}

int main(void)
{
	EventRecord ev;
	Handle mbar;

	InitGraf(&qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(NULL);
	InitCursor();
	FlushEvents(everyEvent, 0);
	MaxApplZone();

	gAppRes = CurResFile();

	mbar = GetNewMBar(128);
	SetMenuBar(mbar);
	AppendResMenu(GetMenuHandle(kMenuApple), 'DRVR');
	DrawMenuBar();

	MakeWindow();

	while (!gDone) {
		if (!WaitNextEvent(everyEvent, &ev, 30, NULL))
			continue;
		switch (ev.what) {
		case mouseDown:
			DoMouseDown(&ev);
			break;
		case keyDown:
		case autoKey:
			DoKey(&ev);
			break;
		case updateEvt:
			if ((WindowPtr)ev.message == gWin) {
				BeginUpdate(gWin);
				DrawWin();
				EndUpdate(gWin);
			}
			break;
		case activateEvt:
			if ((WindowPtr)ev.message == gWin)
				DoActivate((ev.modifiers & activeFlag) != 0);
			break;
		}
	}
	return 0;
}
