/*
 * layers.library from LIBS:, on a RAM bitmap: layers that overlap, their
 * ClipRects, drawing through a layer's RastPort, smart refresh backing
 * store, simple refresh damage and BeginUpdate()/EndUpdate(), depth
 * arrangement, moving and sizing, clip regions, superbitmap layers, locks
 * and teardown.  It reports the stack the deepest calls take.
 */
#include <exec/exec.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxdriver.h>
#include <graphics/rastport.h>
#include <graphics/regions.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <proto/graphics.h>
#include <proto/layers.h>

struct GfxBase *GfxBase;
struct Library *LayersBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL layers: %s\n", what);
		fails++;
	}
}

/* The lowest stack word the deepest of the calls below wrote */
static UWORD *stklow;

static void paint(void)
{
	UWORD here, *p;

	stklow = &here - 1500;
	for (p = stklow; p < &here - 40; p++)
		*p = 0xA5A5;
}

static UWORD used(void)
{
	UWORD here, *p;

	for (p = stklow; *p == 0xA5A5; p++)
		;
	return (UWORD)((char *)&here - (char *)p);
}

static UWORD maxstack;

static void stack(void)
{
	UWORD n = used();

	if (n > maxstack)
		maxstack = n;
}

static struct BitMap *bm;
static struct RastPort scr;	/* the bitmap without a layer */
static struct Layer_Info li;

static LONG pix(WORD x, WORD y)
{
	return ReadPixel(&scr, x, y);
}

/* Every pixel of the rectangle is v */
static int all(WORD x0, WORD y0, WORD x1, WORD y1, LONG v)
{
	WORD x, y;

	for (y = y0; y <= y1; y++)
		for (x = x0; x <= x1; x++)
			if (ReadPixel(&scr, x, y) != v)
				return 0;
	return 1;
}

static LONG area(struct Rectangle *r)
{
	return (LONG)(r->MaxX - r->MinX + 1) * (r->MaxY - r->MinY + 1);
}

static int inside(struct Rectangle *r, struct Rectangle *in)
{
	return r->MinX >= in->MinX && r->MaxX <= in->MaxX &&
	    r->MinY >= in->MinY && r->MaxY <= in->MaxY;
}

static int apart(struct Rectangle *a, struct Rectangle *b)
{
	return a->MinX > b->MaxX || a->MaxX < b->MinX ||
	    a->MinY > b->MaxY || a->MaxY < b->MinY;
}

/*
 * The ClipRects of l tile it; those inside cover (NULL: none) are hidden,
 * with a backing bitmap when bitmap is set, and the others visible.
 */
static int tiles(struct Layer *l, struct Rectangle *cover, int bitmap)
{
	struct ClipRect *cr;
	LONG sum = 0;

	for (cr = l->ClipRect; cr != NULL; cr = cr->Next) {
		if (!inside(&cr->bounds, &l->bounds))
			return 0;
		sum += area(&cr->bounds);
		if (cover != NULL && inside(&cr->bounds, cover)) {
			if (cr->lobs == NULL)
				return 0;
			if (bitmap != (cr->BitMap != NULL))
				return 0;
		} else if (cover == NULL || apart(&cr->bounds, cover)) {
			if (cr->lobs != NULL)
				return 0;
		} else
			return 0;
	}
	return sum == area(&l->bounds);
}

static int nest(struct SignalSemaphore *s)
{
	return s->ss_NestCount;
}

/* The bounding box of a layer's damage, screen relative */
static int damaged(struct Layer *l, WORD x0, WORD y0, WORD x1, WORD y1)
{
	struct Region *d = l->DamageList;

	return (l->Flags & LAYERREFRESH) && d->RegionRectangle != NULL &&
	    d->bounds.MinX + l->bounds.MinX == x0 &&
	    d->bounds.MinY + l->bounds.MinY == y0 &&
	    d->bounds.MaxX + l->bounds.MinX == x1 &&
	    d->bounds.MaxY + l->bounds.MinY == y1;
}

static int counthook;
static struct Rectangle hookrect;

