/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file detects URLs in filtered BBS output and renders clickable URL
 * output for terminals that support OSC 8 hyperlinks.
 */
#include "browser.h"
#include "client_globals.h"
#include "color.h"
#include "defs.h"
#include "filter_globals.h"
#include "utility.h"

static void applyVisibleUrlReportColor( int foregroundColor );
static void beginVisibleUrlReportBodyColor( void );
static void beginVisibleUrlReportHeaderColor( void );
static void clearDetectedUrlQueue( void );
static void endVisibleUrlReportColor( void );
static bool ensureDetectedUrlQueue( void );
static void finalizePendingUrl( char *aryPendingUrl, size_t pendingUrlSize, bool *ptrHasPendingUrl );
static char *findUrlStart( char *ptrText );
static const char *findUrlStartConst( const char *ptrText );
static bool isUrlBodyChar( int inputChar );
static bool isUrlTerminator( int inputChar );
static void queueUrlForReport( const char *ptrUrl );
static void queueUrlIfNew( const char *ptrUrl );
static bool shouldEmitClickableUrls( void );
static void trimUrlTailPunctuation( char *ptrUrlStart );

static queue *ptrDetectedUrlQueue;

/// @brief Apply the themed color used while printing a visible URL report.
///
/// @param foregroundColor Foreground color value to emit.
///
/// @return This helper does not return a value.
static void applyVisibleUrlReportColor( int foregroundColor )
{
   if ( !flagsConfiguration.shouldUseAnsi )
   {
      return;
   }
   printAnsiDisplayStateValue( foregroundColor, color.background );
   lastColor = foregroundColor;
}

/// @brief Start a fresh clickable URL detection report.
///
/// @return This function does not return a value.
void beginUrlDetectionReport( void )
{
   if ( !ensureDetectedUrlQueue() )
   {
      return;
   }
   clearDetectedUrlQueue();
}

/// @brief Switch output to the body color for URL report entries.
///
/// @return This helper does not return a value.
static void beginVisibleUrlReportBodyColor( void )
{
   applyVisibleUrlReportColor( color.text );
}

/// @brief Switch output to the header color for URL reports.
///
/// @return This helper does not return a value.
static void beginVisibleUrlReportHeaderColor( void )
{
   applyVisibleUrlReportColor( color.number );
}

/// @brief Empty the queued list of detected URLs.
///
/// @return This helper does not return a value.
static void clearDetectedUrlQueue( void )
{
   char aryTempText[1024];

   if ( ptrDetectedUrlQueue == NULL )
   {
      return;
   }
   while ( ptrDetectedUrlQueue->itemCount > 0 )
   {
      popQueue( aryTempText, ptrDetectedUrlQueue );
   }
}

/// @brief Emit the queued clickable URL report.
///
/// @return This function does not return a value.
void emitUrlDetectionReport( void )
{
   char aryUrl[1024];

   if ( !shouldEmitClickableUrls() )
   {
      clearDetectedUrlQueue();
      return;
   }
   if ( ptrDetectedUrlQueue == NULL || ptrDetectedUrlQueue->itemCount == 0 )
   {
      return;
   }

   beginVisibleUrlReportHeaderColor();
   stdPrintf( "\r\n[Clickable URL(s) detected by BBS client]\r\n" );
   beginVisibleUrlReportBodyColor();
   while ( popQueue( aryUrl, ptrDetectedUrlQueue ) )
   {
      stdPrintf( " " );
      printWithOsc8Links( aryUrl );
      stdPrintf( "\r\n" );
   }
   endVisibleUrlReportColor();
}

/// @brief Restore normal themed output after a URL report finishes.
///
/// @return This helper does not return a value.
static void endVisibleUrlReportColor( void )
{
   applyVisibleUrlReportColor( color.text );
}

/// @brief Ensure the detected URL queue exists.
///
/// @return `true` if the queue is available, otherwise `false`.
static bool ensureDetectedUrlQueue( void )
{
   if ( ptrDetectedUrlQueue != NULL )
   {
      return true;
   }
   ptrDetectedUrlQueue = newQueue( 1024, 64 );
   return ptrDetectedUrlQueue != NULL;
}

