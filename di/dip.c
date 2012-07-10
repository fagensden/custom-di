/*

SNEEK - SD-NAND/ES + DI emulation kit for Nintendo Wii

Copyright (C) 2009-2011	crediar				   
				   2011 Obcd 
			  2011-2012	OverjoY
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
#include "sram.h"

static u32 error ALIGNED(32);
static u32 PartitionSize ALIGNED(32);
/*** this being 1 forces an eject disk ***/
static u32 DICover ALIGNED(32);
static u32 ChangeDisc ALIGNED(32);
static u32 isASEnabled = 0;

DIConfig *DICfg;
WBFSInfo *WBFSInf;
WBFSFileInfo *WBFSFInf;

static char GamePath[128];
static char WBFSFile[8];
static char WBFSPath[128]; 

static u32 *KeyID ALIGNED(32);
static u32 KeyIDT ALIGNED(32) = 0;
static u8 *FSTable ALIGNED(32);

u32 ApploaderSize = 0;
u32 DolSize = 0;
u32 DolOffset = 0;
u32 FSTableSize = 0;
u32 FSTableOffset = 0;
u32 TMDOffset = 0;
u32 TMDSize = 0;
u32 DataOffset = 0;
u32 DataSize = 0;
u32 CertOffset = 0;
u32 CertSize = 0;
u32 DiscType = 0;

u32 FCEntry = 0;
FileCache FC[FILECACHE_MAX];

u32 Partition = 0;
u32 Disc = 0;
u32 GameHook = 0;
u32 isparsed = 0;

/*** WBFS cache vars ***/
BlockCache BC[BLOCKCACHE_MAX];
FileSplits FS[FILESPLITS_MAX];
u32 splitcount=0;
u32 BCEntry=0;

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
u32 blft;
u32 blft_mask=0;
u32 game_part_offset=0;
u32 disc_part_offset=0;

static u32 bufcache_init=0;
u8 *bufrb ALIGNED(32);

extern u32 FSMode;
u32 FMode = IS_FST;

extern s32 switchtimer;
extern int QueueID;
extern int requested_game;
extern char* cdiconfig ALIGNED(32);

/*** Debug loging: ***/
//#define DEBUG_PRINT_FILES

