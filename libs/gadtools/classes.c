/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The dispatchers of gadtools' classes and their setup
*/

#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <proto/intuition.h>
#include <proto/alib.h>

#include "gadtools_intern.h"

IPTR GTButton__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTButton__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTButton__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GTButton__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTButton__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTText__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTText__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTText__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GTText__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTText__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTText__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR GTSlider__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTSlider__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTSlider__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GTSlider__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTSlider__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR GTSlider__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR GTSlider__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTScroller__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTScroller__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTScroller__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GTScroller__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTScroller__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTArrow__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTArrow__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTArrow__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTString__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTString__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTString__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTString__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTListView__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTListView__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GTListView__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTListView__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTListView__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTListView__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR GTListView__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR GTCheckBox__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTCheckBox__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTCheckBox__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GTCheckBox__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTCheckBox__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTCheckBox__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR GTCheckBox__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR GTCheckBox__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR GTCycle__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTCycle__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTCycle__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GTCycle__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTCycle__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTCycle__GM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR GTCycle__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR GTCycle__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR GTCycle__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR GTMX__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTMX__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTMX__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GTMX__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTMX__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTMX__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR GTPalette__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GTPalette__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GTPalette__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GTPalette__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GTPalette__GM_LAYOUT(Class *cl, Object *o, Msg msg);
IPTR GTPalette__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR GTPalette__GM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR GTPalette__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR GTPalette__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);

