/* SPDX-License-Identifier: MIT */
/*
 * Hello: a Workbench tool.  It takes the WBStartup message, prints its
 * arguments on the serial line, and opens a window on the Workbench
 * screen until its close gadget is clicked or it gets Ctrl-C.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <workbench/startup.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;

static char text[] = "Hello from Workbench";

static void args(struct WBStartup *wbs)
{
	char path[64];
	LONG i;

	xKPrintf("hello: %ld args\n", wbs->sm_NumArgs);
	for (i = 0; i < wbs->sm_NumArgs; i++) {
		if (wbs->sm_ArgList[i].wa_Lock == NULL ||
		    !NameFromLock(wbs->sm_ArgList[i].wa_Lock, path, sizeof(path)))
			path[0] = 0;
		xKPrintf("hello: arg %ld %s %s\n", i, path,
		    wbs->sm_ArgList[i].wa_Name ? wbs->sm_ArgList[i].wa_Name : "");
	}
}

static void window(void)
{
	struct Window *w;
	struct IntuiMessage *im;
	struct TagItem t[9];
	ULONG sigs;
	BOOL done = FALSE;

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = 120;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = 60;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = 240;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = 60;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (IPTR)"Hello";
	t[5].ti_Tag = WA_CloseGadget;	t[5].ti_Data = TRUE;
	t[6].ti_Tag = WA_DragBar;	t[6].ti_Data = TRUE;
	t[7].ti_Tag = WA_IDCMP;		t[7].ti_Data = IDCMP_CLOSEWINDOW;
	t[8].ti_Tag = TAG_DONE;
	if ((w = OpenWindowTagList(NULL, t)) == NULL) {
		KPutStr("hello: no window\n");
		return;
	}
	SetAPen(w->RPort, 1);
	Move(w->RPort, w->BorderLeft + 20, w->BorderTop + 20);
	Text(w->RPort, text, sizeof(text) - 1);
	KPutStr("hello: window open\n");
	while (!done) {
		sigs = Wait((1L << w->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
		while ((im = (struct IntuiMessage *)GetMsg(w->UserPort)) != NULL) {
			if (im->Class == IDCMP_CLOSEWINDOW)
				done = TRUE;
			ReplyMsg((struct Message *)im);
		}
		if (sigs & SIGBREAKF_CTRL_C)
			done = TRUE;
	}
	CloseWindow(w);
	KPutStr("hello: window closed\n");
}

int main(void)
{
	struct Process *me = (struct Process *)FindTask(NULL);
	struct WBStartup *wbs = NULL;

	if (me->pr_CLI == NULL) {
		WaitPort(&me->pr_MsgPort);
		wbs = (struct WBStartup *)GetMsg(&me->pr_MsgPort);
	}
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	if (wbs != NULL && DOSBase != NULL)
		args(wbs);
	if (GfxBase != NULL && IntuitionBase != NULL)
		window();
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)DOSBase);
	if (wbs != NULL) {
		Forbid();
		ReplyMsg((struct Message *)wbs);
	}
	return 0;
}
