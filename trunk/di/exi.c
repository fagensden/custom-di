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
 * EXI handling functions for neek2o
 *
 * exi.c
 *
 ***************************************************************************/

#include "exi.h"
#include "hollywood.h"

static void* PAddr;
static int PLen;

void EXI_Sync(int chan)
{
	vulong *p = (vulong *)EXI_REG_BASE;
	while (p[chan * 5 + 3] & 1);

	if(PAddr)
	{       
		int i;
		ulong v;
		v = p[chan * 5 + 4];
		for(i = 0; i < PLen; ++i)
			((u8*)PAddr)[i] = (v >> ((3 - i) * 8)) & 0xFF;
	}
}

s32 EXI_Imm(int chan, void *buffer, int length, int mode, int cb)
{
	vulong *p = (vulong *)EXI_REG_BASE;
	if(mode == EXI_WRITE)
	{
		p[chan * 5 + 4] = *(u32*)buffer;
		PAddr = 0;
		PLen = 0;
	}
	else
	{
		PAddr = buffer;
		PLen = length;
	}

	p[chan * 5 + 3] = ((length - 1) << 4) | (mode << 2) | 1;
	
	return 0;
}

s32 EXI_ImmEx(int chan, void *buffer, int length, int mode)
{
	u8 *v = (u8*)buffer;
	int blength;
	while(length)
	{
		blength = 4;
		if(blength >= length)
			blength = length;
			
		EXI_Imm(chan, v, blength, mode, 0);
		EXI_Sync(chan);
		length -= blength;
		v += blength;
	}
	
	if(length == 0)
		return 0;
		
	return 1;
}

s32 EXI_Select(int chan, int dev, int freq)
{
	vulong *p = (vulong*)EXI_REG_BASE;
	long v = p[chan * 5];
	v &= 0x405;
	v |= ((1 << dev) << 7) | (freq << 4);
	p[chan * 5] = v;
	
	if(v)
		return 0;
		
	return 1;
}

s32 EXI_Deselect(int chan)
{
	vulong *p = (vulong *)EXI_REG_BASE;
	p[chan * 5] &= 0x405;
	
	return 0;
}