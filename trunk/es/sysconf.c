/**************************************************************************************
***																					***
*** neek2o - sysconf.c																***
***																					***
*** Copyright (C) 2011	OverjoY														***
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
char *txtbuffer;
static char configpath[] ALIGNED(32) = "/shared2/sys/SYSCONF";
static char txtpath[] ALIGNED(32) = "/title/00000001/00000002/data/setting.txt";

config_header *cfg_hdr;

bool tbdec = false;
bool configloaded = false;

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
	{
		hexdump( txtbuffer, 0x20 );
		ret = ES_SUCCESS;
	}
	else
	{
		ret = ES_FATAL;	
	}
	
	return ret;	
}

s32 __configwrite( void )
{
	s32 ret = ES_ENOTINIT;
	hexdump( txtbuffer, 0x20 );
	if( configloaded )
	{
		__Dec_Enc_TB();
		hexdump( txtbuffer, 0x20 );
		
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
						if( PL->Config&CONFIG_FORCE_EuRGB60 )
							__configsetbyte( "IPL.E60", 1 );
						else	
							__configsetbyte( "IPL.E60", 0 );
						__configsetbyte( "IPL.PGS", 1 );
						__configsetbyte( "IPL.LNG", 0 );				
						__configsetsetting( "AREA", "JPN" );
						__configsetsetting( "MODEL", "RVL-001(JPN)" );
						__configsetsetting( "CODE", "LJM" );
						__configsetsetting( "VIDEO", "NTSC" );
						__configsetsetting( "GAME", "JP" );
						__configwrite();				
					} break;
					case 'E':
					{
						if( PL->Config&CONFIG_FORCE_EuRGB60 )
							__configsetbyte( "IPL.E60", 1 );
						else
							__configsetbyte( "IPL.E60", 0 );
						__configsetbyte( "IPL.PGS", 1 );
						__configsetbyte( "IPL.LNG", PL->USLang );
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
						__configsetbyte( "IPL.E60", 1 );
						__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", PL->EULang );
						__configsetsetting( "AREA", "EUR" );
						__configsetsetting( "MODEL", "RVL-001(EUR)" );
						__configsetsetting( "CODE", "LEH" );
						__configsetsetting( "VIDEO", "PAL" );
						__configsetsetting( "GAME", "EU" );
						__configwrite();				
					} break;
					case 'K':					
					{
						if( PL->Config&CONFIG_FORCE_EuRGB60 )
							__configsetbyte( "IPL.E60", 1 );
						else
							__configsetbyte( "IPL.E60", 0 );
						__configsetbyte( "IPL.PGS", 1 );
						__configsetbyte( "IPL.LNG", 9 );
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

void DoSMRegion( u64 TitleID, u16 TitleVersion )
{
	__configloadcfg();
	
	if( PL->Config&CONFIG_REGION_CHANGE )
	{	
		if( __configread() == ES_SUCCESS )
		{
			if( TitleID == 0x0000000100000002LL )
			{
				dbgprintf( "Asked region: %d\n", TitleVersion&0xF );
				switch( TitleVersion&0xF )
				{
					case AREA_JPN:
					{
						__configsetbyte( "IPL.E60", 0 );
						__configsetbyte( "IPL.PGS", 1 );
						__configsetbyte( "IPL.LNG", 0 );				
						__configsetsetting( "AREA", "JPN" );
						__configsetsetting( "MODEL", "RVL-001(JPN)" );
						__configsetsetting( "CODE", "LJM" );
						__configsetsetting( "VIDEO", "NTSC" );
						__configsetsetting( "GAME", "JP" );
						__configwrite();				
					} break;
					case AREA_USA:
					{
						__configsetbyte( "IPL.E60", 0 );
						__configsetbyte( "IPL.PGS", 1 );
						__configsetbyte( "IPL.LNG", PL->USLang );				
						__configsetsetting( "AREA", "USA" );
						__configsetsetting( "MODEL", "RVL-001(USA)" );
						__configsetsetting( "CODE", "LU" );
						__configsetsetting( "VIDEO", "NTSC" );
						__configsetsetting( "GAME", "US" );				
						__configwrite();				
					} break;				
					case AREA_EUR:
					{	
						__configsetbyte( "IPL.E60", 1 );
						__configsetbyte( "IPL.PGS", 0 );
						__configsetbyte( "IPL.LNG", PL->EULang );				
						__configsetsetting( "AREA", "EUR" );
						__configsetsetting( "MODEL", "RVL-001(EUR)" );
						__configsetsetting( "CODE", "LEH" );
						__configsetsetting( "VIDEO", "PAL" );
						__configsetsetting( "GAME", "EU" );
						__configwrite();
					} break;
					case AREA_KOR:					
					{	
						__configsetbyte( "IPL.E60", 0 );
						__configsetbyte( "IPL.PGS", 1 );
						__configsetbyte( "IPL.LNG", 9 );
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
