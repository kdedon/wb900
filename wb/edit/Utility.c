/**************************************************************
**** Utility.c: some useful functions, by T.Pierron        ****
**** Free software under GNU license, started on 17/2/2000 ****
**************************************************************/

#define  UTILITY_C
#include <intuition/intuitionbase.h>
#include <workbench/startup.h>
#include <utility/tagitem.h>
#include <dos/dos.h>
#include <exec/memory.h>
#include <exec/io.h>
#include "ClipLoc.h"
#include "Project.h"
#include "Gui.h"
#include "Utility.h"
#include "DiskIO.h"
#include "ProtoTypes.h"

#include "strings.h"

void SetTitle(struct Window *, STRPTR);

extern struct IntuitionBase *IntuitionBase;
extern ULONG  err_time;
static UBYTE  SPrintfBuf[256], *savea3;

/* Different End Of Line markers */
const UBYTE chEOL[] = {'\n', '\r', '\r', '\n'};
const UBYTE szEOL[] = {1, 1, 2};

/** SPrintf like routine **/
static void PutChProc(UBYTE data, APTR unused)
{
	if( savea3 < SPrintfBuf + sizeof(SPrintfBuf) - 1 )
		*savea3++ = data;
	else *savea3 = 0;
}

/** This is a very simplified routine, but takes only a few hundred bytes **/
STRPTR my_SPrintf(STRPTR fmt, RAWARG data)
{
	savea3 = SPrintfBuf;
	RawDoFmt(fmt, (RAWARG)data, (void *)PutChProc, 0);
	return SPrintfBuf;
}

/** Write column/line in top of window **/
void draw_info(Project p)
{
	UpdateTitle(Wnd, p);
}

CONST_STRPTR InfoTmpl = "%s%s    (%ld, %ld)";

/** Update window title **/
void UpdateTitle(struct Window *W, Project p)
{
	/* Its own buffer: messages in SPrintfBuf would replace it */
	static UBYTE Title[sizeof(SPrintfBuf)];
	struct { TEXT *name; TEXT *modified; ULONG x; ULONG y; } info;

	info.name = p->path? p->path: p->name;
	info.modified = (p->state & MODIFIED) ? STR_MODIF : "";
	info.x = p->nbrc+1; info.y = p->nbl+1;
	CopyMem(my_SPrintf(InfoTmpl, (RAWARG)&info), Title, sizeof(Title));

	SetTitle(W, Title);
}

/** Convert the Workbench or command line arguments into WBArgs **/
void ParseArgs(StartUpArgs *res, struct WBStartup *wbs, STRPTR *files)
{
	res->sa_Free   = 0;
	res->sa_NbArgs = 0;
	if( wbs ) {
		/* Program has been started from Workbench */
		res->sa_NbArgs = wbs->sm_NumArgs-1;
		res->sa_ArgLst = (APTR)(wbs->sm_ArgList+1);
	} else if( files && *files ) {
		/* From CLI */
		struct WBArg *new;
		ULONG nb;
		for(nb=0; files[nb]; nb++);
		if((new = (void *) AllocVec(sizeof(*new)*nb, MEMF_PUBLIC | MEMF_CLEAR)))
		{
			BPTR cwd = (BPTR) CurrentDir( NULL ); /* No need to UnLock so */
			res->sa_ArgLst = (APTR) new;
			res->sa_NbArgs = nb;
			res->sa_Free   = 1;
			for(; nb; new->wa_Name = *files++, new->wa_Lock = cwd, new++, nb--);
			CurrentDir( cwd );
		}
	}
}

