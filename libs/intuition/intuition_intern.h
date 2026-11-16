#ifndef INTUITION_INTERN_H
#define INTUITION_INTERN_H

/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2013, The MorphOS Development Team. All Rights Reserved.
*/

#include <aros/libcall.h>
#include <aros/asmcall.h>
#include <aros/atomic.h>
#include <exec/execbase.h>
#include <exec/semaphores.h>
#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/interrupts.h>
#include <exec/io.h>
#include <exec/types.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/regions.h>
#include <graphics/clip.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/classes.h>
#include <intuition/cghooks.h>
#include <intuition/sghooks.h>
#include <intuition/screens.h>
#include <intuition/iprefs.h>
#include <prefs/icontrol.h>
#include <proto/exec.h>
#include <proto/alib.h>
#include <clib/macros.h>

#include "inputhandler_actions.h"
#include "requesters.h"

/* The C library functions intuition uses (crt.c) */
#define offsetof(t, m)  ((ULONG)&((t *)0)->m)
ULONG strlen(CONST_STRPTR s);
char *strcpy(char *d, CONST_STRPTR s);
int strcmp(CONST_STRPTR a, CONST_STRPTR b);
int strcasecmp(CONST_STRPTR a, CONST_STRPTR b);
int strncmp(CONST_STRPTR a, CONST_STRPTR b, ULONG n);
APTR memcpy(APTR d, CONST_APTR s, ULONG n);
APTR memmove(APTR d, CONST_APTR s, ULONG n);
APTR memset(APTR d, int c, ULONG n);
int memcmp(CONST_APTR a, CONST_APTR b, ULONG n);
int abs(int i);
LONG atol(CONST_STRPTR s);
#define memclr(a,b) memset(a,0,b)
#define bzero(a,b) memset(a,0,b)

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) > (b) ? (b) : (a))

#define SANITY_CHECK(x)     	if (!((IPTR)x)) return;
#define SANITY_CHECKR(x,v)  	if (!((IPTR)x)) return v;

/* Options */

/* The 1.x look has the classic menus */
#define MENUS_AMIGALOOK(base)  ((GetPrivIBase(base)->IControlPrefs.ic_Flags & ICF_3DMENUS) == 0 || \
                                (GetPrivIBase(base)->ActivePreferences.EnableCLI & OLD_LOOK) != 0)

/* --- Values --- */
#define MENULOOK_3D            0
#define MENULOOK_CLASSIC       1

#define MENUS_UNDERMOUSE(base) (GetPrivIBase(base)->IControlPrefs.ic_Flags & ICF_POPUPMENUS)
#define MENUS_TITLEPULL(base)  ((GetPrivIBase(base)->IControlPrefs.ic_Flags & ICF_PULLDOWNTITLEMENUS) != 0)

#define AVOID_WINBORDERERASE(base) \
                               (GetPrivIBase(base)->IControlPrefs.ic_Flags & ICF_AVOIDWINBORDERERASE)

#define FRAME_SIZE(base)       (GetPrivIBase(base)->FrameSize)
/* --- Values --- */
#define FRAMESIZE_THIN         0 /* 1:1 thin */
#define FRAMESIZE_MEDRES       1 /* 2:1 medres like AmigaOS */
#define FRAMESIZE_THICK        2 /* 1:1 thick */

#define ILOCKCHECK(a) ((a->task) && (a->task == IBase->IBaseLock->ss_Owner))

#define LOCKWINDOW ObtainSemaphore(&IBase->WindowLock);
#define UNLOCKWINDOW ReleaseSemaphore(&IBase->WindowLock);
#define ATTEMPTWINDOWLOCK AttemptSemaphore(&IBase->WindowLock)

#define LOCKGADGET(base)       ObtainSemaphore(&GetPrivIBase(base)->GadgetLock);
#define UNLOCKGADGET(base)     ReleaseSemaphore(&GetPrivIBase(base)->GadgetLock);
#define LOCKWINDOWLAYERS(w) ;
#define UNLOCKWINDOWLAYERS(w) ;

#define WLAYER(w)   	    	    	    (((struct IntWindow *)(w))->wlayer)
#define BLAYER(w)   	    	    	    (((struct IntWindow *)(w))->borderlayer)

#define DEFPUBSCREEN 	    	    	    TRUE

