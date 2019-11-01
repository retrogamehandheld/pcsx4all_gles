/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *   schultz.ryan@gmail.com, http://rschultz.ath.cx/code.php               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02111-1307 USA.            *
 ***************************************************************************/

#ifndef __PSXBIOS_H__
#define __PSXBIOS_H__

#include "psxcommon.h"
#include "r3000a.h"
#include "psxmem.h"
#include "misc.h"
#include "sio.h"

#ifdef PSXBIOS_LOG
extern char *biosA0n[256];
extern char *biosB0n[256];
extern char *biosC0n[256];
#endif

void psxBiosInit(void);
void psxBiosShutdown(void);
void psxBiosException(void);
void psxBiosFreeze(int Mode);

extern void (*biosA0[256])(void);
extern void (*biosB0[256])(void);
extern void (*biosC0[256])(void);

#endif /* __PSXBIOS_H__ */
