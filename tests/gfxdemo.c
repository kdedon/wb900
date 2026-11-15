/* SPDX-License-Identifier: MIT */
/*
 * graphics.library on the HR card: the display database, a View on the
 * display's own bitmap in VRAM, and a picture to check on the screen:
 * a double border, two diagonals, four filled rectangles,
 * "wb900 graphics.library" in topaz 8, and the pointer with its tip at
 * (120, 64).  The picture stays on the display when the test ends.
 * Without a display (the C emulator has no HR card) it checks that the
 * database is empty.
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
#include <resources/framebuf.h>

struct GfxBase *GfxBase;
APTR FrameBufBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL gfxdemo: %s\n", what);
		fails++;
	}
}

#define EXTTAGS	(BMF_RTGTAGS | BMF_RTGCHECK | BMF_FRIENDISTAG)
#define PTRX	120
#define PTRY	64

static char title[] = "wb900 graphics.library";

/* Pens 0-7, then the pointer's 17-19: white, black, red */
static ULONG palette[] = {
	8L << 16 | 0,
	0, 0, 0,  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0, 0,  0, 0xFFFFFFFF, 0,  0, 0, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0,  0, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0, 0xFFFFFFFF,
	3L << 16 | 17,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,  0, 0, 0,  0xFFFFFFFF, 0, 0,
	0
};

/* Mono greys: solid, checks, lines across, lines down */
static UWORD ptrns[4][2] = {
	{ 0xFFFF, 0xFFFF }, { 0xAAAA, 0x5555 }, { 0xFFFF, 0x0000 },
	{ 0x8888, 0x8888 }
};

/*
 * An arrow 11 rows high in the old sprite format: two control words,
 * then plane 0 and plane 1 of each row.  Edge colour 1, inside 2.
 */
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

/* No HR card: no modes, and graphics still draws in RAM */
static void nodisplay(void)
{
	struct DisplayInfo di;

	check(NextDisplayInfo(INVALID_ID) == INVALID_ID &&
	    BestModeIDA(NULL) == INVALID_ID &&
	    GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof(di), DTAG_DISP,
	    HRMONO_800_ID) == 0, "no display, no modes");
	KPutStr("gfxdemo: no display\n");
}

static void box(struct RastPort *rp, WORD x0, WORD y0, WORD x1, WORD y1)
{
	Move(rp, x0, y0);
	Draw(rp, x1, y0);
	Draw(rp, x1, y1);
	Draw(rp, x0, y1);
	Draw(rp, x0, y0);
}

static void picture(struct RastPort *rp, WORD w, WORD h, BOOL colour,
    char *name)
{
	WORD i, x, y, rw, rh;

	SetRast(rp, 0);
	SetAPen(rp, 1);
	SetDrMd(rp, JAM1);
	box(rp, 0, 0, w - 1, h - 1);
	box(rp, 4, 4, w - 5, h - 5);
	Move(rp, 8, 8);
	Draw(rp, w - 9, h - 9);
	Move(rp, w - 9, 8);
	Draw(rp, 8, h - 9);

	rw = w / 6;
	rh = h / 6;
	y = h - rh - 24;
	for (i = 0; i < 4; i++) {
		x = 24 + i * (w / 4);
		if (colour) {
			SetAPen(rp, 2 + i);
		} else {
			SetAPen(rp, 1);
			SetAfPt(rp, ptrns[i], 1);
		}
		RectFill(rp, x, y, x + rw - 1, y + rh - 1);
	}
	SetAfPt(rp, NULL, 0);

	SetAPen(rp, 1);
	Move(rp, 16, 24 + rp->TxBaseline);
	Text(rp, title, sizeof(title) - 1);
	SetBPen(rp, colour ? 4 : 1);
	SetAPen(rp, colour ? 5 : 0);
	SetDrMd(rp, JAM2);
	Move(rp, 16, 36 + rp->TxBaseline);
	for (i = 0; name[i]; i++)
		;
	Text(rp, name, i);
	SetDrMd(rp, JAM1);
}

