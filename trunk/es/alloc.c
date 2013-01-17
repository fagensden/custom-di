/*

SNEEK - SD-NAND/ES emulation kit for Nintendo Wii

Copyright (C) 2009-2011  crediar

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

#define DEBUG_ALLOC_ERROR

void *malloc( u32 size )
{
	if(size <= 0)
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("ES:WARNING malloc: req size is 0!\n");
#endif
		return NULL;
	}
	void *ptr = heap_alloc( 0, size );
	if( ptr == NULL )
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("ES:malloc:%p Size:%08X FAILED\n", ptr, size);
#endif
		while(1);
	}
	return ptr;
}
void *malloca( u32 size, u32 align )
{
	if(size <= 0)
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("ES:WARNING malloca: req size is 0!\n");
#endif
		return NULL;
	}
	
	void *ptr = heap_alloc_aligned( 0, size, align );
	if( ptr == NULL )
	{
#ifdef DEBUG_ALLOC_ERROR
		dbgprintf("ES:malloca:%p Size:%08X Alignment:%d FAILED\n", ptr, size, align);
#endif
		while(1);
	}
	return ptr;
}
void free( void *ptr )
{
	if( ptr != NULL )
		heap_free( 0, ptr );

	//dbgprintf("Free:%p\n", ptr );

	return;
}