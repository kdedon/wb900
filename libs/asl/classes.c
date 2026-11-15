/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The dispatchers of asl's classes and their setup
*/

#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>

#include "asl_intern.h"

IPTR AslProp__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR AslProp__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR AslProp__OM_SET(Class *cl, Object *o, Msg msg);
IPTR AslProp__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR AslArrow__OM_NOTIFY(Class *cl, Object *o, Msg msg);
IPTR AslArrow__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR AslListView__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR AslListView__OM_SET(Class *cl, Object *o, Msg msg);
IPTR AslListView__OM_GET(Class *cl, Object *o, Msg msg);
IPTR AslListView__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR AslListView__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR AslListView__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR AslListView__GM_LAYOUT(Class *cl, Object *o, Msg msg);
IPTR AslListView__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR AslButton__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR AslButton__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR AslButton__GM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR AslButton__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR AslButton__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR AslButton__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR AslButton__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR AslButton__GM_LAYOUT(Class *cl, Object *o, Msg msg);
IPTR AslString__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR AslString__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR AslString__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR AslCycle__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR AslCycle__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR AslCycle__OM_SET(Class *cl, Object *o, Msg msg);
IPTR AslCycle__OM_GET(Class *cl, Object *o, Msg msg);
IPTR AslCycle__GM_LAYOUT(Class *cl, Object *o, Msg msg);
IPTR AslCycle__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR AslCycle__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR AslCycle__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR AslCycle__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR AslFontPreview__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR AslFontPreview__OM_SET(Class *cl, Object *o, Msg msg);
IPTR AslFontPreview__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR AslFontPreview__GM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR AslFontPreview__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR AslFontStyle__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR AslFontStyle__OM_SET(Class *cl, Object *o, Msg msg);
IPTR AslFontStyle__OM_GET(Class *cl, Object *o, Msg msg);
IPTR AslFontStyle__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR AslFontStyle__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR AslFontStyle__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR AslColorPicker__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR AslColorPicker__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR AslColorPicker__OM_SET(Class *cl, Object *o, Msg msg);
IPTR AslColorPicker__OM_GET(Class *cl, Object *o, Msg msg);
IPTR AslColorPicker__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR AslColorPicker__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR AslColorPicker__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR AslColorPicker__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR AslEraser__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR AslEraser__GM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR AslEraser__GM_RENDER(Class *cl, Object *o, Msg msg);

