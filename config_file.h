/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONFIG_FILE_H_INCLUDED
#define CONFIG_FILE_H_INCLUDED

#include "defs.h"

FILE *findConfigFile( void );
FILE *openConfigFile( void );

void readConfig( void );
void truncateConfigFile( long userNameLength );
void writeConfig( void );

#endif // CONFIG_FILE_H_INCLUDED