static void Set_key2( void )
{
	u8 *TIK = (u8 *)halloca(0x2a4, 0x40);
	u8 *TitleID    = (u8*)malloca( 0x10, 0x20 );
	u8 *EncTitleKey = (u8*)malloca( 0x10, 0x20 );
	u8 *KeyIDT2 = (u8*)malloca( 0x10, 0x20 );

	WBFS_Read(game_part_offset, 0x2a4, TIK); /*** Read the ticket ***/
	
	memset32(TitleID, 0, 0x10);
	memset32(EncTitleKey, 0, 0x10);
	memset32(KeyIDT2, 0, 0x10);

	memcpy(TitleID, TIK+0x1DC, 8);
	memcpy(EncTitleKey, TIK+0x1BF, 16);
	
	vector *v = (vector*)halloca(sizeof(vector)*3, 32);
	
	v[0].data = (u8 *)TitleID;
	v[0].len = 0x10;
	v[1].data = (u8 *)EncTitleKey;
	v[1].len = 0x10;
	v[2].data = (u8 *)KeyIDT2;
	v[2].len = 0x10;

	s32 ESHandle = IOS_Open("/dev/es", 0);

	IOS_Ioctlv(ESHandle, 0x50, 2, 1, v);

    KeyIDT = *(u32*)(KeyIDT2);
	
	free(KeyIDT2);
    free(EncTitleKey);
    free(TitleID);
	hfree(TIK);
	hfree(v);
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

void InitCache(void)
{
	u32 count = 0;

	for(count=0; count < BLOCKCACHE_MAX; ++count)
	{
		BC[count].bl_num = 0xdeadbeef;
	}

	for(count=0; count < FILECACHE_MAX; ++count)
	{
		FC[count].File = 0xdeadbeef;
	}
	
	for(count=0; count < FILESPLITS_MAX; ++count)
	{
		FS[count].Offset = 0x00000001;
	}
}

void Rebuild_Disc_Usage_Table(void)
{
	u16 zero_pos=0xffff;
	u16	curr_pos=0;
	u32 block_gp=0;
	u32 i, j;
	for(i = start_loc; i < (max_wbfs_sec + 1); ++i)
	{
		if(WBFSInf->disc_usage_table[i] == 0)
		{
			if(zero_pos == 0xffff)
				zero_pos = i;
				
			curr_pos = i;				
			block_gp += 0x4;
			
			for(j = zero_pos; j <= curr_pos; ++j)
				WBFSInf->disc_usage_table[j] = block_gp;			
		}
		else
		{
			zero_pos=0xffff;
			WBFSInf->disc_usage_table[i] = block_gp;
		}
		
	}	
}

u32 GetSystemMenuRegion(void)
{
	fstats* status = (fstats*)malloca(sizeof(fstats), 0x40);
	u32 Region = EUR;
	s32 fd = IOS_Open("/title/00000001/00000002/content/title.tmd", 1);
	if(fd >= 0)
	{
		IOS_Ioctl(fd, ISFS_IOCTL_GETFILESTATS, NULL, 0, status, sizeof(fstats));
		char *TMD = (char*)malloca(status->Size, 32);
		IOS_Read(fd, TMD,status->Size);
		Region = *(u16*)(TMD+0x1DC) & 0xF;
		IOS_Close(fd);
		free(TMD);
	}	
	free(status);
	return Region;
}

s32 DVDUpdateCache(u32 ForceUpdate)
{
	u32 UpdateCache = ForceUpdate;
	u32 GameCount	= 0;
	u32 CurrentGame = 0;
	u32 i;
	u32 DMLite		= 0;
	s32 fres		= 0;
	u32 entry_found;
	
	GameTitles *GT = (GameTitles *)NULL;
	
	char *Path = (char*)malloca(128, 32);

	/*** First check if file exists and create a new one if needed ***/
	s32 fd = DVDOpen(cdiconfig, FA_READ | FA_WRITE);
	if(fd < 0)
	{
		/*** No diconfig.bin found, create a new one ***/
		fd = DVDOpen(cdiconfig, FA_CREATE_ALWAYS | FA_READ | FA_WRITE);
		switch(fd)
		{
			case DVD_NO_FILE:
			{
				/*** In this case there is probably no /sneek folder ***/				
				DVDCreateDir("/sneek");				
				fd = DVDOpen(cdiconfig, FA_CREATE_ALWAYS | FA_READ | FA_WRITE);
				if(fd < 0)
				{
					free(Path);
					return DI_FATAL;
				}

			} break;
			case DVD_SUCCESS:
				break;
			default:
			{
				free(Path);
				return DI_FATAL;
			} break;
		}
		
		/*** Create default config ***/
		DICfg->Gamecount= 0;
		DICfg->SlotID	= 0;
		/*** Disable auto update functions because they don't work probably! ***/
		DICfg->Config = DML_VIDEO_GAME | DML_LANG_ENGLISH;
		DICfg->Region = GetSystemMenuRegion();
		
		DVDWrite(fd, DICfg, DVD_CONFIG_SIZE);
		UpdateCache = 1;
	}

	DVDSeek(fd, 0, 0);

	/*** Read current config and verify the diconfig.bin content ***/
	fres = DVDRead(fd, DICfg, DVD_CONFIG_SIZE);
	if(fres != DVD_CONFIG_SIZE)
	{
		DVDDelete(cdiconfig);
		free( Path );

		return DVDUpdateCache(1);
	}

	if(DICfg->Gamecount == 0 || DICfg->Config & CONFIG_AUTO_UPDATE_LIST)
		UpdateCache = 1;
	
	DICfg->Region = GetSystemMenuRegion();
		
	DVDSeek(fd, 0, 0);
	DVDWrite(fd, DICfg, DVD_CONFIG_SIZE);
	
	/*** Read rest of config ***/

	GameCount = DICfg->Gamecount;
	free(DICfg);

	DICfg = (DIConfig*)malloca(GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE, 32);

	DVDSeek(fd, 0, 0);
	fres = DVDRead(fd, DICfg, GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE);
	if(fres != GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE)
	{
		DVDDelete(cdiconfig);
		free( Path );

		return DVDUpdateCache(1);
	}
	
	DVDClose(fd);
		
	strcpy(Path, "/sneek/titles.txt");	
		
	fd = DVDOpen(Path, FA_READ);
	if( fd >= 0 )
	{
		u32 Contentsize = DVDGetSize(fd);
		char *Content = (char *)malloca(Contentsize+1, 32);
        Content[Contentsize]=0;
        DVDRead(fd, Content, Contentsize);
        DVDClose(fd);
		DVDDelete(Path);
		
		char *lbrk = strchr(Content, '\n');
		char *TitleInfo = (char*)malloca(0x60, 32);
		u32 lastoff=0;
		u32 length=0;
		strcpy(Path, "/sneek/titlecfg.bin");			
		
		DVDDelete(Path);
		fd = DVDOpen(Path, FA_CREATE_ALWAYS | FA_READ | FA_WRITE);
		GT->EntryCount = 0;
		DVDWrite(fd, GT, 0x10);
		
		while(lbrk != NULL)
		{
			length = (lbrk-Content) - lastoff;
			if( length > 73 )
				length = 73;

			memset(TitleInfo, 0 , 0x60);				
			strncpy(TitleInfo, Content + lastoff + 1, length - 1);					
			lastoff = lbrk-Content;
			DVDWrite(fd, TitleInfo, 0x60);
			lbrk = strchr(lbrk+1, '\n');
		}
		DVDClose(fd);
		free(Content);
		free(TitleInfo);
		UpdateCache = 1;
	}	

	if(UpdateCache)
	{		
		DVDDelete(cdiconfig);
		fd = DVDOpen(cdiconfig, FA_CREATE_ALWAYS | FA_READ | FA_WRITE);
		DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );

		char *GameInfo = (char*)malloca( DVD_GAMEINFO_SIZE, 32 );
		char *mch;
		u8 *buf1 = (u8 *)halloca(0x04, 32);
		InitCache();
		splitcount = 0;	

		u32 cTitles = 0;

		strcpy(Path, "/sneek/titlecfg.bin");

		s32 fdct = DVDOpen(Path, DREAD);
		if(fdct >= 0)
		{
			cTitles = DVDGetSize(fdct);
			GT = (GameTitles *)malloca(cTitles, 32);
			DVDRead(fdct, GT, cTitles);

			DVDClose(fdct);				
			cTitles-=0x10;
			cTitles/=0x60;
		}
		
		/*** Check on USB and on SD(DML) ***/
		for(i=0; i < 2; i++)
		{
			strcpy( Path, "/games" );
			if(DVDOpenDir( Path ) == FR_OK)
			{
				while(DVDReadDir() == FR_OK)
				{
					if(DVDDirIsFile())
						continue;
					
					if(strlen(DVDDirGetEntryName()) > 63)
						continue;

					sprintf(Path, "/games/%.63s/sys/boot.bin", DVDDirGetEntryName());	
					
					s32 gi = DVDOpen(Path, FA_READ);
					if(gi < 0)
					{
						sprintf(Path, "/games/%.63s/game.iso", DVDDirGetEntryName());
						gi = DVDOpen(Path, FA_READ);
					}
					
					if(gi >= 0)
					{
						if(DVDRead(gi, GameInfo, DVD_GAMEINFO_SIZE) == DVD_GAMEINFO_SIZE)
						{						
							if(DMLite)
							{
								FSMode = SNEEK;
							}
							else
							{
								if(*(vu32*)(GameInfo+DI_MAGIC_OFF) != 0xc2339f3d)
									strncpy(GameInfo+DI_MAGIC_OFF, "DSCX", 4);
									
								if(strncmp(GameInfo, "RELSAB", 6) == 0)
								{
									DVDClose(gi);
									sprintf(Path, "/games/%.63s/game.iso", DVDDirGetEntryName());
									gi = DVDOpen(Path, FA_READ);
									if(gi >= 0)
									{
										DVDSeek(gi, 0x424, 0);
										if(DVDRead(gi, buf1, 4) == 4)
										{
											if(*(vu32*)buf1 == 0x00439100)
												strncpy(GameInfo+DVD_REAL_NAME_OFF, "Mario Kart Arcade GP", 63);
											else if(*(vu32*)buf1 == 0x0051fc00)
												strncpy(GameInfo+DVD_REAL_NAME_OFF, "Mario Kart Arcade GP 2", 63);
										}									
										strncpy(GameInfo+WII_MAGIC_OFF, "MAME", 4);
									}
								}
							}
													
							if(cTitles)
							{
								char cmp[8];
								int ct;
								for(ct=0; ct<cTitles; ++ct)
								{
									strncpy(cmp, GT->GameTitle[ct], 6);
									cmp[6] = 0;
									if(strncmp(GameInfo, cmp, 6) == 0)
									{
										strncpy(GameInfo+DVD_REAL_NAME_OFF, GT->GameTitle[ct] + 9, 63);
										Asciify2(GameInfo+DVD_REAL_NAME_OFF);
										break;
									}
								}
							}
						
							memcpy(GameInfo+DVD_GAME_NAME_OFF, DVDDirGetEntryName(), strlen(DVDDirGetEntryName()) + 1);
							DVDWrite(fd, GameInfo, DVD_GAMEINFO_SIZE);
							CurrentGame++;

							if(DMLite)
								FSMode = UNEEK;
						}

						DVDClose(gi);
					}
				}
			}
			
			strcpy(Path, "/wbfs"); 
			if(DVDOpenDir(Path) == FR_OK)
			{
				while(DVDReadDir() == FR_OK)
				{					
					entry_found = 0;
					strcpy(WBFSPath, "/wbfs/");
					
					if(strlen(DVDDirGetEntryName()) > 63)
						continue;
						
					if(strchr(DVDDirGetEntryName(), '.') != NULL)
					{
						strcat(WBFSPath, DVDDirGetEntryName());
						
						if(strncmp(DVDDirGetEntryName()+strlen(DVDDirGetEntryName())-5, ".wbfs", 5) == 0)
						{							
							entry_found = 1;
						}				
						else
						{
							s32 fd_cf = DVDOpen(WBFSPath, FA_READ);
							if(fd_cf >= 0)
							{
								DVDClose(fd_cf);
								continue;
							}
						}
					}					
					
					if(!entry_found) 
					{					
						if(strlen(DVDDirGetEntryName()) == 6) 
						{
							strcpy(WBFSFile, DVDDirGetEntryName());
							entry_found = 1;
						}
						else 
						{					
							mch = strchr(DVDDirGetEntryName(), '_');
							if(mch - DVDDirGetEntryName() == 6)
							{ 						
								strncpy(WBFSFile, DVDDirGetEntryName(), 6);
								WBFSFile[6] = 0;
								entry_found = 1;
							}
							mch = strchr(DVDDirGetEntryName(), ']');
							if(mch - DVDDirGetEntryName()+1 == strlen(DVDDirGetEntryName())) 						
							{	
								strncpy(WBFSFile, DVDDirGetEntryName() + strlen(DVDDirGetEntryName()) - 7, 6);
								WBFSFile[6] = 0;
								entry_found = 1;
							}						
						}
						sprintf(WBFSPath, "/wbfs/%.63s/%s.wbfs", DVDDirGetEntryName(), WBFSFile);
					}
					if (!entry_found)
						continue;
						
					WBFS_Read(0x218, 4, buf1);
					if(*(vu32*)buf1 == 0x5d1c9ea3)
					{
						WBFS_Read(0x200, DVD_GAMEINFO_SIZE, GameInfo);
						strncpy(GameInfo+DI_MAGIC_OFF, "WBFS", 4);
						
						if(strncmp(GameInfo, WBFSFile, 6) != 0)
							strncpy(WBFSFile, GameInfo, 6);

						if(cTitles)
						{
							char cmp[8];
							int ct;
							for(ct=0; ct<cTitles; ++ct)
							{
								strncpy(cmp, GT->GameTitle[ct], 6);
								cmp[6] = 0;
								if(strcmp(WBFSFile, cmp ) == 0)
								{
									strncpy(GameInfo+DVD_REAL_NAME_OFF, GT->GameTitle[ct] + 9, 63);
									Asciify2(GameInfo+DVD_REAL_NAME_OFF);
									break;
								}
							}
						}
					
						memcpy(GameInfo+DVD_GAME_NAME_OFF, DVDDirGetEntryName(), strlen(DVDDirGetEntryName())+1);
						DVDWrite(fd, GameInfo, DVD_GAMEINFO_SIZE);
						CurrentGame++;
					}
				}
			}

			if(FSMode == UNEEK)
				break;

			FSMode = UNEEK;
			DMLite = 1;
		}

		if(DMLite)
			FSMode = SNEEK;
		
		free(GameInfo);
		hfree(buf1);
		free(GT);
		DVDClose(fd);
		
		free(DICfg);	
		fd = DVDOpen(cdiconfig, DREAD);
		DICfg = (DIConfig *)malloca(DVDGetSize(fd), 32);
		DVDRead(fd, DICfg, DVDGetSize(fd));
		DVDClose(fd);		

		char check1[64];
		char check2[64];
	
		u8 TitleC[DVD_GAMEINFO_SIZE];
	
		int k, j;
		if (CurrentGame > 0)
		{
			for(j=0; j<CurrentGame-1; ++j)
			{
				for(k=j+1; k<CurrentGame; ++k)
				{			
					sprintf(check1, "%.63s", &DICfg->GameInfo[j][0x20]);
					sprintf(check2, "%.63s", &DICfg->GameInfo[k][0x20]);
					upperCase(check1);
					upperCase(check2);
					if(strcmp(check1, check2)  > 0)
					{				
						memcpy(TitleC, DICfg->GameInfo[j], DVD_GAMEINFO_SIZE);
						memcpy(DICfg->GameInfo[j], DICfg->GameInfo[k], DVD_GAMEINFO_SIZE);
						memcpy(DICfg->GameInfo[k], TitleC, DVD_GAMEINFO_SIZE);				
					}
				}
			}
		}
		DVDDelete(cdiconfig);	
		fd = DVDOpen(cdiconfig, FA_CREATE_ALWAYS | FA_READ | FA_WRITE);	
		DVDWrite(fd, DICfg, CurrentGame * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE);
		DICfg->Gamecount = CurrentGame;
		DVDSeek(fd, 0, 0);
		DVDWrite(fd, DICfg, DVD_CONFIG_SIZE);		
		DVDClose(fd);
	}	

	/*** Read new config ***/

	free(DICfg);	
	fd = DVDOpen(cdiconfig, DREAD);
	DICfg = (DIConfig *)malloca( DVDGetSize(fd), 32);
	DVDRead(fd, DICfg, DVDGetSize(fd));
	DVDClose(fd);	
		
	free(Path);	
	return DI_SUCCESS;
}

