/*** strings.h: JanoEditor's messages, in English.  The numbers index
**** JanoMessages[] (ClipLoc.c) from ERR_BADOS; 0 is RETURN_OK.
***/

#ifndef	STRINGS_H
#define	STRINGS_H

#define	ERR_BADOS            1
#define	ERR_NOMEM            2
#define	ERR_NOGUI            3
#define	ERR_NONAME           4
#define	ERR_LOADFILE         5
#define	ERR_NOTXTINCLIP      6
#define	ERR_WRONG_TYPE       7
#define	ERR_NOBRACKET        8
#define	ERR_NOT_FOUND        9
#define	ERR_FILEMODIFIED     10
#define	ERR_SLC              11
#define	ERR_NOSEARCHTEXT     12
#define	ERR_FILEEXISTS       13
#define	ERR_OC               14
#define	WARN_RECORD          15
#define	WARN_RECORDED        16
#define	WARN_REC             17
#define	MSG_ABOUT            18
#define	MSG_SEARCHWINDOW     25
#define	ERR_TOOBIG           41
#define	ERR_NOASLREQ         42

/* A leading \177 shows the message without a beep */
#define	ERR_BADOS_STR        "Needs intuition, gadtools, keymap and utility V36!"
#define	ERR_NOMEM_STR        "Not enough memory!"
#define	ERR_NOGUI_STR        "Can't open the window!"
#define	ERR_NONAME_STR       "Unnamed"
#define	ERR_LOADFILE_STR     "Error while reading the file!"
#define	ERR_NOTXTINCLIP_STR  "Nothing to paste!"
#define	ERR_WRONG_TYPE_STR   "That is a directory, not a file!"
#define	ERR_NOBRACKET_STR    "No bracket under the cursor!"
#define	ERR_NOT_FOUND_STR    "'%s' not found!"
#define	ERR_FILEMODIFIED_STR "%s has been modified.\nSave the changes?"
#define	ERR_SLC_STR          "Save|Lose|Cancel"
#define	ERR_NOSEARCHTEXT_STR "Enter the text to find."
#define	ERR_FILEEXISTS_STR   "The file already exists.\nOverwrite it?"
#define	ERR_OC_STR           "Overwrite|Cancel"
#define	WARN_RECORD_STR      "\177Recording keystrokes..."
#define	WARN_RECORDED_STR    "\177Macro recorded."
#define	WARN_REC_STR         "REC"

#define	MSG_ABOUT_STR        "Information"
#define	MSG_FORMATINFO_STR   "%s\n\nFile: %s\n%ld %s, %ld %s"
#define	MSG_CONTINUE_STR     "Continue"
#define	MSG_BYTE_STR         "byte"
#define	MSG_BYTES_STR        "bytes"
#define	MSG_LINE_STR         "line"
#define	MSG_LINES_STR        "lines"

#define	MSG_SEARCHWINDOW_STR  "Find"
#define	MSG_REPLACEWINDOW_STR "Replace"
#define	MSG_SEARCHSTRING_STR  "_Find"
#define	MSG_REPLACESTRING_STR "Replace _with"
#define	MSG_OPTCASE_STR       "_Match case"
#define	MSG_OPTWORDS_STR      "Whole _words"
#define	MSG_BUTTONREPLACE_STR "_Replace"
#define	MSG_BUTTONREPALL_STR  "Replace _all"
#define	MSG_BUTTONSEARCH_STR  "_Next"
#define	MSG_NEXTSEARCH_STR    "_Previous"
#define	MSG_USESEARCH_STR     "_Use"
#define	MSG_CANCELSEARCH_STR  "_Cancel"
#define	MSG_REPLACEALL_STR    "\177%ld %s of '%s' replaced by '%s'"
#define	MSG_OCCURENCY_STR     "occurrence"
#define	MSG_OCCURENCIES_STR   "occurrences"
#define	ERR_TOOBIG_STR        "The file is too large!"
#define	ERR_NOASLREQ_STR      "Can't open the file requester!"

/* Menus */
#define	MSG_PROJECTTITLE_STR  "Project"
#define	MSG_NEWEMPTYFILE_STR  "New"
#define	MSG_OPENNEWFILE_STR   "Open..."
#define	MSG_LOADINPRJ_STR     "Open in this window..."
#define	MSG_SAVEFILE_STR      "Save"
#define	MSG_SAVEFILEAS_STR    "Save as..."
#define	MSG_SAVECHANGES_STR   "Save changes"
#define	MSG_INFORMATION_STR   "Information..."
#define	MSG_CLOSE_STR         "Close"
#define	MSG_QUIT_STR          "Quit"
#define	MSG_EDITTITLE_STR     "Edit"
#define	MSG_CUT_STR           "Cut"
#define	MSG_COPY_STR          "Copy"
#define	MSG_PASTE_STR         "Paste"
#define	MSG_MARK_STR          "Mark"
#define	MSG_MARKCOLUMN_STR    "Mark columns"
#define	MSG_SELECTALL_STR     "Select all"
#define	MSG_DELLINE_STR       "Delete line"
#define	MSG_SUBTOOLS_STR      "Tools"
#define	MSG_INDENT_STR        "Indent"
#define	MSG_UNINDENT_STR      "Unindent"
#define	MSG_UPPERCASE_STR     "Upper case"
#define	MSG_LOWERCASE_STR     "Lower case"
#define	MSG_TOGGLECASE_STR    "Toggle case"
#define	MSG_INSERTFILE_STR    "Insert file..."
#define	MSG_UNDO_STR          "Undo"
#define	MSG_REDO_STR          "Redo"
#define	MSG_SEARCHTITLE_STR   "Search"
#define	MSG_FIND_STR          "Find..."
#define	MSG_REPLACE_STR       "Replace..."
#define	MSG_SUBFIND_STR       "Again"
#define	MSG_NEXTFIND_STR      "Find next"
#define	MSG_PREVFIND_STR      "Find previous"
#define	MSG_NEXTREPLACE_STR   "Replace next"
#define	MSG_PREVPAGE_STR      "Previous page"
#define	MSG_NEXTPAGE_STR      "Next page"
#define	MSG_GOTOLINE_STR      "Go to line..."
#define	MSG_FINDBRACKET_STR   "Matching bracket"
#define	MSG_LASTMODIF_STR     "Last change"
#define	MSG_BEGOFLINE_STR     "Start of line"
#define	MSG_ENDOFLINE_STR     "End of line"
#define	MSG_MACROTITLE_STR    "Macro"
#define	MSG_STARTRECORD_STR   "Start recording"
#define	MSG_STOPRECORD_STR    "Stop recording"
#define	MSG_PLAYMACRO_STR     "Play"
#define	MSG_REPEATMACRO_STR   "Repeat..."

#endif
