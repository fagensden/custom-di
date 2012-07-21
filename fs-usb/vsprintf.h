#ifndef __VSPRINTF_H__
#define __GLOBAL_H__

#include <stdarg.h>
#include "syscalls.h"
#include "gecko.h"
#include "string.h"

void hexdump(void *d, int len);
int vsprintf(char *buf, const char *fmt, va_list args);
int _sprintf( char *buf, const char *fmt, ... );
int __sprintf( char *buf, const char *fmt, ... );
int dbgprintf( const char *fmt, ...);

#endif