/* The default screen: the HR card's 1024x800 mono mode */
#define AROS_NOMINAL_WIDTH   1024
#define AROS_NOMINAL_HEIGHT  800
#define AROS_NOMINAL_DEPTH   1

#define TITLEBUFFERLEN      	    	    255


/* ObtainGIRPort must install a 0 clipregion and
   set scrollx/scrolly of layer to 0. Since this
   will be restored only when ReleaseGIRPort is
   called, we must backup the orig values somewhere */

struct LayerContext
{
    struct Region       *clipregion;
    WORD            	 scroll_x;
    WORD            	 scroll_y;
    WORD            	 nestcount;
};

struct Color32
{
    ULONG red;
    ULONG green;
    ULONG blue;
};

#define COLORTABLEENTRIES   	11

struct IntScreen;

#define RESOURCELIST_HASHSIZE 	32

#define RESOURCE_WINDOW     	1
#define RESOURCE_SCREEN	    	2

struct HashNode
{
    struct MinNode node;
    UWORD          type;
    APTR           resource;
};

/* IntuitionBase */
struct IntIntuitionBase
{
    struct IntuitionBase    	 Base;


    struct DosLibrary		*DOSBase;
    struct LayersBase       	*LayersBase;
    struct Library          	*UtilityBase;
    struct GfxBase          	*GfxBase;
    struct Library          	*KeymapBase;

    struct Library          	*InputBase;
    struct Library          	*TimerBase;
    struct MsgPort          	*TimerMP;
    struct timerequest      	*TimerIO;

    struct MsgPort          	*WorkBenchMP;
    struct Screen           	*WorkBench;
    struct SignalSemaphore  	*IBaseLock;
    struct SignalSemaphore       ViewLordLock;

    /* Intuition input handlers replyport. This one is set
    int rom/inputhandler.c/InitIIH()
    */
    struct MsgPort          	*IntuiReplyPort;
    struct MinList          	*IntuiActionQueue;
    struct IOStdReq         	*InputIO;
    struct MsgPort          	*InputMP;


    BOOL                    	 InputDeviceOpen;
    struct Interrupt        	*InputHandler;

    struct Hook             	*GlobalEditHook;
    /* The default global edit hook */
    struct Hook             	 DefaultEditHook;

    struct Screen           	*DefaultPubScreen;
    struct SignalSemaphore  	 PubScrListLock;
    struct MinList          	 PubScreenList;
    UWORD                   	 pubScrGlobalMode;


    struct SignalSemaphore  	 GadgetLock;
    struct SignalSemaphore  	 MenuLock;
    struct SignalSemaphore   	 WindowLock;
    struct SignalSemaphore  	 IntuiActionLock;
    struct SignalSemaphore  	 InputHandlerLock;
    struct LayerContext     	 BackupLayerContext;

    struct IClass           	*dragbarclass;
    struct IClass           	*sizebuttonclass;
    struct IClass               *windowsysiclass;
    struct IClass               *screenclass;
    struct Preferences      	 DefaultPreferences;
    struct Preferences      	 ActivePreferences;

    struct MsgPort          	*MenuHandlerPort;
    BOOL                    	 MenusActive;

    struct TextFont         	*TopazFont;
    struct TextFont         	*ScreenFont;
    struct TextAttr             *ReqFont;


    struct SignalSemaphore  	 ClassListLock;
    struct MinList          	 ClassList;
    struct IClass           	 RootClass;

    BOOL			ViewLord_ok;

    ULONG                   	*IDCMPPool;

    struct IScreenModePrefs 	 *ScreenModePrefs;
    BOOL                         IPrefsScreenMode;  /* ScreenModePrefs from SetIPrefs() */
    struct IIControlPrefs   	 IControlPrefs;
    struct IClass           	*pointerclass;
    Object                  	*DefaultPointer;
    Object                  	*BusyPointer;
    UWORD                   	 DriPens2[NUMDRIPENS];
    UWORD                   	 DriPens4[NUMDRIPENS];
    UWORD                   	 DriPens8[NUMDRIPENS];
    struct Color32          	 Colors[COLORTABLEENTRIES];
    ULONG                    	 DMStartSecs;
    ULONG                   	 DMStartMicro;
    struct IntScreen        	*MenuVerifyScreen;
    ULONG                   	 PointerDelay;

