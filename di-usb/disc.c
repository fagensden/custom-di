/**************************************************************************************
***																					***
*** neek2o - disc.c																	***
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
#include "disc.h"
#include "hollywood.h"


extern u32 DICover ALIGNED(32);
extern u32 ChangeDisc ALIGNED(32);
extern u32 FMode;
extern DIConfig *DICfg; 

u8 *DiscBuffer	= (u8 *)NULL;
u32 InitDB = 0;
u32 Error = 0;
u32 DiscIsBackup = 0;

void DI_Reset(void)
{		
	*(vu32*)HW_RESETS &= 0xFFFDFBFF;
	*(vu32*)HW_RESETS |= 0x00020400;
}

s32 DI_Read(void *ptr, u64 offset, u32 length)
{
	DISR = 0x2E;
	DICMDBUF0 = DiscIsBackup ? 0xD0000000 : 0xA8000000;
	DICMDBUF1 = DiscIsBackup ? (u32)(offset >> 11) : (u32)(offset >> 2);
	DICMDBUF2 = DiscIsBackup ? length >> 11 : length;
	DILENGTH = length;
	DIMAR = (u32)ptr;
	DIMMBUF	= 0;

	sync_before_read(ptr, length);

	DICTRL = 3;

	while(1)
	{
		if(DISR & 0x4)
			return 1;
		if(!DILENGTH)
			return 0;
	}

	return 0;
}

s32 DI_ReadDiskId(void)
{
	DISR = 0x2E;
	DICMDBUF0 = 0xA8000040;
	DICMDBUF1 = 0;
	DICMDBUF2 = 0x20;
	DILENGTH = 0x20;
	DIMAR = 0;
	
	sync_before_read(0, 0x20);
	
	DICTRL = 3;

	while (1)
	{
		if(DISR & (1<<2))
			return 1;
		if(DISR & (1<<4))
			return 0;
	}

	return 0;
}

s32 DI_RequestError(void)
{
	DISR = 0x2E;
	DICMDBUF0 = 0xE0000000;
	DIMMBUF	= 0;
	DICTRL = 1;

	while(DICTRL & 1);

	return DIMMBUF;
}

s32 DiscRead(void *ptr, u64 offset, u32 length)
{
	if(!InitDB)
	{
		DiscBuffer = (u8 *)halloca(DBBLOCKSIZE, 32);
		memset32(DiscBuffer, 0, DBBLOCKSIZE);
		InitDB = 1;
	}	
	
	u32 i;
	s32 ret;	
	
	for(i = 0; i < DISCRETRY_MAX; ++i)
	{
		Error = 0;
		ret = DI_Read(DiscBuffer, offset, DBBLOCKSIZE);
		if(ret == DISC_SUCCES)
			break;

		Error = DI_RequestError();
		if(Error == 0x30200 || Error == 0x30201 || Error == 0x31100)
		{
			if(!(DICfg->Config & CONFIG_READ_ERROR_RETRY))
			{
				memset32(ptr, 0, length);
				return DI_FATAL;
			}
			dbgprintf("DIP:Failed to read from disc (Error: %X) (Attempt: %d/%d)\n", Error, i + 1, DISCRETRY_MAX);			
			continue;
		}			
		else if(Error == 0x53000 && Get_DVDVideo())
		{
			dbgprintf("DIP:DVDR detected... Switching to backup mode\n");
			Set_DVDVideo(0);
			DiscIsBackup = 1;
			continue;
		}
		else
		{
			dbgprintf("DIP:DI_Read(0x%p, 0x%x%08x, %d): %d\n", ptr, offset >> 32, (u32)offset, length);
			dbgprintf("DIP:Error: %X\n", Error);
			DICover |= 1;
			ChangeDisc = 0;
			return DI_FATAL;
		}
	}
	
	if(Error)
	{
		dbgprintf("DIP:Lost one block of data\n");
		memset32(ptr, 0, length);
		if(DICfg->Config & CONFIG_GAME_ERROR_SKIP)
			return DI_SUCCESS;
		else 
			return DI_FATAL;
	}
	
	u32 sector_offset = 0;
	if(DiscIsBackup)
		sector_offset = offset % 0x8000;
		
	memcpy(ptr, DiscBuffer + sector_offset, length);
	return DI_SUCCESS;
}
