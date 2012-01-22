#include "SMenu.h"


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
u32 DVDReinsertDisc=false;
char *DiscName	= (char*)NULL;
char *DVDTitle	= (char*)NULL;
char *DVDBuffer	= (char*)NULL;
ImageStruct* curDVDCover = NULL;

char *PICBuffer = (char*)NULL;
u32 PICSize = 0;
u32 PICNum = 0;

extern char diroot[0x20];

extern u32 LoadDI;

char *RegionStr[] = {
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

unsigned char VISetFB[] =
{
    0x7C, 0xE3, 0x3B, 0x78,		//	mr      %r3, %r7
	0x38, 0x87, 0x00, 0x34,
	0x38, 0xA7, 0x00, 0x38,
	0x38, 0xC7, 0x00, 0x4C, 
};

s32 LaunchTitle(u64 TitleID)
{
	u32 majorTitleID = (TitleID) >> 32;
	u32 minorTitleID = (TitleID) & 0xFFFFFFFF;

	char* ticketPath = (char*) malloca(128,32);
	_sprintf(ticketPath,"/ticket/%08x/%08x.tik",majorTitleID,minorTitleID);
	s32 fd = IOS_Open(ticketPath,1);
	free(ticketPath);

	u32 size = IOS_Seek(fd,0,SEEK_END);
	IOS_Seek(fd,0,SEEK_SET);

	u8* ticketData = (u8*) malloca(size,32);
	IOS_Read(fd,ticketData,size);
	IOS_Close(fd);

	u8* ticketView = (u8*) malloca(0xD8,32);
	iES_GetTicketView(ticketData,ticketView);
	free(ticketData);

	s32 r = ES_LaunchTitle(&TitleID,ticketView);
	free(ticketView);
	return r;
}

void LoadAndRebuildChannelCache()
{
	channelCache = NULL;
	u32 size, i;
	UIDSYS *uid = (UIDSYS *)NANDLoadFile( "/sys/uid.sys", &size );
	if (uid == NULL)
		return;
	u32 numChannels = 0;
	for (i = 0; i * 12 < size; i++){
		u32 majorTitleID = uid[i].TitleID >> 32;
		switch (majorTitleID){
			case 0x00000001: //IOSes
			case 0x00010000: //left over disc stuff
			case 0x00010005: //DLC
			case 0x00010008: //Hidden
				break;
			default:
			{
				s32 fd = ES_OpenContent(uid[i].TitleID,0);
				if (fd >= 0){
					numChannels++;
					IOS_Close(fd);
				}
			} break;
		}
	}

	channelCache = (ChannelCache*)NANDLoadFile("/sneekcache/channelcache.bin",&i);
	if (channelCache == NULL){
		channelCache = (ChannelCache*)malloca(sizeof(ChannelCache),32);
		channelCache->numChannels = 0;
	}

	if (numChannels != channelCache->numChannels || i != sizeof(ChannelCache) + sizeof(ChannelInfo) * numChannels){ // rebuild
		free(channelCache);
		channelCache = (ChannelCache*)malloca(sizeof(ChannelCache) + sizeof(ChannelInfo) * numChannels,32);
		channelCache->numChannels = 0;
		for (i = 0; i * 12 < size; i++){
			u32 majorTitleID = uid[i].TitleID >> 32;
			switch (majorTitleID){
				case 0x00000001: //IOSes
				case 0x00010000: //left over disc stuff
				case 0x00010005: //DLC
				case 0x00010008: //Hidden
					break;
				default:
				{
					s32 fd = ES_OpenContent(uid[i].TitleID,0);
					if (fd >= 0){
						IOS_Seek(fd,0xF0,SEEK_SET);
						u32 j;
						for (j = 0; j < 40; j++){
							IOS_Seek(fd,1,SEEK_CUR);
							IOS_Read(fd,&channelCache->channels[channelCache->numChannels].name[j],1);
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

void __configloadcfg( void )
{
	u32 size;
	PL = NULL;
	PL = (HacksConfig *)NANDLoadFile( "/sneekcache/hackscfg.bin", &size );
	if( PL == NULL )
	{
		PL = (HacksConfig *)malloca( sizeof(HacksConfig), 32);
		PL->EULang 		= 1;
		PL->USLang 		= 1;
		PL->Config 		= 0;
		PL->Autoboot	= 0;
		PL->ChNbr 		= 0;
		PL->DolNr       = 0;
		PL->TitleID 	= 0x0000000100000002LL;
	}
	NANDWriteFileSafe( "/sneekcache/hackscfg.bin", PL , sizeof(HacksConfig) );
}

u32 SMenuFindOffsets( void *ptr, u32 SearchSize )
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
					case VI_EUR60:
						FBSize = 320*480*4;
						break;
					default:
						//dbgprintf("ES:SMenuFindOffsets():Invalid Video mode:%d\n", *(vu32*)(FBEnable+0x20) );
						break;
				}

				return 1;
			}
		}
	}
	return 0;
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
	PICBuffer = (char*)NULL;

	Offsets		= (u32*)malloca( sizeof(u32) * MAX_HITS, 32 );
	GameCount	= (u32*)malloca( sizeof(u32), 32 );
	FB			= (u32*)malloca( sizeof(u32) * MAX_FB, 32 );	
	
	__configloadcfg();

	for( i=0; i < MAX_FB; ++i )
		FB[i] = 0;

//Patches and SNEEK Menu
	switch( TitleID )
	{
		case 0x0000000100000002LL:
		{			
			switch( TitleVersion )
			{
				case 450:	// EUR 4.1
				{
					nisp = 1;
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137D800 = 0x4800001C;
						*(u32*)0x0137D824 = 0x60000000;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013AF580 = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BCDBC = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136AE6C = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136AE40 = 0x4E800020;
					}
				} break;				
				case 482:	// EUR 4.2
				{
					nisp = 1;
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137DC90 = 0x4800001C;
						*(u32*)0x0137E4E4 = 0x60000000;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013AFCFC = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BD620 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136B2CC = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136B2A0 = 0x4E800020;
					}


					//Disc autoboot
					//*(u32*)0x0137AD5C = 0x48000020;
					//*(u32*)0x013799A8 = 0x60000000;

					//BS2Report
					//*(u32*)0x137AEC4 = 0x481B22BC;
					
				} break;
				case 514:	// EUR 4.3
				{
					nisp = 1;
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137DE28 = 0x4800001C;
						*(u32*)0x0137E7A4 = 0x38000001;
					

						//GC-Disc Region free hack
						*(u32*)0x0137DAEC = 0x7F60DB78;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013B0408 = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BDD54 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136B464 = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136B438 = 0x4E800020;
					}
					
					//Autoboot disc
					//*(u32*)0x0137AEF4 = 0x48000020;
					//*(u32*)0x01379B40 = 0x60000000;
					
				} break;
				case 449:	// USA 4.1
				{
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137D758 = 0x4800001C;
						*(u32*)0x0137D77C = 0x60000000;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013AF580 = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BCCC0 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136ADC4 = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136AD98 = 0x4E800020;
					}
				} break;
				case 481:	// USA 4.2
				{
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137DBE8 = 0x4800001C;
						*(u32*)0x0137E43C = 0x60000000;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013AFC00 = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BD524 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136B224 = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136B1F8 = 0x4E800020;
					}
				} break;
				case 513:	// USA 4.3
				{
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137DD80 = 0x4800001C;
						*(u32*)0x0137E5D4 = 0x60000000;

						//GC-Disc Region free hack
						*(u32*)0x137DA44 = 0x7F60DB78;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013B030C = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BDC58 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136B3BC = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136B390 = 0x4E800020;
					}

				} break;
				case 448:   // JPN 4.1
				{				
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137CC0C = 0x4800001C;
						*(u32*)0x0137CC30 = 0x60000000;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013AE770 = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BBFA0 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136A278 = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136A24C = 0x4E800020;
					}

				} break;
				case 480:   // JPN 4.2
				{				
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137D09C = 0x4800001C;
						*(u32*)0x0137D8F0 = 0x60000000;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013AEEEC = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BC804 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136A6D8 = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136A6AC = 0x4E800020;
					}

				} break;
				case 512:   // JPN 4.3
				{				
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137D234 = 0x4800001C;
						*(u32*)0x0137DA88 = 0x60000000;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013AF5F8 = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BCF38 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136A870 = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136A844 = 0x4E800020;
					}

				} break;
				case 486:   // KOR 4.2
				{				
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137CF7C = 0x4800001C;
						*(u32*)0x0137D7D0 = 0x60000000;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013AEF40 = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BC8D4 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136A5B8 = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136A58C = 0x4E800020;
					}

				} break;				
				case 518:   // KOR 4.3
				{	
					if( PL->Config&CONFIG_REGION_FREE )
					{
						//Wii-Disc Region free hack
						*(u32*)0x0137D114 = 0x4800001C;
						*(u32*)0x0137D968 = 0x60000000;
					}
					if( PL->Config&CONFIG_MOVE_DISC_CHANNEL )
					{
						//Move Disc Channel
						*(u32*)0x013AF64C = 0x60000000;
					}
					if( PL->Config&CONFIG_PRESS_A )
					{
						//Auto Press A at Health Screen
						*(u32*)0x013BD008 = 0x48000034;
					}
					if( PL->Config&CONFIG_NO_BG_MUSIC )
					{
						//No System Menu Background Music
						*(u32*)0x0136A750 = 0x4E800020;
					}
					if( PL->Config&CONFIG_NO_SOUND )
					{
						//No System Menu sounds AT ALL
						*(u32*)0x0136A724 = 0x4E800020;
					}
				} break;	
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
void SMenuDraw( void )
{
	u32 i,j;
	s32 EntryCount=0;

	if( *(vu32*)FBEnable != 1 )
		return;

	if( ShowMenu == 0 )
		return;

	if( DICfg == NULL )
		return;

	if( DICfg->Config & CONFIG_SHOW_COVERS )
		EntryCount = 8;
	else
		EntryCount = 20;

	for( i=0; i < MAX_FB; i++)
	{
		if( FB[i] == 0 )
			continue;

		if( DICfg->Region > ALL )
			DICfg->Region = ALL;

		if( MenuType != 3 )
		{
			if( FSUSB )
			{
				if(LoadDI == true)
					PrintFormat( FB[i], MENU_POS_X, 20, "UNEEK2O+cDI r72 %s Games:%d Region:%s", __DATE__, *GameCount, RegionStr[DICfg->Region] );
				else
					PrintFormat( FB[i], MENU_POS_X, 20, "UNEEK2O r72 %s",__DATE__);					
			} 
			else 
			{
				if(LoadDI == true)
					PrintFormat( FB[i], MENU_POS_X, 20, "SNEEK2O+cDI r72 %s Games:%d Region:%s", __DATE__, *GameCount, RegionStr[DICfg->Region] );
				else
					PrintFormat( FB[i], MENU_POS_X, 20, "SNEEK2O r72 %s",__DATE__);					
			}
		}

		switch( MenuType )
		{
			case 0:
			{
				u32 gRegion = 0;

				switch( *(u8*)(DICfg->GameInfo[PosX+ScrollX] + 3) )
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

				PrintFormat( FB[i], MENU_POS_X, 20+16, "Press PLUS for settings  Press MINUS for dumping" );
				
				PrintFormat( FB[i], MENU_POS_X, MENU_POS_Y, "GameRegion:%s", RegionStr[gRegion] );

				PrintFormat( FB[i], MENU_POS_X + 420, MENU_POS_Y,"Press HOME for Channels");

				for( j=0; j<EntryCount; ++j )
				{
					if( j+ScrollX >= *GameCount )
						break;

					if( *(vu32*)(DICfg->GameInfo[ScrollX+j]+0x1C) == 0xc2339f3d )
						PrintFormat( FB[i], MENU_POS_X, MENU_POS_Y+32+16*j, "%.40s (GC)", DICfg->GameInfo[ScrollX+j] + 0x20 );
					else if( *(vu32*)(DICfg->GameInfo[ScrollX+j]+0x18) == 0x5D1C9EA3 &&  *(vu32*)(DICfg->GameInfo[ScrollX+j]+0x1c) == 0x57424653 )
						PrintFormat( FB[i], MENU_POS_X, MENU_POS_Y+32+16*j, "%.40s (WBFS)", DICfg->GameInfo[ScrollX+j] + 0x20 );
					else if( *(vu32*)(DICfg->GameInfo[ScrollX+j]+0x18) == 0x5D1C9EA3 &&  ((*(vu32*)(DICfg->GameInfo[ScrollX+j]+0x1c) == 0x44534358)||(DIConfType == 0)))
						PrintFormat( FB[i], MENU_POS_X, MENU_POS_Y+32+16*j, "%.40s (FST)", DICfg->GameInfo[ScrollX+j] + 0x20 );
					else
						PrintFormat( FB[i], MENU_POS_X, MENU_POS_Y+32+16*j, "%.37s (Invalid)", DICfg->GameInfo[ScrollX+j] + 0x20 );

					if( j == PosX )
						PrintFormat( FB[i], 0, MENU_POS_Y+32+16*j, "-->");
				}

				if( DICfg->Config & CONFIG_SHOW_COVERS )
				{
					if (curDVDCover)
					{
						DrawImage( FB[i], MENU_POS_X, MENU_POS_Y+(j+2)*16, curDVDCover );
					} else
						PrintFormat( FB[i], MENU_POS_X+6*12, MENU_POS_Y+(j+6)*16, "no cover image found!" );
				}

				PrintFormat( FB[i], MENU_POS_X+575, MENU_POS_Y+16*22, "%d/%d", ScrollX/EntryCount + 1, *GameCount/EntryCount + (*GameCount % EntryCount > 0));

				sync_after_write( (u32*)(FB[i]), FBSize );
			} break;

			case 4:
			{
				PrintFormat( FB[i], MENU_POS_X, 20+16, "Close the HOME menu before launching!!!" );
				PrintFormat( FB[i], MENU_POS_X, MENU_POS_Y, "Installed Channels:%u", channelCache->numChannels);

				for( j=0; j<EntryCount; ++j )
				{
					if( j+ScrollX >= channelCache->numChannels )
						break;

					PrintFormat( FB[i], MENU_POS_X, MENU_POS_Y+32+16*j, "%.40s", channelCache->channels[ScrollX+j].name);

					if( j == PosX )
						PrintFormat( FB[i], 0, MENU_POS_Y+32+16*j, "-->");
				}

				if( DICfg->Config & CONFIG_SHOW_COVERS )
				{
					if (curDVDCover){
						DrawImage(FB[i],MENU_POS_X,MENU_POS_Y+(j+2)*16,curDVDCover);
					}
					else
						PrintFormat(FB[i],MENU_POS_X,MENU_POS_Y+(j+2)*16,"no cover image found!");
				}

				PrintFormat( FB[i], MENU_POS_X+575, MENU_POS_Y+16*21, "%d/%d", ScrollX/EntryCount + 1, channelCache->numChannels/EntryCount + (channelCache->numChannels % EntryCount > 0));

				sync_after_write( (u32*)(FB[i]), FBSize );
			} break;

			case 1:
			{
				PrintFormat( FB[i], MENU_POS_X+80, 56, "NEEK Config:" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*0, "Game Region     :%s", RegionStr[DICfg->Region] );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*1, "__fwrite Patch  :%s", (DICfg->Config&CONFIG_PATCH_FWRITE) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*2, "MotionPlus Video:%s", (DICfg->Config&CONFIG_PATCH_MPVIDEO) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*3, "Video Mode Patch:%s", (DICfg->Config&CONFIG_PATCH_VIDEO) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*4, "Error Skipping  :%s", (DICfg->Config&CONFIG_DUMP_ERROR_SKIP) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*5, "Display Covers  :%s", (DICfg->Config&CONFIG_SHOW_COVERS) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*6, "AutoUpdate Games:%s", (DICfg->Config&CONFIG_AUTO_UPDATE_LIST) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*7, "Game Debugging  :%s", (DICfg->Config&CONFIG_DEBUG_GAME) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*8, "Debugger Wait   :%s", (DICfg->Config&CONFIG_DEBUG_GAME_WAIT) ? "On" : "Off" );
				
				switch( (DICfg->Config&HOOK_TYPE_MASK) )
				{
					case HOOK_TYPE_VSYNC:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*9, "Hook Type       :%s", "VIWaitForRetrace" );
					break;
					case HOOK_TYPE_OSLEEP:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*9, "Hook Type       :%s", "OSSleepThread" );
					break;
					//case HOOK_TYPE_AXNEXT:
					//	PrintFormat( FB[i], MENU_POS_X+80, 104+16*9, "Hook type       :%s", "__AXNextFrame" );
					//break;
					default:
						PrintFormat( FB[i], MENU_POS_X+80, 104+16*9, "Hook Type       :Invalid Type:%d", (DICfg->Config&HOOK_TYPE_MASK)>>28 );
					break;
				}

				PrintFormat( FB[i], MENU_POS_X+80, 104+16*11, "Save Config" );
				PrintFormat( FB[i], MENU_POS_X+80, 104+16*12, "Recreate Game Cache(restarts!!)" );
				if( fnnd ) 
					PrintFormat( FB[i], MENU_POS_X+80, 104+16*13, "Select Emunand: %.20s", NandCfg->NandInfo[NandCfg->NandSel]+NANDDESC_OFF );
				else
					PrintFormat( FB[i], MENU_POS_X+80, 104+16*13, "Select Emunand: Root Nand" );
				
				if( FSUSB )
					PrintFormat( FB[i], MENU_POS_X+80, 104+16*14, "Boot NMM" );
				

				PrintFormat( FB[i], MENU_POS_X+60, 40+64+16*PosX, "-->");
				sync_after_write( (u32*)(FB[i]), FBSize );
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
			case 3:
			{
				memcpy( (void *)FB[i], PICBuffer, FBSize );
			} break;
			case 5:
			{				
				PrintFormat( FB[i], MENU_POS_X+15, 52, "Menu Hacks:" );			
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*0, "Auto Press A at Health Screen             :%s", ( PL->Config&CONFIG_PRESS_A ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*1, "No System Menu Background Music           :%s", ( PL->Config&CONFIG_NO_BG_MUSIC ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*2, "No System Menu Sounds At All              :%s", ( PL->Config&CONFIG_NO_SOUND ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*3, "Move Disc Channel                         :%s", ( PL->Config&CONFIG_MOVE_DISC_CHANNEL ) ? "On" : "Off" );				
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*4, "Bypass WIFI Connection Test               :%s", ( PL->Config&CONFIG_FORCE_INET ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*6, "Region Free Hacks:" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*8, "System Region Free Hack                   :%s", ( PL->Config&CONFIG_REGION_FREE ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*9, "Temp Region Change                        :%s", ( PL->Config&CONFIG_REGION_CHANGE ) ? "On" : "Off" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*10, "EUR Default Language:  %s", LanguageStr[PL->EULang] );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*11, "USA Default Language:  %s", LanguageStr[PL->USLang] );
				if( nisp )
					PrintFormat( FB[i], MENU_POS_X+15, 84+16*12, "Force EuRGB60 with NTSC games on PAL nands:%s", ( PL->Config&CONFIG_FORCE_EuRGB60 ) ? "On" : "Off" );
				else	
					PrintFormat( FB[i], MENU_POS_X+15, 84+16*12, "Force EuRGB60 with NTSC games on PAL nands:NA" );
				PrintFormat( FB[i], MENU_POS_X+15, 84+16*14, "Auto Boot Options:" );
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
			default:
			{
				if(LoadDI == true)
					MenuType = 0;
				else
					MenuType = 4;
				ShowMenu = 0;
			} break;
		}
	}
}
void LoadDVDCover()
{
	if (curDVDCover != NULL)
		free(curDVDCover);

	curDVDCover = NULL;
	
	if (DICfg == NULL || PosX + ScrollX >= DICfg->Gamecount)
		return;

	char* imgPathBuffer = (char*)malloca(160,32);
	_sprintf( imgPathBuffer, "/sneek/covers/%s.raw", DICfg->GameInfo[PosX+ScrollX]);

	curDVDCover = LoadImage(imgPathBuffer);

	if (curDVDCover == NULL)
	{
		_sprintf( imgPathBuffer, "/sneek/covers/%s.bmp", DICfg->GameInfo[PosX+ScrollX]);
		curDVDCover = LoadImage(imgPathBuffer);
	}

	free(imgPathBuffer);
}
void LoadChannelCover()
{
	if (curDVDCover != NULL)
		free(curDVDCover);

	curDVDCover = NULL;
	
	if (channelCache == NULL || PosX + ScrollX >= channelCache->numChannels )
		return;

	char* imgPathBuffer = (char*)malloca(160,32);
	u32 minorTitleID = channelCache->channels[PosX+ScrollX].titleID & 0xFFFFFFFF;

	_sprintf( imgPathBuffer, "/sneek/covers/%c%c%c%c.raw", minorTitleID >> 24 & 0xFF, minorTitleID >> 16 & 0xFF, minorTitleID >> 8  & 0xFF, minorTitleID & 0xFF );
	curDVDCover = LoadImage(imgPathBuffer);

	if (curDVDCover == NULL)
	{
		_sprintf( imgPathBuffer, "/sneek/covers/%c%c%c%c.bmp", minorTitleID >> 24 & 0xFF, minorTitleID >> 16 & 0xFF, minorTitleID >> 8  & 0xFF, minorTitleID & 0xFF );
		curDVDCover = LoadImage(imgPathBuffer);
	}

	free(imgPathBuffer);
}
void SMenuReadPad ( void )
{
	s32 EntryCount=0;
	u32 Gameidx;

	memcpy( &GCPad, (u32*)0xD806404, sizeof(u32) * 2 );

	if( ( GCPad.Buttons & 0x1F3F0000 ) == 0 && ( *WPad & 0x0000FFFF ) == 0 )
	{
		SLock = 0;
		return;
	}

	if( SLock != 0 )
		return;

	if( GCPad.Start || (*WPad&WPAD_BUTTON_1) )
	{
		ShowMenu = !ShowMenu;
		if(ShowMenu)
		{
			if( DICfg == NULL )
			{	
				if (LoadDI != true)
				{
                	DICfg = (DIConfig *)malloca( 0x110, 32 );
					DICfg->Gamecount = 0;
					DICfg->SlotID = 0;
					DICfg->Region = 0;
					DICfg->Config = 0;
				}
				else
				{
					DVDGetGameCount( GameCount );
					if ((*GameCount & 0xF0000)==0x10000)
					{
						DIConfType = 1;
                		*GameCount &= ~0x10000;
                		DICfg = (DIConfig *)malloca( *GameCount * 0x100 + 0x10, 32 );
                		DVDReadGameInfo( 0, *GameCount * 0x100 + 0x10, DICfg );
						//DoEE( 432, DICfg );
					}
					else
					{
						DIConfType = 0;
                		DICfgO = (DIConfigO *)malloca( *GameCount * 0x80 + 0x10, 32 );
                		DVDReadGameInfo( 0, *GameCount * 0x80 + 0x10, DICfgO );
                		DICfg = (DIConfig *)malloca( *GameCount * 0x100 + 0x10, 32 );
						DICfg->SlotID = DICfgO->SlotID;
						DICfg->Region = DICfgO->Region;
						DICfg->Gamecount = DICfgO->Gamecount;
						DICfg->Config = DICfgO->Config;
						for(Gameidx = 0;Gameidx < (*GameCount);Gameidx++)
						{
							memcpy(DICfg->GameInfo[Gameidx],DICfgO->GameInfo[Gameidx],0x80);
						}	
						free(DICfgO);
                	}
				}
			}
			if( NandCfg == NULL )
			{
				char *path = malloca( 0x40, 0x40 );
				strcpy(path, "/sneek/NandCfg.bin");
				u32* fsize = malloca(sizeof(u32),0x20);
				*fsize = 0x10;
				NandCfg = (NandConfig*)NANDLoadFile(path,fsize);
				if (NandCfg != NULL)
				{
					*NandCount = NandCfg->NandCnt;
					heap_free( 0, NandCfg );
					*fsize = *NandCount * 0x80 + 0x10;
					NandCfg = (NandConfig*)NANDLoadFile(path,fsize);
					if (NandCfg != NULL)
					{
						fnnd = 1;	
					}
				}
				free (fsize);
				free(path);
			}
			if( MenuType == 0 && (DICfg->Config & CONFIG_SHOW_COVERS) )
				LoadDVDCover();
		}
		SLock = 1;
	}

	if( !ShowMenu )
		return;
		
	if( DICfg->Config & CONFIG_SHOW_COVERS )
		EntryCount = 8;
	else
		EntryCount = 20;

	if( (GCPad.B || (*WPad&WPAD_BUTTON_B) ) && SLock == 0 )
	{
		if( MenuType == 3 )
			free( PICBuffer );
		
		if( curDVDCover != NULL )
			free(curDVDCover);

		curDVDCover = NULL;
		if( MenuType == 0 )
			ShowMenu = 0;
		if(LoadDI == true)
			MenuType = 0;
		else
		{
			if(MenuType == 4)
				ShowMenu = 0;
			MenuType = 4;
		}
		PosX	= 0;
		ScrollX	= 0;
		SLock	= 1;

		if( ShowMenu != 0 && DICfg->Config & CONFIG_SHOW_COVERS )
			LoadDVDCover();
	}

	if( (GCPad.X || (*WPad&WPAD_BUTTON_PLUS) ) && SLock == 0 )
	{
		if( curDVDCover != NULL )
			free(curDVDCover);

		curDVDCover = NULL;
		
		if( MenuType == 1 )
		{
			MenuType = 5;
			PosX = 0;
			
			ScrollX	= 0;
		}
		else
		{
			MenuType = 1;
			if (LoadDI == true)
				PosX = 0;
			else
				PosX = 13;
				
			ScrollX	= 0;
		}
		SLock	= 1;
	}

	if( (GCPad.Y || (*WPad&WPAD_BUTTON_MINUS) ) && SLock == 0 && MenuType != 2 && LoadDI == true)
	{
		if( curDVDCover != NULL )
			free(curDVDCover);

		curDVDCover = NULL;

		MenuType = 2;

		PosX	= 0;
		ScrollX	= 0;
		SLock	= 1;
	}

	if( (GCPad.Z || (*WPad&WPAD_BUTTON_2) ) && SLock == 0 && MenuType != 3 )
	{
		if( curDVDCover != NULL )
			free(curDVDCover);

		curDVDCover = NULL;

		MenuType= 3;
		PICSize	= 0;
		PICNum	= 0;

		s32 fd = IOS_Open("/scrn_00.raw", 1 );
		if( fd >= 0 )
		{
			PICSize = IOS_Seek( fd, 0, SEEK_END );
			IOS_Seek( fd, 0, 0 );
			PICBuffer = (char*)malloca( FBSize, 32 );
			IOS_Read( fd, PICBuffer, FBSize );
			IOS_Close( fd );
		}

		PosX	= 0;
		ScrollX	= 0;
		SLock	= 1;
	}

	if( *WPad & WPAD_BUTTON_HOME && MenuType != 4 )
	{
		if( curDVDCover != NULL )
			free(curDVDCover);

		curDVDCover = NULL;

		MenuType= 4;
		PosX	= 0;
		ScrollX	= 0;
		SLock	= 1;

		if( DICfg->Config & CONFIG_SHOW_COVERS )
			LoadChannelCover();
	}

	switch( MenuType )
	{
		case 4: //channel list
		{
			if( GCPad.A || (*WPad&WPAD_BUTTON_A) )
			{
				if (curDVDCover != NULL)
					free(curDVDCover);
				curDVDCover = NULL;
				ShowMenu = 0;
				LaunchTitle(channelCache->channels[PosX + ScrollX].titleID);
				SLock = 1;
				break;
			}
			if( GCPad.Up || (*WPad&WPAD_BUTTON_UP) )
			{
				if( PosX ){
					PosX--;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadChannelCover();
				}
				else if( ScrollX )
				{
					ScrollX--;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadChannelCover();
				}

				SLock = 1;
			} else if( GCPad.Down || (*WPad&WPAD_BUTTON_DOWN) )
			{
				if( PosX >= EntryCount-1 )
				{
					if( PosX+ScrollX+1 < channelCache->numChannels )
					{
						ScrollX++;
						if( DICfg->Config & CONFIG_SHOW_COVERS )
							LoadChannelCover();
					}
				} else if ( PosX+ScrollX+1 < channelCache->numChannels ){
					PosX++;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadChannelCover();
				}

				SLock = 1;
			} else if( GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT) )
			{
				if( ScrollX/EntryCount*EntryCount + EntryCount < channelCache->numChannels )
				{
					PosX	= 0;
					ScrollX = ScrollX/EntryCount*EntryCount + EntryCount;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadChannelCover();
				} else {
					PosX	= 0;
					ScrollX	= 0;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadChannelCover();
				}

				SLock = 1; 
			} else if( GCPad.Left || (*WPad&WPAD_BUTTON_LEFT) )
			{
				if( ScrollX/EntryCount*EntryCount - EntryCount > 0 )
				{
					PosX	= 0;
					ScrollX-= EntryCount;
				} else {
					PosX	= 0;
					ScrollX	= 0;
				}
				if( DICfg->Config & CONFIG_SHOW_COVERS )
					LoadChannelCover();

				SLock = 1; 
			}
		} break;
		case 0:			// Game list
		{
			if( GCPad.A || (*WPad&WPAD_BUTTON_A) )
			{
				DVDSelectGame( PosX+ScrollX );
				if (curDVDCover != NULL)
					free(curDVDCover);
				curDVDCover = NULL;
				ShowMenu = 0;
				SLock = 1;
			}
			if( GCPad.Up || (*WPad&WPAD_BUTTON_UP) )
			{
				if( PosX )
				{
					PosX--;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadDVDCover();
				}
				else if( ScrollX )
				{
					ScrollX--;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadDVDCover();
				}

				SLock = 1;
			} else if( GCPad.Down || (*WPad&WPAD_BUTTON_DOWN) )
			{
				if( PosX >= EntryCount-1 )
				{
					if( PosX+ScrollX+1 < *GameCount )
					{
						ScrollX++;
						if( DICfg->Config & CONFIG_SHOW_COVERS )
							LoadDVDCover();
					}
				} else if ( PosX+ScrollX+1 < *GameCount )
				{
					PosX++;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadDVDCover();
				}

				SLock = 1;
			} else if( GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT) )
			{
				if( ScrollX/EntryCount*EntryCount + EntryCount < DICfg->Gamecount )
				{
					PosX	= 0;
					ScrollX = ScrollX/EntryCount*EntryCount + EntryCount;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadDVDCover();
				} else {
					PosX	= 0;
					ScrollX	= 0;
					if( DICfg->Config & CONFIG_SHOW_COVERS )
						LoadDVDCover();
				}

				SLock = 1; 
			} else if( GCPad.Left || (*WPad&WPAD_BUTTON_LEFT) )
			{
				if( ScrollX/EntryCount*EntryCount - EntryCount > 0 )
				{
					PosX	= 0;
					ScrollX-= EntryCount;
				} else {
					PosX	= 0;
					ScrollX	= 0;
				}

				if( DICfg->Config & CONFIG_SHOW_COVERS )
					LoadDVDCover();

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
						if( DICfg->Region == LTN )
							DICfg->Region = JAP;
						else
							DICfg->Region++;
						SLock = 1;
						DVDReinsertDisc=true;
					} break;
					case 1:
					{
						DICfg->Config ^= CONFIG_PATCH_FWRITE;
						DVDReinsertDisc=true;
					} break;
					case 2:
					{
						DICfg->Config ^= CONFIG_PATCH_MPVIDEO;
						DVDReinsertDisc=true;
					} break;
					case 3:
					{
						DICfg->Config ^= CONFIG_PATCH_VIDEO;
						DVDReinsertDisc=true;
					} break;
					case 4:
					{
						DICfg->Config ^= CONFIG_DUMP_ERROR_SKIP;
					} break;
					case 5:
					{
						DICfg->Config ^= CONFIG_SHOW_COVERS;
					} break;
					case 6:
					{
						DICfg->Config ^= CONFIG_AUTO_UPDATE_LIST;
					} break;
					case 7:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME;
						DVDReinsertDisc=true;
					} break;
					case 8:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME_WAIT;
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
					case 11:
					{
						if( DVDReinsertDisc )
							DVDSelectGame( DICfg->SlotID );

						DVDWriteDIConfig( DICfg );

						DVDReinsertDisc=false;
					} break;
					case 12:
					{
						DICfg->Gamecount = 0;
						DICfg->Config	|= CONFIG_AUTO_UPDATE_LIST;
						DVDWriteDIConfig( DICfg );
						LaunchTitle( 0x0000000100000002LL );	
					} break;
					case 13:
					{
						Save_Nand_Cfg( NandCfg );
						NANDWriteFileSafe( "/sneek/nandcfg.bin", NandCfg , NandCfg->NandCnt * NANDINFO_SIZE + NANDCFG_SIZE );
						LaunchTitle( 0x0000000100000002LL );
					} break;
					case 14:			
					{
						LaunchTitle( 0x0000000100000100LL );						
					} break;
					
				}
				SLock = 1;
			}
			if( GCPad.Up || (*WPad&WPAD_BUTTON_UP) )
			{
				if(LoadDI == true)
				{
					if( PosX )
					{		
						PosX--;
					}
					else {
						if( FSUSB )
							PosX  = 14;
						else
							PosX = 13;
					}
				}
				if( PosX == 10 )
					PosX  = 9;

				SLock = 1;
			} else if( GCPad.Down || (*WPad&WPAD_BUTTON_DOWN) )
			{
				if(LoadDI == true)
				{
					if( FSUSB )
					{
						if( PosX >= 14 )
						{
							PosX=0;
						} 
						else
						{ 
							PosX++;
						}
					} 
					else 
					{
						if( PosX >= 13 )
						{
							PosX=0;
						} 
						else 
						{	
							PosX++;
						}
					}
				}

				if( PosX == 10 )
					PosX  = 11;

				SLock = 1;
			}

			if( GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT) )
			{
				switch( PosX )
				{
					case 0:
					{
						if( DICfg->Region == LTN )
							DICfg->Region = JAP;
						else
							DICfg->Region++;
						DVDReinsertDisc=true;
					} break;
					case 1:
					{
						DICfg->Config ^= CONFIG_PATCH_FWRITE;
						DVDReinsertDisc=true;
					} break;
					case 2:
					{
						DICfg->Config ^= CONFIG_PATCH_MPVIDEO;
						DVDReinsertDisc=true;
					} break;
					case 3:
					{
						DICfg->Config ^= CONFIG_PATCH_VIDEO;
						DVDReinsertDisc=true;
					} break;
					case 4:
					{
						DICfg->Config ^= CONFIG_DUMP_ERROR_SKIP;
					} break;
					case 5:
					{
						DICfg->Config ^= CONFIG_SHOW_COVERS;
					} break;
					case 6:
					{
						DICfg->Config ^= CONFIG_AUTO_UPDATE_LIST;
					} break;
					case 7:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME;
						DVDReinsertDisc=true;
					} break;
					case 8:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME_WAIT;
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
						if( fnnd )
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
						if( DICfg->Region == JAP )
							DICfg->Region = LTN;
						else
							DICfg->Region--;
						DVDReinsertDisc=true;
					} break;
					case 1:
					{
						DICfg->Config ^= CONFIG_PATCH_FWRITE;
						DVDReinsertDisc=true;
					} break;
					case 2:
					{
						DICfg->Config ^= CONFIG_PATCH_MPVIDEO;
						DVDReinsertDisc=true;
					} break;
					case 3:
					{
						DICfg->Config ^= CONFIG_PATCH_VIDEO;
						DVDReinsertDisc=true;
					} break;
					case 4:
					{
						DICfg->Config ^= CONFIG_DUMP_ERROR_SKIP;
					} break;
					case 5:
					{
						DICfg->Config ^= CONFIG_SHOW_COVERS;
					} break;
					case 6:
					{
						DICfg->Config ^= CONFIG_AUTO_UPDATE_LIST;
					} break;
					case 7:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME;
						DVDReinsertDisc=true;
					} break;
					case 8:
					{
						DICfg->Config ^= CONFIG_DEBUG_GAME_WAIT;
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
			if( GCPad.A || (*WPad&WPAD_BUTTON_A) )
			{
				if( DVDStatus == 2 && DVDType > 0 )
				{
					DVDStatus = 3;
				}
				SLock = 1;
			}
		} break;
		case 3:
		{
			u32 Update = false;
			if( GCPad.Left || (*WPad&WPAD_BUTTON_LEFT) )
			{
				if( PICNum > 0 )
				{
					PICNum--;
					Update = true;
				}
				SLock = 1;
			}
			if( GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT) )
			{
				PICNum++;
				Update = true;
					
				SLock = 1;
			}

			if( Update )
			{
				char *Path = (char*)malloca( 32, 32 );
				_sprintf( Path, "/scrn_%02X.raw", PICNum );

				s32 fd = IOS_Open( Path, 1 );
				if( fd >= 0 )
				{
					PICSize = IOS_Seek( fd, 0, SEEK_END );
					IOS_Seek( fd, 0, 0 );
					IOS_Read( fd, PICBuffer, PICSize );
					IOS_Close( fd );
				}

				free(Path);
			}
		} break;
		case 5:		//Menu and region hacks
		{		
			if( GCPad.A || (*WPad&WPAD_BUTTON_A) )
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
						PL->Config ^= CONFIG_FORCE_INET;
						rebreq = 1;
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
						if( PL->EULang == 6 )
							PL->EULang = 1;
						else
							PL->EULang++;
					} break;
					case 11:
					{
						if( PL->USLang == 1 )
							PL->USLang = 3;
						else if( PL->USLang == 3 )
							PL->USLang = 4;
						else if( PL->USLang == 4 )
							PL->USLang = 1;
					} break;
					case 12:
					{
						if( nisp )
							PL->Config ^= CONFIG_FORCE_EuRGB60;
					} break;
					case 16:
					{
						if( PL->Autoboot == 1 )
							PL->Autoboot = 0;
						else
							PL->Autoboot++;
					} break;
					case 17:
					{
						if( PL->Autoboot == 1 )
						{
							if( PL->ChNbr == channelCache->numChannels-1 )
								PL->ChNbr = 0;
							else
								PL->ChNbr++;
							
							PL->TitleID = channelCache->channels[PL->ChNbr].titleID;
						}
						if( PL->Autoboot == 2 )
						{
							//if( PL->DolNr == *Cnt )
							//	PL->DolNr = 0;
							//else
							//	PL->DolNr++;
							

						}
						
					} break;
					case 19:
					{
						NANDWriteFileSafe( "/sneekcache/hackscfg.bin", PL , sizeof(HacksConfig) );
						if( rebreq )
							LaunchTitle( 0x0000000100000002LL );						
					} break;
				}
				SLock = 1;
			}
			if( GCPad.Up || (*WPad&WPAD_BUTTON_UP) )
			{
				if( PosX )
				{		
					PosX--;
				}					
				
				if( PosX == 18 )
				{
					if( PL->Autoboot == 1 || PL->Autoboot == 2 )
						PosX = 17;
					else
						PosX = 16;
				}
				else if( PosX == 15 )
				{
					PosX = 12;
				}
				else if( PosX == 7 )
				{
					PosX = 4;
				}

				SLock = 1;
			} 
			else if( GCPad.Down || (*WPad&WPAD_BUTTON_DOWN) )
			{
				if( PosX >= 19 )
				{
					PosX=0;
				} 
				else 
				{	
					PosX++;
				}

				if( PosX == 5 )
					PosX = 8;
				else if( PosX == 13 )
					PosX = 16;
				else if( PosX == 17 && PL->Autoboot == 0 )
					PosX = 19;
				else if( PosX == 18 )
					PosX = 19;	
				SLock = 1;
			}

			if( GCPad.Right || (*WPad&WPAD_BUTTON_RIGHT) )
			{
				switch( PosX )
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
						PL->Config ^= CONFIG_FORCE_INET;
						rebreq = 1;
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
						if( PL->EULang == 6 )
							PL->EULang = 1;
						else
							PL->EULang++;
					} break;
					case 11:
					{
						if( PL->USLang == 1 )
							PL->USLang = 3;
						else if( PL->USLang == 3 )
							PL->USLang = 4;
						else if( PL->USLang == 4 )
							PL->USLang = 1;
					} break;
					case 12:
					{
						if( nisp )
							PL->Config ^=CONFIG_FORCE_EuRGB60;
					} break;
					case 16:
					{
						if( PL->Autoboot == 1 )
							PL->Autoboot = 0;
						else
							PL->Autoboot++;
					} break;
					case 17:
					{
						if( PL->Autoboot == 1 )
						{
							if( PL->ChNbr == channelCache->numChannels-1 )
								PL->ChNbr = 0;
							else
								PL->ChNbr++;
							
							PL->TitleID = channelCache->channels[PL->ChNbr].titleID;
						}
						else if( PL->Autoboot == 2 )
						{
							//if( PL->DolNr == *Cnt )
							//	PL->DolNr = 0;
							//else
							//	PL->DolNr++;

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
						PL->Config ^= CONFIG_FORCE_INET;
						rebreq = 1;
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
						if( PL->EULang == 1 )
							PL->EULang = 6;
						else
							PL->EULang--;
					} break;
					case 11:
					{
						if( PL->USLang == 1 )
							PL->USLang = 4;
						else if( PL->USLang == 3 )
							PL->USLang = 1;
						else if( PL->USLang == 4 )
							PL->USLang = 3;
					} break;
					case 12:
					{
						if( nisp )
							PL->Config ^=CONFIG_FORCE_EuRGB60;
					} break;
					case 16:
					{
						if( PL->Autoboot == 0 )
							PL->Autoboot = 1;
						else
							PL->Autoboot--;
					} break;
					case 17:
					{
						if( PL->Autoboot == 1 )
						{
							if( PL->ChNbr == 0 )
							PL->ChNbr = channelCache->numChannels-1;
							else
								PL->ChNbr--;
							
							PL->TitleID = channelCache->channels[PL->ChNbr].titleID;
						}
						if( PL->Autoboot == 2 )
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