    ULONG                   	 LastClickSecs;
    ULONG                   	 LastClickMicro; /* for doubleclick to front */
    ULONG                        LastMenuDownSecs;
    ULONG                        LastMenuDownMicro; /* for auto sticky/non-sticky menus */
    ULONG                   	 DoubleClickCounter;
    ULONG                    	 DoubleClickButton;

    struct RastPort         	 DoGadgetMethodRP;
    struct GadgetInfo       	 DoGadgetMethodGI;

    struct SignalSemaphore   	 ScrDecorSem;
    struct IClass   	    	 *ScrDecorClass;
    struct TagItem   	    	 *ScrDecorTags;
    struct SignalSemaphore   	 MenuDecorSem;
    struct IClass                *MenuDecorClass;
    struct TagItem   	    	 *MenuDecorTags;
    struct SignalSemaphore   	 WinDecorSem;
    struct IClass   	    	 *WinDecorClass;
    struct TagItem   	    	 *WinDecorTags;

    struct List                 Decorations;
    struct NewDecorator         *Decorator;

    LONG                    	 BeepingScreens;

    WORD            	    	 prop_clickoffset_x, prop_clickoffset_y;

    struct MinList            	 ResourceList[RESOURCELIST_HASHSIZE];

    /* Menu Look Settings */
    int                          FrameSize;

    struct SignalSemaphore      *GfxFontSem;
    APTR                         SegList;    /* graphics.library/TextFonts, found on first use (misc.c, all builds) */
};

struct SharedPointer
{
    struct ExtSprite   *sprite;
    WORD    	    	xoffset, yoffset;
    int     	    	ref_count;
};

struct SharedPointer *CreateSharedPointer(struct ExtSprite *, int, int, struct IntuitionBase *);
void ObtainSharedPointer(struct SharedPointer *, struct IntuitionBase *);
void ReleaseSharedPointer(struct SharedPointer *, struct IntuitionBase *);

/*
 * Private version of DrawInfo.
 * We keep compatibility with original structure plus use some
 * private extensions.
 * The whole structure is rewritten in order to simplify MorphOS code merging.
 */
struct IntuitionCustomize;

struct IntDrawInfo
{
    UWORD                      dri_Version;
    UWORD                      dri_NumPens;
    UWORD                     *dri_Pens;
    struct TextFont           *dri_Font;
    UWORD                      dri_Depth;
    struct
    {
        UWORD X;
        UWORD Y;
    }                          dri_Resolution;
    ULONG                      dri_Flags;
    struct Image              *dri_CheckMark;
    struct Image              *dri_AmigaKey;
    /* The following two fields are compatible with AmigaOS v4 */
    struct Screen   	      *dri_Screen;
    struct IntuitionCustomize *dri_Customize;
    /* Private extensions begin */
    ULONG                     *dri_Colors;      /* MorphOS-compatible, unused (yet) */
    IPTR                       dri_Reserved[2]; /* Remaining reserved fields        */
};

/* Private dri_Flags */
#define DRIF_DIRECTCOLOR  (1L << 16)

#define LOCK_WINDECOR(IntuitionBase)   	 ObtainSemaphore(&((struct IntIntuitionBase *)(IntuitionBase))->WinDecorSem);
#define LOCKSHARED_WINDECOR(IntuitionBase) ObtainSemaphoreShared(&((struct IntIntuitionBase *)(IntuitionBase))->WinDecorSem);
#define UNLOCK_WINDECOR(IntuitionBase) 	 ReleaseSemaphore(&((struct IntIntuitionBase *)(IntuitionBase))->WinDecorSem);

#define LOCK_SCRDECOR(IntuitionBase)   	 ObtainSemaphore(&((struct IntIntuitionBase *)(IntuitionBase))->ScrDecorSem);
#define LOCKSHARED_SCRDECOR(IntuitionBase) ObtainSemaphoreShared(&((struct IntIntuitionBase *)(IntuitionBase))->ScrDecorSem);
#define UNLOCK_SCRDECOR(IntuitionBase) 	 ReleaseSemaphore(&((struct IntIntuitionBase *)(IntuitionBase))->ScrDecorSem);

