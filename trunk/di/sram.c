/***************************************************************************
 * Copyright (C) 2012 by OverjoY
 *           
 * Rewritten code from libogc
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * SRAM handling functions for neek2o
 *
 * sram.c
 *
 ***************************************************************************/

#include "sram.h"
#include "vsprintf.h"

struct _sramst
{
	u8 Buffer[64];
	u32 Offset;
	s32 Enabled;
	s32 Locked;
	s32 Sync;
} sramst;

void __BuildChecksum(u16 *buffer, u16 *c1, u16 *c2) 
{ 
	u32 i;
	*c1 = 0; 
	*c2 = 0; 
	for(i = 0; i < 4; ++i) 
	{ 
		*c1 += buffer[6 + i]; 
		*c2 += (buffer[6 + i] ^ -1); 
	}
}

static u32 __SRAM_Read(void *buffer)
{
	u32 command = 0x20000100;

	EXI_Select(0, 1, 3); 
	EXI_Imm(0, &command, 4, EXI_WRITE, 0);
	EXI_Sync(0);
	EXI_ImmEx(0, buffer, 64, EXI_READ);
	EXI_Sync(0);
	EXI_Deselect(0);
	
	return 0;
}

static u32 __SRAM_Write(void *buffer, u32 loc, u32 length)
{
	u32 command = 0xA0000100 + (loc << 6);

	EXI_Select(0, 1, 3);
	EXI_Imm(0, &command, 4, EXI_WRITE, 0);
	EXI_Sync(0);
	EXI_ImmEx(0, buffer, 64, EXI_WRITE);
	EXI_Sync(0);
	EXI_Deselect(0);		

	return 0;
}

static s32 __SRAM_Sync()
{
	return sramst.Sync;
}

static void* __LockSRAM(u32 loc)
{
	if(!sramst.Locked) 
	{
		sramst.Enabled = 1;
		sramst.Locked = 1;
		return (void *)((u32)sramst.Buffer+loc);
	}
	
	return NULL;
}

static u32 __UnlockSRAM(u32 write, u32 loc)
{
	SysSRAM *SRAM = (SysSRAM*)sramst.Buffer;

	if(write) 
	{
		if(!loc) 
		{
			if((SRAM->Flags & 0x03) > 0x02) 
				SRAM->Flags = (SRAM->Flags &~ 0x03);
			
			__BuildChecksum((u16*)sramst.Buffer, &SRAM->Checksum1, &SRAM->Checksum2);
		}
		if(loc < sramst.Offset) 
			sramst.Offset = loc;

		sramst.Sync = __SRAM_Write(sramst.Buffer + sramst.Offset, sramst.Offset, (64-sramst.Offset));
		if(sramst.Sync) 
			sramst.Offset = 64;
	}
	sramst.Enabled = 0;
	sramst.Locked = 0;
		
	return sramst.Sync;
}

SysSRAM *SYS_LockSram(void)
{
	return (SysSRAM*)__LockSRAM(0);
}

u32 SYS_UnlockSram(u32 write)
{
	return __UnlockSRAM(write, 0);
}

u32 SYS_SyncSram(void)
{
	__SRAM_Sync();
	return 1;
}

void SRAM_Init(void)
{
	sramst.Enabled = 0;
	sramst.Locked = 0;
	sramst.Sync = __SRAM_Read(sramst.Buffer);
	sramst.Offset = 64;
}
