/*
 * keymap.library from LIBS:.  The root program opens it and checks the
 * default usa keymap through MapRawKey and MapANSI: plain, shifted,
 * Caps Lock and Control keys, string keys, dead keys, overflow, and an
 * ASCII round trip.  Codes are Amiga raw key codes.
 */
#include <exec/exec.h>
#include <devices/inputevent.h>
#include <devices/keymap.h>
#include <proto/keymap.h>

struct Library *KeymapBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL keymap: %s\n", what);
		fails++;
	}
}

static BOOL same(UBYTE *a, char *b, int n)
{
	while (n-- > 0)
		if (*a++ != (UBYTE)*b++)
			return FALSE;
	return TRUE;
}

static struct InputEvent ie;
static UBYTE buf[16];

/* Map code with qualifier qual into buf, as MapRawKey's result. */
static WORD map(UWORD code, UWORD qual)
{
	ie.ie_Class = IECLASS_RAWKEY;
	ie.ie_Code = code;
	ie.ie_Qualifier = qual;
	return MapRawKey(&ie, buf, sizeof(buf), NULL);
}

static void rawkeys(void)
{
	check(map(0x20, 0) == 1 && buf[0] == 'a', "a");
	check(map(0x20, IEQUALIFIER_LSHIFT) == 1 && buf[0] == 'A', "shift a");
	check(map(0x20, IEQUALIFIER_CAPSLOCK) == 1 && buf[0] == 'A',
	    "caps a");
	check(map(0x01, IEQUALIFIER_CAPSLOCK) == 1 && buf[0] == '1',
	    "caps 1");
	check(map(0x01, IEQUALIFIER_RSHIFT) == 1 && buf[0] == '!', "shift 1");
	check(map(0x20, IEQUALIFIER_CONTROL) == 1 && buf[0] == 0x01, "ctrl a");
	check(map(0x20 | IECODE_UP_PREFIX, 0) == 0, "a released");
	check(map(0x44, 0) == 1 && buf[0] == '\r', "return");
	check(map(0x41, 0) == 1 && buf[0] == '\b', "backspace");
	check(map(0x40, 0) == 1 && buf[0] == ' ', "space");
	check(map(0x50, 0) == 3 && same(buf, "\2330~", 3), "F1");
	check(map(0x50, IEQUALIFIER_LSHIFT) == 4 && same(buf, "\23310~", 4),
	    "shift F1");
	check(map(0x4C, 0) == 2 && same(buf, "\233A", 2), "cursor up");
	check(map(0x42, IEQUALIFIER_LSHIFT) == 2 && same(buf, "\233Z", 2),
	    "shift tab");
	check(map(0x50, IEQUALIFIER_REPEAT) == 0, "F1 does not repeat");
	check(map(0x20, IEQUALIFIER_REPEAT) == 1, "a repeats");
	check(map(0x60, IEQUALIFIER_LSHIFT) == 0, "shift key alone");

	ie.ie_Class = IECLASS_RAWKEY;
	ie.ie_Code = 0x50;
	ie.ie_Qualifier = 0;
	check(MapRawKey(&ie, buf, 2, NULL) == -1, "overflow");
	ie.ie_Class = IECLASS_RAWMOUSE;
	check(MapRawKey(&ie, buf, sizeof(buf), NULL) == 0, "not a key");

	/* Alt-f is dead key 1 (acute): alone it gives nothing, and the
	 * next e is e acute. */
	check(map(0x23, IEQUALIFIER_LALT) == 0, "alt f");
	ie.ie_Prev1DownCode = 0x23;
	ie.ie_Prev1DownQual = IEQUALIFIER_LALT;
	check(map(0x12, 0) == 1 && buf[0] == 0xE9, "alt f, e");
	check(map(0x12, IEQUALIFIER_LSHIFT) == 1 && buf[0] == 0xC9,
	    "alt f, E");
	ie.ie_Prev1DownCode = 0x21;
	ie.ie_Prev1DownQual = 0;
	check(map(0x12, 0) == 1 && buf[0] == 'e', "s, e");
}

static void ansi(void)
{
	UBYTE out[8];
	UBYTE c;
	LONG n;
	int bad;

	n = MapANSI((STRPTR)"aA", 2, out, 4, NULL);
	check(n == 2 && out[0] == 0x20 && out[1] == 0 && out[2] == 0x20 &&
	    out[3] == IEQUALIFIER_LSHIFT, "MapANSI aA");
	n = MapANSI((STRPTR)"\t", 1, out, 4, NULL);
	check(n == 1 && out[0] == 0x42 && out[1] == 0, "MapANSI tab");
	n = MapANSI((STRPTR)"\351", 1, out, 4, NULL);
	check(n == 2 && out[0] == 0x23 && out[1] == IEQUALIFIER_LALT &&
	    out[2] == 0x12 && out[3] == 0, "MapANSI e acute");
	check(MapANSI((STRPTR)"\351", 1, out, 1, NULL) == -1,
	    "MapANSI overflow");

	/* Every printable ASCII character maps to one key and back. */
	bad = 0;
	for (c = ' '; c < 0x7F; c++) {
		if (MapANSI((STRPTR)&c, 1, out, 4, NULL) != 1 ||
		    map(out[0], out[1]) != 1 || buf[0] != c) {
			xKPrintf("keymap: %x does not round-trip\n", (int)c);
			bad++;
		}
	}
	check(bad == 0, "ASCII round trip");
}

static void defaults(void)
{
	struct KeyMapResource *kr;
	struct KeyMapNode *kn;
	struct KeyMap *km, mine;

	km = AskKeyMapDefault();
	check(km != NULL, "AskKeyMapDefault");
	kr = (struct KeyMapResource *)OpenResource("keymap.resource");
	check(kr != NULL, "keymap.resource");
	if (kr != NULL) {
		kn = (struct KeyMapNode *)FindName(&kr->kr_List, "usa");
		check(kn != NULL && kn->kn_KeyMap.km_LoKeyMap == km->km_LoKeyMap,
		    "usa keymap node");
	}

	mine = *km;
	SetKeyMapDefault(&mine);
	check(AskKeyMapDefault() == &mine, "SetKeyMapDefault");
	SetKeyMapDefault(km);
}

int main(void)
{
	KeymapBase = OpenLibrary("keymap.library", 41L);
	if (KeymapBase == NULL) {
		KPutStr("FAIL keymap: OpenLibrary\n");
		return 1;
	}
	defaults();
	rawkeys();
	ansi();
	CloseLibrary(KeymapBase);
	if (fails != 0)
		return 1;
	KPutStr("PASS keymap\n");
	return 0;
}