#define LOCK_MENUDECOR(IntuitionBase)   	 ObtainSemaphore(&((struct IntIntuitionBase *)(IntuitionBase))->MenuDecorSem);
#define LOCKSHARED_MENUDECOR(IntuitionBase) ObtainSemaphoreShared(&((struct IntIntuitionBase *)(IntuitionBase))->MenuDecorSem);
#define UNLOCK_MENUDECOR(IntuitionBase) 	 ReleaseSemaphore(&((struct IntIntuitionBase *)(IntuitionBase))->MenuDecorSem);

#define NUM_SYSIMAGES 8

struct IntScreen
{
    struct Screen            Screen;

    /* Private fields */
    struct HashNode          hashnode;
    struct IntDrawInfo       DInfo;
    ULONG                    realdepth;
    struct TTextAttr         textattr;
    ULONG                    textattrtags[3];
    UWORD                    Pens[NUMDRIPENS];
    struct Hook              BackFillHook;   /* the Workbench screen's background */
    struct PubScreenNode    *pubScrNode;
    Object                  *depthgadget;
    ULONG                    SpecialFlags;   /* SF_* */
    struct Layer            *rootLayer;
    ULONG                    ModeID;
    struct SharedPointer    *Pointer;
    struct Window           *MenuVerifyActiveWindow;
    int                      MenuVerifyTimeOut;
    int                      MenuVerifyMsgCount;
    ULONG                    MenuVerifySeconds;
    ULONG                    MenuVerifyMicros;
    ULONG                    DecorUserBufferSize;
    IPTR                     DecorUserBuffer;
    Object                  *ScrDecorObj;
    Object                  *MenuDecorObj;
    Object                  *WinDecorObj;
    struct NewDecorator     *Decorator;
    /* system gadget images shared by the screen's windows */
    Object                  *SysImages[NUM_SYSIMAGES];
    UBYTE                    SysImageWhich[NUM_SYSIMAGES];

    struct BitMap           *AllocatedBitMap;
    /* For FreeScreenBuffer(): the screen's own bitmap */
    struct DBufInfo         *RestoreDBufInfo;
    struct BitMap           *RestoreBitMap;
    UBYTE                    BeepingCounter;
    BOOL                     ShowPointer;
    BOOL                     SysFont;
    BOOL                     frontmost;

    BOOL                     WindowLock;
};


#define GetPrivScreen(s)    ((struct IntScreen *)s)
#define IS(x)               ((struct IntScreen *)x)

/* SpecialFlags */
#define SF_IsParent 	    (0x0001)
#define SF_IsChild  	    (0x0002)
#define SF_InvisibleBar     (0x0004)
#define SF_AppearingBar     (0x0008)
#define SF_Draggable	    (0x0020) /* Screen can be dragged		     */

struct IntIntuiMessage
{
    struct ExtIntuiMessage  eimsg;

    /*
    ** The following field is needed, because in case of IDCMP_RAWKEY
    ** IntuiMessage->IAddress is a pointer to this data, not the data
    ** itself (which is the case for IDCMP_VANILLAKEY)
    */

    APTR                    prevCodeQuals;
};

#define INT_INTUIMESSAGE(x) 	((struct IntIntuiMessage *)(x))

/*extern struct IntuitionBase * IntuitionBase;*/

#define IW(window)  	    	((struct IntWindow *) (window))

#define GetPubIBase(ib)     	((struct IntuitionBase *)(ib))
#define GetPrivIBase(ib)    	((struct IntIntuitionBase *)(ib))
#define IBase                   GetPrivIBase(IntuitionBase)

/* FIXME: Remove these #define xxxBase hacks
   Do not use this in new code !
*/

/* struct Utilitybase is used in the following file so include it
   before defining Utilitybase
*/
#include <proto/utility.h>

/* stegerg: one can have sysgadgets outside of window border! All sysgadgets in window
            border must have set GACT_???BORDER and, if they are in a gzz window, also
        GTYP_GZZGADGET */

#define IS_GZZ_GADGET(gad)  	(((gad)->GadgetType) & GTYP_GZZGADGET)

#define IS_BORDER_GADGET(gad) 	(IS_GZZ_GADGET(gad) || \
    	    	    	    	 ((gad)->Activation & (GACT_RIGHTBORDER|GACT_LEFTBORDER|GACT_TOPBORDER|GACT_BOTTOMBORDER)))

#define IS_SYS_GADGET(gad)  	(((gad)->GadgetType) & GTYP_SYSTYPEMASK)

