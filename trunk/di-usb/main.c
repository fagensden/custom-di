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
#include "gecko.h"
#include "alloc.h"
#include "dip.h"
#include "DIGlue.h"

extern DIConfig *DICfg;

s32 switchtimer;
int QueueID;
int requested_game;
u32 ignore_logfile;
char *cdiconfig ALIGNED(32);

s32 FS_Get_Di_Path( void )
{
	s32 FFSHandle = IOS_Open("/dev/fs", 0 );
	if( FFSHandle < 0 )
		return FFSHandle;

	u8 *dipath = (u8 *)malloca( 0x20, 0x20 );

	s32 r = IOS_Ioctl( FFSHandle, ISFS_GET_DI_PATH, NULL, 0, (void*)(dipath), 0x20 );
	IOS_Close( FFSHandle );
	
	if ( r == FS_SUCCESS )
	{
		memcpy( cdiconfig, dipath, 0x20 );
		cdiconfig[0x1f] = 0;
	}
	else
	{
		//for compatibility with previous
		strcpy( cdiconfig, "/sneek" );
	}
	strcat( cdiconfig,"/diconfig.bin" );
	free( dipath );

	return r;
}


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
	QueueID = MessageQueueCreate( QueueSpace, 8);

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
	ThreadSetPriority( 0, 0xFF );

	HeapInit();

	void *QueueSpace = malloc( 0x20 );
//	int QueueID = RegisterDevices( QueueSpace );
	QueueID = RegisterDevices( QueueSpace );

	if( QueueID < 0 )
	{
		ThreadCancel( 0, 0x77 );
	}
	
	s32 ret = EnableVideo( 1 );

	DVDInit();								 

	//a 0.5 seconds delay to avoid racing
	//as es is waiting for the harddisk to become ready
	//this needs to be after the DVDInit();
	//which sets the flag for harddisk ready

	udelay( 500000 );


	//basically
	ignore_logfile = 0;
	cdiconfig = (char *)( malloca( 0x40, 0x20 ) );

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
