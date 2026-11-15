/*
 * commodities.library from LIBS:.  The root program checks ParseIX and
 * MatchIX, then writes events to input.device and checks what brokers
 * of filter, sender, signal, translate and custom objects do with them
 * by what a handler behind commodities' sees: hotkeys, translation,
 * priorities, AddIEvents, broker commands and CopyBrokerList.  It
 * reports the input task's stack use.
 */
#include <exec/exec.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <libraries/commodities.h>
#include <proto/commodities.h>
#include <proto/alib.h>

struct Library *CxBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL commodities: %s\n", what);
		fails++;
	}
}

/* The key events that got past commodities */
static struct {
	UWORD keys;
	UWORD code, qual;
} seen;

static struct InputEvent *behind(struct InputEvent *events, APTR data)
{
	struct InputEvent *ie;

	for (ie = events; ie != NULL; ie = ie->ie_NextEvent)
		if (ie->ie_Class == IECLASS_RAWKEY) {
			seen.keys++;
			seen.code = ie->ie_Code;
			seen.qual = ie->ie_Qualifier;
		}
	return events;
}

static struct Interrupt handler;
static struct MsgPort *port, *ioport;
static struct IOStdReq *io;
static struct InputEvent ev;

/* A key written to input.device: whether it got past commodities */
static BOOL key(UWORD code, UWORD qual)
{
	UWORD n = seen.keys;

	ev.ie_NextEvent = NULL;
	ev.ie_Class = IECLASS_RAWKEY;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = qual;
	ev.ie_X = ev.ie_Y = 0;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
	return seen.keys != n;
}

static IX ix;

static BOOL parse(char *desc, UWORD code, UWORD qual)
{
	ix.ix_Version = IX_VERSION;
	return ParseIX(desc, &ix) == 0 && ix.ix_Class == IECLASS_RAWKEY &&
	    ix.ix_Code == code && ix.ix_Qualifier == qual;
}

static void parsing(void)
{
	struct InputEvent ie;

	check(parse("ctrl x", 0x32, IEQUALIFIER_CONTROL), "ctrl x");
	check(parse("lshift F1", RAWKEY_F1, IEQUALIFIER_LSHIFT), "lshift F1");
	check(parse("f10", RAWKEY_F10, 0), "f10");
	check(parse("ctrl 1", 0x01, IEQUALIFIER_CONTROL), "ctrl 1");
	check(parse("numericpad 1", RAWKEY_KP_1, IEQUALIFIER_NUMERICPAD),
	    "numericpad 1");
	check(parse("alt cursor_up", RAWKEY_UP, IXSYM_ALTMASK) &&
	    ix.ix_QualSame == IXSYM_ALT, "alt cursor_up");
	check(parse("-shift help", RAWKEY_HELP, 0) &&
	    (ix.ix_QualMask & IXSYM_SHIFTMASK) == 0, "-shift help");
	check(parse("upstroke a", 0x20 | IECODE_UP_PREFIX, 0), "upstroke a");
	check(parse("-upstroke a", 0x20, 0) &&
	    (ix.ix_CodeMask & IECODE_UP_PREFIX) == 0, "-upstroke a");
	ix.ix_Version = IX_VERSION;
	check(ParseIX("rawmouse lbutton mouse_leftpress", &ix) == 0 &&
	    ix.ix_Class == IECLASS_RAWMOUSE && ix.ix_Code == IECODE_LBUTTON &&
	    ix.ix_Qualifier == IEQUALIFIER_LEFTBUTTON, "rawmouse");
	check(ParseIX("ctrl a b", &ix) == -1, "trailing junk");
	check(ParseIX(NULL, &ix) == -2, "NULL description");

	parse("alt cursor_up", RAWKEY_UP, IXSYM_ALTMASK);
	ie.ie_Class = IECLASS_RAWKEY;
	ie.ie_Code = RAWKEY_UP;
	ie.ie_Qualifier = IEQUALIFIER_RALT | IEQUALIFIER_NUMERICPAD;
	check(!MatchIX(&ie, &ix), "MatchIX extra qualifier");
	ie.ie_Qualifier = IEQUALIFIER_RALT;
	check(MatchIX(&ie, &ix), "MatchIX right alt");
	ie.ie_Qualifier = IEQUALIFIER_LALT | IEQUALIFIER_RELATIVEMOUSE;
	check(MatchIX(&ie, &ix), "MatchIX left alt");
	ie.ie_Code = RAWKEY_DOWN;
	check(!MatchIX(&ie, &ix), "MatchIX code");

	check(InvertKeyMap('a', &ie, NULL) && ie.ie_Code == 0x20 &&
	    ie.ie_Qualifier == 0, "InvertKeyMap a");
	check(InvertKeyMap('A', &ie, NULL) && ie.ie_Code == 0x20 &&
	    (ie.ie_Qualifier & IXSYM_SHIFTMASK), "InvertKeyMap A");
}