#define IS_BOOPSI_GADGET(gad) 	(((gad)->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET)

/*#define IS_BORDER_GADGET(gad) \
    (((gad->GadgetType) & GTYP_SYSGADGET) \
    || ((gad)->Activation & (GACT_RIGHTBORDER|GACT_LEFTBORDER|GACT_TOPBORDER|GACT_BOTTOMBORDER))) */

#define IS_REQ_GADGET(gad)  	((gad)->GadgetType & GTYP_REQGADGET)

#define IS_SCREEN_GADGET(gad) 	((gad)->GadgetType & GTYP_SCRGADGET)

#define SET_GI_RPORT(gi, w, req, gad)   \
    (gi)->gi_RastPort = (IS_SCREEN_GADGET(gad) ? \
    ((gi)->gi_Screen->BarLayer ? (gi)->gi_Screen->BarLayer->rp : NULL) : \
    (IS_BORDER_GADGET(gad) ?  (w)->BorderRPort : \
    ((req) ? ((req)->ReqLayer->rp) : (w)->RPort)) \
)

#define REFRESHGAD_BOOPSI   	1   /* boopsi gadgets */
#define REFRESHGAD_BORDER   	2   /* gadgets in window border */
#define REFRESHGAD_REL      	4   /* gadgets with GFLG_RELRIGHT, GFLG_RELBOTTOM,GFLG_RELWIDTH, GFLG_RELHEIGHT */
#define REFRESHGAD_RELS     	8   /* GFLG_RELSPECIAL gadgets */
#define REFRESHGAD_TOPBORDER    16  /* used by setwindowtitle */
#define REFRESHGAD_NOGADTOOLS   32  /* used in some cases for _mustbe_ */

#define SYSGADGET_ACTIVE (iihdata->ActiveSysGadget != NULL)

VOID int_refreshglist(struct Gadget *gadgets, struct Window *window,
                      struct Requester *requester, LONG numGad, LONG mustbe, LONG mustnotbe,
                      struct IntuitionBase *IntuitionBase);

VOID int_RefreshWindowFrame(struct Window *window, LONG mustbe, LONG mustnotbe,
                            ULONG dwbflags,
                            struct IntuitionBase *IntuitionBase);

#define int_refreshwindowframe(a,b,c,d) int_RefreshWindowFrame(a,b,c,0,d);

struct RethinkDisplayActionMsg
{
    struct IntuiActionMsg msg;
    BOOL   lock;
    LONG   failure;
};

void int_RethinkDisplay(struct RethinkDisplayActionMsg *msg,struct IntuitionBase *IntuitionBase);

/* Keep the system gadgets in the same order than the original intuition,
 * some programs make bad asumptions about that...
 */
enum
{
    DEPTHGAD,
    ZOOMGAD,
    SIZEGAD,
    CLOSEGAD,
    ICONIFYGAD,
    LOCKGAD,
    MUIGAD,
    POPUPGAD,
    SNAPSHOTGAD,
    JUMPGAD,
    DRAGBAR,
    NUM_SYSGADS
};

#define SYSGAD(w, idx) (((struct IntWindow *)(w))->sysgads[idx])

struct IntWindow
{
    struct Window           window;

    Object                 *sysgads[NUM_SYSGADS];
    struct Image           *AmigaKey;
    struct Image           *Checkmark;
    struct Image           *SubMenuImage;
    struct Window          *menulendwindow;

    struct HashNode         hashnode;

    /* When the Zoom gadget is pressed the window will have the
       dimensions stored here. The old dimensions are backed up here
       again. */
    WORD                    ZipLeftEdge;
    WORD                    ZipTopEdge;
    WORD                    ZipWidth;
    WORD                    ZipHeight;

    /* max. number of mousemove events to send to this window */
    WORD                    mousequeue;

    /* act. number of mousemove events sent to this window */
    WORD                    num_mouseevents;

    /* max. number of repeated IDCMP_RAWKEY, IDCMP_VANILLAKEY and IDCMP_IDCMPUPDATE
       messages to send to this window */
    WORD                    repeatqueue;

    /* act. number of repeated IDCMP_RAWKEY, IDCMP_VANILLAKEY and IDCMP_IDCMPUPDATE
       messages sent to this window */
    WORD                    num_repeatevents;

    WORD                    sizeimage_width;
    WORD                    sizeimage_height;

