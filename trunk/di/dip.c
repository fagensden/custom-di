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
//this being 1 forces an eject disk
static u32 DICover ALIGNED(32);
static u32 ChangeDisc ALIGNED(32);
DIConfig *DICfg;

static char GamePath[64];
static char WBFSFile[6];
static char WBFSPath[64];
static char CacheFile[128]={0};
static char CacheFileb[128]={0};
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

// WBFS cache vars
s32 Cachefd=0;
s32 Cachefdb=0;
BlockCache BC[BLOCKCACHE_MAX];
u32 BCEntry=0;
u32 BCRead=0;

u8 wbfs_key[16];
u8 iv[16];
u32 maxblock=0;
u32 lastblock=0;
u64 wbfs_len=0;
u32 dline=0;
u32 debline=0;
u32 wbfstofst=0;

// WBFS image vars
u32 game_part_offset=0;
u32 tmd_offset=0;
u32 tmd_size=0;
u32 fst_offset=0;
u32 fst_size=0;
u32 data_offset=0;
u32 data_size=0;
u32 cert_offset=0;
u32 cert_size=0;
u32 dol_offset=0;

extern u32 FSMode;
u32 FMode = IS_FST;

extern s32 switchtimer;
extern int QueueID;
extern int requested_game;

static void Set_key2(void)
{
	u8 *TIK = (u8 *)halloca(0x2a4, 0x40);
	u8 *TitleID    = (u8*)malloca( 0x10, 0x20 );
	u8 *EncTitleKey = (u8*)malloca( 0x10, 0x20 );
	u8 *KeyIDT2 = (u8*)malloca( 0x10, 0x20 );

	s32 r = WBFS_Read( game_part_offset, 0x2a4, TIK ); // Read the ticket
	if(r < 0) 
	{
    	dbgprintf("DIP:Couldn't read ticket: %d\n", r);
		free ( KeyIDT2);
    	free ( EncTitleKey);
    	free ( TitleID);
    	hfree( TIK );
    	return;
	}
	
	memset( TitleID, 0, 0x10 );
	memset( EncTitleKey, 0, 0x10 );
	memset( KeyIDT2, 0, 0x10);

	memcpy( TitleID, TIK+0x1DC, 8 );
	memcpy( EncTitleKey, TIK+0x1BF, 16 );

	dbgprintf(" DI key setting parameters \n");
	hexdump (TitleID,0x10);
	hexdump ( EncTitleKey,0x10 ); 
	
	vector *v = (vector*)halloca( sizeof(vector)*3, 32 );
	
	v[0].data = (u8 *)TitleID;
	v[0].len = 0x10;
	v[1].data = (u8 *)EncTitleKey;
	v[1].len = 0x10;
	v[2].data = (u8 *)KeyIDT2;
	v[2].len = 0x10;

	s32 ESHandle = IOS_Open("/dev/es", 0 );

	r = IOS_Ioctlv( ESHandle, 0x50, 2, 1, v );
	if( r < 0)
		dbgprintf("DI:ioctlv_es_setkey :%d\n", r );
   
	dbgprintf("DI:KeyIDT = %d\n",*(u32*)(KeyIDT2));

    KeyIDT = *(u32*)(KeyIDT2);
	
	free( KeyIDT2);
    free( EncTitleKey );
    free( TitleID );
	hfree( TIK );
	hfree( v );
}

void Asciify( char *str )
{
	int i=0;
	for( i=0; i < strlen(str); i++ )
		if( str[i] < 0x20 || str[i] > 0x7F )
			str[i] = '_';
}
u32 GetSystemMenuRegion( void )
{
	char *Path = (char*)malloca( 128, 32 );
	u32 Region = EUR;

	sprintf( Path, "/title/00000001/00000002/content/title.tmd" );
	s32 fd = IOS_Open( Path, DREAD );
	if( fd < 0 )
	{
		free( Path );
		return Region;
	} else {
		u32 size = IOS_Seek( fd, 0, 2 );
		char *TMD = (char*)malloca( size, 32 );

		if( IOS_Read( fd, TMD, size ) == size )
			Region = *(u16*)(TMD+0x1DC) & 0xF;


		IOS_Close( fd );

		free( TMD );
	}
	
	free( Path );

	return Region;
}