/** Get the filename inside #include directive **/
STRPTR GetIncludeFile(Project prj, LINE * ln)
{
	STRPTR p = ln->stream;
	LONG   i = ln->size;

	while(i && TypeChar[ *p ] == SPACE) p++, i--;
	if( i > 0 && *p == '#' )
	{
		for(p++, i--; i && TypeChar[*p] == SPACE; p++, i--);
		if( i > 7 && 0 == strncmp(p, "include", 7) )
		{
			for(p+=7, i-=7; i && TypeChar[*p] == SPACE; p++, i--);
			if(i > 2)
			{
				extern UBYTE BufTxt[];
				STRPTR dest = BufTxt;
				UBYTE  end  = *p;
				if(*p == '<') strcpy(BufTxt, "INCLUDE:"), dest+=8, end = '>';
				else if(prj->path == NULL) BufTxt[0] = 0;
				else {
					CopyMem(prj->path, BufTxt, prj->name-prj->path);
					dest += prj->name-prj->path;
				}
				for(p++, i--; i && *p != end; *dest++ = *p++, i--);
				*dest=0;
				if(RETURN_OK == get_full_path(BufTxt, &dest))
					return dest;
			}
		}
	}
	return NULL;
}

/* Generic list used ONLY as pointer */
typedef	struct _list
{
	struct _list *next, *prev;
}	*list;

/*** Insert node Src after the node It ***/
void InsertAfter( list It,list Src )
{
	register list L, Lp;
	if(It)
	{
		Lp=It; L=Lp->next;
		Src->next = L; Src->prev = Lp;
		if( L  ) L->prev = Src;
		if( Lp ) Lp->next= Src;
	}	else
		Src->next = Src->prev = NULL;
}

/*** Remove a node from a list ***/
void Destroy( list *First, list p )
{
	if(p->next) p->next->prev = p->prev;
	if(p->prev) p->prev->next = p->next;
	else *First = p->next;
}

/*** Catenate two path part ***/
STRPTR CatPath(STRPTR dir, STRPTR file)
{
	STRPTR dst;
	UWORD  len;
	if( ( dst = (STRPTR) AllocVec(len = strlen(dir) + strlen(file) + 2, MEMF_PUBLIC) ) )
		strcpy(dst, dir), AddPart(dst, file, len);
	return dst;
}

/*** MemMove: copy overlapping chunk of mem ***/
void MemMove(UBYTE *Src, UWORD Offset, LONG sz)
{
	register UBYTE *src, *dst;
	for(src=Src+sz-1, dst=src+Offset; sz>0; sz--, *dst-- = *src--);
}

static UBYTE TabStop[256], tab=255;

/*** Pre-computes tabstop ***/
void init_tabstop(UBYTE ts)
{
	if(ts != tab) {
		int i;
		for(i=0, tab=ts; i<sizeof(TabStop); i++)
		{
			TabStop[i] = tab;
			if(tab==1) tab=ts; else tab--;
		}
		tab=ts;
	}
}

/*** Returns increment up to the next tabstop ***/
UBYTE tabstop(ULONG nb)
{
	/* Almost all tabulations are situated before the 256th character */
	if(nb<=sizeof(TabStop)) return TabStop[nb];
	else return (UBYTE)(tab - (nb % tab));
}

/*** Display an error message in the window's title ***/
void ThrowError(struct Window *W, STRPTR Msg)
{
	if( W ) {
		if(Msg[0] != 127) DisplayBeep(W->WScreen); else Msg++;

		/* If window is backdrop'ed, change screen's title instead of window */
		if(W->Flags & WFLG_BACKDROP) SetWindowTitles(W,(UBYTE *)-1,Msg);
		else SetWindowTitles(W,Msg,(UBYTE *)-1);

		err_time = 0;
		/* To be sure that message will disappear one day */
		ModifyIDCMP(W,W->IDCMPFlags | IDCMP_INTUITICKS);
	}	else PutStr(Msg), PutStr("\n");
}

/*** Show messages associated with IoErr() number ***/
void ThrowDOSError(struct Window *W, STRPTR Prefix)
{
	static UBYTE Message[100];

	/* Get standard DOS error message */
	Fault(IoErr(), Prefix, Message, sizeof(Message));

	ThrowError(W, Message);
}