    ULONG                   helpflags;
    ULONG                   helpgroup;

    Object                 *pointer;
    BOOL                    busy;
    BOOL                    free_pointer;
    UWORD                   pointer_delay;
    ULONG                   extrabuttons;
    ULONG                   extrabuttonsid; /* replaces ETI_Dummy if apps wishes it */
    ULONG                   reserved1;
    ULONG                   specialflags;

    char                    titlebuffer[TITLEBUFFERLEN+1];

    struct Layer           *wlayer;
    struct Layer           *borderlayer;

    struct timeval          lastmsgsent;
    struct timeval          lastmsgreplied;

    struct Hook     	    DefaultWindowShapeHook;
    BOOL                    CustomShape;
    struct Region          *OutlineShape;
    ULONG                   DecorUserBufferSize;
    IPTR                    DecorUserBuffer;
};

#define SPFLAG_ICONIFIED    	1
#define SPFLAG_NOICONIFY    	2
#define SPFLAG_SKININFO     	4
#define SPFLAG_LAYERREFRESH 	8
#define SPFLAG_TRANSPHOOK   	16
#define SPFLAG_LAYERRESIZED 	32
#define SPFLAG_USERPORT     	64
#define SPFLAG_IAMDEAD      	128
#define SPFLAG_CLOSING      	256 //used with iamdead
#define SPFLAG_WANTBUFFER   	512

#define HELPF_ISHELPGROUP   	1
#define HELPF_GADGETHELP    	2

#define IS_NOCAREREFRESH(win)   (((win)->Flags & WFLG_NOCAREREFRESH) ? TRUE  : FALSE)
#define IS_DOCAREREFRESH(win) 	(((win)->Flags & WFLG_NOCAREREFRESH) ? FALSE : TRUE )
#define IS_SIMPLEREFRESH(win) 	(((win)->Flags & WFLG_SIMPLE_REFRESH) ? TRUE : FALSE)
#define IS_GZZWINDOW(win)     	(((win)->Flags & WFLG_GIMMEZEROZERO) ? TRUE  : FALSE)

/* Flag definitions for MoreFlags */

#define WMFLG_NOTIFYDEPTH        (1 << 0)   /* Window wants notification when
it's depth arranged */

#define WMFLG_DO_UNLOCKPUBSCREEN (1 << 1)
#define WMFLG_MENUHELP           (1 << 2)
#define WMFLG_POINTERDELAY       (1 << 3)
#define WMFLG_TABLETMESSAGES     (1 << 4)

// FIXME: ehm... this one should die a horrible death!
#define WMFLG_IAMMUI             (1 << 5)

struct IntScreenBuffer
{
    struct ScreenBuffer sb;
    BOOL                free_bitmap;
};

/* Driver prototypes */

extern void intui_CloseWindow (struct Window *, struct IntuitionBase *);

void UpdateScreenBitMap(struct Screen *screen, struct IntuitionBase *IntuitionBase);

/* wbtasktalk protos */

ULONG TellWBTaskToCloseWindows(struct IntuitionBase *IntuitionBase);
ULONG TellWBTaskToOpenWindows(struct IntuitionBase *IntuitionBase);

/* intuition_misc protos */
extern void SetDisplayDefaults(struct IntuitionBase * IntuitionBase);
extern void LoadDefaultPreferences(struct IntuitionBase * IntuitionBase);

void FreeSysImages(struct Screen *scr, struct IntuitionBase *IntuitionBase);
extern void CheckRectFill(struct RastPort *rp, WORD x1, WORD y1, WORD x2, WORD y2, struct IntuitionBase * IntuitionBase);
extern BOOL CreateWinSysGadgets(struct Window *w, struct IntuitionBase *IntuitionBase);
extern VOID KillWinSysGadgets(struct Window *w, struct IntuitionBase *IntuitionBase);
extern void CreateScreenBar(struct Screen *scr, struct IntuitionBase *IntuitionBase);
extern void KillScreenBar(struct Screen *scr, struct IntuitionBase *IntuitionBase);
extern void RenderScreenBar(struct Screen *scr, BOOL refresh, struct IntuitionBase *IntuitionBase);
extern void RenderScreenTitle(struct Screen *scr, struct IntuitionBase *IntuitionBase);
extern void UpdateMouseCoords(struct Window *win);
extern WORD SubtractRectFromRect(struct Rectangle *a, struct Rectangle *b, struct Rectangle *destrectarray);

