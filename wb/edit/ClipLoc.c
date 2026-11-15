/**********************************************************
**                                                       **
**      $VER: ClipLoc.c 1.2 (10 dec 1999)                **
**      Messages and clipboard support.                  **
**                                                       **
**      © T.Pierron, C.Guilaume. Free software under     **
**      terms of GNU public license.                     **
**                                                       **
**********************************************************/

#include <libraries/gadtools.h>
#include <exec/memory.h>
#include <exec/io.h>
#include "Memory.h"
#include "ClipLoc.h"
#include "Version.h"
#include "Cursor.h"
#include "Utility.h"
#include "ProtoTypes.h"

#include "strings.h"

/** The clipboard: the text last cut or copied, in memory **/
static STRPTR clip = NULL;
static ULONG  cliplen;

/** All messages of JanoEditor **/
STRPTR JanoMessages[] = {

	/* Global error messages */
	ERR_BADOS_STR,         ERR_NOMEM_STR,
	ERR_NOGUI_STR,         ERR_NONAME_STR,
	ERR_LOADFILE_STR,      ERR_NOTXTINCLIP_STR,
	ERR_WRONG_TYPE_STR,    ERR_NOBRACKET_STR,
	ERR_NOT_FOUND_STR,     ERR_FILEMODIFIED_STR,
	ERR_SLC_STR,           ERR_NOSEARCHTEXT_STR,
	ERR_FILEEXISTS_STR,    ERR_OC_STR,
	WARN_RECORD_STR,       WARN_RECORDED_STR,
	WARN_REC_STR,

	/* About messages */
	MSG_ABOUT_STR,         MSG_FORMATINFO_STR,
	MSG_CONTINUE_STR,      MSG_BYTE_STR,
	MSG_BYTES_STR,         MSG_LINE_STR,
	MSG_LINES_STR,

	/* Search window messages */
	MSG_SEARCHWINDOW_STR,  MSG_REPLACEWINDOW_STR, MSG_SEARCHSTRING_STR,
	MSG_REPLACESTRING_STR, MSG_OPTCASE_STR,       MSG_OPTWORDS_STR,
	MSG_BUTTONREPLACE_STR, MSG_BUTTONREPALL_STR,  MSG_BUTTONSEARCH_STR,
	MSG_NEXTSEARCH_STR,    MSG_USESEARCH_STR,     MSG_CANCELSEARCH_STR, NULL,
	MSG_REPLACEALL_STR,    MSG_OCCURENCY_STR,     MSG_OCCURENCIES_STR,
	ERR_TOOBIG_STR,        ERR_NOASLREQ_STR
};

/*** Free the clipboard ***/
void CBClose( void )
{
	if( clip ) FreeVec( clip ), clip = NULL;
}

/*** Copy the selected lines to the clipboard ***/
BOOL CBWriteFTXT( LINE *stsel, struct cutcopypast *ccp )
{
	LINE  *ln;
	long   length;
	STRPTR dst;

	/* Compute how many chars are selected */
	for(length=0, ln=stsel; ln && ln->flags; ln=ln->next)
	{
		length += ln->size;
		/* In block selection mode, always add a newline */
		if(ccp->select == COLUMN_TYPE) length++;
		if(ln->flags & FIRSTSEL) length -= find_nbc(ln, ccp->startsel);
		if(ln->flags & LASTSEL)  length += find_nbc(ln, ccp->endsel)-ln->size;
		else length++; /* New line */
	}
	if(length == 0) return FALSE;

	if( NULL == (dst = (STRPTR) AllocVec(length, MEMF_PUBLIC)) )
	{
		ThrowError(Wnd, ErrMsg(ERR_NOMEM));
		return FALSE;
	}
	CBClose();
	clip = dst; cliplen = length;

	{
		/* In columnar selection, use '\r' as eol */
		char eol = (ccp->select == COLUMN_TYPE ? '\r':'\n'), add_eol;

		/* Write the content of strings */
		for(ln = stsel; ln && ln->flags; ln = ln->next)
		{
			STRPTR stream;
			ULONG  length;
			stream = (STRPTR)ln->stream;
			length = ln->size;
			if(ln->flags & FIRSTSEL) {
				register long rc = find_nbc(ln, ccp->startsel);
				length -= rc; stream += rc;
			}
			if(ln->flags & LASTSEL) {
				length += find_nbc(ln, ccp->endsel) - ln->size;
				/* Add automatically a newline */
				add_eol = (ccp->select == COLUMN_TYPE);
			}
			else add_eol = 1;

			CopyMem(stream, dst, length); dst += length;
			if(add_eol) *dst++ = eol;
		}
	}
	return TRUE;
}

/*** Insert the clipboard's text ***/
BOOL CBReadCHRS( void *jbuf, LINE *st, ULONG pos, LONG *nbl )
{
	UBYTE eol;

	if( clip == NULL )
	{
		ThrowError(Wnd, ErrMsg(ERR_NOTXTINCLIP));
		return FALSE;
	}

	/* What's kind of paste method shall we used? */
	{	register STRPTR s; register ULONG n;
		for(s=clip,n=cliplen; --n && *s!='\n' && *s!='\r'; s++);
		eol = *s;
	}
	/* Add string to the buffer */
	reg_group_by(jbuf);
	if(eol == '\r') add_block (jbuf,st,pos,clip,cliplen,nbl) ;
	else            add_string(jbuf,st,pos,clip,cliplen,nbl) ;
	reg_group_by(jbuf);
	return TRUE;
}