/*** Set title of window/screen properly ***/
void SetTitle(struct Window *W, STRPTR Msg)
{
	/* If there is a pending msg, change hidden title */
	if( W->IDCMPFlags & IDCMP_INTUITICKS )
		ModifyIDCMP(W,W->IDCMPFlags & ~IDCMP_INTUITICKS);

	/* Modify visible title */
	if(W->Flags & WFLG_BACKDROP) SetWindowTitles(W,(UBYTE *)-1,Msg);
	else SetWindowTitles(W,Msg,(UBYTE *)-1);
	W->UserData = Msg;
}

/*** Reset the old title ***/
void StopError(struct Window *W)
{
	if(W->Flags & WFLG_BACKDROP) SetWindowTitles(W,(UBYTE *)-1,W->UserData);
	else SetWindowTitles(W,W->UserData,(UBYTE *)-1);

	/* INTUITICKS aren't required anymore */
	ModifyIDCMP(W,W->IDCMPFlags & ~IDCMP_INTUITICKS);
}

/** Getting standard busy pointer **/
ULONG IDCMPFlags;
struct TagItem busy_pointer_tags[] =
{
	{WA_BusyPointer,TRUE},
	{TAG_END,0}
};

/*** Shutdown window IDCMP port ***/
void BusyWindow(struct Window *W)
{
	if( W )
	{
		/* Store IDCMP flags and shutdown port */
		IDCMPFlags = W->IDCMPFlags;
		ModifyIDCMP(W,0);
		/* Change window's pointer (OS 3.0+ only) */
		if(IntuitionBase->LibNode.lib_Version >= 39)
			SetWindowPointerA(W,busy_pointer_tags);
	}
}

/*** Reset IDCMP port ***/
void WakeUp(struct Window *W)
{
	if( W )
		ModifyIDCMP(W,IDCMPFlags),
		ClearPointer(W);
}

/* Information window about current project */
struct EasyStruct request;

/*** Show information window ***/
void show_info(Project p)
{
	extern UBYTE WinTitle[];
	STRPTR file;
	ULONG  bytes;

	BusyWindow(Wnd);
	request.es_StructSize = sizeof(struct EasyStruct);
	CopyMem(MsgAbout, &request.es_Title, 3*sizeof(STRPTR));
	bytes = size_count(p->the_line, szEOL[ p->eol ]);
	split_path((AskArgs *)&p->path, NULL, &file);

	{
		IPTR args[6];
		args[0] = (IPTR)WinTitle; args[1] = (IPTR)file;
		args[2] = p->max_lines;   args[3] = (IPTR)MsgAbout[ p->max_lines!=1 ? 6:5 ];
		args[4] = bytes;          args[5] = (IPTR)MsgAbout[ bytes!=1 ? 4:3 ];
		EasyRequestArgs(Wnd,&request,NULL,args);
	}
	WakeUp(Wnd);
}

/*** Warn user that file has been modified ***/
char warn_modif(Project p)
{
	STRPTR file;
	if( p->state & MODIFIED )
	{
		request.es_StructSize   = sizeof(struct EasyStruct);
		request.es_Title        = MsgAbout[0];
		request.es_TextFormat   = ErrMsg(ERR_FILEMODIFIED);
		request.es_GadgetFormat = ErrMsg(ERR_SLC);
		split_path((AskArgs *)&p->path, NULL, &file);
		switch( EasyRequestArgs(Wnd,&request,NULL,&file) )
		{
			case 0:  return 0;
			case 1:  return save_project(p, FALSE, FALSE);
		}
	}
	/* User want to close this file */
	return 1;
}

/*** Warn user that he is going to overwrite a file ***/
char warn_overwrite( STRPTR path )
{
	APTR lock;
	if(NULL != (lock = (APTR) Lock( path, SHARED_LOCK )))
	{
		UnLock( (BPTR) lock );
		/* Fuck'n shit, the file exists */
		request.es_StructSize   = sizeof(struct EasyStruct);
		request.es_Title        = MsgAbout[0];
		request.es_TextFormat   = ErrMsg(ERR_FILEEXISTS);
		request.es_GadgetFormat = ErrMsg(ERR_OC);

		return (char) EasyRequestArgs(Wnd,&request,NULL,NULL);
	}
	return 1;
}

