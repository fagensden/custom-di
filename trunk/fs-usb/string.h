#ifndef _STRING_H_
#define _STRING_H_

#include "global.h"

char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
int strcmp(const char *, const char *);
int strncmp(const char *p, const char *q, size_t n);
int strnccmp(const char *p, const char *q, size_t n);
size_t strlen(const char *);
size_t strnlen(const char *, size_t);
char *strchr(const char *s, int c);
void *memset(void *, int, size_t);

extern void memcpy(void *dst, void *src, u32 size);

int memcmp(const void *s1, const void *s2, size_t n);
char *strcat( char *str1, const char *str2 );

#endif
