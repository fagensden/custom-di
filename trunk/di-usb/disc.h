/**************************************************************************************
***																					***
*** neek2o - disc.h																	***
***																					***
*** Copyright (C) 2012 OverjoY														***
*** 																				***
*** This program is free software; you can redistribute it and/or					***
*** modify it under the terms of the GNU General Public License						***
*** as published by the Free Software Foundation version 2.							***
***																					***
*** This program is distributed in the hope that it will be useful,					***
*** but WITHOUT ANY WARRANTY; without even the implied warranty of					***
*** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the					***
*** GNU General Public License for more details.									***
***																					***
*** You should have received a copy of the GNU General Public License				***
*** along with this program; if not, write to the Free Software						***
*** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. ***
***																					***
**************************************************************************************/
#ifndef __DISC_STRUCT_H__
#define __DISC_STRUCT_H__

#include "syscalls.h"
#include "dip.h"

#define	DI			0x0D806000

#define	DISR		(*(vu32*)(DI+0x00))
#define	DICVR		(*(vu32*)(DI+0x04))
#define	DICMDBUF0	(*(vu32*)(DI+0x08))
#define	DICMDBUF1	(*(vu32*)(DI+0x0C))
#define	DICMDBUF2	(*(vu32*)(DI+0x10))
#define	DIMAR		(*(vu32*)(DI+0x14))
#define	DILENGTH	(*(vu32*)(DI+0x18))
#define	DICTRL		(*(vu32*)(DI+0x1C))
#define	DIMMBUF		(*(vu32*)(DI+0x20))
#define	DICFG		(*(vu32*)(DI+0x24))

#define		ORIGINAL		0
#define		BACKUP			1

#define		DBBLOCKSIZE		0x8000

s32 DI_ReadDiskId(void);
void DI_Reset(void);
s32 DiscRead(void *ptr, u64 offset, u32 length);


#endif