u32 DVDGetInstalledGamesCount( void )
{
	u32 GameCount = 0;
	u8 *buf1 = (u8 *)halloca(0x04, 32);

	//Check SD card for installed games (DML)
	if( FSMode == SNEEK )
	{
		FSMode = UNEEK;
		GameCount = DVDGetInstalledGamesCount();
		FSMode = SNEEK;
	}

	char *Path = (char*)malloca( 128, 32 );
	char *mch;

	sprintf( Path, "/games" );
	if( DVDOpenDir( Path ) == FR_OK )
	{
		while( DVDReadDir() == FR_OK )
		{
			if( DVDDirIsFile() )
				continue;

			sprintf( Path, "/games/%.31s/sys/boot.bin", DVDDirGetEntryName() );
			s32 fd = DVDOpen( Path, FA_READ );
			if( fd >= 0 )
			{
				GameCount++;
				DVDClose(fd);
			}
		}
	}
	
	sprintf( Path, "/wbfs" );
	if( DVDOpenDir( Path ) == FR_OK )
	{
		while( DVDReadDir() == FR_OK )
		{	
			if( strlen( DVDDirGetEntryName() ) > 31 ) continue;
			if( strlen( DVDDirGetEntryName() ) == 11 && strncmp(DVDDirGetEntryName()+7, "wbfs", 4 ) == 0)
			{
				sprintf( WBFSPath, "/wbfs/%.11s", DVDDirGetEntryName() );				
				WBFS_Read( 0x240000, 4, buf1 );
					
				if(*(vu32*)buf1 == 0x00000001) // For now skip games with update partition
				{				
					s32 fd = DVDOpen( WBFSPath, FA_READ );
					if( fd >= 0 )
					{					
						GameCount++;
						DVDClose(fd);
					}
				}
				continue;
			}
				
			if( strlen(DVDDirGetEntryName()) == 6 ) {
				strcpy ( WBFSFile, DVDDirGetEntryName() );
			}
			else {
				mch = strchr(DVDDirGetEntryName(), '_');
				if( mch-DVDDirGetEntryName() == 6 ) {
					strncpy ( WBFSFile, DVDDirGetEntryName(), 6 );
				}
				mch = strchr(DVDDirGetEntryName(), ']');
				if( mch-DVDDirGetEntryName()+1 == strlen(DVDDirGetEntryName()) ) {
					strncpy ( WBFSFile, DVDDirGetEntryName() + strlen(DVDDirGetEntryName()) - 7, 6 );
				}
			}
			
			sprintf( WBFSPath, "/wbfs/%.31s/%s.wbfs", DVDDirGetEntryName(), WBFSFile );

			WBFS_Read( 0x240000, 4, buf1 );
					
			if(*(vu32*)buf1 == 0x00000001) // For now skip games with update partition
			{			
				s32 fd = DVDOpen( WBFSPath, FA_READ );
				if( fd >= 0 )
				{				
					GameCount++;
					DVDClose(fd);
				}
			}
		}
	}

	free( Path );
	hfree( buf1 );
	
	dbgprintf("CDI:Found: %d games\n", GameCount);

	return GameCount;	
}

