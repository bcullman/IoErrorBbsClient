/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This handles short string input and grouped string input before sending the
 * result to the BBS. Collecting the full field locally reduces network traffic
 * and avoids echoing each character individually. The implementation is based
 * on the original BBS code with client-side adaptations.
 */
#include "client_globals.h"
#include "color.h"
#include "defs.h"
#include "getline_input.h"
#include "macos_keychain.h"
#include "telnet.h"
#include "utility.h"
typedef struct
{
   unsigned int invalid;
   int hidden;
   int length;
} GetStringState;

static char aryWrap[80];

static bool appendPrintableChar( int inputChar, char *result, char **ptrCursor,
                                 const GetStringState *ptrState );
static void applyWrapSeed( int length, int line, char *result, char **ptrCursor );
static bool handleBackspaceEdit( int inputChar, const char *result, char **ptrCursor );
static bool handleCtrlWEdit( const char *result, char **ptrCursor );
static bool handleGetStringOverflow( int inputChar, int line, const char *result,
                                     char **ptrCursor );
static void normalizeGetStringMode( int *ptrLength, GetStringState *ptrState );
static void recordCapturedString( const char *ptrResult, const GetStringState *ptrState );
static bool tryAutoFillHiddenInput( char *ptrResult, size_t resultSize,
                                    const GetStringState *ptrState );

static bool appendPrintableChar( int inputChar, char *result, char **ptrCursor,
                                 const GetStringState *ptrState )
{
   if ( *ptrCursor >= result + ptrState->length || !isprint( inputChar ) )
   {
      return false;
   }

   **ptrCursor = (char)inputChar;
   ( *ptrCursor )++;
   putchar( ptrState->hidden ? '.' : inputChar );
   return true;
}

/// @brief Seed a wrapped input field with leftover text from the previous line.
///
/// @param length Maximum input length.
/// @param line Input line index.
/// @param result Destination buffer.
/// @param ptrCursor Receives the updated cursor position.
///
/// @return This helper does not return a value.
static void applyWrapSeed( int length, int line, char *result, char **ptrCursor )
{
   if ( line <= 0 )
   {
      *aryWrap = 0;
   }
   else if ( *aryWrap )
   {
      printf( "%s", aryWrap );
      if ( length > 0 )
      {
         snprintf( result, (size_t)length + 1, "%s", aryWrap );
      }
      else
      {
         *result = 0;
      }
      *ptrCursor = result + strlen( result );
      *aryWrap = 0;
   }
}

/// @brief Collect and send a five-line input block such as an X or profile entry.
///
/// @param which Bitmask controlling the special command handling for this input mode.
///
/// @return This function does not return a value.
void getFiveLines( int which )
{
   register int lineIndex;
   register int sendLineIndex;
   char arySendString[21][80];
   int override = 0;
   int local = 0;

   if ( isAway && sendingXState == SX_SENT_NAME )
   {
      sendingXState = SX_REPLYING;
      replyMessage();
      return;
   }
   if ( isXland )
   {
      sendingXState = SX_SEND_NEXT;
   }
   else
   {
      sendingXState = SX_NOT;
   }
   if ( flagsConfiguration.shouldUseAnsi )
   {
      char aryAnsiSequence[32];

      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.inputText );
      stdPrintf( "%s", aryAnsiSequence );
   }
   for ( lineIndex = 0; lineIndex < ( 20 + override + local ) &&
                        ( !lineIndex || *arySendString[lineIndex - 1] );
         lineIndex++ )
   {
      stdPrintf( ">" );
      getString( 78, arySendString[lineIndex], lineIndex );
      if ( ( which && !strcmp( arySendString[lineIndex], "ABORT" ) ) ||
           ( !( which & 16 ) && !lineIndex &&
             !strcmp( *arySendString, "PING" ) ) )
      {
         lineIndex++;
         break;
      }
      else if ( ( which & 2 ) && !( which & 8 ) &&
                !strcmp( arySendString[lineIndex], "OVERRIDE" ) )
      {
         stdPrintf( "Override on.\r\n" );
         override++;
      }
      else if ( ( which & 4 ) && !lineIndex &&
                !strcmp( *arySendString, "BEEPS" ) )
      {
         lineIndex++;
         break;
      }
      else if ( ( which & 8 ) &&
                !strcmp( arySendString[lineIndex], "LOCAL" ) )
      {
         stdPrintf( "Only broadcasting to local users.\r\n" );
         local++;
      }
   }
   sendBlock();
   if ( !strcmp( *arySendString, "PING" ) )
   {
      arySendString[0][0] = 0;
   }
   for ( sendLineIndex = 0; sendLineIndex < lineIndex; sendLineIndex++ )
   {
      int sendCharIndex;

      sendCharIndex = (int)strlen( arySendString[sendLineIndex] );
      sendTrackedBuffer( arySendString[sendLineIndex], (size_t)sendCharIndex );
      sendTrackedNewline();
   }
   if ( flagsConfiguration.shouldUseAnsi )
   {
      char aryAnsiSequence[32];

      lastColor = color.text;
      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    lastColor );
      stdPrintf( "%s", aryAnsiSequence );
   }
}

