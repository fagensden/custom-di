/**************************************************************************************
***																					***
*** neek2o - sysconf.h																***
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

#ifndef __SYSCONF_H__
#define __SYSCONF_H__

#include "ES.h"
#include "global.h"
#include "string.h"
#include "common.h"

enum Region
{
	NTSCU = 1,
	NTSCJ,
	NTSCK,
	PAL,
};

typedef struct _config_header 
{
	u8		magic[4];
	u16		ncnt;
	u16		noff[];
} config_header;

typedef struct _proxy
{
    u8 use_proxy;
    u8 use_proxy_userandpass;
    u8 padding_1[2];
    u8 proxy_name[255];
    u8 padding_2;
    u16 proxy_port;
    u8 proxy_username[32];
    u8 padding_3;         
    u8 proxy_password[32];
} __attribute__((__packed__)) proxy_t; 

typedef struct _connection
{
    u8 flags;
    u8 padding_1[3]; 
    u8 ip[4];
    u8 netmask[4];
    u8 gateway[4];
    u8 dns1[4];
    u8 dns2[4];
    u8 padding_2[2]; 
    u16 mtu;
    u8 padding_3[8]; 
    proxy_t proxy_settings;
    u8 padding_4; 
    proxy_t proxy_settings_copy;
    u8 padding_5[1297];
    u8 ssid[32]; 
    u8 padding_6;
    u8 ssid_length;
    u8 padding_7[2]; 
    u8 padding_8;
    u8 encryption;
    u8 padding_9[2]; 
    u8 padding_10;
    u8 key_length;
    u8 unknown;
    u8 padding_11; 
    u8 key[64]; 
    u8 padding_12[236];
} connection_t;

typedef struct _netconfig
{
    u8 header0;
    u8 header1;
    u8 header2;
    u8 header3;
    u8 header4;
    u8 header5;
    u8 header6;
    u8 header7; 
    connection_t connection[3];
} netconfig_t;

typedef struct _memcfg
{
	u32 magic;
	u64 titleid;
	u32 config;
	u64 returnto;
	u32 gameid;
	u32 gamemagic;
	char dipath[256];
	char nandpath[256];
} memcfg;

enum ExtNANDCfg
{
	NCON_EXT_DI_PATH		= (1<<0),
	NCON_EXT_NAND_PATH		= (1<<1),
	NCON_HIDE_EXT_PATH		= (1<<2),
	NCON_EXT_RETURN_TO		= (1<<3),
	NCON_EXT_BOOT_GAME		= (1<<4),
};

void DoGameRegion( u64 TitleID );
void DoSMRegion( u64 TitleID, u16 TitleVersion );
s32 Force_Internet_Test( void );
void LoadDOLToMEM( char *path );
s32 GetBootConfigFromMem(u64 *TitleID);
void KillEULA();

#endif