static ULONG backfill(struct Hook *h, struct RastPort *rp, struct {
	struct Layer *l;
	WORD MinX, MinY, MaxX, MaxY;
	LONG OffsetX, OffsetY;
} *msg)
{
	counthook++;
	hookrect.MinX = msg->MinX;
	hookrect.MinY = msg->MinY;
	hookrect.MaxX = msg->MaxX;
	hookrect.MaxY = msg->MaxY;
	/* the hook renders in bitmap coordinates */
	if (rp->Layer == NULL) {
		SetAPen(rp, 1);
		RectFill(rp, msg->MinX, msg->MinY, msg->MaxX, msg->MaxY);
	}
	return 0;
}

static struct Hook fillhook;

static void layers(void)
{
	struct Layer *a, *b, *c;
	struct Rectangle rb, r;
	struct Region *reg, *old;

	InitLayers(&li);
	check(li.top_layer == NULL && li.fatten_count == -1, "InitLayers");

	/*
	 * A: simple refresh, 10,10-49,39.  B: smart refresh in front,
	 * 30,20-79,59.
	 */
	paint();
	a = CreateUpfrontLayer(&li, bm, 10, 10, 49, 39, LAYERSIMPLE, NULL);
	stack();
	check(a != NULL, "CreateUpfrontLayer simple");
	if (a == NULL)
		return;
	check(li.check_lp != NULL && li.check_lp != a && li.top_layer == a &&
	    a->Width == 40 && a->Height == 30 && a->rp->Layer == a &&
	    a->rp->BitMap == bm, "the first layer and the root layer");
	check(tiles(a, NULL, 0), "one visible layer's ClipRects");

	SetAPen(a->rp, 1);
	RectFill(a->rp, 0, 0, 39, 29);
	check(all(10, 10, 49, 39, 1) && pix(9, 10) == 0 && pix(50, 39) == 0 &&
	    pix(10, 40) == 0, "RectFill through a layer");

	b = CreateUpfrontLayer(&li, bm, 30, 20, 79, 59, LAYERSMART, NULL);
	check(b != NULL, "CreateUpfrontLayer smart");
	if (b == NULL)
		return;
	rb = b->bounds;
	check(li.top_layer == b && b->back == a && a->front == b,
	    "the new layer is in front");
	check(tiles(a, &rb, 0) && tiles(b, NULL, 0),
	    "ClipRects split under a layer in front");
	check(all(30, 20, 79, 59, 0) && all(10, 10, 29, 39, 1) &&
	    all(30, 10, 49, 19, 1), "the new layer is backfilled");

	/* drawing in the hidden part of a simple refresh layer is lost */
	SetAPen(a->rp, 1);
	RectFill(a->rp, 0, 0, 39, 29);
	check(all(30, 20, 49, 39, 0), "drawing clipped by the layer in front");

	/* B: 1 but for a hole of 0 at 2,2-5,5 */
	SetAPen(b->rp, 1);
	RectFill(b->rp, 0, 0, 49, 39);
	SetAPen(b->rp, 0);
	RectFill(b->rp, 2, 2, 5, 5);
	check(all(32, 22, 35, 25, 0) && pix(31, 21) == 1 && pix(36, 26) == 1 &&
	    pix(79, 59) == 1 && pix(80, 59) == 0, "drawing into the front layer");
	check(WhichLayer(&li, 35, 25) == b && WhichLayer(&li, 15, 15) == a &&
	    WhichLayer(&li, 100, 5) == NULL, "WhichLayer");

	/* smart refresh: what C hides is kept, drawing included */
	SetAPen(b->rp, 0);
	RectFill(b->rp, 30, 25, 35, 30);
	c = CreateUpfrontLayer(&li, bm, 55, 40, 99, 63, LAYERSMART, NULL);
	check(c != NULL, "CreateUpfrontLayer over a smart layer");
	if (c == NULL)
		return;
	r.MinX = 55; r.MinY = 40; r.MaxX = 99; r.MaxY = 63;
	check(tiles(b, &r, 1), "hidden smart ClipRects have backing bitmaps");
	check(all(55, 40, 99, 63, 0), "the covering layer is backfilled");
	SetAPen(b->rp, 0);
	RectFill(b->rp, 26, 21, 27, 22);	/* screen 56,41-57,42: hidden */
	SetAPen(c->rp, 1);
	RectFill(c->rp, 0, 0, 44, 23);
	check(all(55, 40, 99, 63, 1) && pix(54, 41) == 1,
	    "drawing into a hidden part goes to the backing store");
	paint();
	check(DeleteLayer(0, c), "DeleteLayer");
	stack();
	check(li.top_layer == b && tiles(b, NULL, 0), "ClipRects joined again");
	check(all(60, 45, 65, 50, 0) && all(56, 41, 57, 42, 0) &&
	    pix(58, 43) == 1 && pix(55, 40) == 1 && pix(79, 59) == 1 &&
	    pix(66, 51) == 1 && all(80, 40, 99, 63, 0),
	    "smart refresh content restored, the rest backfilled");

	/* B behind A: A gains the overlap as damage, backfilled */
	paint();
	check(BehindLayer(0, b), "BehindLayer");
	stack();
	check(li.top_layer == a && a->back == b && tiles(b, &a->bounds, 1) &&
	    tiles(a, NULL, 0), "layers swapped");
	check(damaged(a, 30, 20, 49, 39) && all(30, 20, 49, 39, 0) &&
	    pix(29, 20) == 1, "simple refresh damage and backfill");
	check(WhichLayer(&li, 35, 25) == a, "WhichLayer after BehindLayer");
	check(BeginUpdate(a), "BeginUpdate");
	SetAPen(a->rp, 1);
	RectFill(a->rp, 0, 0, 39, 29);
	EndUpdate(a, TRUE);
	check(all(10, 10, 49, 39, 1) && !(a->Flags & LAYERREFRESH) &&
	    a->DamageList->RegionRectangle == NULL && pix(50, 25) == 1,
	    "refreshed between BeginUpdate and EndUpdate");

	/* B in front again: its hidden part comes back from the backing store */
	paint();
	check(UpfrontLayer(0, b), "UpfrontLayer");
	stack();
	check(li.top_layer == b && b->back == a && tiles(a, &rb, 0),
	    "UpfrontLayer order");
	check(all(32, 22, 35, 25, 0) && pix(31, 21) == 1 && pix(40, 30) == 1,
	    "UpfrontLayer restores the smart layer");

	/* MoveLayerInFrontOf both ways */
	check(MoveLayerInFrontOf(a, b) && li.top_layer == a && a->back == b,
	    "MoveLayerInFrontOf forward");
	check(MoveLayerInFrontOf(b, a) && li.top_layer == b && b->back == a,
	    "MoveLayerInFrontOf back");
	check(all(32, 22, 35, 25, 0) && all(10, 10, 29, 39, 1) &&
	    damaged(a, 30, 20, 49, 39), "after MoveLayerInFrontOf");
	BeginUpdate(a);
	EndUpdate(a, TRUE);

	/* MoveLayer B right by 40: A's part under it is damaged */
	paint();
	check(MoveLayer(0, b, 40, 0), "MoveLayer");
	stack();
	check(b->bounds.MinX == 70 && b->bounds.MaxX == 119 &&
	    b->bounds.MinY == 20 && b->bounds.MaxY == 59, "moved bounds");
	check(all(72, 22, 75, 25, 0) && pix(71, 21) == 1 &&
	    all(100, 45, 105, 50, 0) && pix(110, 30) == 1 && pix(119, 59) == 1,
	    "the smart layer's content moved");
	check(damaged(a, 30, 20, 49, 39) && all(30, 20, 49, 39, 0) &&
	    all(50, 20, 69, 59, 0) && all(30, 40, 49, 59, 0),
	    "damage and backfill where the layer was");
	check(tiles(a, NULL, 0) && tiles(b, NULL, 0), "ClipRects after MoveLayer");
	BeginUpdate(a);
	SetAPen(a->rp, 1);
	RectFill(a->rp, 0, 0, 39, 29);
	EndUpdate(a, TRUE);
	check(all(10, 10, 49, 39, 1), "simple layer refreshed after the move");

	/* SizeLayer B: -10 wide, +4 high */
	paint();
	check(SizeLayer(0, b, -10, 4), "SizeLayer");
	stack();
	check(b->bounds.MaxX == 109 && b->bounds.MaxY == 63 && b->Width == 40 &&
	    b->Height == 44, "sized bounds");
	check(all(72, 22, 75, 25, 0) && pix(90, 30) == 1 && pix(109, 59) == 1 &&
	    all(70, 60, 109, 63, 0) && all(110, 20, 119, 59, 0),
	    "content kept, the new part and the old column backfilled");

	/* A clip region limits drawing to it, in layer coordinates */
	reg = NewRegion();
	check(reg != NULL, "NewRegion");
	if (reg != NULL) {
		r.MinX = 4; r.MinY = 4; r.MaxX = 9; r.MaxY = 9;
		OrRectRegion(reg, &r);
		old = InstallClipRegion(b, reg);
		check(old == NULL, "InstallClipRegion");
		/* moving a smart layer with a clip region, under another */
		c = CreateUpfrontLayer(&li, bm, 100, 0, 127, 30, LAYERSMART, NULL);
		paint();
		MoveSizeLayer(b, 1, 0, -1, 0);
		MoveSizeLayer(b, -1, 0, 1, 0);
		stack();
		if (c != NULL)
			DeleteLayer(0, c);
		SetAPen(b->rp, 0);
		RectFill(b->rp, 0, 0, 39, 43);
		check(all(74, 24, 79, 29, 0) && pix(73, 30) == 1 &&
		    pix(80, 29) == 1 && pix(90, 30) == 1,
		    "drawing clipped to the clip region");
		check(InstallClipRegion(b, NULL) == reg && b->ClipRegion == NULL,
		    "InstallClipRegion NULL");
		SetAPen(b->rp, 1);
		RectFill(b->rp, 0, 0, 39, 43);
		check(all(70, 20, 109, 63, 1), "no clip region");
		DisposeRegion(reg);
	}

	/* the backfill hook renders without a layer, in bitmap coordinates */
	fillhook.h_Entry = (ULONG (*)())backfill;
	c = CreateUpfrontHookLayer(&li, bm, 0, 0, 7, 7, LAYERSIMPLE, &fillhook,
	    NULL);
	check(c != NULL && counthook == 1 && hookrect.MinX == 0 &&
	    hookrect.MaxX == 7 && hookrect.MaxY == 7 && all(0, 0, 7, 7, 1),
	    "backfill hook");
	if (c != NULL)
		DeleteLayer(0, c);
	check(all(0, 0, 7, 7, 0), "root backfill after DeleteLayer");

	/* LockLayers locks the Layer_Info and every layer */
	LockLayers(&li);
	check(nest(&li.Lock) == 1 && nest(&a->Lock) == 1 &&
	    nest(&b->Lock) == 1 && nest(&li.check_lp->Lock) == 1, "LockLayers");
	UnlockLayers(&li);
	check(nest(&li.Lock) == 0 && nest(&a->Lock) == 0 && nest(&b->Lock) == 0,
	    "UnlockLayers");
	LockLayer(0, a);
	check(nest(&a->Lock) == 1, "LockLayer");
	UnlockLayer(a);
	check(nest(&a->Lock) == 0, "UnlockLayer");

	/* layers behind and hidden layers */
	c = CreateBehindLayer(&li, bm, 0, 0, 127, 63, LAYERSMART, NULL);
	check(c != NULL && c->front == a && c->back == li.check_lp,
	    "CreateBehindLayer");
	if (c != NULL) {
		check(all(0, 0, 9, 63, 0) && pix(15, 15) == 1,
		    "a layer behind keeps the others");
		/* b, a, c: b to directly in front of c, then back again */
		check(MoveLayerInFrontOf(b, c) && li.top_layer == a &&
		    a->back == b && b->back == c, "MoveLayerInFrontOf backwards");
		check(UpfrontLayer(0, b) && li.top_layer == b && b->back == a &&
		    a->back == c, "UpfrontLayer from the middle");
		check(ChangeLayerVisibility(a, FALSE) && !IsLayerVisible(a) &&
		    all(10, 10, 49, 39, 0) && WhichLayer(&li, 15, 15) == c,
		    "ChangeLayerVisibility hides");
		check(ChangeLayerVisibility(a, TRUE) && IsLayerVisible(a) &&
		    damaged(a, 10, 10, 49, 39), "ChangeLayerVisibility shows");
		BeginUpdate(a);
		EndUpdate(a, TRUE);
		check(DeleteLayer(0, c), "DeleteLayer behind");
	}

	check(DeleteLayer(0, b) && DeleteLayer(0, a) && li.top_layer == li.check_lp,
	    "DeleteLayer all");
	check(all(0, 0, 127, 63, 0), "everything backfilled");
	ThinLayerInfo(&li);
	check(li.check_lp == NULL && li.top_layer == NULL,
	    "ThinLayerInfo frees the root layer");
}

