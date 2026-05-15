/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "browser.h"
#include "client_globals.h"
#include "color.h"
#include "defs.h"
#include "filter.h"
#include "filter_globals.h"
#include "network_globals.h"
#include "sysio.h"
#include "telnet.h"
#include "utility.h"
typedef struct
{
   unsigned int crlf : 1;    // need to add a CR/LF
   unsigned int prochdr : 1; // process post header
   unsigned int ignore : 1;  // kill this post
   unsigned int secondN : 1; // send a second n
} PostFilterFlags;

static void appendFilterLineChar( int inputChar );
static void beginPostMessage( PostFilterFlags *ptrFlags, char *posthdr,
                              char **ptrPostHeaderCursor, int *ptrIsFriend );
static void finishPostMessage( const PostFilterFlags *ptrFlags, int *ptrPostCount );
static void maybePrintMorePromptColor( int inputChar );
static bool tryKillPost( PostFilterFlags *ptrFlags, const char *ptrSenderName,
                         const char *ptrHeader, int postCount );

/// @brief Append one character to the current filtered line buffer.
///
/// @param inputChar Character to append.
///
/// @return This helper does not return a value.
static void appendFilterLineChar( int inputChar )
{
   char *ptrCursor = aryFilterLine;

   for ( ; *ptrCursor; ptrCursor++ )
   {
      ;
   }

   *ptrCursor++ = (char)inputChar;
   *ptrCursor = 0;
}

/// @brief Reset post parsing state at the start of a new post.
///
/// @param ptrFlags Post parsing state to initialize.
/// @param posthdr Buffer used for the post header.
/// @param ptrPostHeaderCursor Receives the current header write position.
/// @param ptrIsFriend Receives the initial friend flag.
///
/// @return This helper does not return a value.
static void beginPostMessage( PostFilterFlags *ptrFlags, char *posthdr,
                              char **ptrPostHeaderCursor, int *ptrIsFriend )
{
   beginUrlDetectionReport();
   *ptrPostHeaderCursor = posthdr;
   *posthdr = 0;
   ptrFlags->crlf = 0;
   ptrFlags->prochdr = 1;
   *ptrIsFriend = 0;
   ptrFlags->ignore = 0;
   ptrFlags->secondN = 0;
}

/// @brief Re-emit the standard continued-post color sequence through the post filter.
///
/// @return This function does not return a value.
void continuedPostHelper( void )
{
   static char aryTempText[] = "\033[32m";
   char *ptrText;

   for ( ptrText = aryTempText; *ptrText; ptrText++ )
   {
      filterPost( *ptrText );
   }
}

