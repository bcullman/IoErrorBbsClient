/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client.h"
#include "client_globals.h"
#include "color.h"
#include "defs.h"
#include "filter.h"
#include "filter_globals.h"
#include "utility.h"
static void beginWhoListIfNeeded( unsigned char **ptrPtrWhoEntryWrite, char *ptrNew,
                                  int *ptrFriendColumn, unsigned char *aryWhoEntry );
static char *duplicateWhoNameOrDie( const char *ptrName, const char *ptrErrorText );
static void finishCurrentWhoList( unsigned char **ptrPtrWhoEntryWrite, char *ptrNew );
static void formatElapsedWhoHeader( char *ptrBuffer, size_t bufferSize,
                                    const char *ptrPrefix, long elapsedSeconds );
static void formatWhoTimeText( char *ptrBuffer, size_t bufferSize, long elapsedMinutes );
static void handleCompletedWhoEntry( unsigned char *aryWhoEntry,
                                     unsigned char **ptrPtrWhoEntryWrite,
                                     int *ptrFriendColumn, long *ptrTimestamp,
                                     long *ptrExtendedTime );
static void handleWhoListNull( unsigned char *aryWhoEntry,
                               unsigned char **ptrPtrWhoEntryWrite,
                               char *ptrNew, int *ptrFriendColumn,
                               long *ptrTimestamp, long *ptrExtendedTime );
static void printSavedWhoList( long elapsedSeconds, int *ptrFriendColumn );
static void printSavedWhoSummary( int *ptrFriendColumn, long timestamp );
static void printThemedWhoListEntry( const char *ptrName, char statusMarker,
                                     const char *ptrTimeText, const char *ptrInfo );
static void printThemedWhoListHeader( const char *ptrText );
static void refreshSavedWhoList( void );
static void storeFriendWhoEntry( const unsigned char *aryWhoEntry,
                                 const friend *ptrFriend );
static void updateSavedWhoListWithEntry( const unsigned char *aryWhoEntry );

/// @brief Start a who-list capture if one is not already active.
///
/// @param ptrPtrWhoEntryWrite Current entry write pointer.
/// @param ptrNew Tracks whether any new entries have been seen.
/// @param ptrFriendColumn Tracks the printed friend column count.
/// @param aryWhoEntry Entry buffer to initialize.
///
/// @return This helper does not return a value.
static void beginWhoListIfNeeded( unsigned char **ptrPtrWhoEntryWrite, char *ptrNew,
                                  int *ptrFriendColumn, unsigned char *aryWhoEntry )
{
   if ( *ptrPtrWhoEntryWrite )
   {
      return;
   }

   *ptrPtrWhoEntryWrite = aryWhoEntry;
   *aryWhoEntry = 0;
   *ptrNew = 0;
   *ptrFriendColumn = 0;
}

/// @brief Duplicate a who-list name or abort if memory allocation fails.
///
/// @param ptrName Name to duplicate.
/// @param ptrErrorText Fatal error text to show on allocation failure.
///
/// @return Heap-allocated name copy.
static char *duplicateWhoNameOrDie( const char *ptrName, const char *ptrErrorText )
{
   char *ptrWhoCopy;

   ptrWhoCopy = (char *)calloc( 1, strlen( ptrName ) + 1 );
   if ( !ptrWhoCopy )
   {
      fatalExit( ptrErrorText, "Fatal error" );
      return NULL;
   }
   snprintf( ptrWhoCopy, strlen( ptrName ) + 1, "%s", ptrName );
   return ptrWhoCopy;
}

/// @brief Filter one byte of incoming who-list data.
///
/// @param inputChar Next input byte from the server.
///
/// @return This function does not return a value.
void filterWhoList( register int inputChar )
{
   static char new;
   static int friendColumn;
   static unsigned char aryWhoEntry[21]; // Buffer for current name in aryWhoEntry list
   static unsigned char *ptrWhoEntryWrite = NULL;
   static long timestamp = 0; // Friend list timestamp
   static long extime = 0;    // Extended time decoder

   beginWhoListIfNeeded( &ptrWhoEntryWrite, &new, &friendColumn, aryWhoEntry );

   if ( inputChar )
   {
      new = 1;
      *ptrWhoEntryWrite++ = (unsigned char)inputChar;
      *ptrWhoEntryWrite = 0;
   }
   else
   {
      handleWhoListNull( aryWhoEntry, &ptrWhoEntryWrite, &new,
                         &friendColumn, &timestamp, &extime );
   }
}

