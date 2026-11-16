/*
 * lc.gfxdriver under graphics.library, on a model of the LC card's 8568:
 * the display database, the display bitmap, and after every drawing step
 * (fills, patterns, text, lines, aligned and unaligned blits and scrolls,
 * the pointer) the model's bitmap must be the shadow, the chip idle and
 * every port write waited for.  Each step's port actions and their time
 * at the expected rates are printed as a cost regression, and the main
 * cases checked against their limits.
 */
#include <exec/exec.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/gfxdriver.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <graphics/sprite.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <graphics/hrmodeid.h>
#include <proto/graphics.h>

#include "lc/vdcmodel.h"
#include "lc/lctest.h"

struct GfxBase *GfxBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL lcgfx: %s\n", what);
		fails++;
	}
}

#define EXTTAGS	(BMF_RTGTAGS | BMF_RTGCHECK | BMF_FRIENDISTAG)

static struct BitMap *bm;

/* The step's check; its time at most limit microseconds (0: any) */
static void step(char *what, ULONG limit)
{
	ULONG us;

	check(lct_step(bm, what, &us), what);
	if (limit && us > limit) {
		xKPrintf("FAIL lcgfx: %s takes %ld us, over %ld\n", what, us,
		    limit);
		fails++;
	}
}

/* Pens 0 and 1: Workbench 1.x blue and white */
static ULONG palette[] = {
	2L << 16 | 0,
	0, 0x55555555, 0xAAAAAAAA,  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0
};

static UWORD checks[2] = { 0xAAAA, 0x5555 };
static UWORD lines2[2] = { 0xFFFF, 0x0000 };

static UWORD arrow[2 + 2 * 11 + 2];

static void makearrow(void)
{
	UWORD r, edge, in;

	for (r = 0; r < 11; r++) {
		if (r == 10) {
			edge = 0xFFE0;
			in = 0;
		} else {
			edge = 0x8000 | (0x8000 >> r);
			in = r > 1 ? (UWORD)(0xFFFF << (16 - r)) >> 1 &
			    ~(0x8000 >> r) : 0;
		}
		arrow[2 + 2 * r] = edge;
		arrow[3 + 2 * r] = in;
	}
}

