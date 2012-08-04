/***************************************************************************
 * Copyright (C) 2012 by OverjoY
 *           
 * Rewritten code from libogc
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * EXI handling functions for neek2o
 *
 * exi.h
 *
 ***************************************************************************/

#ifndef _EXI_H_
#define _EXI_H_

#include "global.h"

#define EXI_READ 0
#define EXI_WRITE 1

void EXI_Sync(int chan);
s32 EXI_Imm(int chan, void *buffer, int length, int mode, int cb);
s32 EXI_ImmEx(int chan, void *buffer, int length, int mode);
s32 EXI_Select(int chan, int dev, int freq);
s32 EXI_Deselect(int chan);

#endif