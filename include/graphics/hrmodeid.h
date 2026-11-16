/* SPDX-License-Identifier: MIT */
/*
 * ModeIDs of the C900 displays, in AROS's RTG scheme: card number in bits
 * 31-16 (from 0x0010, above the Amiga chipset monitors), sync index in
 * bits 15-8, pixel format index in bits 7-0.
 */
#ifndef GRAPHICS_HRMODEID_H
#define GRAPHICS_HRMODEID_H

/* The HR card: one mode at a time, set by its straps */
#define HR_MONITOR_ID		0x00100000L
#define HRMONO_800_ID		(HR_MONITOR_ID | 0x0000)	/* 1024x800x1 */
#define HRMONO_1024_ID		(HR_MONITOR_ID | 0x0100)	/* 1024x1024x1 */
#define HRCOLOR_ID		(HR_MONITOR_ID | 0x0201)	/* 640x400x4 RGBI */

/* The LC card: modes its driver programs, 1 bit deep, 60 Hz or 50 Hz */
#define LC_MONITOR_ID		0x00110000L
#define LCHIRES_ID		(LC_MONITOR_ID | 0x0000)	/* 640x200 */
#define LCLORES_ID		(LC_MONITOR_ID | 0x0100)	/* 320x200 */
#define LCHIRESPAL_ID		(LC_MONITOR_ID | 0x0200)	/* 640x256 */
#define LCLORESPAL_ID		(LC_MONITOR_ID | 0x0300)	/* 320x256 */

#endif