static char line80[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*";

static void draw(struct RastPort *rp)
{
	struct BitMap *ram;
	WORD i;

	SetRast(rp, 1);
	step("clear to 1", 20000);
	SetRast(rp, 0);
	step("clear to 0", 20000);

	SetAPen(rp, 1);
	SetDrMd(rp, JAM1);
	RectFill(rp, 13, 7, 300, 90);
	step("unaligned rectangle", 0);
	SetAfPt(rp, checks, 1);
	SetBPen(rp, 0);
	SetDrMd(rp, JAM2);
	RectFill(rp, 320, 10, 600, 60);
	step("checked rectangle", 0);
	SetAfPt(rp, lines2, 1);
	RectFill(rp, 330, 70, 610, 100);
	step("lined rectangle", 0);
	SetAfPt(rp, checks, 1);
	SetDrMd(rp, JAM1);
	RectFill(rp, 20, 100, 200, 130);
	step("JAM1 pattern", 0);
	SetAfPt(rp, NULL, 0);
	SetDrMd(rp, COMPLEMENT);
	RectFill(rp, 100, 50, 400, 80);
	step("complement", 0);
	SetDrMd(rp, JAM2 | INVERSVID);
	RectFill(rp, 8, 140, 631, 150);
	step("inverse fill", 0);

	SetAPen(rp, 1);
	SetBPen(rp, 0);
	SetDrMd(rp, JAM2);
	Move(rp, 0, 160 + rp->TxBaseline);
	Text(rp, line80, 80);
	step("text line, 80 chars", 12000);
	SetDrMd(rp, JAM1);
	Move(rp, 3, 172 + rp->TxBaseline);
	Text(rp, line80, 40);
	step("text, unaligned", 0);

	Move(rp, 0, 0);
	Draw(rp, 639, 199);
	step("diagonal", 0);
	Move(rp, 639, 0);
	Draw(rp, 0, 199);
	step("other diagonal", 0);
	Move(rp, 5, 195);
	Draw(rp, 634, 180);
	step("shallow line", 0);
	Move(rp, 321, 2);
	Draw(rp, 330, 197);
	step("steep line", 0);
	Move(rp, 10, 3);
	Draw(rp, 630, 3);
	Draw(rp, 630, 196);
	step("box sides", 0);
	for (i = 0; i < 20; i++)
		WritePixel(rp, 7 * i + 1, 3 * i + 50);
	step("pixels", 0);
	check(ReadPixel(rp, 1, 50) == 1, "ReadPixel");

	SetBPen(rp, 0);
	ScrollRaster(rp, 0, 8, 0, 0, 639, 199);
	step("scroll up, full width", 36000);
	ScrollRaster(rp, 0, -8, 0, 0, 639, 199);
	step("scroll down, full width", 36000);
	ScrollRaster(rp, 0, 8, 160, 20, 479, 199);
	step("scroll up, 40 bytes", 34000);
	ScrollRaster(rp, 0, -8, 163, 20, 476, 199);
	step("scroll down, unaligned", 0);
	BltBitMap(bm, 16, 30, bm, 24, 30, 200, 50, 0xC0, 0xFF, NULL);
	step("aligned move right", 0);
	BltBitMap(bm, 40, 100, bm, 8, 100, 300, 40, 0xC0, 0xFF, NULL);
	step("aligned move left", 0);
	BltBitMap(bm, 100, 20, bm, 100, 60, 200, 100, 0xC0, 0xFF, NULL);
	step("aligned move down", 0);
	BltBitMap(bm, 104, 90, bm, 304, 10, 300, 100, 0xC0, 0xFF, NULL);
	step("window move 300x100, aligned", 15000);
	BltBitMap(bm, 100, 10, bm, 203, 90, 300, 100, 0xC0, 0xFF, NULL);
	step("window move 300x100, unaligned", 0);
	BltBitMap(bm, 0, 0, bm, 0, 0, 640, 200, 0x30, 0xFF, NULL);
	step("invert all", 0);

	ram = AllocBitMap(640, 200, 1, BMF_CLEAR, bm);
	check(ram != NULL, "a RAM bitmap");
	if (ram) {
		BltBitMap(bm, 0, 0, ram, 0, 0, 640, 200, 0xC0, 0xFF, NULL);
		step("display to RAM", 1);
		BltBitMap(ram, 0, 0, bm, 3, 0, 637, 200, 0xC0, 0xFF, NULL);
		step("RAM to display, all", 0);
		FreeBitMap(ram);
	}
}

static void pointer(struct ViewPort *vp, struct RastPort *rp)
{
	static struct TagItem old[] = {
		{ SPRITEA_OldDataFormat, TRUE }, { TAG_DONE, 0 }
	};
	struct ExtSprite *es;
	WORD i;

	makearrow();
	es = AllocSpriteDataA((struct BitMap *)arrow, old);
	check(es != NULL, "AllocSpriteDataA");
	if (es == NULL)
		return;
	check(ChangeExtSpriteA(vp, NULL, es, NULL), "ChangeExtSpriteA");
	step("pointer on", 0);
	MoveSprite(vp, &es->es_SimpleSprite, 100, 50);
	step("pointer jump", 0);
	for (i = 0; i < 4; i++) {
		MoveSprite(vp, &es->es_SimpleSprite, 103 + 3 * i, 52 + i);
		step("pointer move", 1500);
	}
	MoveSprite(vp, &es->es_SimpleSprite, 636, 195);
	step("pointer at the corner", 0);
	SetAPen(rp, 1);
	RectFill(rp, 600, 180, 639, 199);
	step("fill under the pointer", 0);
	check(ChangeExtSpriteA(vp, es, NULL, NULL), "pointer off");
	step("pointer off", 0);
	FreeSpriteData(es);
}

/* The modes, their names and ticks, and BestModeIDA()'s choices */
static void database(void)
{
	static const ULONG ids[4] = {
		LCHIRES_ID, LCLORES_ID, LCHIRESPAL_ID, LCLORESPAL_ID
	};
	static const char *names[4] = {
		"LC:640x200", "LC:320x200", "LC:640x256", "LC:320x256"
	};
	static const WORD ticks[4][2] = {
		{ 22, 53 }, { 44, 53 }, { 22, 41 }, { 44, 41 }
	};
	struct TagItem t[3];
	struct DisplayInfo di;
	struct NameInfo ni;
	ULONG id = INVALID_ID;
	UWORD i, k;

	for (i = 0; i < 4; i++) {
		id = NextDisplayInfo(id);
		check(id == ids[i], "the mode list");
		GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, id);
		for (k = 0; ni.Name[k] && ni.Name[k] == names[i][k]; k++)
			;
		check(ni.Name[k] == names[i][k], names[i]);
		check(GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof(di),
		    DTAG_DISP, id) == sizeof(di) && !di.NotAvailable &&
		    di.Resolution.x == ticks[i][0] &&
		    di.Resolution.y == ticks[i][1], "the ticks");
	}
	check(NextDisplayInfo(id) == INVALID_ID, "4 modes");
	t[0].ti_Tag = BIDTAG_DesiredWidth;
	t[1].ti_Tag = BIDTAG_DesiredHeight;
	t[2].ti_Tag = TAG_DONE;
	t[0].ti_Data = 320;
	t[1].ti_Data = 200;
	check(BestModeIDA(t) == LCLORES_ID, "best for 320x200");
	t[0].ti_Data = 600;
	t[1].ti_Data = 220;
	check(BestModeIDA(t) == LCHIRESPAL_ID, "best for 600x220");
	t[0].ti_Data = 300;
	t[1].ti_Data = 256;
	check(BestModeIDA(t) == LCLORESPAL_ID, "best for 300x256");
}