u32 DVDVerifyGames( void )
{
	u32 UpdateGameCache = 0;
	u32 i;
	
	char *Path = (char*)malloca( 128, 32 );
	char *mch;

	for( i=0; i < DICfg->Gamecount; ++i )
	{
		//hexdump( DICfg->GameInfo[i], DVD_GAMEINFO_SIZE );

		sprintf( Path, "/games/%.31s/sys/boot.bin", DICfg->GameInfo[i]+DVD_GAME_NAME_OFF );
		s32 fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			//If not FST extr. check for WBFS
			if( strlen((char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF) == 6 ) {
				strcpy ( WBFSFile, (char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF );
			}
			else {
				mch = strchr((char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF, '_');
				if( mch-(char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF == 6 ) 
					strncpy( WBFSFile, (char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF, 6 );
				
				mch = strchr((char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF, ']');
				if( mch-(char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF+1 == strlen((char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF) ) 
					strncpy( WBFSFile, (char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF +  strlen((char*)DICfg->GameInfo[i]+DVD_GAME_NAME_OFF) - 7, 6 );
						
			}
			sprintf( WBFSPath, "/wbfs/%.31s/%s.wbfs", DICfg->GameInfo[i]+DVD_GAME_NAME_OFF, WBFSFile );
			s32 fd = DVDOpen( WBFSPath, DREAD );
			if( fd < 0 )
			{	
				//If not WBFS aswell check SD (DML)
				if( FSMode == SNEEK )
				{
					FSMode = UNEEK;

					fd = DVDOpen( Path, DREAD );
					if( fd < 0 )
					{
						UpdateGameCache = 1;
						FSMode = SNEEK;
						break;
					} else {
						DVDClose( fd );
					}

					FSMode = SNEEK;

				} 
				else 
				{
					UpdateGameCache = 1;
					break;
				}
			}
			else
			{
				DVDClose( fd );
			}				
		} else {
			DVDClose( fd );
		}
	}

	free( Path );

	return UpdateGameCache;		
}
s32 DVDUpdateCache( u32 ForceUpdate )
{
	u32 UpdateCache = ForceUpdate;
	u32 GameCount	= 0;
	u32 CurrentGame = 0;
	u32 i;
	u32 DMLite		= 0;
	s32 fres		= 0;
	char *Path = (char*)malloca( 128, 32 );

//First check if file exists and create a new one if needed

	sprintf( Path, "/sneek/diconfig.bin" );
	s32 fd = DVDOpen( Path, FA_READ | FA_WRITE );
	if( fd < 0 )
	{
		//No diconfig.bin found, create a new one
		fd = DVDOpen( Path, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );
		switch(fd)
		{
			case DVD_NO_FILE:
			{
				//In this case there is probably no /sneek folder
				sprintf( Path, "/sneek" );
				DVDCreateDir( Path );
				
				sprintf( Path, "/sneek/diconfig.bin" );
				fd = DVDOpen( Path, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );
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
		
		//Create default config
		DICfg->Gamecount= 0;
		DICfg->Config	= CONFIG_AUTO_UPDATE_LIST;
		DICfg->SlotID	= 0;
		DICfg->Region	= GetSystemMenuRegion();

		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );

		UpdateCache = 1;

	} else if( DVDGetSize( fd ) < DVD_CONFIG_SIZE ) {
		
		//Create default config
		DICfg->Gamecount= 0;
		DICfg->Config	= CONFIG_AUTO_UPDATE_LIST;
		DICfg->SlotID	= 0;
		DICfg->Region	= GetSystemMenuRegion();
		
		DVDSeek( fd, 0, 0 );
		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );

		UpdateCache = 1;
	}

	DVDSeek( fd, 0, 0 );

//Read current config and verify the diconfig.bin content
	fres = DVDRead( fd, DICfg, DVD_CONFIG_SIZE );
	if( fres != DVD_CONFIG_SIZE )
	{
		sprintf( Path, "/sneek/diconfig.bin" );
		DVDDelete(Path);

		return DVDUpdateCache(1);
	}

//Sanity Check config
	if( DICfg->Gamecount > 9000 || DICfg->SlotID > 9000 || DICfg->Region > LTN )
	{
		//Create default config
		DICfg->Gamecount= 0;
		DICfg->Config	= CONFIG_AUTO_UPDATE_LIST;
		DICfg->SlotID	= 0;
		DICfg->Region	= GetSystemMenuRegion();
		
		DVDSeek( fd, 0, 0 );
		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );

		UpdateCache = 1;
	}

//Read rest of config

	//Cache count for size calc
	GameCount = DICfg->Gamecount;
	free( DICfg );

	DICfg = (DIConfig*)malloca( GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE, 32 );

	DVDSeek( fd, 0, 0 );
	fres = DVDRead( fd, DICfg, GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE );
	if( fres != GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE )
		UpdateCache = 1;

	if( DICfg->Config & CONFIG_AUTO_UPDATE_LIST )
		GameCount = DVDGetInstalledGamesCount();
	else
		GameCount = DICfg->Gamecount;

	if( GameCount != DICfg->Gamecount )
		UpdateCache = 1;

//No need to check if we are going to rebuild anyway
	if( UpdateCache == 0 )
	if( DICfg->Config & CONFIG_AUTO_UPDATE_LIST )
		UpdateCache = DVDVerifyGames();

	if( UpdateCache )
	{
		DVDClose(fd);
		
		sprintf( Path, "/sneek/diconfig.bin" );
		DVDDelete( Path );

		fd = DVDOpen( Path, FA_CREATE_ALWAYS | FA_READ | FA_WRITE );

		DICfg->Gamecount = GameCount;

		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );

		char *GameInfo = (char*)malloca( DVD_GAMEINFO_SIZE, 32 );
		char *mch;
		u8 *buf1 = (u8 *)halloca(0x04, 32);		
		
		//Check on USB and on SD(DML)
		for( i=0; i < 2; i++ )
		{
			sprintf( Path, "/games" );
			if( DVDOpenDir( Path ) == FR_OK )
			{
				while( DVDReadDir() == FR_OK )
				{
					if( DVDDirIsFile() )
						continue;					
					
					sprintf( Path, "/games/%.31s/sys/boot.bin", DVDDirGetEntryName() );
					s32 gi = DVDOpen( Path, FA_READ );
					if( gi >= 0 )
					{
						if( DVDRead( gi, GameInfo, DVD_GAMEINFO_SIZE ) == DVD_GAMEINFO_SIZE )
						{
							memcpy( GameInfo+DVD_GAME_NAME_OFF, DVDDirGetEntryName(), strlen( DVDDirGetEntryName() ) );

							if( DMLite )
								FSMode = SNEEK;

							DVDWrite( fd, GameInfo, DVD_GAMEINFO_SIZE );

							if( DMLite )
								FSMode = UNEEK;
						}

						DVDClose( gi );
						CurrentGame++;
					}
				}
			}
			
			sprintf( Path, "/wbfs" );
			if( DVDOpenDir( Path ) == FR_OK )
			{
				while( DVDReadDir() == FR_OK )
				{					
					if( strlen( DVDDirGetEntryName() ) > 31 )
					{
						dbgprintf( "CDI:Skipping to long folder entry: %s\n", DVDDirGetEntryName() );
						continue;
					}					
					
					if( strlen( DVDDirGetEntryName() ) == 11 && strncmp(DVDDirGetEntryName()+7, "wbfs", 4 ) == 0 )
					{
						sprintf( WBFSPath, "/wbfs/%s", DVDDirGetEntryName() );
						dbgprintf( "CDI:Set gamepath to: %s\n", WBFSPath );
						WBFS_Read( 0x240000, 4, buf1 );						
						if( *(vu32*)buf1 == 0x00000001 ) // For now skip games with update partition
						{	
							s32 r = WBFS_Read( 0x200, DVD_GAMEINFO_SIZE, GameInfo );
							memcpy( GameInfo+DVD_GAME_NAME_OFF, DVDDirGetEntryName(), strlen( DVDDirGetEntryName() ) );									
							DVDWrite( fd, GameInfo, DVD_GAMEINFO_SIZE );
							CurrentGame++;
							if( r == WBFS_OK )
								dbgprintf( "CDI:Saved: %d, %s\n", CurrentGame, DVDDirGetEntryName() );
							else
								dbgprintf( "CDI:Failed opening entry: %s\n", DVDDirGetEntryName() );
								
							continue;							
						}
						else
						{
							dbgprintf( "CDI:Skipping game with update partition: %s\n", DVDDirGetEntryName() );
							continue;
						}
					}

					if( strlen(DVDDirGetEntryName()) == 6 ) {
						strcpy ( WBFSFile, DVDDirGetEntryName() );
					}
					else {
						mch = strchr(DVDDirGetEntryName(), '_');
						if( mch-DVDDirGetEntryName() == 6 ) 						
							strncpy( WBFSFile, DVDDirGetEntryName(), 6 );
						
						mch = strchr(DVDDirGetEntryName(), ']');
						if( mch-DVDDirGetEntryName()+1 == strlen(DVDDirGetEntryName()) ) 						
							strncpy( WBFSFile, DVDDirGetEntryName() +  strlen(DVDDirGetEntryName()) - 7, 6 );
						
					}

					sprintf( WBFSPath, "/wbfs/%.31s/%s.wbfs", DVDDirGetEntryName(), WBFSFile );		
					dbgprintf(" CDI:Set gamepath to: %s\n", WBFSPath );
					WBFS_Read( 0x240000, 4, buf1 );					
					if( *(vu32*)buf1 == 0x00000001 ) // For now skip games with update partition
					{							
						s32 r = WBFS_Read( 0x200, DVD_GAMEINFO_SIZE, GameInfo );
						memcpy( GameInfo+DVD_GAME_NAME_OFF, DVDDirGetEntryName(), strlen( DVDDirGetEntryName() ) );
						DVDWrite( fd, GameInfo, DVD_GAMEINFO_SIZE );
						CurrentGame++;
						if( r == WBFS_OK )
							dbgprintf( "CDI:Saved: %d, %s\n", CurrentGame, DVDDirGetEntryName() );
						else
							dbgprintf( "CDI:Failed opening entry: %s\n", DVDDirGetEntryName() );

						continue;
					}
					else
					{
						dbgprintf( "CDI:Skipping game with update partition: %s\n", DVDDirGetEntryName() );
						continue;
					}
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
		free( buf1 );
	}
	
	DVDClose( fd );

//Read new config
	free( DICfg );
	
	sprintf( Path, "/sneek/diconfig.bin" );
	fd = DVDOpen( Path, DREAD );

	DICfg = (DIConfig*)malloca( DVDGetSize(fd), 32 );

	DVDRead( fd, DICfg, DVDGetSize(fd) );
	DVDClose(fd);

	free( Path );
	
	return DI_SUCCESS;
}

u32 DMLite = 0;
s32 DVDSelectGame( int SlotID )
{
	if( SlotID >= 2000 )
	{
		SlotID -= 2000;
		wbfstofst = 1;
	}

	GameHook = 0;
	
	FMode = IS_FST;

	if( SlotID < 0 )
		SlotID = 0;

	if( SlotID >= DICfg->Gamecount )
		SlotID = 0;

	char *str = (char *)malloca( 256, 32 );
	
	//build path
	sprintf( GamePath, "/games/%.31s/", &DICfg->GameInfo[SlotID][0x60] );
	
	FSTable = (u8*)NULL;
	ChangeDisc = 1;
	DICover |= 1;
		
	//Get Apploader size
	sprintf( str, "%ssys/apploader.img", GamePath );
	s32 fd = DVDOpen( str, DREAD );

	if( (FSMode == UNEEK) && (fd < 0) )
	{
		FMode = IS_WBFS;		
		
		if( KeyIDT > 0 )
			DestroyKey( KeyIDT );
		KeyIDT = 0;
		
		sprintf( WBFSFile, "%s", DICfg->GameInfo[SlotID] );
		
		//if(strcmp( (char*)&DICfg->GameInfo[SlotID][0x60], "wbfs" ) == 0 )
		if( strncmp( (char*)&DICfg->GameInfo[SlotID][0x60]+7, "wbfs", 4 ) == 0 )
			sprintf( WBFSPath, "/wbfs/%s.wbfs",  WBFSFile );			
		else			
			sprintf( WBFSPath, "/wbfs/%.31s/%s.wbfs", &DICfg->GameInfo[SlotID][0x60], WBFSFile );
		
		
		dbgprintf("CDI:DVDSelectGame:gamepath=%s\n",WBFSPath);
		
		fd = DVDOpen(WBFSPath, FA_READ);
		if( fd < 0 )
		{
			FMode = IS_FST;
		}
		else {
			u8 *buf2 = (u8 *)halloca(0x480, 32);
			
			maxblock=0;
			lastblock=0;
			
			WBFS_Read( 0x240000, 4, buf2 );
			
			if(*(vu32*)buf2 == 0x00000001) { // Should be true for now		
				game_part_offset = 0x400000;
			}
			else {
				WBFS_Read( 0x2502bc, 4, buf2 );
				data_size = *(vu32*)(buf2);
				if(data_size != 0x2fe8000)
					game_part_offset = ((((data_size * 4) + 0x450000) / 0x100000) * 0x100000);
				else
					game_part_offset = 0xc400000;
			}
			
			dbgprintf("CDI:game_partition_offset=0x%x\n",game_part_offset);
			
			s32 ret = WBFS_Read( game_part_offset, 0x480, buf2 );
			if( ret != WBFS_OK ){
				ChangeDisc = 0;
				DICover |= 1;
				hfree( buf2 );
				free( str );
				return DI_FATAL;
			}
			
			data_offset = *(vu32*)(buf2+0x2b8);
			data_size   = *(vu32*)(buf2+0x2bc);
			tmd_size    = *(vu32*)(buf2+0x2a4);
			tmd_offset  = *(vu32*)(buf2+0x2a8);
			cert_size   = *(vu32*)(buf2+0x2ac);
			cert_offset = *(vu32*)(buf2+0x2b0);
		
			maxblock=(((((data_size) / 0x2000) / 0x10) * 0x10) + 0x10);
			
			if ( strncmp( WBFSFile, "SZBxxx", 3 ) == 0 )
				maxblock=0x20c80;
			
			if ( strncmp( WBFSFile, "R3Oxxx", 3 ) == 0 )
				maxblock=0x3d5c0;
			
			if ( strncmp( WBFSFile, "RSBxxx", 3 ) == 0 )
				maxblock=0;
	
			data_offset <<= 2;			
		
			ret = WBFS_Encrypted_Read(0, 0x480, buf2);
			if( ret != WBFS_OK ){
				ChangeDisc = 0;
				DICover |= 1;
				hfree( buf2 );
				free( str );
				return DI_FATAL;
			}
   			
   			dbgprintf("CDI:Decrypted game buffer\n");
   			hexdump(buf2,0x20);
		
			dol_offset 	= *(vu32*)(buf2+0x420);
			fst_offset  = *(vu32*)(buf2+0x424);
			fst_size    = *(vu32*)(buf2+0x428);
			
			wbfs_len = DVDGetSize(fd) - 0x400000;
			
			// Check for split file
			sprintf( str, "/wbfs/%.31s/%s.wbf1", &DICfg->GameInfo[SlotID][0x60], WBFSFile  );
			s32 fd_sf = DVDOpen(str, FA_READ);
			if( fd_sf >= 0 )		
				wbfs_len += DVDGetSize(fd_sf);		
			
			DVDClose(fd_sf);		
		
			lastblock = wbfs_len / 0x8000;
		
			if(  lastblock > maxblock )
				maxblock=lastblock;			
				
			hfree( buf2 );		
		}
	}	
	
	if( (FSMode == SNEEK) && (fd < 0) )
	{
		FSMode = UNEEK;
		DMLite = 1;
		//Write boot info for DML
		s32 bi = DVDOpen( "/games/boot.bin", FA_CREATE_ALWAYS|FA_WRITE );
		if( bi >= 0 )
		{
			DVDWrite( bi, &DICfg->GameInfo[SlotID][0x60], strlen((char*)(&DICfg->GameInfo[SlotID][0x60])) );
			DVDClose( bi );
		}
		fd = DVDOpen( str, DREAD );
	} 

	if( (fd < 0) && (FMode != IS_WBFS) )
	{			
		if( DMLite )
			FSMode = SNEEK;

		DVDUpdateCache(1);

		free( str );
		return DVDSelectGame(0);
	}
	else {
		DMLite = 0;
	}

	ApploaderSize = DVDGetSize( fd ) >> 2;
	DVDClose( fd );

	if( DMLite )
		FSMode = SNEEK;

	free( str );

	//update di-config
	fd = DVDOpen( "/sneek/diconfig.bin", DWRITE );
	if( fd >= 0 )
	{
		DICfg->SlotID = SlotID;
		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );
		DVDClose( fd );
	}

	//Init cache
	u32 count = 0;

	for( count=0; count < BLOCKCACHE_MAX; ++count )
	{
		BC[count].bl_num = 0xdeadbeef;
	}

	for(count=0; count < FILECACHE_MAX; ++count )
	{
		FC[count].File = 0xdeadbeef;
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

	if( Offset < 0x110 )	// 0x440
	{
		sprintf( Path, "%ssys/boot.bin", GamePath );
		fd = DVDOpen( Path, DREAD );
		if( fd < 0 )
		{
			return DI_FATAL;
		} else {
			u32 *rbuf = (u32*)halloca( sizeof(u32), 32 );

			//read requested data
			DVDSeek( fd, Offset<<2, 0 );
			DVDRead( fd, ptr, Length );

			//Read DOL/FST offset/sizes for later usage
			DVDSeek( fd, 0x0420, 0 );
			DVDRead( fd, rbuf, 4 );
			DolOffset = *rbuf;

			DVDSeek( fd, 0x0424, 0 );
			DVDRead( fd, rbuf, 4 );
			FSTableOffset = *rbuf;

			DVDSeek( fd, 0x0428, 0 );
			DVDRead( fd, rbuf, 4 );
			FSTableSize = *rbuf;

			hfree( rbuf );

			DolSize = FSTableOffset - DolOffset;
			FSTableSize <<= 2;

			DVDClose( fd );
			return DI_SUCCESS;
		}

	} else if( Offset < 0x910 )	// 0x2440
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

			//GC region patch
			if( DiscType == DISC_DOL )
				*(vu32*)(ptr+0x18) = DICfg->Region;
			
			return DI_SUCCESS;
		}


	} else if( Offset < 0x910+ApploaderSize )	// 0x2440
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
				//Remote debugging and USBgecko debug printf doesn't work well at the same time!
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

				sprintf( Path, "/sneek/kenobiwii.bin" );
				s32 fd = IOS_Open( Path, 1 );
				if( fd < 0 )
					return DI_SUCCESS;

				u32 Size = IOS_Seek( fd, 0, 2 );
				IOS_Seek( fd, 0, 0 );

				//Read file to memory
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
	} else {
		return Search_FST( Offset, Length, ptr, FST_READ );		
	}
	return DI_FATAL;
}
s32 DVDLowReadUnencrypted( u32 Offset, u32 Length, void *ptr )
{
	switch( Offset )
	{
		case 0x00:
		case 0x08:		// 0x20
		{
			if( FMode == IS_WBFS )
			{
				WBFS_Encrypted_Read(Offset, Length, ptr);

				return DI_SUCCESS;
			}
			else {
				char *str = (char *)malloca( 64, 32 );
				sprintf( str, "%ssys/boot.bin", GamePath );

				u32 ret = DI_FATAL;
				s32 fd = DVDOpen( str, FA_READ );
				if( fd >= 0 )
				{
					if( DVDRead( fd, ptr, Length ) == Length )
					{
						free( str );
						ret = DI_SUCCESS;				
					}
					DVDClose( fd );
				}

				free( str );
				return ret;
			}
			
		} break;
		case 0x10000:		// 0x40000
		{
			memset( ptr, 0, Length );

			*(u32*)(ptr)	= 1;				// one partition
			*(u32*)(ptr+4)	= 0x40020>>2;		// partition table info

			return DI_SUCCESS;
		} break;
		case 0x10008:		// 0x40020
		{
			memset( ptr, 0, Length );

			*(u32*)(ptr)	= 0x03E00000;		// partition offset
			*(u32*)(ptr+4)	= 0x00000000;		// partition type

			return DI_SUCCESS;
		} break;
		case 0x00013800:		// 0x4E000
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
		WBFS_Encrypted_Read(Offset, Length, ptr);
	}
	else {
		char *str = (char *)malloca( 32, 32 );
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

	// hexdump( ptr, Length );

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
	s32 fd;
			
	//dbgprintf("CDI:Ioctl -> command = %d\n",msg->ioctl.command);
	switch(msg->ioctl.command)
	{
		//Added for slow harddrives
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

			sprintf( name, "%s", "/sneek/diconfig.bin" );
			fd = DVDOpen( name, FA_WRITE|FA_OPEN_EXISTING );
			if( fd < 0 )
			{
				DVDDelete( name );
				fd = DVDOpen( name, FA_WRITE|FA_CREATE_ALWAYS );
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
		case DVD_READ_GAMEINFO:
		{
			u32 *vec = (u32*)msg->ioctl.buffer_in;

			fd = DVDOpen( "/sneek/diconfig.bin", FA_READ );
			if( fd < 0 )
			{
				ret = DI_FATAL;
			} else {

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
		case DVD_GET_GAMECOUNT:	//Get Game count
		{
			*(u32*)bufout = DICfg->Gamecount;
			ret = DI_SUCCESS;
		} break;
		case DVD_SELECT_GAME:
		{
			//ret = DVDSelectGame( *(u32*)(bufin) );
			requested_game = *(u32*)(bufin);
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
	
				write32( 0x0D80601C, 3 );		// send cmd
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
		case 0xDD:			// 0 out
		{
			ret = DI_SUCCESS;
		} break;
		case 0x95:			// 0x20 out
		{
			*(u32*)bufout = DIStatus;
			ret = DI_SUCCESS;
		} break;
		case 0x7A:			// 0x20 out
		{
			*(u32*)bufout = DICover;

			if( ChangeDisc )
			{
				DICover &= ~1;
				ChangeDisc = 0;
			}

			ret = DI_SUCCESS;
		} break;
		case 0x86:			// 0 out
		{
			ret = DI_SUCCESS;
		} break;
		case DVD_GETCOVER:	// 0x88
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
		case DVD_GET_ERROR:	// 0xE0
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
		case DVD_LOW_READ:	// 0x71, GC never calls this
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
						if( (*(u32*)(bufin+8)) > fst_offset + fst_size ) 
						{
							u32 bl_offset, bl_length, i;
							u32 offset = *(u32*)(bufin+8);
							u32 length =((*(u32*)(bufin+4))+31)&(~31);

							while(length) {
								bl_offset = offset % (0x7c00 >> 2);
								bl_length = 0x7c00 - (bl_offset << 2);
								BCRead = 0;
		
								if (bl_length > length) 
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
											memcpy(bufout, BC[i].bl_buf + (bl_offset << 2), bl_length);
											BCRead = 1;
										}
									}
								}						
						
								if(BCRead != 1)
								{
									if( BCEntry >= BLOCKCACHE_MAX )
										BCEntry = 0;
						
									ret = WBFS_Read_Block( offset / (0x7c00 >> 2), BC[BCEntry].bl_buf, FST_READ);		
									memcpy(bufout, BC[BCEntry].bl_buf + (bl_offset << 2), bl_length);
									BC[BCEntry].bl_num = offset / (0x7c00 >> 2);
									BCEntry++;						
								}			

								bufout += bl_length;
								offset += bl_length >> 2;
								length -= bl_length;
							}
							if( wbfstofst )
							{
								WBFS_To_FST();
								wbfstofst = 0;
							}
						}
						else {
							ret = WBFS_Encrypted_Read(*(u32*)(bufin+8), *(u32*)(bufin+4), bufout);					
						}
						if(ret != WBFS_OK) 
							ret = DI_FATAL;
					}
					else {
						ret = DVDLowRead( *(u32*)(bufin+8), *(u32*)(bufin+4), bufout );
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
				//Invalid disc type!
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

	//Reset errror after a successful command
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
			if( Partition == 1 )
			{
				DIP_Fatal( "DVDLowOpenPartition", __LINE__, __FILE__, 0, "Partition already open!");
			}

			PartitionOffset = *(u32*)(v[0].data+4);
			PartitionOffset <<= 2;

			u8 *TMD;
			u8 *TIK;
			u8 *CRT;
			u8 *hashes = (u8*)halloca( sizeof(u8)*0x14, 32 );
			u8 *buffer = (u8*)halloca( 0x40, 32 );
			memset( buffer, 0, 0x40 );

			char *str = (char*)halloca( 0x40,32 );
			
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
				((u32*)buffer)[0x04] = (u32)TIK;			//0x10
				((u32*)buffer)[0x05] = 0x2a4;				//0x14
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
					((u32*)buffer)[0x06] = (u32)TMD;		//0x18
					((u32*)buffer)[0x07] = tmd_size;		//0x1C
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

					((u32*)buffer)[0x06] = (u32)TMD;		//0x18
					((u32*)buffer)[0x07] = DVDGetSize(fd);		//0x1C
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
				((u32*)buffer)[0x00] = (u32)CRT;		//0x00
				((u32*)buffer)[0x01] = 0xA00;			//0x04
				sync_before_read(CRT, 0xA00);
			}			

			hfree( str );

			KeyID = (u32*)malloca( sizeof( u32 ), 32 );


			((u32*)buffer)[0x02] = (u32)NULL;			//0x08
			((u32*)buffer)[0x03] = 0;					//0x0C

			//out
			((u32*)buffer)[0x08] = (u32)KeyID;			//0x20
			((u32*)buffer)[0x09] = 4;					//0x24
			((u32*)buffer)[0x0A] = (u32)hashes;			//0x28
			((u32*)buffer)[0x0B] = 20;					//0x2C

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

	//Reset errror after a successful command
	if( ret == DI_SUCCESS )
		error = 0;

	return ret;
}

s32 WBFS_Read( u64 offset, u64 length, void *ptr ) 
{
	if( !( strcmp( WBFSPath, CacheFile ) == 0 ) ) 
	{
		if(Cachefd > 0)
			IOS_Close(Cachefd); 
			
		Cachefd = IOS_Open( WBFSPath, 1 );
		if(Cachefd < 0)		
		{	
			dbgprintf("CDI:WBFS_Read failure(1) for %s\n", WBFSPath);
			return WBFS_FATAL;
		}	
		sprintf( CacheFile, "%s", WBFSPath );
	}
	
	if(offset + length > 0xffff8000)
	{
		dbgprintf("CDI:WBFS_Read wbf1 file \n");
		char *str = (char *)malloca( 128, 32 );
		sprintf( str, "%s%s.wbf1", GamePath, WBFSFile );
		
		if( !( strcmp( str, CacheFileb ) == 0 ) ) 
		{
			if(Cachefdb > 0)
				IOS_Close(Cachefdb);
				
			Cachefdb = IOS_Open( str, 1 );
			if(Cachefdb < 0)		
			{
				dbgprintf("CDI:WBFS_Read failure(2) for %s\n", str);
				return WBFS_FATAL;
			}			
			sprintf( CacheFileb, "%s", str );
		}
		
		free(str);
		
		if(offset > 0xffff8000)
		{
			offset -= 0xffff8000;	
			IOS_Seek(Cachefdb, offset, 0);
			IOS_Read(Cachefdb, ptr, length);	
		}
		else 
		{
			u32 toread = (offset + length) - 0xffff8000;
			IOS_Seek(Cachefd, offset, 0);
			IOS_Read(Cachefd, ptr, (length - toread));
			ptr += (length - toread);
			IOS_Seek(Cachefdb, 0, 0);
			IOS_Read(Cachefdb, ptr, toread);
		}	
	}
	else 
	{	
		IOS_Seek(Cachefd, offset, 0);
		IOS_Read(Cachefd, ptr, length);
	}

	return WBFS_OK;
}

s32 WBFS_Read_Block( u64 block, void *ptr, u32 read) 
{
	u8 *bufrb = (u8 *)malloca(0x8000, 0x40);
	u8 *iv = (u8 *)malloca(0x10, 0x40);
	u64 offset;

	if( KeyIDT == 0 )
		Set_key2();		

	if( read == FST_READ && block > (maxblock - lastblock)) 
		block -= (maxblock - lastblock);

	offset = game_part_offset + data_offset + (0x8000 * block);
	
	WBFS_Read( offset, 0x8000, bufrb );
	memcpy(iv, bufrb + 0x3d0, 16);

	aes_decrypt_( KeyIDT, iv, bufrb + 0x400, 0x7c00, bufrb);
	memcpy(ptr, bufrb ,0x7c00);
	
	free(bufrb);
	free(iv);
	
	return WBFS_OK;
}

s32 WBFS_Encrypted_Read( u32 offset, u32 length, void *ptr)  
{
	u32 bl_offset, bl_length;
	
	while(length) {
		bl_offset = offset % (0x7c00 >> 2);
		bl_length = 0x7c00 - (bl_offset << 2);
		
		if (bl_length > length) 
			bl_length = length;
		
		WBFS_Read_Block( offset / (0x7c00 >> 2), BC[0].bl_buf, PAR_READ);
		memcpy(ptr, BC[0].bl_buf + (bl_offset << 2), bl_length);

		ptr += bl_length;
		offset += bl_length >> 2;
		length -= bl_length;
	}
	return WBFS_OK;
}

s32 WBFS_Decrypted_Write( char *path, char *filename, u32 offset, u32 length, u32 fst)
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
}

s32 WBFS_To_FST( void )
{
	u8 *buffer = (u8*)halloca( 0xA00, 32 );
	char *str = (char*)halloca( 0x40,32 );
	
	sprintf( str, "/games/%s", WBFSFile );
	DVDCreateDir( str );

	s32 ret = WBFS_Read( game_part_offset, 0x2a4, buffer );
	sprintf( str, "/games/%s/ticket.bin", WBFSFile );
	s32 writefd = DVDOpen( str, FA_WRITE|FA_READ|FA_CREATE_ALWAYS );
	DVDWrite( writefd, buffer, 0x2a4);
	DVDClose(writefd);	

	ret = WBFS_Read( game_part_offset + (tmd_offset << 2), tmd_size, buffer );
	sprintf( str, "/games/%s/tmd.bin", WBFSFile );
	writefd = DVDOpen( str, FA_WRITE|FA_READ|FA_CREATE_ALWAYS );
	DVDWrite( writefd, buffer, tmd_size);
	DVDClose(writefd);	
	
	ret = WBFS_Read( game_part_offset + (cert_offset << 2), 0xA00, buffer );	
	sprintf( str, "/games/%s/cert.bin", WBFSFile );
	writefd = DVDOpen( str, FA_WRITE|FA_READ|FA_CREATE_ALWAYS );
	DVDWrite( writefd, buffer, 0xA00);
	DVDClose(writefd);	
	
	u32 appldr_offset = 0x2440 >> 2;
	
	ret = WBFS_Encrypted_Read(appldr_offset, 0x20, buffer);
	
	u32 appldr_size = 0x20 + *(vu32*)(buffer + 0x14) + *(vu32*)(buffer + 0x18);
	u32 dol_size = (fst_offset - dol_offset);
	
	sprintf( str, "/games/%s/sys", WBFSFile );
	ret = WBFS_Decrypted_Write( str, "apploader.img", appldr_offset, appldr_size, PAR_READ);
	ret = WBFS_Decrypted_Write( str, "fst.bin", fst_offset, fst_size, PAR_READ);
	ret = WBFS_Decrypted_Write( str, "main.dol", dol_offset, dol_size, PAR_READ);

	hfree( str );
	hfree( buffer );

	Search_FST( 0, 0, NULL, WBFS_CONF );
	
	return ret;
}

s32 Search_FST( u32 Offset, u32 Length, void *ptr, u32 mode )
{
	char Path[256];
	char File[64];
	
	u32 i,j;

	if( FSTable == NULL )
		FSTable	= (u8*)((*(vu32*)0x38) & 0x7FFFFFFF);	

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
				case WBFS_CONF:
				{
					memset( Path, 0, 256 );	
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
						continue;
						
				} break;
					
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
							} else {

								FC[FCEntry].Size	= fe[i].FileLength;
								FC[FCEntry].Offset	= fe[i].FileOffset;

								DVDSeek( FC[FCEntry].File, nOffset, 0 );
								DVDRead( FC[FCEntry].File, ptr, Length );
							
								FCEntry++;

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
