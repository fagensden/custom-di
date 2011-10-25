/*

SNEEK - SD-NAND/ES + DI emulation kit for Nintendo Wii

Copyright (C) 2009-2011	crediar
				   2011	OverjoY 
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
#include "dip.h"

static u32 error ALIGNED(32);
static u32 PartitionSize ALIGNED(32);
static u32 DIStatus ALIGNED(32);
/*** this being 1 forces an eject disk ***/
static u32 DICover ALIGNED(32);
static u32 ChangeDisc ALIGNED(32);
DIConfig *DICfg;
NandConfig *NandCfg;
GameConfig *GameCFG;
GameTitles *GT;

WBFSInfo *WBFSInf;
WBFSFileInfo *WBFSFInf;

static char GamePath[128];
static char WBFSFile[8];
static char WBFSPath[128];
//static char SuvolutionPath[128];

static u32 *KeyID ALIGNED(32);
static u32 KeyIDT ALIGNED(32) = 0;
static u8 *FSTable ALIGNED(32);

u32 ApploaderSize=0;
u32 DolSize=0;
u32 DolOffset=0;
u32 FSTableSize=0;
u32 FSTableOffset=0;
u32 DiscType=0;

u32 FCEntry=0;
FileCache FC[FILECACHE_MAX];

int verbose = 0;
u32 Partition = 0;
u32 Motor = 0;
u32 Disc = 0;
u64 PartitionOffset=0;
u32 GameHook=0;

/*** WBFS cache vars ***/
BlockCache BC[BLOCKCACHE_MAX];
FileSplits FS[FILESPLITS_MAX];
u32 splitcount=0;
u32 BCEntry=0;
u32 BCRead=0;

/*** WBFS image vars ***/
u16 start_loc=0;
u16 start_val=0;
u32 bl_shift=0;
u32 wbfs_sector_size=0;
u32 hdd_sector_size=0;
u32 wii_sector_size_s=0;
u32 wii_sector_size=0;
u32 max_wii_sec=0;
u32 max_wbfs_sec=0;
u32 blft_mask=0;
u32 game_part_offset=0;
u32 tmd_offset=0;
u32 tmd_size=0;
u32 data_offset=0;
u32 data_size=0;
u32 cert_offset=0;
u32 cert_size=0;

extern u32 FSMode;
u32 FMode = IS_FST;

extern s32 switchtimer;
extern int QueueID;
extern int requested_game;
extern char* cdiconfig ALIGNED (32);
extern char* cdiconfigpath ALIGNED (32);

/*** Debug loging: ***/
//#define DEBUG_CACHE
//#define DEBUG_GAMES
//#define DEBUG_PRINT_FILES
//#define DEBUG_KEY
//#define DEBUG_DVDSelectGameA
//#define DEBUG_DVDSelectGameB
//#define DEBUG_DVDSelectGameC
//#define DEBUG_DVDSelectGameD
//#define DEBUG_APPLOADER_STUFF
//#define DEBUG_WBFSREAD
//#define OUTP_DEC_WBFS_BUF
//#define OUTP_DEC_FST_BUF

static void Set_key2(void)
{
	u8 *TIK = (u8 *)halloca(0x2a4, 0x40);
	u8 *TitleID    = (u8*)malloca( 0x10, 0x20 );
	u8 *EncTitleKey = (u8*)malloca( 0x10, 0x20 );
	u8 *KeyIDT2 = (u8*)malloca( 0x10, 0x20 );

	WBFS_Read( game_part_offset, 0x2a4, TIK ); /*** Read the ticket ***/
	
	memset( TitleID, 0, 0x10 );
	memset( EncTitleKey, 0, 0x10 );
	memset( KeyIDT2, 0, 0x10);

	memcpy( TitleID, TIK+0x1DC, 8 );
	memcpy( EncTitleKey, TIK+0x1BF, 16 );

#ifdef DEBUG_KEY
	dbgprintf( " CDI:Key setting parameters \n");
	hexdump (TitleID,0x10);
	hexdump ( EncTitleKey,0x10 ); 
#endif
	
	vector *v = (vector*)halloca( sizeof(vector)*3, 32 );
	
	v[0].data = (u8 *)TitleID;
	v[0].len = 0x10;
	v[1].data = (u8 *)EncTitleKey;
	v[1].len = 0x10;
	v[2].data = (u8 *)KeyIDT2;
	v[2].len = 0x10;

	s32 ESHandle = IOS_Open("/dev/es", 0 );

	IOS_Ioctlv( ESHandle, 0x50, 2, 1, v );

    KeyIDT = *(u32*)(KeyIDT2);
	
	free( KeyIDT2);
    free( EncTitleKey );
    free( TitleID );
	hfree( TIK );
	hfree( v );
}

static u8 size_to_shift(u32 size)
{
	u8 ret = 0;
	while(size)
	{
		ret++;
		size >>= 1;
	}
	return ret - 1;
}

void InitCache( void )
{
	u32 count = 0;

	for( count=0; count < BLOCKCACHE_MAX; ++count )
	{
		BC[count].bl_num = 0xdeadbeef;
	}

	for( count=0; count < FILECACHE_MAX; ++count )
	{
		FC[count].File = 0xdeadbeef;
	}
	
	for( count=0; count < FILESPLITS_MAX; ++count )
	{
		FS[count].Offset = 0x00000001;
	}
}

void Rebuild_Disc_Usage_Table( void )
{
	u16 zero_pos=0xffff;
	u16	curr_pos=0;
	u32 block_gp=0;
	u32 i, j;
	for( i = start_loc; i < max_wbfs_sec; ++i )
	{
		if( WBFSInf->disc_usage_table[i] == 0 )
		{
			if( zero_pos == 0xffff )
				zero_pos = i;
				
			curr_pos = i;				
			block_gp += 0x4;
			
			for( j = zero_pos; j <= curr_pos; ++j )
				WBFSInf->disc_usage_table[j] = block_gp;			
		}
		else
		{
			zero_pos=0xffff;
			WBFSInf->disc_usage_table[i] = block_gp;
		}
		
	}	
}
u32 GetSystemMenuRegion( void )
{
	char *Path = (char*)malloca( 128, 32 );
	fstats* status = (fstats*)malloca(sizeof(fstats),0x40);
	u32 Region = EUR;
	strcpy( Path, "/title/00000001/00000002/content/title.tmd" );
	s32 fd = IOS_Open( Path, 1 );
	if( fd >= 0 )
	{
		IOS_Ioctl(fd,ISFS_IOCTL_GETFILESTATS,NULL,0,status,sizeof(fstats));
		char *TMD = (char*)malloca( status->Size, 32 );
		IOS_Read(fd,TMD,status->Size);
		Region = *(u16*)(TMD+0x1DC) & 0xF;
		dbgprintf("CDI:Region l = %d\n",status->Size); 
		IOS_Close(fd);
		free( TMD );
	}	
	else
	{
		dbgprintf("CDI:Region fail\n");
	}
	free( status );
	free( Path );
	return Region;
}

/*
u32 GetSystemMenuRegion2( void )
{
	char *Path = (char*)malloca( 128, 32 );
	u32 Region = EUR;
	
	strcpy( Path,"/sneek/nandcfg.bin" );
	s32 fd = DVDOpen( Path, DREAD );
	if( fd < 0 )
	{
		strcpy( Path, "/title/00000001/00000002/content/title.tmd" );
	}
	else
	{
		NandCfg = (NandConfig*)malloca( DVDGetSize( fd ), 32 );
		DVDRead( fd, NandCfg, DVDGetSize( fd ) );
		sprintf( Path, "/nands/%.63s/title/00000001/00000002/content/title.tmd", NandCfg->NandInfo[NandCfg->NandSel] );
		DVDClose( fd );
		free( NandCfg );
	}
	
	fd = DVDOpen( Path, DREAD );
	if( fd < 0 )
	{
		free( Path );
		return Region;		
	} 
	else 
	{
		char *TMD = (char*)malloca( DVDGetSize( fd ), 32 );

		DVDRead( fd, TMD, DVDGetSize( fd ) );
		Region = *(u16*)(TMD+0x1DC) & 0xF;

		DVDClose( fd );

		free( TMD );
	}
	
	free( Path );

	return Region;
}
*/
u32 DVDGetInstalledGamesCount( void )
{
	u32 GameCount = 0;

	/*** Check SD card for installed games (DML) ***/
	if( FSMode == SNEEK )
	{
		FSMode = UNEEK;
		GameCount = DVDGetInstalledGamesCount();
		FSMode = SNEEK;
	}

	char *Path = (char*)malloca( 128, 32 );
	splitcount = 0;

	strcpy( Path, "/games" );
	if( DVDOpenDir( Path ) == FR_OK )
	{
		while( DVDReadDir() == FR_OK )
		{
			GameCount++;
		}
	}
	
	strcpy( Path, "/wbfs" );
	if( DVDOpenDir( Path ) == FR_OK )
	{
		while( DVDReadDir() == FR_OK )
		{	
			if( DVDDirIsFile() )
			{
				if( strlen( DVDDirGetEntryName() ) == 11 && strncmp( DVDDirGetEntryName()+7, "wbfs", 4 ) == 0)
				{
					GameCount++;
				}
			}
			else
			{
				GameCount++;
			}			
		}
	}			

	free( Path );

#ifdef DEBUG_CACHE
	dbgprintf( "CDI:Found: %d rawgames\n", GameCount );
#endif

	return GameCount;	
}

