/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2013, The MorphOS Development Team. All Rights Reserved.
*/

/****************************************************************************************/

#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <proto/dos.h>

#include <exec/lists.h>
#include <exec/resident.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/alerts.h>
#include <devices/input.h>
#include <devices/timer.h>
#include <intuition/classes.h>
#include <intuition/pointerclass.h>
#include <utility/hooks.h>
#include <utility/utility.h>
#include <graphics/layers.h>

#include "intuition_intern.h"
#include "strgadgets.h" /* To get GlobalEditFunc prototype */
#include "inputhandler.h"

#define AN_IntuitionLib  0x04000000L

static char intuiname[] = INTUITIONNAME;
static char intuiid[] = "intuition 50.26 (14.11.2026)";

extern APTR IntuitionFuncs[];

AROS_UFP3(IPTR, rootDispatcher,
          AROS_UFPA(Class *,  cl,  A0),
          AROS_UFPA(Object *, obj, A2),
          AROS_UFPA(Msg,      msg, A1)
         );
BOOL InitClasses(struct IntuitionBase *IntuitionBase);

#define LIBBASE ((struct IntIntuitionBase *)IntuitionBase)

/****************************************************************************************/

/*
 * Default colors for a new screen: white paper and black ink, grey fills
 * and blue highlights, all colours of the fixed RGBI palette, which
 * pens take the nearest of.  The pointer's black rim and light body
 * show on RGBI and mono alike.
 */

const ULONG coltab[] =
{
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, /* White      */
    0x00000000, 0x00000000, 0x00000000, /* Black      */
    0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, /* Grey       */
    0x00000000, 0x00000000, 0xAAAAAAAA, /* Blue       */

    0xEEEEEEEE, 0x44444444, 0x44444444, /* Red        */
    0x55555555, 0xDDDDDDDD, 0x55555555, /* Green      */
    0x00000000, 0x44444444, 0xDDDDDDDD, /* Dark Blue  */
    0xEEEEEEEE, 0x99999999, 0x00000000, /* Yellow     */

    0xEEEEEEEE, 0x44444444, 0x44444444, /* Mouse pointer: red, */
    0x00000000, 0x00000000, 0x00000000, /* black rim, */
    0xEEEEEEEE, 0xEEEEEEEE, 0xCCCCCCCC  /* light body */
};

/* Built-in busy pointer in the classic two-plane sprite format. Keep this
 * distinct from Preferences.PointerMatrix: the latter is the normal pointer
 * and using it for both shapes makes WA_BusyPointer visually ambiguous.
 * IPrefs may still replace this through IPREFS_TYPE_POINTER_V39. */
static CONST UWORD IntuitionBusyPointer[] =
{
    0x0000, 0x0000,             /* sprite control words */
    0x0000, 0x07c0,
    0x07c0, 0x1ff0,
    0x1ff0, 0x3ff8,
    0x3078, 0x7ffc,
    0x7efc, 0xfffe,
    0x7dfc, 0xfffe,
    0x707e, 0xffff,
    0x7fc2, 0xffff,
    0x3ff4, 0x7ffe,
    0x1fc0, 0x3ffc,
    0x07c0, 0x1ff0,
    0x0000, 0x07c0,
    0x00c0, 0x01e0,
    0x0000, 0x01cc,
    0x0004, 0x000e,
    0x0000, 0x000e,
    0x0000, 0x0000              /* sprite terminator words */
};

/* The offsets of the semaphores embedded in the library base, see IntuitionInit() */

static const UWORD ibase_semaphores[] =
{
    offsetof(struct IntIntuitionBase, WinDecorSem),
    offsetof(struct IntIntuitionBase, ScrDecorSem),
    offsetof(struct IntIntuitionBase, MenuDecorSem),
    offsetof(struct IntIntuitionBase, PubScrListLock),
    offsetof(struct IntIntuitionBase, GadgetLock),
    offsetof(struct IntIntuitionBase, MenuLock),
    offsetof(struct IntIntuitionBase, WindowLock),
    offsetof(struct IntIntuitionBase, IntuiActionLock),
    offsetof(struct IntIntuitionBase, InputHandlerLock),
};

