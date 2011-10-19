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
#include "NAND.h"
#include "di.h"

extern u8  *CNTMap;
extern char diroot[0x20];


u8 *NANDLoadFile( char *path, u32 *Size )
{
	s32 fd = IOS_Open( path, 1 );
	if( fd < 0 )
	{
		dbgprintf("ES:NANDLoadFile->IOS_Open(\"%s\", 1 ):%d\n", path, fd );
		*Size = fd;
		return NULL;
	}

	//dbgprintf("ES:NANDLoadFile->IOS_Open(\"%s\", 1 ):%d\n", path, fd );

	fstats *status = (fstats*)heap_alloc_aligned( 0, sizeof(fstats), 0x40 );

	s32 r = ISFS_GetFileStats( fd, status );
	//dbgprintf("ES:NANDLoadFile->ISFS_GetFileStats(\"%s\", 1 ):%d\n", path, r );
	if( r < 0 )
	{
		dbgprintf("ES:NANDLoadFile->ISFS_GetFileStats(%d, %p ):%d\n", fd, status, r );
		heap_free( 0, status );
		*Size = r;
		return NULL;
	}

	*Size = status->Size;
	//dbgprintf("ES:NANDLoadFile->Size:%d\n", *Size );

	u8 *data = (u8*)heap_alloc_aligned( 0, status->Size, 0x40 );
	if( data == NULL )
	{
		dbgprintf("ES:NANDLoadFile(\"%s\")->Failed to alloc %d bytes!\n", path, status->Size );
		heap_free( 0, status );
		return NULL;
	}

	r = IOS_Read( fd, data, status->Size );
	//dbgprintf("ES:NANDLoadFile->IOS_Read():%d\n", r );
	if( r < 0 )
	{
		dbgprintf("ES:NANDLoadFile->IOS_Read():%d\n", r );
		heap_free( 0, status );
		*Size = r;
		return NULL;
	}

	heap_free( 0, status );
	IOS_Close( fd );

	return data;
}
s32 NANDWriteFileSafe( char *pathdst, void *data, u32 size )
{
	//Create file in tmp folder and move it to destination
	char *path = (char*)heap_alloc_aligned( 0, 0x40, 32 );

	_sprintf( path, "/tmp/file.tmp" );

	s32 r = ISFS_CreateFile( path, 0, 3, 3, 3 );
	if( r == FS_EEXIST2 )
	{
		ISFS_Delete( path );
		r = ISFS_CreateFile( path, 0, 3, 3, 3 );
		if( r < 0 )
		{
			heap_free( 0, path );
			return r;
		}
	} else {
		if( r < 0 )
		{
			heap_free( 0, path );
			return r;
		}
	}

	s32 fd = IOS_Open( path, 2 );
	if( fd < 0 )
	{
		heap_free( 0, path );
		return r;
	}

	r = IOS_Write( fd, data, size );
	if( r < 0 || r != size )
	{
		IOS_Close( fd );
		heap_free( 0, path );
		return r;
	}

	IOS_Close( fd );

	r = ISFS_Rename( path, pathdst );
	if( r < 0 )
	{
		heap_free( 0, path );
		return r;
	}

	heap_free( 0, path );
	return r;
}

