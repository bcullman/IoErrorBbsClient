/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PANE_UI_H_INCLUDED
#define PANE_UI_H_INCLUDED

#include "defs.h"

bool paneUiHandleIncomingChar( int inputChar );
bool paneUiHandleLocalInput( int inputChar, bool hasMoreLocalInput );
bool paneUiHasPendingLocalInput( void );
bool paneUiIsActive( void );
bool paneUiSelectTimeout( struct timeval *ptrTimeout );
bool paneUiTakePendingLocalInput( int *ptrInputChar );
size_t paneUiTerminalContentColumns( void );
void paneUiAfterOutputChar( int outputChar );
void paneUiAfterOutputText( const char *ptrText );
void paneUiEnterIfEligible( void );
void paneUiHandleTimer( void );
void paneUiHandleTimerAt( time_t now );
void paneUiLeave( void );
void paneUiLeaveForExit( void );
void paneUiMarkResizePending( void );
void paneUiNoteUserInput( void );
void paneUiResetSession( void );

#endif // PANE_UI_H_INCLUDED
