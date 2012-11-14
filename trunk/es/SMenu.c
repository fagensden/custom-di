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

#include "SMenu.h"
#include "svn.h"
#include "patch.h"


u32 FrameBuffer	= 0;
u32 FBOffset	= 0;
u32 FBEnable	= 0;
u32	FBSize		= 0;
u32 *WPad		= NULL;
u32 *GameCount;
u32 *NandCount;

u32 ShowMenu=0;
u32 MenuType=0;
u32 DVDStatus = 0;
u32 DVDType = 0;
u32 DVDError=0;
u32 SLock=0;
s32 PosX=0,ScrollX=0;
u32 fnnd=0;
u32 rebreq=0;
u32 nisp=0;

u32 *FB;

u32 Freeze;
u32 value;
u32 *offset;

extern u32 FSUSB;

u32 PosValX;
u32 Hits;
u32 edit;
u32 *Offsets;

GCPadStatus GCPad;

u32 DIConfType; 

DIConfig *DICfg;
DIConfigO *DICfgO;
NandConfig *NandCfg;
ChannelCache *channelCache;
HacksConfig *PL;

u32 DVDErrorSkip=0;
u32 DVDErrorRetry=0;
double DVDTimer = 0;
u32 DVDTimeStart = 0;
u32 DVDSectorSize = 0;
u32 DVDOffset = 0;
u32 DVDOldOffset = 0;
u32 DVDSpeed = 0;
u32 DVDTimeLeft = 0;
s32 DVDHandle = 0;
u32 DMLVideoMode = 0;
u32 DVDReinsertDisc=false;
s32 EntryCount = 20;
u32 CacheSel = 0;
char *DiscName	= (char*)NULL;
char *DVDTitle	= (char*)NULL;
char *DVDBuffer	= (char*)NULL;

bool MIOSIsDML = true;

extern char diroot[0x20];
extern u32 LoadDI;

u32 startaddress = 0;
u32 endaddress = 0;

char *RegionStr[] = 
{
	"JAP",
	"USA",
	"EUR",
	"KOR",
	"ASN",
	"LTN",
	"UNK",
	"ALL",
};

char *LanguageStr[] = 
{
	"Japanese",
	"English",
	"German",
	"French",
	"Spanish",
	"Italian",
	"Dutch",
	"Chinese Simple",
	"Chinese Traditional",
	"Korean",
};

char *BootStr[] = 
{	
	"System Menu",
	"Installed Channel",	
	"Installed DOL",
};

char *RegStr[] =
{
	"No Patch",
	"NTSC-U",
	"NTSC-J",
	"NTSC-K",
	"PAL",
};

char *VidStr[] =
{
	"PAL50 (576i)",
	"PAL50 (576p)",
	"PAL60 (480i)",
	"PAL60 (480p)",
	"NTSC (480i)",
	"NTSC (480p)",
	"NTSC (480i) PAL Enc.",
	"NTSC (480p) PAL Enc.",
};

char *gTypeStr[] =
{
	"MAME",
	"GC",
	"WBFS",
	"FST",
	"Invalid",
};

char *DmpStr[] =
{
	"Extract",
	"Available",
	"NA",
};

char *CacheStr[] =
{
	"Game",
	"Channel",
	"Nand",
};

unsigned char VISetFB[] =
{
    0x7C, 0xE3, 0x3B, 0x78,		//	mr      %r3, %r7
	0x38, 0x87, 0x00, 0x34,
	0x38, 0xA7, 0x00, 0x38,
	0x38, 0xC7, 0x00, 0x4C, 
};

void LoadAndRebuildChannelCache()
{
	channelCache = NULL;
	u32 size, i;
	UIDSYS *uid = (UIDSYS *)NANDLoadFile("/sys/uid.sys", &size);
	if(uid == NULL)
		return;
		
	u32 numChannels = 0;
	for(i = 0; i * 12 < size; i++)
	{
		u32 majorTitleID = uid[i].TitleID >> 32;
		switch (majorTitleID)
		{
			case 0x00000001: //IOSes
			case 0x00010000: //left over disc stuff
			case 0x00010005: //DLC
			case 0x00010008: //Hidden
				break;
			default:
			{
				s32 fd = ES_OpenContent(uid[i].TitleID, 0);
				if (fd >= 0)
				{
					numChannels++;
					IOS_Close(fd);
				}
			} break;
		}
	}

	channelCache = (ChannelCache*)NANDLoadFile("/sneekcache/channelcache.bin", &i);
	if(channelCache == NULL)
	{
		channelCache = (ChannelCache*)malloca(sizeof(ChannelCache), 32);
		channelCache->numChannels = 0;
	}

	if(numChannels != channelCache->numChannels || i != sizeof(ChannelCache) + sizeof(ChannelInfo) * numChannels)
	{
		free(channelCache);
		channelCache = (ChannelCache*)malloca(sizeof(ChannelCache) + sizeof(ChannelInfo) * numChannels, 32);
		channelCache->numChannels = 0;
		for(i = 0; i * 12 < size; i++)
		{
			u32 majorTitleID = uid[i].TitleID >> 32;
			switch (majorTitleID){
				case 0x00000001: //IOSes
				case 0x00010000: //left over disc stuff
				case 0x00010005: //DLC
				case 0x00010008: //Hidden
					break;
				default:
				{
					s32 fd = ES_OpenContent(uid[i].TitleID, 0);
					if(fd >= 0)
					{
						IOS_Seek(fd, 0xF0, SEEK_SET);
						u32 j;
						for(j = 0; j < 40; j++)
						{
							IOS_Seek(fd, 1, SEEK_CUR);
							IOS_Read(fd, &channelCache->channels[channelCache->numChannels].name[j], 1);
						}
						channelCache->channels[channelCache->numChannels].name[40] = 0;
						channelCache->channels[channelCache->numChannels].titleID = uid[i].TitleID;
						channelCache->numChannels += 1;
						IOS_Close(fd);
					}
				} break;
			}
		}
		NANDWriteFileSafe("/sneekcache/channelcache.bin",channelCache,sizeof(ChannelCache) + sizeof(ChannelInfo) * channelCache->numChannels);
	}
	free(uid);
}

void __configloadcfg(void)
{
	u32 size;
	PL = NULL;
	PL = (HacksConfig *)NANDLoadFile( "/sneekcache/hackscfg.bin", &size );
	if( PL == NULL )
	{
		PL = (HacksConfig *)malloca( sizeof(HacksConfig), 32);
		PL->EULang   = 1;
		PL->USLang   = 1;
		PL->PALVid	 = 1;
		PL->NTSCVid	 = 5;
		PL->Shop1    = 0;
		PL->Config   = 0;
		PL->Autoboot = 0;
		PL->ChNbr    = 0;
		PL->DolNr    = 0;
		PL->TitleID  = 0x0000000100000002LL;
	}
	NANDWriteFileSafe( "/sneekcache/hackscfg.bin", PL , sizeof(HacksConfig) );
}

int __GetGameFormat(u32 MagicA, u32 MagicB)
{
	if(MagicA == 0xc2339f3d && MagicB == 0x4d414d45)
		return MAME;
	else if(MagicA == 0xc2339f3d)
		return GC;
	else if(MagicA == 0x57424653 && MagicB == 0x5D1C9EA3)
		return WBFS;
	else if(MagicA == 0x44534358 && MagicB == 0x5D1C9EA3)
		return FST;
	else
		return INV;
}

u32 SMenuFindOffsets(void *ptr, u32 SearchSize)
{
	u32 i;
	u32 r13  = 0;

	FBOffset = 0;
	FBEnable = 0;
	WPad	 = (u32*)NULL;
		
	//dbgprintf("ES:Start:%p Len:%08X\n", ptr, SearchSize );

	while(1)
	{
		for( i = 0; i < SearchSize; i+=4 )
		{
			if( *(u32*)(ptr+i) >> 16 == 0x3DA0 && r13 == 0 )
			{
				r13 = ((*(u32*)(ptr+i)) & 0xFFFF) << 16;
				r13|= (*(u32*)(ptr+i+4)) & 0xFFFF;
			}

			if( memcmp( ptr+i, VISetFB, sizeof(VISetFB) ) == 0 && FBEnable == 0 )
			{
				FBEnable = ( *(u32*)(ptr+i+sizeof(VISetFB)) );

				if( FBEnable & 0x8000 )
				{
					FBEnable = ((~FBEnable) & 0xFFFF) + 1;
					FBEnable = (r13 - FBEnable) & 0x7FFFFFF;
				} else {
					FBEnable = FBEnable & 0xFFFF;
					FBEnable = (r13 + FBEnable) & 0x7FFFFFF;
				}
				FBOffset = FBEnable + 0x18;
			}


			//Wpad pattern new
			if( (*(u32*)(ptr+i+0x00)) >> 16 == 0x1C03		&&		//  mulli   %r0, %r3, 0x688
				(*(u32*)(ptr+i+0x04)) >> 16 == 0x3C60		&&		//  lis     %r3, inside_kpads@h
				(*(u32*)(ptr+i+0x08)) >> 16 == 0x3863		&&		//  addi    %r3, %r3, inside_kpads@l
				(*(u32*)(ptr+i+0x0C))	    == 0x7C630214	&&		//  add     %r3, %r3, %r0
				(*(u32*)(ptr+i+0x10)) >> 16 == 0xD023		&&		//  stfs    %fp1, 0xF0(%r3)
				(*(u32*)(ptr+i+0x18))	    == 0x4E800020			//  blr
				)
			{
				if( *(u32*)(ptr+i+0x08) & 0x8000 )
					WPad = (u32*)( ((((*(u32*)(ptr+i+0x04)) & 0xFFFF) << 16) - (((~(*(u32*)(ptr+i+0x08))) & 0xFFFF)+1) ) & 0x7FFFFFF );
				else
					WPad = (u32*)( ((((*(u32*)(ptr+i+0x04)) & 0xFFFF) << 16) + ((*(u32*)(ptr+i+0x08)) & 0xFFFF)) & 0x7FFFFFF );
			}

			//WPad pattern old
			if( (*(u32*)(ptr+i+0x00)) >> 16 == 0x3C80		&&		//  lis     %r3, inside_kpads@h
				(*(u32*)(ptr+i+0x04))		== 0x5460502A	&&		//  slwi    %r0, %r3, 10
				(*(u32*)(ptr+i+0x08)) >> 16 == 0x3884		&&		//  addi    %r3, %r3, inside_kpads@l
				(*(u32*)(ptr+i+0x0C))	    == 0x7C640214	&&		//  add     %r3, %r4, %r0
				(*(u32*)(ptr+i+0x10)) >> 16 == 0xD023		&&		//  stfs    %fp1, 0xF0(%r3)
				(*(u32*)(ptr+i+0x18))	    == 0x4E800020			//  blr
				)
			{
				if( *(u32*)(ptr+i+0x08) & 0x8000 )
					WPad = (u32*)( ((((*(u32*)(ptr+i+0x00)) & 0xFFFF) << 16) - (((~(*(u32*)(ptr+i+0x08))) & 0xFFFF)+1) ) & 0x7FFFFFF );
				else
					WPad = (u32*)( ((((*(u32*)(ptr+i+0x00)) & 0xFFFF) << 16) + ((*(u32*)(ptr+i+0x08)) & 0xFFFF)) & 0x7FFFFFF );
			}
			
			if( r13 && FBEnable && FBOffset && WPad != NULL )
			{
				switch( *(vu32*)(FBEnable+0x20) )
				{
					case VI_NTSC:
						FBSize = 304*480*4;
						break;
					case VI_PAL:
						FBSize = 288*432*4;
						break;
					case VI_EUR60:
						FBSize = 320*480*4;
						break;
					default:
						dbgprintf("ES:SMenuFindOffsets():Invalid Video mode:%d\n", *(vu32*)(FBEnable+0x20) );
						break;
				}

				return 1;
			}
		}
	}
	return 0;
}

