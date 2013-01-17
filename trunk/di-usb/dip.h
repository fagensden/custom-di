/*

SNEEK - SD-NAND/ES + DI emulation kit for Nintendo Wii

Copyright (C) 2009-2011  crediar
Copyright (C) 2011-2012  OverjoY

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

#ifndef _DIP_
#define _DIP_

#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
#include "alloc.h"
#include "vsprintf.h"
#include "DIGlue.h"
#include "utils.h"
#include "hollywood.h"

#define DISC_SUCCES		0
#define DI_SUCCESS		1
#define DI_ERROR		2
#define DI_FATAL		64

#define IS_FST			0
#define IS_WBFS			1
#define IS_DISC			2

#define FILECACHE_MAX	5
#define BLOCKCACHE_MAX	5
#define FILESPLITS_MAX	10
#define FRAG_MAX 		20000
#define DISCRETRY_MAX	5

#define DVD_CONFIG_SIZE		0x20
#define DVD_REAL_NAME_OFF	0x20
#define DVD_GAMEINFO_SIZE	0x140
#define DVD_GAME_NAME_OFF	0xA0
#define WII_MAGIC_OFF		0x18
#define DI_MAGIC_OFF		0x1c
#define CUS_CONFIG_SIZE		0x10
#define CUS_TITLE_SIZE		0x50

#define GCMAGIC				0xc2339f3d
#define WIIMAGIC			0x5d1c9ea3
#define CONFIGMAGIC			0x504c4159



enum fstmodes
{
	PAR_READ 	= 0,
	FST_READ,
	FST_EXTR,
	DEBUG_READ,
	NII_READ,
	NII_PARSE_FST,
};

enum disctypes
{
	DISC_REV	= 0,
	DISC_DOL	= 1,
	DISC_INV	= 2,
};

enum GameRegion 
{
	JAP			= 0,
	USA,
	EUR,
	KOR,
	ASN,
	LTN,
};

enum opcodes
{
	DVD_IDENTIFY			= 0x12,
	DVD_READ_DISCID			= 0x70,
	DVD_LOW_READ			= 0x71,
	DVD_WAITFORCOVERCLOSE	= 0x79,
	DVD_READ_PHYSICAL		= 0x80,
	DVD_READ_COPYRIGHT		= 0x81,
	DVD_READ_DISCKEY		= 0x82,
	DVD_GETCOVER			= 0x88,
	DVD_RESET				= 0x8A,
	DVD_OPEN_PARTITION		= 0x8B,
	DVD_CLOSE_PARTITION		= 0x8C,
	DVD_READ_UNENCRYPTED	= 0x8D,	
	DVD_REPORTKEY			= 0xA4,
	DVD_LOW_SEEK			= 0xAB,
	DVD_READ_CONFIG			= 0xD1,
	DVD_READ_BCA			= 0xDA,
	DVD_GET_ERROR			= 0xE0,
	DVD_SET_MOTOR			= 0xE3,
	DVD_SET_AUDIO_BUFFER	= 0xE4,
	
	DVD_SELECT_GAME			= 0x23,
	DVD_GET_GAMECOUNT		= 0x24,
	DVD_LOAD_DISC			= 0x25,
	DVD_MOUNT_DISC			= 0x26,
	DVD_EJECT_DISC			= 0x27,
	DVD_INSERT_DISC			= 0x28,
	DVD_UPDATE_GAME_CACHE	= 0x2F,
	DVD_READ_INFO			= 0x30,
	DVD_WRITE_CONFIG		= 0x31,

	DVD_OPEN				= 0x40,
	DVD_READ				= 0x41,
	DVD_WRITE				= 0x42,
	DVD_CLOSE				= 0x43,	
	DVD_CREATEDIR			= 0x45,
	DVD_SEEK				= 0x46,
	
	DVD_FRAG_SET			= 0xF9,
};

enum SNEEKConfig2
{
	DML_CHEATS				= (1<<0),
	DML_DEBUGGER			= (1<<1),
	DML_DEBUGWAIT			= (1<<2),
	DML_NMM					= (1<<3),
	DML_NMM_DEBUG			= (1<<4),
	DML_ACTIVITY_LED		= (1<<5),
	DML_PADHOOK				= (1<<6),
	DML_BOOT_DISC			= (1<<7),
	DML_BOOT_DOL			= (1<<8),
	DML_PROG_PATCH			= (1<<9),
	DML_FORCE_WIDESCREEN	= (1<<10),
};

enum SNEEKConfig
{
	CONFIG_PATCH_FWRITE		= (1<<0),
	CONFIG_PATCH_MPVIDEO	= (1<<1),
	CONFIG_PATCH_VIDEO		= (1<<2),
	CONFIG_DUMP_ERROR_SKIP	= (1<<3),
	CONFIG_DEBUG_GAME		= (1<<4),
	CONFIG_DEBUG_GAME_WAIT	= (1<<5),	
	CONFIG_READ_ERROR_RETRY	= (1<<6),
	CONFIG_GAME_ERROR_SKIP	= (1<<7),	
	CONFIG_MOUNT_DISC		= (1<<8),
	CONFIG_DI_ACT_LED		= (1<<9),
	CONFIG_REV_ACT_LED		= (1<<10),	
	DEBUG_CREATE_DIP_LOG	= (1<<11),
	DEBUG_CREATE_ES_LOG		= (1<<12),	
	CONFIG_SCROLL_TITLES	= (1<<13),
};

enum DMLLang
{
	DML_LANG_CONF			= (0xF<<20),
	
	DML_LANG_ENGLISH		= (1<<20),
	DML_LANG_GERMAN			= (2<<20),
	DML_LANG_FRENCH			= (3<<20),
	DML_LANG_SPANISH		= (4<<20),
	DML_LANG_ITALIAN		= (5<<20),
	DML_LANG_DUTCH			= (6<<20),
};

enum DMLVideo
{
	DML_VIDEO_CONF			= (0xF<<24),
	
	DML_VIDEO_GAME			= (1<<24),
	DML_VIDEO_PAL50			= (2<<24),
	DML_VIDEO_PAL60			= (3<<24),
	DML_VIDEO_NTSC			= (4<<24),	
	DML_VIDEO_NONE			= (5<<24),
};

enum HookTypes
{
	HOOK_TYPE_MASK			= (0xF<<28),
	
	HOOK_TYPE_VSYNC			= (1<<28),
	HOOK_TYPE_OSLEEP		= (2<<28),
	//HOOK_TYPE_AXNEXT		= (3<<28),
};

enum dmlconfig
{
	DML_CFG_CHEATS			= (1<<0),
	DML_CFG_DEBUGGER		= (1<<1),
	DML_CFG_DEBUGWAIT		= (1<<2),
	DML_CFG_NMM				= (1<<3),
	DML_CFG_NMM_DEBUG		= (1<<4),
	DML_CFG_GAME_PATH		= (1<<5),
	DML_CFG_CHEAT_PATH		= (1<<6),
	DML_CFG_ACTIVITY_LED	= (1<<7),
	DML_CFG_PADHOOK			= (1<<8),
	DML_CFG_FORCE_WIDE		= (1<<9),
	DML_CFG_BOOT_DISC		= (1<<10),
	DML_CFG_BOOT_DISC2		= (1<<11),
	DML_CFG_NODISC			= (1<<12),	
};

enum dmlvideomode
{
	DML_VID_AUTO			= (0<<16),
	DML_VID_FORCE			= (1<<16),
	DML_VID_NONE			= (2<<16),

	DML_VID_FORCE_PAL50		= (1<<0),
	DML_VID_FORCE_PAL60		= (1<<1),
	DML_VID_FORCE_NTSC		= (1<<2),
	DML_VID_FORCE_PROG		= (1<<3),	
	DML_VID_PROG_PATCH		= (1<<4),
};

typedef struct DML_CFG
{
	u32 Magicbytes;
	u32 CfgVersion;
	u32 VideoMode;
	u32 Config;
	char GamePath[255];
	char CheatPath[255];
} DML_CFG;

typedef struct
{
	u32		SlotID;
	u32		Region;
	u32		Gamecount;
	u32		Config;
	u32		Config2;
	u32		Magic;
	u16		FBGame;
	u16		GCGame;
	u32		Padding2;
	u8		GameInfo[][DVD_GAMEINFO_SIZE];
} DIConfig;

typedef struct
{
	union
	{
		struct
		{
			u32 Type		:8;
			u32 NameOffset	:24;
		};
		u32 TypeName;
	};
	union
	{
		struct		// File Entry
		{
			u32 FileOffset;
			u32 FileLength;
		};
		struct		// Dir Entry
		{
			u32 ParentOffset;
			u32 NextOffset;
		};
		u32 entry[2];
	};
} FEntry;

typedef struct
{
	u64 Offset;
	u32 Size;
	s32 File;
} FileCache;

typedef struct
{	
	u64 Offset;
	u32 Size;
	char Path[128];
} FileSplits;

typedef struct
{
	u8 *data;
	u32 len;
} vector;

typedef struct
{ 
//	u8 bl_buf[0x7c00];
	u32 bl_num;
} BlockCache;

typedef struct
{
	u32 EntryCount;
	u32 Padding1;
	u32 Padding2;
	u32 Padding3;
	char GameTitle[][0x50];
} GameTitles;

typedef struct
{
	u32 magic;
	u32 nbr_hd_sectors;
	u8  hdd_sector_size_s;
	u8  wbfs_sector_size_s;
	u8  Padding[6];
} WBFSFileInfo;

typedef struct
{
	u8  header[0x100];
	u16 disc_usage_table[];
} WBFSInfo;

typedef struct
{
	u32 offset;
	u32 sector;
	u32 count;
} Fragment;

typedef struct
{
	u32 size;
	u32 num;
	u32 maxnum;
	Fragment frag[FRAG_MAX];
} FragList;

u8 HardDriveConnected;//holds status of USB harddrive

int DIP_Ioctl( struct ipcmessage * );
int DIP_Ioctlv( struct ipcmessage * );
s32 DVDUpdateCache( u32 ForceUpdate );
s32 DVDSelectGame( int SlotID );
s32 DVDLowReadFiles( u32 Offset, u32 Length, void *ptr );
s32 DVDLowReadUnencrypted( u32 Offset, u32 Length, void *ptr );
s32 DVDLowReadDiscIDFiles( u32 Offset, u32 Length, void *ptr );
s32 Read( u64 offset, u32 length, void *ptr );
s32 Read_Block( u64 block, u32 bl_num ); 
s32 Encrypted_Read( u32 offset, u32 length, void *ptr);
s32 Decrypted_Write( char *path, char *filename, u32 offset, u32 length );
s32 Search_FST( u32 Offset, u32 Length, void *ptr, u32 mode );
s32 Do_Dol_Patches( u32 Length, void *ptr );

#endif