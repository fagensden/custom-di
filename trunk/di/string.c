/*
 *  linux/lib/string.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include "string.h"

size_t strnlen(const char *s, size_t count)
{
	const char *sc;

	for (sc = s; count-- && *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

size_t strlen(const char *s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

char * strstr ( const char *str1, const char *str2)
{
	char *cp = (char *) str1;
	char *s1, *s2;

	if ( !*str2 )
		return((char *)str1);

	while (*cp)
	{
		s1 = cp;
		s2 = (char *) str2;
		while ( *s1 && *s2 && !(*s1-*s2) )
			s1++, s2++;

		if (!*s2)
			return(cp);
		cp++;

	}

	return(NULL);
}
char *strncpy(char *dst, const char *src, size_t n)
{
	char *ret = dst;

	while (n && (*dst++ = *src++))
		n--;

	while (n--)
		*dst++ = 0;

	return ret;
}

char *strcpy(char *dst, const char *src)
{
	char *ret = dst;

	while ((*dst++ = *src++))
		;

	return ret;
}

int strcmp(const char *p, const char *q)
{
	for (;;) {
		unsigned char a, b;
		a = *p++;
		b = *q++;
		if (a == 0 || a != b)
			return a - b;
	}
}

int strcmpi(const char *p, const char *q)
{
	for (;;) {
		unsigned char a, b;
		a = *p++;
		b = *q++;
		if (a >= 'A' && a <= 'Z')
			a += 32;
		if (b >= 'A' && b <= 'Z')
			b += 32;
		if (a == 0 || a != b)
			return a - b;
	}
}

int strncmp(const char *p, const char *q, size_t n)
{
	while (n-- != 0) {
		unsigned char a, b;
		a = *p++;
		b = *q++;
		if (a == 0 || a != b)
			return a - b;
	}
	return 0;
}

int strncmpi(const char *p, const char *q, size_t n)
{
	while (n-- != 0) {
		unsigned char a, b;
		a = *p++;
		b = *q++;
		if (a >= 'A' && a <= 'Z')
			a += 32;
		if (b >= 'A' && b <= 'Z')
			b += 32;
		if (a == 0 || a != b)
			return a - b;
	}
	return 0;
}

void *memset(void *dst, int x, size_t n)
{
	unsigned char *p;

	for (p = dst; n; n--)
		*p++ = x;

	return dst;
}

void *memcpy32(void *dest, const void *src, size_t count)
{
	u32 *tmp = (u32*)(dest);
	u32 *s = (u32*)(src);

	while (count--)
		*tmp++ = *s++;
	return (void*)(dest);
}

//void *memcpy(void *dst, const void *src, size_t n)
//{
//	unsigned char *p;
//	const unsigned char *q;
//
//	for (p = dst, q = src; n; n--)
//		*p++ = *q++;
//
//	return dst;
//}

int memcmp(const void *s1, const void *s2, size_t n)
{
	unsigned char *us1 = (unsigned char *) s1;
	unsigned char *us2 = (unsigned char *) s2;
	while (n-- != 0) {
		if (*us1 != *us2)
			return (*us1 < *us2) ? -1 : +1;
		us1++;
		us2++;
	}
	return 0;
}

char *strchr(const char *s, int c)
{
	do {
		if(*s == c)
			return (char *)s;
	} while(*s++ != 0);
	return NULL;
}

char* skipPastArticles(char* s){
	if (strncmpi(s,"the ",4) == 0)
		return &s[4];
	if (strncmpi(s,"a ",4) == 0)
		return &s[2];
	return s;
}

void Asciify( char *str )
{
	int i=0;
	for( i=0; i<strlen( str ); i++ )
		if( str[i] < 0x20 || str[i] > 0x7F )
			str[i] = '_';
}

void Asciify2( char *str )
{
	const char *ptr = str;
	char *ctr = str;
	int i;
	
	for( i=0; i < strlen(str); ++i )
	{
		switch( str[i] )
		{
			case 0xc3:
			case 0x8c:
			case 0xe2:
			case 0x27:
				ctr--;
				break;
			case 0x87:
				*ctr = 0x80; 
				break;
			case 0xa7:
				*ctr = 0x87; 
				break;
			case 0xa0:
				*ctr = 0x85; 
				break;
			case 0xa2:
				*ctr = 0x83; 
				break;
			case 0x80:
				*ctr = 0x41; 
				break;
			case 0x82:
				*ctr = 0x41; 
				break;
			case 0xaa:
				*ctr = 0x88; 
				break;
			case 0xa8:
				*ctr = 0x8a; 
				break;
			case 0xa9:
				*ctr = 0x65;  
				break;	
			case 0x89:
				*ctr = 0x90; 
				break;
			case 0x88:
				*ctr = 0x45; 
				break;
			case 0xc5:
				*ctr = 0x4f; 
				break;
			case 0xb1:
				*ctr = 0xe4; 
				break;
			case 0x9f:
				*ctr = 0xe1; 
				break;
			case 0xab:
				*ctr = 0xeb;
				break;
			default:
				*ctr = str[i];
				break;				
		}
		ctr++;
	}
	
	*ctr = *ptr;
	*ctr = '\0';
}

void upperCase( char *str )
{
	int i;
	for( i=0; i<strlen( str ); ++i )
		if( str[i] >= 'a' && str[i] <= 'z' )
			str[i] -= 0x20;
}