u32 DVDVerifyGames( void )
{
	u32 i=0;
	
	char *Path = (char*)malloca( 128, 32 );

	while ( i < DICfg->Gamecount )
	{
		switch ( *(vu32*)(DICfg->GameInfo[i]+DI_MAGIC_OFF) )
		{	
			case 0x44534358: /*** FST Extracted ***/
			case 0xc2339f3d: /*** Gamecube DML ***/
				sprintf( Path, "/games/%.63s/sys/boot.bin", DICfg->GameInfo[i]+DVD_GAME_NAME_OFF );
				if ( *(vu32*)(DICfg->GameInfo[i]+DI_MAGIC_OFF) == 0xc2339f3d )
					FSMode = UNEEK;
					
				s32 fd = DVDOpen( Path, DREAD );
				if( fd >= 0 )
				{
					DVDClose ( fd );
					i++;
				}
				else 
				{
					free( Path );
					return 1;
				}
				if ( *(vu32*)(DICfg->GameInfo[i]+DI_MAGIC_OFF) == 0xc2339f3d )
					FSMode = SNEEK;
				break;
			case 0x57424653: /*** WBFS ***/
				sprintf( WBFSFile, "%s", DICfg->GameInfo[i] );
		
				if( strncmp( (char *)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF+6, ".wbfs", 5 ) == 0 )
					sprintf( WBFSPath, "/wbfs/%s.wbfs",  WBFSFile );			
				else			
					sprintf( WBFSPath, "/wbfs/%.63s/%s.wbfs", DICfg->GameInfo[i]+DVD_GAME_NAME_OFF, WBFSFile );
				
				fd = DVDOpen( WBFSPath, DREAD );
				if (fd>=0)
				{
					DVDClose ( fd );
					i++;
				}
				else
				{
					free( Path );
					return 1;
				}
				break;
			default:
				/*** how the hell did we got here? I don't know ***/
				free( Path );
				return 1;
				break;
		}
	}
	free( Path );
	return 0;		
}

s32 DVDUpdateCache( u32 ForceUpdate )
{
	u32 UpdateCache = ForceUpdate;
	u32 GameCount	= 0;
	u32 *RawGameCount = 0;
	u32 CurrentGame = 0;
	u32 i;
	u32 DMLite		= 0;
	s32 fres		= 0;

	size_t	plen;
	char *Path = (char*)malloca( 128, 32 );

/*** First check if file exists and create a new one if needed ***/
#ifdef DEBUG_CACHE
	dbgprintf("CDI:Timer1 %x \n",read32(HW_TIMER));
#endif
	//strcpy( Path, "/sneek/diconfig.bin" );
	s32 fd = DVDOpen( cdiconfig, FA_READ | FA_WRITE );
	if( fd < 0 )
	{
		/*** No diconfig.bin found, create a new one ***/
		fd = DVDOpen( cdiconfig, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );
		switch(fd)
		{
			case DVD_NO_FILE:
			{
				/*** In this case there is probably no /sneek folder ***/
				//strcpy( Path, "/sneek" );
				//DVDCreateDir( Path );
				
				//strcpy( Path, "/sneek/diconfig.bin" );
				//DVDCreateDir ( cdiconfigpath );
				
				fd = DVDOpen( cdiconfig, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );
				if( fd < 0 )
				{
					free( Path );
					return DI_FATAL;
				}

			} break;
			case DVD_SUCCESS:
				break;
			default:
			{
				free( Path );
				return DI_FATAL;
			} break;
		}
		/*** Create default config ***/
		DICfg->Gamecount= 0;
		DICfg->SlotID	= 0;	
		//DICfg->Config	= CONFIG_AUTO_UPDATE_LIST;
		DICfg->Config = 0;
		DICfg->Region = GetSystemMenuRegion();
		
		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );
		UpdateCache = 1;
	} 

	DVDSeek( fd, 0, 0 );

/*** Read current config and verify the diconfig.bin content ***/
	fres = DVDRead( fd, DICfg, DVD_CONFIG_SIZE );
	if( fres != DVD_CONFIG_SIZE )
	{
		//strcpy( Path,"/sneek/diconfig.bin" );
		DVDDelete(cdiconfig);
		free( Path );

		return DVDUpdateCache(1);
	}	
	
	DICfg->Region = GetSystemMenuRegion();
		
	DVDSeek( fd, 0, 0 );
	DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );
	
	/*** Read rest of config ***/

	/*** Cache count for size calc ***/
	GameCount = DICfg->Gamecount;
	free( DICfg );

	DICfg = (DIConfig*)malloca( GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE, 32 );

	DVDSeek( fd, 0, 0 );
	fres = DVDRead( fd, DICfg, GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE );
	if( fres != GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE )
	{
		UpdateCache = 1;
	}
	else
	{
		RawGameCount =(u32*)malloca(sizeof(u32),32);
		fres = DVDRead( fd, RawGameCount, sizeof(u32) );
		if (fres != sizeof(u32))
		{
			*RawGameCount = 0;
			UpdateCache = 1;
		}
	}
	if( DICfg->Config & CONFIG_AUTO_UPDATE_LIST )
	{	
		GameCount = DVDGetInstalledGamesCount();
#ifdef DEBUG_CACHE
		dbgprintf( "CDI:Timer2 = %x \n", read32( HW_TIMER ) );
#endif
		if (GameCount != *RawGameCount)
		{
			*RawGameCount = GameCount;
			UpdateCache = 1;
		} 
	}
	else
		GameCount = DICfg->Gamecount;