static struct NewBroker nb = {
	NB_VERSION, "Test", "Commodities test", "Checks the library",
	NBU_UNIQUE | NBU_NOTIFY, 0, 0, NULL, 0
};

/* Custom objects, on the input task */
static UWORD order[4], norder;

static void first(CxMsg *msg, CxObj *co)
{
	if (norder < 4)
		order[norder++] = 1;
}

static void second(CxMsg *msg, CxObj *co)
{
	struct InputEvent *ie = (struct InputEvent *)CxMsgData(msg);

	if (norder < 4)
		order[norder++] = 2;
	ie->ie_Qualifier |= IEQUALIFIER_LCOMMAND;
}

/* The next commodity message of the port, or NULL */
static CxMsg *next(void)
{
	return (CxMsg *)GetMsg(port);
}

static void brokers(void)
{
	static struct InputEvent shifta = {
		NULL, IECLASS_RAWKEY, 0, 0x20, IEQUALIFIER_LSHIFT };
	static struct InputEvent b = {
		&shifta, IECLASS_RAWKEY, 0, 0x35, 0 };
	struct List list;
	CxObj *broker, *broker2, *hot, *filter, *tr, *co;
	CxMsg *msg;
	LONG err;
	BYTE sig;
	UWORD n;

	nb.nb_Port = port;
	broker = CxBroker(&nb, &err);
	check(broker != NULL && err == CBERR_OK, "CxBroker");
	if (broker == NULL)
		return;
	check(CxObjType(broker) == CX_BROKER, "CxObjType");
	check(CxBroker(&nb, &err) == NULL && err == CBERR_DUP, "CBERR_DUP");
	msg = next();
	check(msg != NULL && CxMsgType(msg) == CXM_COMMAND &&
	    CxMsgID(msg) == CXCMD_UNIQUE, "CXCMD_UNIQUE");
	if (msg)
		ReplyMsg((struct Message *)msg);
	nb.nb_Version = NB_VERSION - 1;
	check(CxBroker(&nb, &err) == NULL && err == CBERR_VERSION,
	    "CBERR_VERSION");
	nb.nb_Version = NB_VERSION;

	/* A hotkey: the key is swallowed and comes as a message */
	hot = HotKey("ctrl x", port, 7);
	check(hot != NULL && CxObjError(hot) == 0, "HotKey");
	AttachCxObj(broker, hot);
	check(key(0x32, IEQUALIFIER_CONTROL), "inactive broker passes keys");
	while ((msg = next()) != NULL)
		ReplyMsg((struct Message *)msg);
	ActivateCxObj(broker, TRUE);
	check(!key(0x32, IEQUALIFIER_CONTROL), "hotkey swallowed");
	msg = next();
	check(msg != NULL && CxMsgType(msg) == CXM_IEVENT &&
	    CxMsgID(msg) == 7 &&
	    ((struct InputEvent *)CxMsgData(msg))->ie_Code == 0x32,
	    "hotkey message");
	if (msg)
		ReplyMsg((struct Message *)msg);
	check(key(0x33, IEQUALIFIER_CONTROL) && seen.code == 0x33,
	    "other keys pass");

	/* A filter with a translator: F2 becomes b, shift a */
	filter = CxFilter("f2");
	tr = CxTranslate(&b);
	AttachCxObj(filter, tr);
	AttachCxObj(broker, filter);
	n = seen.keys;
	key(RAWKEY_F2, 0);
	check(seen.keys == n + 2 && seen.code == 0x20 &&
	    seen.qual == IEQUALIFIER_LSHIFT, "translation");

	/* A signal object */
	sig = AllocSignal(-1);
	filter = CxFilter("f3");
	AttachCxObj(filter, CxSignal(FindTask(NULL), sig));
	AttachCxObj(broker, filter);
	SetSignal(0, 1L << sig);
	check(key(RAWKEY_F3, 0) && (SetSignal(0, 1L << sig) & (1L << sig)),
	    "CxSignal");
	FreeSignal(sig);

	/* Brokers by priority; a custom object changes the event */
	nb.nb_Name = "Second";
	nb.nb_Pri = 5;
	broker2 = CxBroker(&nb, &err);
	check(broker2 != NULL, "second broker");
	msg = next();
	check(msg == NULL, "Exchange is not there");
	AttachCxObj(broker2, CxCustom(second, 0));
	AttachCxObj(broker, CxCustom(first, 0));
	ActivateCxObj(broker2, TRUE);
	norder = 0;
	check(key(0x21, 0) && seen.qual == IEQUALIFIER_LCOMMAND,
	    "custom object");
	check(norder == 2 && order[0] == 2 && order[1] == 1, "priorities");

	/* AddIEvents: the events go in with the next input event */
	n = seen.keys;
	AddIEvents(&shifta);
	key(0x22, 0);
	check(seen.keys == n + 2 && seen.code == 0x22, "AddIEvents");

	NewList(&list);
	check(CopyBrokerList(&list) == 2 && list.lh_Head->ln_Succ->ln_Succ &&
	    !list.lh_Head->ln_Succ->ln_Succ->ln_Succ, "CopyBrokerList");
	FreeBrokerList(&list);
	check(IsListEmpty(&list), "FreeBrokerList");

	/* Commands: Exchange's way to disable a broker */
	check(BrokerCommand("Test", CXCMD_DISABLE) == 0, "BrokerCommand");
	msg = next();
	check(msg != NULL && CxMsgType(msg) == CXM_COMMAND &&
	    CxMsgID(msg) == CXCMD_DISABLE, "CXCMD_DISABLE");
	if (msg)
		ReplyMsg((struct Message *)msg);
	check(ActivateCxObj(broker, FALSE) == COF_ACTIVE, "ActivateCxObj");
	check(key(0x32, IEQUALIFIER_CONTROL), "disabled broker");
	check(BrokerCommand("Nobody", CXCMD_KILL) != 0, "unknown broker");

	co = CxDebug(1);
	check(co != NULL && CxObjType(co) == CX_DEBUG, "CxDebug");
	DeleteCxObj(co);
	co = CreateCxObj(42, 0, 0);
	check(co == NULL, "bad type");
	SetFilter(tr, "a");
	check(CxObjError(tr) & COERR_BADTYPE, "COERR_BADTYPE");
	ClearCxObjError(tr);
	check(CxObjError(tr) == 0, "ClearCxObjError");
	check(SetCxObjPri(broker2, -3) == 5, "SetCxObjPri");

	DeleteCxObjAll(broker2);
	DeleteCxObjAll(broker);
	while ((msg = next()) != NULL)
		ReplyMsg((struct Message *)msg);
	check(key(0x32, IEQUALIFIER_CONTROL) && seen.code == 0x32,
	    "no brokers");
}