/// @brief Read a short input string with local editing and optional hidden echo.
///
/// @param length Maximum input length. Negative values request hidden echo.
/// @param result Destination buffer for the collected string.
/// @param line Line index used for wrapped multi-line input.
///
/// @return This function does not return a value.
void getString( int length, char *result, int line )
{
   GetStringState state;
   char *ptrCursor = result;

   normalizeGetStringMode( &length, &state );
   applyWrapSeed( length, line, result, &ptrCursor );
   if ( tryAutoFillHiddenInput( result, (size_t)length + 1, &state ) )
   {
      recordCapturedString( result, &state );
      stdPrintf( "\r\n" );
      return;
   }

   while ( true )
   {
      int inputChar;

      inputChar = inKey();
      if ( inputChar == ' ' && length == 29 && ptrCursor == result )
      {
         break;
      }
      if ( inputChar == '\n' )
      {
         break;
      }
      if ( inputChar < ' ' && inputChar != '\b' &&
           inputChar != CTRL_X && inputChar != CTRL_W )
      {
         handleInvalidInput( &state.invalid );
         continue;
      }
      else
      {
         state.invalid = 0;
      }
      if ( handleBackspaceEdit( inputChar, result, &ptrCursor ) )
      {
         continue;
      }
      if ( inputChar == CTRL_W )
      {
         handleCtrlWEdit( result, &ptrCursor );
         continue;
      }
      if ( appendPrintableChar( inputChar, result, &ptrCursor, &state ) )
      {
         continue;
      }
      if ( line < 0 || line == 19 )
      {
         continue;
      }
      if ( !handleGetStringOverflow( inputChar, line, result, &ptrCursor ) )
      {
         break;
      }
   }
   *ptrCursor = 0;
   handleKeychainHiddenInput( result );
   recordCapturedString( result, &state );
   stdPrintf( "\r\n" );
}

/// @brief Handle backspace-style editing inside `getString()`.
///
/// @param inputChar Input character to process.
/// @param result Start of the input buffer.
/// @param ptrCursor Current cursor position.
///
/// @return `true` if the input was handled as backspace editing, otherwise `false`.
static bool handleBackspaceEdit( int inputChar, const char *result, char **ptrCursor )
{
   if ( inputChar != '\b' && inputChar != CTRL_X )
   {
      return false;
   }
   if ( *ptrCursor == result )
   {
      return true;
   }

   do
   {
      printf( "\b \b" );
      --( *ptrCursor );
   } while ( inputChar == CTRL_X && *ptrCursor > result );
   return true;
}