static int IntuitionInit(struct IntuitionBase *IntuitionBase)
{
    /* Open our dependencies */
    if (!(LIBBASE->UtilityBase = OpenLibrary(UTILITYNAME, 39L)) ||
        !(LIBBASE->GfxBase = (APTR)OpenLibrary(GRAPHICSNAME, 39L)) ||
        !(LIBBASE->LayersBase = (APTR)OpenLibrary("layers.library", 39L)) ||
        !(LIBBASE->KeymapBase = OpenLibrary("keymap.library", 39L)))
        return FALSE;

    /* Create semaphore and initialize it */
    GetPrivIBase(LIBBASE)->IBaseLock = AllocMem (sizeof(struct SignalSemaphore), MEMF_PUBLIC|MEMF_CLEAR);

    if (!GetPrivIBase(LIBBASE)->IBaseLock)
        return FALSE;

    InitSemaphore(GetPrivIBase(LIBBASE)->IBaseLock);

    {
        WORD i;

        for (i = 0; i < sizeof(ibase_semaphores) / sizeof(ibase_semaphores[0]); i++)
            InitSemaphore((struct SignalSemaphore *)((UBYTE *)LIBBASE + ibase_semaphores[i]));
    }

    /* Initialize global stringgadget edit hook */
    GetPrivIBase(LIBBASE)->DefaultEditHook.h_Entry  = (ULONG (*)())GlobalEditFunc;
    GetPrivIBase(LIBBASE)->DefaultEditHook.h_SubEntry   = NULL;
    GetPrivIBase(LIBBASE)->DefaultEditHook.h_Data   = LIBBASE;

    GetPrivIBase(LIBBASE)->GlobalEditHook = &(GetPrivIBase(LIBBASE)->DefaultEditHook);

    GetPrivIBase(LIBBASE)->DefaultPubScreen = NULL;
    NewList((struct List *)&GetPrivIBase(LIBBASE)->PubScreenList);

    NewList((struct List *)&GetPrivIBase(LIBBASE)->Decorations);

    /*
     * Setup the default pens to the default
     * colors so that screens have proper color
     * even before IPrefs is loaded.
     */
    {
        struct Color32 *p;
        ULONG               i;

        p = GetPrivIBase(LIBBASE)->Colors;

        for (i = 0; i < COLORTABLEENTRIES; i++)
        {
            p[i].red = coltab[i*3];
            p[i].green = coltab[i*3+1];
            p[i].blue = coltab[i*3+2];
        }
    }

    LoadDefaultPreferences(GetPubIBase(LIBBASE));

    if (!(GetPrivIBase(LIBBASE)->IDCMPPool = CreatePool(MEMF_SEM_PROTECTED,(sizeof (struct IntIntuiMessage)) * 16,sizeof (struct IntIntuiMessage)))) return FALSE;

    GetPrivIBase(LIBBASE)->FrameSize = FRAMESIZE_THIN;

    {
        WORD i;

        for(i = 0; i < RESOURCELIST_HASHSIZE; i++)
        {
            NewList((struct List *)&GetPrivIBase(LIBBASE)->ResourceList[i]);
        }
    }

    GetPrivIBase(LIBBASE)->ScrDecorClass = FindClass(SCRDECORCLASS);
    GetPrivIBase(LIBBASE)->ScrDecorTags = NULL;
    GetPrivIBase(LIBBASE)->MenuDecorClass = FindClass(MENUDECORCLASS);
    GetPrivIBase(LIBBASE)->MenuDecorTags = NULL;
    GetPrivIBase(LIBBASE)->WinDecorClass = FindClass(WINDECORCLASS);
    GetPrivIBase(LIBBASE)->WinDecorTags = NULL;
    LIBBASE->ViewLord_ok = FALSE;

    GetPrivIBase(LIBBASE)->DefaultPointer = MakePointerFromPrefs
    (
        GetPubIBase(LIBBASE), &GetPrivIBase(LIBBASE)->ActivePreferences
    );
    GetPrivIBase(LIBBASE)->BusyPointer = MakePointerFromData
    (
        GetPubIBase(LIBBASE), IntuitionBusyPointer, 0, 0, 16, 16
    );

    if
    (
           !GetPrivIBase(LIBBASE)->DefaultPointer
        || !GetPrivIBase(LIBBASE)->BusyPointer
    )
    {
        return FALSE;
    }

    return TRUE;
}

