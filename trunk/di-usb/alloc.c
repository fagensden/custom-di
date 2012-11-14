/*

SNEEK - SD-NAND/ES emulation kit for Nintendo Wii

Copyright (C) 2009-2011  crediar
			  2011-2012  OverjoY

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "alloc.h"
#include "vsprintf.h"

//#define DEBUG_ALLOC
#define DEBUG_ALLOC_ERROR

int HeapID;

void HeapInit(void)
{
	HeapID = HeapCreate((void*)0x13600000, 0x18000);
}

void *halloc(u32 size)
{	
	if(size <= 0)
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("DIP:WARNING halloc: req size is 0!\n");
#endif
		return NULL;
	}
		
	void *ptr = HeapAlloc(HeapID, size);
	if(ptr == NULL)
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("DIP:halloc:%p Size:%08X FAILED\n", ptr, size);
#endif
		while(1);
	}
#ifdef DEBUG_ALLOC
	else
	{
		dbgprintf("DIP:HALLOC:%p Size:%08X\n", ptr, size);
	}	
#endif
	return ptr;
}
void *halloca(u32 size, u32 align)
{	
	if(size <= 0)
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("DIP:WARNING halloca: req size is 0!\n");
#endif
		return NULL;
	}

	void *ptr = HeapAllocAligned(HeapID, size, align);	
	if(ptr == NULL)
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("DIP:halloca:%p Size:%08X Alignment:%d FAILED\n", ptr, size, align);
#endif
		while(1);
	}
#ifdef DEBUG_ALLOC
	else
	{
		dbgprintf("DIP:HALLOCA:%p Size:%08X Alignment:%d\n", ptr, size, align);
	}
#endif
	return ptr;
}
void hfree(void *ptr)
{
	if(ptr != NULL)
	{
#ifdef DEBUG_ALLOC
		dbgprintf("DIP:HFREE:%p\n", ptr );
#endif
		HeapFree(HeapID, ptr);
	}
		
	return;
}


void *malloc(u32 size)
{
	if(size <= 0)
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("DIP:WARNING malloc: req size is 0!\n");
#endif
		return NULL;
	}

	void *ptr = HeapAlloc(0, size);	
	if(ptr == NULL)
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("DIP:malloc:%p Size:%08X FAILED\n", ptr, size);
#endif
		while(1);
	}

	return ptr;
}
void *malloca(u32 size, u32 align)
{
	if(size <= 0)
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("DIP:WARNING malloca: req size is 0!\n");
#endif
		return NULL;
	}

	void *ptr = HeapAllocAligned(0, size, align);
	if( ptr == NULL )
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("DIP:malloca:%p Size:%08X Alignment:%d FAILED\n", ptr, size, align);
#endif
		while(1);
	}
		
	return ptr;
}
void free(void *ptr)
{
	if(ptr != NULL)
	{
		//dbgprintf("FREE:%p\n", ptr);
		HeapFree(0, ptr);
	}
	
	return;
}