/*** No need to check if we are going to rebuild anyway ***/
	if( UpdateCache == 0 )
	if( DICfg->Config & CONFIG_AUTO_UPDATE_LIST )
	{	
		UpdateCache = DVDVerifyGames();
#ifdef DEBUG_CACHE
		dbgprintf("CDI:Timer3 %x result %d\n",read32(HW_TIMER),UpdateCache);
#endif
	}
	if( UpdateCache )
	{
		DVDClose(fd);
		
		//strcpy( Path, "/sneek/titles.txt" );	
		strcpy( Path, cdiconfigpath);
		plen = strlen(Path);
		strcpy( Path+plen,"/titles.txt" );
		
		fd = DVDOpen( Path, DREAD );
		if( fd >= 0 )
		{
			u32 Contentsize = DVDGetSize(fd);
			char *Content = (char *)malloca( Contentsize+1, 32 );
            Content[Contentsize]=0;
            DVDRead( fd, Content, Contentsize );
            DVDClose( fd );
			DVDDelete( Path );
			
			char *lbrk = strchr( Content, '\n' );
			char *TitleInfo = (char*)malloca( 0x60, 32 );
			u32 lastoff=0;
			u32 length=0;
			//strcpy( Path, "/sneek/titlecfg.bin" );
			strcpy( Path, cdiconfigpath);
			plen = strlen(Path);
			strcpy( Path+plen,"/titlecfg.bin" );
			
			
			DVDDelete( Path );
			fd = DVDOpen( Path, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );
			GT->EntryCount = 0;
			DVDWrite( fd, GT, 0x10 );
			
			while( lbrk != NULL )
			{
				length = ( lbrk-Content ) - lastoff;
				if( length > 73 )
					length = 73;

				memset( TitleInfo, 0 , 0x60 );				
				strncpy( TitleInfo, Content + lastoff + 1, length - 1 );					
				lastoff = lbrk-Content;
				DVDWrite( fd, TitleInfo, 0x60 );
				lbrk = strchr( lbrk+1, '\n' );
			}
			DVDClose( fd );
			free( Content );

			free( TitleInfo );
		}		
		
		//strcpy( Path, "/sneek/diconfig.bin" );
		
		DVDDelete( cdiconfig );
		fd = DVDOpen( cdiconfig, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );
		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );

		char *GameInfo = (char*)malloca( DVD_GAMEINFO_SIZE, 32 );
		char *mch;
		u8 *buf1 = (u8 *)halloca(0x04, 32);
		InitCache();
		splitcount = 0;	
		u32 cTitles = 0;

		//strcpy( Path, "/sneek/titlecfg.bin" );
		strcpy( Path, cdiconfigpath);
		plen = strlen(Path);
		strcpy( Path+plen,"/titlecfg.bin" );

		s32 fdct = DVDOpen( Path, DREAD );
		if( fdct >= 0 )
		{
			cTitles = DVDGetSize(fdct) / 0x60;
			GT = (GameTitles *)malloca( DVDGetSize(fdct), 32 );
			DVDRead( fdct, GT, DVDGetSize(fdct) );
			DVDClose( fdct );				
		}

		/*** Check on USB and on SD(DML) ***/
		for( i=0; i < 2; i++ )
		{
			strcpy( Path, "/games" );
			if( DVDOpenDir( Path ) == FR_OK )
			{
				while( DVDReadDir() == FR_OK )
				{
					if( DVDDirIsFile() )
						continue;
					
					if( strlen( DVDDirGetEntryName() ) > 63 )
					{
#ifdef DEBUG_CACHE
						dbgprintf( "CDI:Skipping to long folder entry: %s\n", DVDDirGetEntryName() );
#endif
						continue;
					}
					sprintf( Path, "/games/%.63s/sys/boot.bin", DVDDirGetEntryName() );
					s32 gi = DVDOpen( Path, FA_READ );
					if( gi >= 0 )
					{
						if( DVDRead( gi, GameInfo, DVD_GAMEINFO_SIZE ) == DVD_GAMEINFO_SIZE )
						{
						
							if( DMLite )
							{
								FSMode = SNEEK;
							}
							else
							{						
								strncpy( GameInfo+DI_MAGIC_OFF, "DSCX", 4 );
							}
							
							memcpy( GameInfo+DVD_GAME_NAME_OFF, DVDDirGetEntryName(), strlen( DVDDirGetEntryName() ) + 1 );

							DVDWrite( fd, GameInfo, DVD_GAMEINFO_SIZE );
							CurrentGame++;

							if( DMLite )
								FSMode = UNEEK;
						}

						DVDClose( gi );
					}
				}
			}
			
			strcpy( Path, "/wbfs" ); 
			if( DVDOpenDir( Path ) == FR_OK )
			{
				while( DVDReadDir() == FR_OK )
				{					
					if( strlen( DVDDirGetEntryName() ) > 63 )
					{
#ifdef DEBUG_CACHE
						dbgprintf( "CDI:Skipping to long folder entry: %s\n", DVDDirGetEntryName() );
#endif
						continue;
					}

					if( strlen( DVDDirGetEntryName() ) == 11 && strncmp( DVDDirGetEntryName()+6, ".wbfs", 5 ) == 0 )
					{
						sprintf( WBFSPath, "/wbfs/%s", DVDDirGetEntryName() );
						strncpy( WBFSFile, DVDDirGetEntryName(), 6 );
						WBFSFile[6] = 0;
					}
					else 
					{					
						if	( strlen(DVDDirGetEntryName()) == 6 ) 
						{
							strcpy ( WBFSFile, DVDDirGetEntryName() );
						}
						else 
						{					
							mch = strchr(DVDDirGetEntryName(), '_');
							if( mch-DVDDirGetEntryName() == 6 )
							{ 						
								strncpy( WBFSFile, DVDDirGetEntryName(), 6 );
								WBFSFile[6] = 0;
							}
							mch = strchr(DVDDirGetEntryName(), ']');
							if( mch-DVDDirGetEntryName()+1 == strlen(DVDDirGetEntryName()) ) 						
							{	
								strncpy( WBFSFile, DVDDirGetEntryName() +  strlen(DVDDirGetEntryName()) - 7, 6 );
								WBFSFile[6] = 0;
							}						
						}
						sprintf( WBFSPath, "/wbfs/%.63s/%s.wbfs", DVDDirGetEntryName(), WBFSFile );
					}
#ifdef DEBUG_CACHE						
					dbgprintf(" CDI:Set gamepath to: %s\n", WBFSPath );
#endif
					WBFS_Read( 0x218, 4, buf1 );
					if( *(vu32*)buf1 == 0x5d1c9ea3 )
					{
						WBFS_Read( 0x200, DVD_GAMEINFO_SIZE, GameInfo );
						strncpy( GameInfo+DI_MAGIC_OFF, "WBFS", 4 );
						
						if( cTitles )
						{
							char cmp[6];
							int ct = 0;
							for( ct=0; ct<cTitles; ++ct )
							{
								strncpy( cmp, GT->GameTitle[ct], 6 );
								if( strcmp( WBFSFile, cmp ) == 0 )
								{
									strncpy( GameInfo+DVD_REAL_NAME_OFF, GT->GameTitle[ct] + 9, 63 );
									break;
								}
							}
						}
						memcpy( GameInfo+DVD_GAME_NAME_OFF, DVDDirGetEntryName(), strlen( DVDDirGetEntryName() )+1 );
						DVDWrite( fd, GameInfo, DVD_GAMEINFO_SIZE );
						CurrentGame++;
#ifdef DEBUG_CACHE
						dbgprintf( "CDI:Saved: %d, %s\n", CurrentGame, WBFSPath );
#endif
					}
			
#ifdef DEBUG_CACHE
					else
					{
						dbgprintf( "CDI:Magic doesn't match! This isn't a Wii game: %s\n", WBFSPath );
					}
#endif				
				}
			}

			if( FSMode == UNEEK )
				break;

			FSMode = UNEEK;
			DMLite = 1;
		}

		if( DMLite )
			FSMode = SNEEK;

		free( GameInfo );
		hfree( buf1 );
		free( GT );
		DVDClose( fd );
		
		free( DICfg );
		//strcpy( Path, "/sneek/diconfig.bin" );	
		fd = DVDOpen( cdiconfig, DREAD );
		DICfg = (DIConfig *)malloca( DVDGetSize( fd ), 32 );
		DVDRead( fd, DICfg, DVDGetSize(fd) );
		DVDClose(fd);		
	
		char check1[64];
		char check2[64];
	
		u8 TitleC[DVD_GAMEINFO_SIZE];
	
		int k, j;
		for( j=0; j<CurrentGame-1; ++j )
		{
			for( k=j+1; k<CurrentGame; ++k )
			{			
				sprintf( check1, "%.63s", &DICfg->GameInfo[j][0x20] ) ;
				sprintf( check2, "%.63s", &DICfg->GameInfo[k][0x20] ) ;
				upperCase( check1 );
				upperCase( check2 );
				if( strcmp( check1, check2 )  > 0 )
				{				
					memcpy( TitleC, DICfg->GameInfo[j], DVD_GAMEINFO_SIZE );
					memcpy( DICfg->GameInfo[j], DICfg->GameInfo[k], DVD_GAMEINFO_SIZE );
					memcpy( DICfg->GameInfo[k], TitleC, DVD_GAMEINFO_SIZE );				
				}
			}
		}

		//DVDDelete( Path );
		DVDDelete(cdiconfig);
		//fd = DVDOpen( Path, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );	
		fd = DVDOpen( cdiconfig, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );	
		DVDWrite( fd, DICfg, CurrentGame * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE );
		DVDSeek( fd, CurrentGame * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE, 0 );
		DVDWrite( fd, RawGameCount, 4 );
		DICfg->Gamecount = CurrentGame;
		DICfg->Config = 0x00000000;
		DVDSeek( fd, 0, 0 );
		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );	
		
		free( RawGameCount);
	}

	DVDClose( fd );

/*** Read new config ***/

	free( DICfg );
//	strcpy( Path, "/sneek/diconfig.bin" );	
	fd = DVDOpen( cdiconfig, DREAD );
	DICfg = (DIConfig *)malloca( DVDGetSize( fd ), 32 );
	DVDRead( fd, DICfg, DVDGetSize(fd) );
	DVDClose(fd);	
		
	free( Path );	
	return DI_SUCCESS;
}

