/* SPDX-License-Identifier: MIT */
/*
 * icon.library: the built-in default icons, .info files written to and
 * read back from the V7 boot volume, tool types, position updates,
 * deletion, default icons for objects without an icon, and the pixels
 * DrawIconStateA() draws on a mono and a colour RAM screen.  The test
 * runs in a process, as Workbench does, so that the DOS calls have a
 * current directory.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/doshunks.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxdriver.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/intuitionbase.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/icon.h>

struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *IconBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL icon: %s\n", what);
		fails++;
	}
}

static int streq(char *a, char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

/* a and b into d */
static char *cat(char *d, char *a, char *b)
{
	char *p = d;

	while ((*p = *a++) != 0)
		p++;
	while ((*p++ = *b++) != 0)
		;
	return d;
}

/* The pen of pixel x, y of a planar image */
static UWORD imgpen(struct Image *im, WORD x, WORD y)
{
	UWORD wpr = (im->Width + 15) >> 4, pen = 0, d;
	UWORD *row;

	for (d = 0; d < im->Depth; d++) {
		row = im->ImageData + (d * im->Height + y) * wpr;
		if (row[x >> 4] & (0x8000 >> (x & 15)))
			pen |= 1 << d;
	}
	return pen;
}

static struct TagItem borderless[] = {
	{ ICONDRAWA_Borderless, TRUE }, { TAG_DONE, 0 }
};

/*
 * Draw icon at x, y without a frame and compare every pixel with the
 * image's pen, which a one-plane screen shows as its plane 0, and a
 * selected icon complemented in the image's planes.
 */
static int drawn(struct Screen *s, struct DiskObject *icon, ULONG state,
    struct Image *im, WORD x, WORD y)
{
	struct RastPort *rp = &s->RastPort;
	UWORD depth = GetBitMapAttr(rp->BitMap, BMA_DEPTH);
	UWORD mask = depth == 1 ? 1 : 3;
	UWORD comp = im == icon->do_Gadget.GadgetRender && state == IDS_SELECTED ?
	    3 : 0;
	WORD i, j;

	DrawIconStateA(rp, icon, NULL, x, y, state, borderless);
	for (j = 0; j < im->Height; j++)
		for (i = 0; i < im->Width; i++)
			if (ReadPixel(rp, x + i, y + j) !=
			    ((imgpen(im, i, j) ^ comp) & mask)) {
				xKPrintf("icon: pixel %d,%d is %ld, not %d\n",
				    i, j, ReadPixel(rp, x + i, y + j),
				    (imgpen(im, i, j) ^ comp) & mask);
				return FALSE;
			}
	return TRUE;
}

static UWORD pen(struct Screen *s, int which)
{
	struct DrawInfo *dri = GetScreenDrawInfo(s);
	UWORD p = dri->dri_Pens[which];

	FreeScreenDrawInfo(s, dri);
	return p;
}