static IPTR GTButton_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTButton__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return GTButton__OM_DISPOSE(cl, o, msg);
    case OM_SET:
        return GTButton__OM_SET(cl, o, msg);
    case OM_GET:
        return GTButton__OM_GET(cl, o, msg);
    case GM_RENDER:
        return GTButton__GM_RENDER(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTText_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTText__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return GTText__OM_DISPOSE(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return GTText__OM_SET(cl, o, msg);
    case OM_GET:
        return GTText__OM_GET(cl, o, msg);
    case GM_RENDER:
        return GTText__GM_RENDER(cl, o, msg);
    case GM_GOACTIVE:
        return GTText__GM_GOACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTSlider_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTSlider__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return GTSlider__OM_DISPOSE(cl, o, msg);
    case OM_SET:
        return GTSlider__OM_SET(cl, o, msg);
    case OM_GET:
        return GTSlider__OM_GET(cl, o, msg);
    case GM_GOACTIVE:
        return GTSlider__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return GTSlider__GM_HANDLEINPUT(cl, o, msg);
    case GM_RENDER:
        return GTSlider__GM_RENDER(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTScroller_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTScroller__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return GTScroller__OM_DISPOSE(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return GTScroller__OM_SET(cl, o, msg);
    case OM_GET:
        return GTScroller__OM_GET(cl, o, msg);
    case GM_RENDER:
        return GTScroller__GM_RENDER(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTArrow_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTArrow__OM_NEW(cl, o, msg);
    case OM_GET:
        return GTArrow__OM_GET(cl, o, msg);
    case OM_DISPOSE:
        return GTArrow__OM_DISPOSE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTString_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
    case OM_SET:
        return GTString__OM_NEW(cl, o, msg);
    case OM_GET:
        return GTString__OM_GET(cl, o, msg);
    case OM_DISPOSE:
        return GTString__OM_DISPOSE(cl, o, msg);
    case GM_RENDER:
        return GTString__GM_RENDER(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTListView_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTListView__OM_NEW(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return GTListView__OM_SET(cl, o, msg);
    case OM_GET:
        return GTListView__OM_GET(cl, o, msg);
    case OM_DISPOSE:
        return GTListView__OM_DISPOSE(cl, o, msg);
    case GM_RENDER:
        return GTListView__GM_RENDER(cl, o, msg);
    case GM_HANDLEINPUT:
    case GM_GOACTIVE:
        return GTListView__GM_HANDLEINPUT(cl, o, msg);
    case GM_GOINACTIVE:
        return GTListView__GM_GOINACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTCheckBox_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTCheckBox__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return GTCheckBox__OM_DISPOSE(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return GTCheckBox__OM_SET(cl, o, msg);
    case OM_GET:
        return GTCheckBox__OM_GET(cl, o, msg);
    case GM_RENDER:
        return GTCheckBox__GM_RENDER(cl, o, msg);
    case GM_GOACTIVE:
        return GTCheckBox__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return GTCheckBox__GM_HANDLEINPUT(cl, o, msg);
    case GM_GOINACTIVE:
        return GTCheckBox__GM_GOINACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTCycle_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTCycle__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return GTCycle__OM_DISPOSE(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return GTCycle__OM_SET(cl, o, msg);
    case OM_GET:
        return GTCycle__OM_GET(cl, o, msg);
    case GM_RENDER:
        return GTCycle__GM_RENDER(cl, o, msg);
    case GM_HITTEST:
        return GTCycle__GM_HITTEST(cl, o, msg);
    case GM_GOACTIVE:
        return GTCycle__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return GTCycle__GM_HANDLEINPUT(cl, o, msg);
    case GM_GOINACTIVE:
        return GTCycle__GM_GOINACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTMX_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTMX__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return GTMX__OM_DISPOSE(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return GTMX__OM_SET(cl, o, msg);
    case OM_GET:
        return GTMX__OM_GET(cl, o, msg);
    case GM_RENDER:
        return GTMX__GM_RENDER(cl, o, msg);
    case GM_GOACTIVE:
        return GTMX__GM_GOACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GTPalette_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return GTPalette__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return GTPalette__OM_DISPOSE(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return GTPalette__OM_SET(cl, o, msg);
    case OM_GET:
        return GTPalette__OM_GET(cl, o, msg);
    case GM_LAYOUT:
        return GTPalette__GM_LAYOUT(cl, o, msg);
    case GM_RENDER:
        return GTPalette__GM_RENDER(cl, o, msg);
    case GM_HITTEST:
        return GTPalette__GM_HITTEST(cl, o, msg);
    case GM_GOACTIVE:
        return GTPalette__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return GTPalette__GM_HANDLEINPUT(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

/* The classes, private, with their superclasses */
static const struct
{
    ClassID      super;
    UWORD        size;
    UWORD        field;  /* offset of the class pointer in the base */
    IPTR       (*dispatcher)(Class *, Object *, Msg);
} classtab[] =
{
    { FRBUTTONCLASS, sizeof(struct ButtonData), offsetof(struct GadToolsBase_intern, buttonclass), GTButton_Dispatcher },
    { GADGETCLASS, sizeof(struct TextData), offsetof(struct GadToolsBase_intern, textclass), GTText_Dispatcher },
    { PROPGCLASS, sizeof(struct SliderData), offsetof(struct GadToolsBase_intern, sliderclass), GTSlider_Dispatcher },
    { PROPGCLASS, sizeof(struct ScrollerData), offsetof(struct GadToolsBase_intern, scrollerclass), GTScroller_Dispatcher },
    { FRBUTTONCLASS, sizeof(struct ArrowData), offsetof(struct GadToolsBase_intern, arrowclass), GTArrow_Dispatcher },
    { STRGCLASS, sizeof(struct StringData), offsetof(struct GadToolsBase_intern, stringclass), GTString_Dispatcher },
    { GADGETCLASS, sizeof(struct LVData), offsetof(struct GadToolsBase_intern, listviewclass), GTListView_Dispatcher },
    { GADGETCLASS, sizeof(struct CheckBoxData), offsetof(struct GadToolsBase_intern, checkboxclass), GTCheckBox_Dispatcher },
    { GADGETCLASS, sizeof(struct CycleData), offsetof(struct GadToolsBase_intern, cycleclass), GTCycle_Dispatcher },
    { GADGETCLASS, sizeof(struct MXData), offsetof(struct GadToolsBase_intern, mxclass), GTMX_Dispatcher },
    { GADGETCLASS, sizeof(struct PaletteData), offsetof(struct GadToolsBase_intern, paletteclass), GTPalette_Dispatcher },
};

#define NUMCLASSES (sizeof(classtab) / sizeof(classtab[0]))
#define CLASSPTR(i) (*(struct IClass **)((UBYTE *)GadToolsBase + classtab[i].field))

BOOL InitClasses(struct GadToolsBase_intern *GadToolsBase)
{
    struct IClass *cl;
    WORD i;

    for (i = 0; i < NUMCLASSES; i++)
    {
        cl = MakeClass(NULL, classtab[i].super, NULL, classtab[i].size, 0);
        if (!cl)
            return FALSE;
        cl->cl_Dispatcher.h_Entry = (ULONG (*)())classtab[i].dispatcher;
        cl->cl_UserData = (IPTR)GadToolsBase;
        CLASSPTR(i) = cl;
    }
    return TRUE;
}

/* FALSE while a class has objects */
BOOL FreeClasses(struct GadToolsBase_intern *GadToolsBase)
{
    WORD i;

    for (i = 0; i < NUMCLASSES; i++)
        if (CLASSPTR(i) && CLASSPTR(i)->cl_ObjectCount)
            return FALSE;
    for (i = 0; i < NUMCLASSES; i++)
    {
        if (CLASSPTR(i))
            FreeClass(CLASSPTR(i));
        CLASSPTR(i) = NULL;
    }
    return TRUE;
}