/* A superbitmap layer: what is hidden is in the superbitmap */
static void superbitmap(void)
{
	struct Layer_Info *pli;
	struct Layer *s, *f;
	struct BitMap *sb;
	struct RastPort srp;

	pli = NewLayerInfo();
	sb = AllocBitMap(64, 32, 1, BMF_CLEAR, NULL);
	check(pli != NULL && sb != NULL, "NewLayerInfo");
	if (pli == NULL || sb == NULL)
		return;
	InitRastPort(&srp);
	srp.BitMap = sb;
	s = CreateUpfrontLayer(pli, bm, 0, 0, 63, 31, LAYERSMART | LAYERSUPER, sb);
	f = CreateUpfrontLayer(pli, bm, 32, 0, 63, 31, LAYERSIMPLE, NULL);
	check(s != NULL && f != NULL && s->SuperBitMap == sb,
	    "superbitmap layer");
	if (s != NULL && f != NULL) {
		SetAPen(s->rp, 1);
		RectFill(s->rp, 0, 0, 63, 31);
		check(all(0, 0, 31, 31, 1) && all(32, 0, 63, 31, 0),
		    "drawing a superbitmap layer");
		check(ReadPixel(&srp, 40, 10) == 1,
		    "the hidden part is drawn in the superbitmap");
		DeleteLayer(0, f);
		check(all(0, 0, 63, 31, 1), "the superbitmap shows");
		SyncSBitMap(s);
		check(ReadPixel(&srp, 10, 10) == 1, "SyncSBitMap");
		DeleteLayer(0, s);
	}
	DisposeLayerInfo(pli);
	FreeBitMap(sb);
	SetRast(&scr, 0);
}

