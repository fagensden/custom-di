/*

SNEEK - SD-NAND/ES + DI emulation kit for Nintendo Wii

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
#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
#include "ehci_types.h"
#include "ehci.h"
#include "gecko.h"
#include "alloc.h"
#include "dip.h"
#include "DIGlue.h"


extern DIConfig *DICfg;
extern char *RegionStr;

s32 switchtimer;
int QueueID;
int requested_game;
u32 ignore_logfile;
char* cdiconfigpath ALIGNED(32);
char* cdiconfig ALIGNED(32);
size_t slen;

static u8 *dipath ALIGNED(32);


s32 FS_Get_Di_Path( void )
{
	
	
	//dbgprintf("CDI:About to Get_Di_Path \n");

	s32 FFSHandle = IOS_Open("/dev/fs", 0 );
	if( FFSHandle < 0 )
		return FFSHandle;
	//u8 *dipath = (u8 *)HeapAlloc( 0, 0x20 );
	dipath = (u8 *)malloca( 0x20, 0x20 );

	s32 r = IOS_Ioctl( FFSHandle, ISFS_GET_DI_PATH, NULL, 0, (void*)(dipath), 0x20 );
	//s32 r = -1;
	IOS_Close( FFSHandle );
	
	if (r == FS_SUCCESS)
	{
		memcpy(cdiconfigpath,dipath,0x20);
		cdiconfigpath[0x1f] = 0;
	}
	else
	{
		//for compatibility with previous
		strcpy(cdiconfigpath,"/sneek");
	}
	//dbgprintf("CDI:Get_Di_Path = %s\n",cdiconfigpath);
	//strcpy(cdiconfigpath,"/sneek");

	strcpy(cdiconfig,cdiconfigpath);
	slen = strlen(cdiconfig);
	strcpy (cdiconfig+slen ,"/diconfig.bin");
	//HeapFree  (0,dipath);
	free  (dipath);

	//dbgprintf("CDI:Get_Di_Path = %s\n",cdiconfig);
	return r;
}

/*
s32 FS_Running( void )
{
	s32 fd = IOS_Open("/dev/fs", 0 );
	if( fd < 0 )
		return fd;

	s32 r = IOS_Ioctl( fd, ISFS_SUPPORT_SD_DI, NULL, 0, NULL, 0);
	
	IOS_Close( fd );

	return r;
}
*/

void udelay(int us)
{
	u8 heap[0x10];
	struct ipcmessage *Message;
	int mqueue = -1;
	int TimeID = -1;

	mqueue = MessageQueueCreate(heap, 1);
	if(mqueue < 0)
		goto out;
	TimeID = TimerCreate(us, 0, mqueue, 0xbabababa);
	if(TimeID < 0)
		goto out;
	MessageQueueReceive(mqueue, &Message, 0);
	
out:
	if(TimeID > 0)
		TimerDestroy(TimeID);
	if(mqueue > 0)
		MessageQueueDestroy(mqueue);
}

s32 RegisterDevices( void *QueueSpace )
{
	int QueueID = MessageQueueCreate( QueueSpace, 8);

	s32 ret = IOS_Register("/dev/di", QueueID );

	if( ret < 0 )
		return ret;

	return QueueID;
}
char __aeabi_unwind_cpp_pr0[0];

void _main(void)
{
	struct ipcmessage *IPCMessage = NULL;

	//we can only start logging if FS is running
	//so we will disable it until so
	ignore_logfile = 1;
	//dbgprintf("CDI: main starting...\n");
	ThreadSetPriority( 0, 0xF4 );

	HeapInit();

	void *QueueSpace = halloc( 0x20 );
//	int QueueID = RegisterDevices( QueueSpace );
	QueueID = RegisterDevices( QueueSpace );

	if( QueueID < 0 )
	{
		ThreadCancel( 0, 0x77 );
	}
	
	s32 ret = EnableVideo(1);

	DVDInit();								 
	
	//a 2 seconds delay to avoid racing
	//udelay(2000000);
/*
	s32 tries = 0;

	s32 runresult;
	runresult = FS_Running();
	while((runresult!=FS_SUCCESS)&&(tries < 20)) 
	{
		//dbgprintf("CDI:Init FS runresult = %d\n",runresult);
		udelay(1000000);
		tries++;
		runresult = FS_Running();
	}
	if (tries == 20)
	{
		dbgprintf("CDI:FS-USB init failure...?\n");
	}
*/
//	else
//	{
//		dbgprintf("CDI:Init FS Succeeded after %d\n",tries+1);
//	}

//  SNEEK problem
//	DVDInit();

	//a 0.5 seconds delay to avoid racing
	//as es is waiting for the harddisk to become ready
	//this needs to be after the DVDInit();
	//which sets the flag for harddisk ready

	udelay(500000);


	//basically
	ignore_logfile = 0;
	cdiconfig = (char*)(malloca( 0x40, 0x20));
	cdiconfigpath = (char*)(malloca( 0x20, 0x20));

	FS_Get_Di_Path();

	DICfg = (DIConfig*)malloca( DVD_CONFIG_SIZE, 32 );

//	dbgprintf("CDI:before DVDUpdateCache\n"); 
	DVDUpdateCache(0);

	DVDSelectGame( DICfg->SlotID );

	while (1)
	{
		ret = MessageQueueReceive( QueueID, &IPCMessage, 0 );

		if( (u32)IPCMessage == 0xDEADDEAE )
		{
			TimerStop( switchtimer );
			TimerDestroy(switchtimer);
			//dbgprintf( "CDI:switchtimer expired -> requested_game = %d\n", requested_game ); 
			DVDSelectGame( requested_game );
			continue;
		}

		switch( IPCMessage->command )
		{
			case IOS_OPEN:
			{
				if( strncmp("/dev/di", IPCMessage->open.device, 8 ) == 0 )
				{
					MessageQueueAck( IPCMessage, 24 );
				} else {
					MessageQueueAck( IPCMessage, -6 );
				}
				
			} break;

			case IOS_CLOSE:
			{
				MessageQueueAck( IPCMessage, 0 );
			} break;

			case IOS_IOCTL:
				MessageQueueAck( IPCMessage, DIP_Ioctl( IPCMessage ) );
			break;

			case IOS_IOCTLV:
				MessageQueueAck( IPCMessage, DIP_Ioctlv( IPCMessage ) );
				break;

			case IOS_READ:
			case IOS_WRITE:
			case IOS_SEEK:
			default:
				MessageQueueAck( IPCMessage, -4 );
				break;
		}
	}
}