s32 Create_Nand_Cfg(char* from)
{

	size_t fnameln,clen,nlen;
	u32 buf_offset;
	u32 FileSel;
	s32 teller;			
	
	char* path = malloca( 0x40, 0x40 );
	char* Cpath = malloca( 0x80, 0x20);
	u32* FileCount = malloca( sizeof(u32), 0x20);
	u32* FileLn = malloca( sizeof(u32), 0x20);

	strcpy(path, "/sneek/NandPath.bin");
	*FileLn = 0x80;
	char* Npath = (char*)NANDLoadFile(path,FileLn);
	free (FileLn);
	if (Npath == NULL)
	{
		Npath = malloca(0x80, 0x20);
		Npath[0] = 0;
	}	
	else
	{
		Npath[0x7f] = 0;
		nlen = strlen(Npath);
		if (nlen > 0)
		{
			for (teller=nlen-1;teller>=0;teller--)
			{
				if ((Npath[teller] == 13)||(Npath[teller] == 10)||(Npath[teller] == 32))	
				{
					Npath[teller+1] = 0;
				}
				else
				{
					Npath[teller+1] = Npath[teller];
				}
			}
			Npath[0] = '/';
		}
	}
	nlen = strlen(Npath);
//	dbgprintf("ES:Create_Nand_Cfg Npath = %s\n",Npath);

/*
	free (Npath);
	free (FileCount);
	free (Cpath);
	free (path);
	return 0;
*/


//	strcpy(path, "/sneek/NandCfg.bin");
	//ISFS_Delete(path);
//	ISFS_CreateFile(path,0,0,0,0);

	char* Afrom = malloca( 0x80, 0x20);
	memset32(Afrom,0x00,0x80);
	memcpy(Afrom,from,0x80);
	//dbgprintf("Es:Afrom = %s \n",Afrom);
	//this will read the number of subfolders
	ISFS_ReadDir(Afrom, NULL, FileCount);
	dbgprintf("ES:cnc FileCount = %d\n",*FileCount);
	if (*FileCount > 20)
	{
		*FileCount = 20;
	}
	NandConfig* NandCfg = (NandConfig*)malloca( *FileCount * 0x80 + 0x10, 32 );
	char* FileBuf = (char *)malloca( *FileCount * 0x40, 32);
	memset32(FileBuf,0,*FileCount * 0x40);
	ISFS_ReadDir(Afrom, FileBuf, FileCount);
	free(Afrom);
	// nu nog copieren van FileBuf -> NandCfg
	//NandCfg->NandCnt = *FileCount;
	buf_offset = 0;
	FileSel = 0;
	NandCfg->NandSel = FileSel;
	fnameln = strlen(FileBuf);
	while (fnameln != 0)
	{
		//memcpy(NandCfg+FileSel * 0x80 + 0x10,FileBuf+buf_offset,fnameln+1);
		//memcpy(NandCfg+FileSel * 0x80 + 0x10 + 0x40,FileBuf+buf_offset,fnameln+1);
		
		// check for nandpath.bin match!
		strcpy(Cpath,from);
		clen = strlen(Cpath);
		Cpath[clen] = '/';
		memcpy(Cpath +clen + 1,FileBuf+buf_offset,fnameln+1);
		//dbgprintf("ES:Npath: %s\n",Cpath);
		
		//we don't need the first / to stay compatible
		memcpy(NandCfg->NandInfo[FileSel],Cpath+1,clen+fnameln+1);
		memcpy(NandCfg->NandInfo[FileSel]+0x40,Cpath+1,clen+fnameln+1);
		
		
		
		if (memcmp(Npath,Cpath,nlen)== 0)
		{
			NandCfg->NandSel = FileSel;
		}
		FileSel++;
		buf_offset+=fnameln;
		buf_offset+=1;
		if (FileSel < *FileCount)
		{
			fnameln = strlen(FileBuf+buf_offset);
		}
		else
		{
			fnameln = 0;
		}
	}

	// basically we should at least have *FileCount entries
	// if some of those are empty, FileSel is a better solution
	
	NandCfg->NandCnt = FileSel;
	
	// nu netjes saven, liefst met een beetje errorchecking
	// hier eventueel de file wissen
	// of 
	
	strcpy(path, "/sneek/NandCfg.bin");
	ISFS_Delete(path);
	NANDWriteFileSafe(path, NandCfg, FileSel * 0x80 + 0x10);
	
/*	
	
	
	ISFS_CreateFile(path,0,0,0,0);

	s32 fd = IOS_Open( path, 2 );
	//s32 r = IOS_Seek(fd,0,2);
	if (fd >= 0)
	{
		s32 r = IOS_Write( fd, NandCfg, *FileCount * 0x80 + 0x10);
		if (r < 0)
		{
			dbgprintf("ES:IOS_Write returning %d \n",r);
		}
		IOS_Close(fd);
	}
	else
	{
		dbgprintf("ES:IOS_Open(%s) returning %d \n",path,fd);
	}
	
*/	
	free(FileBuf);
	free(NandCfg);
	free (Npath);
	free (FileCount);
	free (Cpath);
	free (path);
	return FileSel;

}

void Save_Nand_Cfg(NandConfig* NandCfg)
{
	size_t nlen;
	
	char* path = malloca( 0x40, 0x40 );
	char* nand = malloca( 0x40, 0x40 );
	
	strcpy(path, "/sneek/NandPath.bin");
	nlen = strlen((const char*)(NandCfg->NandInfo[NandCfg->NandSel]));
	nlen &= 0x3f;
	memcpy(nand,NandCfg->NandInfo[NandCfg->NandSel],nlen+1);
	nand[0x3f] = 0;
	ISFS_Delete(path);
	NANDWriteFileSafe(path,nand, nlen+1);
	strcpy(path, "/sneek/NandCfg.bin");
	ISFS_Delete(path);
	NANDWriteFileSafe(path, NandCfg, NandCfg->NandCnt * 0x80 + 0x10);
	free(nand);
	free(path);
}