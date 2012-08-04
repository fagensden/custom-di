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
 * sram.h
 *
 ***************************************************************************/

#ifndef _SRAM_H_
#define _SRAM_H_

#include "exi.h"

typedef struct _SysSRAM 
{
	u16 Checksum1;
	u16 Checksum2;
	u32 Ead0;
	u32 Ead1;
	u32 CounterBias;
	s8 DisplayOffsetH;
	u8 NTD;
	u8 Lang;
	u8 Flags;
	u8 FlashID[2][12];
	u32 WirelessKbdID;
	u16 WirelessPadID[4];
	u8 DVDErrCode;
	u8 Padding1;
	u16 FlashIDChecksum[2];
	u8 Padding2[2];
} SysSRAM;

SysSRAM *SYS_LockSram(void);
u32 SYS_UnlockSram(u32 write);
u32 SYS_SyncSram(void);
void SRAM_Init(void);

#endif