AROS_UFP3(BOOL, DefaultWindowShapeFunc,
    AROS_UFPA(struct Hook *, hook, A0),
    AROS_UFPA(struct Layer *, lay, A2),
    AROS_UFPA(struct ShapeHookMsg *, msg, A1));

extern LONG CalcResourceHash(APTR resource);
extern void AddResourceToList(APTR resource, UWORD resourcetype, struct IntuitionBase *IntuitionBase);
extern void RemoveResourceFromList(APTR resource, UWORD resourcetype, struct IntuitionBase *IntuitionBase);
extern BOOL ResourceExisting(APTR resource, UWORD resourcetype, struct IntuitionBase *IntuitionBase);

/* misc.c */
extern void SetPointerPos(struct IntuitionBase *IntuitionBase);
extern void ShowPointer(struct IntuitionBase *IntuitionBase);
extern void SetScreenPointer(struct IntScreen *scr, struct Window *window, Object *pointer,
                             struct IntuitionBase *IntuitionBase);
extern struct RastPort *MyCreateRastPort(struct IntuitionBase *IntuitionBase);
extern struct RastPort *MyCloneRastPort(struct IntuitionBase *IntuitionBase, struct RastPort *rp);
extern void MyFreeRastPort(struct IntuitionBase *IntuitionBase, struct RastPort *rp);
struct TextFont *SafeReopenFont(struct IntuitionBase *, struct TextFont **);
extern Object *MakePointerFromPrefs(struct IntuitionBase *, struct Preferences *);
extern Object *MakePointerFromData(struct IntuitionBase *, const UWORD *, int, int, int, int);
void InstallPointer(struct IntuitionBase *, Object **, Object *);
void SetPointerColors(struct IntuitionBase *IntuitionBase);
struct Gadget *DoActivateGadget(struct Window *win, struct Requester *req, struct Gadget *gad,
struct IntuitionBase *IntuitionBase);
VOID DoGMLayout(struct Gadget *glist, struct Window *win, struct Requester *req,
    	    	UWORD numgad, BOOL initial, struct IntuitionBase *IntuitionBase);
BOOL ih_fire_intuimessage(struct Window * w, ULONG Class, UWORD Code, APTR IAddress,
    	    	    	  struct IntuitionBase *IntuitionBase);
void NotifyDepthArrangement(struct Window *w, struct IntuitionBase *IntuitionBase);

/* printitext.c */

void int_PrintIText(struct RastPort * rp, struct IntuiText * iText,
            	    LONG leftOffset, LONG topOffset, BOOL ignore_attributes,
		    struct IntuitionBase *IntuitionBase);


void SetupGInfo(struct GadgetInfo *gi, struct Window *win, struct Requester *req,
struct Gadget *gad, struct IntuitionBase *IntuitionBase);

IPTR Custom_DoMethodA(struct IntuitionBase *, struct Gadget *, Msg);

#define HAS_CHILDREN(w)     	    (NULL != w->firstchild)

#define CreateRastPort()            MyCreateRastPort(IntuitionBase)
#define CloneRastPort(rp)           MyCloneRastPort(IntuitionBase, rp)
#define FreeRastPort(rp)            MyFreeRastPort(IntuitionBase, rp)

#define POINTERA_SharedPointer  0x80039010

/*
 * Private data structures of the classes defined by intuition.library
 */

/* ICClass */
struct ICData
{
    Object          *ic_Target;
    struct TagItem  *ic_Mapping;
    struct TagItem  *ic_CloneTags;
    ULONG            ic_LoopCounter;
};

/* ModelClass */
struct ModelData
{
    struct MinList memberlist;
};

/* FrameIClass */
struct FrameIData
{
    /* render bevel only with no fill? */
    BOOL fid_EdgesOnly;

    /* inverted bevel pens? */
    BOOL fid_Recessed;

    /* frame style? */
    WORD fid_FrameType;

    WORD fid_HOffset;
    WORD fid_VOffset;
};

/* SysIClass */
struct SysIData
{
    struct DrawInfo *dri;
    struct Image    *frame;
    ULONG            type;
    UWORD            flags;
};

/* FillRectClass */
struct FillRectData
{
    WORD apatsize;
    WORD mode;
};