u32 DMLite = 0;
s32 DVDSelectGame( int SlotID )
{

	size_t plen;
	s32 ret;

#ifdef DEBUG_DVDSelectGameB	
	dbgprintf("DVDSelectGame 0.5sec timer expired at %x \n",read32(HW_TIMER));
#endif

	GameHook = 0;		
	InitCache();
	if( SlotID < 0 )
		SlotID = 0;

	if( SlotID >= DICfg->Gamecount )
		SlotID = 0;
		
	FSTable = (u8*)NULL;
	ChangeDisc = 1;
	DICover |= 1;
	
	char *str = (char *)malloca( 128, 32 );
	s32 fd=0;
		
	switch ( *(vu32*)(DICfg->GameInfo[SlotID]+DI_MAGIC_OFF) )
	{	
		case 0x44534358: /*** FST Extracted ***/
		case 0xc2339f3d: /*** Gamecube DML ***/
			FMode = IS_FST;				
			sprintf( GamePath, "/games/%.63s/", &DICfg->GameInfo[SlotID][0x60] );
			
			sprintf( str, "%ssys/apploader.img", GamePath );
			fd = DVDOpen( str, DREAD );
			
			if( (FSMode == SNEEK) && (fd < 0) )
			{
				FSMode = UNEEK;
				DMLite = 1;
				/*** Write boot info for DML ***/
				s32 bi = DVDOpen( "/games/boot.bin", FA_CREATE_ALWAYS|FA_WRITE );
				if( bi >= 0 )
				{
					DVDWrite( bi, &DICfg->GameInfo[SlotID][0x60], strlen( (char *)( &DICfg->GameInfo[SlotID][0x60] ) ) );
					DVDClose( bi );
				}
				fd = DVDOpen( str, DREAD );
			}
			ApploaderSize = DVDGetSize( fd ) >> 2;		
		break;
		case 0x57424653: /*** WBFS ***/
			FMode = IS_WBFS;
			
			if( KeyIDT > 0 )
				DestroyKey( KeyIDT );
				
			KeyIDT = 0;
		
			sprintf( WBFSFile, "%s", DICfg->GameInfo[SlotID] );
			
			if( strncmp( (char *)&DICfg->GameInfo[SlotID][0x60]+7, "wbfs", 4 ) == 0 )
			{
				strcpy( GamePath, "/wbfs/" );
				sprintf( WBFSPath, "/wbfs/%s.wbfs", WBFSFile );
			}
			else
			{
				sprintf( GamePath, "/wbfs/%.63s/", &DICfg->GameInfo[SlotID][0x60] );
				sprintf( WBFSPath, "/wbfs/%.63s/%s.wbfs", &DICfg->GameInfo[SlotID][0x60], WBFSFile );
			}
			
			fd = DVDOpen( WBFSPath, FA_READ);
			if( fd >= 0 )
			{			
				u8 *buf2 = (u8 *)halloca(0x480, 32);			

				u64 nOffset=0;			
				FS[0].Offset = nOffset+DVDGetSize( fd );
				FS[0].Size = DVDGetSize( fd );
				strcpy( FS[0].Path, WBFSPath );
				nOffset += FS[0].Size;
			
				splitcount=1;
				while(1)
				{			
					sprintf( str, "%s%s.wbf%d", GamePath, WBFSFile, splitcount );
					s32 fd_sf = DVDOpen( str, FA_READ );
					if( fd_sf >= 0 )
					{					
						FS[splitcount].Offset = nOffset+DVDGetSize( fd_sf );
						FS[splitcount].Size = DVDGetSize( fd_sf );
						strcpy( FS[splitcount].Path, str );
						nOffset += FS[splitcount].Size;					
						splitcount++;
					}
					else
					{
#ifdef DEBUG_DVDSelectGameA	
						if( splitcount > 1 && splitcount <= FILESPLITS_MAX )
							dbgprintf( "CDI:Game is splitted in %d wbfs files\n", splitcount );
						else if( splitcount > FILESPLITS_MAX )
							dbgprintf( "CDI:WARNING: Game is splitted in %d files. Max %d files are supported!\n", splitcount, FILESPLITS_MAX );
						else
							dbgprintf( "CDI:Game isn't splitted\n" );
#endif
						break;
					}
				
					DVDClose( fd_sf );
				}
				
				strcpy( str, "/title/mom.bin" );
				s32 fd_mom = DVDOpen( str, FA_READ );
				if( fd_mom < 0 && strncmp( WBFSFile, "R3Oxxx", 3 ) == 0 )
				{
					fd_mom = DVDOpen( str, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );
					DVDWrite( fd_mom, buf2, sizeof(u32) );
					ISFS_Rename( "/title/00010000", "/title/10010000" );
					if( DVDOpenDir( "/title/20010000" ) == FR_OK )
						ISFS_Rename( "/title/20010000", "/title/00010000" );
					else
						DVDCreateDir( "/title/00010000" );
						
					DVDClose( fd_mom );
				}
				else if( fd_mom >= 0 && strncmp( WBFSFile, "R3Oxxx", 3 ) != 0 )
				{					
					ISFS_Rename( "/title/00010000", "/title/20010000" );
					ISFS_Rename( "/title/10010000", "/title/00010000" );
					DVDClose( fd_mom );
					DVDDelete( str );
				}
				
				free( WBFSInf );
				free( WBFSFInf );				
				wii_sector_size_s = size_to_shift(0x8000);
                wii_sector_size = 1 << wii_sector_size_s;				
				max_wii_sec = 143432 * 2;				
				WBFSFInf = (WBFSFileInfo *)malloca( 0x10, 32 );				
				DVDSeek( fd, 0, 0 );
				DVDRead( fd, WBFSFInf, 0x10 );				
				max_wbfs_sec = max_wii_sec >> ( WBFSFInf->wbfs_sector_size_s - wii_sector_size_s );				
				WBFSInf = (WBFSInfo *)malloca( 0x100 + ( max_wbfs_sec * sizeof( u16 ) ), 32 );				
				DVDSeek( fd, 0x200, 0 );
				DVDRead( fd, WBFSInf, 0x100 + ( max_wbfs_sec * sizeof( u16 ) ) );
				DVDSeek( fd, 0x240000, 0 );
				DVDRead( fd, buf2, sizeof( u32 ) * 2 );				
				u32 part_info_cnt = *(vu32*)(buf2);
				u32 part_info_off = *(vu32*)(buf2+0x04);				
				DVDSeek( fd, 0x200000 + ( part_info_off << 2 ), 0 );
				DVDRead( fd, buf2, ( sizeof( u32 ) * 2 ) * part_info_cnt );
				u32 i, part_info_type;
				
				for( i=0; i<part_info_cnt; ++i )
				{
					game_part_offset = *(vu32*)(buf2+(i*0x08));
					part_info_type = *(vu32*)(buf2+0x04+(i*0x08));
					if( part_info_type == 0 )
						break;
				}				
 
				u16 blloc = game_part_offset>>( WBFSFInf->wbfs_sector_size_s-2 );
				bl_shift = WBFSFInf->wbfs_sector_size_s - WBFSFInf->hdd_sector_size_s;
				hdd_sector_size = 1 << WBFSFInf->hdd_sector_size_s;
				u16 valblloc = WBFSInf->disc_usage_table[blloc];
				wbfs_sector_size = 1 << WBFSFInf->wbfs_sector_size_s;
				blft_mask = ( wbfs_sector_size-1 )>>( WBFSFInf->hdd_sector_size_s );
				start_loc = blloc;
				start_val = valblloc;	
				game_part_offset = ( valblloc<<bl_shift ) * hdd_sector_size;				
				Rebuild_Disc_Usage_Table();
	
				WBFS_Read( game_part_offset, 0x480, buf2 );
			
				data_offset = *(vu32*)(buf2+0x2b8);
				data_size   = *(vu32*)(buf2+0x2bc);
				tmd_size    = *(vu32*)(buf2+0x2a4);
				tmd_offset  = *(vu32*)(buf2+0x2a8);
				cert_size   = *(vu32*)(buf2+0x2ac);
				cert_offset = *(vu32*)(buf2+0x2b0);	
				data_offset <<= 2;							
				
				hfree( buf2 );
			}
		break;
	}
	
	if( fd < 0 )
	{			
		if( DMLite )
			FSMode = SNEEK;

		DVDUpdateCache(1);

		free( str );
		ChangeDisc = 0;
		DICover |= 1;
		return DI_FATAL;
	}
	else {
		DMLite = 0;
	}
			
	if( DMLite )
		FSMode = SNEEK;	
		
	DVDClose( fd );
	free( str );

	/*** update di-config ***/
	//fd = DVDOpen( "/sneek/diconfig.bin", DWRITE );
	fd = DVDOpen( cdiconfig, DWRITE );
	
	if( fd >= 0 )
	{
		DICfg->SlotID = SlotID;
		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );
		DVDClose( fd );
	}
	return DI_SUCCESS;
}

unsigned char sig_fwrite[32] =
{
    0x94, 0x21, 0xFF, 0xD0,
	0x7C, 0x08, 0x02, 0xA6,
	0x90, 0x01, 0x00, 0x34,
	0xBF, 0x21, 0x00, 0x14, 
    0x7C, 0x9B, 0x23, 0x78,
	0x7C, 0xDC, 0x33, 0x78,
	0x7C, 0x7A, 0x1B, 0x78,
	0x7C, 0xB9, 0x2B, 0x78, 
} ;

unsigned char patch_fwrite[144] =
{
    0x7C, 0x85, 0x21, 0xD7, 0x40, 0x81, 0x00, 0x84, 0x3C, 0xE0, 0xCD, 0x00, 0x3D, 0x40, 0xCD, 0x00, 
    0x3D, 0x60, 0xCD, 0x00, 0x60, 0xE7, 0x68, 0x14, 0x61, 0x4A, 0x68, 0x24, 0x61, 0x6B, 0x68, 0x20, 
    0x38, 0xC0, 0x00, 0x00, 0x7C, 0x06, 0x18, 0xAE, 0x54, 0x00, 0xA0, 0x16, 0x64, 0x08, 0xB0, 0x00, 
    0x38, 0x00, 0x00, 0xD0, 0x90, 0x07, 0x00, 0x00, 0x7C, 0x00, 0x06, 0xAC, 0x91, 0x0A, 0x00, 0x00, 
    0x7C, 0x00, 0x06, 0xAC, 0x38, 0x00, 0x00, 0x19, 0x90, 0x0B, 0x00, 0x00, 0x7C, 0x00, 0x06, 0xAC, 
    0x80, 0x0B, 0x00, 0x00, 0x7C, 0x00, 0x04, 0xAC, 0x70, 0x09, 0x00, 0x01, 0x40, 0x82, 0xFF, 0xF4, 
    0x80, 0x0A, 0x00, 0x00, 0x7C, 0x00, 0x04, 0xAC, 0x39, 0x20, 0x00, 0x00, 0x91, 0x27, 0x00, 0x00, 
    0x7C, 0x00, 0x06, 0xAC, 0x74, 0x09, 0x04, 0x00, 0x41, 0x82, 0xFF, 0xB8, 0x38, 0xC6, 0x00, 0x01, 
    0x7F, 0x86, 0x20, 0x00, 0x40, 0x9E, 0xFF, 0xA0, 0x7C, 0xA3, 0x2B, 0x78, 0x4E, 0x80, 0x00, 0x20, 
};
unsigned char sig_iplmovie[] =
{
    0x4D, 0x50, 0x4C, 0x53,
	0x2E, 0x4D, 0x4F, 0x56,
	0x49, 0x45, 0x00, 
} ;

unsigned char patch_iplmovie[] =
{
    0x49, 0x50, 0x4C, 0x2E,
	0x45, 0x55, 0x4C, 0x41,
	0x00, 
} ;