static int InitRootClass(struct IntuitionBase *IntuitionBase)
{
    InitSemaphore(&GetPrivIBase(LIBBASE)->ClassListLock);
    NewList((struct List *)&GetPrivIBase(LIBBASE)->ClassList);

    /* Setup root class */

    GetPrivIBase(LIBBASE)->RootClass.cl_Dispatcher.h_Entry = (ULONG (*)())rootDispatcher;
    GetPrivIBase(LIBBASE)->RootClass.cl_ID                 = (ClassID)ROOTCLASS;
    GetPrivIBase(LIBBASE)->RootClass.cl_UserData           = (IPTR)LIBBASE;

    AddClass(&(GetPrivIBase(LIBBASE)->RootClass));

    return InitClasses(GetPubIBase(LIBBASE));
}

/****************************************************************************************/

/* The Preferences saved in DEVS:system-configuration, as on AmigaOS 1.x */
static void LoadSystemConfiguration(struct IntuitionBase *IntuitionBase)
{
    struct DosLibrary  *DOSBase = GetPrivIBase(LIBBASE)->DOSBase;
    struct Preferences *prefs;
    BPTR                fh;
    LONG                len;

    if (!DOSBase)
        GetPrivIBase(LIBBASE)->DOSBase = DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
    if (!DOSBase)
        return;

    fh = Open("DEVS:system-configuration", MODE_OLDFILE);
    if (!fh)
        return;

    prefs = AllocMem(sizeof(struct Preferences), MEMF_ANY);
    if (prefs)
    {
        len = Read(fh, prefs, sizeof(struct Preferences));
        if (len > 0)
            SetPrefs(prefs, len, FALSE);
        FreeMem(prefs, sizeof(struct Preferences));
    }
    Close(fh);
}

/****************************************************************************************/