/* GadgetClass */
struct GadgetData
{
    struct ExtGadget    EG;
    struct ICData       IC;
};

/* PropGClass */
struct PropGData
{
    /* We use a propinfo structure, because we use the same routines
    for intuition propgadtets and in propgclass */

    struct PropInfo  propinfo;

    /* A little kludge: since the HandleMouseMove function
    wants dx/dy and not absolute mouse coords, we
    have to remember the last ones */

    UWORD            last_x;
    UWORD            last_y;

    /* One only have to store total or visble, and use some other
    formulas than those in the RKRM:L, but for
    code simplicity I store them all.  */
    UWORD            top, visible, total;

    UWORD            flags;
    struct BBox     *old_knobbox;
    struct Hook     *DisplayHook;
};

/* StrGClass */
struct StrGData
{
    struct StringInfo   StrInfo;
    struct StringExtend StrExtend;
    UBYTE           	Flags;
};

/* GroupGClass */
struct GroupGData
{
    struct MinList   memberlist;
    struct Gadget    *activegad;
};

/* DragBarClass */
struct dragbar_data
{
    /* Current left- and topedge of moving window. Ie when the user releases
    the LMB after a windowdrag, the window's new coords will be (curleft, curtop)
    */

    LONG    	     curleft;
    LONG    	     curtop;

    /* The current x and y coordinates relative to curleft/curtop */
    LONG    	     mousex;
    LONG    	     mousey;

    /* Whether the dragframe is currently drawn or erased */
    BOOL    	     isrendered;

    /* Used to tell GM_GOINACTIVE whether the drag was canceled or not */
    BOOL    	     drag_canceled;

    /* Used to tell GM_GOINACTIVE whether UnlockLayer() or not */
    BOOL    	     drag_layerlock;
    BOOL    	     drag_gadgetlock;
    BOOL    	     drag_inputhandlerlock;
    BOOL    	     drag_refreshed;

    /* Rastport to use during update */
    struct RastPort *rp;

    ULONG   	     lasteventtime;

    LONG    	     startleft;
    LONG    	     starttop;

    /* used to prevent windows from opening/closing while user drags a window */
    BOOL    	     drag_windowlock;

    struct Task     *movetask;

};

/* SizeButtonClass */
struct sizebutton_data
{

    /* The current width and height of the rubber band frame */
    ULONG   	     width;
    ULONG   	     height;
    ULONG   	     top;
    ULONG   	     left;

    /* holds original sizes */
    ULONG   	     Width;
    ULONG   	     Height;
    ULONG   	     TopEdge;
    ULONG   	     LeftEdge;

    /* the offset of the mouse pointer to the rubber band frame*/
    LONG    	     mouseoffsetx;
    LONG    	     mouseoffsety;

    /* Whether the dragframe is currently drawn or erased */
    BOOL    	     isrendered;

    /* Used to tell GM_GOINACTIVE whether the drag was canceled or not */
    BOOL    	     drag_canceled;

    /* Used to tell GM_GOINACTIVE whether UnlockLayer() or not */
    BOOL    	     drag_layerlock;
    BOOL    	     drag_gadgetlock;
    BOOL    	     drag_inputhandlerlock;

    BOOL    	     drag_refreshed;

    /* Rastport to use during update */
    struct RastPort *rp;

    ULONG   	     lasteventtime;

    /* used to prevent windows from opening/closing while user drags a window */
    BOOL    	     drag_windowlock;

    ULONG   	     drag_ticks;

};

/* MenuBarLabelClass */
struct MenuBarLabelData
{
    struct DrawInfo *dri;
};

/* PointerClass */
struct PointerData
{
    struct SharedPointer *shared_pointer;
};

/* WinDecorClass */
#define SYSIMCACHE 16

/* A system image as drawn, to copy it from */
struct sysimcache
{
	struct BitMap	*bm;
	UWORD		 w, h;
	UBYTE		 which, state, framesize;
};

struct windecor_data
{
	ULONG		 userbuffersize;
	struct sysimcache cache[SYSIMCACHE];
	UWORD		 next;
};

/* ScrDecorClass */
struct scrdecor_data
{
	ULONG	userbuffersize;
};

/* MenuDecorClass */
struct menudecor_data
{
	ULONG	userbuffersize;
};

#endif /* INTUITION_INTERN_H */
