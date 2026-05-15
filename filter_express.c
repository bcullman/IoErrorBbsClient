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
#include "utility.h"
typedef struct
{
   unsigned int crlf : 1;      // Needs initial CR/LF
   unsigned int prochdr : 1;   // Needs killfile processing
   unsigned int ignore : 1;    // Ignore the remainder of the X
   unsigned int truncated : 1; // X message exceeded buffer
} ExpressFilterFlags;

static void beginExpressMessage( ExpressFilterFlags *ptrFlags );
static void finishExpressMessage( const ExpressFilterFlags *ptrFlags,
                                  const char *ptrSenderName );
static void processExpressHeader( ExpressFilterFlags *ptrFlags, char **ptrSenderName );

/// @brief Reset express-message parsing state at the start of a new message.
///
/// @param ptrFlags Express parsing state to initialize.
///
/// @return This helper does not return a value.
static void beginExpressMessage( ExpressFilterFlags *ptrFlags )
{
   beginUrlDetectionReport();
   ptrExpressMessageBuffer = aryExpressMessageBuffer;
   *aryExpressMessageBuffer = 0;
   ptrFlags->ignore = 0;
   ptrFlags->crlf = 0;
   ptrFlags->prochdr = 1;
   ptrFlags->truncated = 0;
}

/// @brief Filter one byte of incoming express-message output.
///
/// @param inputChar Next input byte from the server, or `-1` for state transitions.
///
/// @return This function does not return a value.
void filterExpress( register int inputChar )
{
   static char *ptrSenderName; // comparison pointer
   static ExpressFilterFlags needs;

   if ( inputChar == -1 )
   { // Signal from IAC to begin or end an X message.
      if ( isExpressMessageInProgress )
      { // Reinitialize for a new X message.
         beginExpressMessage( &needs );
         return;
      }
      else if ( !needs.ignore )
      { // Finish the X message and emit it.
         finishExpressMessage( &needs, ptrSenderName );
         return;
      }
      emitUrlDetectionReport();
   }
   // Stop when the message has already been killed.
   if ( needs.ignore )
   {
      return;
   }
   if ( needs.truncated )
   {
      return;
   }

   if ( aryExpressMessageBuffer == ptrExpressMessageBuffer )
   { // Check for initial CR/LF pair
      if ( inputChar == '\r' || inputChar == '\n' )
      {
         needs.crlf = 1;
         return;
      }
   }
   // Insert character into the buffer (drop excess to avoid overflow)
   if ( ptrExpressMessageBuffer >= aryExpressMessageBuffer + sizeof( aryExpressMessageBuffer ) - 1 )
   {
      needs.truncated = 1;
      return;
   }
   *ptrExpressMessageBuffer++ = (char)inputChar;
   *ptrExpressMessageBuffer = 0;

   // Extract URLs if any
   if ( !needs.prochdr && inputChar == '\r' )
   {
      filterUrl( ptrExpressMessageBuffer );
   }

   // If reached a \r it's time to do header processing
   if ( needs.prochdr && inputChar == '\r' )
   {
      needs.prochdr = 0;
      processExpressHeader( &needs, &ptrSenderName );
   }
}

/// @brief Finish an express message and emit it with any needed reply handling.
///
/// @param ptrFlags Final express parsing state.
/// @param ptrSenderName Sender name extracted from the express header.
///
/// @return This helper does not return a value.
static void finishExpressMessage( const ExpressFilterFlags *ptrFlags,
                                  const char *ptrSenderName )
{
   int itemIndex;
   int isAutoReply;

   itemIndex = extractNumber( aryExpressMessageBuffer );
   isAutoReply = isAutomaticReply( aryExpressMessageBuffer );

   if ( ( isAway || isXland ) && ptrSenderName &&
        !isQueued( ptrSenderName, xlandQueue ) &&
        itemIndex > highestExpressMessageId && !isAutoReply )
   {
      pushQueue( ptrSenderName, xlandQueue );
      shouldSendExpressMessage = true;
   }
   else if ( isAutoReply && itemIndex > highestExpressMessageId )
   {
      notReplyingTransformExpress( aryExpressMessageBuffer, sizeof( aryExpressMessageBuffer ) );
   }

   if ( itemIndex > highestExpressMessageId )
   {
      highestExpressMessageId = itemIndex;
   }

   replyCodeTransformExpress( aryExpressMessageBuffer, sizeof( aryExpressMessageBuffer ) );
   ansiTransformExpress( aryExpressMessageBuffer, sizeof( aryExpressMessageBuffer ) );
   if ( ptrFlags->crlf )
   {
      stdPrintf( "\r\n" );
   }
   printWithOsc8Links( aryExpressMessageBuffer );
   if ( ptrFlags->truncated )
   {
      stdPrintf( "\r\n[X message truncated]\r\n" );
   }
   emitUrlDetectionReport();
}

