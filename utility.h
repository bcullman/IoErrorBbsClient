/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UTILITY_H_INCLUDED
#define UTILITY_H_INCLUDED

#include "defs.h"

typedef enum
{
   COLOR_EDITOR_ACTION_CANCEL = 0,
   COLOR_EDITOR_ACTION_DECREASE_FAST,
   COLOR_EDITOR_ACTION_DECREASE_SMALL,
   COLOR_EDITOR_ACTION_INCREASE_FAST,
   COLOR_EDITOR_ACTION_INCREASE_SMALL,
   COLOR_EDITOR_ACTION_MOVE_LEFT,
   COLOR_EDITOR_ACTION_MOVE_RIGHT,
   COLOR_EDITOR_ACTION_NEXT_FIELD,
   COLOR_EDITOR_ACTION_PREVIOUS_FIELD,
   COLOR_EDITOR_ACTION_RESET_FIELD,
   COLOR_EDITOR_ACTION_SAVE
} ColorEditorAction;

int capPrintf( const char *format, ... );
int capPutChar( int inputChar );
int capPuts( const char *ptrText );
int deleteFile( const char *pathname );
int deleteQueue( queue *ptrQueue );
char *duplicateString( const char *ptrSource );
char *extractName( const char *header );
char *extractNameNoHistory( const char *header );
int extractNumber( const char *header );
char *findChar( const char *ptrString, int targetChar );
char *findSubstring( const char *ptrString, const char *ptrSubstring );
int fSortCompare( const friend *const *ptrLeft, const friend *const *ptrRight );
int fSortCompareVoid( const void *ptrLeft, const void *ptrRight );
int fStrCompare( const char *ptrName, const friend *ptrFriend );
int fStrCompareVoid( const void *ptrName, const void *ptrFriend );
int getKey( void );
void handleInvalidInput( unsigned int *ptrInvalidCount );
int inKey( void );
int isQueued( const char *ptrObject, queue *ptrQueue );
void looper( void );
int more( int *line, int percentComplete );
int netPrintf( const char *format, ... );
int netPutChar( int inputChar );
int netPuts( const char *ptrText );
queue *newQueue( int size, int itemCount );
int popQueue( char *ptrObject, queue *ptrQueue );
int pushQueue( const char *ptrObject, queue *ptrQueue );
int readFoldedKey( void );
int readNormalizedLine( FILE *ptrFileHandle, char *ptrLine, size_t lineSize,
                        int *ptrLineNumber, int *ptrReadCount,
                        const char *ptrSourceName );
ColorEditorAction readColorEditorAction( void );
int readValidatedKey( const char *allowedChars );
int readValidatedMenuKey( const char *allowedCharsLowercase );
void replyMessage( void );
int safeDeleteQueue( queue *ptrQueue );
void sendAnX( void );
void sendTrackedBuffer( const char *ptrBuffer, size_t length );
void sendTrackedChar( int inputChar );
void sendTrackedCharWithoutReplay( int inputChar );
void sendTrackedNewline( void );
int slistAddItem( slist *list, void *item, int deferSort );
slist *slistCreate( int nitems, int ( *sortfn )( const void *, const void * ), ... );
void slistDestroy( slist *list );
void slistDestroyItems( slist *list );
int slistFind( slist *list, void *toFind,
               int ( *findfn )( const void *, const void * ) );
slist *slistIntersection( const slist *list1, const slist *list2 );
int slistRemoveItem( slist *list, int item );
void slistSort( slist *list );
int sortCompare( char **ptrLeft, char **ptrRight );
int sortCompareVoid( const void *ptrLeft, const void *ptrRight );
void sPerror( const char *message, const char *heading );
int stdPrintf( const char *format, ... );
int stdPutChar( int inputChar );
int stdPuts( const char *ptrText );
int strCompareVoid( const void *ptrLeft, const void *ptrRight );
char *stripAnsi( char *ptrText, size_t bufferSize );
void tempFileError( void );
void trackReplayableSentChar( int inputChar );
void trackUnreplayableSentChar( int inputChar );
void trimTrailingWhitespace( char *ptrLine );
int yesNo( void );
int yesNoDefault( int defaultAnswer );

#endif // UTILITY_H_INCLUDED
