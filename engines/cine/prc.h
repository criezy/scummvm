/* ScummVM - Scumm Interpreter
 * Copyright (C) 2006 The ScummVM project
 *
 * cinE Engine is (C) 2004-2005 by CinE Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

#ifndef CINE_PRC_H_
#define CINE_PRC_H_

struct prcLinkedListStruct {
	struct prcLinkedListStruct *next;
	s16 stack[SCRIPT_STACK_SIZE];
	s16 localVars[50];
	u16 compareResult;
	u16 scriptPosition;
	byte *scriptPtr;
	s16 scriptIdx;
};

typedef struct prcLinkedListStruct prcLinkedListStruct;

extern prcLinkedListStruct globalScriptsHead;
extern prcLinkedListStruct objScriptList;

void resetglobalScriptsHead(void);
void freePrcLinkedList(void);
void loadPrc(char *pPrcName);

#endif