s32 DVDLowRead( u32 Offset, u32 Length, void *ptr )
{
	s32 fd;
	u32 DebuggerHook=0;
	char Path[256];
	size_t plen;

	if( Offset < 0x110 )	/*** 0x440 ***/
	{
		sprintf( Path, "%ssys/boot.bin", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			return DI_FATAL;
		} else {
			u8 *rbuf = (u8*)halloca( 0x10, 32 );

			/*** read requested data ***/
			DVDSeek( fd, Offset<<2, 0 );
			DVDRead( fd, ptr, Length );
			
			/*** Read DOL/FST offset/sizes for later usage ***/
			DVDSeek( fd, 0x0420, 0 );
			DVDRead( fd, rbuf, 0x10 );
			DolOffset = *(vu32*)(rbuf);
			FSTableOffset = *(vu32*)(rbuf+0x04);
			FSTableSize = *(vu32*)(rbuf+0x08);

			hfree( rbuf );

			DolSize = FSTableOffset - DolOffset;
			FSTableSize <<= 2;

			DVDClose( fd );
			return DI_SUCCESS;
		}

	} else if( Offset < 0x910 )	/*** 0x2440 ***/
	{
		Offset -= 0x110;

		sprintf( Path, "%ssys/bi2.bin", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			return DI_FATAL;
		} else {
			DVDSeek( fd, Offset<<2, 0 );
			DVDRead( fd, ptr, Length );
			DVDClose( fd );

			/*** GC region patch ***/
			if( DiscType == DISC_DOL )
				*(vu32*)(ptr+0x18) = DICfg->Region;
			
			return DI_SUCCESS;
		}


	} else if( Offset < 0x910+ApploaderSize )	/*** 0x2440 ***/
	{
		Offset -= 0x910;

		sprintf( Path, "%ssys/apploader.img", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			return DI_FATAL;
		} else {
			DVDSeek( fd, Offset<<2, 0 );
			DVDRead( fd, ptr, Length );
			DVDClose( fd );
			return DI_SUCCESS;
		}
	} else if( Offset < DolOffset + DolSize )
	{
		Offset -= DolOffset;

		sprintf( Path, "%ssys/main.dol", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			return DI_FATAL;
		} else {
			DVDSeek( fd, Offset<<2, 0 );
			DVDRead( fd, ptr, Length );
			DVDClose( fd );

			int i;
			for( i=0; i < Length; i+=4 )
			{
				/*** Remote debugging and USBgecko debug printf doesn't work well at the same time! ***/
				if( DICfg->Config & CONFIG_DEBUG_GAME )
				{
					switch( DICfg->Config&HOOK_TYPE_MASK )
					{
						case HOOK_TYPE_VSYNC:	
						{	
							//VIWaitForRetrace(Pattern 1)
							if( *(vu32*)(ptr+i) == 0x4182FFF0 && *(vu32*)(ptr+i+4) == 0x7FE3FB78 )
								DebuggerHook = (u32)ptr + i + 8 * sizeof(u32);

						} break;
						case HOOK_TYPE_OSLEEP:	
						{	
							//OSSleepThread(Pattern 1)
							if( *(vu32*)(ptr+i+0) == 0x3C808000 &&
								*(vu32*)(ptr+i+4) == 0x38000004 &&
								*(vu32*)(ptr+i+8) == 0x808400E4 )
							{
								int j = 0;

								while( *(vu32*)(ptr+i+j) != 0x4E800020 )
									j+=4;

								DebuggerHook = (u32)ptr + i + j;
							}
						} break;
					}
					
				} else if( (DICfg->Config & CONFIG_PATCH_FWRITE) )
				{
					if( memcmp( (void*)(ptr+i), sig_fwrite, sizeof(sig_fwrite) ) == 0 )					
						memcpy( (void*)(ptr+i), patch_fwrite, sizeof(patch_fwrite) );
						
				}
				if( DICfg->Config & CONFIG_PATCH_MPVIDEO )
				{
					if( memcmp( (void*)(ptr+i), sig_iplmovie, sizeof(sig_iplmovie) ) == 0 )
						memcpy( (void*)(ptr+i), patch_iplmovie, sizeof(patch_iplmovie) );

				}
				if( DICfg->Config & CONFIG_PATCH_VIDEO )
				{
					if( *(vu32*)(ptr+i) == 0x3C608000 )
					{
						if( ((*(vu32*)(ptr+i+4) & 0xFC1FFFFF ) == 0x800300CC) && ((*(vu32*)(ptr+i+8) >> 24) == 0x54 ) )
							*(vu32*)(ptr+i+4) = 0x5400F0BE | ((*(vu32*)(ptr+i+4) & 0x3E00000) >> 5	);
		
					}
				}
			}

			if( GameHook != 0xdeadbeef )
			if( DebuggerHook && (DICfg->Config & CONFIG_DEBUG_GAME) )
			{
				GameHook = 0xdeadbeef;

				//strcpy(	Path, "/sneek/kenobiwii.bin" );
				strcpy( Path, cdiconfigpath);
				plen = strlen(Path);
				strcpy(	Path+plen, "/kenobiwii.bin" );
				
				s32 fd = IOS_Open( Path, 1 );
				if( fd < 0 )
					return DI_SUCCESS;

				u32 Size = IOS_Seek( fd, 0, 2 );
				IOS_Seek( fd, 0, 0 );

				/*** Read file to memory ***/
				s32 ret = IOS_Read( fd, (void*)0x1800, Size );
				if( ret != Size )
					return DI_SUCCESS;

				*(vu32*)((*(vu32*)0x1808)&0x7FFFFFFF) = !!(DICfg->Config & CONFIG_DEBUG_GAME_WAIT);

				memcpy( (void *)0x1800, (void*)0, 6 );

				u32 newval = 0x00018A8 - DebuggerHook;
					newval&= 0x03FFFFFC;
					newval|= 0x48000000;

				*(vu32*)(DebuggerHook) = newval;
			}

			return DI_SUCCESS;
		}
	} else if( Offset < FSTableOffset+(FSTableSize>>2) )
	{
		Offset -= FSTableOffset;

		sprintf( Path, "%ssys/fst.bin", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			return DI_FATAL;
		} else {
			DVDSeek( fd, (u64)Offset<<2, 0 );
			DVDRead( fd, ptr, Length );
			DVDClose( fd );

			if( FSTable == NULL )
				FSTable	= (u8*)ptr;

			return DI_SUCCESS;
		}
	} 
	else 
	{
		return Search_FST( Offset, Length, ptr, FST_READ );		
	}
	return DI_FATAL;
}
s32 DVDLowReadUnencrypted( u32 Offset, u32 Length, void *ptr )
{
	switch( Offset )
	{
		case 0x00:
		case 0x08:		/*** 0x20 ***/
		{
			if( FMode == IS_WBFS )
			{
				WBFS_Encrypted_Read( Offset, Length, ptr );

				return DI_SUCCESS;
			}
			else {
				char *str = (char *)malloca( 128, 32 );
				sprintf( str, "%ssys/boot.bin", GamePath );

				s32 fd = DVDOpen( str, FA_READ );
				if( fd >= 0 )
				{					
					DVDRead( fd, ptr, Length );
					DVDClose( fd );
					free( str );
					return DI_SUCCESS;					
				}

				free( str );
				return DI_FATAL;
			}
			
		} break;
		case 0x10000:		/*** 0x40000 ***/
		{
			memset( ptr, 0, Length );

			*(u32*)(ptr)	= 1;				/*** one partition ***/
			*(u32*)(ptr+4)	= 0x40020>>2;		/*** partition table info ***/

			return DI_SUCCESS;
		} break;
		case 0x10008:		/*** 0x40020 ***/
		{
			memset( ptr, 0, Length );

			*(u32*)(ptr)	= 0x03E00000;		/*** partition offset ***/
			*(u32*)(ptr+4)	= 0x00000000;		/*** partition type ***/

			return DI_SUCCESS;
		} break;
		case 0x00013800:		/*** 0x4E000 ***/
		{
			memset( ptr, 0, Length );

			*(u32*)(ptr)		= DICfg->Region;
			*(u32*)(ptr+0x1FFC)	= 0xC3F81A8E;
			return DI_SUCCESS;
		} break;
		default:
			while(1);
		break;
	}
}
s32 DVDLowReadDiscID( u32 Offset, u32 Length, void *ptr )
{
	if( FMode == IS_WBFS )
	{
		WBFS_Encrypted_Read( Offset, Length, ptr );
	}
	else {
		char *str = (char *)malloca( 128, 32 );
		sprintf( str, "%ssys/boot.bin", GamePath );
		s32 fd = DVDOpen( str, FA_READ );
		if( fd < 0 )
		{
			return DI_FATAL;
		} else {
			DVDRead( fd, ptr, Length );
			DVDClose( fd );
		}

		free( str );
	}

#ifdef DEBUG_APPLOADER_STUFF
	hexdump( ptr, Length );
#endif
	

	if( *(vu32*)(ptr+0x1C) == 0xc2339f3d )
	{
		DiscType = DISC_DOL;
	} else if( *(vu32*)(ptr+0x18) == 0x5D1C9EA3 )
	{
		DiscType = DISC_REV;
	} else {
		DiscType = DISC_INV;
	}
	
	return DI_SUCCESS;
}