/// @brief Check whether an express message is an automatic reply.
///
/// @param message Express message text to inspect.
///
/// @return Non-zero if the message is an automatic reply, otherwise `0`.
int isAutomaticReply( const char *message )
{
   const char *ptrCursor;

   // Find first aryLine
   ptrCursor = findSubstring( message, ">" );

   // Wasn't a first aryLine? - move past '>'
   if ( !ptrCursor )
   {
      return false;
   }
   ptrCursor++;

   // Check for valid automatic reply messages
   if ( !strncmp( ptrCursor, "+!R", 3 ) ||
        !strncmp( ptrCursor, "This message was automatically generated", 40 ) ||
        !strncmp( ptrCursor, "*** ISCA Windows Client", 23 ) )
   {
      return true;
   }

   // Treat the message as a normal express message.
   return false;
}

/// @brief Mark an automatic reply message so it will not trigger a reply.
///
/// @param ptrText Express message text to rewrite in place.
/// @param size Size of the destination buffer.
///
/// @return This function does not return a value.
void notReplyingTransformExpress( char *ptrText, size_t size )
{
   char aryTempText[580];
   char *ptrMessageStart;

   // Verify that this is an X message and set up the rewrite pointers.
   ptrMessageStart = findSubstring( ptrText, " at " );
   if ( !ptrMessageStart )
   {
      return;
   }

   *( ptrMessageStart++ ) = 0;

   snprintf( aryTempText, sizeof( aryTempText ), "%s (not replying) %s", ptrText, ptrMessageStart );
   snprintf( ptrText, size, "%s", aryTempText );
}

/// @brief Process the express header and check whether the sender should be ignored.
///
/// @param ptrFlags Express parsing state to update.
/// @param ptrSenderName Receives the extracted sender name.
///
/// @return This helper does not return a value.
static void processExpressHeader( ExpressFilterFlags *ptrFlags, char **ptrSenderName )
{
   *ptrSenderName = extractNameNoHistory( aryExpressMessageBuffer );
   if ( *ptrSenderName &&
        slistFind( enemyList, *ptrSenderName, strCompareVoid ) != -1 )
   {
      if ( !flagsConfiguration.shouldSquelchExpress )
      {
         stdPrintf( "\r\n[X message by %s killed]\r\n", *ptrSenderName );
      }
      ptrFlags->ignore = 1;
      return;
   }
   if ( *ptrSenderName )
   {
      *ptrSenderName = extractName( aryExpressMessageBuffer );
   }
}

/// @brief Remove the internal automatic-reply prefix from an express message.
///
/// @param ptrText Express message text to rewrite in place.
/// @param size Size of the destination buffer.
///
/// @return This function does not return a value.
void replyCodeTransformExpress( char *ptrText, size_t size )
{
   char aryTempText[580];
   char *ptrMessageStart;

   // Verify that this is an automatic reply and set up the rewrite pointers.
   ptrMessageStart = findSubstring( ptrText, ">" );
   if ( !ptrMessageStart || strncmp( ptrMessageStart, ">+!R ", 5 ) )
   {
      return;
   }

   *( ++ptrMessageStart ) = 0;
   ptrMessageStart += 4;

   snprintf( aryTempText, sizeof( aryTempText ), "%s%s", ptrText, ptrMessageStart );
   snprintf( ptrText, size, "%s", aryTempText );
}