/// @brief Finish the current who-list capture.
///
/// @param ptrPtrWhoEntryWrite Entry write pointer to clear.
/// @param ptrNew New-entry flag to reset.
///
/// @return This helper does not return a value.
static void finishCurrentWhoList( unsigned char **ptrPtrWhoEntryWrite, char *ptrNew )
{
   *ptrPtrWhoEntryWrite = NULL;
   *ptrNew = 0;
   whoListProgress = 0;
}

/// @brief Format the saved who-list age header text.
///
/// @param ptrBuffer Destination buffer.
/// @param bufferSize Size of the destination buffer.
/// @param ptrPrefix Header prefix text.
/// @param elapsedSeconds Age of the saved list in seconds.
///
/// @return This helper does not return a value.
static void formatElapsedWhoHeader( char *ptrBuffer, size_t bufferSize,
                                    const char *ptrPrefix, long elapsedSeconds )
{
   snprintf( ptrBuffer, bufferSize, "%s (%d:%02d old)%s",
             ptrPrefix, (int)( elapsedSeconds / 60 ),
             (int)( elapsedSeconds % 60 ),
             strstr( ptrPrefix, "Your friends online" ) ? "\r\n\n" : "" );
}

/// @brief Format the displayed who-list elapsed time text for one entry.
///
/// @param ptrBuffer Destination buffer.
/// @param bufferSize Size of the destination buffer.
/// @param elapsedMinutes Elapsed time in minutes.
///
/// @return This helper does not return a value.
static void formatWhoTimeText( char *ptrBuffer, size_t bufferSize, long elapsedMinutes )
{
   if ( elapsedMinutes >= 1440 )
   {
      snprintf( ptrBuffer, bufferSize, "%2ldd%02ld:%02ld",
                elapsedMinutes / 1440,
                ( elapsedMinutes % 1440 ) / 60,
                ( elapsedMinutes % 1440 ) % 60 );
   }
   else
   {
      snprintf( ptrBuffer, bufferSize, "   %2ld:%02ld",
                elapsedMinutes / 60,
                elapsedMinutes % 60 );
   }
}

/// @brief Finish one completed who-list entry and update saved state.
///
/// @param aryWhoEntry Completed encoded who-list entry.
/// @param ptrPtrWhoEntryWrite Entry write pointer to reset.
/// @param ptrFriendColumn Printed friend column count.
/// @param ptrTimestamp Timestamp for the current who list.
/// @param ptrExtendedTime Extended time accumulator.
///
/// @return This helper does not return a value.
static void handleCompletedWhoEntry( unsigned char *aryWhoEntry,
                                     unsigned char **ptrPtrWhoEntryWrite,
                                     int *ptrFriendColumn, long *ptrTimestamp,
                                     long *ptrExtendedTime )
{
   char aryTempText[80];

   *ptrPtrWhoEntryWrite = aryWhoEntry;
   if ( whoListProgress++ == 1 )
   {
      refreshSavedWhoList();
   }

   if ( *aryWhoEntry == 0xfe )
   {
      unsigned char *ptrEncodedTime;

      for ( ptrEncodedTime = aryWhoEntry + 1; *ptrEncodedTime; ptrEncodedTime++ )
      {
         *ptrExtendedTime = 10 * *ptrExtendedTime + *ptrEncodedTime - 1;
      }
      return;
   }

   snprintf( aryTempText, sizeof( aryTempText ), "%s", (char *)aryWhoEntry + 1 );
   *aryTempText &= 0x7f;
   updateSavedWhoListWithEntry( aryWhoEntry );
   *ptrTimestamp = time( NULL );

   {
      int friendIndex;

      friendIndex = slistFind( friendList, aryTempText, fStrCompareVoid );
      if ( friendIndex != -1 )
      {
         const friend *ptrFriend;

         ptrFriend = friendList->items[friendIndex];
         if ( !( *ptrFriendColumn )++ )
         {
            stdPrintf( "Your friends online (new)\r\n\n" );
         }
         --*aryWhoEntry;
         if ( *ptrFriendColumn <= 60 )
         {
            char aryTimeText[16];

            if ( *ptrExtendedTime == 0 )
            {
               *ptrExtendedTime = (long)( *aryWhoEntry );
            }
            formatWhoTimeText( aryTimeText, sizeof( aryTimeText ),
                               *ptrExtendedTime );
            printThemedWhoListEntry( aryTempText,
                                     aryWhoEntry[1] & 0x80 ? '*' : ' ',
                                     aryTimeText, ptrFriend->info );
            storeFriendWhoEntry( aryWhoEntry, ptrFriend );
         }
      }
   }

   *ptrExtendedTime = 0;
}