void DIP_Fatal( char *name, u32 line, char *file, s32 error, char *msg )
{
	while(1);
}
int DIP_Ioctl( struct ipcmessage *msg )
{
	u8  *bufin  = (u8*)msg->ioctl.buffer_in;
	u32 lenin   = msg->ioctl.length_in;
	u8  *bufout = (u8*)msg->ioctl.buffer_io;
	u32 lenout  = msg->ioctl.length_io;
	s32 ret		= DI_FATAL;
	s32 fd		= 0;
	size_t	plen;
			
	//dbgprintf("CDI:Ioctl -> command = %d\n",msg->ioctl.command);
	switch(msg->ioctl.command)
	{
		/*** Added for slow harddrives ***/
		case DVD_CONNECTED:
		{
			if( HardDriveConnected )
				ret = 1;
			else
				ret = 0;
		} break;
		case DVD_WRITE_CONFIG:
		{
			u32 *vec = (u32*)msg->ioctl.buffer_in;
			char *name = (char*)halloca( 256, 32 );
			
			memcpy( DICfg, (u8*)(vec[0]), DVD_CONFIG_SIZE );

			//sprintf( name, "%s", "/sneek/diconfig.bin" );
			fd = DVDOpen( cdiconfig, FA_WRITE|FA_OPEN_EXISTING );
			if( fd < 0 )
			{
				DVDDelete( cdiconfig );
				fd = DVDOpen( cdiconfig, FA_WRITE|FA_CREATE_ALWAYS );
				if( fd < 0 )
				{
					ret = DI_FATAL;
					break;
				}
			}

			DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );
			DVDClose( fd );					

			ret = DI_SUCCESS;
			hfree( name );
		} break;
/*
		case DVD_WRITE_NANDCONFIG:
		{
			u32 *vec = (u32*)msg->ioctl.buffer_in;
			char *name = (char*)halloca( 256, 32 );
			
			memcpy( NandCfg, (u8*)(vec[0]), DVD_CONFIG_SIZE );

			//sprintf( name, "%s", "/sneek/nandcfg.bin" );

			strcpy( name, cdiconfigpath);
			plen = strlen(name);
			strcpy( name+plen,"/nandcfg.bin" );
				
				
			
			
			fd = DVDOpen( name, FA_WRITE|FA_OPEN_EXISTING );
			if( fd >= 0 )
			{
				DVDWrite( fd, NandCfg, DVD_CONFIG_SIZE );
				DVDClose( fd );	
				ret = DI_SUCCESS;
				hfree( name );
			}
			else
			{
				ret = DI_FATAL;
				hfree( name );
			}			
		} break;
*/

		case DVD_READ_INFO:
		{
			u32 *vec = (u32*)msg->ioctl.buffer_in;
			
//			if( vec[3] == 0 )
//			{
				//dbgprintf( "CDI:Read gameinfo\n" );
//				fd = DVDOpen( "/sneek/diconfig.bin", FA_READ );
				fd = DVDOpen( cdiconfig, FA_READ );

//			}
/*			
			if( vec[3] == 1 )
			{
				//dbgprintf( "CDI:Read nandinfo\n" );
				fd = DVDOpen( "/sneek/nandcfg.bin", FA_READ );
				strcpy( name, cdiconfigpath);
				plen = strlen(name);
				strcpy( name+plen,"/nandcfg.bin" );

			}
*/			
			if( fd < 0 )
			{
				ret = DI_FATAL;
			} 
			else 
			{
				DVDSeek( fd, vec[0], 0 );
				DVDRead( fd, (u8*)(vec[2]), vec[1] );
				DVDClose( fd );
				ret = DI_SUCCESS;
			}
		} break;
		
		case DVD_INSERT_DISC:
		{
			DICover &= ~1;
			ret = DI_SUCCESS;
		} break;
		case DVD_EJECT_DISC:
		{
			DICover |= 1;
			ret = DI_SUCCESS;
		} break;
		case DVD_GET_GAMECOUNT:	/*** Get Game count ***/
		{
			*(u32*)bufout = DICfg->Gamecount | 0x10000;
			ret = DI_SUCCESS;
		} break;
		case DVD_SELECT_GAME:
		{
			requested_game = *(u32*)(bufin);
#ifdef DEBUG_DVDSelectGameB	
			dbgprintf("Game select before 0.5 sec timer %x \n",read32(HW_TIMER));
#endif
			switchtimer = TimerCreate( 500000, 0, QueueID, 0xDEADDEAE );
			ret = DI_SUCCESS;

		} break;
		case DVD_SET_AUDIO_BUFFER:
		{
			memset32( bufout, 0, lenout );

			static u32 isASEnabled = 0;
			
			if( isASEnabled == 1)
			{
				write32( 0x0D806008, 0xA8000040 );
				write32( 0x0D80600C, 0 );
				write32( 0x0D806010, 0x20 );
				write32( 0x0D806018, 0x20 );
	
				write32( 0x0D806014, (u32)0 );

				write32( 0x0D806000, 0x3A );
	
				write32( 0x0D80601C, 3 );
				while( (read32( 0x0D806000 ) & 0x14) == 0 );

				write32( 0x0D806004, read32( 0x0D806004 ) );

				write32( 0x0D806008, 0xE4000000 | 0x10000 | 0x0A );
	
				write32( 0x0D80601C, 1 );

				while( read32(0x0D80601C) & 1 );

				isASEnabled = 1;
			}

			ret = DI_SUCCESS;
		} break;
		case 0x96:
		case DVD_REPORTKEY:
		{
			ret = DI_ERROR;
			error = 0x00052000;
		} break;
		case 0xDD:			/*** 0 ou***/
		{
			ret = DI_SUCCESS;
		} break;
		case 0x95:			/*** 0x20 out ***/
		{
			*(u32*)bufout = DIStatus;
			ret = DI_SUCCESS;
		} break;
		case 0x7A:			/*** 0x20 out ***/
		{
			*(u32*)bufout = DICover;

			if( ChangeDisc )
			{
				DICover &= ~1;
				ChangeDisc = 0;
			}

			ret = DI_SUCCESS;
		} break;
		case 0x86:			/*** 0 out ***/
		{
			ret = DI_SUCCESS;
		} break;
		case DVD_GETCOVER:	/*** 0x88 ***/
		{
			*(u32*)bufout = DICover;
			ret = DI_SUCCESS;
		} break;
		case 0x89:
		{
			DICover &= ~4;
			DICover |= 2;
			ret = DI_SUCCESS;
		} break;
		case DVD_IDENTIFY:
		{
			memset( bufout, 0, 0x20 );

			*(u32*)(bufout)	= 0x00000002;
			*(u32*)(bufout+4)	= 0x20070213;
			*(u32*)(bufout+8)	= 0x41000000;

			ret = DI_SUCCESS;
		} break;
		case DVD_GET_ERROR:	/*** 0xE0 ***/
		{
			*(u32*)bufout = error; 
			ret = DI_SUCCESS;
		} break;
		case 0x8E:
		{
			if( (*(u8*)(bufin+4)) == 0 )
				EnableVideo( 1 );
			else
				EnableVideo( 0 );

			ret = DI_SUCCESS;
		} break;
		case DVD_LOW_READ:	/*** 0x71, GC never calls this ***/
		{
			if( Partition )
			{
				if( *(u32*)(bufin+8) > PartitionSize )
				{
					ret = DI_ERROR;
					error = 0x00052100;
				} else {
					if( FMode == IS_WBFS )
					{
						u32 bl_offset, bl_length, i;
						u32 offset = *(u32*)(bufin+8);
						u32 length =((*(u32*)(bufin+4))+31)&(~31);

#ifdef DEBUG_PRINT_FILES							
						Search_FST( offset, length, NULL, DEBUG_READ );
#endif
						while( length ) 
						{

							bl_offset = offset % ( 0x7c00 >> 2 );
							bl_length = 0x7c00 - ( bl_offset << 2 );
							BCRead = 0;
		
							if ( bl_length > length ) 
							bl_length = length;
							
							if(BCRead != 1)
							{
								for( i=0; i < BLOCKCACHE_MAX; ++i )
								{
									if( BC[i].bl_num == 0xdeadbeef )
										continue;

									if( offset / (0x7c00 >> 2) == BC[i].bl_num )
									{
										ret = WBFS_OK;
										memcpy(bufout, BC[i].bl_buf + ( bl_offset << 2 ), bl_length);
										
										BCRead = 1;
									}
								}
							}						
						
							if( BCRead != 1 )
							{
								if( BCEntry >= BLOCKCACHE_MAX )
									BCEntry = 0;
										
								u16 blloc, valblloc;
									
								blloc = ( offset+0x3e00000 )>>( WBFSFInf->wbfs_sector_size_s-2 );		
								valblloc = WBFSInf->disc_usage_table[blloc];
						
								ret = WBFS_Read_Block( ( offset / (0x7c00 >> 2) ) - (valblloc * 0x10), BC[BCEntry].bl_buf );
								memcpy( bufout, BC[BCEntry].bl_buf + ( bl_offset << 2 ), bl_length );
										
								BC[BCEntry].bl_num = offset / ( 0x7c00 >> 2 );
								BCEntry++;						
							}

							bufout += bl_length;
							offset += bl_length >> 2;
							length -= bl_length;
						}

						if(ret != WBFS_OK) 
							ret = DI_FATAL;
					}
					else {
						ret = DVDLowRead( *(u32 *)( bufin+8 ), *(u32 *)( bufin+4 ), bufout );
					}
				}
			}
		} break;
		case DVD_READ_UNENCRYPTED:
		{
			if( DMLite )
				FSMode = UNEEK;

			if( DiscType == DISC_DOL )
			{
				ret = DVDLowRead( *(u32*)(bufin+8), *(u32*)(bufin+4), bufout );

			} else if(  DiscType == DISC_REV )
			{
				if( *(u32*)(bufin+8) > 0x46090000 )
				{
					error = 0x00052100;
					return DI_ERROR;

				} else {
					ret = DVDLowReadUnencrypted( *(u32*)(bufin+8), *(u32*)(bufin+4), bufout );
				}
			} else {
				/*** Invalid disc type! ***/
				ret = DI_FATAL;
			}

			if( DMLite )
				FSMode = SNEEK;

		} break;
		case DVD_READ_DISCID:
		{
			if( DMLite )
				FSMode = UNEEK;

			ret = DVDLowReadDiscID( 0, lenout, bufout );

			if( DMLite )
				FSMode = SNEEK;

		} break;
		case DVD_RESET:
		{
			DIStatus = 0x00;
			DICover  = 0x00;
			Partition = 0;

			ret = DI_SUCCESS;
		} break;
		case DVD_SET_MOTOR:
		{
			Motor = 0;
			ret = DI_SUCCESS;
		} break;
		case DVD_CLOSE_PARTITION:
		{
			Partition=0;
			ret = DI_SUCCESS;
		} break;
		case DVD_READ_BCA:
		{
			memset32( (u32*)bufout, 0, 0x40 );
			*(u32*)(bufout+0x30) = 0x00000001;

			ret = DI_SUCCESS;
		} break;
		case DVD_LOW_SEEK:
		{
			ret = DI_SUCCESS;
		} break;
		default:
			MessageQueueAck( (void *)msg, DI_FATAL);
			while(1);
		break;
	}

	/*** Reset error after a successful command ***/
	if( msg->command != 0xE0 && ret == DI_SUCCESS )
		error = 0;

	return ret;
}
int DIP_Ioctlv(struct ipcmessage *msg)
{
	vector *v	= (vector*)(msg->ioctlv.argv);
	s32 ret		= DI_FATAL;

	switch(msg->ioctl.command)
	{
		case DVD_CLOSE:
		{
			DVDClose( (u32)(v[0].data) );
			ret = DI_SUCCESS;
		} break;
		case DVD_WRITE:
		{
			ret = DVDWrite( (u32)(v[0].data), (u8*)(v[1].data), v[1].len );
		} break;
		case DVD_READ:
		{
			ret = DVD_FATAL;
		} break;
		case DVD_OPEN:
		{
			s32 fd = DVDOpen( (char*)(v[0].data), FA_WRITE|FA_READ|FA_CREATE_ALWAYS );
			if( fd < 0 )
			{
				ret = DI_FATAL;
				break;
			}
			ret = fd;
		} break;
		case DVD_OPEN_PARTITION:
		{
			/*if( Partition == 1 )
			{
				DIP_Fatal( "DVDLowOpenPartition", __LINE__, __FILE__, 0, "Partition already open!");
			}*/

			PartitionOffset = *(u32*)(v[0].data+4);
			PartitionOffset <<= 2;

			u8 *TMD;
			u8 *TIK;
			u8 *CRT;
			u8 *hashes = (u8*)halloca( sizeof(u8)*0x14, 32 );
			u8 *buffer = (u8*)halloca( 0x40, 32 );
			memset( buffer, 0, 0x40 );

			char *str = (char*)halloca( 0x40, 32 );
			
			TIK = (u8*)halloca( 0x2a4, 32 );
			
			if( FMode == IS_WBFS )
			{
				ret = WBFS_Read( game_part_offset, 0x2a4, TIK );
				if ( ret != WBFS_OK)
					ret = DI_FATAL;			
			}
			else {
				sprintf( str, "%sticket.bin", GamePath );

				s32 fd = DVDOpen( str, FA_READ );
				if( fd < 0 )
				{
					ret = DI_FATAL;
					break;
				} else {
					ret = DVDRead( fd, TIK, 0x2a4 );

					if( ret != 0x2A4 )
						ret = DI_FATAL;

					DVDClose( fd );
				}
			}
			
			if( ret != DI_FATAL )
			{
				((u32*)buffer)[0x04] = (u32)TIK;
				((u32*)buffer)[0x05] = 0x2a4;
				sync_before_read(TIK, 0x2a4);
			}
			
			if( FMode == IS_WBFS )
			{
				TMD = (u8*)halloca( tmd_size, 32 );
			
				ret = WBFS_Read( game_part_offset + (tmd_offset << 2), tmd_size, TMD );
				if( ret != WBFS_OK )
				{
					ret = DI_FATAL;
					break;
				} else {				
					((u32*)buffer)[0x06] = (u32)TMD;
					((u32*)buffer)[0x07] = tmd_size;
					sync_before_read(TMD, (tmd_size+31)&(~31));

					PartitionSize = (u32)((*(u64*)(TMD+0x1EC)) >> 2 );

					if( v[3].data != NULL )
					{
						memcpy( v[3].data, TMD, tmd_size );
						sync_after_write( v[3].data, tmd_size );
					}				
				}
			}
			else {
				sprintf( str, "%stmd.bin", GamePath );
			
				s32 fd = DVDOpen( str, FA_READ );
				if( fd < 0 )
				{
					ret = DI_FATAL;
					break;
				} else {
					u32 asize = (DVDGetSize(fd)+31)&(~31);
					TMD = (u8*)halloca( asize, 32 );
					memset( TMD, 0, asize );
					ret = DVDRead( fd, TMD, DVDGetSize(fd) );
					if( ret != DVDGetSize(fd) )
					{
						ret = DI_FATAL;
						DVDClose( fd );
						break;
					} 

					((u32*)buffer)[0x06] = (u32)TMD;
					((u32*)buffer)[0x07] = DVDGetSize(fd);
					sync_before_read(TMD, asize);

					PartitionSize = (u32)((*(u64*)(TMD+0x1EC)) >> 2 );

					if( v[3].data != NULL )
					{
						memcpy( v[3].data, TMD, DVDGetSize(fd) );
						sync_after_write( v[3].data, DVDGetSize(fd) );
					}

					DVDClose( fd );
				}
			}
			
			CRT = (u8*)halloca( 0xA00, 32 );
			
			if( FMode == IS_WBFS )
			{
				ret = WBFS_Read( game_part_offset + (cert_offset << 2), 0xA00, CRT );
				if(  ret != WBFS_OK )
					ret = DI_FATAL;
			}
			else {
				sprintf( str, "%scert.bin", GamePath );
				s32 fd = DVDOpen( str, FA_READ );
				if( fd < 0 )
				{
					ret = DI_FATAL;
					break;
				} else {				
					ret = DVDRead( fd, CRT, 0xA00 );
					if( ret != 0xA00 )
						ret = DI_FATAL;
				
					DVDClose( fd );
				}
			}

			if( ret != DI_FATAL )
			{
				((u32*)buffer)[0x00] = (u32)CRT;
				((u32*)buffer)[0x01] = 0xA00;
				sync_before_read(CRT, 0xA00);
			}			

			hfree( str );

			KeyID = (u32*)malloca( sizeof( u32 ), 32 );


			((u32*)buffer)[0x02] = (u32)NULL;
			((u32*)buffer)[0x03] = 0;

			((u32*)buffer)[0x08] = (u32)KeyID;
			((u32*)buffer)[0x09] = 4;
			((u32*)buffer)[0x0A] = (u32)hashes;
			((u32*)buffer)[0x0B] = 20;

			sync_before_read(buffer, 0x40);

			s32 ESHandle = IOS_Open("/dev/es", 0 );

			s32 r = IOS_Ioctlv( ESHandle, 0x1C, 4, 2, buffer );

			IOS_Close( ESHandle );

			if( FMode == IS_FST )
			{
				*(u32*)(v[4].data) = 0x00000000;
				free( KeyID );
			}

			hfree( buffer );
			hfree( TIK );
			hfree( CRT );
			hfree( TMD );

			if( r < 0 )
			{
				DIP_Fatal( "DVDLowOpenPartition", __LINE__, __FILE__, r, "ios_ioctlv() failed!");
			}
			
			ret = DI_SUCCESS;

			Partition=1;
		} break;
		default:
			MessageQueueAck( (struct ipcmessage *)msg, DI_FATAL);
			while(1);
		break;
	}

	/*** Reset error after a successful command ***/
	if( ret == DI_SUCCESS )
		error = 0;

	return ret;
}

