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
#include "ff.h"
#include "FS.h"

#define NANDCFGFILE		"/sneek/nandcfg.bin"
#define NANDFOLDER 		"/nands"
#define NANDCFG_SIZE 	0x10
#define NANDDESC_SIZE 	0x40
#define NANDDESC_OFF	0x80
#define NANDDI_OFF		0xC0
#define NANDINFO_SIZE	0x100

FATFS fatfs;

extern char nandroot[NANDDESC_OFF] ALIGNED(32);
extern char diroot[NANDDESC_SIZE] ALIGNED(32);

static char Heap[0x100] ALIGNED(32);
void *QueueSpace = NULL;
int QueueID = 0;
int HeapID = 0;
int tiny_ehci_init(void);
int ehc_loop(void);

int verbose = 0;
int unlockcfg = 0;

typedef struct
{	
	u32 NandCnt;
	u32 NandSel;
	u32 NandExt;
	u32 Config;
	u8  NandInfo[][NANDINFO_SIZE];
} NandConfig;

typedef struct
{
	u32 magic;
	u64 titleid;
	u32 config;
	u32 paddinga;
	u32 paddingb;
	u32 paddingc;
	u32 paddingd;
	char dipath[256];
	char nandpath[256];
} MEMCfg;

enum ExtNANDCfg
{
	NCON_EXT_DI_PATH		= (1<<0),
	NCON_EXT_NAND_PATH		= (1<<1),
	NCON_HIDE_EXT_PATH		= (1<<2),
};

NandConfig *NandCFG;

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

