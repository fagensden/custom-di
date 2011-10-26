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

#define DIPATHFILE "/sneek/dipath.bin"

#define PATHFILE 		"/sneek/nandcfg.bin"
#define NANDFOLDER 		"/nands"
#define NANDCFG_SIZE 	0x10
#define NANDDESC_OFF	0x40
#define NANDINFO_SIZE	0x80

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

typedef struct
{	
	u32 NandCnt;
	u32 NandSel;
	u32 Padding1;
	u32 Padding2;
	u8  NandInfo[][NANDINFO_SIZE];
} NandConfig;

NandConfig *NandCFG;

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
	FILINFO FInfo;
	FIL fil;
	DIR dir;
	u32 read;
	u32 write;
	u32 usenfol=0;
	u32 counter;
	UINT toread, read_ok;

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
	char *npath = (char*)heap_alloc_aligned( 0, 0x40, 32 );
	u8 *NInfo = (u8 *)heap_alloc_aligned( 0, 0x60, 32 );

	strcpy( path, PATHFILE );
	strcpy( npath, NANDFOLDER );
	
	if( f_open( &fil, path, FA_READ ) != FR_OK )
	{
		if( f_opendir( &dir, npath ) == FR_OK )
		{
			u32 ncnt=0;
			f_open( &fil, path, FA_WRITE|FA_CREATE_ALWAYS );
			NandCFG->NandCnt = 0;
			NandCFG->NandSel = 0;
			f_write( &fil, NandCFG, NANDCFG_SIZE, &write );
			f_lseek( &fil, 0x10 );
			while( f_readdir( &dir, &FInfo ) == FR_OK )
			{
				if( FInfo.lfsize )
				{
					memcpy( NInfo, FInfo.lfname, NANDDESC_OFF );
					memcpy( NInfo+NANDDESC_OFF, FInfo.lfname, NANDDESC_OFF );
				}
				else
				{
					memcpy( NInfo, FInfo.fname, NANDDESC_OFF );
					memcpy( NInfo+NANDDESC_OFF, FInfo.fname, NANDDESC_OFF );
				}
				f_write( &fil, NInfo, NANDINFO_SIZE, &write );
				ncnt++;
			}
			NandCFG->NandCnt = ncnt;
			f_lseek( &fil, 0 );
			f_write( &fil, NandCFG, NANDCFG_SIZE, &write );
			f_close( &fil );
			f_open( &fil, path, FA_READ );
			usenfol = 1;
		}
		else
		{
			nandroot[0] = 0;
		}
	}
	else
	{
		usenfol = 1;
	}
	
	if( usenfol )
	{
		if( NandCFG )
			heap_free( 0, NandCFG );
			
		NandCFG = (NandConfig *)heap_alloc_aligned( 0, fil.fsize, 32 );
		f_read( &fil, NandCFG, fil.fsize, &read );
	
		// this is no good
		// why do we use a define and a fixed one here?	
		__sprintf( nandroot, "/nands/%.63s", NandCFG->NandInfo[NandCFG->NandSel] );
		f_close(&fil);
	}
	
	//this was missing...
		
	strcpy(path,nandroot);
	size_t plen=strlen(path);
	strcpy(path+plen,"/sneekcache");
	if (f_opendir(&dir,path) != FR_OK)
	{
		FS_CreateDir("/sneekcache");
	}
	
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
			if(f_read(&fil,npath,toread,&read_ok) == FR_OK)
			{
				diroot[0] = '/';
				counter = 0;
				while (counter < read_ok)
				{
					//we might terminate with <CR> <LF> <0> or <space>
					if ((npath[counter] != 13)&&(npath[counter] != 10)&&(npath[counter] != 0)&&(npath[counter] != 32))
					{
						diroot[counter+1] = npath[counter];
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
	
	heap_free( 0, NInfo );
	heap_free( 0, path );
	heap_free( 0, npath );
	heap_free( 0, NandCFG );
	dbgprintf("Nand folder set to %s\n",nandroot);	
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