/* The default icons laid out on s and drawn with and without frames */
static void drawing(struct Screen *s, char *what)
{
	struct RastPort *rp = &s->RastPort;
	struct DiskObject *icon;
	struct Rectangle r;
	struct Image *im;
	LONG type;
	WORD x = 20, y, i, n, fw, lw;
	char msg[48];

	for (type = WBDISK; type <= WBGARBAGE; type++) {
		icon = GetDefDiskObject(type);
		if (icon == NULL)
			continue;
		y = s->BarHeight + 10;
		check(LayoutIconA(icon, s, NULL), "LayoutIconA");
		im = icon->do_Gadget.GadgetRender;
		check(drawn(s, icon, IDS_NORMAL, im, x, y),
		    cat(msg, what, " normal pixels"));
		if ((icon->do_Gadget.Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHIMAGE)
			im = icon->do_Gadget.SelectRender;
		y += 40;
		check(drawn(s, icon, IDS_SELECTED, im, x, y),
		    cat(msg, what, " selected pixels"));

		/* A frame and a label below it, centred */
		y += 40;
		GetIconRectangleA(rp, icon, "Label", &r, NULL);
		fw = icon->do_Gadget.Width + 10;
		lw = TextLength(rp, "Label", 5);
		check(r.MinY == 0 && r.MaxY == icon->do_Gadget.Height + 9 +
		    rp->TxHeight && (lw > fw ? r.MinX == -(lw - fw) / 2 &&
		    r.MaxX - r.MinX + 1 == lw : r.MinX == 0 && r.MaxX == fw - 1),
		    "GetIconRectangleA");
		DrawIconStateA(rp, icon, "Label", x, y, IDS_NORMAL, NULL);
		check(ReadPixel(rp, x, y) == pen(s, SHINEPEN) &&
		    ReadPixel(rp, x + fw - 1, y + r.MaxY - rp->TxHeight) ==
		    pen(s, SHADOWPEN), "frame");
		for (n = 0, i = r.MinX; i <= r.MaxX; i++)
			if (ReadPixel(rp, x + i, y + r.MaxY - 2) == pen(s, TEXTPEN))
				n++;
		check(n > 5, "label");
		x += r.MaxX - r.MinX + 8;
		FreeDiskObject(icon);
	}
}

static struct TagItem chunkytags[] = {
	{ BMATags_GfxFormat, GDF_CHUNKY4 }, { TAG_DONE, 0 }
};

static void screens(void)
{
	struct Screen *s;
	struct BitMap *bm;
	struct TagItem t[5];

	t[0].ti_Tag = SA_Width;		t[0].ti_Data = 640;
	t[1].ti_Tag = SA_Height;	t[1].ti_Data = 200;
	t[2].ti_Tag = SA_Depth;		t[2].ti_Data = 1;
	t[3].ti_Tag = SA_Title;		t[3].ti_Data = (IPTR)"Mono";
	t[4].ti_Tag = TAG_DONE;
	s = OpenScreenTagList(NULL, t);
	check(s != NULL, "mono screen");
	if (s != NULL) {
		drawing(s, "mono");
		CloseScreen(s);
	}

	bm = AllocBitMap(640, 200, 4, BMF_CLEAR | BMF_RTGTAGS | BMF_RTGCHECK |
	    BMF_FRIENDISTAG, (struct BitMap *)chunkytags);
	check(bm != NULL, "chunky bitmap");
	if (bm == NULL)
		return;
	t[2].ti_Data = 4;
	t[3].ti_Tag = SA_BitMap;	t[3].ti_Data = (IPTR)bm;
	t[4].ti_Tag = TAG_DONE;
	s = OpenScreenTagList(NULL, t);
	check(s != NULL, "colour screen");
	if (s != NULL) {
		drawing(s, "colour");
		CloseScreen(s);
	}
	FreeBitMap(bm);
}

/* Spot checks of the built-in artwork */
static void defaults(void)
{
	static UBYTE w[] = { 24, 32, 24, 24, 22 }, h[] = { 22, 17, 21, 17, 17 };
	struct DiskObject *icon;
	struct Image *im;
	LONG type;

	for (type = WBDISK; type <= WBGARBAGE; type++) {
		icon = GetDefDiskObject(type);
		check(icon != NULL, "GetDefDiskObject");
		if (icon == NULL)
			continue;
		im = icon->do_Gadget.GadgetRender;
		check(icon->do_Magic == WB_DISKMAGIC && icon->do_Type == type &&
		    icon->do_Gadget.Width == w[type - 1] &&
		    icon->do_Gadget.Height == h[type - 1] && im != NULL &&
		    im->Depth == 2, "default icon");
		check((type == WBDISK || type == WBDRAWER) ==
		    (icon->do_DrawerData != NULL), "default drawer data");
		check(((icon->do_Gadget.Flags & GFLG_GADGHIGHBITS) ==
		    GFLG_GADGHIMAGE) == (type == WBDRAWER || type == WBGARBAGE),
		    "default select image");
		if (type == WBDISK)
			check(imgpen(im, 0, 0) == 1 && imgpen(im, 1, 1) == 3 &&
			    imgpen(im, 6, 1) == 2 && imgpen(im, 23, 0) == 0,
			    "disk artwork");
		FreeDiskObject(icon);
	}
	check(GetDefDiskObject(WBKICK) == NULL, "no WBKICK icon");
}

static char *tt[] = { "FILETYPE=text|ascii", "DONOTWAIT", NULL };

/* Write an icon, read it back and compare */
static void files(void)
{
	struct DiskObject *icon, *back;
	struct Image *a, *b;
	struct TagItem t[2];
	BPTR lock, fh;
	ULONG n, size;
	ULONG hunk = HUNK_HEADER;
	char name[32];

	lock = CreateDir("SYS:IconTest");
	check(lock != NULL, "CreateDir");
	if (lock != NULL)
		UnLock(lock);

	icon = GetDefDiskObject(WBTOOL);
	icon->do_DefaultTool = "SYS:C/Tool";
	icon->do_ToolTypes = tt;
	icon->do_CurrentX = 123;
	icon->do_CurrentY = 45;
	icon->do_StackSize = 4096;
	check(PutDiskObject("SYS:IconTest/Tool", icon), "PutDiskObject");
	back = GetDiskObject("SYS:IconTest/Tool");
	check(back != NULL, "GetDiskObject");
	if (back != NULL) {
		a = icon->do_Gadget.GadgetRender;
		b = back->do_Gadget.GadgetRender;
		size = RASSIZE(a->Width, a->Height) * a->Depth;
		for (n = 0; b && n < size / 2 &&
		    a->ImageData[n] == b->ImageData[n]; n++)
			;
		check(back->do_Type == WBTOOL && back->do_CurrentX == 123 &&
		    back->do_CurrentY == 45 && back->do_StackSize == 4096 &&
		    back->do_Gadget.Width == icon->do_Gadget.Width &&
		    back->do_Gadget.Height == icon->do_Gadget.Height &&
		    back->do_Gadget.SelectRender == NULL &&
		    back->do_DrawerData == NULL, "icon fields");
		check(b != NULL && b->Width == a->Width && b->Height == a->Height &&
		    b->Depth == a->Depth && n == size / 2, "icon image");
		check(back->do_DefaultTool != NULL &&
		    streq(back->do_DefaultTool, "SYS:C/Tool"), "default tool");
		check(back->do_ToolTypes != NULL &&
		    back->do_ToolTypes[2] == NULL &&
		    streq(back->do_ToolTypes[1], "DONOTWAIT"), "tool types");
		check(MatchToolValue(FindToolType(back->do_ToolTypes, "FILETYPE"),
		    "ascii") && !MatchToolValue(FindToolType(back->do_ToolTypes,
		    "FILETYPE"), "asc") && FindToolType(back->do_ToolTypes,
		    "DONOTWAIT") != NULL && FindToolType(back->do_ToolTypes,
		    "WAIT") == NULL, "FindToolType, MatchToolValue");

		/* Only the position */
		back->do_CurrentX = 7;
		back->do_CurrentY = 9;
		t[0].ti_Tag = ICONPUTA_OnlyUpdatePosition;	t[0].ti_Data = TRUE;
		t[1].ti_Tag = TAG_DONE;
		check(PutIconTagList("SYS:IconTest/Tool", back, t),
		    "OnlyUpdatePosition");
		FreeDiskObject(back);
		back = GetDiskObject("SYS:IconTest/Tool");
		check(back != NULL && back->do_CurrentX == 7 &&
		    back->do_CurrentY == 9 && back->do_ToolTypes != NULL,
		    "updated position");
		FreeDiskObject(back);
	}
	icon->do_DefaultTool = NULL;
	icon->do_ToolTypes = NULL;
	FreeDiskObject(icon);

	/* A drawer icon with its window */
	icon = GetDefDiskObject(WBDRAWER);
	icon->do_DrawerData->dd_NewWindow.LeftEdge = 30;
	icon->do_DrawerData->dd_NewWindow.Width = 333;
	icon->do_DrawerData->dd_Flags = DDFLAGS_SHOWALL;
	icon->do_Gadget.UserData = (APTR)WB_DISKREVISION;
	check(PutDiskObject("SYS:IconTest", icon), "drawer PutDiskObject");
	FreeDiskObject(icon);
	icon = GetDiskObject("SYS:IconTest");
	check(icon != NULL && icon->do_Type == WBDRAWER &&
	    icon->do_DrawerData != NULL &&
	    icon->do_DrawerData->dd_NewWindow.LeftEdge == 30 &&
	    icon->do_DrawerData->dd_NewWindow.Width == 333 &&
	    icon->do_DrawerData->dd_Flags == DDFLAGS_SHOWALL &&
	    icon->do_Gadget.SelectRender != NULL, "drawer data");
	FreeDiskObject(icon);

	/* Objects without an icon get a default one of their type */
	fh = Open("SYS:IconTest/Prog", MODE_NEWFILE);
	Write(fh, &hunk, 4);
	Close(fh);
	fh = Open("SYS:IconTest/Text", MODE_NEWFILE);
	Write(fh, "hello\n", 6);
	Close(fh);
	icon = GetDiskObjectNew("SYS:IconTest/Prog");
	check(icon != NULL && icon->do_Type == WBTOOL, "default tool icon");
	FreeDiskObject(icon);
	icon = GetDiskObjectNew("SYS:IconTest/Text");
	check(icon != NULL && icon->do_Type == WBPROJECT, "default project icon");
	FreeDiskObject(icon);
	icon = GetDiskObjectNew("SYS:C");
	check(icon != NULL && icon->do_Type == WBDRAWER, "default drawer icon");
	FreeDiskObject(icon);
	icon = GetDiskObjectNew("SYS:");
	check(icon != NULL && icon->do_Type == WBDISK, "default disk icon");
	FreeDiskObject(icon);
	check(GetDiskObject("SYS:IconTest/Text") == NULL &&
	    IoErr() == ERROR_OBJECT_NOT_FOUND, "no icon");

	check(DeleteDiskObject("SYS:IconTest/Tool"), "DeleteDiskObject");
	check(GetDiskObject("SYS:IconTest/Tool") == NULL, "deleted icon");

	BumpRevision(name, "Disk");
	check(streq(name, "copy_of_Disk"), "BumpRevision 1");
	BumpRevision(name, "copy_of_Disk");
	check(streq(name, "copy_2_of_Disk"), "BumpRevision 2");
	BumpRevision(name, "copy_12_of_Disk");
	check(streq(name, "copy_13_of_Disk"), "BumpRevision 13");
}

static LONG run(char *args, LONG len)
{
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	IconBase = OpenLibrary(ICONNAME, 44L);
	if (GfxBase == NULL || IntuitionBase == NULL || IconBase == NULL) {
		KPutStr("FAIL icon: OpenLibrary\n");
		return RETURN_FAIL;
	}
	KPutStr("icon: libraries open\n");
	defaults();
	files();
	screens();
	CloseLibrary(IconBase);
	return fails ? RETURN_FAIL : RETURN_OK;
}

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
	struct TagItem t[6];
	struct sync s;

	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	if (DOSBase == NULL) {
		KPutStr("FAIL icon: no dos.library\n");
		return 1;
	}
	s.s_Task = FindTask(NULL);
	s.s_RC = -1;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)run;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"IconTest";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 4096;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (ULONG)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (ULONG)&s;
	t[5].ti_Tag = TAG_DONE;
	if (CreateNewProc(t) == NULL) {
		KPutStr("FAIL icon: CreateNewProc\n");
		return 1;
	}
	Wait(SIGBREAKF_CTRL_F);
	if (s.s_RC != RETURN_OK)
		return 1;
	KPutStr("PASS icon\n");
	return 0;
}
