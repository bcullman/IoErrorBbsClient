/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CLIENT_H_INCLUDED
#define CLIENT_H_INCLUDED

#include "defs.h"
#include <stdnoreturn.h>
void arguments( int argc, char **argv );
RETSIGTYPE bye( int signalNumber );
void connectBbs( void );
void copyright( void );
void deinitialize( void );
noreturn void fatalExit( const char *message, const char *heading );
noreturn void fatalPerror( const char *error, const char *heading );
void feedPager( int startrow, ... );
void findHome( void );
void flushInput( unsigned int invalid );
int getWindowSize( void );
void information( void );
void initialize( void );
void license( void );
noreturn void myExit( void );
void mySleep( unsigned int sec );
RETSIGTYPE naws( int signalNumber );
void noTitleBar( void );
void openTmpFile( void );
RETSIGTYPE reapChild( int signalNumber );
void resetTerm( void );
void run( const char *aryCommand, const char *arg );
void sError( const char *message, const char *heading );
void setTerm( void );
void sigInit( void );
void sigOff( void );
void sInfo( const char *info, const char *heading );
int sPrompt( const char *info, const char *question, int def );
void suspend( void );
void techInfo( void );
void telInit( void );
void titleBar( void );
int waitNextEvent( void );
void warranty( void );

#endif // CLIENT_H_INCLUDED