u32 DMLite = 0;
s32 DVDSelectGame(int SlotID)
{
	if(bufcache_init)
	{					
		free(bufrb);
		bufcache_init = 0;
	}
	
	isparsed = 0;
	
	GameHook = 0;		
	InitCache();
	if(SlotID < 0 || SlotID >= DICfg->Gamecount)
		SlotID = 0;
		
	FSTable = (u8*)NULL;
	ChangeDisc = 1;
	DICover |= 1;
	char *str = (char *)malloca(256, 32);
	s32 fd=-1;
		
	if (DICfg->Gamecount > 0)
	{
		switch (*(vu32*)(DICfg->GameInfo[SlotID]+DI_MAGIC_OFF))
		{	
			case 0xc2339f3d: /*** Gamecube DML ***/
				FMode = IS_FST;				
				fd = 1;
				sprintf(GamePath, "/games/%.63s/", &DICfg->GameInfo[SlotID][0x60]);
				sprintf(str, "%sgame.iso", GamePath);
			
				DML_CFG *DMLCfg = (DML_CFG *)0x01200000;
				memset32(DMLCfg, 0, sizeof(DML_CFG));				
			
				DMLCfg->CfgVersion = 0x00000001;
				DMLCfg->Magicbytes = 0xD1050CF6;
				
				DMLCfg->VideoMode |= DML_VID_FORCE;
				
				SRAM_Init();
				
				SysSRAM *Sram;				
				Sram = SYS_LockSram();
				
				Sram->Flags &= 0x7F;
				Sram->Flags &= 0xFE;
				Sram->NTD &= 0xBF;
				
				switch(DICfg->Config&DML_VIDEO_CONF)
				{
					case DML_VIDEO_PAL50:							
						Sram->Flags |= 0x01;
						DMLCfg->VideoMode |= DML_VID_FORCE_PAL50;
					break;
					case DML_VIDEO_NTSC:						
						DMLCfg->VideoMode |= DML_VID_FORCE_NTSC;						
					break;
					case DML_VIDEO_PAL60:		
						Sram->Flags |= 0x80;
						Sram->Flags |= 0x01;
						Sram->NTD |= 0x40;
						DMLCfg->VideoMode |= DML_VID_FORCE_PAL60;
					break;
					case DML_VIDEO_PROG:		
						Sram->Flags |= 0x80;
						DMLCfg->VideoMode |= DML_VID_FORCE_PROG;
						DMLCfg->VideoMode |= DML_VID_PROG_PATCH;						
					break;
					case DML_VIDEO_PROGP:
						Sram->Flags |= 0x80;
						Sram->NTD |= 0x40;
						DMLCfg->VideoMode |= DML_VID_FORCE_PROG;
						DMLCfg->VideoMode |= DML_VID_PROG_PATCH;						
					break;
					default:
						/***  Todo: rewrite the next part ***/
						switch(DICfg->GameInfo[SlotID][3])
						{
							case 'J':
							case 'E':
							case 'K':
								DMLCfg->VideoMode |= DML_VID_FORCE_NTSC;
							break;
							case 'D':
							case 'F':
							case 'I':
							case 'M':
							case 'P':
							case 'S':
							case 'U':
								Sram->Flags |= 0x80;;
								Sram->Flags |= 0x01;
								Sram->NTD |= 0x40;
								DMLCfg->VideoMode |= DML_VID_FORCE_PAL60;
							break;
						}						
				}

				switch(DICfg->Config&DML_LANG_CONF)
				{
					case DML_LANG_ENGLISH:		
						Sram->Lang = 0;
					break;
					case DML_LANG_GERMAN:		
						Sram->Lang = 1;
					break;
					case DML_LANG_FRENCH:		
						Sram->Lang = 2;
					break;
					case DML_LANG_SPANISH:		
						Sram->Lang = 3;
					break;
					case DML_LANG_ITALIAN:		
						Sram->Lang = 4;
					break;
					case DML_LANG_DUTCH:		
						Sram->Lang = 5;
					break;
				}

				SYS_UnlockSram(1);
				SYS_SyncSram();
			
				memcpy(DMLCfg->GamePath, str, strlen(str));			
				DMLCfg->Config |= DML_CFG_GAME_PATH;
			
				if(DICfg->Config & DML_CHEATS)
				{
					sprintf(str, "%s%s.gct", GamePath, DICfg->GameInfo[SlotID]);
					memcpy(DMLCfg->CheatPath, str, strlen(str));
					DMLCfg->Config |= DML_CFG_CHEAT_PATH;
				}				
			
				if(DICfg->Config & DML_DEBUGGER)
					DMLCfg->Config |= DML_CFG_DEBUGGER;
				if(DICfg->Config & DML_DEBUGWAIT)
					DMLCfg->Config |= DML_CFG_DEBUGWAIT;
				if(DICfg->Config & DML_NMM)
					DMLCfg->Config |= DML_CFG_NMM;
				if(DICfg->Config & DML_NMM_DEBUG)
					DMLCfg->Config |= DML_CFG_NMM_DEBUG;
				if(DICfg->Config & DML_ACTIVITY_LED)
					DMLCfg->Config |= DML_CFG_ACTIVITY_LED;
				if(DICfg->Config & DML_PADHOOK)
					DMLCfg->Config |= DML_CFG_PADHOOK;	
			
				free(str);			
			break;
			case 0x44534358: /*** FST Extracted ***/		
				FMode = IS_FST;	
				sprintf(GamePath, "/games/%.63s/", &DICfg->GameInfo[SlotID][0x60]);
			
				sprintf(str, "%ssys/apploader.img", GamePath);
				fd = DVDOpen( str, DREAD );
			
				ApploaderSize = DVDGetSize(fd) >> 2;		
			break;
			case 0x57424653: /*** WBFS ***/
				FMode = IS_WBFS;
			
				if(KeyIDT > 0)
					DestroyKey(KeyIDT);
				
				KeyIDT = 0;
			
				char WBFSsFile[128];
		
				sprintf(WBFSFile, "%s", DICfg->GameInfo[SlotID]);
				
				sprintf(WBFSPath, "/custom/%s", WBFSFile);
				
				if(DVDOpenDir(WBFSPath) == FR_OK)
					isparsed = 1;
			
				if(strncmp((char *)&DICfg->GameInfo[SlotID][0x60]+strlen( (char *)( &DICfg->GameInfo[SlotID][0x60])) - 5, ".wbfs", 5) == 0)
				{
					strcpy(GamePath, "/wbfs/");
					sprintf(WBFSPath, "/wbfs/%.63s", &DICfg->GameInfo[SlotID][0x60]);
					strncpy(WBFSsFile, (char *)&DICfg->GameInfo[SlotID][0x60], strlen((char *)( &DICfg->GameInfo[SlotID][0x60])) - 5);
					WBFSsFile[strlen( (char *)( &DICfg->GameInfo[SlotID][0x60])) - 5] = '\0';
				}
				else
				{
					sprintf(GamePath, "/wbfs/%.63s/", &DICfg->GameInfo[SlotID][0x60]);
					sprintf(WBFSPath, "/wbfs/%.63s/%s.wbfs", &DICfg->GameInfo[SlotID][0x60], WBFSFile);
					strcpy(WBFSsFile, WBFSFile);
				}
			
				fd = DVDOpen(WBFSPath, FA_READ);
				if(fd >= 0)
				{			
					u8 *buf2 = (u8 *)halloca(0x480, 32);			

					u64 nOffset=0;			
					FS[0].Offset = nOffset+DVDGetSize(fd);
					FS[0].Size = DVDGetSize(fd);
					strcpy(FS[0].Path, WBFSPath);
					nOffset += FS[0].Size;
			
					splitcount=1;
					while(1)
					{			
						sprintf(str, "%s%s.wbf%d", GamePath, WBFSsFile, splitcount);
						s32 fd_sf = DVDOpen(str, FA_READ);
						if(fd_sf >= 0)
						{					
							FS[splitcount].Offset = nOffset+DVDGetSize(fd_sf);
							FS[splitcount].Size = DVDGetSize(fd_sf);
							strcpy(FS[splitcount].Path, str);
							nOffset += FS[splitcount].Size;					
							splitcount++;
						}
						else
						{
							break;
						}				
						DVDClose(fd_sf);
					}
				
					free(WBFSInf);
					free(WBFSFInf);				
					wii_sector_size_s = size_to_shift(0x8000);
					wii_sector_size = 1 << wii_sector_size_s;				
					max_wii_sec = 143432 * 2;				
					WBFSFInf = (WBFSFileInfo *)malloca(0x10, 32);				
					DVDSeek(fd, 0, 0);
					DVDRead(fd, WBFSFInf, 0x10);				
					max_wbfs_sec = max_wii_sec >> (WBFSFInf->wbfs_sector_size_s - wii_sector_size_s);				
					WBFSInf = (WBFSInfo *)malloca( 0x100 + ((max_wbfs_sec + 1) * sizeof(u16)), 32);				
					DVDSeek(fd, 0x200, 0);
					DVDRead(fd, WBFSInf, 0x100 + (max_wbfs_sec * sizeof(u16)));
					DVDSeek(fd, 0x240000, 0);
					DVDRead(fd, buf2, sizeof(u32) * 2);				
					u32 part_info_cnt = *(vu32*)(buf2);
					u32 part_info_off = *(vu32*)(buf2+0x04);				
					DVDSeek(fd, 0x200000 + (part_info_off << 2), 0);
					DVDRead(fd, buf2, ( sizeof(u32) * 2) * part_info_cnt);
					u32 i, part_info_type;
				
					for(i=0; i<part_info_cnt; ++i)
					{
						disc_part_offset = *(vu32*)(buf2+(i*0x08));
						part_info_type = *(vu32*)(buf2+0x04+(i*0x08));
						if(part_info_type == 0)
							break;
					}
 
					u16 blloc = disc_part_offset>>(WBFSFInf->wbfs_sector_size_s-2);
					bl_shift = WBFSFInf->wbfs_sector_size_s - WBFSFInf->hdd_sector_size_s;
					hdd_sector_size = 1 << WBFSFInf->hdd_sector_size_s;
					u16 valblloc = WBFSInf->disc_usage_table[blloc];
					wbfs_sector_size = 1 << WBFSFInf->wbfs_sector_size_s;
					blft_mask = (wbfs_sector_size-1)>>(WBFSFInf->hdd_sector_size_s);
					blft = (disc_part_offset >>(WBFSFInf->hdd_sector_size_s-2))&blft_mask;
					start_loc = blloc;
					start_val = valblloc;
					game_part_offset = ((valblloc<<bl_shift) + blft) * hdd_sector_size;				
					Rebuild_Disc_Usage_Table();

					if(strncmp(WBFSFile, "R3Oxxx", 3) == 0) { WBFSInf->disc_usage_table[0xf57] = 0x0348; } /*** Metroid Other M Fix ***/
					if(strncmp(WBFSFile, "RM3xxx", 3) == 0) { WBFSInf->disc_usage_table[0x19c] = 0x0008;   /*** Metroid Prime 3 Fix ***/
																WBFSInf->disc_usage_table[0x19d] = 0x0008; }
	
					WBFS_Read(game_part_offset, 0x480, buf2);
			
					DataOffset = *(vu32*)(buf2+0x2b8);
					DataSize   = *(vu32*)(buf2+0x2bc);
					TMDSize    = *(vu32*)(buf2+0x2a4);
					TMDOffset  = *(vu32*)(buf2+0x2a8);
					CertSize   = *(vu32*)(buf2+0x2ac);
					CertOffset = *(vu32*)(buf2+0x2b0);	
					DataOffset <<= 2;

					WBFS_Encrypted_Read(0, 0x480, buf2);
					DolOffset     = *(vu32*)(buf2+0x420);
					FSTableOffset = *(vu32*)(buf2+0x424);
					FSTableSize   = *(vu32*)(buf2+0x428);
					DolSize       = FSTableOffset - DolOffset;
				
					hfree(buf2);
				}
			break;
		}
	}
	if(fd < 0)
	{			
		free(str);
		DICover |= 1;
		ChangeDisc = 0;
		return DI_FATAL;
	}

		
	DVDClose(fd);
	free(str);

	/*** update di-config ***/
	fd = DVDOpen(cdiconfig, DWRITE);
	
	if(fd >= 0)
	{
		DICfg->SlotID = SlotID;
		DVDWrite(fd, DICfg, DVD_CONFIG_SIZE);
		DVDClose(fd);
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
	char Path[256];
	
	if( FMode == IS_WBFS )
	{
		u32 bl_offset, bl_length, i, BCRead;
		u32 woffset = Offset;
		u32 wlength = Length;
		s32 ret = DI_FATAL;

#ifdef DEBUG_PRINT_FILES							
		Search_FST( woffset, wlength, NULL, DEBUG_READ );
#endif
		if(isparsed == 1)
			Search_FST(0, 0, NULL, NII_PARSE_FST);
		
		if(isparsed == 2)
			if(Search_FST( Offset, Length, ptr, NII_READ ) == DI_SUCCESS)
				return DI_SUCCESS;
		
		while( wlength ) 
		{

			bl_offset = woffset % ( 0x7c00 >> 2 );
			bl_length = 0x7c00 - ( bl_offset << 2 );
			BCRead = 0;
		
			if ( bl_length > wlength ) 
			bl_length = wlength;

			if(BCRead != 1)
			{
				for( i=0; i < BLOCKCACHE_MAX; ++i )
				{
					if( BC[i].bl_num == 0xdeadbeef )
						continue;

					if( woffset / (0x7c00 >> 2) == BC[i].bl_num )
					{
						ret = DI_SUCCESS;
						memcpy( ptr, bufrb + (i * wii_sector_size) + ( bl_offset << 2 ), bl_length );
						BCRead = 1;
					}
				}
			}						
	
			if( BCRead != 1 )
			{
				if( BCEntry >= BLOCKCACHE_MAX )
					BCEntry = 0;
		
				u16 blloc, valblloc;
	
				blloc = ( woffset+disc_part_offset )>>( WBFSFInf->wbfs_sector_size_s-2 );		
				valblloc = WBFSInf->disc_usage_table[blloc];
	
				ret = WBFS_Read_Block( ( woffset / ( 0x7c00 >> 2 ) ) - ( valblloc * 0x10 ), BCEntry );								
				memcpy( ptr, bufrb + (BCEntry * wii_sector_size) + ( bl_offset << 2 ), bl_length );
										
				BC[BCEntry].bl_num = woffset / ( 0x7c00 >> 2 );
				BCEntry++;						
			}

			ptr += bl_length;
			woffset += bl_length >> 2;
			wlength -= bl_length;
		}
		if( Offset >= DolOffset && Offset < FSTableOffset )
			return Do_Dol_Patches( Length, ptr );

		if(ret != DI_SUCCESS) 
			ret = DI_FATAL;
			
		return ret;
	}
	else 
	{
		if( Offset < 0x110 )	/*** 0x440 ***/
		{		
			sprintf(Path, "%ssys/boot.bin", GamePath);
			fd = DVDOpen(Path, DREAD);
			if(fd < 0)
			{
				sprintf(Path, "%sgame.iso", GamePath);
				fd = DVDOpen(Path, FA_READ);
			} 
			if(fd < 0)
			{
				return DI_FATAL;
			} 
			else 
			{
				u8 *rbuf = (u8*)halloca(0x10, 32);

				/*** read requested data ***/
				DVDSeek(fd, Offset<<2, 0);
				DVDRead(fd, ptr, Length);
			
				/*** Read DOL/FST offset/sizes for later usage ***/
				DVDSeek(fd, 0x0420, 0);
				DVDRead(fd, rbuf, 0x10);
				DolOffset     = *(vu32*)(rbuf);
				FSTableOffset = *(vu32*)(rbuf+0x04);
				FSTableSize   = *(vu32*)(rbuf+0x08);

				hfree(rbuf);

				DolSize = FSTableOffset - DolOffset;
				FSTableSize <<= 2;

				DVDClose( fd );
				return DI_SUCCESS;
			}
		} 
		else if( Offset < 0x910 )	/*** 0x2440 ***/
		{
			Offset -= 0x110;

			sprintf( Path, "%ssys/bi2.bin", GamePath );
			fd = DVDOpen(Path, DREAD);
			if(fd < 0)
			{
				sprintf(Path, "%sgame.iso", GamePath);
				fd = DVDOpen(Path, FA_READ );
				if(fd >= 0)
					DVDSeek(fd, 0x440, 0);
				else
					return DI_FATAL;
			} 
			else 
				DVDSeek(fd, Offset<<2, 0);
				
			DVDRead(fd, ptr, Length);
			DVDClose(fd);

			/*** GC region patch ***/
			if( DiscType == DISC_DOL )
				*(vu32*)(ptr+0x18) = DICfg->Region;
		
			return DI_SUCCESS;
		} 
		else if( Offset < 0x910+ApploaderSize )	/*** 0x2440 ***/
		{
			Offset -= 0x910;

			sprintf( Path, "%ssys/apploader.img", GamePath );
			fd = DVDOpen( Path, DREAD );
			if( fd < 0 )
			{
				return DI_FATAL;
			} 
			else 
			{
				DVDSeek( fd, Offset<<2, 0 );
				DVDRead( fd, ptr, Length );
				DVDClose( fd );
				return DI_SUCCESS;
			}
		} 
		else if( Offset < DolOffset + DolSize )
		{
			Offset -= DolOffset;

			sprintf( Path, "%ssys/main.dol", GamePath );
			fd = DVDOpen( Path, DREAD );
			if( fd < 0 )
			{
				return DI_FATAL;
			} 
			else 
			{
				DVDSeek( fd, Offset<<2, 0 );
				DVDRead( fd, ptr, Length );
				DVDClose( fd );
			
				return Do_Dol_Patches( Length, ptr );
			}
		} 
		else if( Offset < FSTableOffset+(FSTableSize>>2) )
		{
			Offset -= FSTableOffset;

			sprintf( Path, "%ssys/fst.bin", GamePath );
			fd = DVDOpen( Path, DREAD );
			if( fd < 0 )
			{
				return DI_FATAL;
			} 
			else 
			{
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
				return WBFS_Encrypted_Read( Offset, Length, ptr );
			else 
				return DVDLowRead(Offset, Length, ptr);
			
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
	else 
	{
		DVDLowRead(Offset, Length, ptr);
	}

	if(*(vu32*)(ptr+0x1C) == 0xc2339f3d)
		DiscType = DISC_DOL;
	else if(*(vu32*)(ptr+0x18) == 0x5D1C9EA3)
		DiscType = DISC_REV;
	else 
		DiscType = DISC_INV;
	
	return DI_SUCCESS;
}

int DIP_Ioctl( struct ipcmessage *msg )
{
	u8  *bufin  = (u8*)msg->ioctl.buffer_in;
	//u32 lenin   = msg->ioctl.length_in;
	u8  *bufout = (u8*)msg->ioctl.buffer_io;
	u32 lenout  = msg->ioctl.length_io;
	s32 ret		= DI_FATAL;
	s32 fd		= -1;
			
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

			fd = DVDOpen( cdiconfig, FA_WRITE|FA_OPEN_EXISTING );
			if( fd < 0 )
			{
				DVDDelete( cdiconfig );
				fd = DVDOpen( cdiconfig, FA_WRITE|FA_CREATE_ALWAYS );
				if( fd < 0 )
				{
					hfree(name);
					ret = DI_FATAL;
					break;
				}
			}

			DVDWrite( fd, DICfg, DVD_CONFIG_SIZE );
			DVDClose( fd );					

			ret = DI_SUCCESS;
			hfree(name);
		} break;

		case DVD_READ_INFO:
		{
			u32 *vec = (u32*)msg->ioctl.buffer_in;

			fd = DVDOpen( cdiconfig, FA_READ );
		
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
			switchtimer = TimerCreate( 500000, 0, QueueID, 0xDEADDEAE );
			ret = DI_SUCCESS;

		} break;
		case DVD_LOAD_DISC:
		{
			u32 *vec = (u32*)msg->ioctl.buffer_in;
						
			u32 i;
			for(i = 0; i < DICfg->Gamecount; ++i)
			{
				dbgprintf("DIP:DVDLOADDISC CALLED (%08x) (%08x) (%08x)\n", vec[0], *(vu32*)(DICfg->GameInfo[i]), *(vu32*)(DICfg->GameInfo[i]+WII_MAGIC_OFF));
				if(vec[0] == *(vu32*)(DICfg->GameInfo[i]) && vec[1] == *(vu32*)(DICfg->GameInfo[i]+WII_MAGIC_OFF)) 
					break;
			}
			requested_game = i;
			switchtimer = TimerCreate( 500000, 0, QueueID, 0xDEADDEAE );
			ret = i;

		} break;
		case DVD_SET_AUDIO_BUFFER:
		{
			memset32( bufout, 0, lenout );
			
			if(isASEnabled == 0)
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
		case 0xDD:			/*** 0 out***/
		{
			ret = DI_SUCCESS;
		} break;
		case 0x95:			/*** 0x20 out ***/
		{
			*(u32*)bufout = 0;
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
				} 
				else 
				{
					ret = DVDLowRead( *(u32 *)( bufin+8 ), *(u32 *)( bufin+4 ), bufout );
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
			} 
			else if(  DiscType == DISC_REV )
			{
				if( *(u32*)(bufin+8) > 0x46090000 )
				{
					error = 0x00052100;
					return DI_ERROR;
				} 
				else 
				{
					ret = DVDLowReadUnencrypted( *(u32*)(bufin+8), *(u32*)(bufin+4), bufout );
				}
			} 
			else 
			{
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
			DICover  = 0x00;
			Partition = 0;

			ret = DI_SUCCESS;
		} break;
		case DVD_SET_MOTOR:
		{
			//Motor = 0;
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
		case DVD_CREATEDIR:
		{
			ret = DVDCreateDir( (char*)(v[0].data) );
		} break;
		case DVD_SEEK:
		{
			ret = DVDSeek((u32)(v[0].data), (u32)(v[1].data), (u32)(v[2].data));
		} break;
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
			ret = DVDRead( (u32)(v[0].data), (u8*)(v[1].data), v[1].len );
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
				if ( ret != DI_SUCCESS)
					ret = DI_FATAL;			
			}
			else 
			{
				sprintf( str, "%sticket.bin", GamePath );

				s32 fd = DVDOpen( str, FA_READ );
				if( fd < 0 )
				{
					ret = DI_FATAL;
					break;
				} 
				else 
				{
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
				TMD = (u8*)halloca( TMDSize, 32 );
			
				ret = WBFS_Read( game_part_offset + (TMDOffset << 2), TMDSize, TMD );
				if( ret != DI_SUCCESS )
				{
					ret = DI_FATAL;
					break;
				} 
				else 
				{				
					((u32*)buffer)[0x06] = (u32)TMD;
					((u32*)buffer)[0x07] = TMDSize;
					sync_before_read(TMD, (TMDSize+31)&(~31));

					PartitionSize = (u32)((*(u64*)(TMD+0x1EC)) >> 2 );

					if( v[3].data != NULL )
					{
						memcpy( v[3].data, TMD, TMDSize );
						sync_after_write( v[3].data, TMDSize );
					}				
				}
			}
			else 
			{
				sprintf( str, "%stmd.bin", GamePath );
			
				s32 fd = DVDOpen( str, FA_READ );
				if( fd < 0 )
				{
					ret = DI_FATAL;
					break;
				} 
				else 
				{
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
				ret = WBFS_Read( game_part_offset + (CertOffset << 2), 0xA00, CRT );
				if(  ret != DI_SUCCESS )
					ret = DI_FATAL;
			}
			else 
			{
				sprintf( str, "%scert.bin", GamePath );
				s32 fd = DVDOpen( str, FA_READ );
				if( fd < 0 )
				{
					ret = DI_FATAL;
					break;
				} 
				else 
				{				
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

			IOS_Ioctlv( ESHandle, 0x1C, 4, 2, buffer );

			IOS_Close( ESHandle );

			if( FMode == IS_FST )
			{
				*(u32*)(v[4].data) = 0x00000000;
				free( KeyID );
			}

			hfree(buffer);
			hfree(TIK);
			hfree(CRT);
			hfree(TMD);
			hfree(hashes);
			
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

s32 WBFS_Read(u64 offset, u32 length, void *ptr) 
{
	if(!splitcount)
	{
		if(!(strcmp(WBFSPath, FS[0].Path) == 0)) 
		{
			if(FC[0].File != 0xdeadbeef)
			{
				DVDClose(FC[0].File);
				FC[0].File = 0xdeadbeef;
			}
			
			FC[0].File = DVDOpen(WBFSPath, DREAD);
			if(FC[0].File < 0)
			{
				FC[0].File = 0xdeadbeef;

				error = 0x031100;
				return DI_FATAL;
			}
			else 
			{
				FC[0].Size = DVDGetSize(FC[0].File);
				FC[0].Offset = 0;
				strcpy(FS[0].Path, WBFSFile);
						
				DVDSeek(FC[0].File, offset, 0);
				DVDRead(FC[0].File, ptr, length);
				return DI_SUCCESS;
			}
		}
		else 
		{
			DVDSeek(FC[0].File, offset, 0);
			DVDRead(FC[0].File, ptr, length);
			return DI_SUCCESS;
		}
		error = 0x31100;
		return DI_FATAL;
	}
	int i, j;
		
	while(length)
	{
		for(i=0; i<FILESPLITS_MAX; ++i)
		{
			if(FS[i].Offset == 0xdeadbeef)
				continue;
				
			if(offset < FS[i].Offset && offset > (FS[i].Offset - FS[i].Size))
			{
				u32 doread = 0;
				for(j=0; j<FILECACHE_MAX; ++j)
				{
					if(FC[j].File == 0xdeadbeef)
						continue;						
					
					if(offset >= FC[j].Offset && offset < (FC[j].Offset + FC[j].Size))
					{
						u32 readsize = length;
						u64 nOffset = offset-FC[j].Offset;
	
						if(nOffset + readsize > FS[i].Offset)
							readsize = FS[i].Size - nOffset;
							
						DVDSeek(FC[j].File, nOffset, 0);
						DVDRead(FC[j].File, ptr, readsize);
						
						offset += readsize;
						ptr += readsize;
						length -= readsize;	
						doread = 1;

						if(length <= 0)
							return DI_SUCCESS;							
					}					
				}
				
				if(!doread)
				{
					if(FCEntry >= FILECACHE_MAX)
						FCEntry = 0;

					if(FC[FCEntry].File != 0xdeadbeef)
					{
						DVDClose(FC[FCEntry].File);
						FC[FCEntry].File = 0xdeadbeef;
					}
					
					FC[FCEntry].File = DVDOpen(FS[i].Path, DREAD);
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

						if(nOffset + readsize > FS[i].Offset)
							readsize = FS[i].Size - nOffset;						
						
						DVDSeek(FC[FCEntry].File, nOffset, 0);
						DVDRead(FC[FCEntry].File, ptr, length);
						
						offset += readsize;
						ptr += readsize;
						length -= readsize;
						
						FCEntry++;
						
						if(length <= 0)
							return DI_SUCCESS;
					}
				}				
			}
		}		
	}
	error = 0x31100;
	return DI_FATAL;
}

s32 WBFS_Read_Block(u64 block, u32 bl_num) 
{
	if(bufcache_init == 0)
	{
		bufrb = (u8 *)malloca(wii_sector_size * BLOCKCACHE_MAX, 0x40);
		bufcache_init = 1;
	}
	u8 *iv = (u8 *)malloca( 0x10, 0x40 );
	u64 offset;

	if(KeyIDT == 0)
		Set_key2();		

	u8* u8ptr = bufrb + (bl_num * wii_sector_size); 

	offset = game_part_offset + DataOffset + (wii_sector_size * block);
	
	WBFS_Read(offset, wii_sector_size, u8ptr);
	memcpy(iv, u8ptr + 0x3d0, 16);
	
	aes_decrypt_(KeyIDT, iv, u8ptr + 0x400 , 0x7c00, u8ptr);	
	free(iv);
	
	return DI_SUCCESS;
}

s32 WBFS_Encrypted_Read(u32 offset, u32 length, void *ptr)  
{
	u32 bl_offset, bl_length;

	u16 blloc, valblloc;

	while(length) {
		bl_offset = offset % (0x7c00 >> 2);
		bl_length = 0x7c00 - (bl_offset << 2);
		
		blloc = (offset+disc_part_offset)>>(WBFSFInf->wbfs_sector_size_s-2);		
		valblloc = WBFSInf->disc_usage_table[blloc];
		
		if(bl_length > length) 
			bl_length = length;
		
		WBFS_Read_Block((offset / (0x7c00 >> 2)) - (valblloc * 0x10), 0);
		memcpy(ptr, bufrb + (bl_offset << 2), bl_length);		

		ptr += bl_length;
		offset += bl_length >> 2;
		length -= bl_length;
	}
	return DI_SUCCESS;
}

/*s32 WBFS_Decrypted_Write(char *path, char *filename, u32 offset, u32 length)
{
	u32 bl_offset, bl_length;
	char *str = (char *)malloca(128, 32);
	sprintf(str, "%s/%s", path, filename);
	DVDCreateDir(path);
	s32 writefd = DVDOpen( str, FA_WRITE|FA_READ|FA_CREATE_ALWAYS );
	
	while(length) 
	{		
		bl_offset = offset % (0x7c00 >> 2);
		bl_length = 0x7c00 - (bl_offset << 2);
		
		if(bl_length > length) 
			bl_length = length;

		u16 blloc, valblloc;
	
		blloc = (offset+disc_part_offset )>>( WBFSFInf->wbfs_sector_size_s-2);		
		valblloc = WBFSInf->disc_usage_table[blloc];

		BC[0].bl_num = 0xdeadbeef;
		WBFS_Read_Block((offset / (0x7c00 >> 2)) - (valblloc * 0x10), 0);
		DVDWrite(writefd, bufrb + (bl_offset << 2), bl_length);					

		offset += bl_length >> 2;
		length -= bl_length;
	}
	
	DVDClose(writefd);
	free(str);
	return DI_SUCCESS;
}*/

s32 Search_FST(u32 Offset, u32 Length, void *ptr, u32 mode)
{
	char Path[256];
	char File[64];
	
	u32 i,j;

	if(FSTable == NULL)
		FSTable	= (u8 *)((*(vu32 *)0x38)&0x7FFFFFFF);	

	if(mode == FST_READ)
	{
		for(i=0; i < FILECACHE_MAX; ++i)
		{
			if(FC[i].File == 0xdeadbeef)
				continue;

			if(Offset >= FC[i].Offset)
			{
				u64 nOffset = ((u64)(Offset-FC[i].Offset)) << 2;
				if(nOffset < FC[i].Size)
				{
					DVDSeek(FC[i].File, nOffset, 0);
					DVDRead(FC[i].File, ptr, ((Length)+31)&(~31));
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

	for(i=1; i < Entries; ++i)
	{
		if(level)
		{
			while(LEntry[level-1] == i)
			{
				level--;
			}
		}

		if(fe[i].Type)
		{
			if(fe[i].NextOffset == i+1)
				continue;

			Entry[level] = i;
			LEntry[level++] = fe[i].NextOffset;
			if(level > 15)
				break;
		} 
		else 
		{		
			switch(mode)
			{	
				case NII_PARSE_FST:
				{
					memset(Path, 0, 256);	
					memset(File, 0, 64);					
						
					memcpy(File, NameOff + fe[i].NameOffset, strlen(NameOff + fe[i].NameOffset));				
					
					sprintf(Path, "/custom/%s/%s", WBFSFile, File);
					s32 niifd = DVDOpen(Path, DREAD);
					if(niifd >= 0)
					{
						dbgprintf("CDI:Found: %s (%d)\n", File, fe[i].FileLength);
						fe[i].FileLength = DVDGetSize(niifd);
						DVDClose(niifd);
						isparsed = 2;
						dbgprintf("CDI:Changed size: %s (%d)\n", File, fe[i].FileLength);
					}
					continue;					
				} break;
				case DEBUG_READ:
				case FST_READ:
				case NII_READ:
				{	
					if(Offset >= fe[i].FileOffset)
					{
						u64 nOffset = ((u64)(Offset-fe[i].FileOffset)) << 2;
						if(nOffset < fe[i].FileLength)
						{
							memset(Path, 0, 256);							
							sprintf(Path, "%sfiles/", GamePath);

							for(j=0; j<level; ++j)
							{
								if(j)
									Path[strlen(Path)] = '/';

								memcpy( Path+strlen(Path), NameOff + fe[Entry[j]].NameOffset, strlen(NameOff + fe[Entry[j]].NameOffset ) );
							}
							if(level)
								Path[strlen(Path)] = '/';

							if(mode == NII_READ)
							{
								memset(Path, 0, 256);
								sprintf(Path, "/custom/%s/", WBFSFile);
							}
						
							memcpy(Path+strlen(Path), NameOff + fe[i].NameOffset, strlen(NameOff + fe[i].NameOffset));
													
#ifdef DEBUG_PRINT_FILES
								dbgprintf("Checking file: %s (%d)\n", Path, Length);
#endif
							
							if(mode == FST_READ || mode == NII_READ)
							{
							
								if(FCEntry >= FILECACHE_MAX)
									FCEntry = 0;

								if(FC[FCEntry].File != 0xdeadbeef)
								{
									DVDClose(FC[FCEntry].File);
									FC[FCEntry].File = 0xdeadbeef;
								}
							
								Asciify(Path);							
							
								FC[FCEntry].File = DVDOpen(Path, DREAD);
								if(FC[FCEntry].File < 0)
								{
									FC[FCEntry].File = 0xdeadbeef;

									if(mode == FST_READ)
										error = 0x031100;
										
									return DI_FATAL;
								} 
								else 
								{
#ifdef DEBUG_PRINT_FILES
								dbgprintf("Found file: %s (%d)\n", Path, Length);
#endif
									FC[FCEntry].Size	= fe[i].FileLength;
									FC[FCEntry].Offset	= fe[i].FileOffset;

									DVDSeek(FC[FCEntry].File, nOffset, 0);
									DVDRead(FC[FCEntry].File, ptr, Length);
							
									FCEntry++;

									return DI_SUCCESS;
								}
							}
							else
							{
#ifdef DEBUG_PRINT_FILES
								dbgprintf("Reading file: %s (%d)\n", Path, Length);
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
	if(mode == NII_PARSE_FST)		
		return DI_SUCCESS;

	return DI_FATAL;
}

s32 Do_Dol_Patches( u32 Length, void *ptr )
{
	s32 fd;
	u32 DebuggerHook=0;
	char Path[256];
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
					if( *(vu32*)(ptr+i+0) == 0x3C808000 && *(vu32*)(ptr+i+4) == 0x38000004 && *(vu32*)(ptr+i+8) == 0x808400E4 )
					{
						int j = 0;
						while( *(vu32*)(ptr+i+j) != 0x4E800020 )
							j+=4;

						DebuggerHook = (u32)ptr + i + j;
					}
				} break;
			}					
		} 
		else if( (DICfg->Config & CONFIG_PATCH_FWRITE) )
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
	{
		if( DebuggerHook && (DICfg->Config & CONFIG_DEBUG_GAME) )
		{
			GameHook = 0xdeadbeef;

			strcpy(	Path, "/sneek/kenobiwii.bin" );
				
			fd = IOS_Open( Path, 1 );
			if( fd < 0 )					
				return DI_SUCCESS;

			u32 Size = IOS_Seek( fd, 0, 2 );
			IOS_Seek( fd, 0, 0 );

			/*** Read file to memory ***/
			s32 ret = IOS_Read( fd, (void*)0x1800, Size );
			if( ret != Size )
			{
				IOS_Close( fd );
				return DI_SUCCESS;
			}
			IOS_Close( fd );

			*(vu32*)((*(vu32*)0x1808)&0x7FFFFFFF) = !!(DICfg->Config & CONFIG_DEBUG_GAME_WAIT);

			memcpy( (void *)0x1800, (void*)0, 6 );

			u32 newval = 0x00018A8 - DebuggerHook;
				newval&= 0x03FFFFFC;
				newval|= 0x48000000;

			*(vu32*)(DebuggerHook) = newval;
				
			if( FMode == IS_WBFS )
				sprintf( Path, "/sneek/cheat/%s.gct", WBFSFile );
			else
				sprintf( Path, "%scodes.gct", GamePath );
				
				
			fd = IOS_Open( Path, 1 );
			if ( fd < 0)
				return DI_SUCCESS;
				
			Size = IOS_Seek( fd, 0, 2 );
			IOS_Seek( fd, 0, 0 );
					
			ret = IOS_Read( fd, (void*)0x27D0, Size );
			if( ret != Size )
			{
				IOS_Close( fd );
				return DI_SUCCESS;
			}
			IOS_Close( fd );
			*(vu8*)0x1807 = 0x01;
		}
		return DI_SUCCESS;	
	}
	return DI_FATAL;
}