/// @brief Finalize a pending wrapped URL fragment and queue it if valid.
///
/// @param aryPendingUrl Buffer holding the pending URL.
/// @param pendingUrlSize Size of the pending URL buffer.
/// @param ptrHasPendingUrl Tracks whether a pending URL is active.
///
/// @return This helper does not return a value.
static void finalizePendingUrl( char *aryPendingUrl, size_t pendingUrlSize, bool *ptrHasPendingUrl )
{
   (void)pendingUrlSize;
   trimUrlTailPunctuation( aryPendingUrl );
   if ( *aryPendingUrl )
   {
      queueUrlIfNew( aryPendingUrl );
   }
   aryPendingUrl[0] = '\0';
   *ptrHasPendingUrl = false;
}

/// @brief Scan one rendered line for URLs and queue newly detected links.
///
/// Wrapped URLs are stitched back together best-effort before they are queued.
///
/// @param ptrLine Line text to inspect.
///
/// @return This function does not return a value.
void filterUrl( const char *ptrLine )
{
   static bool hasPendingUrl = false;
   static char aryPendingUrl[2048];
   static const size_t WRAP_GUESS_MIN_LENGTH = 70;
   char aryLineBuffer[1024];
   char *ptrCursor;
   char *ptrNext;

   if ( !urlQueue )
   {
      return;
   }
   if ( ptrLine == NULL )
   {
      return;
   }

   snprintf( aryLineBuffer, sizeof( aryLineBuffer ), "%s", ptrLine );
   {
      size_t lineLength;

      lineLength = strlen( aryLineBuffer );
      while ( lineLength > 0 &&
              ( aryLineBuffer[lineLength - 1] == ' ' || aryLineBuffer[lineLength - 1] == '\t' ||
                aryLineBuffer[lineLength - 1] == '\r' ) )
      {
         aryLineBuffer[lineLength - 1] = '\0';
         lineLength--;
      }
   }

   ptrCursor = aryLineBuffer;
   if ( hasPendingUrl )
   {
      size_t pendingLength;
      size_t appendedCount;

      while ( *ptrCursor != '\0' && isspace( (unsigned char)*ptrCursor ) )
      {
         ptrCursor++;
      }
      pendingLength = strlen( aryPendingUrl );
      appendedCount = 0;
      while ( *ptrCursor != '\0' && isUrlBodyChar( (unsigned char)*ptrCursor ) )
      {
         if ( pendingLength + 1 < sizeof( aryPendingUrl ) )
         {
            aryPendingUrl[pendingLength++] = *ptrCursor;
            aryPendingUrl[pendingLength] = '\0';
         }
         appendedCount++;
         ptrCursor++;
      }
      if ( appendedCount > 0 )
      {
         if ( *ptrCursor == '\0' )
         {
            size_t continuationVisibleLength;

            continuationVisibleLength = strlen( ptrCursor - appendedCount );
            if ( continuationVisibleLength < WRAP_GUESS_MIN_LENGTH )
            {
               finalizePendingUrl( aryPendingUrl, sizeof( aryPendingUrl ), &hasPendingUrl );
            }
         }
         else
         {
            finalizePendingUrl( aryPendingUrl, sizeof( aryPendingUrl ), &hasPendingUrl );
         }
      }
      else
      {
         finalizePendingUrl( aryPendingUrl, sizeof( aryPendingUrl ), &hasPendingUrl );
      }
   }

   for ( ptrCursor = findUrlStart( aryLineBuffer ); ptrCursor != NULL; ptrCursor = findUrlStart( ptrNext ) )
   {
      for ( ptrNext = ptrCursor; *ptrNext; ptrNext++ )
      {
         if ( !isUrlTerminator( *ptrNext ) && isUrlBodyChar( (unsigned char)*ptrNext ) )
         {
            continue;
         }
         break;
      }

      if ( *ptrNext == '\0' )
      {
         size_t fragmentLength;

         fragmentLength = strlen( ptrCursor );
         if ( fragmentLength >= WRAP_GUESS_MIN_LENGTH )
         {
            snprintf( aryPendingUrl, sizeof( aryPendingUrl ), "%s", ptrCursor );
            hasPendingUrl = true;
         }
         else
         {
            trimUrlTailPunctuation( ptrCursor );
            queueUrlIfNew( ptrCursor );
         }
         return;
      }

      *ptrNext = '\0';
      trimUrlTailPunctuation( ptrCursor );
      queueUrlIfNew( ptrCursor );

      ptrNext++;
   }
}