void ApplyPatch(u8 *hash, u32 hashsize, u8 *patch, u32 patchsize)
{
	u32 step = 0;
	for(step = startaddress; step < endaddress; ++step)
	{
		if(!memcmp((void *)step, hash, hashsize))
		{
			dbgprintf("ES:Patching @ offset: 0x%08X\n", step);
			memcpy((void *)step, patch, patchsize);
		}
	}
}

void SMenuInit( u64 TitleID, u16 TitleVersion )
{
	int i;
	value	= 0;
	Freeze	= 0;
	ShowMenu= 0;
	if (LoadDI == true)
		MenuType= 0;
	else
		MenuType= 4;
	SLock	= 0;
	PosX	= 0;
	ScrollX	= 0;
	PosValX	= 0;
	Hits	= 0;
	edit	= 0;
	DVDStatus = 0;
	DVDError=0;
	DVDReinsertDisc=false;
	DICfg	= NULL;
	NandCfg = NULL;

	Offsets		= (u32*)malloca( sizeof(u32) * MAX_HITS, 32 );
	GameCount	= (u32*)malloca( sizeof(u32), 32 );
	FB			= (u32*)malloca( sizeof(u32) * MAX_FB, 32 );	
	
	__configloadcfg();

	for( i=0; i < MAX_FB; ++i )
		FB[i] = 0;

//Patches and SNEEK Menu
	switch(TitleID)
	{
		case 0x0000000100000002LL:
		{
			char path[256];
			u32 size, res;
			_sprintf(path, "/title/%08x/%08x/content/title.tmd", (u32)(TitleID>>32), (u32)(TitleID));
			TitleMetaData *TMD = (TitleMetaData *)NANDLoadFile( path, &size );			
			dolhdr *menuhdr = (dolhdr *)heap_alloc_aligned(0, ALIGN32(sizeof(dolhdr)), 32);
			memset32(menuhdr, 0, ALIGN32(sizeof(dolhdr)));			
			_sprintf(path, "/title/%08x/%08x/content/%08X.app", (u32)(TitleID>>32), (u32)(TitleID), TMD->Contents[TMD->BootIndex].ID);
			free(TMD);
			menuhdr = (dolhdr *)NANDReadFromFile(path, 0, ALIGN32(sizeof(dolhdr)), &res);			
			startaddress =  (u32)(*menuhdr->addressData - *menuhdr->offsetData);
			endaddress = (u32)(*menuhdr->addressData + *menuhdr->sizeData);			
			startaddress -= 0x80000000;
			endaddress -= 0x80000000;
			
			if(PL->Config&CONFIG_REGION_FREE)
			{
				dbgprintf("ES:Patching for \"Region Free Wii Games\":\n");
				if(TitleVersion >= 480 && TitleVersion <= 518)
				{
					ApplyPatch(hash_B, sizeof(hash_B), patch_B, sizeof(patch_B));
					ApplyPatch(hash_C, sizeof(hash_C), patch_B, sizeof(patch_B));
					ApplyPatch(hash_D, sizeof(hash_D), patch_C, sizeof(patch_C));
					ApplyPatch(hash_E, sizeof(hash_E), patch_A, sizeof(patch_A));
					ApplyPatch(hash_F, sizeof(hash_F), patch_D, sizeof(patch_D));
				}
				else if(TitleVersion >= 416 && TitleVersion <= 454)
				{
					ApplyPatch(hash_G, sizeof(hash_G), patch_E, sizeof(patch_E));
					ApplyPatch(hash_H, sizeof(hash_H), patch_B, sizeof(patch_B));
					ApplyPatch(hash_I, sizeof(hash_I), patch_C, sizeof(patch_C));
					ApplyPatch(hash_J, sizeof(hash_J), patch_D, sizeof(patch_D));
				}
				else if(TitleVersion >= 288 && TitleVersion <= 390)
				{
					ApplyPatch(hash_Q, sizeof(hash_Q), patch_D, sizeof(patch_D));
					ApplyPatch(hash_R, sizeof(hash_R), patch_B, sizeof(patch_B));
				}
				
				dbgprintf("ES:Patching for \"Region Free Gamecube Games\":\n");
				
				if(TitleVersion >= 288 && TitleVersion <= 390)
				{
					ApplyPatch(hash_S, sizeof(hash_S), patch_I, sizeof(patch_I));
				}
				else if(TitleVersion >= 416 && TitleVersion <= 454)
				{
					ApplyPatch(hash_U, sizeof(hash_U), patch_I, sizeof(patch_I));
				}
				else if(TitleVersion >= 480 && TitleVersion <= 518)
				{
					ApplyPatch(hash_T, sizeof(hash_T), patch_I, sizeof(patch_I));
				}
			}
			
			if(PL->Config&CONFIG_MOVE_DISC_CHANNEL)
			{
				dbgprintf("ES:Patching for \"Move Disc Channel\":\n");
				if(TitleVersion >= 288 && TitleVersion <= 518)
					ApplyPatch(hash_K, sizeof(hash_K), patch_A, sizeof(patch_A));
			}
			
			if(PL->Config&CONFIG_PRESS_A)
			{
				dbgprintf("ES:Patching for \"Auto Press A at Health Screen\":\n");
				if(TitleVersion >= 288 && TitleVersion <= 518)
					ApplyPatch(hash_L, sizeof(hash_L), patch_F, sizeof(patch_F));
			}
			
			if(PL->Config&CONFIG_NO_BG_MUSIC)
			{
				dbgprintf("ES:Patching for \"No System Menu Sounds AT ALL\":\n");
				if(TitleVersion >= 416 && TitleVersion <= 518)
					ApplyPatch(hash_M, sizeof(hash_M), patch_G, sizeof(patch_G));
			}
			
			if(PL->Config&CONFIG_NO_SOUND)
			{
				dbgprintf("ES:Patching for \"No System Menu Background Music\":\n");
				if(TitleVersion >= 416 && TitleVersion <= 518)
					ApplyPatch(hash_N, sizeof(hash_N), patch_G, sizeof(patch_G));
			}
			
			if(PL->Config&CONFIG_BLOCK_DISC_UPDATE)
			{
				dbgprintf("ES:Patching for \"Skip Disc Update Check\":\n");
				if(TitleVersion >= 1 && TitleVersion <= 511)
				{
					ApplyPatch(hash_Z, sizeof(hash_Z), patch_K, sizeof(patch_K));
					ApplyPatch(hash_1, sizeof(hash_1), patch_K, sizeof(patch_K));
				}
				if(TitleVersion >= 512 && TitleVersion <= 518)
				{
					ApplyPatch(hash_3, sizeof(hash_3), patch_K, sizeof(patch_K));
					ApplyPatch(hash_4, sizeof(hash_4), patch_K, sizeof(patch_K));
				}
				if(TitleVersion >= 288 && TitleVersion <= 511)
					ApplyPatch(hash_2, sizeof(hash_2), patch_K, sizeof(patch_K));
			}
			
			if(*(vu32*)0x01200000 == 0x47414d45)
			{
				dbgprintf("ES:Patching for \"Auto Boot Disc\":\n");
				ApplyPatch(hash_O, sizeof(hash_O), patch_A, sizeof(patch_A));
				ApplyPatch(hash_P, sizeof(hash_P), patch_H, sizeof(patch_H));
			}
		} break;
	}
}
void SMenuAddFramebuffer( void )
{
	u32 i,j,f;

	if( *(vu32*)FBEnable != 1 )
		return;

	FrameBuffer = (*(vu32*)FBOffset) & 0x7FFFFFFF;

	for( i=0; i < MAX_FB; i++)
	{
		if( FB[i] )	//add a new entry
			continue;
		
		//check if we already know this address
		f=0;
		for( j=0; j<i; ++j )
		{
			if( FrameBuffer == FB[j] )	// already known!
			{
				f=1;
				return;
			}
		}
		if( !f && FrameBuffer && FrameBuffer < 0x14000000 )	// add new entry
		{
			//dbgprintf("ES:Added new FB[%d]:%08X\n", i, FrameBuffer );
			FB[i] = FrameBuffer;
		}
	}
}
void SMenuDraw(void)
{
	u32 i, j;	

	if(*(vu32*)FBEnable != 1 || !ShowMenu || DICfg == NULL)
		return;

	for(i=0; i < MAX_FB; i++)
	{
		if(FB[i] == 0)
			continue;

		if(DICfg->Region > ALL)
			DICfg->Region = ALL;

		if(MenuType != 3)
			PrintFormat(FB[i], MENU_POS_X, 20, "%sNEEK2O%s r%s %s %s %s: %d",  FSUSB ? "U" : "S", LoadDI ? "+DI" : "", SVN_REV, SVN_BETA, __DATE__, MenuType == 4 ? "Channels" : "Games", MenuType == 4 ? channelCache->numChannels : *GameCount);

		switch(MenuType)
		{
			case 0:
			{
				u32 gRegion = 0;

				switch(*(u8*)(DICfg->GameInfo[PosX+ScrollX] + 3))
				{
					case 'X':	// hamster heroes uses this for some reason
					case 'A':
					case 'Z':
						gRegion =  ALL;
						break;
					case 'E':
						gRegion =  USA;
						break;
					case 'F':	// France
					case 'I':	// Italy
					case 'U':	// United Kingdom
					case 'S':	// Spain
					case 'D':	// Germany
					case 'P':	
						gRegion =  EUR;
						break;
					case 'J':
						gRegion =  JAP;
						break;
					default:
						gRegion =  UNK;
						break;
				}

				if(LoadDI == true)				
					PrintFormat(FB[i], MENU_POS_X, MENU_POS_Y, "Game region: %s / Menu region: %s", RegionStr[gRegion], RegionStr[DICfg->Region]);

				for(j = 0; j < EntryCount; ++j)
				{
					if(j+ScrollX >= *GameCount)
						break;

					if(__GetGameFormat(*(vu32*)(DICfg->GameInfo[ScrollX+j]+0x1C), *(vu32*)(DICfg->GameInfo[ScrollX+j]+0x18)) == INV)
						PrintFormat(FB[i], MENU_POS_X, MENU_POS_Y+32+16*j, "%.38s (%s)", DICfg->GameInfo[ScrollX+j] + 0x20, gTypeStr[__GetGameFormat(*(vu32*)(DICfg->GameInfo[ScrollX+j]+0x1C), *(vu32*)(DICfg->GameInfo[ScrollX+j]+0x18))]);
					else
						PrintFormat(FB[i], MENU_POS_X, MENU_POS_Y+32+16*j, "%.40s (%s)", DICfg->GameInfo[ScrollX+j] + 0x20, gTypeStr[__GetGameFormat(*(vu32*)(DICfg->GameInfo[ScrollX+j]+0x1C), *(vu32*)(DICfg->GameInfo[ScrollX+j]+0x18))]);
					
					if(j == PosX)
						PrintFormat( FB[i], 0, MENU_POS_Y+32+16*j, "-->");
				}

				PrintFormat( FB[i], MENU_POS_X+575, MENU_POS_Y+16*22, "%d/%d", ScrollX/EntryCount + 1, *GameCount/EntryCount + (*GameCount % EntryCount > 0));

				sync_after_write( (u32*)(FB[i]), FBSize );
			} break;
			
			case 1:
			{
				PrintFormat(FB[i], MENU_POS_X+15, 68, "NEEK2O Config:");			
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*0, "__fwrite Patch                            :%s", (DICfg->Config&CONFIG_PATCH_FWRITE) ? "On" : "Off");
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*1, "MotionPlus Video                          :%s", (DICfg->Config&CONFIG_PATCH_MPVIDEO) ? "On" : "Off");
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*2, "Video Mode Patch                          :%s", (DICfg->Config&CONFIG_PATCH_VIDEO) ? "On" : "Off");				
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*4, "Debugging:");				
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*5, "Autocreate log for DIP module             :%s", (DICfg->Config&DEBUG_CREATE_DIP_LOG) ? "On" : "Off");
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*6, "Autocreate log for ES module              :%s", (DICfg->Config&DEBUG_CREATE_ES_LOG) ? "On" : "Off");
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*7, "Game Debugging                            :%s", (DICfg->Config&CONFIG_DEBUG_GAME) ? "On" : "Off");
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*8, "Debugger Wait                             :%s", (DICfg->Config&CONFIG_DEBUG_GAME_WAIT) ? "On" : "Off");
				switch((DICfg->Config&HOOK_TYPE_MASK))
				{
					case HOOK_TYPE_VSYNC:
						PrintFormat( FB[i], MENU_POS_X+15, 84+16*9, "Hook Type : %s", "VIWaitForRetrace" );
					break;
					case HOOK_TYPE_OSLEEP:
						PrintFormat( FB[i], MENU_POS_X+15, 84+16*9, "Hook Type : %s", "OSSleepThread" );
					break;
					//case HOOK_TYPE_AXNEXT:
					//	PrintFormat( FB[i], MENU_POS_X+15, 84+16*9, "Hook type : %s", "__AXNextFrame" );
					//break;
					default:
						PrintFormat( FB[i], MENU_POS_X+15, 84+16*9, "Hook Type : Invalid Type: %d", (DICfg->Config&HOOK_TYPE_MASK)>>28 );
					break;
				}
			
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*11, "Disc Options:");
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*12, "Load Disc");
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*13, "Retry On Read Error                       :%s", (DICfg->Config&CONFIG_READ_ERROR_RETRY) ? "On" : "Off");
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*14, "Error Skipping Game Play                  :%s", (DICfg->Config&CONFIG_GAME_ERROR_SKIP) ? "On" : "Off");
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*15, "Error Skipping Dumper                     :%s", (DICfg->Config&CONFIG_DUMP_ERROR_SKIP) ? "On" : "Off");			
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*17, "Cache and Config Files:");				
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*18, "Recreate %s Cache", CacheStr[CacheSel]);
				PrintFormat(FB[i], MENU_POS_X+15, 84+16*19, "Reset Configuration");
				
				if(fnnd) 
					PrintFormat(FB[i], MENU_POS_X+15, 84+16*20, "Select Emunand: %.20s", NandCfg->NandInfo[NandCfg->NandSel]+NANDDESC_OFF);
				else
					PrintFormat(FB[i], MENU_POS_X+15, 84+16*20, "Select Emunand: Root Nand");
				
				
				PrintFormat( FB[i], MENU_POS_X-5, 84+16*PosX, "-->");
				sync_after_write((u32*)(FB[i]), FBSize);
			} break;

			case 2:
			{
				if( DVDStatus == 0 )
				{
					DVDEjectDisc();
					DVDStatus = 1;					
				}

				if( DIP_COVER & 1 )
				{
					PrintFormat( FB[i], MENU_POS_X+80, 104+16*0, "Please insert a disc" );

					DVDStatus = 1;
					DVDError  = 0;

				} else {

					if( DVDError )
					{
						switch(DVDError)
						{
							default:
								PrintFormat( FB[i], MENU_POS_X+80, 104+16*0, "DVDCommand failed with:%08X", DVDError );	
							break;
						}					
					} else {
						switch( DVDStatus )
						{
							case 1:
							{
								DVDLowReset();

								s32 r = DVDLowReadDiscID( (void*)0 );
								if( r != DI_SUCCESS )
								{
									//("DVDLowReadDiscID():%d\n", r );
									DVDError = DVDLowRequestError();
									//dbgprintf("DVDLowRequestError():%X\n", DVDError );
								} else {

									hexdump( (void*)0, 0x20);

									//Detect disc type
									if( *(u32*)0x18 == 0x5D1C9EA3 )
									{
										//try a read outside the normal single layer area
										r = DVDLowRead( (char*)0x01000000, 0x172A33100LL, 0x8000 );
										if( r != 0 )
										{
											r = DVDLowRequestError();
											if( r == 0x052100 )
												DVDType = 2;
											else 
												DVDError = r;
										} else {
											DVDType = 3;
										}										

									} else if( *(u32*)0x1C == 0xC2339F3D ) {
										DVDType = 1;
									}

									DVDStatus = 2;

									r = DVDLowRead( (char*)(0x01000000+READSIZE), 0x20, 0x40 );
									if( r == DI_SUCCESS )
										DVDTitle = (char*)(0x01000000+READSIZE);

									DVDTimer = *(vu32*)0x0d800010;
								}
							} break;
							case 2:
							{
								switch(DVDType)
								{
									case 1:
										PrintFormat( FB[i], MENU_POS_X, 104+16*0, "Press A to dump: %.24s(GC)", DVDTitle );
									break;
									case 2:
										PrintFormat( FB[i], MENU_POS_X, 104+16*0, "Press A to dump: %.20s(WII-SL)", DVDTitle );
									break;
									case 3:
										PrintFormat( FB[i], MENU_POS_X, 104+16*0, "Press A to dump: %.20s(WII-DL)", DVDTitle );
									break;
									default:
										PrintFormat( FB[i], MENU_POS_X, 104+16*0, "UNKNOWN disc type!");
									break;
								}
								
							} break;
							case 3:		// Setup ripping 
							{
								DiscName = (char*)malloca( 64, 32 );

								switch( DVDType )
								{
									case 1:	// GC		0x57058000,44555
									{
										_sprintf( DiscName, "/%.6s.gcm", (void*)0 );

										DVDSectorSize = 44555;
										DVDStatus = 4;
									} break;
									case 2:	// WII-SL	0x118240000, 143432
									{
										_sprintf( DiscName, "/%.6s_0.iso", (void*)0 );

										DVDSectorSize = 143432;
										DVDStatus = 4;
									} break;
									case 3:	// WII-DL	0x1FB4E0000, 259740 
									{
										_sprintf( DiscName, "/%.6s_0.iso", (void*)0 );

										DVDSectorSize = 259740;
										DVDStatus = 4;
									} break;
								}

								if( DVDStatus == 4 )
								{
									DVDHandle = DVDOpen( DiscName );
									if( DVDHandle < 0 )
									{
										//dbgprintf("ES:DVDOpen():%d\n", DVDHandle );
										DVDError = DI_FATAL|(DVDHandle<<16);
										break;
									}

									DVDErrorRetry = 0;
									DVDBuffer = (char*)0x01000000;
									memset32( DVDBuffer, 0, READSIZE );
									DVDTimer = *(vu32*)0x0d800010;
									DVDTimeStart = DVDTimer;
								}

							} break;
							case 4:
							{
								PrintFormat( FB[i], MENU_POS_X, 104+16*0, "Dumping:  %.24s", DVDTitle );
								if( DVDSpeed / 1024 / 1024 )
									PrintFormat( FB[i], MENU_POS_X, 104+16*2, "Speed:    %u.%uMB/s ", DVDSpeed / 1024 / 1024, (DVDSpeed / 1024) % 1024 );
								else
									PrintFormat( FB[i], MENU_POS_X, 104+16*2, "Speed:    %uKB/s ", DVDSpeed / 1024 );
								PrintFormat( FB[i], MENU_POS_X, 104+16*3, "Progress: %u%%", DVDOffset*100 / DVDSectorSize );
								PrintFormat( FB[i], MENU_POS_X, 104+16*4, "Time left:%02d:%02d:%02d", DVDTimeLeft/3600, (DVDTimeLeft/60)%60, DVDTimeLeft%60%60 );

								if( (DVDOffset%16) == 0 )
									dbgprintf("\rES:Dumping:%s %08X/%08X", DiscName, DVDOffset, DVDSectorSize );

								if( i == 0 )
								{
									double Now = *(vu32*)0x0d800010;
									if( (u32)((Now-DVDTimer) * 128.0f / 243000000.0f) )	//Update values once per second
									{	
										DVDSpeed	= ( DVDOffset - DVDOldOffset ) * READSIZE;
										DVDTimeLeft = ( DVDSectorSize - DVDOffset ) / ( DVDSpeed / READSIZE );

										DVDOldOffset=  DVDOffset;
										DVDTimer	= *(vu32*)0x0d800010;
									}

									s32 ret = DVDLowRead( DVDBuffer, (u64)DVDOffset * READSIZE, READSIZE );
									if( ret != 0 )
									{
										DVDError = DVDLowRequestError();
										if( DVDError == 0x0030200 )
										{
											if( DVDErrorSkip == 1 )
											{
												memset32( DVDBuffer, 0, READSIZE );
											} else {
												if( DICfg->Config & CONFIG_DUMP_ERROR_SKIP )
												{
													//dbgprintf("\nES:Enabled error skipping\n");
													DVDErrorSkip = 1;
												} else {
													//dbgprintf("\nES:DVDLowRead():%d\n", ret );
													//dbgprintf("ES:DVDError:%X\n", DVDError );
													break;
												}
											}

										} else if ( DVDError == 0x0030201 && DVDErrorRetry < 5 ) {

											//dbgprintf("\nES:DVDLowRead failed:%x Retry:%d\n", DVDError, DVDErrorRetry );
											DVDError = 0;
											DVDErrorRetry++;
											DVDOffset--;
											continue;

										} else {

											//dbgprintf("\nES:DVDLowRead():%d\n", ret );
											//dbgprintf("ES:DVDError:%X\n", DVDError );
											break;
										}
									}

									ret = DVDWrite( DVDHandle, DVDBuffer, READSIZE );
									if( ret != READSIZE )
									{
										//dbgprintf("\nES:DVDWrite():%d\n", ret );
										DVDError = DI_FATAL|(ret<<16);
										break;
									}

									DVDOffset++;
									if( DVDOffset == 131071 )
									{
										DVDClose( DVDHandle );

										_sprintf( DiscName, "/%.6s_1.iso", (void*)0 );
										DVDHandle = DVDOpen( DiscName );
										if( DVDHandle < 0 )
										{
											//dbgprintf("ES:DVDOpen():%d\n", DVDHandle );
											DVDError = DI_FATAL|(DVDHandle<<16);
											break;
										}

									}
									if( DVDOffset >= DVDSectorSize )
									{
										DVDClose( DVDHandle );
										free( DiscName );
										DVDStatus = 5;
										break;
									}
								}

							} break;
							case 5:
							{
								PrintFormat( FB[i], MENU_POS_X, 104+16*0, "Dumped:   %.25s", DVDTitle );
							} break;
						}
					}
				}

			} break;
			case 4:
			{
				for( j=0; j<EntryCount; ++j )
				{
					if( j+ScrollX >= channelCache->numChannels )
						break;

					PrintFormat( FB[i], MENU_POS_X, MENU_POS_Y+32+16*j, "%.40s", channelCache->channels[ScrollX+j].name);

					if( j == PosX )
						PrintFormat( FB[i], 0, MENU_POS_Y+32+16*j, "-->");
				}

				PrintFormat( FB[i], MENU_POS_X+575, MENU_POS_Y+16*21, "%d/%d", ScrollX/EntryCount + 1, channelCache->numChannels/EntryCount + (channelCache->numChannels % EntryCount > 0));

				sync_after_write( (u32*)(FB[i]), FBSize );
			} break;			
			case 5:
			{				
				PrintFormat( FB[i], MENU_POS_X+15, 68, "Menu Hacks:" );			
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*0, "Auto Press A at Health Screen             :%s", ( PL->Config&CONFIG_PRESS_A ) ? "On" : "Off" );				
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*1, "No System Menu Background Music           :%s", ( PL->Config&CONFIG_NO_BG_MUSIC ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*2, "No System Menu Sounds At All              :%s", ( PL->Config&CONFIG_NO_SOUND ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*3, "Move Disc Channel                         :%s", ( PL->Config&CONFIG_MOVE_DISC_CHANNEL ) ? "On" : "Off" );				
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*4, "Skip Disc Update Check                    :%s", ( PL->Config&CONFIG_BLOCK_DISC_UPDATE ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*5, "Console Country Code: %d", PL->Shop1 );
				
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*7, "Region Free Hacks:" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*8, "System Region Free Hack                   :%s", ( PL->Config&CONFIG_REGION_FREE ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*9, "Temp Region Change                        :%s", ( PL->Config&CONFIG_REGION_CHANGE ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*10, "EUR Default Language : %s", LanguageStr[PL->EULang] );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*11, "USA Default Language : %s", LanguageStr[PL->USLang] );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*12, "Force PAL Video Mode : %s", VidStr[PL->PALVid] );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*13, "Force NTSC Video Mode: %s", VidStr[PL->NTSCVid] );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*15, "Auto Boot Options:" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*16, "Autoboot: %s", BootStr[PL->Autoboot] );
				if( PL->Autoboot == 1 )					
					PrintFormat( FB[i], MENU_POS_X+15, 84+16*17, "AB Channel: %.20s", channelCache->channels[PL->ChNbr].name );				
				else if( PL->Autoboot == 2 )
					PrintFormat( FB[i], MENU_POS_X+15, 84+16*17, "AB DOL: %.20s", (char *)PL->DOLName );
					
				
				
				if( rebreq )
					PrintFormat( FB[i], MENU_POS_X+15, 84+16*19, "Save Hacks (*Reboot Required)" );
				else
					PrintFormat( FB[i], MENU_POS_X+15, 84+16*19, "Save Hacks" );
				
				PrintFormat( FB[i], MENU_POS_X-5, 84+16*PosX, "-->");
				sync_after_write( (u32*)(FB[i]), FBSize );			
			} break;
			case 6:
			{
				PrintFormat( FB[i], MENU_POS_X+80, 56, "DML/QuadForce Setup:" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*0, "NMM             :%s", (DICfg->Config&DML_NMM) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*1, "NMM Debug       :%s", (DICfg->Config&DML_NMM_DEBUG) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*2, "Padhook         :%s", (DICfg->Config&DML_PADHOOK) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*3, "Activity LED    :%s", (DICfg->Config&DML_ACTIVITY_LED) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*4, "Debugger        :%s", (DICfg->Config&DML_DEBUGGER) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*5, "Debugger Wait   :%s", (DICfg->Config&DML_DEBUGWAIT) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*6, "Use Cheats      :%s", (DICfg->Config&DML_CHEATS) ? "On" : "Off" );
				
				switch((DICfg->Config&DML_VIDEO_CONF))
				{
					case DML_VIDEO_GAME:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*7, "Video Mode: Game");
					break;
					case DML_VIDEO_PAL50:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*7, "Video Mode: PAL 576i");
					break;
					case DML_VIDEO_NTSC:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*7, "Video Mode: NTSC 480i");
					break;
					case DML_VIDEO_PAL60:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*7, "Video Mode: PAL 480i");
					break;
					case DML_VIDEO_PROG:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*7, "Video Mode: NTSC 480p");
					break;
					case DML_VIDEO_PROGP:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*7, "Video Mode: PAL 480p");
					break;					
				}

				switch((DICfg->Config&DML_LANG_CONF))
				{
					case DML_LANG_ENGLISH:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*8, "Language  : English");
					break;
					case DML_LANG_GERMAN:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*8, "Language  : German");
					break;
					case DML_LANG_FRENCH:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*8, "Language  : French");
					break;
					case DML_LANG_SPANISH:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*8, "Language  : Spanish");
					break;
					case DML_LANG_ITALIAN:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*8, "Language  : Italian");
					break;
					case DML_LANG_DUTCH:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*8, "Language  : Dutch");
					break;					
				}				
				
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*10, "Save Config" );			

				PrintFormat( FB[i], MENU_POS_X+60, 40+64+16*PosX, "-->");
				sync_after_write( (u32*)(FB[i]), FBSize );
			} break;
			default:
			{
				ShowMenu = 0;
			} break;
		}
	}
}