static struct TagItem chunkytags[] = {
	{ BMATags_GfxFormat, GDF_CHUNKY4 },
	{ TAG_DONE, 0 }
};

/* Backing bitmaps are friends of the screen bitmap: the same format */
static void friends(void)
{
	struct BitMap *cb;
	struct Layer_Info cli;
	struct Layer *l1, *l2;
	struct ClipRect *cr;
	int ok = 0;

	cb = AllocBitMap(64, 32, 4, BMF_CLEAR | BMF_RTGTAGS | BMF_RTGCHECK |
	    BMF_FRIENDISTAG, (struct BitMap *)chunkytags);
	check(cb != NULL, "chunky bitmap");
	if (cb == NULL)
		return;
	InitLayers(&cli);
	l1 = CreateUpfrontLayer(&cli, cb, 0, 0, 31, 31, LAYERSMART, NULL);
	l2 = CreateUpfrontLayer(&cli, cb, 16, 8, 47, 23, LAYERSMART, NULL);
	if (l1 != NULL && l2 != NULL) {
		for (cr = l1->ClipRect; cr != NULL; cr = cr->Next)
			if (cr->lobs != NULL && cr->BitMap != NULL &&
			    GFXBM(cr->BitMap)->gb_Driver == GFXBM(cb)->gb_Driver &&
			    cr->BitMap->Depth == 4 &&
			    !(GFXBM(cr->BitMap)->gb_Flags & GBF_DISPLAY))
				ok = 1;
		SetAPen(l1->rp, 9);
		RectFill(l1->rp, 0, 0, 31, 31);
		DeleteLayer(0, l2);
		check(ReadPixel(l1->rp, 20, 10) == 9, "chunky backing store");
	}
	check(ok, "backing bitmaps in the screen's format");
	if (l1 != NULL)
		DeleteLayer(0, l1);
	ThinLayerInfo(&cli);
	FreeBitMap(cb);
}