int main(void)
{
	struct TagItem t[2];
	struct DimensionInfo dims;
	struct DisplayInfo disp;
	struct NameInfo ni;
	struct FBInfo *fi;
	struct View v;
	struct ViewPort vp;
	struct RasInfo ri;
	struct RastPort rp;
	struct BitMap *bm, *bm2;
	struct ColorMap *cm;
	struct ExtSprite *es;
	struct TextFont *tf;
	struct TextAttr topaz8;
	static struct TagItem old[] = {
		{ SPRITEA_OldDataFormat, TRUE }, { TAG_DONE, 0 }
	};
	ULONG id;
	WORD w, h;
	BOOL colour;

	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 45L);
	if (GfxBase == NULL) {
		KPutStr("FAIL gfxdemo: OpenLibrary\n");
		return 1;
	}
	FrameBufBase = OpenResource(FRAMEBUFNAME);
	id = BestModeIDA(NULL);
	if (FrameBufBase == NULL) {
		nodisplay();
		goto done;
	}

	/* The database: the card's mode first */
	fi = GetFBInfo();
	check(id != INVALID_ID && NextDisplayInfo(INVALID_ID) == id &&
	    FindDisplayInfo(id) != NULL, "the display's mode");
	check(ModeNotAvailable(id) == 0, "ModeNotAvailable");
	check(GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS,
	    id) == sizeof(dims) && dims.Header.DisplayID == id &&
	    dims.Nominal.MaxX + 1 == fi->fbi_Modes[fi->fbi_Mode].fbm_Width &&
	    dims.Nominal.MaxY + 1 == fi->fbi_Modes[fi->fbi_Mode].fbm_Height,
	    "DTAG_DIMS");
	check(GetDisplayInfoData(FindDisplayInfo(id), (UBYTE *)&disp,
	    sizeof(disp), DTAG_DISP, INVALID_ID) == sizeof(disp) &&
	    disp.NotAvailable == 0 && (disp.PropertyFlags & DIPF_IS_WB) &&
	    disp.PaletteRange == 1 << dims.MaxDepth, "DTAG_DISP");
	check(GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, id)
	    == sizeof(ni), "DTAG_NAME");
	w = dims.Nominal.MaxX + 1;
	h = dims.Nominal.MaxY + 1;
	colour = dims.MaxDepth > 1;
	t[0].ti_Tag = BIDTAG_Depth;
	t[0].ti_Data = dims.MaxDepth + 1;
	t[1].ti_Tag = TAG_DONE;
	check(BestModeIDA(t) == INVALID_ID, "BestModeIDA, too deep");
	xKPrintf("gfxdemo: mode %lx %s, %dx%dx%d, resolution %d:%d\n", id,
	    ni.Name, w, h, dims.MaxDepth, disp.Resolution.x,
	    disp.Resolution.y);

	/* The screen's bitmap is the display's, in VRAM */
	t[0].ti_Tag = BMATags_DisplayID;
	t[0].ti_Data = id;
	bm = AllocBitMap(w, h, dims.MaxDepth, BMF_CLEAR | BMF_DISPLAYABLE |
	    EXTTAGS, (struct BitMap *)t);
	check(bm != NULL && bm->Planes[0] ==
	    fi->fbi_Modes[fi->fbi_Mode].fbm_Half[0] &&
	    GetBitMapAttr(bm, BMA_WIDTH) == w, "AllocBitMap in VRAM");
	bm2 = AllocBitMap(w, h, dims.MaxDepth, EXTTAGS, (struct BitMap *)t);
	check(bm2 != NULL && bm2->Planes[0] != bm->Planes[0],
	    "a second bitmap of the mode, in RAM");
	if (bm2)
		FreeBitMap(bm2);
	cm = GetColorMap(32);
	if (bm == NULL || cm == NULL)
		goto done;

	InitView(&v);
	InitVPort(&vp);
	ri.Next = NULL;
	ri.BitMap = bm;
	ri.RxOffset = ri.RyOffset = 0;
	vp.RasInfo = &ri;
	vp.ColorMap = cm;
	vp.DWidth = w;
	vp.DHeight = h;
	cm->VPModeID = id;
	v.ViewPort = &vp;
	check(GetVPModeID(&vp) == id, "GetVPModeID");
	LoadRGB32(&vp, palette);
	check(MakeVPort(&v, &vp) == MVP_OK && MrgCop(&v) == MCOP_OK,
	    "MakeVPort, MrgCop");
	LoadView(&v);
	check(GfxBase->ActiView == &v, "LoadView");

	InitRastPort(&rp);
	rp.BitMap = bm;
	topaz8.ta_Name = "topaz.font";
	topaz8.ta_YSize = 8;
	topaz8.ta_Style = 0;
	topaz8.ta_Flags = 0;
	tf = OpenFont(&topaz8);
	check(tf != NULL && tf->tf_YSize == 8, "topaz 8");
	if (tf)
		SetFont(&rp, tf);
	picture(&rp, w, h, colour, (char *)ni.Name);
	check(ReadPixel(&rp, 0, 0) == 1 && ReadPixel(&rp, w - 1, h - 1) == 1 &&
	    ReadPixel(&rp, 2, 2) == 0 && ReadPixel(&rp, w / 2, 20) == 0,
	    "the picture");

	makearrow();
	es = AllocSpriteDataA((struct BitMap *)arrow, old);
	check(es != NULL, "AllocSpriteDataA");
	if (es) {
		check(ChangeExtSpriteA(&vp, NULL, es, NULL), "ChangeExtSpriteA");
		MoveSprite(&vp, &es->es_SimpleSprite, PTRX, PTRY);
		check(ReadPixel(&rp, PTRX, PTRY) == 0, "ReadPixel under the pointer");
	}
	WaitTOF();

done:
	if (fails != 0)
		return 1;
	KPutStr("PASS gfxdemo\n");
	return 0;
}