/* The pool clears a new stack: count the bytes above the last zero. */
static void stackuse(void)
{
	struct Task *t;
	UBYTE *p, *top;

	t = FindTask("input.device");
	if (t == NULL)
		return;
	p = (UBYTE *)t->tc_SPLower + 2;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("commodities: input task stack %d of %d bytes used\n",
	    (int)(top - p), (int)(top - (UBYTE *)t->tc_SPLower));
}

int main(void)
{
	port = CreateMsgPort();
	ioport = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(ioport,
	    sizeof(struct IOStdReq));
	if (OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL commodities: input.device\n");
		return 1;
	}
	if ((CxBase = OpenLibrary("commodities.library", 0)) == NULL) {
		KPutStr("FAIL commodities: OpenLibrary\n");
		return 1;
	}
	check(CxBase->lib_Version == 50, "version");
	handler.is_Node.ln_Pri = 0;
	handler.is_Code = (int (*)())behind;
	io->io_Command = IND_ADDHANDLER;
	io->io_Data = &handler;
	DoIO((struct IORequest *)io);

	parsing();
	brokers();

	io->io_Command = IND_REMHANDLER;
	io->io_Data = &handler;
	DoIO((struct IORequest *)io);
	stackuse();
	CloseLibrary(CxBase);
	CloseDevice((struct IORequest *)io);
	if (fails != 0)
		return 1;
	KPutStr("PASS commodities\n");
	return 0;
}