/*
 * The tags intuition gives: a shape, a child layer, an input shape,
 * a hidden layer and the window pointer.
 */
static void tags(void)
{
	struct Layer_Info tli;
	struct Layer *p, *k, *h;
	struct Region *shape, *input, *old;
	struct Rectangle r;
	struct TagItem t[4];
	int dummy;

	shape = NewRegion();
	input = NewRegion();
	if (shape == NULL || input == NULL)
		return;
	/* an L: the top row and the left column, 4 pixels wide */
	r.MinX = 0; r.MinY = 0; r.MaxX = 31; r.MaxY = 3;
	OrRectRegion(shape, &r);
	r.MaxX = 3; r.MaxY = 31;
	OrRectRegion(shape, &r);
	r.MinX = 0; r.MinY = 0; r.MaxX = 7; r.MaxY = 7;
	OrRectRegion(input, &r);

	InitLayers(&tli);
	t[0].ti_Tag = LA_ShapeRegion; t[0].ti_Data = (IPTR)shape;
	t[1].ti_Tag = LA_WindowPtr; t[1].ti_Data = (IPTR)&dummy;
	t[2].ti_Tag = LA_InputShape; t[2].ti_Data = (IPTR)input;
	t[3].ti_Tag = TAG_DONE; t[3].ti_Data = 0;
	p = CreateUpfrontLayerTagList(&tli, bm, 10, 10, 41, 41, LAYERSIMPLE, t);
	check(p != NULL && p->Window == (APTR)&dummy, "CreateUpfrontLayerTagList");
	if (p == NULL)
		return;
	SetAPen(p->rp, 1);
	RectFill(p->rp, 0, 0, 31, 31);
	check(all(10, 10, 41, 13, 1) && all(10, 10, 13, 41, 1) &&
	    pix(20, 20) == 0 && pix(41, 41) == 0, "drawing limited to the shape");
	check(WhichLayer(&tli, 12, 12) == p && WhichLayer(&tli, 30, 11) == NULL &&
	    WhichLayer(&tli, 20, 20) == NULL, "WhichLayer and the input shape");

	/* a child, its coordinates relative to the parent */
	t[0].ti_Tag = LA_ChildOf; t[0].ti_Data = (IPTR)p;
	t[1].ti_Tag = TAG_DONE;
	k = CreateUpfrontLayerTagList(&tli, bm, 2, 2, 9, 9, LAYERSMART, t);
	check(k != NULL && k->parent == p && k->bounds.MinX == 12 &&
	    k->front == NULL && k->back == p, "LA_ChildOf");
	if (k != NULL) {
		SetAPen(k->rp, 1);
		RectFill(k->rp, 0, 0, 7, 7);
		check(all(12, 12, 13, 19, 1) && pix(15, 15) == 0,
		    "a child is clipped to its parent's shape");
		paint();
		ChangeLayerShape(p, NULL, NULL);
		stack();
		SetAPen(k->rp, 1);
		RectFill(k->rp, 0, 0, 7, 7);
		check(p->shaperegion == NULL && all(12, 12, 19, 19, 1),
		    "ChangeLayerShape to the rectangle");
		paint();
		MoveLayer(0, p, 50, 0);
		stack();
		check(k->bounds.MinX == 62 && p->bounds.MinX == 60 &&
		    all(62, 12, 69, 19, 1) && all(10, 10, 41, 41, 0),
		    "a child moves with its parent");
		check(!IsLayerHiddenBySibling(k, FALSE), "IsLayerHiddenBySibling");
		DeleteLayer(0, k);
	}
	old = ChangeLayerShape(p, shape, NULL);
	check(old == NULL && p->shaperegion == shape, "ChangeLayerShape back");

	/* a hidden layer has no visible ClipRects and hides nothing */
	t[0].ti_Tag = LA_Hidden; t[0].ti_Data = TRUE;
	t[1].ti_Tag = TAG_DONE;
	h = CreateUpfrontLayerTagList(&tli, bm, 0, 0, 127, 63, LAYERSMART, t);
	check(h != NULL && !IsLayerVisible(h) && WhichLayer(&tli, 62, 12) == p,
	    "LA_Hidden");
	if (h != NULL)
		DeleteLayer(0, h);
	DeleteLayer(0, p);
	ThinLayerInfo(&tli);
	DisposeRegion(input);
	DisposeRegion(shape);
	SetRast(&scr, 0);
}

