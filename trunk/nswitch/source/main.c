/**************************************************************************************
***																					***
*** nswitch - Simple neek/realnand switcher to embed in a channel					***
***																					***
*** Copyright (C) 2011-2013  OverjoY												***
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

#include <gccore.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

#include "armboot.h"

#define le32(i) (((((u32) i) & 0xFF) << 24) | ((((u32) i) & 0xFF00) << 8) | \
                ((((u32) i) & 0xFF0000) >> 8) | ((((u32) i) & 0xFF000000) >> 24))

typedef struct _PR 
{
    u8 state;                            
    u8 chs_st[3];                        
    u8 type;                              
    u8 chs_e[3];                         
    u32 lba;                         
    u32 bc;                       
} __attribute__((__packed__)) _pr;

typedef struct _MBR
{
    u8 ca[446];               
    _pr part[4]; 
    u16 sig;                       
} __attribute__((__packed__)) _mbr;

int main() 
{
	s32 ESHandle = IOS_Open("/dev/es", 0);
	bool neek = IOS_Ioctlv(ESHandle, 0xA2, 0, 0, NULL) == 0x666c6f77;
	IOS_Close(ESHandle);
	if(!neek)
	{	
		bool KernelFound = false;
		FILE *f = NULL;
		int retry = 0;
		
		while(retry < 10)
		{
			if(__io_usbstorage.startup() && __io_usbstorage.isInserted())
				break;
					
			retry++;
			usleep(150000);				
		}
		
		if(retry < 10)
		{	
			_mbr mbr;
			char buffer[4096];
			
			__io_usbstorage.readSectors(0, 1, &mbr);
			
			if(mbr.part[1].type != 0)
			{				
				__io_usbstorage.readSectors(le32(mbr.part[1].lba), 1, buffer);
				
				if(*((u16*)(buffer + 0x1FE)) == 0x55AA)
				{
					if(memcmp(buffer + 0x36, "FAT", 3) == 0 || memcmp(buffer + 0x52, "FAT", 3) == 0)
					{
						fatMount("usb", &__io_usbstorage, le32(mbr.part[1].lba), 8, 64);
						f = fopen("usb:/sneek/kernel.bin", "rb");
					}
				}
			}
		}	
		
		if(!f)
		{
			if(__io_wiisd.startup() || !__io_wiisd.isInserted())
				if(fatMount("sd", &__io_wiisd, 0, 8, 64))
					f = fopen("sd:/sneek/kernel.bin", "rb");
		}			
	
		if(f) 
		{
			fseek(f , 0 , SEEK_END);
			long fsize = ftell(f);
			rewind(f);
			fread((void *)0x91000000, 1, fsize, f);
			DCFlushRange((void *)0x91000000, fsize);
			KernelFound = true;
		}

		fclose(f);
		fatUnmount("sd:");
		fatUnmount("usb:");		
		__io_usbstorage.shutdown();
		__io_wiisd.shutdown();
		
		if(!KernelFound)
		{
			SYS_ResetSystem( SYS_RETURNTOMENU, 0, 0 );
			return 0;
		}
	
		/*** Boot mini from mem code by giantpune ***/
		void *mini = memalign(32, armboot_size);  
		if(!mini) 
			return 0;    
  
		memcpy(mini, armboot, armboot_size);  
		DCFlushRange(mini, armboot_size);		
   
		*(u32*)0xc150f000 = 0x424d454d;  
		asm volatile("eieio");  
  
		*(u32*)0xc150f004 = MEM_VIRTUAL_TO_PHYSICAL(mini);  
		asm volatile("eieio");
  
		IOS_ReloadIOS(0xfe);   
  
		free(mini);

		return 0;
	}
	else
	{
		SYS_ResetSystem(SYS_RESTART, 0, 0);		
		return 0;
	}
}
