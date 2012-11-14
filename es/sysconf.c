/**************************************************************************************
***																					***
*** neek2o - sysconf.c																***
***																					***
*** Copyright (C) 2011-2012 OverjoY													***
*** 																				***
*** This program is free software; you can redistribute it and/or					***
*** modify it under the terms of the GNU General Public License						***
*** as published by the Free Software Foundation version 2.							***
***																					***
*** This program is distributed in the hope that it will be useful,					***
*** but WITHOUT ANY WARRANTY; without even the implied warranty of					***
*** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the					***
*** GNU General Public License for more details.									***
***																					***
*** You should have received a copy of the GNU General Public License				***
*** along with this program; if not, write to the Free Software						***
*** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. ***
***																					***
**************************************************************************************/

#include "sysconf.h"
#include "SMenu.h"

u8 *confbuffer;
u8 CCode[0x1008];
char SCode[4];
char *txtbuffer;
static char configpath[] ALIGNED(32) = "/shared2/sys/SYSCONF";
static char txtpath[] ALIGNED(32) = "/title/00000001/00000002/data/setting.txt";

config_header *cfg_hdr;

bool tbdec = false;
bool configloaded = false;
bool AutoBootDisc = false;

u32 GameID = 0;
u32 GameMagic = 0;

HacksConfig *PL;

void __Dec_Enc_TB( void ) 
{	
	u32 key = 0x73B5DBFA;
    int i;
    
	for( i=0; i < 0x100; ++i ) 
	{    
		txtbuffer[i] ^= key&0xFF;
		key = (key<<1) | (key>>31);
	}
	
	tbdec = tbdec ? false : true;
}

void __configshifttxt( char *str )
{
	const char *ptr = str;
	char *ctr = str;
	int i;
	
	for( i=0; i < strlen(str); ++i )
	{		
		if( strncmp( str+(i-3), "PALC", 4 ) == 0 )
			*ctr = 0x0d;
		else if( strncmp( str+(i-2), "LUH", 3 ) == 0 )
			*ctr = 0x0d;
		else if( strncmp( str+(i-2), "LUM", 3 ) == 0 )	
			*ctr = 0x0d;
		else 
			*ctr = str[i];

		ctr++;
	}	
	*ctr = *ptr;
	*ctr = '\0';
}

s32 __configread( void )
{
	confbuffer = (u8 *)malloca( 0x4000, 0x20 );
	cfg_hdr = (config_header *)NULL;
	
	s32 fd = IOS_Open( configpath, 1 );
	if(fd < 0 )
		return fd;
		
	s32 ret = IOS_Read( fd, confbuffer, 0x4000 );
	IOS_Close( fd );
	if( ret != 0x4000 )
		return ES_EBADFILE;
		
	fd = IOS_Open( txtpath, 1 );
	if( fd < 0 )
		return fd;
		
	txtbuffer = (char *)malloca( 0x100, 0x20 );
		
	ret = IOS_Read( fd, txtbuffer, 0x100 );
	IOS_Close( fd );
	if( ret != 0x100 )
		return ES_EBADFILE;
		
	cfg_hdr = (config_header *)confbuffer;
		
	__Dec_Enc_TB();
	
	configloaded = configloaded ? false : true;
	
	if( tbdec && configloaded )
		ret = ES_SUCCESS;
	else
		ret = ES_FATAL;	
	
	return ret;	
}

s32 __configwrite( void )
{
	s32 ret = ES_ENOTINIT;
	if( configloaded )
	{
		__Dec_Enc_TB();
		
		if( !tbdec )
		{
			s32 fd = IOS_Open( configpath, 2 );
			if( fd < 0 )
				return fd;
				
			ret = IOS_Write( fd, confbuffer, 0x4000 );
			IOS_Close( fd );
			if( ret != 0x4000 )
				return ES_EBADSAVE;
				
			fd = IOS_Open( txtpath, 2 );
			if( fd < 0 )
				return fd;
				
			ret = IOS_Write( fd, txtbuffer, 0x100 );
			IOS_Close( fd );
			if( ret != 0x100 )
				return ES_EBADSAVE;
			
			free( confbuffer );
			free( txtbuffer );
				
			configloaded = configloaded ? false : true;
			
			if( !tbdec && !configloaded )
				ret = ES_SUCCESS;
			else
				ret = ES_FATAL;
		}
	}
	
	return ret;				
} 

