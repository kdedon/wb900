/*
 * input.device from DEVS:.  The root program adds two handlers and
 * checks the chain's order, IND_WRITEEVENT time stamps against
 * timer.device, the dead-key history, key repeat from IND_ADDEVENT with
 * IND_SETTHRESH/IND_SETPERIOD, timer events, CMD_STOP/CMD_START,
 * IND_REMHANDLER and PeekQualifier, then reports the input task's stack
 * use.
 */
#include <exec/exec.h>
#include <devices/inputevent.h>
#include <devices/input.h>
#include <devices/newstyle.h>
#include <devices/timer.h>
#include <proto/input.h>

struct Device *TimerBase;
struct Device *InputBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL input: %s\n", what);
		fails++;
	}
}

/* What the handlers saw, in the input task. */
static struct {
	struct Task *task;
	UWORD order;		/* 1: the first handler ran first */
	UWORD keys, repeats, timers, second;
	struct InputEvent last;	/* last key event, copied */
	struct timeval lastrep;
	BOOL repordered;
} seen;

static struct InputEvent *first(struct InputEvent *events, APTR data)
{
	struct InputEvent *ie;

	seen.task = FindTask(NULL);
	seen.order = 1;
	for (ie = events; ie != NULL; ie = ie->ie_NextEvent) {
		if (ie->ie_Class == IECLASS_TIMER) {
			seen.timers++;
		} else if (ie->ie_Class == IECLASS_RAWKEY) {
			if (ie->ie_Qualifier & IEQUALIFIER_REPEAT) {
				if (seen.repeats != 0 &&
				    CmpTime(&ie->ie_TimeStamp, &seen.lastrep) != -1)
					seen.repordered = FALSE;
				seen.lastrep = ie->ie_TimeStamp;
				seen.repeats++;
			} else
				seen.keys++;
			seen.last = *ie;
		}
	}
	return events;
}

static struct InputEvent *second(struct InputEvent *events, APTR data)
{
	if (events != NULL && events->ie_Class == IECLASS_RAWKEY &&
	    seen.order == 1)
		seen.second++;
	seen.order = 2;
	return events;
}

static struct Interrupt h1, h2;
static struct MsgPort *port;
static struct IOStdReq *io;
static struct timerequest *tio;

static BYTE cmd(UWORD command, APTR data, ULONG length)
{
	io->io_Command = command;
	io->io_Data = data;
	io->io_Length = length;
	return DoIO((struct IORequest *)io);
}

/* IND_SETTHRESH and IND_SETPERIOD take a timerequest's tr_time. */
static BYTE settime(UWORD command, ULONG micro)
{
	struct timerequest *t = (struct timerequest *)io;

	t->tr_node.io_Command = command;
	t->tr_time.tv_secs = 0;
	t->tr_time.tv_micro = micro;
	return DoIO((struct IORequest *)t);
}

static void delay(ULONG micro)
{
	tio->tr_node.io_Command = TR_ADDREQUEST;
	tio->tr_time.tv_secs = 0;
	tio->tr_time.tv_micro = micro;
	DoIO((struct IORequest *)tio);
}

static struct InputEvent ev;

static void key(UWORD command, UWORD code, UWORD qual)
{
	ev.ie_NextEvent = NULL;
	ev.ie_Class = IECLASS_RAWKEY;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = qual;
	ev.ie_X = ev.ie_Y = 0;
	check(cmd(command, &ev, sizeof(ev)) == 0, "key event");
}

static void writes(void)
{
	struct timeval t0, t1;
	UWORD n;

	n = seen.keys;
	GetSysTime(&t0);
	key(IND_WRITEEVENT, 0x20, IEQUALIFIER_LSHIFT);
	GetSysTime(&t1);
	check(seen.keys == n + 1, "IND_WRITEEVENT reached the handler");
	check(seen.task == FindTask("input.device"), "handler on input task");
	check(seen.second == 1, "handlers by priority");
	check(seen.last.ie_Code == 0x20 &&
	    seen.last.ie_Qualifier == IEQUALIFIER_LSHIFT, "event contents");
	check(CmpTime(&seen.last.ie_TimeStamp, &t0) != 1 &&
	    CmpTime(&seen.last.ie_TimeStamp, &t1) != -1, "time stamp");
	check(seen.last.ie_TimeStamp.tv_secs > 0, "system time stamp");

	key(IND_WRITEEVENT, 0x23, IEQUALIFIER_LALT);
	key(IND_WRITEEVENT, 0x23 | IECODE_UP_PREFIX, IEQUALIFIER_LALT);
	key(IND_WRITEEVENT, 0x12, 0);
	check(seen.last.ie_Prev1DownCode == 0x23 &&
	    seen.last.ie_Prev1DownQual == IEQUALIFIER_LALT &&
	    seen.last.ie_Prev2DownCode == 0x20 &&
	    seen.last.ie_Prev2DownQual == IEQUALIFIER_LSHIFT,
	    "dead-key history");
}

