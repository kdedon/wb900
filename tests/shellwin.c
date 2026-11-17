/*
 * A Shell window on the Workbench screen.  With a display the root
 * program runs Workbench as wbboot does and opens a Shell in a CON:
 * window in front of it: the user's session, which ends when Workbench
 * quits.  Without one it types into the Shell through input.device, finds
 * the command's output in the window's cells, and ends the Shell.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <devices/conunit.h>
#include <graphics/gfxbase.h>
#include <graphics/modeid.h>
#include <graphics/text.h>
#include <intuition/intuitionbase.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/alib.h>

extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

LONG Workbench(char *args, LONG len);
extern BOOL (*wbselftest)(void);
extern LONG (*wbselftestdone)(void);
BOOL selftest(void);
LONG selftestdone(void);

#define SHELLWINDOW "CON:0/20/640/180/AmigaShell/CLOSE"

/* Run a Shell in a new CON: window until it ends */
static LONG shell(char *args, LONG len)
{
	BPTR in, out;

	if ((in = Open(SHELLWINDOW, MODE_OLDFILE)) == NULL)
		return RETURN_FAIL;
	/* The window is this process's console, so "*" is the same window */
	out = Open("*", MODE_NEWFILE);
	if (out != NULL) {
		Execute("", in, out);
		Close(out);
	}
	Close(in);
	return out != NULL ? RETURN_OK : RETURN_FAIL;
}

struct sync {
	struct Task *s_Task;
	ULONG s_Sig;
	LONG s_RC;
};

static void done(LONG rc, struct sync *s)
{
	s->s_RC = rc;
	Signal(s->s_Task, s->s_Sig);
}

static BOOL start(LONG (*entry)(), char *name, struct sync *s, ULONG sig)
{
	struct TagItem t[6];

	s->s_Task = FindTask(NULL);
	s->s_Sig = sig;
	s->s_RC = -1;
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)entry;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)name;
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 3072;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (ULONG)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (ULONG)s;
	t[5].ti_Tag = TAG_DONE;
	return CreateNewProc(t) != NULL;
}

/* Without a display: type and read the window back */

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL shellwin: %s\n", what);
		fails++;
	}
}

static struct IOStdReq *io;

static void event(UBYTE class, UWORD code)
{
	struct InputEvent ev;

	ev.ie_NextEvent = NULL;
	ev.ie_Class = class;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = 0;
	ev.ie_X = ev.ie_Y = 0;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
}

static UWORD rawcode(char c)
{
	static char *rows[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
	static UWORD first[3] = { 0x10, 0x20, 0x31 };
	int r, i;

	if (c == '\n')
		return RAWKEY_RETURN;
	for (r = 0; r < 3; r++)
		for (i = 0; rows[r][i]; i++)
			if (rows[r][i] == c)
				return first[r] + i;
	return RAWKEY_SPACE;
}

static void type(char *s)
{
	for (; *s; s++) {
		event(IECLASS_RAWKEY, rawcode(*s));
		event(IECLASS_RAWKEY, rawcode(*s) | IECODE_UP_PREFIX);
	}
	event(IECLASS_NULL, 0);
	TimeDelay(UNIT_VBLANK, 0, 400000);
}

/* Does row y of the console show s? */
static BOOL rowis(struct ConUnit *cu, int y, char *s)
{
	struct RastPort *rp = cu->cu_Window->RPort;
	struct TextFont *tf = rp->Font;
	int x, i, j, px, py, ch;
	ULONG loc, bit;
	UBYTE *glyph = (UBYTE *)tf->tf_CharData;

	for (x = 0; s[x]; x++) {
		ch = s[x];
		loc = ((ULONG *)tf->tf_CharLoc)[ch - tf->tf_LoChar] >> 16;
		px = cu->cu_XROrigin + x * cu->cu_XRSize;
		py = cu->cu_YROrigin + y * cu->cu_YRSize;
		for (j = 0; j < cu->cu_YRSize; j++)
			for (i = 0; i < cu->cu_XRSize; i++) {
				bit = loc + i;
				if ((ReadPixel(rp, px + i, py + j) != 0) !=
				    ((glyph[j * tf->tf_Modulo + (bit >> 3)] >>
				    (7 - (bit & 7))) & 1))
					return FALSE;
			}
	}
	return TRUE;
}

/* The console unit of the active window's CON: handler */
static struct ConUnit *activeunit(void)
{
	struct Process *p;
	struct FileHandle *fh;
	struct InfoData id;
	BPTR f;
	struct ConUnit *cu = NULL;

	p = (struct Process *)FindTask("Shell Process");
	if (p == NULL || (f = p->pr_COS) == NULL)
		return NULL;
	fh = (struct FileHandle *)f;
	if (DoPkt(fh->fh_Type, ACTION_DISK_INFO, (LONG)&id, 0, 0, 0, 0))
		cu = (struct ConUnit *)((struct IOStdReq *)id.id_InUse)->io_Unit;
	return cu;
}

static void drive(struct sync *s)
{
	struct ConUnit *cu;
	WORD y;

	int n;

	for (n = 0; n < 50 && (cu = activeunit()) == NULL; n++)
		TimeDelay(UNIT_VBLANK, 0, 100000);
	if (cu == NULL)
		xKPrintf("shellwin: Shell ended with %ld\n", s->s_RC);
	check(cu != NULL, "Shell window");
	if (cu == NULL)
		return;
	check(IntuitionBase->ActiveWindow == cu->cu_Window, "window active");
	y = cu->cu_YCP;
	type("echo hello\n");
	check(cu->cu_YCP == y + 2, "a line of output");
	check(rowis(cu, y + 1, "hello"), "Echo's output in the window");
	type("endshell\n");
	TimeDelay(UNIT_VBLANK, 1, 0);
	check(s->s_RC == RETURN_OK, "EndShell");
	check(IntuitionBase->ActiveWindow == NULL ||
	    IntuitionBase->ActiveWindow != cu->cu_Window, "window closed");
}

int main(void)
{
	struct MsgPort *port;
	struct sync wb, sh;
	BOOL display;

	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	wbselftest = selftest;
	wbselftestdone = selftestdone;
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	if (DOSBase == NULL || IntuitionBase == NULL || GfxBase == NULL ||
	    OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL shellwin: OpenLibrary\n");
		return 1;
	}
	display = NextDisplayInfo(INVALID_ID) != INVALID_ID;
	SetSignal(0L, SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
	OpenWorkBench();
	if (display) {
		KPutStr("shellwin: starting Workbench\n");
		if (!start((LONG (*)())Workbench, "Workbench", &wb,
		    SIGBREAKF_CTRL_F)) {
			KPutStr("FAIL shellwin: Workbench\n");
			return 1;
		}
		/* the Shell window opens in front of Workbench's */
		TimeDelay(UNIT_VBLANK, 2, 0);
	}
	KPutStr("shellwin: opening " SHELLWINDOW "\n");
	if (!start((LONG (*)())shell, "Shell", &sh, SIGBREAKF_CTRL_E)) {
		KPutStr("FAIL shellwin: Shell\n");
		return 1;
	}
	if (display) {
		Wait(SIGBREAKF_CTRL_F);
		xKPrintf("shellwin: Workbench ended, result %ld\n", wb.s_RC);
		return 0;
	}
	drive(&sh);
	CloseWorkBench();
	if (fails != 0)
		return 1;
	KPutStr("PASS shellwin\n");
	return 0;
}
