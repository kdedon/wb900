/*
 * utility.library from LIBS:.  The root program opens it, which loads it
 * from disk, and checks tag lists, hooks, dates, strings, arithmetic and
 * the name space.  Literal ints and 0 passed for LONG and pointer
 * parameters check the widths the fd file declares.
 */
#include <exec/exec.h>
#include <utility/utility.h>
#include <utility/date.h>
#include <utility/hooks.h>
#include <utility/name.h>
#include <proto/utility.h>

struct Library *UtilityBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL utility: %s\n", what);
		fails++;
	}
}

static BOOL same(char *a, char *b)
{
	while (*a == *b && *a != '\0')
		a++, b++;
	return *a == *b;
}

#define T1	(TAG_USER + 1)
#define T2	(TAG_USER + 2)
#define T3	(TAG_USER + 3)
#define T4	(TAG_USER + 4)

static struct TagItem more[] = {
	{ T4, 40 },
	{ TAG_DONE, 0 }
};

/* T1, an ignored item, a skipped T2, T3, then T4 through TAG_MORE. */
static struct TagItem list[] = {
	{ T1, 10 },
	{ TAG_IGNORE, 0 },
	{ TAG_SKIP, 1 },
	{ T2, 20 },
	{ T3, 30 },
	{ TAG_MORE, 0 }
};

static void tags(void)
{
	struct TagItem *state, *ti, *clone;
	static Tag filter[] = { T1, T4, TAG_DONE };
	static struct TagItem bools[] = {
		{ T1, TRUE }, { T2, FALSE }, { T3, TRUE }, { TAG_DONE, 0 }
	};
	static struct TagItem boolmap[] = {
		{ T1, 0x1 }, { T2, 0x100 }, { T3, 0x10000L }, { TAG_DONE, 0 }
	};
	static struct TagItem map[] = { { T3, T2 }, { TAG_DONE, 0 } };

	list[5].ti_Data = (IPTR)more;

	ti = FindTagItem(T3, list);
	check(ti != NULL && ti->ti_Data == 30, "FindTagItem T3");
	ti = FindTagItem(T4, list);
	check(ti == &more[0], "FindTagItem through TAG_MORE");
	check(FindTagItem(T2, list) == NULL, "FindTagItem past TAG_SKIP");
	check(FindTagItem(T1, 0) == NULL, "FindTagItem in a NULL list");

	check(GetTagData(T1, 5, list) == 10, "GetTagData T1");
	check(GetTagData(T2, 0, list) == 0, "GetTagData default 0");
	check(GetTagData(TAG_USER + 9, 0x12345678L, list) == 0x12345678L,
	    "GetTagData long default");

	state = list;
	check((ti = NextTagItem(&state)) != NULL && ti->ti_Tag == T1,
	    "NextTagItem 1");
	check((ti = NextTagItem(&state)) != NULL && ti->ti_Tag == T3,
	    "NextTagItem 2");
	check((ti = NextTagItem(&state)) != NULL && ti->ti_Tag == T4,
	    "NextTagItem 3");
	check(NextTagItem(&state) == NULL && state == NULL, "NextTagItem end");

	clone = CloneTagItems(list);
	check(clone != NULL && clone[0].ti_Tag == T1 && clone[1].ti_Tag == T3 &&
	    clone[2].ti_Tag == T4 && clone[2].ti_Data == 40 &&
	    clone[3].ti_Tag == TAG_DONE, "CloneTagItems");
	check(TagInArray(T4, filter) && !TagInArray(T3, filter), "TagInArray");
	check(FilterTagItems(clone, filter, TAGFILTER_AND) == 2 &&
	    clone[0].ti_Tag == T1 && clone[1].ti_Tag == TAG_IGNORE &&
	    clone[2].ti_Tag == T4, "FilterTagItems AND");
	RefreshTagItemClones(clone, list);
	check(clone[1].ti_Tag == T3, "RefreshTagItemClones");
	check(FilterTagItems(clone, filter, TAGFILTER_NOT) == 1 &&
	    clone[0].ti_Tag == TAG_IGNORE && clone[1].ti_Tag == T3,
	    "FilterTagItems NOT");
	RefreshTagItemClones(clone, list);
	MapTags(clone, map, MAP_KEEP_NOT_FOUND);
	check(clone[0].ti_Tag == T1 && clone[1].ti_Tag == T2 &&
	    clone[1].ti_Data == 30, "MapTags");
	FreeTagItems(clone);

	clone = CloneTagItems(NULL);
	check(clone != NULL && clone[0].ti_Tag == TAG_DONE,
	    "CloneTagItems(NULL)");
	FreeTagItems(clone);

	check(PackBoolTags(0x100, bools, boolmap) == 0x10001L,
	    "PackBoolTags");
}

static IPTR hookfn(struct Hook *hook, APTR object, APTR message)
{
	return (IPTR)hook->h_Data + *(LONG *)message + (object == NULL);
}

static void hooks(void)
{
	struct Hook hook;
	LONG msg;

	hook.h_Entry = (HOOKFUNC)hookfn;
	hook.h_Data = (APTR)0x10000L;
	msg = 41;
	check(CallHookPkt(&hook, 0, &msg) == 0x1002AL, "CallHookPkt");
	/* Through hookfn's prototype the 0 is a 32-bit NULL. */
	check(hookfn(&hook, 0, &msg) == 0x1002AL, "hook called directly");
}