static void repeat(void)
{
	UWORD n;

	check(settime(IND_SETTHRESH, 100000L) == 0, "IND_SETTHRESH");
	check(settime(IND_SETPERIOD, 50000L) == 0, "IND_SETPERIOD");
	seen.repeats = 0;
	seen.repordered = TRUE;
	key(IND_ADDEVENT, 0x21, IEQUALIFIER_LSHIFT);
	check(PeekQualifier() == IEQUALIFIER_LSHIFT, "PeekQualifier");
	delay(400000L);
	key(IND_ADDEVENT, 0x21 | IECODE_UP_PREFIX, IEQUALIFIER_LSHIFT);
	n = seen.repeats;
	xKPrintf("input: %d repeats in 0.4 s\n", n);
	check(n >= 4 && n <= 7, "repeat count");
	check(seen.repordered, "repeat time stamps");
	check(seen.last.ie_Code == (0x21 | IECODE_UP_PREFIX), "release");
	delay(200000L);
	check(seen.repeats == n, "repeat stops on release");

	/* A qualifier key does not repeat. */
	key(IND_ADDEVENT, 0x60, IEQUALIFIER_LSHIFT);
	delay(200000L);
	key(IND_ADDEVENT, 0x60 | IECODE_UP_PREFIX, 0);
	check(seen.repeats == n, "shift does not repeat");
}

static void stopstart(void)
{
	UWORD n;

	n = seen.timers;
	delay(300000L);
	check(seen.timers >= n + 2, "timer events");
	check(cmd(CMD_STOP, NULL, 0) == 0, "CMD_STOP");
	n = seen.timers;
	delay(300000L);
	check(seen.timers == n, "no timer events while stopped");
	check(cmd(CMD_START, NULL, 0) == 0, "CMD_START");
	delay(300000L);
	check(seen.timers >= n + 2, "timer events after CMD_START");
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
	xKPrintf("input: task stack %d of %d bytes used\n",
	    (int)(top - p), (int)(top - (UBYTE *)t->tc_SPLower));
}

int main(void)
{
	struct NSDeviceQueryResult q;
	UWORD n;

	port = CreateMsgPort();
	tio = (struct timerequest *)CreateIORequest(port,
	    sizeof(struct timerequest));
	io = (struct IOStdReq *)CreateIORequest(port,
	    sizeof(struct IOStdReq));
	if (OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)tio, 0) != 0) {
		KPutStr("FAIL input: timer.device\n");
		return 1;
	}
	TimerBase = tio->tr_node.io_Device;
	check(FindName(&SysBase->DeviceList, "input.device") == NULL,
	    "input.device resident before the test");
	if (OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL input: OpenDevice\n");
		return 1;
	}
	InputBase = io->io_Device;
	check(InputBase->dd_Library.lib_Version == 41, "version");
	xKPrintf("input: gameport.device %s\n",
	    FindName(&SysBase->DeviceList, "gameport.device") ? "found" :
	    "absent");

	q.SizeAvailable = 0;
	check(cmd(NSCMD_DEVICEQUERY, &q, sizeof(q)) == 0 &&
	    q.DeviceType == NSDEVTYPE_INPUT && io->io_Actual == sizeof(q),
	    "NSCMD_DEVICEQUERY");
	check(cmd(NSCMD_DEVICEQUERY, &q, 4) == IOERR_BADLENGTH,
	    "NSCMD_DEVICEQUERY short");
	check(cmd(CMD_READ, NULL, 0) == IOERR_NOCMD, "CMD_READ");
	check(AbortIO((struct IORequest *)io) == -1, "AbortIO of a done request");

	h1.is_Node.ln_Pri = 100;
	h1.is_Code = (int (*)())first;
	h2.is_Node.ln_Pri = 0;
	h2.is_Code = (int (*)())second;
	check(cmd(IND_ADDHANDLER, &h2, 0) == 0, "IND_ADDHANDLER");
	check(cmd(IND_ADDHANDLER, &h1, 0) == 0, "IND_ADDHANDLER");

	writes();
	repeat();
	stopstart();

	check(cmd(IND_REMHANDLER, &h1, 0) == 0 &&
	    cmd(IND_REMHANDLER, &h2, 0) == 0, "IND_REMHANDLER");
	n = seen.keys;
	key(IND_WRITEEVENT, 0x20, 0);
	check(seen.keys == n, "removed handler");

	stackuse();
	CloseDevice((struct IORequest *)io);
	CloseDevice((struct IORequest *)tio);
	if (fails != 0)
		return 1;
	KPutStr("PASS input\n");
	return 0;
}
