/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PANE_UI_TEST_HELPERS_H
#define PANE_UI_TEST_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

void feedIncomingText( const char *ptrText, bool expectedHandled );
void feedMouseInput( const char *ptrText );
void formatLocalTimestamp( char *ptrBuffer, size_t bufferSize, time_t timestamp );
void readNetOutput( char *ptrBuffer, size_t bufferSize );
void readOutput( char *ptrBuffer, size_t bufferSize );
void resetOutput( void );
void setTerminalSize( int columns );
int setup( void **state );
int teardown( void **state );

#endif