/// @brief Erase the previous word inside `getString()`.
///
/// @param result Start of the input buffer.
/// @param ptrCursor Current cursor position.
///
/// @return Always returns `true`.
static bool handleCtrlWEdit( const char *result, char **ptrCursor )
{
   int foundWord;
   const char *ptrWordStart;

   for ( ptrWordStart = result; ptrWordStart < *ptrCursor; ptrWordStart++ )
   {
      if ( *ptrWordStart != ' ' )
      {
         break;
      }
   }
   foundWord = ( ptrWordStart == *ptrCursor );
   for ( ; *ptrCursor > result &&
           ( !foundWord || ( *ptrCursor )[-1] != ' ' );
         ( *ptrCursor )-- )
   {
      if ( ( *ptrCursor )[-1] != ' ' )
      {
         foundWord = 1;
      }
      printf( "\b \b" );
   }

   return true;
}

/// @brief Move overflow text into the wrap buffer for the next input line.
///
/// @param inputChar Character that triggered overflow.
/// @param line Current line index.
/// @param result Input buffer.
/// @param ptrCursor Current cursor position.
///
/// @return `true` if input should continue on the current line, otherwise `false`.
static bool handleGetStringOverflow( int inputChar, int line, const char *result,
                                     char **ptrCursor )
{
   char *ptrWordStart;

   if ( line < 0 || line == 19 )
   {
      return true;
   }
   if ( inputChar == ' ' )
   {
      return false;
   }
   for ( ptrWordStart = *ptrCursor - 1;
         ptrWordStart > result && *ptrWordStart != ' ';
         ptrWordStart-- )
   {
      ;
   }
   if ( ptrWordStart > result )
   {
      char *rest;

      *ptrWordStart = 0;
      for ( rest = aryWrap, ptrWordStart++; ptrWordStart < *ptrCursor;
            printf( "\b \b" ) )
      {
         *rest++ = *ptrWordStart++;
      }
      *rest++ = (char)inputChar;
      *rest = 0;
   }
   else
   {
      *aryWrap = (char)inputChar;
      *( aryWrap + 1 ) = 0;
   }

   return false;
}

/// @brief Normalize the raw `getString()` length into active input mode state.
///
/// @param ptrLength Requested input length to normalize in place.
/// @param ptrState Receives the normalized mode state.
///
/// @return This helper does not return a value.
static void normalizeGetStringMode( int *ptrLength, GetStringState *ptrState )
{
   ptrState->invalid = 0;
   ptrState->hidden = 0;
   ptrState->length = *ptrLength;

   if ( ptrState->length < 0 )
   {
      ptrState->length = -ptrState->length;
      ptrState->hidden = ptrState->length;
   }
   if ( ptrState->length > 128 )
   {
      ptrState->length = 256 - ptrState->length;
      ptrState->hidden = ptrState->length;
   }

   *ptrLength = ptrState->length;
}

/// @brief Record the collected string into the capture log.
///
/// @param ptrResult Collected input string.
/// @param ptrState Current input mode state.
///
/// @return This helper does not return a value.
static void recordCapturedString( const char *ptrResult, const GetStringState *ptrState )
{
   size_t charIndex;
   size_t resultLength;

   if ( !ptrState->hidden )
   {
      capPuts( ptrResult );
      return;
   }

   resultLength = strlen( ptrResult );
   for ( charIndex = 0; charIndex < resultLength; charIndex++ )
   {
      capPutChar( '.' );
   }
}

/// @brief Fill a hidden prompt from macOS Keychain when the current prompt allows it.
///
/// @param ptrResult Destination buffer for the hidden input.
/// @param resultSize Size of the destination buffer.
/// @param ptrState Current input mode state.
///
/// @return `true` if the prompt was filled from Keychain, otherwise `false`.
static bool tryAutoFillHiddenInput( char *ptrResult, size_t resultSize,
                                    const GetStringState *ptrState )
{
   size_t charIndex;
   size_t passwordLength;

   if ( ptrState->hidden == 0 ||
        !tryGetKeychainPasswordForPrompt( ptrResult, resultSize ) )
   {
      return false;
   }

   passwordLength = strlen( ptrResult );
   for ( charIndex = 0; charIndex < passwordLength; charIndex++ )
   {
      putchar( '.' );
   }
   return true;
}
