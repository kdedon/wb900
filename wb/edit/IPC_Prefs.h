/***************************************************************
**** IPC_Prefs.h: Datatypes used to communicate with Jano   ****
**** Free software under GNU license, started on 11/11/2000 ****
**** Written by T.Pierron                                   ****
***************************************************************/

#ifndef	IPC_PREFS_H
#define	IPC_PREFS_H

#define	JANO_PORT			"JANO"

/** Packet used to communicate between process **/
struct JPacket
{
	struct Message Msg;				/* Exec's message */
	ULONG  class;						/* Class of message (see below) */
	union {
		StartUpArgs *args;			/* List of file to edit */
	}	msg;
};

/** Possible values of class field **/
#define	CMD_SHOW					(CMD_NONSTD+1)		/* CMD_NONSTD defined in exec/io.h */
#define	CMD_KILL					(CMD_NONSTD+2)
#define	CMD_NEWEDIT				(CMD_NONSTD+6)

ULONG create_port( void );				/* Create control port */
void  handle_port( void );				/* Process commands */
void  close_port( void );

#endif