static void dates(void)
{
	struct ClockData cd;

	Amiga2Date(0, &cd);
	check(cd.year == 1978 && cd.month == 1 && cd.mday == 1 &&
	    cd.wday == 0 && cd.hour == 0, "Amiga2Date 0");
	Amiga2Date(1542198896L, &cd);
	check(cd.year == 2026 && cd.month == 11 && cd.mday == 14 &&
	    cd.wday == 6 && cd.hour == 12 && cd.min == 34 && cd.sec == 56,
	    "Amiga2Date 2026");
	check(Date2Amiga(&cd) == 1542198896L, "Date2Amiga 2026");
	Amiga2Date(699407999L, &cd);
	check(cd.year == 2000 && cd.month == 2 && cd.mday == 29 &&
	    cd.wday == 2 && cd.hour == 23 && cd.sec == 59, "Amiga2Date 2000");
	check(CheckDate(&cd) == 699407999L, "CheckDate 2000-02-29");
	Amiga2Date(3855081601L, &cd);
	check(cd.year == 2100 && cd.month == 3 && cd.mday == 1 &&
	    cd.wday == 1 && cd.sec == 1, "Amiga2Date 2100");
	check(Date2Amiga(&cd) == 3855081601L, "Date2Amiga 2100");
	cd.year = 2023;
	cd.month = 2;
	cd.mday = 29;
	check(CheckDate(&cd) == 0, "CheckDate 2023-02-29");
}

static void strings(void)
{
	char buf[16];
	struct {
		char *s;
		LONG l;
		WORD w;
	} args;
	int i;

	check(Stricmp("Hello", "hELLO") == 0, "Stricmp equal");
	check(Stricmp("abc", "ABD") < 0 && Stricmp("b", "A") > 0, "Stricmp order");
	check(Strnicmp("abcX", "ABCy", 3) == 0 && Strnicmp("abcX", "ABCy", 4) < 0,
	    "Strnicmp");
	check(ToUpper('a') == 'A' && ToLower('Q') == 'q' && ToUpper('1') == '1' &&
	    ToUpper(0xE4) == 0xC4 && ToLower(0xC4) == 0xE4, "ToUpper/ToLower");

	check(Strlcpy(buf, "hello", 4) == 5 && same(buf, "hel"), "Strlcpy");
	check(Strlcat(buf, "p me", sizeof(buf)) == 7 && same(buf, "help me"),
	    "Strlcat");
	check(Strlcat(buf, "!!", 8) == 9 && same(buf, "help me"),
	    "Strlcat truncated");

	args.s = "x";
	args.l = 123456L;
	args.w = -7;
	check(VSNPrintf(buf, sizeof(buf), "%s=%ld,%d", &args) == 12 &&
	    same(buf, "x=123456,-7"), "VSNPrintf");
	check(VSNPrintf(buf, 5, "%s=%ld,%d", &args) == 12 && same(buf, "x=12"),
	    "VSNPrintf truncated");

	buf[14] = '\0';
	buf[15] = 1;
	check(SetMem(buf + 1, (UBYTE)0x80, 13) == buf + 1, "SetMem result");
	for (i = 1; i < 14 && (buf[i] & 0xFF) == 0x80; i++)
		;
	check(i == 14 && buf[14] == '\0' && buf[15] == 1, "SetMem fill");
}

static void arith(void)
{
	check(UMult32(300, 300) == 90000L, "UMult32 int arguments");
	check(UMult32(70000L, 70000L) == 605032704L, "UMult32 wraps");
	check(SMult32(-3, 100000L) == -300000L, "SMult32");
	check(UDivMod32(1000000L, 7) == 142857L, "UDivMod32");
	/* SDivMod32's vector is the unimplemented stub: an alert, then 0. */
	check((*(ULONG (*)())((char *)UtilityBase + LVOSDivMod32))(UtilityBase,
	    7L, 2L) == 0, "SDivMod32 stub");
}

static void names(void)
{
	struct NamedObject *a, *b, *f;
	struct TagItem t[2];
	ULONG id;

	t[0].ti_Tag = ANO_UserSpace;
	t[0].ti_Data = 32;
	t[1].ti_Tag = TAG_DONE;
	a = AllocNamedObjectA("Alpha", t);
	b = AllocNamedObjectA("Beta", NULL);
	check(a != NULL && b != NULL && a->no_Object != NULL &&
	    b->no_Object == NULL, "AllocNamedObjectA");
	check(same((char *)NamedObjectName(a), "Alpha"), "NamedObjectName");
	check(AddNamedObject(NULL, a) && AddNamedObject(NULL, b), "AddNamedObject");
	check(!AddNamedObject(NULL, a), "AddNamedObject twice");
	check(FindNamedObject(NULL, "Gamma", NULL) == NULL, "FindNamedObject missing");
	f = FindNamedObject(NULL, "BETA", NULL);
	check(f == b, "FindNamedObject without case");
	check(!AttemptRemNamedObject(f), "AttemptRemNamedObject in use");
	ReleaseNamedObject(f);
	/* Each allocation still holds its one use: removal succeeds. */
	check(AttemptRemNamedObject(a) && AttemptRemNamedObject(b),
	    "AttemptRemNamedObject");
	check(FindNamedObject(NULL, "alpha", NULL) == NULL, "removed");
	ReleaseNamedObject(a);
	ReleaseNamedObject(b);
	FreeNamedObject(a);
	FreeNamedObject(b);

	id = GetUniqueID();
	check(GetUniqueID() == id + 1, "GetUniqueID");
}

int main(void)
{
	check(FindName(&SysBase->LibList, UTILITYNAME) == NULL,
	    "utility.library resident before the test");
	UtilityBase = OpenLibrary(UTILITYNAME, 50L);
	if (UtilityBase == NULL) {
		KPutStr("FAIL utility: OpenLibrary\n");
		return 1;
	}
	check(UtilityBase->lib_Version == 50, "version");
	tags();
	hooks();
	dates();
	strings();
	arith();
	names();
	CloseLibrary(UtilityBase);
	if (fails != 0)
		return 1;
	KPutStr("PASS utility\n");
	return 0;
}
