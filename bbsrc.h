/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BBSRC_H_INCLUDED
#define BBSRC_H_INCLUDED

#include "defs.h"

FILE *findBbsRc( void );
FILE *openBbsRc( void );

void readBbsRc( void );
void truncateBbsRc( long userNameLength );
void writeBbsRc( void );

#endif // BBSRC_H_INCLUDED