u8 __configgetbyte( char *item )
{
	u32 i;
	for( i=0; i<cfg_hdr->ncnt; ++i )
	{
		if( memcmp( confbuffer+( cfg_hdr->noff[i] + 1), item, strlen( item ) ) == 0 )
		{
			return (u8)( confbuffer[cfg_hdr->noff[i] + 1 + strlen( item ) ] );
			break;
		}
	}
	return 0;
}

u32 __configsetbyte( char *item, u8 val )
{
	u32 i;
	for( i=0; i<cfg_hdr->ncnt; ++i )
	{
		if( memcmp( confbuffer+( cfg_hdr->noff[i] + 1), item, strlen( item ) ) == 0 )
		{
			*(u8*)( confbuffer+cfg_hdr->noff[i] + 1 + strlen( item ) ) = val;
			break;
		}
	}
	return ES_ENOENT;
}

u32	__configgetlong( char * item )
{
	u32 i;
	for( i=0; i<cfg_hdr->ncnt; ++i )
	{
		if( memcmp( confbuffer+( cfg_hdr->noff[i] + 1), item, strlen( item ) ) == 0 )
		{
			return (u32)( confbuffer[cfg_hdr->noff[i] + 1 + strlen( item ) ] );
			break;
		}
	}
	return ES_ENOENT;
}

u32 __configsetbigarray( char *item, void *val, u32 size )
{
	u32 i;
	for( i=0; i<cfg_hdr->ncnt; ++i )
	{
		if( memcmp( confbuffer+( cfg_hdr->noff[i] + 1), item, strlen( item ) ) == 0 )
		{
			memcpy( confbuffer+cfg_hdr->noff[i] + 3 + strlen( item ), val, size);
			break;
		}
	}
	return ES_ENOENT;
}

u32 __configsetlong( char *item, u32 val )
{
	u32 i;
	for( i=0; i<cfg_hdr->ncnt; ++i )
	{
		if( memcmp( confbuffer+( cfg_hdr->noff[i] + 1), item, strlen( item ) ) == 0 )
		{
			*(u32*)( confbuffer+cfg_hdr->noff[i] + 1 + strlen( item ) ) = val;
			break;
		}
	}
	return ES_ENOENT;
}


u32 __configgetsetting( char *item, char *val )
{
	char *curitem = strstr( txtbuffer, item );
	char *curstrt, *curend;
	
	if( curitem == NULL )
		return ES_ENOENT;
	
	curstrt = strchr( curitem, '=' );
	curend = strchr( curitem, 0x0d );
	
	if( curstrt && curend )
	{
		curstrt += 1;
		u32 len = curend - curstrt;
		memcpy( val, curstrt, len );
		val[len] = 0;
		return ES_SUCCESS;
	}
	
	return ES_ENOENT;
}

u32 __configsetsetting( char *item, char *val )
{		
	char *curitem = strstr( txtbuffer, item );
	char *curstrt, *curend;
	
	if( curitem == NULL )
		return ES_ENOENT;
	
	curstrt = strchr( curitem, '=' );
	curend = strchr( curitem, 0x0d );
	
	if( curstrt && curend )
	{
		curstrt += 1;
		u32 len = curend - curstrt;
		if( strlen( val ) > len )
		{
			static char buffer[0x100];
			u32 nlen;
			nlen = txtbuffer-( curstrt+strlen( val ) );
			strcpy( buffer, txtbuffer+nlen );
			strncpy( curstrt, val, strlen( val ) );
			curstrt += strlen( val ); 
			strncpy( curstrt, buffer, strlen( buffer ) );
		}
		else
		{
			strncpy( curstrt, val, strlen( val ) );
		}

		__configshifttxt( txtbuffer );

		return ES_SUCCESS;
	}
	return ES_ENOENT;
}

