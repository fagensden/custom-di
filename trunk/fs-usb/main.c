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
#include "ff.h"
#include "FS.h"

#define NANDPATHFILE "/sneek/nandpath.bin"
#define DIPATHFILE "/sneek/dipath.bin"

#define MAXPATHLEN	16

FATFS fatfs;

extern char nandroot[0x20] ALIGNED(32);
extern char diroot[0x20] ALIGNED(32);

static char Heap[0x100] ALIGNED(32);
void *QueueSpace = NULL;
int QueueID = 0;
int HeapID=0;
int tiny_ehci_init(void);
int ehc_loop(void);

int verbose=0;

#undef DEBUG

void udelay(int us)
{
	u8 heap[0x10];
	u32 msg;
	s32 mqueue = -1;
	s32 timer = -1;

	mqueue = mqueue_create(heap, 1);
	if(mqueue < 0)
		goto out;
	timer = timer_create(us, 0, mqueue, 0xbabababa);
	if(timer < 0)
		goto out;
	mqueue_recv(mqueue, &msg, 0);
	
out:
	if(timer > 0)
		timer_destroy(timer);
	if(mqueue > 0)
		mqueue_destroy(mqueue);
}

#define ALIGN_FORWARD(x,align) \
	((typeof(x))((((u32)(x)) + (align) - 1) & (~(align-1))))

#define ALIGN_BACKWARD(x,align) \
	((typeof(x))(((u32)(x)) & (~(align-1))))

static char ascii(char s)
{
  if(s < 0x20) return '.';
  if(s > 0x7E) return '.';
  return s;
}

void hexdump(void *d, int len)
{
  u8 *data;
  int i, off;
  data = (u8*)d;
  for (off=0; off<len; off += 16) {
    dbgprintf("%08x  ",off);
    for(i=0; i<16; i++)
      if((i+off)>=len) dbgprintf("   ");
      else dbgprintf("%02x ",data[off+i]);

    dbgprintf(" ");
    for(i=0; i<16; i++)
      if((i+off)>=len) dbgprintf(" ");
      else dbgprintf("%c",ascii(data[off+i]));
    dbgprintf("\n");
  }
}

s32 RegisterDevices( void )
{
	QueueID = mqueue_create(QueueSpace, 8);

//	s32 ret = device_register("/dev/flash", QueueID);
//#ifdef DEBUG
//	dbgprintf("FFS:DeviceRegister(\"/dev/flash\"):%d\n", ret );
//#endif
//	if( ret < 0 )
//		return ret;
//
//	ret = device_register("/dev/boot2", QueueID);
//#ifdef DEBUG
//	dbgprintf("FFS:DeviceRegister(\"/dev/boot2\"):%d\n", ret );
//#endif
//	if( ret < 0 )
//		return ret;

	s32 ret = device_register("/", QueueID);
#ifdef DEBUG
	dbgprintf("FFS:DeviceRegister(\"/\"):%d\n", ret );
#endif
	if( ret < 0 )
		return ret;

//	ret = device_register("/dev/sdio", QueueID );
//#ifdef DEBUG
//	dbgprintf("FFS:DeviceRegister(\"/dev/sdio\"):%d QueueID:%d\n", ret, QueueID );
//#endif

	return QueueID;
}

