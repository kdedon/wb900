/* SPDX-License-Identifier: MIT */
/*
 * An alert raised on input.device's task, which polls gameport.device's
 * buttons on the HR mouse: a left press and release answers TRUE, a right
 * one FALSE.  The alert has no timeout.
 */
/* EMUFLAGS: --hr --mouse=3000000:0:0:4 --mouse=4000000:0:0:0 --mouse=9000000:0:0:1 --mouse=10000000:0:0:0 */
/* HWMARK: gpalert: press left */
#include <exec/exec.h>
#include <devices/gameport.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL gpalert: %s\n", what);
		fails++;
	}
}

static UBYTE alerttext[] = "\000(\024Button alert\000";

/* A NULL event with code ALERTEVENT raises the alert in the handler */
#define ALERTEVENT	0xA1E
static volatile BOOL ret, done;

static struct InputEvent *raiser(struct InputEvent *ie, APTR data)
{
	struct InputEvent *e;

	for (e = ie; e; e = e->ie_NextEvent)
		if (e->ie_Class == IECLASS_NULL && e->ie_Code == ALERTEVENT) {
			ret = DisplayAlert(RECOVERY_ALERT, alerttext, 40);
			done = TRUE;
		}
	return ie;
}

static struct IOStdReq *io;

static void alert(char *mark, BOOL want, char *what)
{
	static struct InputEvent ev;

	ev.ie_Class = IECLASS_NULL;
	ev.ie_Code = ALERTEVENT;
	ret = !want;
	done = FALSE;
	xKPrintf("gpalert: %s\n", mark);
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
	check(done && ret == want, what);
}

int main(void)
{
	struct Interrupt h;
	struct Node *gp;

	io = (struct IOStdReq *)CreateIORequest(CreateMsgPort(), sizeof(*io));
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	if (io == NULL || IntuitionBase == NULL ||
	    OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL gpalert: setup\n");
		return 1;
	}
	Forbid();
	gp = FindName(&SysBase->DeviceList, GAMEPORTNAME);
	Permit();
	check(gp != NULL, "gameport.device");

	h.is_Node.ln_Type = NT_INTERRUPT;
	h.is_Node.ln_Pri = 60;
	h.is_Node.ln_Name = "alert raiser";
	h.is_Code = (int (*)())raiser;
	h.is_Data = NULL;
	io->io_Command = IND_ADDHANDLER;
	io->io_Data = &h;
	DoIO((struct IORequest *)io);
	alert("press left", TRUE, "left button");
	alert("press right", FALSE, "right button");
	io->io_Command = IND_REMHANDLER;
	io->io_Data = &h;
	DoIO((struct IORequest *)io);
	if (fails != 0)
		return 1;
	KPutStr("PASS gpalert\n");
	return 0;
}