void DoGameRegion( u64 TitleID )
{
	__configloadcfg();
	
	if( PL->Config&CONFIG_REGION_CHANGE )
	{
		if( __configread() == ES_SUCCESS )
		{
			if( TitleID != 0x0000000100000002LL )
			{	
				switch( TitleID&0xFF )
				{
					case 'J':
					{
						if( PL->NTSCVid == 6 ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.E60", 1 );
						else	
							__configsetbyte( "IPL.E60", 0 );
						if( PL->NTSCVid == 5  ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.PGS", 1 );
						else
							__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", 0 );
						CCode[0] = 1;
						__configsetbigarray( "SADR.LNG", CCode, 0x1007 );
						__configsetsetting( "AREA", "JPN" );
						__configsetsetting( "MODEL", "RVL-001(JPN)" );
						__configsetsetting( "CODE", "LJM" );
						__configsetsetting( "VIDEO", "NTSC" );
						__configsetsetting( "GAME", "JP" );
						__configwrite();				
					} break;
					case 'E':
					{
						if( PL->NTSCVid == 6 ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.E60", 1 );
						else	
							__configsetbyte( "IPL.E60", 0 );
						if( PL->NTSCVid == 5  ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.PGS", 1 );
						else
							__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", PL->USLang );
						if( PL->Shop1 >= 8 &&  PL->Shop1 <= 52 )
							CCode[0] = PL->Shop1;
						else
							CCode[0] = 31;
						__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
						__configsetsetting( "AREA", "USA" );
						__configsetsetting( "MODEL", "RVL-001(USA)" );
						__configsetsetting( "CODE", "LU" );
						__configsetsetting( "VIDEO", "NTSC" );
						__configsetsetting( "GAME", "US" );				
						__configwrite();				
					} break;
					case 'D':
					case 'F':
					case 'I':
					case 'M':
					case 'P':
					case 'S':
					case 'U':					
					{
						if( PL->PALVid == 2 ||  PL->PALVid == 3)
							__configsetbyte( "IPL.E60", 1 );
						else	
							__configsetbyte( "IPL.E60", 0 );
						if( PL->PALVid == 1 ||  PL->PALVid == 3)
							__configsetbyte( "IPL.PGS", 1 );
						else
							__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", PL->EULang );
						if( PL->Shop1 >= 64 && PL->Shop1 <= 121 )
							CCode[0] = PL->Shop1;
						else
							CCode[0] = 110;
						__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
						__configsetsetting( "AREA", "EUR" );
						__configsetsetting( "MODEL", "RVL-001(EUR)" );
						__configsetsetting( "CODE", "LEH" );
						__configsetsetting( "VIDEO", "PAL" );
						__configsetsetting( "GAME", "EU" );
						__configwrite();				
					} break;
					case 'K':					
					{
						if( PL->NTSCVid == 6 ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.E60", 1 );
						else	
							__configsetbyte( "IPL.E60", 0 );
						if( PL->NTSCVid == 5  ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.PGS", 1 );
						else
							__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", 9 );
						CCode[0] = 136;
						__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
						__configsetsetting( "AREA", "KOR" );
						__configsetsetting( "MODEL", "RVL-001(KOR)" );
						__configsetsetting( "CODE", "LKM" );
						//if( PL->NTSCVid == 1 ||  PL->NTSCVid == 2)
						//	__configsetsetting( "VIDEO", "PAL" );
						//else
						__configsetsetting( "VIDEO", "NTSC" );
						__configsetsetting( "GAME", "KR" );
						__configwrite();				
					} break;
				}
			}
		}
	}	
}

void DoSMRegion( u64 TitleID, u16 TitleVersion )
{
	__configloadcfg();
	
	if( TitleID == 0x0001000248414241LL || TitleID == 0x000100024841424BLL )
	{
		if( PL->Shop1 != 0 )
		{
			if( __configread() == ES_SUCCESS )
			{				
				if( PL->Shop1 >= 8 &&  PL->Shop1 <= 52 ) 
				{
					CCode[0] = PL->Shop1;
					strcpy(SCode, "LU");
				}
				else if( PL->Shop1 >= 64 &&  PL->Shop1 <= 121 )
				{
					CCode[0] = PL->Shop1;
					strcpy(SCode, "LEH");
				}
				else if( PL->Shop1 == 136 )
				{
					CCode[0] = PL->Shop1;
					strcpy(SCode, "LKM");
				}
				else
				{
					CCode[0] = PL->Shop1;
					strcpy(SCode, "LJM");
				}
				__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
				__configsetsetting( "CODE", SCode );
				__configwrite();
			}
		}
	}
	
	if( PL->Config&CONFIG_REGION_CHANGE )
	{	
		if( __configread() == ES_SUCCESS )
		{			
			if( TitleID == 0x0000000100000002LL )
			{
				switch( TitleVersion&0xF )
				{
					case AREA_JPN:
					{
						if( PL->NTSCVid == 6 ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.E60", 1 );
						else	
							__configsetbyte( "IPL.E60", 0 );
						if( PL->NTSCVid == 5  ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.PGS", 1 );
						else
							__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", 0 );
						CCode[0] = 1;
						__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
						__configsetsetting( "AREA", "JPN" );
						__configsetsetting( "MODEL", "RVL-001(JPN)" );
						__configsetsetting( "CODE", "LJM" );
						__configsetsetting( "VIDEO", "NTSC" );
						__configsetsetting( "GAME", "JP" );
						__configwrite();				
					} break;
					case AREA_USA:
					{
						if( PL->NTSCVid == 6 ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.E60", 1 );
						else	
							__configsetbyte( "IPL.E60", 0 );
						if( PL->NTSCVid == 5  ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.PGS", 1 );
						else
							__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", PL->USLang );
						if( PL->Shop1 >= 8 &&  PL->Shop1 <= 52 )
							CCode[0] = PL->Shop1;
						else
							CCode[0] = 31;
						__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
						__configsetsetting( "CODE", "LU" );
						__configsetsetting( "AREA", "USA" );
						__configsetsetting( "MODEL", "RVL-001(USA)" );						
						__configsetsetting( "VIDEO", "NTSC" );
						__configsetsetting( "GAME", "US" );				
						__configwrite();				
					} break;				
					case AREA_EUR:
					{	
						if( PL->PALVid == 2 ||  PL->PALVid == 3)
							__configsetbyte( "IPL.E60", 1 );
						else	
							__configsetbyte( "IPL.E60", 0 );
						if( PL->PALVid == 1 ||  PL->PALVid == 3)
							__configsetbyte( "IPL.PGS", 1 );
						else
							__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", PL->EULang );
						if( PL->Shop1 >= 64 &&  PL->Shop1 <= 121 )
							CCode[0] = PL->Shop1;
						else
							CCode[0] = 110;
						__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
						__configsetsetting( "AREA", "EUR" );
						__configsetsetting( "MODEL", "RVL-001(EUR)" );
						__configsetsetting( "CODE", "LEH" );
						__configsetsetting( "VIDEO", "PAL" );
						__configsetsetting( "GAME", "EU" );
						__configwrite();
					} break;
					case AREA_KOR:					
					{	
						if( PL->NTSCVid == 6 ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.E60", 1 );
						else	
							__configsetbyte( "IPL.E60", 0 );
						if( PL->NTSCVid == 5  ||  PL->NTSCVid == 7)
							__configsetbyte( "IPL.PGS", 1 );
						else
							__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", 9 );
						CCode[0] = 136;
						__configsetbigarray( "IPL.SADR", CCode, 0x1007 );
						__configsetsetting( "AREA", "KOR" );
						__configsetsetting( "MODEL", "RVL-001(KOR)" );
						__configsetsetting( "CODE", "LKM" );
						__configsetsetting( "VIDEO", "NTSC" );
						__configsetsetting( "GAME", "KR" );
						__configwrite();				
					} break;
				}
			}
		}
	}	
}

s32 Force_Internet_Test( void )
{
	if( PL->Config&CONFIG_FORCE_INET )
	{
		char *path	= (char *)malloca( 0x80, 32 );
		u32 *size	= (u32*)malloca( sizeof(u32), 32 );
		u8 tmp = 0xa0;	
		strcpy( path, "/shared2/sys/net/02/config.dat" );
		netconfig_t *NETCfg = (netconfig_t *)NANDLoadFile( path, size );	
		if( NETCfg == NULL )
		{
			free( path );
			return *size;
		}
	
		if( NETCfg->connection[0].flags && NETCfg->connection[0].flags < 0xa0 )
		{
			NETCfg->header4 = 0x01;
			NETCfg->header5 = 0x07;
			if( !NETCfg->connection[0].ssid_length )
				tmp += 0x01;
			if( NETCfg->connection[0].dns1[0] == 0 )
				tmp += 0x02;
			if( NETCfg->connection[0].ip[0] == 0 )
				tmp += 0x04;
			if( NETCfg->connection[0].proxy_settings.use_proxy )
				tmp += 0x16;
			
			NETCfg->connection[0].flags = tmp;
			
			NANDWriteFileSafe( path, NETCfg, *size );
		}	
		else if( NETCfg->connection[1].flags && NETCfg->connection[1].flags < 0xa0 )
		{
			NETCfg->header4 = 0x01;
			NETCfg->header5 = 0x07;
			if( !NETCfg->connection[1].ssid_length )
				tmp += 0x01;
			if( NETCfg->connection[1].dns1[0] == 0 )
				tmp += 0x02;
			if( NETCfg->connection[1].ip[0] == 0 )
				tmp += 0x04;
			if( NETCfg->connection[1].proxy_settings.use_proxy )
				tmp += 0x16;
			
			NETCfg->connection[1].flags = tmp;
			
			NANDWriteFileSafe( path, NETCfg, *size );
		}	
		else if( NETCfg->connection[2].flags && NETCfg->connection[2].flags < 0xa0 )
		{
			NETCfg->header4 = 0x01;
			NETCfg->header5 = 0x07;
			if( !NETCfg->connection[2].ssid_length )
				tmp += 0x01;
			if( NETCfg->connection[2].dns1[0] == 0 )
				tmp += 0x02;
			if( NETCfg->connection[2].ip[0] == 0 )
				tmp += 0x04;
			if( NETCfg->connection[2].proxy_settings.use_proxy )
				tmp += 0x16;
			
			NETCfg->connection[2].flags = tmp;
			
			NANDWriteFileSafe( path, NETCfg, *size );
		}	
	
		free( NETCfg );
		free( size );
		free( path );
	
		if( __configread() == ES_SUCCESS )
		{
			__configsetbyte( "IPL.CD2", 1 );
			__configsetbyte( "IPL.EULA", 1 );
			__configwrite();
		}
	}	
	return 0;	
}

void LoadDOLToMEM( char *path )
{
	s32 fd = IOS_Open( path, 1 );
	fstats *status = (fstats*)heap_alloc_aligned( 0, sizeof(fstats), 0x40 );
	ISFS_GetFileStats( fd, status );
	IOS_Read( fd, (void *)0x12000000, status->Size );
	heap_free( 0, status );
	IOS_Close( fd );
}

s32 GetBootConfigFromMem(u64 *TitleID)
{
	memcfg *MC = (memcfg *)0x01200000;

	dbgprintf("ES:Checking magic in memory 0x%08x\n", *(vu32*)0x01200000);
	if(MC->magic != 0x666c6f77)
		return 0;
		
	if(MC->titleid == 0)
		*TitleID = 0x100000002LL;
	else
	{
		*TitleID = MC->titleid;	
		dbgprintf("ES:Title found in memory: %08x-%08x...\n", (u32)(MC->titleid>>32), (u32)MC->titleid);
	}
	
	if(MC->config&NCON_EXT_RETURN_TO)
	{
		dbgprintf("ES:Return to title: %08x-%08x set by external app\n", (u32)(MC->returnto>>32), (u32)MC->returnto );
		u8 *data = (u8*)malloca(0xE0, 0x40);
		memcpy(data, &MC->returnto, sizeof(u64));
		
		NANDWriteFileSafe("/sneek/reload.sys", data , 0xE0);
		free(data);
	}
	
	if(MC->config&NCON_EXT_BOOT_GAME)
	{
		//DVDLoadGame(0x534e4d50, 0x5d1c9ea3);
		//DVDLoadGame(MC->gameid, MC->gamemagic);
		//GameID = MC->gameid;
		//GameMagic = MC->gamemagic;
		//GameID = 0x534e4d50;
		//GameMagic = 0x5d1c9ea3;
		AutoBootDisc = true;
	}
	
	MC->magic = 0x4f4a4f59;
	return 1;
}

void KillEULA()
{
	if( __configread() == ES_SUCCESS )
	{
		__configsetbyte( "IPL.EULA", 1 );
		__configwrite();
	}
}
