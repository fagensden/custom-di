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
	
	//a 2 seconds delay to avoid racing
	//udelay(2000000);
	
	DVDInit();

	//a 2 seconds delay to avoid racing
	//as es is waiting for the harddisk to become ready
	//this needs to be after the DVDInit();
	//which sets the flag for harddisk ready

	udelay(500000);

	DICfg = (DIConfig*)malloca( DVD_CONFIG_SIZE, 32 );


	DVDUpdateCache(0);
	s32 fres = DVDSelectGame( DICfg->SlotID );

//	s32 Timer = TimerCreate( 4000000, 0, QueueID, 0xDEADDEAD );
//	dbgprintf("CDI:Timer:%d\n", Timer );
	//TimerRestart( Timer, 0, 2000000 );

	while (1)
	{
		ret = MessageQueueReceive( QueueID, &IPCMessage, 0 );

/*
		if( (u32)IPCMessage == 0xDEADDEAD )
		{
			TimerStop( Timer );
			TimerDestroy( Timer );
			dbgprintf( "CDI:Delay Timer expired\n"); 
			DVDUpdateCache(0);
			dbgprintf("CDI:DI-Config: Region:%d Slot:%02d Games:%02d\n", DICfg->Region, DICfg->SlotID, DICfg->Gamecount );
			s32 fres = DVDSelectGame( DICfg->SlotID );
			continue;
		}
*/
		if( (u32)IPCMessage == 0xDEADDEAE )
		{
			TimerStop( switchtimer );
			TimerDestroy(switchtimer);
			dbgprintf( "CDI:switchtimer expired -> requested_game = %d\n",requested_game); 
			s32 fres = DVDSelectGame( requested_game );
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