char __aeabi_unwind_cpp_pr0[0];
void _main(void)
{
	s32 ret=0;
	struct IPCMessage *CMessage=NULL;
	FIL fil;
	DIR dir;
	UINT toread, read_ok;
	u32 counter;

	thread_set_priority( 0, 0x58 );

#ifdef DEBUG
	dbgprintf("$IOSVersion: FFS-USB: %s %s 64M DEBUG$\n", __DATE__, __TIME__ );
#else
	dbgprintf("$IOSVersion: FFS-USB: %s %s 64M Release$\n", __DATE__, __TIME__ );
#endif

	//dbgprintf("FFS:Heap Init...");
	HeapID = heap_create(Heap, sizeof Heap);
	QueueSpace = heap_alloc(HeapID, 0x20);
	//dbgprintf("ok\n");

	QueueID = RegisterDevices();
	if( QueueID < 0 )
	{
		ThreadCancel( 0, 0x77 );
	}

	//dbgprintf("FFS:Mounting USB...\n");
	ret = f_mount(0, &fatfs);
	//dbgprintf("FFS:f_mount():%d\n", fres);

	if(ret != FR_OK)
	{
		dbgprintf("FFS:Error %d while trying to mount USB\n", ret );
		ThreadCancel( 0, 0x77 );
	}

	//dbgprintf("FFS:Clean up...");
	
	//strcpy(nandroot,"/nandsneek");
	
	// get the nandfolder from /sneek/nandpath.bin file.
	
	char *path = (char*)heap_alloc_aligned( 0, 0x40, 32 );
	char *rbuf = (char*)heap_alloc_aligned( 0, MAXPATHLEN + 16, 32 );

	strcpy( path,NANDPATHFILE );

	nandroot[0] = 0;
	if( f_open( &fil, (char*)path, FA_READ ) == FR_OK )
	{
		if (fil.fsize > 0)
		{
			if (fil.fsize <= (MAXPATHLEN + 16))
			{
				toread = (UINT)(fil.fsize);
			}
			else
			{
				toread = MAXPATHLEN + 16;
			}
			if(f_read(&fil,rbuf,toread,&read_ok) == FR_OK)
			{
				nandroot[0] = '/';
				counter = 0;
				while (counter < read_ok)
				{
					//we might terminate with <CR> <LF> <0> or <space>
					if ((rbuf[counter] != 13)&&(rbuf[counter] != 10)&&(rbuf[counter] != 0)&&(rbuf[counter] != 32))
					{
						nandroot[counter+1] = rbuf[counter];
						// just in case counter might reach read_ok
						nandroot[counter+2] = 0;
						counter++;
					}
					else
					{
						nandroot[counter+1] = 0;
						counter = read_ok;
					}
				}
			}
		}
		f_close(&fil);
		//check if the nandroot folder exist
		dbgprintf("Nand folder set to %s\n",nandroot);
		strcpy(path,nandroot);
		if (f_opendir(&dir,path) != FR_OK)
		{
			nandroot[0] = 0;
		}
		else
		{
			
			size_t plen=strlen(path);
			strcpy(path+plen,"/sneekcache");
			if (f_opendir(&dir,path) != FR_OK)
			{
				FS_CreateDir("/sneekcache");
			}
		}
	}
	
	// get the difolder from /sneek/dipath.bin file.

	strcpy( path,DIPATHFILE );

	diroot[0] = 0;
	if( f_open( &fil, (char*)path, FA_READ ) == FR_OK )
	{
		if (fil.fsize > 0)
		{
			if (fil.fsize <= (MAXPATHLEN + 16))
			{
				toread = (UINT)(fil.fsize);
			}
			else
			{
				toread = MAXPATHLEN + 16;
			}
			if(f_read(&fil,rbuf,toread,&read_ok) == FR_OK)
			{
				diroot[0] = '/';
				counter = 0;
				while (counter < read_ok)
				{
					//we might terminate with <CR> <LF> <0> or <space>
					if ((rbuf[counter] != 13)&&(rbuf[counter] != 10)&&(rbuf[counter] != 0)&&(rbuf[counter] != 32))
					{
						diroot[counter+1] = rbuf[counter];
						// just in case counter might reach read_ok
						diroot[counter+2] = 0;
						counter++;
					}
					else
					{
						diroot[counter+1] = 0;
						counter = read_ok;
					}
				}
			}
		}
		f_close(&fil);
		//check if the nandroot folder exist
		dbgprintf("Di folder set to %s\n",diroot);
		strcpy(path,diroot);
		if (f_opendir(&dir,path) != FR_OK)
		{
			strcpy(diroot,"sneek");
		}
	}
	heap_free( 0, path );
	heap_free( 0, rbuf );	
	dbgprintf("Nand folder set to %s\n",nandroot);
	dbgprintf("Di folder set to %s\n",diroot);
	
	//clean up folders
	FS_Delete("/tmp");
	FS_Delete("/import");

	FS_CreateDir("/tmp");
	FS_CreateDir("/import");	
	

/*
	f_mkdir("/tmp");
	f_mkdir("/import");
*/
	//dbgprintf("ok\n");

	while (1)
	{
		ret = mqueue_recv(QueueID, (void *)&CMessage, 0);
		if( ret != 0 )
		{
			//dbgprintf("FFS:mqueue_recv(%d) FAILED:%d\n", QueueID, ret);
			continue;
		}
		
		switch (CMessage->command)
		{
			case IOS_OPEN:
			{
				ret = FS_Open( CMessage->open.device, CMessage->open.mode );
#ifdef DEBUG
				if( ret != FS_ENOENT )
					dbgprintf("FFS:IOS_Open(\"%s\", %d):%d\n", CMessage->open.device, CMessage->open.mode, ret );
#endif
//				if( ret != FS_ENOENT )
//					dbgprintf("FFS:IOS_Open(\"%s\", %d):%d\n", CMessage->open.device, CMessage->open.mode, ret );
				
				mqueue_ack( (void *)CMessage, ret);
				
			} break;
			
			case IOS_CLOSE:
			{
				ret = FS_Close( CMessage->fd );
#ifdef DEBUG
				dbgprintf("FFS:IOS_Close(%d):%d\n", CMessage->fd, ret );
#endif
				mqueue_ack( (void *)CMessage, ret);
			} break;

			case IOS_READ:
			{

				ret = FS_Read( CMessage->fd, CMessage->read.data, CMessage->read.length );
					
#ifdef DEBUG
				dbgprintf("FFS:IOS_Read(%d, 0x%p, %d):%d\n", CMessage->fd, CMessage->read.data, CMessage->read.length, ret );
#endif
				mqueue_ack( (void *)CMessage, ret );

			} break;
			case IOS_WRITE:
			{
				ret = FS_Write( CMessage->fd, CMessage->write.data, CMessage->write.length );

#ifdef DEBUG
				dbgprintf("FFS:IOS_Write(%d, 0x%p, %d):%d\n", CMessage->fd, CMessage->write.data, CMessage->write.length, ret );
#endif
				mqueue_ack( (void *)CMessage, ret );
			} break;
			case IOS_SEEK:
			{
				ret = FS_Seek( CMessage->fd, CMessage->seek.offset, CMessage->seek.origin );

#ifdef DEBUG
				dbgprintf("FFS:IOS_Seek(%d, %d, %d):%d\n", CMessage->fd, CMessage->seek.offset, CMessage->seek.origin, ret);
#endif
				mqueue_ack( (void *)CMessage, ret );

			} break;
			
			case IOS_IOCTL:
				if( CMessage->fd == SD_FD )
				{
					if( CMessage->ioctl.command == 0x0B )
						*(vu32*)(CMessage->ioctl.buffer_io) = 2;

					mqueue_ack( (void *)CMessage, 0);
				} else
					FFS_Ioctl(CMessage);
			break;	

			case IOS_IOCTLV:
				if( CMessage->fd == FL_FD )
				{
					//dbgprintf("FFS:IOS_Ioctlv( %d 0x%x %d %d 0x%p )\n", CMessage->fd, CMessage->ioctlv.command, CMessage->ioctlv.argc_in, CMessage->ioctlv.argc_io, CMessage->ioctlv.argv);
					mqueue_ack( (void *)CMessage, -1017);
				} else
					FFS_Ioctlv(CMessage);
			break;
#ifdef EDEBUG
			default:
				dbgprintf("FFS:unimplemented/invalid msg: %08x\n", CMessage->command);
				mqueue_ack( (void *)CMessage, -1017);
#endif
		}
	}

	return;
}