/// @brief Filter one byte of incoming post output.
///
/// @param inputChar Next input byte from the server, or `-1` for state transitions.
///
/// @return This function does not return a value.
void filterPost( register int inputChar )
{
   static char aryTempText[160];
   static int isFriend; // Current post is by a friend
   static PostFilterFlags needs;
   static int numposts = 0;  // count of the # of posts received so far
   static char posthdr[140]; // store the post header here
   static char *posthdrp;    // pointer into posthdr

   if ( inputChar == -1 )
   { // control: begin/end of post
      if ( postProgressState )
      { // beginning of post
         beginPostMessage( &needs, posthdr, &posthdrp, &isFriend );
      }
      else
      { // end of post
         finishPostMessage( &needs, &numposts );
      }
      return;
   }
   // If post is killed, do no further processing.
   if ( needs.ignore )
   { // Ignore killed enemy posts.
      return;
   }

   if ( posthdr == posthdrp )
   { // Check for initial CR/LF pair
      if ( inputChar == '\r' || inputChar == '\n' )
      {
         needs.crlf = 1;
         return;
      }
   }
   if ( needs.prochdr && posthdrp == NULL )
   {
      posthdrp = posthdr;
      *posthdr = 0;
   }
   // Insert the character into the post buffer, or echo it directly.
   if ( !needs.prochdr )
   {
      static char aryAnsiSequence[8];
      static size_t ansiSequenceLength = 0;

      // Store this line for processing
      if ( inputChar == '\n' )
      {
         filterUrl( aryFilterLine );
         aryFilterLine[0] = 0;
      }
      else
      {
         appendFilterLineChar( inputChar );
      }

      // Process ANSI codes in the middle of a post
      if ( ansiSequenceLength > 0 )
      {
         if ( ansiSequenceLength < sizeof( aryAnsiSequence ) )
         {
            aryAnsiSequence[ansiSequenceLength++] = (char)inputChar;
         }
         if ( inputChar == 'm' || ansiSequenceLength >= sizeof( aryAnsiSequence ) )
         {
            emitTransformedAnsiSequence( aryAnsiSequence, ansiSequenceLength, 1, isFriend );
            ansiSequenceLength = 0;
         }
         return;
      }
      if ( inputChar == '\033' )
      { // Escape character
         ansiSequenceLength = 0;
         aryAnsiSequence[ansiSequenceLength++] = (char)inputChar;
         if ( !flagsConfiguration.shouldUseAnsi )
         {
            flagsConfiguration.shouldUseAnsi = 1;
            if ( !flagsConfiguration.shouldUseBold )
            {
               flagsConfiguration.shouldDisableBold = 1;
            }
         }
         return;
      }
      // Change color for end of more prompt
      maybePrintMorePromptColor( inputChar );

      // Output character
      stdPutChar( inputChar );
   }
   else
   {
      *posthdrp++ = (char)inputChar; // store character in buffer
      *posthdrp = 0;

      // If reached a \r it's time to do header processing
      if ( inputChar == '\r' )
      {
         const char *ptrSenderNameForChecks;

         needs.prochdr = 0;

         // Process for enemy list kill file
         char *ptrSenderName;

         snprintf( aryTempText, sizeof( aryTempText ), "%s", posthdr );
         ptrSenderNameForChecks = extractNameNoHistory( aryTempText );
         if ( tryKillPost( &needs, ptrSenderNameForChecks, posthdr, numposts ) )
         {
            return;
         }

         ptrSenderName = extractName( aryTempText );
         isFriend = ( ptrSenderName &&
                      slistFind( friendList, ptrSenderName, fStrCompareVoid ) != -1 )
                       ? 1
                       : 0;
         ansiTransformPostHeader( posthdr, sizeof( posthdr ), isFriend );
         snprintf( arySavedHeader, sizeof( arySavedHeader ), "%s\r\n", posthdr );
         if ( needs.crlf )
         {
            stdPrintf( "\r\n" );
         }
         printWithOsc8Links( posthdr );
         stdPrintf( "\r" );
      }
   }
}

/// @brief Finish a post and flush any delayed state updates.
///
/// @param ptrFlags Final post parsing state.
/// @param ptrPostCount Running count of posts seen so far.
///
/// @return This helper does not return a value.
static void finishPostMessage( const PostFilterFlags *ptrFlags, int *ptrPostCount )
{
   if ( ptrFlags->secondN )
   {
      netPutChar( 'n' );
      byte++;
   }
   filterUrl( " " );
   ( *ptrPostCount )++;
   emitUrlDetectionReport();
}

/// @brief Restore the normal text color after a more prompt inside a post.
///
/// @param inputChar Current input character.
///
/// @return This helper does not return a value.
static void maybePrintMorePromptColor( int inputChar )
{
   if ( !( flagsConfiguration.shouldUseAnsi &&
           flagsConfiguration.isMorePromptActive && inputChar == ' ' ) )
   {
      return;
   }

   {
      char aryMorePromptSequence[ANSI_SEQUENCE_BUFFER_SIZE];

      lastColor = color.text;
      formatAnsiForegroundSequence( aryMorePromptSequence,
                                    sizeof( aryMorePromptSequence ),
                                    lastColor );
      stdPrintf( "%s", aryMorePromptSequence );
   }
}

/// @brief Check whether the current post should be killed by the enemy list.
///
/// @param ptrFlags Post parsing state to update.
/// @param ptrSenderName Sender name extracted from the header.
/// @param ptrHeader Raw post header text.
/// @param postCount Current post count used for protocol bookkeeping.
///
/// @return `true` if the post was killed, otherwise `false`.
static bool tryKillPost( PostFilterFlags *ptrFlags, const char *ptrSenderName,
                         const char *ptrHeader, int postCount )
{
   if ( !ptrSenderName ||
        slistFind( enemyList, (void *)ptrSenderName, strCompareVoid ) == -1 )
   {
      return false;
   }

   ptrFlags->ignore = 1;
   postHeaderActive = -1;
   postProgressState = -1;
   netPrintf( "%c%c%c%c", IAC, POST_K, postCount & 0xFF, 17 );
   netflush();
   if ( !flagsConfiguration.shouldSquelchPost )
   {
      stdPrintf( "%s[Post by %s killed]\r\n",
                 *ptrHeader == '\n' ? "\r\n" : "", ptrSenderName );
   }
   else
   {
      pendingLinesToEat = ( ptrFlags->crlf ? 1 : 2 );
      netPutChar( 'n' );
      netflush();
      byte++;
   }
   return true;
}