/// @brief Handle a NUL terminator while processing who-list data.
///
/// @param aryWhoEntry Current encoded who-list entry buffer.
/// @param ptrPtrWhoEntryWrite Current entry write pointer.
/// @param ptrNew New-entry flag.
/// @param ptrFriendColumn Printed friend column count.
/// @param ptrTimestamp Timestamp for the current who list.
/// @param ptrExtendedTime Extended time accumulator.
///
/// @return This helper does not return a value.
static void handleWhoListNull( unsigned char *aryWhoEntry,
                               unsigned char **ptrPtrWhoEntryWrite,
                               char *ptrNew, int *ptrFriendColumn,
                               long *ptrTimestamp, long *ptrExtendedTime )
{
   if ( aryWhoEntry != *ptrPtrWhoEntryWrite )
   {
      handleCompletedWhoEntry( aryWhoEntry, ptrPtrWhoEntryWrite,
                               ptrFriendColumn, ptrTimestamp, ptrExtendedTime );
      return;
   }

   if ( *ptrNew == 1 )
   {
      if ( !savedWhoCount )
      {
         stdPrintf( "No friends online (new)" );
      }
      finishCurrentWhoList( ptrPtrWhoEntryWrite, ptrNew );
      return;
   }

   printSavedWhoSummary( ptrFriendColumn, *ptrTimestamp );
   whoListProgress = 0;
   *ptrPtrWhoEntryWrite = NULL;
}

/// @brief Print the saved who list with refreshed elapsed times.
///
/// @param elapsedSeconds Age of the saved list in seconds.
/// @param ptrFriendColumn Printed friend column count.
///
/// @return This helper does not return a value.
static void printSavedWhoList( long elapsedSeconds, int *ptrFriendColumn )
{
   char aryTempText[80];

   for ( ; ( *ptrFriendColumn )++ < savedWhoCount; )
   {
      char aryTimeText[16];

      snprintf( aryTempText, sizeof( aryTempText ), "%s",
                (char *)arySavedWhoNames[*ptrFriendColumn - 1] + 1 );
      snprintf( aryTimeText, sizeof( aryTimeText ), "   %2d:%02d",
                ( *arySavedWhoNames[*ptrFriendColumn - 1] +
                  (int)( elapsedSeconds / 60 ) ) /
                   60,
                ( *arySavedWhoNames[*ptrFriendColumn - 1] +
                  (int)( elapsedSeconds / 60 ) ) %
                   60 );
      printThemedWhoListEntry( aryTempText + 1,
                               *aryTempText & 0x80 ? '*' : ' ',
                               aryTimeText,
                               (char *)arySavedWhoInfo[*ptrFriendColumn - 1] );
   }
   ( *ptrFriendColumn )--;
}

/// @brief Print the saved who-list summary header.
///
/// @param ptrFriendColumn Printed friend column count.
/// @param timestamp Timestamp of the saved who list.
///
/// @return This helper does not return a value.
static void printSavedWhoSummary( int *ptrFriendColumn, long timestamp )
{
   static long timer = 0;
   long elapsedSeconds;
   char aryTempText[80];

   elapsedSeconds = time( NULL ) - timestamp;
   if ( elapsedSeconds - 66 == timer )
   {
      timer = -1;
   }
   else
   {
      timer = elapsedSeconds;
   }
   if ( savedWhoCount )
   {
      if ( timer == -1 )
      {
         printThemedWhoListHeader( "Die die die fornicate (666)\r\n\n" );
      }
      else
      {
         formatElapsedWhoHeader( aryTempText, sizeof( aryTempText ),
                                 "Your friends online", elapsedSeconds );
         printThemedWhoListHeader( aryTempText );
      }
      printSavedWhoList( elapsedSeconds, ptrFriendColumn );
      return;
   }

   if ( timer == -1 )
   {
      printThemedWhoListHeader( "Die die die (666)" );
   }
   else
   {
      formatElapsedWhoHeader( aryTempText, sizeof( aryTempText ),
                              "No friends online", elapsedSeconds );
      printThemedWhoListHeader( aryTempText );
   }
}

