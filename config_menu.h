/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONFIG_MENU_H_INCLUDED
#define CONFIG_MENU_H_INCLUDED

#include "defs.h"

void configClient( void );
void configureHotkeys( void );
void configureOptionsMenu( void );
void defaultNameAutocompleteIfUnset( void );
void editUsers( slist *list, int ( *findfn )( const void *, const void * ),
                const char *name );
void expressConfig( void );
void newAwayMessage( void );
int newKey( int oldkey );
void promptForScreenReaderModeIfUnset( void );
void setup( int newVersion );
char *strCtrl( int inputChar );

#endif // CONFIG_MENU_H_INCLUDED
