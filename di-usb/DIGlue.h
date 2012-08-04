#ifndef _DIGLUE_
#define _DIGLUE_

#include "string.h"
#include "syscalls.h"
#include "global.h"
#include "ipc.h"
#include "alloc.h"
#include "dip.h"
#include "FS.h"

#define MAX_HANDLES				10

#define FS_ENOENT2				-106
#define ISFS_IS_USB				30
#define ISFS_SUPPORT_SD_DI		33
#define ISFS_GET_DI_PATH		34

#define	FA_READ					0x01
#define	FA_OPEN_EXISTING		0x00
#define	FA_WRITE				0x02
#define	FA_CREATE_NEW			0x04
#define	FA_CREATE_ALWAYS		0x08
#define	FA_OPEN_ALWAYS			0x10
#define FA__WRITTEN				0x20
#define FA__DIRTY				0x40
#define FA__ERROR				0x80
#define FR_OK  					0

enum DVDOMODE
{
	DREAD	= 1,
	DWRITE	= 2,
	DCREATE	= 4,
};

enum DVDStatus
{
	DVD_SUCCESS	=	0,
	DVD_NO_FILE =	-106,
	DVD_FATAL	=	-101,
};

void DVDInit(void);

s32 DVDOpen(char *Filename, u32 Mode);
void DVDClose(s32 handle);

u32 DVDGetSize(s32 handle);
s32 DVDDelete(char *Path);
s32 DVDCreateDir(char *Path);

u32 DVDRead(s32 handle, void *ptr, u32 len);
u32 DVDWrite(s32 handle, void *ptr, u32 len);
s32 DVDSeek(s32 handle, s32 where, u32 whence);

s32 DVDOpenDir(char *Path);
s32 DVDReadDir(void);
char *DVDDirGetEntryName(void);


#endif