s32 WBFS_Read( u64 offset, u32 length, void *ptr ) 
{
	if( !splitcount )
	{
		if( !( strcmp( WBFSPath, FS[0].Path ) == 0 ) ) 
		{
			if( FC[0].File != 0xdeadbeef )
			{
				DVDClose( FC[0].File );
				FC[0].File = 0xdeadbeef;
			}
			
			FC[0].File = DVDOpen( WBFSPath, DREAD );
			if( FC[0].File < 0 )
			{
				FC[0].File = 0xdeadbeef;

				error = 0x031100;
				return DI_FATAL;
			}
			else 
			{
				FC[0].Size = DVDGetSize( FC[0].File );
				FC[0].Offset = 0;
				strcpy( FS[0].Path, WBFSFile );
						
				DVDSeek( FC[0].File, offset, 0 );
				DVDRead( FC[0].File, ptr, length );
				return WBFS_OK;
			}
		}
		else 
		{
			DVDSeek( FC[0].File, offset, 0 );
			DVDRead( FC[0].File, ptr, length );
			return WBFS_OK;
		}
		error = 0x31100;
		return DI_FATAL;
	}
	int i, j;
		
	while(length)
	{
		for( i=0; i<FILESPLITS_MAX; ++i )
		{
			if( FS[i].Offset == 0xdeadbeef )
				continue;
				
			if( offset < FS[i].Offset && offset > ( FS[i].Offset - FS[i].Size ) )
			{
#ifdef DEBUG_WBFSREAD
				dbgprintf( "CDI:Found start offset in splitfile: %d\n", i );
				
#endif
				u32 doread = 0;
				for( j=0; j<FILECACHE_MAX; ++j )
				{
					if( FC[j].File == 0xdeadbeef )
					continue;						
					
					if( offset >= FC[j].Offset && offset < ( FC[j].Offset + FC[j].Size ) )
					{
#ifdef DEBUG_WBFSREAD
						dbgprintf( "CDI:Found splitfile in cache: %d\n", j );
#endif
						u32 readsize = length;
						u64 nOffset = offset-FC[j].Offset;
#ifdef DEBUG_WBFSREAD
						dbgprintf( "CDI:Block offset: 0x%08x\n", (u32)nOffset );
#endif	
						if( nOffset + readsize > FS[i].Offset )
							readsize = FS[i].Size - nOffset;
							
						DVDSeek( FC[j].File, nOffset, 0 );
						DVDRead( FC[j].File, ptr, readsize );
						
						offset += readsize;
						ptr += readsize;
						length -= readsize;	
						doread = 1;
#ifdef DEBUG_WBFSREAD
						dbgprintf( "CDI:Read: 0x%08x - Todo: 0x%08x\n", readsize, length );
#endif
						if( length <= 0 )
							return WBFS_OK;
							
					}					
				}
				
				if( !doread )
				{
					if( FCEntry >= FILECACHE_MAX )
						FCEntry = 0;
#ifdef DEBUG_WBFSREAD						
					dbgprintf( "CDI:Splitfile not found in cache adding it to cache: %d\n", FCEntry );
#endif

					if( FC[FCEntry].File != 0xdeadbeef )
					{
						DVDClose( FC[FCEntry].File );
						FC[FCEntry].File = 0xdeadbeef;
					}
					
					FC[FCEntry].File = DVDOpen( FS[i].Path, DREAD );
					if( FC[FCEntry].File < 0 )
					{
						FC[FCEntry].File = 0xdeadbeef;

						error = 0x031100;
						return DI_FATAL;
					}
					else 
					{
						u32 readsize = length;
						FC[FCEntry].Size	= FS[i].Size;
						FC[FCEntry].Offset	= FS[i].Offset-FS[i].Size;
						
						u64 nOffset = offset-FC[FCEntry].Offset;
#ifdef DEBUG_WBFSREAD
						dbgprintf( "CDI:Block offset: 0x%08x\n", (u32)nOffset );
#endif
						if( nOffset + readsize > FS[i].Offset )
							readsize = FS[i].Size - nOffset;
						
						
						DVDSeek( FC[FCEntry].File, nOffset, 0 );
						DVDRead( FC[FCEntry].File, ptr, length );
						
						offset += readsize;
						ptr += readsize;
						length -= readsize;
#ifdef DEBUG_WBFSREAD
						dbgprintf( "CDI:Read: 0x%08x - Todo: 0x%08x\n", readsize, length );
#endif							
						FCEntry++;
						
						if( length <= 0 )
							return WBFS_OK;
					}
				}				
			}
		}		
	}
	error = 0x31100;
	return DI_FATAL;
}