/*** Simple requester to ask user for a number ***/
int get_number( Project p, CONST_STRPTR title, LONG * result )
{
	struct Window *win;
	struct TagItem tags[9];
	static UBYTE  LineNum[10];
	static struct StringInfo SI = {LineNum,NULL,0,sizeof(LineNum),0,0,0,0,0,0,NULL,0,NULL};
	static struct Gadget StrGad = {
		NULL,0,0,0,0,GFLG_GADGHCOMP,GACT_IMMEDIATE | GACT_RELVERIFY | GACT_LONGINT | GACT_STRINGCENTER,
		GTYP_STRGADGET,NULL,NULL,NULL,0,(APTR) &SI,0,NULL
	};
	int ok = 0;

	/* Open our window */
	tags[0].ti_Tag = WA_Width;       tags[0].ti_Data = 160;
	tags[1].ti_Tag = WA_InnerHeight; tags[1].ti_Data = prefs.scrfont->tf_YSize+2;
	tags[2].ti_Tag = WA_Left;        tags[2].ti_Data = Wnd->LeftEdge + (Wnd->Width  - 160) / 2;
	tags[3].ti_Tag = WA_Top;         tags[3].ti_Data = Wnd->TopEdge  + (Wnd->Height - 30)  / 2;
	tags[4].ti_Tag = WA_Title;       tags[4].ti_Data = (IPTR) title;
	tags[5].ti_Tag = WA_IDCMP;       tags[5].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_GADGETUP;
	tags[6].ti_Tag = WA_Flags;       tags[6].ti_Data = WFLG_CLOSEGADGET  | WFLG_ACTIVATE | WFLG_RMBTRAP | WFLG_DRAGBAR;
	tags[7].ti_Tag = WA_PubScreen;   tags[7].ti_Data = (IPTR) Scr;
	tags[8].ti_Tag = TAG_DONE;
	if((win = OpenWindowTagList( NULL, tags )))
	{
		extern struct IntuiMessage msgbuf,*msg;

		BusyWindow(Wnd);
		/* Attach the simple OS1.3 compliant string gadget */
		*LineNum = 0;
		StrGad.Width    = 160 - win->BorderRight - (
		StrGad.LeftEdge = win->BorderLeft);
		StrGad.TopEdge  = win->BorderTop+1;
		StrGad.Height   = prefs.scrfont->tf_YSize;
		AddGList(win, &StrGad, 0, 1, NULL);
		ActivateGadget(&StrGad, win, NULL);

		/* Quickly collects events */
		for(;;) {

			WaitPort( win->UserPort );

			while((msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
			{
				CopyMemQuick(msg, &msgbuf, sizeof(msgbuf));
				ReplyMsg((struct Message *)msg);

				switch( msgbuf.Class )
				{
					case IDCMP_GADGETUP:    ok = LineNum[0] != 0;
					case IDCMP_CLOSEWINDOW: goto the_end;
				}
			}
		}
		/* Cleanup everything */
		the_end: WakeUp(Wnd);
		CloseWindow(win);
		*result = SI.LongInt;

	}	else ThrowError(Wnd, ErrMsg(ERR_NOMEM));
	return ok;
}

/*** The C library functions the editor uses ***/
ULONG strlen( CONST_STRPTR s )
{
	CONST_STRPTR p = s;

	while (*p) p++;
	return (ULONG)(p - s);
}

STRPTR strcpy( STRPTR d, CONST_STRPTR s )
{
	STRPTR p = d;

	while ((*p++ = *s++) != 0);
	return d;
}

STRPTR strcat( STRPTR d, CONST_STRPTR s )
{
	strcpy(d + strlen(d), s);
	return d;
}

int strncmp( CONST_STRPTR a, CONST_STRPTR b, ULONG n )
{
	for (; n && *a && *a == *b; n--, a++, b++);
	return n ? *a - *b : 0;
}

APTR memset( APTR d, int c, ULONG n )
{
	UBYTE *p = d;

	while (n--) *p++ = c;
	return d;
}
