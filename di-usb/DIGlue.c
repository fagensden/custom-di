#include "DIGlue.h"

s32 FSHandle;
s32 *Nhandle;
char *Entries;
char *CEntry;
u32 FirstEntry;
u32 *EntryCount;

void DVDInit(void)
{
	int i;
	HardDriveConnected = 1;
	FSHandle = IOS_Open("/dev/fs", 0);

	Nhandle = (s32 *)malloc(sizeof(s32) * MAX_HANDLES);
	EntryCount = (u32 *)malloc(sizeof(s32));

	for(i = 0; i < MAX_HANDLES; i++)
		Nhandle[i] = 0xdeadbeef;

	Entries = (char *)NULL;

	return;
}

s32 DVDOpen(char *Filename, u32 Mode)
{
	int i;		
	u32 handle = 0xdeadbeef;

	for(i = 0; i < MAX_HANDLES; i++)
	{
		if(Nhandle[i] == 0xdeadbeef)
		{
			handle = i;
			break;
		}
	}

	if(handle == 0xdeadbeef)
		return DVD_FATAL;

	char *name = (char *)halloca(256, 32);
	memcpy(name, Filename, strlen(Filename) + 1);
	vector *vec = (vector*)halloca(sizeof(vector) * 2, 32);
		
	vec[0].data = (u8 *)name;
	vec[0].len = 256;
	vec[1].data = (u8 *)Mode;
	vec[1].len = sizeof(u32);

	Nhandle[handle] = IOS_Ioctlv(FSHandle, 0x60, 2, 0, vec);
		
	name[0] = '/';
	name[1] = 'A';
	name[2] = 'X';
	name[3] = '\0';

	Nhandle[handle] = IOS_Open(name, Mode & (FA_READ | FA_WRITE));

	hfree(name);
	hfree(vec);

	if(Nhandle[handle] < 0)
	{
		s32 temp = Nhandle[handle];
		Nhandle[handle] = 0xdeadbeef;
		handle = temp;
	}	
	return handle;
}

s32 DVDOpenDir(char *Path)
{
	char *name = (char*)halloca(256, 32);
	memcpy(name, Path, strlen(Path) + 1);

	s32 ret = ISFS_ReadDir(name, (char *)NULL, EntryCount);
	if( ret < 0 )
	{
		hfree(name);
		return ret;
	}

	if(Entries != NULL)
		free(Entries);
		
	FirstEntry = 0;
	if (*EntryCount != 0)
	{
		Entries = (char *)malloca(*EntryCount * 64, 32);

		ret = ISFS_ReadDir( name, Entries, EntryCount );
		CEntry = Entries;
	}		
	hfree(name);
	return ret;
}

s32 DVDReadDir(void)
{
	if(FirstEntry > 0)
	{
		while(*CEntry++ != '\0');
	}

	FirstEntry++;

	if(FirstEntry <= *EntryCount)
		return DVD_SUCCESS;
	else
		return DVD_NO_FILE;
}

char *DVDDirGetEntryName(void)
{
	return CEntry;
}

u32 DVDRead(s32 handle, void *ptr, u32 len)
{
	if(handle >= MAX_HANDLES)
		return 0;
	if(Nhandle[handle] == 0xdeadbeef)
		return 0;

	return IOS_Read(Nhandle[handle], ptr, len);
}

u32 DVDWrite(s32 handle, void *ptr, u32 len)
{
	if(handle >= MAX_HANDLES)
		return 0;
	if(Nhandle[handle] == 0xdeadbeef)
		return 0;

	return IOS_Write( Nhandle[handle], ptr, len );
}

s32 DVDSeek( s32 handle, s32 where, u32 whence )
{
	if(handle >= MAX_HANDLES)
		return 0;
	if(Nhandle[handle] == 0xdeadbeef)
		return 0;

	return IOS_Seek(Nhandle[handle], where, whence);
}

u32 DVDGetSize(s32 handle)
{
	if(handle >= MAX_HANDLES)
		return 0;
	if(Nhandle[handle] == 0xdeadbeef)
		return 0;

	u32 size = 0;
	fstats *status = (fstats *)halloca(sizeof(fstats), 32);

	if(ISFS_GetFileStats( Nhandle[handle], status) >= 0)
		size = status->Size;

	hfree(status);

	return size;		
}

s32 DVDDelete(char *Path)
{
	return ISFS_Delete(Path);
}

s32 DVDCreateDir(char *Path)
{
	return ISFS_CreateDir(Path, 0, 3, 3, 3);
}

void DVDClose(s32 handle)
{
	if(handle >= MAX_HANDLES)
		return;
	if(Nhandle[handle] == 0xdeadbeef)
		return;

	IOS_Close(Nhandle[handle]);
	Nhandle[handle] = 0xdeadbeef;		

	return;
}
