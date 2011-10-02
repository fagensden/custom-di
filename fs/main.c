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
#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
#include "diskio.h"
#include "ff.h"
#include "sdhcvar.h"
#include "FS.h"

#define PATHFILE 		"/sneek/nandcfg.bin"
#define NANDFOLDER 		"/nands"
#define NANDCFG_SIZE 	0x10
#define NANDDESC_OFF	0x40
#define NANDINFO_SIZE	0x80

extern char nandroot[0x20] ALIGNED(32);


FATFS fatfs;
int verbose = 0;
u32 base_offset=0;
static char Heap[0x100] ALIGNED(32);
void *QueueSpace = NULL;
int QueueID = 0;
int HeapID=0;

#undef DEBUG

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
	//dbgprintf("FFS:DeviceRegister(\"/\"):%d\n", ret );
#endif
	if( ret < 0 )
		return ret;

//	ret = device_register("/dev/sdio", QueueID );
//#ifdef DEBUG
//	dbgprintf("FFS:DeviceRegister(\"/dev/sdio\"):%d QueueID:%d\n", ret, QueueID );
//#endif

	return QueueID;
}
int _main( int argc, char *argv[] )
{
	int fres=0;
	s32 ret=0;
	struct IPCMessage *CMessage=NULL;
	FIL fil;
	DIR dir;
	u32 read;
	u32 write;
	u32 usenfol=0;

	thread_set_priority( 0, 0x58 );

#ifdef DEBUG
	dbgprintf("$IOSVersion: FFS-SD: %s %s 64M DEBUG$\n", __DATE__, __TIME__ );
#else
	dbgprintf("$IOSVersion: FFS-SD: %s %s 64M Release$\n", __DATE__, __TIME__ );
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

	sdhc_init();

	//dbgprintf("FFS:Mounting SD...\n");
	fres = f_mount(0, &fatfs);
	//dbgprintf("FFS:f_mount():%d\n", fres);

	if(fres != FR_OK)
	{
		//dbgprintf("FFS:Error %d while trying to mount SD\n", fres);
		ThreadCancel( 0, 0x77 );
	}

	//dbgprintf("FFS:Clean up...");
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
		__sprintf( nandroot, "/nands/%.63s", NandCFG->NandInfo[NandCFG->NandSel] );
		f_close(&fil);
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

//	f_mkdir("/tmp");
//	f_mkdir("/import");

	//dbgprintf("ok\n");

	while (1)
	{
		ret = mqueue_recv(QueueID, (void *)&CMessage, 0);
		if( ret != 0 )
		{
			//dbgprintf("FFS:mqueue_recv(%d) FAILED:%d\n", QueueID, ret);
			continue;
		}

		//dbgprintf("FFS:Cmd:%d\n", CMessage->command );
		
		switch( CMessage->command )
		{
			case IOS_OPEN:
			{
				ret = FS_Open( CMessage->open.device, CMessage->open.mode );
				
#ifdef DEBUG
				if( ret != FS_NO_DEVICE )
					dbgprintf("FFS:IOS_Open(\"%s\", %d):%d\n", CMessage->open.device, CMessage->open.mode, ret );
#endif
				mqueue_ack( (void *)CMessage, ret);
				
			} break;
			
			case IOS_CLOSE:
			{
#ifdef DEBUG
				dbgprintf("FFS:IOS_Close(%d):", CMessage->fd);
#endif
				ret = FS_Close( CMessage->fd );
#ifdef DEBUG
				dbgprintf("%d\n", ret );
#endif
				mqueue_ack( (void *)CMessage, ret);
			} break;

			case IOS_READ:
			{

#ifdef DEBUG
				dbgprintf("FFS:IOS_Read(%d, 0x%p, %d):", CMessage->fd, CMessage->read.data, CMessage->read.length );
#endif
				ret = FS_Read( CMessage->fd, CMessage->read.data, CMessage->read.length );

#ifdef DEBUG
				dbgprintf("%d\n", ret );
#endif
				mqueue_ack( (void *)CMessage, ret );

			} break;
			case IOS_WRITE:
			{
#ifdef DEBUG
				dbgprintf("FFS:IOS_Write(%d, 0x%p, %d)", CMessage->fd, CMessage->write.data, CMessage->write.length );
#endif
				ret = FS_Write( CMessage->fd, CMessage->write.data, CMessage->write.length );

#ifdef DEBUG
				dbgprintf(":%d\n", ret );
#endif
				mqueue_ack( (void *)CMessage, ret );
			} break;
			case IOS_SEEK:
			{
#ifdef DEBUG
				dbgprintf("FFS:IOS_Seek(%d, %d, %d):", CMessage->fd, CMessage->seek.offset, CMessage->seek.origin );
#endif
				ret = FS_Seek( CMessage->fd, CMessage->seek.offset, CMessage->seek.origin );

#ifdef DEBUG
				dbgprintf("%d\n", ret);
#endif
				mqueue_ack( (void *)CMessage, ret );

			} break;
			
			case IOS_IOCTL:
				FFS_Ioctl(CMessage);
			break;	

			case IOS_IOCTLV:
				FFS_Ioctlv(CMessage);
			break;
#ifdef EDEBUG
			default:
				dbgprintf("FFS:unimplemented/invalid msg: %08x\n", CMessage->command);
				mqueue_ack( (void *)CMessage, -1017);
#endif
		}
	}

	return 0;
}