void _main(void)
{
	s32 ret = 0;
	struct IPCMessage *CMessage=NULL;
	FILINFO FInfo;
	FIL fil;
	DIR dir;
	u32 read, write, i;
	u32 extpath = 0;
	u32 Extcnt = 0;

	thread_set_priority( 0, 0x58 );

#ifdef DEBUG
	dbgprintf("$IOSVersion: FFS-USB: %s %s 64M DEBUG$\n", __DATE__, __TIME__ );
#else
	dbgprintf("$IOSVersion: FFS-USB: %s %s 64M Release$\n", __DATE__, __TIME__ );
#endif

	HeapID = heap_create(Heap, sizeof Heap);
	QueueSpace = heap_alloc(HeapID, 0x20);

	QueueID = RegisterDevices();
	if( QueueID < 0 )
		ThreadCancel( 0, 0x77 );

	ret = f_mount(0, &fatfs);
	if(ret != FR_OK)
		ThreadCancel(0, 0x77);


	char *path = (char*)heap_alloc_aligned( 0, NANDDESC_SIZE, 0x40 );
	char *npath = (char*)heap_alloc_aligned( 0, NANDDESC_SIZE, 0x40 );
	char *tempnroot = (char*)heap_alloc_aligned( 0, NANDDESC_OFF, 0x40 );	
	char *fpath = (char *)heap_alloc_aligned( 0, NANDDESC_OFF, 32 );
	u8 *NInfo = (u8 *)heap_alloc_aligned( 0, NANDINFO_SIZE, 32 );
	u8 *MInfo = NULL;
	
	nandroot[0] = 0;
	strcpy(diroot, "/sneek");

	MEMCfg *MC = (MEMCfg *)0x01200000;
	
	if(MC->magic == 0x666c6f77)
	{
		dbgprintf("FFS:Found config magic in memory: 0x%08x\n", *(vu32*)0x01200000);
		if(MC->config&NCON_EXT_DI_PATH)
		{
			dbgprintf("FFS:DI path changed by external app to \"%s\"\n", MC->dipath);
			strcpy(diroot, MC->dipath);
		}
		if(MC->config&NCON_EXT_NAND_PATH)
		{
			dbgprintf("FFS:Nand path changed by external app to \"%s\"\n", MC->nandpath);
			strcpy(nandroot, MC->nandpath);
			if(!(MC->config&NCON_HIDE_EXT_PATH))
			{
				memset32(NInfo, 0, NANDINFO_SIZE);
				memcpy(NInfo, nandroot, NANDDESC_OFF);
				memcpy(NInfo+NANDDESC_OFF, nandroot, NANDDESC_SIZE);
				memcpy(NInfo+NANDDI_OFF, diroot, NANDDESC_SIZE);
				extpath = 1;
			}
		}
	}		

	strcpy(path, NANDCFGFILE);
	strcpy(npath, NANDFOLDER);
	
	unlockcfg = 1;
	
	if(f_open(&fil, path, FA_READ) == FR_OK)
	{
		if(NandCFG)
			heap_free(0, NandCFG);
			
		NandCFG = (NandConfig *)heap_alloc_aligned(0, fil.fsize, 32);
		f_read(&fil, NandCFG, fil.fsize, &read);		
		Extcnt = NandCFG->NandExt;
		__sprintf(tempnroot, "%.127s", NandCFG->NandInfo[NandCFG->NandSel]);
		if(Extcnt != 0)
		{
			MInfo = (u8 *)heap_alloc_aligned( 0, NANDINFO_SIZE * Extcnt, 32 );
			memset32(MInfo, 0, NANDINFO_SIZE * Extcnt);
			f_lseek(&fil, NANDCFG_SIZE);
			f_read(&fil, MInfo, NANDINFO_SIZE * Extcnt, &read);
			if(nandroot[0] != 0)
			{
				for(i = 0; i < Extcnt; ++i)
				{
					if(strncmp(nandroot, (char *)NandCFG->NandInfo[i], strlen(nandroot)) == 0)
					{
						extpath = 0;
						break;
					}
				}
			}
		}
		f_close(&fil);
		f_unlink(path);
		heap_free(0, NandCFG);	
	}
	
	if(f_opendir(&dir, npath) == FR_OK)
	{
		NandCFG = (NandConfig *)heap_alloc_aligned(0, NANDCFG_SIZE, 32);
		memset32(NandCFG, 0, NANDCFG_SIZE);
		u32 ncnt=0;
		f_open(&fil, path, FA_WRITE|FA_CREATE_ALWAYS);
		NandCFG->NandCnt = 0;
		NandCFG->NandSel = 0;
		NandCFG->NandExt = Extcnt + extpath;
		f_write(&fil, NandCFG, NANDCFG_SIZE, &write);
		f_lseek(&fil, NANDCFG_SIZE);
		if(extpath != 0)
		{
			f_write(&fil, NInfo, NANDINFO_SIZE, &write);
		}
		if(Extcnt != 0)
		{
			f_write(&fil, MInfo, NANDINFO_SIZE * Extcnt, &write);
		}
		heap_free(0, MInfo);
		ncnt += Extcnt;
		while(f_readdir(&dir, &FInfo) == FR_OK)
		{
			memset32(NInfo, 0, NANDINFO_SIZE);
			if(FInfo.lfsize)
				memcpy(NInfo+NANDDESC_OFF, FInfo.lfname, strlen(FInfo.lfname));
			else
				memcpy(NInfo+NANDDESC_OFF, FInfo.fname, strlen(FInfo.fname));
				
			memset32(fpath, 0, NANDDESC_OFF);
			strcpy(fpath, NANDFOLDER);
			strcat(fpath, "/");
			strcat(fpath, (char *)(NInfo+NANDDESC_OFF));			
			
			memcpy(NInfo, fpath, strlen(fpath));
			memcpy(NInfo+NANDDI_OFF, diroot, strlen(fpath));

			f_write(&fil, NInfo, NANDINFO_SIZE, &write);
			
			if(strcmp(tempnroot, fpath) == 0)
				NandCFG->NandSel = ncnt  + extpath;
				
			ncnt++;
		}
		NandCFG->NandCnt = ncnt;
		f_lseek(&fil, 0);
		f_write(&fil, NandCFG, NANDCFG_SIZE, &write);
		f_close(&fil);
	}


	if(NandCFG)
		heap_free(0, NandCFG);
			
	NandCFG = (NandConfig *)heap_alloc_aligned(0, fil.fsize, 32);
	f_open(&fil, path, FA_READ);
	f_read(&fil, NandCFG, fil.fsize, &read);
	f_close(&fil);
	unlockcfg = 0;
	if(nandroot[0] == 0)
		__sprintf(nandroot, "%.127s", NandCFG->NandInfo[NandCFG->NandSel]);
	
	dbgprintf("FFS:Using path to nand: \"%s\"\n", nandroot);

	strcpy(path, nandroot);
	strcat(path, "/sneekcache");
	
	if (f_opendir( &dir, path ) != FR_OK)
		FS_CreateDir("/sneekcache");
	
	
	heap_free(0, fpath);
	heap_free(0, tempnroot);
	heap_free(0, NInfo);
	heap_free(0, path);
	heap_free(0, npath);
	heap_free(0, NandCFG);
	
	FS_Delete("/tmp");
	FS_Delete("/import");

	FS_CreateDir("/tmp");
	FS_CreateDir("/import");	
	
	thread_set_priority(0, 0x08);

	while(1)
	{
		ret = mqueue_recv(QueueID, (void *)&CMessage, 0);
		if( ret != 0 )
			continue;
		
		switch (CMessage->command)
		{
			case IOS_OPEN:
			{
				ret = FS_Open( CMessage->open.device, CMessage->open.mode );
#ifdef DEBUG
				if( ret != FS_ENOENT )
					dbgprintf("FFS:IOS_Open(\"%s\", %d):%d\n", CMessage->open.device, CMessage->open.mode, ret );
#endif
				
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
					mqueue_ack( (void *)CMessage, -1017);
				else
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
