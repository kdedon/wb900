/* EMUFLAGS: --ram=08-1F */
/*
 * Edit on files of 100 KB and 300 KB, over many buffers: a line near the
 * end edited, saved, undone and saved again.  The 300 KB file needs
 * 1.5 MB.
 */
#define BIG
#define NAME	"editbig"
#include "edit.c"