/* A ClipRect of 65536 pixels, more than an int counts */
static void big(void)
{
	struct BitMap *b2;
	struct RastPort rp2;
	struct Layer_Info bli;
	struct Layer *l1, *l2;

	b2 = AllocBitMap(272, 264, 1, BMF_CLEAR, NULL);
	check(b2 != NULL, "AllocBitMap 272x264");
	if (b2 == NULL)
		return;
	InitRastPort(&rp2);
	rp2.BitMap = b2;
	InitLayers(&bli);
	l1 = CreateUpfrontLayer(&bli, b2, 0, 0, 255, 255, LAYERSMART, NULL);
	if (l1 != NULL) {
		SetAPen(l1->rp, 1);
		RectFill(l1->rp, 0, 0, 255, 255);
		l2 = CreateUpfrontLayer(&bli, b2, 0, 0, 271, 263, LAYERSMART, NULL);
		check(l2 != NULL && ReadPixel(&rp2, 100, 100) == 0,
		    "a layer over a big one");
		if (l2 != NULL)
			DeleteLayer(0, l2);
		check(ReadPixel(&rp2, 0, 0) == 1 && ReadPixel(&rp2, 255, 255) == 1 &&
		    ReadPixel(&rp2, 128, 200) == 1 && ReadPixel(&rp2, 256, 0) == 0,
		    "a big ClipRect restored");
		DeleteLayer(0, l1);
	}
	ThinLayerInfo(&bli);
	FreeBitMap(b2);
}

