/* SPDX-License-Identifier: MIT */
/* LoadWB: runs Workbench in its own process until it quits. */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>

extern struct DosLibrary *DOSBase;

LONG Workbench(char *args, LONG len);

static void done(LONG rc, struct Task *t)
{
	Signal(t, SIGBREAKF_CTRL_F);
}

int main(void)
{
	struct TagItem t[6];
	LONG rc = RETURN_FAIL;

	if ((DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L)) == NULL)
		return RETURN_FAIL;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)Workbench;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"Workbench";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 3072;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (ULONG)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (ULONG)FindTask(NULL);
	t[5].ti_Tag = TAG_DONE;
	if (CreateNewProc(t) != NULL) {
		Wait(SIGBREAKF_CTRL_F);
		rc = RETURN_OK;
	}
	CloseLibrary((struct Library *)DOSBase);
	return rc;
}