static int IntuitionOpen(struct IntuitionBase *IntuitionBase)
{
    struct GfxBase *GfxBase = GetPrivIBase(LIBBASE)->GfxBase;

    /* Open the input device */

    if (!GetPrivIBase(LIBBASE)->InputMP)
    {
        if (!(GetPrivIBase(LIBBASE)->InputMP = CreateMsgPort()))
        {

            return FALSE;
        }
    }

    if (!GetPrivIBase(LIBBASE)->InputIO)
    {
        if (!(GetPrivIBase(LIBBASE)->InputIO = (struct IOStdReq *)
              CreateIORequest(GetPrivIBase(LIBBASE)->InputMP, sizeof (struct IOStdReq))) )
        {

            return FALSE;
        }
    }

    if (!GetPrivIBase(LIBBASE)->InputDeviceOpen)
    {
        if (!OpenDevice("input.device", 0, (struct IORequest *)GetPrivIBase(LIBBASE)->InputIO, 0))
        {
            GetPrivIBase(LIBBASE)->InputDeviceOpen = TRUE;
            GetPrivIBase(LIBBASE)->InputBase = (struct Library *)GetPrivIBase(LIBBASE)->InputIO->io_Device;
        }
        else
        {

            return FALSE;
        }
    }

    if (!GetPrivIBase(LIBBASE)->InputHandler)
    {

        if ( !(GetPrivIBase(LIBBASE)->InputHandler = InitIIH(GetPubIBase(LIBBASE))) )
        {

            return FALSE;
        }

        GetPrivIBase(LIBBASE)->InputIO->io_Data = (APTR)GetPrivIBase(LIBBASE)->InputHandler;
        GetPrivIBase(LIBBASE)->InputIO->io_Command = IND_ADDHANDLER;

        DoIO((struct IORequest *)GetPrivIBase(LIBBASE)->InputIO);
        /* Known before the handler's first event, which an earlier handler may hold up */
        ((struct IIHData *)GetPrivIBase(LIBBASE)->InputHandler->is_Data)->InputDeviceTask =
            FindTask("input.device");

    }

    if (!LIBBASE->ViewLord_ok)
    {

        InitView(&LIBBASE->Base.ViewLord);
        InitSemaphore(&LIBBASE->ViewLordLock);
        LIBBASE->ViewLord_ok = TRUE;
    }

    if (!GetPrivIBase(LIBBASE)->ScreenFont)
        GetPrivIBase(LIBBASE)->ScreenFont = GetPrivIBase(LIBBASE)->GfxBase->DefaultFont;

    if (!GetPrivIBase(LIBBASE)->TimerBase)
    {
        if (!(GetPrivIBase(LIBBASE)->TimerMP = CreateMsgPort()))
        {

            return FALSE; /* don't close anything */
        }

        if (!(GetPrivIBase(LIBBASE)->TimerIO = (struct timerequest *)CreateIORequest(GetPrivIBase(LIBBASE)->TimerMP, sizeof(struct timerequest))))
        {

            return FALSE; /* don't close anything */
        }

        if (OpenDevice(TIMERNAME,UNIT_VBLANK, (struct IORequest *)GetPrivIBase(LIBBASE)->TimerIO,0))
        {

            return FALSE; /* don't close anything */
        }

        GetPrivIBase(LIBBASE)->TimerBase = (struct Library *)GetPrivIBase(LIBBASE)->TimerIO->tr_node.io_Device;

        SetPrefs(&GetPrivIBase(LIBBASE)->DefaultPreferences, sizeof(struct Preferences), FALSE);
        LoadSystemConfiguration(IntuitionBase);
    }

    return TRUE;
}


static struct Library *intuiinit(struct IntuitionBase *lib, APTR seg, APTR sysbase)
{
    struct IntIntuitionBase *ib = (struct IntIntuitionBase *)lib;

    lib->LibNode.lib_Node.ln_Name = intuiname;
    lib->LibNode.lib_IdString = (APTR)intuiid;
    lib->LibNode.lib_Version = 50;
    lib->LibNode.lib_Revision = 26;
    ib->SegList = seg;

    if (!InitRootClass(lib) || !IntuitionInit(lib))
    {
        Alert(AN_IntuitionLib | AG_MakeLib);
        return NULL;
    }
    return &lib->LibNode;
}

/* The first open adds the input handler: input.device may need DEVS:. */
struct Library *itopen(struct IntuitionBase *lib, ULONG ver)
{
    if (!IntuitionOpen(lib))
        return NULL;
    lib->LibNode.lib_OpenCnt++;
    return &lib->LibNode;
}

/* The screens, windows and classes of other tasks stay: never expunged */
APTR itexpunge(struct Library *lib)
{
    return NULL;
}

APTR itclose(struct Library *lib)
{
    lib->lib_OpenCnt--;
    return NULL;
}

APTR itnull(void)
{
    return NULL;
}

/* The vectors of the functions not ported */
ULONG itunimpl(void)
{
    Alert(AN_IntuitionLib);
    return 0;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} intuiinittab = {
    sizeof(struct IntIntuitionBase), (APTR)IntuitionFuncs, NULL, (APTR)intuiinit
};

struct Resident intuitag = {
    RTC_MATCHWORD, &intuitag, (APTR)(&intuitag + 1), RTF_AUTOINIT, 50,
    NT_LIBRARY, 15, intuiname, intuiid, (APTR)&intuiinittab
};