/* Layers come and go without losing memory */
static void leaks(void)
{
	struct Layer_Info lli;
	struct Layer *l1, *l2;
	ULONG avail[3];
	int i;

	for (i = 0; i < 3; i++) {
		InitLayers(&lli);
		l1 = CreateUpfrontLayer(&lli, bm, 0, 0, 63, 31, LAYERSMART, NULL);
		l2 = CreateUpfrontLayer(&lli, bm, 20, 10, 100, 50, LAYERSIMPLE, NULL);
		MoveSizeLayer(l2, -5, 3, 7, -2);
		DeleteLayer(0, l2);
		DeleteLayer(0, l1);
		ThinLayerInfo(&lli);
		avail[i] = AvailMem(MEMF_ANY);
	}
	check(avail[1] == avail[2], "no memory lost");
}

int main(void)
{
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 45L);
	LayersBase = OpenLibrary("layers.library", 50L);
	if (GfxBase == NULL || LayersBase == NULL) {
		KPutStr("FAIL layers: OpenLibrary\n");
		return 1;
	}
	check(LayersBase->lib_Version == 50, "version");
	check(GfxBase->gb_LayersBase == (ULONG *)LayersBase, "gb_LayersBase");
	bm = AllocBitMap(128, 64, 1, BMF_CLEAR, NULL);
	if (bm == NULL) {
		KPutStr("FAIL layers: AllocBitMap\n");
		return 1;
	}
	InitRastPort(&scr);
	scr.BitMap = bm;
	layers();
	superbitmap();
	friends();
	tags();
	big();
	leaks();
	xKPrintf("layers: deepest call %d bytes of stack\n", maxstack);
	FreeBitMap(bm);
	CloseLibrary(LayersBase);
	CloseLibrary((struct Library *)GfxBase);
	if (fails != 0)
		return 1;
	KPutStr("PASS layers\n");
	return 0;
}