/// @brief Find the first URL start sequence in mutable text.
///
/// @param ptrText Text to scan.
///
/// @return Pointer to the first detected URL start, or `NULL` if none is found.
static char *findUrlStart( char *ptrText )
{
   char *ptrEarliest;
   char *ptrHttps;
   char *ptrWww;

   ptrHttps = strstr( ptrText, "https://" );
   ptrWww = strstr( ptrText, "www." );

   ptrEarliest = ptrHttps;
   if ( ptrWww != NULL && ( ptrEarliest == NULL || ptrWww < ptrEarliest ) )
   {
      ptrEarliest = ptrWww;
   }

   if ( ptrWww != NULL && ptrEarliest == ptrWww )
   {
      if ( ptrWww != ptrText && !isspace( (unsigned char)ptrWww[-1] ) && ptrWww[-1] != '(' &&
           ptrWww[-1] != '<' && ptrWww[-1] != '"' && ptrWww[-1] != '\'' )
      {
         ptrEarliest = NULL;
      }
   }

   return ptrEarliest;
}

/// @brief Find the first URL start sequence in read-only text.
///
/// @param ptrText Text to scan.
///
/// @return Pointer to the first detected URL start, or `NULL` if none is found.
static const char *findUrlStartConst( const char *ptrText )
{
   const char *ptrEarliest;
   const char *ptrHttps;
   const char *ptrWww;

   ptrHttps = strstr( ptrText, "https://" );
   ptrWww = strstr( ptrText, "www." );

   ptrEarliest = ptrHttps;
   if ( ptrWww != NULL && ( ptrEarliest == NULL || ptrWww < ptrEarliest ) )
   {
      ptrEarliest = ptrWww;
   }

   if ( ptrWww != NULL && ptrEarliest == ptrWww )
   {
      if ( ptrWww != ptrText && !isspace( (unsigned char)ptrWww[-1] ) && ptrWww[-1] != '(' &&
           ptrWww[-1] != '<' && ptrWww[-1] != '"' && ptrWww[-1] != '\'' )
      {
         ptrEarliest = NULL;
      }
   }

   return ptrEarliest;
}

/// @brief Check whether a character is valid inside a detected URL body.
///
/// @param inputChar Character to classify.
///
/// @return `true` if the character can remain inside the URL, otherwise `false`.
static bool isUrlBodyChar( int inputChar )
{
   static const char *ptrAllowedPunctuation = "-._~:/?#[]@!$&'()*+,;=%";

   if ( isalnum( inputChar ) )
   {
      return true;
   }
   return findChar( ptrAllowedPunctuation, inputChar ) != NULL;
}

/// @brief Check whether a character terminates a detected URL.
///
/// @param inputChar Character to classify.
///
/// @return `true` if the character ends the URL, otherwise `false`.
static bool isUrlTerminator( int inputChar )
{
   if ( inputChar == 0 || isspace( inputChar ) )
   {
      return true;
   }
   return false;
}