s32 WBFS_Read_Block( u64 block, void *ptr ) 
{
	u8 *bufrb = (u8 *)malloca(0x8000, 0x40);
	u8 *iv = (u8 *)malloca(0x10, 0x40);
	u64 offset;

	if( KeyIDT == 0 )
		Set_key2();		
	
	offset = game_part_offset + data_offset + (0x8000 * block);
	
	WBFS_Read( offset, 0x8000, bufrb );
	memcpy(iv, bufrb + 0x3d0, 16);

	aes_decrypt_( KeyIDT, iv, bufrb + 0x400, 0x7c00, bufrb);	
	memcpy(ptr, bufrb , 0x7c00);
	
	free(bufrb);
	free(iv);
	
	return WBFS_OK;
}

s32 WBFS_Encrypted_Read( u32 offset, u32 length, void *ptr)  
{
	u32 bl_offset, bl_length;

	u16 blloc, valblloc;

	while(length) {
		bl_offset = offset % (0x7c00 >> 2);
		bl_length = 0x7c00 - (bl_offset << 2);
		
		blloc = ( offset+0x3e00000 )>>( WBFSFInf->wbfs_sector_size_s-2 );		
		valblloc = WBFSInf->disc_usage_table[blloc];
		
		if (bl_length > length) 
			bl_length = length;

		WBFS_Read_Block( ( offset / (0x7c00 >> 2) ) - (valblloc * 0x10), BC[0].bl_buf );
		memcpy(ptr, BC[0].bl_buf + (bl_offset << 2), bl_length);		

		ptr += bl_length;
		offset += bl_length >> 2;
		length -= bl_length;
	}
	return WBFS_OK;
}

/*s32 WBFS_Decrypted_Write( char *path, char *filename, u32 offset, u32 length, u32 fst)
{
	u8 *bufrw = (u8 *)malloca(0x7c00, 0x40);
	u32 bl_offset, bl_length;
	char *str = (char *)malloca( 128, 32 );
	sprintf( str, "%s/%s", path, filename );
	DVDCreateDir( path );
	s32 writefd = DVDOpen( str, FA_WRITE|FA_READ|FA_CREATE_ALWAYS );
	
	while(length) {
		bl_offset = offset % (0x7c00 >> 2);
		bl_length = 0x7c00 - (bl_offset << 2);
		
		if (bl_length > length) 
			bl_length = length;
		
		WBFS_Read_Block( offset / (0x7c00 >> 2), bufrw, fst);
		DVDWrite( writefd, bufrw + (bl_offset << 2), bl_length);
		
		offset += bl_length >> 2;
		length -= bl_length;
	}
	DVDClose(writefd);
	free(bufrw);
	free(str);
	return WBFS_OK;
}*/

s32 Search_FST( u32 Offset, u32 Length, void *ptr, u32 mode )
{
	char Path[256];
	char File[64];
	
	u32 i,j;

	if( FSTable == NULL )
		FSTable	= (u8 *)( ( *(vu32 *)0x38 ) & 0x7FFFFFFF );	

	if( mode == FST_READ )
	{
		for( i=0; i < FILECACHE_MAX; ++i )
		{
			if( FC[i].File == 0xdeadbeef )
				continue;

			if( Offset >= FC[i].Offset )
			{
				u64 nOffset = ((u64)(Offset-FC[i].Offset)) << 2;
				if( nOffset < FC[i].Size )
				{
					DVDSeek( FC[i].File, nOffset, 0 );
					DVDRead( FC[i].File, ptr, ((Length)+31)&(~31) );
					return DI_SUCCESS;
				}
			}
		}
	}

	u32 Entries = *(u32*)(FSTable+0x08);
	char *NameOff = (char*)(FSTable + Entries * 0x0C);
	FEntry *fe = (FEntry*)(FSTable);

	u32 Entry[16];
	u32 LEntry[16];
	u32 level=0;

	for( i=1; i < Entries; ++i )
	{
		if( level )
		{
			while( LEntry[level-1] == i )
			{
				level--;
			}
		}

		if( fe[i].Type )
		{
			if( fe[i].NextOffset == i+1 )
				continue;

			Entry[level] = i;
			LEntry[level++] = fe[i].NextOffset;
			if( level > 15 )
				break;
		} else {
		
			switch( mode )
			{
				//case WBFS_CONF:
				//{
					/*memset( Path, 0, 256 );	
					memset( File, 0, 64 );
					
					sprintf( Path, "/games/%s/files", WBFSFile );
					DVDCreateDir( Path );
					if( level )
						sprintf( Path, "/games/%s/files/", WBFSFile );
					
					for( j=0; j<level; ++j )
					{
						if( j )
							Path[strlen(Path)] = '/';

						memcpy( Path+strlen(Path), NameOff + fe[Entry[j]].NameOffset, strlen(NameOff + fe[Entry[j]].NameOffset ) );
						DVDCreateDir( Path );
					}

					memcpy( File, NameOff + fe[i].NameOffset, strlen(NameOff + fe[i].NameOffset) );

					if( WBFS_Decrypted_Write( Path, File, fe[i].FileOffset, fe[i].FileLength, FST_READ) )
						continue;*/
						
				//} break;
					
				case DEBUG_READ:
				case FST_READ:
				{	
					if( Offset >= fe[i].FileOffset )
					{
						u64 nOffset = ((u64)(Offset-fe[i].FileOffset)) << 2;
						if( nOffset < fe[i].FileLength )
						{
							memset( Path, 0, 256 );					
							sprintf( Path, "%sfiles/", GamePath );

							for( j=0; j<level; ++j )
							{
								if( j )
									Path[strlen(Path)] = '/';

								memcpy( Path+strlen(Path), NameOff + fe[Entry[j]].NameOffset, strlen(NameOff + fe[Entry[j]].NameOffset ) );
							}
							if( level )
								Path[strlen(Path)] = '/';						
						
							memcpy( Path+strlen(Path), NameOff + fe[i].NameOffset, strlen(NameOff + fe[i].NameOffset) );

							if( mode == FST_READ )
							{
							
								if( FCEntry >= FILECACHE_MAX )
								FCEntry = 0;

								if( FC[FCEntry].File != 0xdeadbeef )
								{
									DVDClose( FC[FCEntry].File );
									FC[FCEntry].File = 0xdeadbeef;
								}
							
								Asciify( Path );
							
							
								FC[FCEntry].File = DVDOpen( Path, DREAD );
								if( FC[FCEntry].File < 0 )
								{
									FC[FCEntry].File = 0xdeadbeef;

									error = 0x031100;
									return DI_FATAL;
								} 
								else 
								{

									FC[FCEntry].Size	= fe[i].FileLength;
									FC[FCEntry].Offset	= fe[i].FileOffset;

									DVDSeek( FC[FCEntry].File, nOffset, 0 );
									DVDRead( FC[FCEntry].File, ptr, Length );
							
									FCEntry++;

									return DI_SUCCESS;
								}
							}
							else
							{
#ifdef DEBUG_PRINT_FILES
								dbgprintf( "DEBUG:Reading file: %s\n", Path );
#endif
								return DI_SUCCESS;
							}
						}
					}
				} break;
				default:
					while(1);
				break;
			}
		}
	}
	if( mode == WBFS_CONF )
		return DI_SUCCESS;
		
	else
	 return DI_FATAL;
}