/// @brief Print one who-list entry using the active theme.
///
/// @param ptrName Display name to print.
/// @param statusMarker Status marker character.
/// @param ptrTimeText Formatted time text.
/// @param ptrInfo Associated info text.
///
/// @return This helper does not return a value.
static void printThemedWhoListEntry( const char *ptrName, char statusMarker,
                                     const char *ptrTimeText, const char *ptrInfo )
{
   if ( flagsConfiguration.shouldUseAnsi )
   {
      char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.postFriendName );
      stdPrintf( "%s", aryAnsiSequence );
      stdPrintf( "%-19s%c ", ptrName, statusMarker );
      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.postFriendDate );
      stdPrintf( "%s", aryAnsiSequence );
      stdPrintf( "%s", ptrTimeText );
      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.postFriendText );
      stdPrintf( "%s", aryAnsiSequence );
      stdPrintf( "  %s\r\n", ptrInfo );
      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.text );
      stdPrintf( "%s", aryAnsiSequence );
      return;
   }

   stdPrintf( "%-19s%c %s  %s\r\n", ptrName, statusMarker, ptrTimeText, ptrInfo );
}

/// @brief Print a who-list header using the active theme.
///
/// @param ptrText Header text to print.
///
/// @return This helper does not return a value.
static void printThemedWhoListHeader( const char *ptrText )
{
   if ( flagsConfiguration.shouldUseAnsi )
   {
      char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.forum );
      stdPrintf( "%s", aryAnsiSequence );
      stdPrintf( "%s", ptrText );
      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.text );
      stdPrintf( "%s", aryAnsiSequence );
      return;
   }

   stdPrintf( "%s", ptrText );
}

/// @brief Rebuild the saved who list from the configured friend list.
///
/// @return This helper does not return a value.
static void refreshSavedWhoList( void )
{
   unsigned int ui;

   savedWhoCount = 0;
   slistDestroyItems( whoList );
   slistDestroy( whoList );
   if ( !( whoList = slistCreate( 0, sortCompareVoid ) ) )
   {
      fatalExit( "Can't re-create saved aryWhoEntry list!\r\n", "Fatal error" );
   }
   for ( ui = 0; ui < friendList->nitems; ui++ )
   {
      const friend *ptrFriend;
      char *ptrWhoCopy;

      ptrFriend = friendList->items[ui];
      ptrWhoCopy = duplicateWhoNameOrDie( ptrFriend->name,
                                          "Out of memory for list copy!\r\n" );
      if ( !( slistAddItem( whoList, ptrWhoCopy, 0 ) ) )
      {
         fatalExit( "Out of memory adding item in list copy!\r\n", "Fatal error" );
      }
   }
}

/// @brief Store a newly seen friend entry into the saved who-list cache.
///
/// @param aryWhoEntry Encoded who-list entry.
/// @param ptrFriend Friend record supplying the info text.
///
/// @return This helper does not return a value.
static void storeFriendWhoEntry( const unsigned char *aryWhoEntry,
                                 const friend *ptrFriend )
{
   *arySavedWhoNames[savedWhoCount] = *aryWhoEntry;
   snprintf( (char *)arySavedWhoNames[savedWhoCount] + 1,
             sizeof( arySavedWhoNames[savedWhoCount] ) - 1,
             "%s", (char *)aryWhoEntry + 1 );
   snprintf( (char *)arySavedWhoInfo[savedWhoCount],
             sizeof( arySavedWhoInfo[savedWhoCount] ),
             "%s", ptrFriend->info );
   savedWhoCount++;
}

/// @brief Add a who-list entry to the saved-name cache if it is new.
///
/// @param aryWhoEntry Encoded who-list entry.
///
/// @return This helper does not return a value.
static void updateSavedWhoListWithEntry( const unsigned char *aryWhoEntry )
{
   char aryTempText[80];
   char *ptrWhoCopy;

   snprintf( aryTempText, sizeof( aryTempText ), "%s", (char *)aryWhoEntry + 1 );
   *aryTempText &= 0x7f;
   ptrWhoCopy = duplicateWhoNameOrDie( aryTempText,
                                       "Out of memory adding to saved aryWhoEntry list!\r\n" );
   if ( slistFind( whoList, ptrWhoCopy, strCompareVoid ) == -1 )
   {
      if ( !( slistAddItem( whoList, ptrWhoCopy, 0 ) ) )
      {
         fatalExit( "Can't add item to saved aryWhoEntry list!\r\n", "Fatal error" );
      }
   }
}
