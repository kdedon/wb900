/* SPDX-License-Identifier: MIT */
/*
 * The Workbench boot: the root program runs Workbench as a process and
 * waits for it to quit.  With a display that is the user's session;
 * without one Workbench drives itself through its disk and quits, and
 * the run passes when its checks did and the Workbench screen is closed.
 * The self test writes icons, so it runs on a copy of the disk, which
 * boots twice: the second run finds what the first left out.
 */
/* BOOTS: 2 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuitionbase.h>
#include <proto/dos.h>

extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;

LONG Workbench(char *args, LONG len);

struct sync {
	struct Task *s_Task;
	LONG s_RC;
};

static void done(LONG rc, struct sync *s)
{
	s->s_RC = rc;
	Signal(s->s_Task, SIGBREAKF_CTRL_F);
}

int main(void)
{
	struct IntuitionBase *ib;
	struct DosLibrary *dos;
	struct TagItem t[6];
	struct sync s;

	if ((dos = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L)) == NULL ||
	    (ib = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L))
	    == NULL) {
		KPutStr("FAIL wbboot: OpenLibrary\n");
		return 1;
	}
	DOSBase = dos;
	s.s_Task = FindTask(NULL);
	s.s_RC = -1;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)Workbench;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"Workbench";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 3072;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (ULONG)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (ULONG)&s;
	t[5].ti_Tag = TAG_DONE;
	KPutStr("wbboot: starting Workbench\n");
	if (CreateNewProc(t) == NULL) {
		KPutStr("FAIL wbboot: CreateNewProc\n");
		return 1;
	}
	Wait(SIGBREAKF_CTRL_F);
	xKPrintf("wbboot: Workbench ended, result %ld\n", s.s_RC);
	if (ib->FirstScreen != NULL) {
		KPutStr("FAIL wbboot: a screen is still open\n");
		return 1;
	}
	KPutStr("wbboot: the display is closed\n");
	if (s.s_RC != RETURN_OK)
		return 1;
	KPutStr("PASS wbboot\n");
	return 0;
}