void LoadDIConfig(void)
{
	if(!LoadDI)
	{
		DICfg = (DIConfig *)malloca( 0x110, 32 );
		DICfg->Gamecount = 0;
		DICfg->SlotID = 0;
		DICfg->Region = 0;
		DICfg->Config = 0;
	}
	else
	{
		DVDGetGameCount(GameCount);
		if((*GameCount & 0xF0000) == 0x10000)
		{
			DIConfType = 1;
			*GameCount &= ~0x10000;
			DICfg = (DIConfig *)malloca( *GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE, 32);
			DVDReadGameInfo(0, *GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE, DICfg);
		}
		else
		{
			DIConfType = 0;
			DICfgO = (DIConfigO *)malloca(*GameCount * OLD_GAMEINFO_SIZE + OLD_CONFIG_SIZE, 32);
			DVDReadGameInfo(0, *GameCount * OLD_GAMEINFO_SIZE + OLD_CONFIG_SIZE, DICfgO);
			DICfg = (DIConfig *)malloca(*GameCount * DVD_GAMEINFO_SIZE + DVD_CONFIG_SIZE, 32);
			DICfg->SlotID = DICfgO->SlotID;
			DICfg->Region = DICfgO->Region;
			DICfg->Gamecount = DICfgO->Gamecount;
			DICfg->Config = DICfgO->Config;
			u32 i;
			for(i = 0; i < (*GameCount); i++)
			{
				memcpy(DICfg->GameInfo[i], DICfgO->GameInfo[i], OLD_GAMEINFO_SIZE);
			}	
			free(DICfgO);
		}
	}
}