int main(void)
{
	struct TagItem t[2];
	struct DimensionInfo dims;
	struct View v;
	struct ViewPort vp;
	struct RasInfo ri;
	struct RastPort rp;
	struct ColorMap *cm;
	struct TextFont *tf;
	struct TextAttr topaz8;
	ULONG id;

	if (!lct_setup("lcgfx", 0xAA)) {
		KPutStr("FAIL lcgfx: setup\n");
		return 1;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 45L);
	if (GfxBase == NULL) {
		KPutStr("FAIL lcgfx: OpenLibrary\n");
		return 1;
	}

	database();
	id = BestModeIDA(NULL);
	check(id == LCHIRES_ID, "the LC mode");
	check(GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS,
	    id) == sizeof(dims) && dims.Nominal.MaxX == 639 &&
	    dims.Nominal.MaxY == 199 && dims.MaxDepth == 1, "DTAG_DIMS");

	t[0].ti_Tag = BMATags_DisplayID;
	t[0].ti_Data = id;
	t[1].ti_Tag = TAG_DONE;
	bm = AllocBitMap(640, 200, 1, BMF_CLEAR | BMF_DISPLAYABLE | EXTTAGS,
	    (struct BitMap *)t);
	check(bm != NULL && lct_allocs == 1, "the display bitmap");
	cm = GetColorMap(32);
	if (bm == NULL || cm == NULL)
		goto done;
	check(lcm_reg[25] == 0x87 && lcm_reg[28] == 0x1F && lcm_reg[36] == 0xF5 &&
	    lcm_reg[26] == 0x00, "bitmap mode, blank");
	check(lct_mode(bm, 60), "640x200 at 60 Hz");
	step("allocate", 40000);

	InitView(&v);
	InitVPort(&vp);
	ri.Next = NULL;
	ri.BitMap = bm;
	ri.RxOffset = ri.RyOffset = 0;
	vp.RasInfo = &ri;
	vp.ColorMap = cm;
	vp.DWidth = 640;
	vp.DHeight = 200;
	cm->VPModeID = id;
	v.ViewPort = &vp;
	LoadRGB32(&vp, palette);
	check(MakeVPort(&v, &vp) == MVP_OK && MrgCop(&v) == MCOP_OK,
	    "MakeVPort, MrgCop");
	LoadView(&v);
	check(lcm_reg[26] == 0xF2, "shown, white on blue");
	step("show", 0);

	InitRastPort(&rp);
	rp.BitMap = bm;
	topaz8.ta_Name = "topaz.font";
	topaz8.ta_YSize = 8;
	topaz8.ta_Style = 0;
	topaz8.ta_Flags = 0;
	tf = OpenFont(&topaz8);
	check(tf != NULL, "topaz 8");
	if (tf)
		SetFont(&rp, tf);
	draw(&rp);
	pointer(&vp, &rp);

	LoadView(NULL);
	check(lcm_reg[26] == 0x00, "blank once not shown");
	FreeBitMap(bm);
	check(lct_frees == 1, "the display goes back");
	if (tf)
		CloseFont(tf);
	FreeColorMap(cm);
done:
	lct_cleanup();
	if (fails == 0)
		KPutStr("PASS lcgfx\n");
	return fails != 0;
}