/// @brief Print text while wrapping detected URLs in OSC 8 hyperlink escapes.
///
/// @param ptrText Text to print.
///
/// @return This function does not return a value.
void printWithOsc8Links( const char *ptrText )
{
   const char *ptrCursor;

   if ( ptrText == NULL )
   {
      return;
   }
   if ( !shouldEmitClickableUrls() )
   {
      stdPrintf( "%s", ptrText );
      return;
   }

   ptrCursor = ptrText;
   while ( *ptrCursor != '\0' )
   {
      const char *ptrEndPunctuation;
      const char *ptrTrimEnd;
      const char *ptrUrlEnd;
      const char *ptrUrlStart;
      char aryUrlTarget[1152];
      char aryUrlText[1024];
      size_t urlLength;

      ptrUrlStart = findUrlStartConst( ptrCursor );
      if ( ptrUrlStart == NULL )
      {
         stdPrintf( "%s", ptrCursor );
         return;
      }

      if ( ptrUrlStart > ptrCursor )
      {
         stdPrintf( "%.*s", (int)( ptrUrlStart - ptrCursor ), ptrCursor );
      }

      for ( ptrUrlEnd = ptrUrlStart; *ptrUrlEnd != '\0'; ptrUrlEnd++ )
      {
         if ( isUrlTerminator( (unsigned char)*ptrUrlEnd ) ||
              !isUrlBodyChar( (unsigned char)*ptrUrlEnd ) )
         {
            break;
         }
      }

      ptrTrimEnd = ptrUrlEnd;
      while ( ptrTrimEnd > ptrUrlStart )
      {
         char tailCharacter;

         tailCharacter = ptrTrimEnd[-1];
         if ( tailCharacter == '.' || tailCharacter == ',' || tailCharacter == ';' ||
              tailCharacter == ':' || tailCharacter == '!' || tailCharacter == '?' ||
              tailCharacter == ')' || tailCharacter == ']' || tailCharacter == '}' ||
              tailCharacter == '>' || tailCharacter == '"' || tailCharacter == '\'' )
         {
            ptrTrimEnd--;
         }
         else
         {
            break;
         }
      }

      if ( ptrTrimEnd == ptrUrlStart )
      {
         stdPrintf( "%.*s", (int)( ptrUrlEnd - ptrUrlStart ), ptrUrlStart );
         ptrCursor = ptrUrlEnd;
         continue;
      }

      urlLength = (size_t)( ptrTrimEnd - ptrUrlStart );
      if ( urlLength >= sizeof( aryUrlText ) )
      {
         urlLength = sizeof( aryUrlText ) - 1;
      }
      memcpy( aryUrlText, ptrUrlStart, urlLength );
      aryUrlText[urlLength] = '\0';

      if ( strncmp( aryUrlText, "www.", 4 ) == 0 )
      {
         snprintf( aryUrlTarget, sizeof( aryUrlTarget ), "https://%s", aryUrlText );
      }
      else
      {
         snprintf( aryUrlTarget, sizeof( aryUrlTarget ), "%s", aryUrlText );
      }

      stdPrintf( "\033]8;;%s\033\\", aryUrlTarget );
      stdPrintf( "%s", aryUrlText );
      stdPrintf( "\033]8;;\033\\" );

      ptrEndPunctuation = ptrTrimEnd;
      if ( ptrUrlEnd > ptrEndPunctuation )
      {
         stdPrintf( "%.*s", (int)( ptrUrlEnd - ptrEndPunctuation ), ptrEndPunctuation );
      }

      ptrCursor = ptrUrlEnd;
   }
}

/// @brief Queue a detected URL for the visible report output.
///
/// @param ptrUrl URL text to queue.
///
/// @return This helper does not return a value.
static void queueUrlForReport( const char *ptrUrl )
{
   char aryTempText[1024];

   if ( ptrUrl == NULL || !*ptrUrl || !ensureDetectedUrlQueue() )
   {
      return;
   }
   if ( isQueued( ptrUrl, ptrDetectedUrlQueue ) )
   {
      return;
   }
   while ( !pushQueue( ptrUrl, ptrDetectedUrlQueue ) )
   {
      popQueue( aryTempText, ptrDetectedUrlQueue );
   }
}

/// @brief Queue a detected URL only if it is not already present.
///
/// @param ptrUrl URL text to queue.
///
/// @return This helper does not return a value.
static void queueUrlIfNew( const char *ptrUrl )
{
   char aryTempText[1024];

   if ( ptrUrl == NULL || !*ptrUrl )
   {
      return;
   }
   queueUrlForReport( ptrUrl );
   if ( isQueued( ptrUrl, urlQueue ) )
   {
      return;
   }
   while ( !pushQueue( ptrUrl, urlQueue ) )
   {
      popQueue( aryTempText, urlQueue );
   }
}

/// @brief Check whether clickable URL reporting should be shown.
///
/// @return `true` if clickable URL reporting is enabled, otherwise `false`.
static bool shouldEmitClickableUrls( void )
{
   if ( flagsConfiguration.isScreenReaderModeEnabled )
   {
      return false;
   }

   return flagsConfiguration.shouldEnableClickableUrls;
}

/// @brief Trim trailing punctuation that should not remain part of a URL.
///
/// @param ptrUrlStart URL text to trim in place.
///
/// @return This helper does not return a value.
static void trimUrlTailPunctuation( char *ptrUrlStart )
{
   size_t urlLength;

   urlLength = strlen( ptrUrlStart );
   while ( urlLength > 0 )
   {
      char tailCharacter;

      tailCharacter = ptrUrlStart[urlLength - 1];
      if ( tailCharacter == '.' || tailCharacter == ',' || tailCharacter == ';' ||
           tailCharacter == ':' || tailCharacter == '!' || tailCharacter == '?' ||
           tailCharacter == ')' || tailCharacter == ']' || tailCharacter == '}' ||
           tailCharacter == '>' || tailCharacter == '"' || tailCharacter == '\'' )
      {
         ptrUrlStart[urlLength - 1] = '\0';
         urlLength--;
      }
      else
      {
         break;
      }
   }
}