void SMenuReadPad(void)
{	
	memcpy(&GCPad, (u32*)0xD806404, sizeof(u32) * 2);
	
	if((GCPad.Buttons & 0x1F3F0000) == 0 && (*WPad & 0x0000FFFF) == 0)
	{
		SLock = 0;
		return;
	}

	if(SLock)
		return;	

	if((GCPad.Start || (*WPad&WPAD_BUTTON_1)) && !SLock)
	{
		ShowMenu = !ShowMenu;
			
		if(DICfg == NULL)
			LoadDIConfig();

		MenuType = LoadDI ? 0 : 4;
		PosX	= 0;
		ScrollX	= 0;
		SLock = 1;
		
	}
	
	if((GCPad.Z || (*WPad&WPAD_BUTTON_2)) && !SLock)
	{
		if(ShowMenu && (!MenuType || MenuType == 4))
		{
			MenuType = MenuType ? 0 : 4;				
			PosX	= 0;
			ScrollX	= 0;
			SLock	= 1;			
		}
		else
		{				
			ShowMenu = !ShowMenu;
			
			if(DICfg == NULL)
				LoadDIConfig();

			if(ShowMenu)
			{
				if(NandCfg == NULL)
				{
					char *path = malloca(0x40, 0x40);
					strcpy(path, "/sneek/NandCfg.bin");
					u32* fsize = malloca(sizeof(u32), 0x20);
					*fsize = 0x10;
					NandCfg = (NandConfig*)NANDLoadFile(path, fsize);
					if(NandCfg != NULL)
					{
						*NandCount = NandCfg->NandCnt;
						heap_free( 0, NandCfg );
						*fsize = *NandCount * 0x80 + 0x10;
						NandCfg = (NandConfig*)NANDLoadFile(path, fsize);
						if (NandCfg != NULL)
							fnnd = 1;	
					}
					free(fsize);
					free(path);
				}
			}
			MenuType = 1;
			SLock = 1;
		}
	}

	if(!ShowMenu)
		return;

	if((GCPad.B || (*WPad&WPAD_BUTTON_B) ) && !SLock)
	{			
		if(MenuType == 0 || MenuType == 1)
		{
			ShowMenu = 0;
		}
		else if(MenuType == 4)
		{
			if(LoadDI)
				MenuType = 0;
			else 
				ShowMenu = 0;
		}
		else
		{
			MenuType = 1;
		}
			
		PosX	= 0;
		ScrollX	= 0;
		SLock	= 1;
	}

	if((GCPad.X || (*WPad&WPAD_BUTTON_PLUS)) && !SLock)
	{		
		if(MenuType == 1)
		{
			MenuType = 5;
			PosX = 0;			
			ScrollX	= 0;
			SLock	= 1;
		}
		else if(MenuType == 5)
		{
			MenuType = 6;
			PosX = 0;			
			ScrollX	= 0;
			SLock	= 1;
		}
		else if(MenuType == 6)
		{
			MenuType = 1;
			if (LoadDI == true)
				PosX = 0;
			else
				PosX = 13;
				
			ScrollX	= 0;
			SLock	= 1;
		}		
	}

	if((GCPad.Y || (*WPad&WPAD_BUTTON_MINUS) ) && !SLock)
	{
		if(MenuType == 1)
		{
			MenuType = 6;
			PosX = 0;			
			ScrollX	= 0;
			SLock	= 1;
		}
		else if(MenuType == 5)
		{
			MenuType = 1;
			if(LoadDI == true)
				PosX = 0;
			else
				PosX = 13;
				
			ScrollX	= 0;
			SLock	= 1;
		}
		else if(MenuType == 6)
		{
			MenuType = 5;
			PosX = 0;			
			ScrollX	= 0;
			SLock	= 1;			
		}	
	}

	switch(MenuType)
	{		
		case 0:			// Game list
		{
			if(GCPad.Z || (*WPad&WPAD_BUTTON_2))
			{
				//DVDSelectGame( PosX+ScrollX, 1 );
				//ShowMenu = 0;
				//GameSelected = PosX+ScrollX;
				//MenuType = 7;
				//PosX = 7;
				//SLock = 1;
			}
			if(GCPad.A || (*WPad&WPAD_BUTTON_A))
			{
				DVDSelectGame(PosX+ScrollX, 0);
				ShowMenu = 0;
				SLock = 1;
			}
			if(GCPad.Up || (*WPad&WPAD_BUTTON_UP))
			{
				if(PosX)
					PosX--;
				else if(ScrollX)
					ScrollX--;

				SLock = 1;
			} 
			else if(GCPad.Down || (*WPad&WPAD_BUTTON_DOWN))
			{
				if(PosX >= EntryCount-1)
				{
					if(PosX+ScrollX+1 < *GameCount)
						ScrollX++;
				} 
				else if(PosX+ScrollX+1 < *GameCount)
					PosX++;

				SLock = 1;
			} 
			else if(GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT))
			{
				if(ScrollX/EntryCount*EntryCount + EntryCount < DICfg->Gamecount)
				{
					PosX	= 0;
					ScrollX = ScrollX/EntryCount*EntryCount + EntryCount;
				} 
				else 
				{
					PosX	= 0;
					ScrollX	= 0;
				}

				SLock = 1; 
			} 
			else if(GCPad.Left || (*WPad&WPAD_BUTTON_LEFT))
			{
				if(ScrollX/EntryCount*EntryCount - EntryCount > 0)
				{
					PosX	= 0;
					ScrollX-= EntryCount;
				} 
				else 
				{
					PosX	= 0;
					ScrollX	= 0;
				}
				SLock = 1; 
			}
			else if(GCPad.X || (*WPad&WPAD_BUTTON_PLUS))
			{
				if(IgnoreCase((u8)DICfg->GameInfo[PosX+ScrollX][0x20]) >= IgnoreCase((u8)DICfg->GameInfo[*GameCount-1][0x20]))
				{
					PosX	= 0;
					ScrollX	= 0;
				}
				else
				{
					u32 snum;
					u32 lnum = PosX + ScrollX;
				
					for(snum = PosX+ScrollX; snum <= *GameCount; snum++)
					{				
						if(IgnoreCase((u8)DICfg->GameInfo[snum][0x20]) > IgnoreCase((u8)DICfg->GameInfo[lnum][0x20]))
						{
							ScrollX += PosX;
							PosX = 0;
							break;						
						}
						
						if(PosX >= EntryCount-1)
						{
							if(PosX+ScrollX <= *GameCount)							
								ScrollX++;
						}
						else if(PosX+ScrollX <= *GameCount)
						{
							PosX++;	
						}
					}
				}				
				SLock = 1;
			}
			else if(GCPad.Y || (*WPad&WPAD_BUTTON_MINUS))
			{
				u32 snum;
				u32 tnum = 0;
				if(IgnoreCase((u8)DICfg->GameInfo[PosX+ScrollX][0x20]) <= IgnoreCase((u8)DICfg->GameInfo[0][0x20]))
				{
					PosX = 0;
					ScrollX	= *GameCount-1;
					tnum = ScrollX;
					for(snum = tnum; snum >= 0; snum--)
					{
						if(IgnoreCase((u8)DICfg->GameInfo[snum][0x20]) < IgnoreCase((u8)DICfg->GameInfo[tnum][0x20]))
						{
							ScrollX++;
							break;
						}
						ScrollX--;
					}
				}
				else
				{					
					u32 lnum = PosX + ScrollX;
					ScrollX = lnum;
					PosX = 0;
				
					for(snum = PosX+ScrollX; snum >= 0; snum--)
					{
						if(IgnoreCase((u8)DICfg->GameInfo[snum][0x20]) < IgnoreCase((u8)DICfg->GameInfo[tnum][0x20]))
						{
							ScrollX++;
							break;
						}
					
						if(IgnoreCase((u8)DICfg->GameInfo[snum][0x20]) < IgnoreCase((u8)DICfg->GameInfo[lnum][0x20]))
							tnum = snum;
				
						ScrollX--;
					}
				}
				SLock = 1;
			}
		} break;
		case 1:		//SNEEK Settings
		{
			if( GCPad.A || (*WPad&WPAD_BUTTON_A) )
			{
				switch(PosX)
				{
					case 0:
					{
						DICfg->Config ^= CONFIG_PATCH_FWRITE;
						DVDWriteDIConfig(DICfg);						
						DVDReinsertDisc=true;
					} break;
					case 1:
					{
						DICfg->Config ^= CONFIG_PATCH_MPVIDEO;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 2:
					{
						DICfg->Config ^= CONFIG_PATCH_VIDEO;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;				
					case 5:
					{
						DICfg->Config ^= DEBUG_CREATE_DIP_LOG;
						DVDWriteDIConfig(DICfg);
					} break;
					case 6:
					{
						DICfg->Config ^= DEBUG_CREATE_ES_LOG;
						DVDWriteDIConfig(DICfg);
					} break;
					case 7:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 8:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME_WAIT;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 9:
					{
						if((DICfg->Config & HOOK_TYPE_MASK) == HOOK_TYPE_OSLEEP)
						{
							DICfg->Config &= ~HOOK_TYPE_MASK;
							DICfg->Config |= HOOK_TYPE_VSYNC;
						} 
						else 
						{
							DICfg->Config += HOOK_TYPE_VSYNC;								
						}
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 12:
					{
						DVDLowReset();
						DVDLowReadDiscID((void*)0);
						DVDMountDisc();							
						ShowMenu = 0;
						SLock = 1;
					} break;
					case 13:
					{
						DICfg->Config ^= CONFIG_READ_ERROR_RETRY;
						DVDWriteDIConfig(DICfg);
					} break;
					case 14:
					{
						DICfg->Config ^= CONFIG_GAME_ERROR_SKIP;
						DVDWriteDIConfig(DICfg);
					} break;
					case 15:
					{
						DICfg->Config ^= CONFIG_DUMP_ERROR_SKIP;
						DVDWriteDIConfig(DICfg);
					} break;
					case 18:
					{
						switch(CacheSel)
						{
							case 0:
							{
								DICfg->Gamecount = 0;
								DVDWriteDIConfig(DICfg);
							} break;
							case 1:
							{
								ISFS_Delete("/sneekcache/channelcache.bin");
							} break;
							case 2:
							{
								ISFS_Delete("/sneek/nandcfg.bin");
								
							} break;
						}
						LaunchTitle(0x0000000100000002LL);
					} break;
					case 19:
					{
						PL->EULang   = 1;
						PL->USLang   = 1;
						PL->PALVid	 = 1;
						PL->NTSCVid	 = 5;
						PL->Shop1    = 0;
						PL->Config   = 0;
						PL->Autoboot = 0;
						PL->ChNbr    = 0;
						PL->DolNr    = 0;
						PL->TitleID  = 0x0000000100000002LL;
						NANDWriteFileSafe("/sneekcache/hackscfg.bin", PL , sizeof(HacksConfig));
						DICfg->Config = DML_VIDEO_GAME | DML_LANG_ENGLISH | HOOK_TYPE_OSLEEP;
						DVDWriteDIConfig( DICfg );
						LaunchTitle(0x0000000100000002LL);
					} break;
						
					case 20:
					{
						NANDWriteFileSafe( "/sneek/nandcfg.bin", NandCfg , NandCfg->NandCnt * NANDINFO_SIZE + NANDCFG_SIZE );
						LaunchTitle( 0x0000000100000002LL );
					} break;					
				}
				SLock = 1;
			}
			if( GCPad.Up || (*WPad&WPAD_BUTTON_UP) )
			{
				if(PosX == 0)
					PosX = 20;
				else if(PosX == 5)
					PosX = 2;
				else if(PosX == 12)
					PosX = 9;
				else if(PosX == 18)
					PosX = 15;
				else
					PosX--;

				SLock = 1;
			} 
			else if( GCPad.Down || (*WPad&WPAD_BUTTON_DOWN) )
			{
				PosX++;
				if(PosX == 3)
					PosX = 5;
				else if(PosX == 10)
					PosX = 12;
				else if(PosX == 16)
					PosX = 18;
				else if(PosX >= 21)
					PosX = 0;
					
				SLock = 1;
			}

			if( GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT) )
			{
				switch( PosX )
				{
					case 0:
					{
						DICfg->Config ^= CONFIG_PATCH_FWRITE;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 1:
					{
						DICfg->Config ^= CONFIG_PATCH_MPVIDEO;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 2:
					{
						DICfg->Config ^= CONFIG_PATCH_VIDEO;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 5:
					{
						DICfg->Config ^= DEBUG_CREATE_DIP_LOG;
						DVDWriteDIConfig(DICfg);
					} break;
					case 6:
					{
						DICfg->Config ^= DEBUG_CREATE_ES_LOG;
						DVDWriteDIConfig(DICfg);
					} break;
					case 7:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 8:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME_WAIT;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 9:
					{
						if( (DICfg->Config & HOOK_TYPE_MASK) == HOOK_TYPE_OSLEEP )
						{
							DICfg->Config &= ~HOOK_TYPE_MASK;
							DICfg->Config |= HOOK_TYPE_VSYNC;
						} else {
							DICfg->Config += HOOK_TYPE_VSYNC;								
						}
						
						DVDReinsertDisc=true;
					} break;
					case 13:
					{
						DICfg->Config ^= CONFIG_READ_ERROR_RETRY;
						DVDWriteDIConfig(DICfg);
					} break;
					case 14:
					{
						DICfg->Config ^= CONFIG_GAME_ERROR_SKIP;
						DVDWriteDIConfig(DICfg);
					} break;
					case 15:
					{
						DICfg->Config ^= CONFIG_DUMP_ERROR_SKIP;
						DVDWriteDIConfig(DICfg);
					} break;
					case 18: 
					{
						CacheSel++;
						if(CacheSel >= 3)
							CacheSel = 0;
					} break;
					case 20:
					{
						if(fnnd)
						{
							if( NandCfg->NandSel == NandCfg->NandCnt-1 )
								NandCfg->NandSel = 0;
							else
								NandCfg->NandSel++;
						}
					} break;
				}
				SLock = 1;
			} else if( GCPad.Left || (*WPad&WPAD_BUTTON_LEFT) )
			{
				switch( PosX )
				{
					case 0:
					{
						DICfg->Config ^= CONFIG_PATCH_FWRITE;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 1:
					{
						DICfg->Config ^= CONFIG_PATCH_MPVIDEO;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 2:
					{
						DICfg->Config ^= CONFIG_PATCH_VIDEO;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 5:
					{
						DICfg->Config ^= DEBUG_CREATE_DIP_LOG;
						DVDWriteDIConfig(DICfg);
					} break;
					case 6:
					{
						DICfg->Config ^= DEBUG_CREATE_ES_LOG;
						DVDWriteDIConfig(DICfg);
					} break;
					case 7:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 8:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME_WAIT;
						DVDWriteDIConfig(DICfg);
						DVDReinsertDisc=true;
					} break;
					case 9:
					{
						if( (DICfg->Config & HOOK_TYPE_MASK) == HOOK_TYPE_OSLEEP )
						{
							DICfg->Config &= ~HOOK_TYPE_MASK;
							DICfg->Config |= HOOK_TYPE_VSYNC;
						} else {
							DICfg->Config += HOOK_TYPE_VSYNC;								
						}
						
						DVDReinsertDisc=true;
					} break;
					case 13:
					{
						DICfg->Config ^= CONFIG_READ_ERROR_RETRY;
						DVDWriteDIConfig(DICfg);
					} break;
					case 14:
					{
						DICfg->Config ^= CONFIG_GAME_ERROR_SKIP;
						DVDWriteDIConfig(DICfg);
					} break;
					case 15:
					{
						DICfg->Config ^= CONFIG_DUMP_ERROR_SKIP;
						DVDWriteDIConfig(DICfg);
					} break;
					case 18: 
					{						
						if(CacheSel == 0)
							CacheSel = 2;
						else
							CacheSel--;
					} break;
					case 20:
					{
						if( fnnd )
						{
							if( NandCfg->NandSel == 0 )
								NandCfg->NandSel = NandCfg->NandCnt-1;
							else
								NandCfg->NandSel--;
						}
					} break;					
				}
				SLock = 1;
			} 
		} break;
		case 2:
		{
			if(GCPad.A || (*WPad&WPAD_BUTTON_A))
			{
				if(DVDStatus == 2 && DVDType > 0)
				{
					DVDStatus = 3;
				}
				SLock = 1;
			}
		} break;
		case 4: //channel list
		{
			if(GCPad.A || (*WPad&WPAD_BUTTON_A))
			{
				ShowMenu = 0;
				LaunchTitle(channelCache->channels[PosX + ScrollX].titleID);
				SLock = 1;
				break;
			}
			if(GCPad.Up || (*WPad&WPAD_BUTTON_UP))
			{
				if(PosX)
					PosX--;
				else if(ScrollX)
					ScrollX--;

				SLock = 1;
			} 
			else if(GCPad.Down || (*WPad&WPAD_BUTTON_DOWN))
			{
				if(PosX >= EntryCount-1)
				{
					if(PosX+ScrollX+1 < channelCache->numChannels)
						ScrollX++;
				}
				else if(PosX+ScrollX+1 < channelCache->numChannels)
					PosX++;

				SLock = 1;
			} 
			else if(GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT))
			{
				if(ScrollX/EntryCount*EntryCount + EntryCount < channelCache->numChannels)
				{
					PosX	= 0;
					ScrollX = ScrollX/EntryCount*EntryCount + EntryCount;
				} 
				else 
				{
					PosX	= 0;
					ScrollX	= 0;
				}

				SLock = 1; 
			} 
			else if(GCPad.Left || (*WPad&WPAD_BUTTON_LEFT))
			{
				if(ScrollX/EntryCount*EntryCount - EntryCount > 0)
				{
					PosX	= 0;
					ScrollX-= EntryCount;
				} 
				else 
				{
					PosX	= 0;
					ScrollX	= 0;
				}

				SLock = 1; 
			}
		} break;
		case 5:		//Menu and region hacks
		{		
			if(GCPad.A || (*WPad&WPAD_BUTTON_A))
			{
				switch(PosX)
				{
					case 0:
					{
						PL->Config ^= CONFIG_PRESS_A;
					} break;
					case 1:
					{
						PL->Config ^= CONFIG_NO_BG_MUSIC;
						rebreq = 1;
					} break;
					case 2:
					{
						PL->Config ^= CONFIG_NO_SOUND;
						rebreq = 1;
					} break;
					case 3:
					{
						PL->Config ^= CONFIG_MOVE_DISC_CHANNEL;
						rebreq = 1;
					} break;
					case 4:
					{
						PL->Config ^= CONFIG_BLOCK_DISC_UPDATE;
						rebreq = 1;
					} break;
					case 5:
					{
						if(PL->Shop1 == 1)
							PL->Shop1 = 8;
						else if(PL->Shop1 == 52)
							PL->Shop1 = 64;
						else if(PL->Shop1 == 121)
							PL->Shop1 = 136;
						else if(PL->Shop1 == 136)
							PL->Shop1 = 148;
						else if(PL->Shop1 == 148)
							PL->Shop1 = 150;
						else if(PL->Shop1 == 157)
							PL->Shop1 = 168;
						else if(PL->Shop1 == 177)
							PL->Shop1 = 1;
						else
							PL->Shop1++;
							
					} break;
					case 8:
					{
						PL->Config ^= CONFIG_REGION_FREE;
						rebreq = 1;
					} break;
					case 9:
					{
						PL->Config ^= CONFIG_REGION_CHANGE;
					} break;
					case 10:
					{
						if(PL->EULang == 6)
							PL->EULang = 1;
						else
							PL->EULang++;
					} break;
					case 11:
					{
						if(PL->USLang == 1)
							PL->USLang = 3;
						else if(PL->USLang == 3)
							PL->USLang = 4;
						else if(PL->USLang == 4)
							PL->USLang = 1;
					} break;
					case 12:
					{
						if(PL->PALVid == 3)
							PL->PALVid = 0;
						else
							PL->PALVid++;
							
						rebreq = 1;
					} break;
					case 13:
					{
						if(PL->NTSCVid == 7)
							PL->NTSCVid = 4;
						else
							PL->NTSCVid++;
							
						rebreq = 1;
					} break;					
					case 16:
					{
						if(PL->Autoboot == 1)
							PL->Autoboot = 0;
						else
							PL->Autoboot++;
					} break;
					case 17:
					{
						if(PL->Autoboot == 1)
						{
							if(PL->ChNbr == channelCache->numChannels-1)
								PL->ChNbr = 0;
							else
								PL->ChNbr++;
							
							PL->TitleID = channelCache->channels[PL->ChNbr].titleID;
						}
						if(PL->Autoboot == 2)
						{
							//if( PL->DolNr == *Cnt )
							//	PL->DolNr = 0;
							//else
							//	PL->DolNr++;
							

						}
						
					} break;
					case 19:
					{
						NANDWriteFileSafe("/sneekcache/hackscfg.bin", PL , sizeof(HacksConfig));
						if(rebreq)
							LaunchTitle(0x0000000100000002LL);						
					} break;
				}
				SLock = 1;
			}
			if(GCPad.Up || (*WPad&WPAD_BUTTON_UP))
			{
				if(PosX)
				{		
					PosX--;
				}					
				
				if(PosX == 18)
				{
					if(PL->Autoboot == 1 || PL->Autoboot == 2)
						PosX = 17;
					else
						PosX = 16;
				}
				else if(PosX == 15)
				{
					PosX = 13;
				}
				else if(PosX == 7)
				{
					PosX = 5;
				}

				SLock = 1;
			} 
			else if(GCPad.Down || (*WPad&WPAD_BUTTON_DOWN))
			{
				PosX++;

				if(PosX == 6)
					PosX = 8;
				else if(PosX == 14)
					PosX = 16;
				else if(PosX == 17 && PL->Autoboot == 0)
					PosX = 19;
				else if(PosX == 18)
					PosX = 19;	
				else if(PosX == 20)
					PosX = 0;
				SLock = 1;
			}
			if(GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT))
			{
				switch(PosX)
				{
					case 0:
					{
						PL->Config ^= CONFIG_PRESS_A;
					} break;
					case 1:
					{
						PL->Config ^= CONFIG_NO_BG_MUSIC;
						rebreq = 1;
					} break;
					case 2:
					{
						PL->Config ^= CONFIG_NO_SOUND;
						rebreq = 1;
					} break;
					case 3:
					{
						PL->Config ^= CONFIG_MOVE_DISC_CHANNEL;
						rebreq = 1;
					} break;
					case 4:
					{
						PL->Config ^= CONFIG_BLOCK_DISC_UPDATE;
						rebreq = 1;
					} break;
					case 5:
					{
						if(PL->Shop1 == 1)
							PL->Shop1 = 8;
						else if(PL->Shop1 == 52)
							PL->Shop1 = 64;
						else if(PL->Shop1 == 121)
							PL->Shop1 = 136;
						else if(PL->Shop1 == 136)
							PL->Shop1 = 148;
						else if(PL->Shop1 == 148)
							PL->Shop1 = 150;
						else if(PL->Shop1 == 157)
							PL->Shop1 = 168;
						else if(PL->Shop1 == 177)
							PL->Shop1 = 1;
						else
							PL->Shop1++;
							
					} break;
					case 8:
					{
						PL->Config ^= CONFIG_REGION_FREE;
						rebreq = 1;
					} break;
					case 9:
					{
						PL->Config ^= CONFIG_REGION_CHANGE;
					} break;
					case 10:
					{
						if(PL->EULang == 6)
							PL->EULang = 1;
						else
							PL->EULang++;
					} break;
					case 11:
					{
						if(PL->USLang == 1)
							PL->USLang = 3;
						else if(PL->USLang == 3)
							PL->USLang = 4;
						else if(PL->USLang == 4)
							PL->USLang = 1;
					} break;
					case 12:
					{
						if(PL->PALVid == 3)
							PL->PALVid = 0;
						else
							PL->PALVid++;
							
						rebreq = 1;
					} break;
					case 13:
					{
						if(PL->NTSCVid == 7)
							PL->NTSCVid = 4;
						else
							PL->NTSCVid++;
							
						rebreq = 1;
					} break;
					case 16:
					{
						if(PL->Autoboot == 1)
							PL->Autoboot = 0;
						else
							PL->Autoboot++;
					} break;
					case 17:
					{
						if(PL->Autoboot == 1)
						{
							if(PL->ChNbr == channelCache->numChannels-1)
								PL->ChNbr = 0;
							else
								PL->ChNbr++;
							
							PL->TitleID = channelCache->channels[PL->ChNbr].titleID;
						}
						else if(PL->Autoboot == 2)
						{
							//if( PL->DolNr == *Cnt )
							//	PL->DolNr = 0;
							//else
							//	PL->DolNr++;

						}
						
					} break;
				}
				SLock = 1;
			} 
			else if(GCPad.Left || (*WPad&WPAD_BUTTON_LEFT))
			{
				switch(PosX)
				{
					case 0:
					{
						PL->Config ^= CONFIG_PRESS_A;
					} break;
					case 1:
					{
						PL->Config ^= CONFIG_NO_BG_MUSIC;
						rebreq = 1;
					} break;
					case 2:
					{
						PL->Config ^= CONFIG_NO_SOUND;
						rebreq = 1;
					} break;
					case 3:
					{
						PL->Config ^= CONFIG_MOVE_DISC_CHANNEL;
						rebreq = 1;
					} break;
					case 4:
					{
						PL->Config ^= CONFIG_BLOCK_DISC_UPDATE;
						rebreq = 1;
					} break;
					case 5:
					{
						if(PL->Shop1 == 8)
							PL->Shop1 = 1;
						else if(PL->Shop1 == 64)
							PL->Shop1 = 52;
						else if(PL->Shop1 == 136)
							PL->Shop1 = 121;
						else if(PL->Shop1 == 14)
							PL->Shop1 = 136;
						else if(PL->Shop1 == 150)
							PL->Shop1 = 148;
						else if(PL->Shop1 == 168)
							PL->Shop1 = 157;
						else if(PL->Shop1 == 1)
							PL->Shop1 = 177;
						else
							PL->Shop1--;
							
					} break;
					case 8:
					{
						PL->Config ^= CONFIG_REGION_FREE;
						rebreq = 1;
					} break;
					case 9:
					{
						PL->Config ^=CONFIG_REGION_CHANGE;
					} break;
					case 10:
					{
						if(PL->EULang == 1)
							PL->EULang = 6;
						else
							PL->EULang--;
					} break;
					case 11:
					{
						if(PL->USLang == 1)
							PL->USLang = 4;
						else if(PL->USLang == 3)
							PL->USLang = 1;
						else if(PL->USLang == 4)
							PL->USLang = 3;
					} break;
					case 12:
					{
						if(PL->PALVid == 0)
							PL->PALVid = 3;
						else
							PL->PALVid--;
							
						rebreq = 1;
					} break;
					case 13:
					{
						if(PL->NTSCVid == 4)
							PL->NTSCVid = 7;
						else
							PL->NTSCVid--;
							
						rebreq = 1;
					} break;
					case 16:
					{
						if(PL->Autoboot == 0)
							PL->Autoboot = 1;
						else
							PL->Autoboot--;
					} break;
					case 17:
					{
						if(PL->Autoboot == 1)
						{
							if(PL->ChNbr == 0)
							PL->ChNbr = channelCache->numChannels-1;
							else
								PL->ChNbr--;
							
							PL->TitleID = channelCache->channels[PL->ChNbr].titleID;
						}
						if(PL->Autoboot == 2)
						{
							//if( PL->DolNr == 0 )
							//	PL->DolNr = *Cnt;
							//else
							//	PL->DolNr--;

						}
						
					} break;
				}
				SLock = 1;
			}
		} break;
		case 6:     // DML Settings
		{		
			if(GCPad.A || (*WPad&WPAD_BUTTON_A))
			{
				switch(PosX)
				{					
					case 0:
					{
						DICfg->Config ^= DML_NMM;
						DVDReinsertDisc = true;
					} break;
					case 1:
					{
						DICfg->Config ^= DML_NMM_DEBUG;
						DVDReinsertDisc = true;
					} break;
					case 2:
					{
						DICfg->Config ^= DML_PADHOOK;
						DVDReinsertDisc = true;
					} break;
					case 3:
					{
						DICfg->Config ^= DML_ACTIVITY_LED;
						DVDReinsertDisc = true;
					} break;
					case 4:
					{
						DICfg->Config ^= DML_DEBUGGER;
						DVDReinsertDisc = true;	
					} break;
					case 5:
					{
						DICfg->Config ^= DML_DEBUGWAIT;
						DVDReinsertDisc = true;
					} break;
					case 6:
					{
						DICfg->Config ^= DML_CHEATS;
						DVDReinsertDisc = true;
					} break;					
					case 7:
					{
						if((DICfg->Config & DML_VIDEO_CONF) == DML_VIDEO_PROGP)
						{
							DICfg->Config &= ~DML_VIDEO_CONF;
							DICfg->Config |= DML_VIDEO_GAME;
						} else {
							DICfg->Config += DML_VIDEO_GAME;								
						}

						DVDReinsertDisc = true;
					} break;
					case 8:
					{
						if((DICfg->Config & DML_LANG_CONF) == DML_LANG_DUTCH)
						{
							DICfg->Config &= ~DML_LANG_CONF;
							DICfg->Config |= DML_LANG_ENGLISH;
						} else {
							DICfg->Config += DML_LANG_ENGLISH;								
						}

						DVDReinsertDisc = true;
					} break;
					case 10:
					{
						DVDWriteDIConfig(DICfg);
						
						if(DVDReinsertDisc)
							DVDSelectGame(DICfg->SlotID, 0);

						DVDReinsertDisc = false;
					} break;
				}
				SLock = 1;
			}
			if(GCPad.Up || (*WPad&WPAD_BUTTON_UP))
			{				
				if(PosX == 0)		
					PosX = 10;
				else if(PosX == 10)
					PosX = 8;
				else					
					PosX--;
					
				SLock = 1;
			} 
			else if(GCPad.Down || (*WPad&WPAD_BUTTON_DOWN))
			{
				if(PosX == 8)
					PosX = 10;
				else if(PosX == 10)
					PosX = 0;
				else 	
					PosX++;

				SLock = 1;
			}

			if(GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT))
			{
				switch(PosX)
				{
					case 0:
					{
						DICfg->Config ^= DML_NMM;
						DVDReinsertDisc = true;
					} break;
					case 1:
					{
						DICfg->Config ^= DML_NMM_DEBUG;
						DVDReinsertDisc = true;
					} break;
					case 2:
					{
						DICfg->Config ^= DML_PADHOOK;
						DVDReinsertDisc = true;
					} break;
					case 3:
					{
						DICfg->Config ^= DML_ACTIVITY_LED;
						DVDReinsertDisc = true;
					} break;
					case 4:
					{
						DICfg->Config ^= DML_DEBUGGER;
						DVDReinsertDisc = true;
					} break;
					case 5:
					{
						DICfg->Config ^= DML_DEBUGWAIT;
						DVDReinsertDisc = true;
					} break;
					case 6:
					{
						DICfg->Config ^= DML_CHEATS;
						DVDReinsertDisc = true;
					} break;
					case 7:
					{
						if((DICfg->Config & DML_VIDEO_CONF) == DML_VIDEO_PROGP)
						{
							DICfg->Config &= ~DML_VIDEO_CONF;
							DICfg->Config |= DML_VIDEO_GAME;
						} 
						else 
						{
							DICfg->Config += DML_VIDEO_GAME;								
						}

						DVDReinsertDisc = true;
					} break;
					case 8:
					{
						if((DICfg->Config & DML_LANG_CONF) == DML_LANG_DUTCH)
						{
							DICfg->Config &= ~DML_LANG_CONF;
							DICfg->Config |= DML_LANG_ENGLISH;
						} 
						else 
						{
							DICfg->Config += DML_LANG_ENGLISH;								
						}

						DVDReinsertDisc = true;
					} break;
				}
				SLock = 1;
			} 
			else if(GCPad.Left || (*WPad&WPAD_BUTTON_LEFT))
			{
				switch(PosX)
				{
					case 0:
					{
						DICfg->Config ^= DML_NMM;
						DVDReinsertDisc = true;
					} break;
					case 1:
					{
						DICfg->Config ^= DML_NMM_DEBUG;
						DVDReinsertDisc = true;
					} break;
					case 2:
					{
						DICfg->Config ^= DML_PADHOOK;
						DVDReinsertDisc = true;
					} break;
					case 3:
					{
						DICfg->Config ^= DML_ACTIVITY_LED;
						DVDReinsertDisc = true;
					} break;
					case 4:
					{
						DICfg->Config ^= DML_DEBUGGER;	
						DVDReinsertDisc = true;
					} break;
					case 5:
					{
						DICfg->Config ^= DML_DEBUGWAIT;
						DVDReinsertDisc = true;
					} break;
					case 6:
					{
						DICfg->Config ^= DML_CHEATS;
						DVDReinsertDisc = true;
					} break;
					case 7:
					{
						if((DICfg->Config & DML_VIDEO_CONF) == DML_VIDEO_GAME)
						{
							DICfg->Config &= ~DML_VIDEO_CONF;
							DICfg->Config |= DML_VIDEO_PROGP;
						} 
						else 
						{
							DICfg->Config -= DML_VIDEO_GAME;								
						}

						DVDReinsertDisc = true;
					} break;
					case 8:
					{
						if((DICfg->Config & DML_LANG_CONF) == DML_LANG_ENGLISH)
						{
							DICfg->Config &= ~DML_LANG_CONF;
							DICfg->Config |= DML_LANG_DUTCH;
						} 
						else 
						{
							DICfg->Config -= DML_LANG_ENGLISH;								
						}

						DVDReinsertDisc = true;
					} break;
				}
				SLock = 1;
			}
		} break;
	}
}
void SCheatDraw( void )
{
	u32 i,j;
	offset = (u32*)0x007D0500;
	
	if( Freeze == 0xdeadbeef )
	{
		*offset = value;
	}

	if( *(vu32*)FBEnable != 1 )
		return;

	if( ShowMenu == 0 )
		return;

	for( i=0; i<3; i++)
	{
		if( FB[i] == 0 )
			continue;
		
		switch( ShowMenu )
		{
			case 0:
			{
				PrintFormat( FB[i], MENU_POS_X, 40, "SNEEK+DI " __DATE__ "  Cheater!!!");
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*0, "Search value..." );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*1, "RAM viewer...(NYI)" );

				PrintFormat( FB[i], MENU_POS_X+80-6*3, 104+16*PosX, "-->");

			} break;
			case 2:
			{
				PrintFormat( FB[i], MENU_POS_X, 40, "SNEEK+DI %s  Search value", __DATE__ );
				PrintFormat( FB[i], MENU_POS_X, 104+16*-1, "Hits :%08X", Hits );
				PrintFormat( FB[i], MENU_POS_X+6*6+PosValX*6, 108, "_" );
				PrintFormat( FB[i], MENU_POS_X, 104+16*0, "Value:%08X(%d:%u)", value, value, value );

				for( j=0; j < Hits; ++j )
				{
					if( j > 10 )
						break;
					PrintFormat( FB[i], MENU_POS_X, 104+32+16*j, "%08X:%08X(%d:%u)", Offsets[j], *(u32*)(Offsets[j]), *(u32*)(Offsets[j]), *(u32*)(Offsets[j]) );
				}
				
			} break;
			case 3:
			{
				PrintFormat( FB[i], MENU_POS_X, 32, "SNEEK+DI %s  edit value", __DATE__ );

				for( j=0; j < Hits; ++j )
				{
					if( j > 20 )
						break;
					if( j == PosX && edit )
						PrintFormat( FB[i], MENU_POS_X+9*6+PosValX*6, 64+18*j+2, "_" );
					PrintFormat( FB[i], MENU_POS_X, 64+18*j, "%08X:%08X(%d:%u)", Offsets[j], *(u32*)(Offsets[j]), *(u32*)(Offsets[j]), *(u32*)(Offsets[j]) );
				}

				PrintFormat( FB[i], MENU_POS_X-6*3, 64+18*PosX, "-->");
				
			} break;
			
		}
		//PrintFormat( FB[i], MENU_POS_X+80, 104+16*0, "%08X:%08X(%d)", offset, *offset, *offset );

		//if( Freeze == 0xdeadbeef )
		//	PrintFormat( FB[i], MENU_POS_X+80, 104+16*1, "Frozen!" );

		sync_after_write( (u32*)(FB[i]), FBSize );
	}
}
void SCheatReadPad ( void )
{
	int i;

	memcpy( &GCPad, (u32*)0xD806404, sizeof(u32) * 2 );

	if( ( GCPad.Buttons & 0x1F3F0000 ) == 0 && ( *WPad & 0x0000FFFF ) == 0 )
	{
		SLock = 0;
		return;
	}

	if( SLock == 0 )
	{
		if( (*WPad & ( WPAD_BUTTON_B | WPAD_BUTTON_1 )) == ( WPAD_BUTTON_B | WPAD_BUTTON_1 ) ||
			GCPad.Start )
		{
			ShowMenu = !ShowMenu;
			SLock = 1;
		}

		if( (*WPad & ( WPAD_BUTTON_1 | WPAD_BUTTON_2 )) == ( WPAD_BUTTON_1 | WPAD_BUTTON_2 ) ||
			GCPad.X	)
		{
			u8 *buf = (u8*)malloc( 40 );
			//memcpy( buf, (void*)(FB[0]), FBSize );

			//dbgprintf("ES:Taking RamDump...");

			char *str = (char*)malloc( 32 );

			i=0;

			do
			{
				_sprintf( str, "/scrn_%02X.raw", i++ );
				s32 r = ISFS_CreateFile(str, 0, 3, 3, 3);
				if( r < 0  )
				{
					if( r != -105 )
					{
						//dbgprintf("ES:ISFS_CreateFile():%d\n", r );
						free( buf );
						free( str );
						return;
					}
				} else {
					break;
				}
			} while(1);

			s32 fd = IOS_Open( str, 3 );
			if( fd < 0 )
			{
				//dbgprintf("ES:IOS_Open():%d\n", fd );
				free( buf );
				free( str );
				return;
			}

			IOS_Write( fd, (void*)0, 24*1024*1024 );

			IOS_Close( fd );

			free( buf );
			free( str );

			//dbgprintf("done\n");
			SLock = 1;
		}
	
		//if( !ShowMenu )
		//	return;

		switch( ShowMenu )
		{
			case 0:
			{
				if( (*WPad&WPAD_BUTTON_1) && SLock == 0 )
				{
					hexdump( (void*)0x5CBE60, 0x10 );
					SLock = 1;
				}
			} break;
			case 1:
			{
				if( GCPad.A || (*WPad&WPAD_BUTTON_A) )
				{
					ShowMenu= 2;
					SLock	= 1;
					PosValX	= 0;
				}
			} break;
			case 2:
			{
				if( GCPad.A || (*WPad&WPAD_BUTTON_A) )
				{
					Hits = 0;

					for( i=0; i < 0x01800000; i+=4 )
					{
						if( *(u32*)i == value )
						{
							if( Hits < MAX_HITS )
							{
								Offsets[Hits] = i;
							}
							Hits++;
						}
					}

					SLock	= 1;
				}
				if(  GCPad.B || (*WPad&WPAD_BUTTON_B) )
				{
					ShowMenu= 1;
					SLock	= 1;
				}
				if( GCPad.X || (*WPad&WPAD_BUTTON_PLUS) )
				{
					ShowMenu= 3;
					PosX	= 0;
					edit	= 0;
					SLock	= 1;
				}
				if(  GCPad.Left || (*WPad&WPAD_BUTTON_LEFT))
				{
					if( PosValX > 0 )
						PosValX--;
					SLock = 1;
				} else if(  GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT))
				{
					if( PosValX < 7 )
						PosValX++;
					SLock = 1;
				}
				if( GCPad.Up || (*WPad&WPAD_BUTTON_UP))
				{
					if( ((value>>((7-PosValX)<<2)) & 0xF) == 0xF )
					{
						value &= ~(0xF << ((7-PosValX)<<2));
					} else {
						value += 0x1 << ((7-PosValX)<<2);
					}
					SLock = 1;
				} else if( GCPad.Down || (*WPad&WPAD_BUTTON_DOWN))
				{
					if( ((value>>((7-PosValX)<<2)) & 0xF) == 0x0 )
					{
						value |= (0xF << ((7-PosValX)<<2));
					} else {
						value -= 0x1 << ((7-PosValX)<<2);
					}
					SLock = 1;
				}
			} break;
			case 3:
			{
				if( *WPad == WPAD_BUTTON_A )
				{
					edit	= !edit;
					SLock	= 1;
					PosValX	= 0;
				}
				if( *WPad == WPAD_BUTTON_PLUS )
				{
					ShowMenu= 2;
					PosX	= 0;
					edit	= 0;
					SLock	= 1;
				}
				if( edit == 0 )
				{
					if( *WPad == WPAD_BUTTON_UP )
					{
						if( PosX > 0 )
							PosX--;

						SLock = 1;
					} else if( *WPad == WPAD_BUTTON_DOWN )
					{
						if( PosX <= Hits && PosX <= 20 )
							PosX++;
						else
							PosX = 0;

						SLock = 1;
					}
				} else {
					if( *WPad == WPAD_BUTTON_LEFT )
					{
						if( PosValX > 0 )
							PosValX--;
						SLock = 1;
					} else if( *WPad == WPAD_BUTTON_RIGHT )
					{
						if( PosValX < 7 )
							PosValX++;
						SLock = 1;
					}
					if( *WPad == WPAD_BUTTON_UP )
					{
						u32 val = *(vu32*)(Offsets[PosX]);

						if( ((val>>((7-PosValX)<<2)) & 0xF) == 0xF )
						{
							val &= ~(0xF << ((7-PosValX)<<2));
						} else {
							val += 0x1 << ((7-PosValX)<<2);
						}

						 *(vu32*)(Offsets[PosX]) = val;

						SLock = 1;
					} else if( *WPad == WPAD_BUTTON_DOWN )
					{
						u32 val = *(vu32*)(Offsets[PosX]);

						if( ((val>>((7-PosValX)<<2)) & 0xF) == 0x0 )
						{
							val |= (0xF << ((7-PosValX)<<2));
						} else {
							val -= 0x1 << ((7-PosValX)<<2);
						}

						 *(vu32*)(Offsets[PosX]) = val;

						SLock = 1;
					}
				}
			} break;
		}
	}
}
