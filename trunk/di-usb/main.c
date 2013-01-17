/*
SNEEK - SD-NAND/ES + DI emulation kit for Nintendo Wii

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
#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
#include "alloc.h"
#include "dip.h"
#include "disc.h"
#include "DIGlue.h"
#include "common.h"

s32 switchtimer;
int QueueID;
int requested_game;
int CVR;
u32 ignore_logfile;
char *cdiconfig ALIGNED(32);
static char DIFolder[40];

extern u32 DICover ALIGNED(32);
extern u32 ChangeDisc ALIGNED(32);
extern DIConfig *DICfg;

s32 RegisterDevices(void *QueueSpace)
{
	QueueID = MessageQueueCreate(QueueSpace, 8);

	s32 ret = IOS_Register("/dev/di", QueueID);

	if(ret < 0)
		return ret;

	return QueueID;
}

void _main(void)
{
	s32 ret = DI_FATAL;
	struct ipcmessage *IPCMessage = NULL;
	ignore_logfile = 1;
	ThreadSetPriority(0, 0xFF);

	HeapInit();

	void *QueueSpace = malloc(0x20);
	QueueID = RegisterDevices(QueueSpace);

	if(QueueID < 0)
		ThreadCancel(0, 0x77);
	
	Set_DVDVideo(1);
	DVDInit();
	udelay(1200000);

	ignore_logfile = 0;
	cdiconfig = (char *)(malloca(0x40, 0x20));

	ISFS_GetDIPath(DIFolder);	
	sprintf(cdiconfig, "%s/%s", DIFolder, CACHEFILE);

	DICfg = (DIConfig*)malloca(DVD_CONFIG_SIZE, 32);

	DVDUpdateCache(0);

	if(DICfg->Config & CONFIG_MOUNT_DISC)
	{
		if(!DICVR)
		{
			DI_Reset();			
			udelay(500000);
		}
			
		if(DICVR & 1)
		{
			DVDSelectGame(DICfg->SlotID);
		}
		else if(DICVR & 4)
		{
			if(*(vu32*)0 == 0)
				DI_ReadDiskId();
				
			DVDSelectGame(9999);
		}
	}
	else
	{
		DVDSelectGame(DICfg->SlotID);
	}
	
	CVR = DICVR;

	while(1)
	{			
		if(CVR != DICVR)
		{
			if(DICVR & 1)
			{
				ChangeDisc = 1;
				DICover |= 1;				
			}
			else if(DICVR & 4)
			{
				DI_Reset();			
				udelay(1000000);
				DI_ReadDiskId();				
				DVDSelectGame(9999);
			}	
			CVR = DICVR;
		}			
		
		ret = MessageQueueReceive( QueueID, &IPCMessage, 0 );

		if( (u32)IPCMessage == 0xDEADDEAE )
		{
			TimerStop(switchtimer);
			TimerDestroy(switchtimer);
			if(requested_game == 9999)
			{				
				udelay(500000);
				if(*(vu32*)0 == 0)
					DI_ReadDiskId();				
			}
			DVDSelectGame(requested_game);
			continue;
		}

		switch(IPCMessage->command)
		{
			case IOS_OPEN:
			{
				if(strncmp("/dev/di", IPCMessage->open.device, 8) == 0)
					MessageQueueAck(IPCMessage, 24);
				else
					MessageQueueAck(IPCMessage, -6);				
			} break;

			case IOS_CLOSE:
			{
				MessageQueueAck(IPCMessage, 0);
			} break;

			case IOS_IOCTL:
				MessageQueueAck(IPCMessage, DIP_Ioctl(IPCMessage));
			break;

			case IOS_IOCTLV:
				MessageQueueAck(IPCMessage, DIP_Ioctlv(IPCMessage));
				break;

			case IOS_READ:
			case IOS_WRITE:
			case IOS_SEEK:
			default:
				MessageQueueAck(IPCMessage, -4);
				break;
		}
	}
}