static IPTR AslProp_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return AslProp__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return AslProp__OM_DISPOSE(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return AslProp__OM_SET(cl, o, msg);
    case GM_RENDER:
        return AslProp__GM_RENDER(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR AslArrow_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NOTIFY:
        return AslArrow__OM_NOTIFY(cl, o, msg);
    case GM_GOACTIVE:
        return AslArrow__GM_GOACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR AslListView_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return AslListView__OM_NEW(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return AslListView__OM_SET(cl, o, msg);
    case OM_GET:
        return AslListView__OM_GET(cl, o, msg);
    case OM_DISPOSE:
        return AslListView__OM_DISPOSE(cl, o, msg);
    case GM_GOACTIVE:
        return AslListView__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return AslListView__GM_HANDLEINPUT(cl, o, msg);
    case GM_LAYOUT:
        return AslListView__GM_LAYOUT(cl, o, msg);
    case GM_RENDER:
        return AslListView__GM_RENDER(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR AslButton_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return AslButton__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return AslButton__OM_DISPOSE(cl, o, msg);
    case GM_HITTEST:
        return AslButton__GM_HITTEST(cl, o, msg);
    case GM_GOACTIVE:
        return AslButton__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return AslButton__GM_HANDLEINPUT(cl, o, msg);
    case GM_GOINACTIVE:
        return AslButton__GM_GOINACTIVE(cl, o, msg);
    case GM_RENDER:
        return AslButton__GM_RENDER(cl, o, msg);
    case GM_LAYOUT:
        return AslButton__GM_LAYOUT(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR AslString_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return AslString__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return AslString__OM_DISPOSE(cl, o, msg);
    case GM_RENDER:
        return AslString__GM_RENDER(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR AslCycle_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return AslCycle__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return AslCycle__OM_DISPOSE(cl, o, msg);
    case OM_SET:
        return AslCycle__OM_SET(cl, o, msg);
    case OM_GET:
        return AslCycle__OM_GET(cl, o, msg);
    case GM_LAYOUT:
        return AslCycle__GM_LAYOUT(cl, o, msg);
    case GM_RENDER:
        return AslCycle__GM_RENDER(cl, o, msg);
    case GM_GOACTIVE:
        return AslCycle__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return AslCycle__GM_HANDLEINPUT(cl, o, msg);
    case GM_GOINACTIVE:
        return AslCycle__GM_GOINACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR AslFontPreview_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return AslFontPreview__OM_NEW(cl, o, msg);
    case OM_SET:
        return AslFontPreview__OM_SET(cl, o, msg);
    case OM_DISPOSE:
        return AslFontPreview__OM_DISPOSE(cl, o, msg);
    case GM_HITTEST:
        return AslFontPreview__GM_HITTEST(cl, o, msg);
    case GM_RENDER:
        return AslFontPreview__GM_RENDER(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR AslFontStyle_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return AslFontStyle__OM_NEW(cl, o, msg);
    case OM_SET:
        return AslFontStyle__OM_SET(cl, o, msg);
    case OM_GET:
        return AslFontStyle__OM_GET(cl, o, msg);
    case OM_DISPOSE:
        return AslFontStyle__OM_DISPOSE(cl, o, msg);
    case GM_RENDER:
        return AslFontStyle__GM_RENDER(cl, o, msg);
    case GM_GOACTIVE:
        return AslFontStyle__GM_GOACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR AslColorPicker_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return AslColorPicker__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return AslColorPicker__OM_DISPOSE(cl, o, msg);
    case OM_SET:
        return AslColorPicker__OM_SET(cl, o, msg);
    case OM_GET:
        return AslColorPicker__OM_GET(cl, o, msg);
    case GM_RENDER:
        return AslColorPicker__GM_RENDER(cl, o, msg);
    case GM_GOACTIVE:
        return AslColorPicker__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return AslColorPicker__GM_HANDLEINPUT(cl, o, msg);
    case GM_GOINACTIVE:
        return AslColorPicker__GM_GOINACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR AslEraser_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return AslEraser__OM_NEW(cl, o, msg);
    case GM_HITTEST:
        return AslEraser__GM_HITTEST(cl, o, msg);
    case GM_RENDER:
        return AslEraser__GM_RENDER(cl, o, msg);
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
    { PROPGCLASS, sizeof(struct AslPropData), offsetof(struct AslBase_intern, aslpropclass), AslProp_Dispatcher },
    { BUTTONGCLASS, sizeof(struct AslArrowData), offsetof(struct AslBase_intern, aslarrowclass), AslArrow_Dispatcher },
    { GADGETCLASS, sizeof(struct AslListViewData), offsetof(struct AslBase_intern, asllistviewclass), AslListView_Dispatcher },
    { GADGETCLASS, sizeof(struct AslButtonData), offsetof(struct AslBase_intern, aslbuttonclass), AslButton_Dispatcher },
    { STRGCLASS, sizeof(struct AslStringData), offsetof(struct AslBase_intern, aslstringclass), AslString_Dispatcher },
    { GADGETCLASS, sizeof(struct AslCycleData), offsetof(struct AslBase_intern, aslcycleclass), AslCycle_Dispatcher },
    { GADGETCLASS, sizeof(struct AslFontPreviewData), offsetof(struct AslBase_intern, aslfontpreviewclass), AslFontPreview_Dispatcher },
    { GADGETCLASS, sizeof(struct AslFontStyleData), offsetof(struct AslBase_intern, aslfontstyleclass), AslFontStyle_Dispatcher },
    { GADGETCLASS, sizeof(struct AslColorPickerData), offsetof(struct AslBase_intern, aslcolorpickerclass), AslColorPicker_Dispatcher },
    { GADGETCLASS, 0, offsetof(struct AslBase_intern, asleraserclass), AslEraser_Dispatcher },
};

#define NUMCLASSES (sizeof(classtab) / sizeof(classtab[0]))
#define CLASSPTR(i) (*(struct IClass **)((UBYTE *)AslBase + classtab[i].field))

BOOL InitClasses(struct AslBase_intern *AslBase)
{
    struct IClass *cl;
    WORD i;

    for (i = 0; i < NUMCLASSES; i++)
    {
        cl = MakeClass(NULL, classtab[i].super, NULL, classtab[i].size, 0);
        if (!cl)
            return FALSE;
        cl->cl_Dispatcher.h_Entry = (ULONG (*)())classtab[i].dispatcher;
        cl->cl_UserData = (IPTR)AslBase;
        CLASSPTR(i) = cl;
    }
    return TRUE;
}

/* FALSE while a class has objects */
BOOL FreeClasses(struct AslBase_intern *AslBase)
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
