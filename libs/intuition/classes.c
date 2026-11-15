/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: The dispatchers of intuition's classes and their setup
*/

#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/icclass.h>
#include <intuition/windecorclass.h>
#include <intuition/scrdecorclass.h>
#include <intuition/menudecorclass.h>
#include <intuition/pointerclass.h>
#include <proto/intuition.h>

#include "intuition_intern.h"

IPTR ICClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR ICClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR ICClass__OM_NOTIFY(Class *cl, Object *o, Msg msg);
IPTR ICClass__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR ICClass__OM_GET(Class *cl, Object *o, Msg msg);
void ICClass__ICM_SETLOOP(Class *cl, Object *o, Msg msg);
void ICClass__ICM_CLEARLOOP(Class *cl, Object *o, Msg msg);
IPTR ICClass__ICM_CHECKLOOP(Class *cl, Object *o, Msg msg);
IPTR ImageClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR ImageClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR ImageClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR ImageClass__IM_ERASE(Class *cl, Object *o, Msg msg);
IPTR ImageClass__IM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__OM_NOTIFY(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__One(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__Zero(Class *cl, Object *o, Msg msg);
IPTR GadgetClass_NoReuse(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__GM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__GM_HELPTEST(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__ICM_SETLOOP(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__ICM_CLEARLOOP(Class *cl, Object *o, Msg msg);
IPTR GadgetClass__ICM_CHECKLOOP(Class *cl, Object *o, Msg msg);
IPTR PointerClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR PointerClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR PointerClass__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__WDM_GETDEFSIZE_SYSIMAGE(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__WDM_DRAW_SYSIMAGE(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__WDM_DRAW_WINBORDER(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__WDM_LAYOUT_BORDERGADGETS(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__WDM_DRAW_BORDERPROPBACK(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__WDM_DRAW_BORDERPROPKNOB(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__WDM_INITWINDOW(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__WDM_EXITWINDOW(Class *cl, Object *o, Msg msg);
IPTR WinDecorClass__WDM_WINDOWSHAPE(Class *cl, Object *o, Msg msg);
IPTR ScrDecorClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR ScrDecorClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR ScrDecorClass__SDM_INITSCREEN(Class *cl, Object *o, Msg msg);
IPTR ScrDecorClass__SDM_EXITSCREEN(Class *cl, Object *o, Msg msg);
IPTR ScrDecorClass__SDM_GETDEFSIZE_SYSIMAGE(Class *cl, Object *o, Msg msg);
IPTR ScrDecorClass__SDM_DRAW_SYSIMAGE(Class *cl, Object *o, Msg msg);
IPTR ScrDecorClass__SDM_DRAW_SCREENBAR(Class *cl, Object *o, Msg msg);
IPTR ScrDecorClass__SDM_LAYOUT_SCREENGADGETS(Class *cl, Object *o, Msg msg);
IPTR MenuDecorClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR MenuDecorClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR MenuDecorClass__MDM_GETDEFSIZE_SYSIMAGE(Class *cl, Object *o, Msg msg);
IPTR MenuDecorClass__MDM_DRAW_SYSIMAGE(Class *cl, Object *o, Msg msg);
IPTR MenuDecorClass__MDM_GETMENUSPACES(Class *cl, Object *o, Msg msg);
IPTR MenuDecorClass__MDM_DRAWBACKGROUND(Class *cl, Object *o, Msg msg);
IPTR MenuDecorClass__MDM_INITMENU(Class *cl, Object *o, Msg msg);
IPTR MenuDecorClass__MDM_EXITMENU(Class *cl, Object *o, Msg msg);
IPTR ScreenClass__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR ScreenClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR ScreenClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR FrameIClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR FrameIClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR FrameIClass__IM_FRAMEBOX(Class *cl, Object *o, Msg msg);
IPTR FrameIClass__IM_DRAW(Class *cl, Object *o, Msg msg);
IPTR FrameIClass__IM_DRAWFRAME(Class *cl, Object *o, Msg msg);
Object * SysIClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR SysIClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR SysIClass__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR SysIClass__IM_DRAW(Class *cl, Object *o, Msg msg);
IPTR ButtonGClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR ButtonGClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR ButtonGClass__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR ButtonGClass__GM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR ButtonGClass__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR ButtonGClass__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR ButtonGClass__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR PropGClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR PropGClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR PropGClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR PropGClass__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR PropGClass__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR PropGClass__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR PropGClass__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR StrGClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR StrGClass__OM_DISPOSE(Class *cl, Object *o, Msg msg);
IPTR StrGClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR StrGClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR StrGClass__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR StrGClass__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR StrGClass__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR StrGClass__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR DragBarClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR DragBarClass__NOP(Class *cl, Object *o, Msg msg);
IPTR DragBarClass__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR DragBarClass__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR DragBarClass__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR DragBarClass__GM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR SizeButtonClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR SizeButtonClass__GM_GOACTIVE(Class *cl, Object *o, Msg msg);
IPTR SizeButtonClass__GM_GOINACTIVE(Class *cl, Object *o, Msg msg);
IPTR SizeButtonClass__GM_HANDLEINPUT(Class *cl, Object *o, Msg msg);
IPTR FrButtonClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR FrButtonClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR FrButtonClass__GM_RENDER(Class *cl, Object *o, Msg msg);
IPTR FrButtonClass__GM_HITTEST(Class *cl, Object *o, Msg msg);
IPTR MenuBarLabelClass__OM_NEW(Class *cl, Object *o, Msg msg);
IPTR MenuBarLabelClass__OM_SET(Class *cl, Object *o, Msg msg);
IPTR MenuBarLabelClass__OM_GET(Class *cl, Object *o, Msg msg);
IPTR MenuBarLabelClass__IM_DRAW(Class *cl, Object *o, Msg msg);

static IPTR ICClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)ICClass__OM_NEW(cl, o, msg);
    case OM_SET:
        return (IPTR)ICClass__OM_SET(cl, o, msg);
    case OM_NOTIFY:
    case OM_UPDATE:
        return (IPTR)ICClass__OM_NOTIFY(cl, o, msg);
    case OM_DISPOSE:
        return (IPTR)ICClass__OM_DISPOSE(cl, o, msg);
    case OM_GET:
        return (IPTR)ICClass__OM_GET(cl, o, msg);
    case ICM_SETLOOP:
        ICClass__ICM_SETLOOP(cl, o, msg);
        return 0;
    case ICM_CLEARLOOP:
        ICClass__ICM_CLEARLOOP(cl, o, msg);
        return 0;
    case ICM_CHECKLOOP:
        return (IPTR)ICClass__ICM_CHECKLOOP(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR ImageClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)ImageClass__OM_NEW(cl, o, msg);
    case OM_SET:
        return (IPTR)ImageClass__OM_SET(cl, o, msg);
    case OM_GET:
        return (IPTR)ImageClass__OM_GET(cl, o, msg);
    case IM_ERASE:
    case IM_ERASEFRAME:
        return (IPTR)ImageClass__IM_ERASE(cl, o, msg);
    case IM_HITTEST:
    case IM_HITFRAME:
        return (IPTR)ImageClass__IM_HITTEST(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR GadgetClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)GadgetClass__OM_NEW(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return (IPTR)GadgetClass__OM_SET(cl, o, msg);
    case OM_GET:
        return (IPTR)GadgetClass__OM_GET(cl, o, msg);
    case OM_NOTIFY:
        return (IPTR)GadgetClass__OM_NOTIFY(cl, o, msg);
    case OM_DISPOSE:
        return (IPTR)GadgetClass__OM_DISPOSE(cl, o, msg);
    case GM_RENDER:
        return (IPTR)GadgetClass__One(cl, o, msg);
    case GM_LAYOUT:
    case GM_DOMAIN:
    case GM_GOINACTIVE:
        return (IPTR)GadgetClass__Zero(cl, o, msg);
    case GM_HANDLEINPUT:
    case GM_GOACTIVE:
        return (IPTR)GadgetClass_NoReuse(cl, o, msg);
    case GM_HITTEST:
        return (IPTR)GadgetClass__GM_HITTEST(cl, o, msg);
    case GM_HELPTEST:
        return (IPTR)GadgetClass__GM_HELPTEST(cl, o, msg);
    case ICM_SETLOOP:
        return (IPTR)GadgetClass__ICM_SETLOOP(cl, o, msg);
    case ICM_CLEARLOOP:
        return (IPTR)GadgetClass__ICM_CLEARLOOP(cl, o, msg);
    case ICM_CHECKLOOP:
        return (IPTR)GadgetClass__ICM_CHECKLOOP(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR PointerClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)PointerClass__OM_NEW(cl, o, msg);
    case OM_GET:
        return (IPTR)PointerClass__OM_GET(cl, o, msg);
    case OM_DISPOSE:
        return (IPTR)PointerClass__OM_DISPOSE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR WinDecorClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)WinDecorClass__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return (IPTR)WinDecorClass__OM_DISPOSE(cl, o, msg);
    case OM_GET:
        return (IPTR)WinDecorClass__OM_GET(cl, o, msg);
    case WDM_GETDEFSIZE_SYSIMAGE:
        return (IPTR)WinDecorClass__WDM_GETDEFSIZE_SYSIMAGE(cl, o, msg);
    case WDM_DRAW_SYSIMAGE:
        return (IPTR)WinDecorClass__WDM_DRAW_SYSIMAGE(cl, o, msg);
    case WDM_DRAW_WINBORDER:
        return (IPTR)WinDecorClass__WDM_DRAW_WINBORDER(cl, o, msg);
    case WDM_LAYOUT_BORDERGADGETS:
        return (IPTR)WinDecorClass__WDM_LAYOUT_BORDERGADGETS(cl, o, msg);
    case WDM_DRAW_BORDERPROPBACK:
        return (IPTR)WinDecorClass__WDM_DRAW_BORDERPROPBACK(cl, o, msg);
    case WDM_DRAW_BORDERPROPKNOB:
        return (IPTR)WinDecorClass__WDM_DRAW_BORDERPROPKNOB(cl, o, msg);
    case WDM_INITWINDOW:
        return (IPTR)WinDecorClass__WDM_INITWINDOW(cl, o, msg);
    case WDM_EXITWINDOW:
        return (IPTR)WinDecorClass__WDM_EXITWINDOW(cl, o, msg);
    case WDM_WINDOWSHAPE:
        return (IPTR)WinDecorClass__WDM_WINDOWSHAPE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR ScrDecorClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)ScrDecorClass__OM_NEW(cl, o, msg);
    case OM_GET:
        return (IPTR)ScrDecorClass__OM_GET(cl, o, msg);
    case SDM_INITSCREEN:
        return (IPTR)ScrDecorClass__SDM_INITSCREEN(cl, o, msg);
    case SDM_EXITSCREEN:
        return (IPTR)ScrDecorClass__SDM_EXITSCREEN(cl, o, msg);
    case SDM_GETDEFSIZE_SYSIMAGE:
        return (IPTR)ScrDecorClass__SDM_GETDEFSIZE_SYSIMAGE(cl, o, msg);
    case SDM_DRAW_SYSIMAGE:
        return (IPTR)ScrDecorClass__SDM_DRAW_SYSIMAGE(cl, o, msg);
    case SDM_DRAW_SCREENBAR:
        return (IPTR)ScrDecorClass__SDM_DRAW_SCREENBAR(cl, o, msg);
    case SDM_LAYOUT_SCREENGADGETS:
        return (IPTR)ScrDecorClass__SDM_LAYOUT_SCREENGADGETS(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR MenuDecorClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)MenuDecorClass__OM_NEW(cl, o, msg);
    case OM_GET:
        return (IPTR)MenuDecorClass__OM_GET(cl, o, msg);
    case MDM_GETDEFSIZE_SYSIMAGE:
        return (IPTR)MenuDecorClass__MDM_GETDEFSIZE_SYSIMAGE(cl, o, msg);
    case MDM_DRAW_SYSIMAGE:
        return (IPTR)MenuDecorClass__MDM_DRAW_SYSIMAGE(cl, o, msg);
    case MDM_GETMENUSPACES:
        return (IPTR)MenuDecorClass__MDM_GETMENUSPACES(cl, o, msg);
    case MDM_DRAWBACKGROUND:
        return (IPTR)MenuDecorClass__MDM_DRAWBACKGROUND(cl, o, msg);
    case MDM_INITMENU:
        return (IPTR)MenuDecorClass__MDM_INITMENU(cl, o, msg);
    case MDM_EXITMENU:
        return (IPTR)MenuDecorClass__MDM_EXITMENU(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR ScreenClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_DISPOSE:
        return (IPTR)ScreenClass__OM_DISPOSE(cl, o, msg);
    case OM_GET:
        return (IPTR)ScreenClass__OM_GET(cl, o, msg);
    case OM_SET:
        return (IPTR)ScreenClass__OM_SET(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR FrameIClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)FrameIClass__OM_NEW(cl, o, msg);
    case OM_SET:
        return (IPTR)FrameIClass__OM_SET(cl, o, msg);
    case IM_FRAMEBOX:
        return (IPTR)FrameIClass__IM_FRAMEBOX(cl, o, msg);
    case IM_DRAW:
        return (IPTR)FrameIClass__IM_DRAW(cl, o, msg);
    case IM_DRAWFRAME:
        return (IPTR)FrameIClass__IM_DRAWFRAME(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR SysIClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)SysIClass__OM_NEW(cl, o, msg);
    case OM_SET:
        return (IPTR)SysIClass__OM_SET(cl, o, msg);
    case OM_DISPOSE:
        return (IPTR)SysIClass__OM_DISPOSE(cl, o, msg);
    case IM_DRAW:
        return (IPTR)SysIClass__IM_DRAW(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR ButtonGClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)ButtonGClass__OM_NEW(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return (IPTR)ButtonGClass__OM_SET(cl, o, msg);
    case GM_RENDER:
        return (IPTR)ButtonGClass__GM_RENDER(cl, o, msg);
    case GM_HITTEST:
        return (IPTR)ButtonGClass__GM_HITTEST(cl, o, msg);
    case GM_GOACTIVE:
        return (IPTR)ButtonGClass__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return (IPTR)ButtonGClass__GM_HANDLEINPUT(cl, o, msg);
    case GM_GOINACTIVE:
        return (IPTR)ButtonGClass__GM_GOINACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR PropGClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)PropGClass__OM_NEW(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return (IPTR)PropGClass__OM_SET(cl, o, msg);
    case OM_GET:
        return (IPTR)PropGClass__OM_GET(cl, o, msg);
    case GM_RENDER:
        return (IPTR)PropGClass__GM_RENDER(cl, o, msg);
    case GM_GOACTIVE:
        return (IPTR)PropGClass__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return (IPTR)PropGClass__GM_HANDLEINPUT(cl, o, msg);
    case GM_GOINACTIVE:
        return (IPTR)PropGClass__GM_GOINACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR StrGClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)StrGClass__OM_NEW(cl, o, msg);
    case OM_DISPOSE:
        return (IPTR)StrGClass__OM_DISPOSE(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return (IPTR)StrGClass__OM_SET(cl, o, msg);
    case OM_GET:
        return (IPTR)StrGClass__OM_GET(cl, o, msg);
    case GM_RENDER:
        return (IPTR)StrGClass__GM_RENDER(cl, o, msg);
    case GM_GOACTIVE:
        return (IPTR)StrGClass__GM_GOACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return (IPTR)StrGClass__GM_HANDLEINPUT(cl, o, msg);
    case GM_GOINACTIVE:
        return (IPTR)StrGClass__GM_GOINACTIVE(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR DragBarClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)DragBarClass__OM_NEW(cl, o, msg);
    case GM_LAYOUT:
    case GM_DOMAIN:
        return (IPTR)DragBarClass__NOP(cl, o, msg);
    case GM_GOACTIVE:
        return (IPTR)DragBarClass__GM_GOACTIVE(cl, o, msg);
    case GM_GOINACTIVE:
        return (IPTR)DragBarClass__GM_GOINACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return (IPTR)DragBarClass__GM_HANDLEINPUT(cl, o, msg);
    case GM_HITTEST:
        return (IPTR)DragBarClass__GM_HITTEST(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR SizeButtonClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)SizeButtonClass__OM_NEW(cl, o, msg);
    case GM_LAYOUT:
    case GM_DOMAIN:
        return (IPTR)DragBarClass__NOP(cl, o, msg);
    case GM_GOACTIVE:
        return (IPTR)SizeButtonClass__GM_GOACTIVE(cl, o, msg);
    case GM_GOINACTIVE:
        return (IPTR)SizeButtonClass__GM_GOINACTIVE(cl, o, msg);
    case GM_HANDLEINPUT:
        return (IPTR)SizeButtonClass__GM_HANDLEINPUT(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR FrButtonClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return (IPTR)FrButtonClass__OM_NEW(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return (IPTR)FrButtonClass__OM_SET(cl, o, msg);
    case GM_RENDER:
        return (IPTR)FrButtonClass__GM_RENDER(cl, o, msg);
    case GM_HITTEST:
        return (IPTR)FrButtonClass__GM_HITTEST(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

static IPTR MenuBarLabelClass_Dispatcher(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return MenuBarLabelClass__OM_NEW(cl, o, msg);
    case OM_SET:
        return MenuBarLabelClass__OM_SET(cl, o, msg);
    case OM_GET:
        return MenuBarLabelClass__OM_GET(cl, o, msg);
    case IM_DRAW:
        return MenuBarLabelClass__IM_DRAW(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}

/* The classes, superclasses first */
static const struct
{
    ClassID      id;
    ClassID      super;
    UWORD        size;
    UWORD        field;  /* offset of the IntIntuitionBase class pointer, or 0 */
    IPTR       (*dispatcher)(Class *, Object *, Msg);
} classtab[] =
{
    { ICCLASS, ROOTCLASS, sizeof(struct ICData), 0, ICClass_Dispatcher },
    { IMAGECLASS, ROOTCLASS, sizeof(struct Image), 0, ImageClass_Dispatcher },
    { GADGETCLASS, ROOTCLASS, sizeof(struct GadgetData), 0, GadgetClass_Dispatcher },
    { POINTERCLASS, ROOTCLASS, sizeof(struct PointerData), offsetof(struct IntIntuitionBase, pointerclass), PointerClass_Dispatcher },
    { WINDECORCLASS, ROOTCLASS, sizeof(struct windecor_data), 0, WinDecorClass_Dispatcher },
    { SCRDECORCLASS, ROOTCLASS, sizeof(struct scrdecor_data), 0, ScrDecorClass_Dispatcher },
    { MENUDECORCLASS, ROOTCLASS, sizeof(struct menudecor_data), 0, MenuDecorClass_Dispatcher },
    { NULL, ROOTCLASS, sizeof(struct IntScreen), offsetof(struct IntIntuitionBase, screenclass), ScreenClass_Dispatcher },
    { FRAMEICLASS, IMAGECLASS, sizeof(struct FrameIData), 0, FrameIClass_Dispatcher },
    { SYSICLASS, IMAGECLASS, sizeof(struct SysIData), 0, SysIClass_Dispatcher },
    { MENUBARLABELCLASS, IMAGECLASS, sizeof(struct MenuBarLabelData), 0, MenuBarLabelClass_Dispatcher },
    { BUTTONGCLASS, GADGETCLASS, 0, offsetof(struct IntIntuitionBase, windowsysiclass), ButtonGClass_Dispatcher },
    { PROPGCLASS, GADGETCLASS, sizeof(struct PropGData), 0, PropGClass_Dispatcher },
    { STRGCLASS, GADGETCLASS, sizeof(struct StrGData), 0, StrGClass_Dispatcher },
    { NULL, GADGETCLASS, sizeof(struct dragbar_data), offsetof(struct IntIntuitionBase, dragbarclass), DragBarClass_Dispatcher },
    { NULL, GADGETCLASS, sizeof(struct sizebutton_data), offsetof(struct IntIntuitionBase, sizebuttonclass), SizeButtonClass_Dispatcher },
    { FRBUTTONCLASS, BUTTONGCLASS, 0, 0, FrButtonClass_Dispatcher },
};

BOOL InitClasses(struct IntuitionBase *IntuitionBase)
{
    struct IClass *cl;
    WORD i;

    for (i = 0; i < sizeof(classtab) / sizeof(classtab[0]); i++)
    {
        cl = MakeClass(classtab[i].id, classtab[i].super, NULL, classtab[i].size, 0);
        if (!cl)
            return FALSE;
        cl->cl_Dispatcher.h_Entry = (ULONG (*)())classtab[i].dispatcher;
        cl->cl_UserData = (IPTR)IntuitionBase;
        if (classtab[i].field)
            *(struct IClass **)((UBYTE *)IntuitionBase + classtab[i].field) = cl;
        if (classtab[i].id)
            AddClass(cl);
    }
    return TRUE;
}
