/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * System I/O routines.
 */
#include "client.h"
#include "client_globals.h"
#include "color.h"
#include "config_globals.h"
#include "defs.h"
#include "network_globals.h"
#include "pane_ui.h"
#include <stdarg.h>
#include "sysio.h"
#include "utility.h"
static bool shouldFlushImmediately( const char *ptrText );

/// @brief Print formatted text to the capture file when capture mode is active.
///
/// @param format `printf`-style format string.
/// @param ... Format arguments.
///
/// @return `1` after formatting and capture output handling.
int capPrintf( const char *format, ... )
{
   char aryString[BUFSIZ];
   va_list ap;

   if ( capture )
   {
      va_start( ap, format );
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
      (void)vsnprintf( aryString, sizeof( aryString ), format, ap );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
      va_end( ap );
      return capPuts( aryString );
   }
   return 1;
}

/// @brief Write one character to the capture file while honoring ANSI filtering.
///
/// @param inputChar Character to capture.
///
/// @return The input character value.
int capPutChar( int inputChar )
{
   static int skipansi = 0; // Counter for avoidance of capturing ANSI

   if ( skipansi )
   {
      skipansi--;
      if ( skipansi == 1 )
      {
         if ( flagsConfiguration.shouldDisableBold && inputChar == 109 )
         { // Keep capture/reset state aligned when bold output is suppressed.
            printf( "\033[0m" );
            skipansi--;
         }
         else
         {
            lastColor = colorValueFromLegacyDigit( inputChar );
         }
      }
   }
   else if ( inputChar == '\033' )
   {
      skipansi = 4;
   }
   else if ( capture > 0 && !flagsConfiguration.isPosting &&
             !flagsConfiguration.isMorePromptActive &&
             inputChar != '\r' )
   {
      if ( putc( inputChar, tempFile ) < 0 )
      {
         tempFileError();
      }
   }
   return inputChar;
}

/// @brief Write a string to the capture file after stripping ANSI escapes.
///
/// @param ptrText Text to capture.
///
/// @return `1` on success, or `1` after reporting a capture error.
int capPuts( const char *ptrText )
{
   if ( capture > 0 && !flagsConfiguration.isPosting && !flagsConfiguration.isMorePromptActive )
   {
      char aryBuffer[BUFSIZ];

      snprintf( aryBuffer, sizeof( aryBuffer ), "%s", ptrText );
      stripAnsi( aryBuffer, sizeof( aryBuffer ) );
      if ( fputs( aryBuffer, tempFile ) == EOF )
      {
         tempFileError();
         return 1;
      }
      if ( shouldFlushImmediately( aryBuffer ) )
      {
         fflush( tempFile );
      }
   }
   return 1;
}

/// @brief Print formatted text to the network output stream.
///
/// @param format `printf`-style format string.
/// @param ... Format arguments.
///
/// @return Always returns `1`.
int netPrintf( const char *format, ... )
{
   va_list ap;
   static char work[BUFSIZ];

   va_start( ap, format );
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
   (void)vsnprintf( work, sizeof( work ), format, ap );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
   va_end( ap );
   netPuts( work );
   return 1;
}

/// @brief Write one character to the network output stream.
///
/// @param inputChar Character to send.
///
/// @return Result from the low-level network put routine.
int netPutChar( int inputChar )
{
   return ( netput( inputChar ) );
}

/// @brief Write a string to the network output stream.
///
/// @param ptrText Text to send.
///
/// @return Always returns `1` on success.
int netPuts( const char *ptrText )
{
   size_t textLength;

   textLength = strlen( ptrText );
   if ( textLength == 0 )
   {
      return 1;
   }
   if ( fwrite( ptrText, sizeof( char ), textLength, netOutputFile ) != textLength )
   {
      fatalPerror( "netPuts", "Network error" );
   }

   return 1;
}

char swork[BUFSIZ]; // temp buffer for color stripping

/// @brief Check whether capture or standard output should be flushed immediately.
///
/// @param ptrText Text that was just written.
///
/// @return `true` if an immediate flush should follow, otherwise `false`.
static bool shouldFlushImmediately( const char *ptrText )
{
   return strchr( ptrText, '\n' ) == NULL;
}

/// @brief Print formatted text to standard output and capture output.
///
/// @param format `printf`-style format string.
/// @param ... Format arguments.
///
/// @return Result from `stdPuts()`.
int stdPrintf( const char *format, ... )
{
   // capPrintf cannot be called directly from this path.
   char aryString[BUFSIZ];
   va_list ap;

   va_start( ap, format );
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
   (void)vsnprintf( aryString, sizeof( aryString ), format, ap );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
   va_end( ap );
   return stdPuts( aryString );
}

/// @brief Write one character to standard output and mirror it into capture output.
///
/// @param inputChar Character to print.
///
/// @return The input character value.
int stdPutChar( int inputChar )
{
   if ( paneUiWriteLocalOutputChar( inputChar ) )
   {
      capPutChar( inputChar );
      return inputChar;
   }

   if ( putchar( inputChar ) < 0 )
   {
      fatalPerror( "stdPutChar", "Local error" );
   }
   capPutChar( inputChar );
   paneUiAfterOutputChar( inputChar );
   return inputChar;
}

/// @brief Write a string to standard output without adding a trailing newline.
///
/// @param ptrText Text to print.
///
/// @return Always returns `1` on success.
int stdPuts( const char *ptrText )
{
   if ( paneUiWriteLocalOutputText( ptrText ) )
   {
      capPuts( ptrText );
      return 1;
   }

   if ( fputs( ptrText, stdout ) == EOF )
   {
      fatalPerror( "stdPuts", "Local error" );
   }
   if ( shouldFlushImmediately( ptrText ) )
   {
      fflush( stdout );
   }
   capPuts( ptrText );
   paneUiAfterOutputText( ptrText );
   return 1;
}

/// @brief Remove ANSI escape sequences from a string in place.
///
/// @param ptrText Text buffer to clean.
/// @param bufferSize Size of the destination buffer.
///
/// @return Pointer to the cleaned text buffer.
char *stripAnsi( char *ptrText, size_t bufferSize )
{
   const char *ptrRead;
   char *ptrWrite;
   ptrWrite = swork;
   for ( ptrRead = ptrText; *ptrRead != '\0'; ptrRead++ )
   {
      if ( *ptrRead != '\033' )
      {
         *ptrWrite++ = *ptrRead;
      }
      else
      {
         for ( ; *ptrRead != '\0' && !isalpha( *ptrRead ); ptrRead++ )
         {
            ;
         }
      }
   }
   if ( *ptrRead == '\r' )
   { // Strip ^M at the same time.
      ptrWrite--;
   }
   *ptrWrite = '\0';
   if ( bufferSize > 0 )
   {
      snprintf( ptrText, bufferSize, "%s", swork );
   }
   return ptrText;
}
