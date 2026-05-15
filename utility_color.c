/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file handles ANSI color output helpers.
 */
#include "client_globals.h"
#include "color.h"
#include "defs.h"
#include "utility.h"
static void printAnsiColorValue( int colorValue, bool isBackground );
static void printAnsiSequence( const char *ptrSequence );
static bool tryPrintLegacyAtColor( int inputChar );

/// @brief Expand the legacy `@` color markup into ANSI output.
///
/// @param str Text that may contain legacy `@` color markers.
///
/// @return Always returns `1`.
int colorize( const char *str )
{
   const char *ptrText;

   for ( ptrText = str; *ptrText; ptrText++ )
   {
      if ( *ptrText == '@' )
      {
         if ( !*( ptrText + 1 ) )
         {
            ptrText--;
         }
         else
         {
            if ( !tryPrintLegacyAtColor( *++ptrText ) )
            {
               // Ignore unknown @-tokens for compatibility with the legacy formatter.
            }
         }
      }
      else
      {
         stdPutChar( (int)*ptrText );
      }
   }
   return 1;
}

/// @brief Emit an ANSI background color escape for the supplied color value.
///
/// @param colorValue Color value to emit.
///
/// @return This function does not return a value.
void printAnsiBackgroundColorValue( int colorValue )
{
   printAnsiColorValue( colorValue, true );
}

/// @brief Emit an ANSI foreground or background color escape.
///
/// @param colorValue Color value to emit.
/// @param isBackground Non-zero to emit a background color, zero for foreground.
///
/// @return This function does not return a value.
static void printAnsiColorValue( int colorValue, bool isBackground )
{
   char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

   if ( !flagsConfiguration.shouldUseAnsi )
   {
      return;
   }

   if ( isBackground )
   {
      formatAnsiBackgroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ), colorValue );
   }
   else
   {
      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ), colorValue );
   }
   printAnsiSequence( aryAnsiSequence );
}

/// @brief Emit a full ANSI display state from the supplied foreground and background.
///
/// @param foregroundColor Foreground color value.
/// @param backgroundColor Background color value.
///
/// @return This function does not return a value.
void printAnsiDisplayStateValue( int foregroundColor, int backgroundColor )
{
   char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

   if ( !flagsConfiguration.shouldUseAnsi )
   {
      return;
   }

   formatAnsiDisplayStateSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                   foregroundColor, backgroundColor,
                                   flagsConfiguration.shouldUseBold );
   printAnsiSequence( aryAnsiSequence );
}

/// @brief Emit an ANSI foreground color escape for the supplied color value.
///
/// @param colorValue Color value to emit.
///
/// @return This function does not return a value.
void printAnsiForegroundColorValue( int colorValue )
{
   printAnsiColorValue( colorValue, false );
}

/// @brief Emit the configured ANSI reset sequence.
///
/// @return This function does not return a value.
void printAnsiResetValue( void )
{
   char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

   if ( !flagsConfiguration.shouldUseAnsi )
   {
      return;
   }

   formatAnsiResetSequence( aryAnsiSequence, sizeof( aryAnsiSequence ) );
   printAnsiSequence( aryAnsiSequence );
}

/// @brief Print a prebuilt ANSI escape sequence.
///
/// @param ptrSequence Escape sequence to write.
///
/// @return This function does not return a value.
static void printAnsiSequence( const char *ptrSequence )
{
   stdPrintf( "%s", ptrSequence );
}

/// @brief Print menu text and recolor mnemonic characters wrapped in angle brackets.
///
/// @param ptrText Text to print.
/// @param defaultColor Foreground color used for non-mnemonic text.
///
/// @return This function does not return a value.
void printThemedMnemonicText( const char *ptrText, int defaultColor )
{
   const char *ptrScan;

   if ( ptrText == NULL )
   {
      return;
   }

   if ( !flagsConfiguration.shouldUseAnsi )
   {
      stdPrintf( "%s", ptrText );
      return;
   }

   printAnsiForegroundColorValue( defaultColor );

   for ( ptrScan = ptrText; *ptrScan; )
   {
      if ( ptrScan[0] == '<' && ptrScan[1] != '\0' && ptrScan[2] == '>' )
      {
         printAnsiForegroundColorValue( color.forum );
         stdPutChar( ptrScan[1] );
         printAnsiForegroundColorValue( defaultColor );
         ptrScan += 3;
         continue;
      }

      stdPutChar( *ptrScan );
      ptrScan++;
   }
}

/// @brief Translate a single legacy `@` color token into ANSI output.
///
/// @param inputChar Token character that follows the `@`.
///
/// @return `true` when the token was recognized, otherwise `false`.
static bool tryPrintLegacyAtColor( int inputChar )
{
   switch ( inputChar )
   {
      case '@':
         stdPutChar( '@' );
         return true;
      case 'k':
         printAnsiColorValue( 0, true );
         return true;
      case 'K':
         printAnsiColorValue( 0, false );
         return true;
      case 'r':
         printAnsiColorValue( 1, true );
         return true;
      case 'R':
         printAnsiColorValue( 1, false );
         return true;
      case 'g':
         printAnsiColorValue( 2, true );
         return true;
      case 'G':
         printAnsiColorValue( 2, false );
         return true;
      case 'y':
         printAnsiColorValue( 3, true );
         return true;
      case 'Y':
         printAnsiColorValue( 3, false );
         return true;
      case 'b':
         printAnsiColorValue( 4, true );
         return true;
      case 'B':
         printAnsiColorValue( 4, false );
         return true;
      case 'm':
      case 'p':
         printAnsiColorValue( 5, true );
         return true;
      case 'M':
      case 'P':
         printAnsiColorValue( 5, false );
         return true;
      case 'c':
         printAnsiColorValue( 6, true );
         return true;
      case 'C':
         printAnsiColorValue( 6, false );
         return true;
      case 'w':
         printAnsiColorValue( 7, true );
         return true;
      case 'W':
         printAnsiColorValue( 7, false );
         return true;
      case 'd':
         printAnsiColorValue( 9, true );
         return true;
      case 'D':
         printAnsiColorValue( 9, false );
         return true;
      default:
         return false;
   }
}
