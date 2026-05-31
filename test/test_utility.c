/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config_file.h"
#include "browser.h"
#include "client.h"
#include "test/cmocka_compat.h"
#include "color.h"
#include "config_menu.h"
#include "defs.h"
#include "edit.h"
#include "ext.h"
#include "filter.h"
#include "getline_input.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include "telnet.h"
#include "utility.h"
static int aryInputQueue[16];
static size_t inputCount;
static size_t inputIndex;
static int flushCount;
static int lastNetPutChar;

// Stubs for utility.c dependencies that are outside this test target's scope.
void deinitialize( void )
{
   // Test stub: shutdown side effects are not relevant in this test.
}

void flushInput( unsigned int count )
{
   (void)count;
}

int inKey( void )
{
   if ( inputIndex < inputCount )
   {
      return aryInputQueue[inputIndex++];
   }
   return '\n';
}

int fflush( FILE *stream )
{
   (void)stream;
   flushCount++;
   return 0;
}

int netPutChar( int inputChar )
{
   lastNetPutChar = inputChar;
   return inputChar;
}

void resetTerm( void )
{
   // Test stub: terminal reset behavior is not relevant in this test.
}

void paneUiLeaveForExit( void )
{
   // Test stub: pane cleanup behavior is not relevant in this test.
}

void sError( const char *message, const char *heading )
{
   (void)message;
   (void)heading;
}

void sPerror( const char *message, const char *heading )
{
   (void)message;
   (void)heading;
}

void sendBlock( void )
{
   // Test stub: telnet block signaling is not relevant in this test.
}

void sigOff( void )
{
   // Test stub: signal cleanup is not relevant in this test.
}

int stdPrintf( const char *format, ... )
{
   va_list argList;

   (void)format;
   va_start( argList, format );
   va_end( argList );
   return 0;
}

int stdPutChar( int inputChar )
{
   return inputChar;
}

static void setInputSequence( const int *arySource, size_t sourceCount )
{
   size_t itemIndex;

   flushCount = 0;
   inputCount = sourceCount;
   if ( inputCount > sizeof( aryInputQueue ) / sizeof( aryInputQueue[0] ) )
   {
      inputCount = sizeof( aryInputQueue ) / sizeof( aryInputQueue[0] );
   }
   inputIndex = 0;
   for ( itemIndex = 0; itemIndex < inputCount; itemIndex++ )
   {
      aryInputQueue[itemIndex] = arySource[itemIndex];
   }
}

/// @brief Verify that validated interactive input is sent, flushed, and tracked.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void looper_WhenValidInputIsForwarded_FlushesImmediately( void **state )
{
   const int arySequence[] = { 'J', -1 };

   // Arrange
   (void)state;

   byte = 1;
   lastInteractiveInputByte = -1;
   lastNetPutChar = 0;
   setInputSequence( arySequence, sizeof( arySequence ) / sizeof( arySequence[0] ) );

   // Act
   looper();

   // Assert
   if ( lastNetPutChar != aryKeyMap['J'] )
   {
      fail_msg( "looper should forward valid input immediately; expected %d got %d",
                aryKeyMap['J'], lastNetPutChar );
   }
   if ( flushCount != 1 )
   {
      fail_msg( "looper should flush one forwarded interactive key immediately; got %d flushes",
                flushCount );
   }
   if ( arySavedBytes[1] != 'J' || byte != 2 )
   {
      fail_msg( "looper should track the original input byte after sending; tracked=%d byte=%ld",
                arySavedBytes[1], byte );
   }
   if ( lastInteractiveInputByte != 1 )
   {
      fail_msg( "looper should remember the saved-byte position of the last interactive input; got %ld",
                lastInteractiveInputByte );
   }
   if ( !arySavedByteCanReplay[1] )
   {
      fail_msg( "looper should mark normal typed input as replayable" );
   }
}

/// @brief Verify that tracked buffers populate the replay buffer.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void sendTrackedBuffer_WhenBytesSent_PopulatesReplayBuffer( void **state )
{
   (void)state;

   byte = 10;
   arySavedBytes[10] = 0;
   arySavedBytes[11] = 0;
   arySavedBytes[12] = 0;
   arySavedByteCanReplay[10] = false;
   arySavedByteCanReplay[11] = false;
   arySavedByteCanReplay[12] = false;
   lastNetPutChar = 0;

   sendTrackedBuffer( "abc", 3 );

   if ( byte != 13 )
   {
      fail_msg( "sendTrackedBuffer should advance byte once per sent char; got %ld", byte );
   }
   if ( arySavedBytes[10] != 'a' ||
        arySavedBytes[11] != 'b' ||
        arySavedBytes[12] != 'c' )
   {
      fail_msg( "sendTrackedBuffer should populate replay bytes; got %d,%d,%d",
                arySavedBytes[10], arySavedBytes[11], arySavedBytes[12] );
   }
   if ( lastNetPutChar != 'c' )
   {
      fail_msg( "sendTrackedBuffer should send every byte; last sent=%d", lastNetPutChar );
   }
   if ( !arySavedByteCanReplay[10] ||
        !arySavedByteCanReplay[11] ||
        !arySavedByteCanReplay[12] )
   {
      fail_msg( "sendTrackedBuffer should mark sent bytes as replayable" );
   }
}

/// @brief Verify that non-replayable tracked bytes advance the byte counter without replay.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void sendTrackedCharWithoutReplay_WhenByteSent_MarksByteNonReplayable( void **state )
{
   (void)state;

   byte = 20;
   arySavedBytes[20] = 0;
   arySavedByteCanReplay[20] = true;
   lastNetPutChar = 0;

   sendTrackedCharWithoutReplay( 'M' );

   if ( byte != 21 )
   {
      fail_msg( "sendTrackedCharWithoutReplay should advance byte once; got %ld", byte );
   }
   if ( arySavedBytes[20] != 'M' )
   {
      fail_msg( "sendTrackedCharWithoutReplay should still save the byte value; got %d",
                arySavedBytes[20] );
   }
   if ( arySavedByteCanReplay[20] )
   {
      fail_msg( "sendTrackedCharWithoutReplay should mark the byte as non-replayable" );
   }
   if ( lastNetPutChar != 'M' )
   {
      fail_msg( "sendTrackedCharWithoutReplay should send the byte; last sent=%d",
                lastNetPutChar );
   }
}

static void findSubstring_WhenNeedleExists_ReturnsPointerToFirstMatch( void **state )
{
   // Arrange
   const char *ptrHaystack;
   const char *ptrResult;

   (void)state;

   ptrHaystack = "Did I leave my needle in this haystack?";

   // Act
   ptrResult = findSubstring( ptrHaystack, "needle" );

   // Assert
   if ( ptrResult == NULL )
   {
      fail_msg( "findSubstring should return a match pointer when substring exists" );
   }
   if ( strcmp( ptrResult, "needle in this haystack?" ) != 0 )
   {
      fail_msg( "findSubstring should return pointer at first match; got '%s'", ptrResult );
   }
}

static void readFoldedKey_WhenInputIsUppercaseLetter_ReturnsLowercaseLetter( void **state )
{
   // Arrange
   const int arySequence[] = { 'Q' };
   int result;

   (void)state;
   setInputSequence( arySequence, sizeof( arySequence ) / sizeof( arySequence[0] ) );

   // Act
   result = readFoldedKey();

   // Assert
   if ( result != 'q' )
   {
      fail_msg( "readFoldedKey should lowercase alphabetic input; got '%c'", result );
   }
}

static void readValidatedMenuKey_WhenInputIsUppercaseLetter_ReturnsLowercaseMatch( void **state )
{
   // Arrange
   const int arySequence[] = { 'X' };
   int result;

   (void)state;
   setInputSequence( arySequence, sizeof( arySequence ) / sizeof( arySequence[0] ) );

   // Act
   result = readValidatedMenuKey( "axq \n" );

   // Assert
   if ( result != 'x' )
   {
      fail_msg( "readValidatedMenuKey should return lowercase validated menu input; got '%c'", result );
   }
}

static void readColorEditorAction_WhenArrowSequenceReceived_ReturnsNormalizedAction( void **state )
{
   // Arrange
   const int arySequence[] = { '\033', '[', 'A' };
   ColorEditorAction result;

   (void)state;
   setInputSequence( arySequence, sizeof( arySequence ) / sizeof( arySequence[0] ) );

   // Act
   result = readColorEditorAction();

   // Assert
   if ( result != COLOR_EDITOR_ACTION_PREVIOUS_FIELD )
   {
      fail_msg( "readColorEditorAction should map up-arrow to previous-field navigation; got %d", result );
   }
}

static void readColorEditorAction_WhenFallbackCommandReceived_ReturnsNormalizedAction( void **state )
{
   // Arrange
   const int arySequence[] = { 'P' };
   ColorEditorAction result;

   (void)state;
   setInputSequence( arySequence, sizeof( arySequence ) / sizeof( arySequence[0] ) );

   // Act
   result = readColorEditorAction();

   // Assert
   if ( result != COLOR_EDITOR_ACTION_PREVIOUS_FIELD )
   {
      fail_msg( "readColorEditorAction should lowercase fallback commands and map them; got %d", result );
   }
}

static void readColorEditorAction_WhenShiftedFastStepAliasReceived_ReturnsFastStepAction( void **state )
{
   // Arrange
   const int aryIncreaseSequence[] = { '=' };
   const int aryDecreaseSequence[] = { '_' };
   ColorEditorAction result;

   (void)state;

   setInputSequence( aryIncreaseSequence,
                     sizeof( aryIncreaseSequence ) / sizeof( aryIncreaseSequence[0] ) );

   // Act
   result = readColorEditorAction();

   // Assert
   if ( result != COLOR_EDITOR_ACTION_INCREASE_FAST )
   {
      fail_msg( "readColorEditorAction should accept '=' as a fast increase alias; got %d",
                result );
   }

   // Arrange
   setInputSequence( aryDecreaseSequence,
                     sizeof( aryDecreaseSequence ) / sizeof( aryDecreaseSequence[0] ) );

   // Act
   result = readColorEditorAction();

   // Assert
   if ( result != COLOR_EDITOR_ACTION_DECREASE_FAST )
   {
      fail_msg( "readColorEditorAction should accept '_' as a fast decrease alias; got %d",
                result );
   }
}

static void readColorEditorAction_WhenApplicationKeypadOperatorReceived_ReturnsFastStepAction( void **state )
{
   // Arrange
   const int aryIncreaseSequence[] = { '\033', 'O', 'k' };
   const int aryDecreaseSequence[] = { '\033', 'O', 'm' };
   ColorEditorAction result;

   (void)state;

   setInputSequence( aryIncreaseSequence,
                     sizeof( aryIncreaseSequence ) / sizeof( aryIncreaseSequence[0] ) );

   // Act
   result = readColorEditorAction();

   // Assert
   if ( result != COLOR_EDITOR_ACTION_INCREASE_FAST )
   {
      fail_msg( "readColorEditorAction should accept SS3 keypad plus as a fast increase; got %d",
                result );
   }

   // Arrange
   setInputSequence( aryDecreaseSequence,
                     sizeof( aryDecreaseSequence ) / sizeof( aryDecreaseSequence[0] ) );

   // Act
   result = readColorEditorAction();

   // Assert
   if ( result != COLOR_EDITOR_ACTION_DECREASE_FAST )
   {
      fail_msg( "readColorEditorAction should accept SS3 keypad minus as a fast decrease; got %d",
                result );
   }
}

static void findSubstring_WhenNeedleMissing_ReturnsNull( void **state )
{
   // Arrange
   const char *ptrResult;

   (void)state;

   // Act
   ptrResult = findSubstring( "Watson. Come here. I need you.", "Moriarty" );

   // Assert
   if ( ptrResult != NULL )
   {
      fail_msg( "findSubstring should return NULL when substring is missing; got '%s'", ptrResult );
   }
}

static void findChar_WhenTargetExists_ReturnsPointerToCharacter( void **state )
{
   // Arrange
   const char *ptrInput;
   const char *ptrResult;

   (void)state;

   ptrInput = "Elementary, my dear Watson.";

   // Act
   ptrResult = findChar( ptrInput, 'W' );

   // Assert
   if ( ptrResult == NULL )
   {
      fail_msg( "findChar should return a pointer when target char exists" );
   }
   if ( *ptrResult != 'W' )
   {
      fail_msg( "findChar should point to the matched char 'W'; got '%c'", *ptrResult );
   }
}

static void findChar_WhenTargetMissing_ReturnsNull( void **state )
{
   // Arrange
   const char *ptrResult;

   (void)state;

   // Act
   ptrResult = findChar( "The game is afoot.", 'z' );

   // Assert
   if ( ptrResult != NULL )
   {
      fail_msg( "findChar should return NULL when target char is absent; got '%s'", ptrResult );
   }
}

static void duplicateString_WhenSourceProvided_ReturnsIndependentCopy( void **state )
{
   // Arrange
   char arySource[] = "Watson. Come here. I need you.";
   char *ptrCopy;

   (void)state;

   // Act
   ptrCopy = duplicateString( arySource );

   // Assert
   if ( ptrCopy == NULL )
   {
      fail_msg( "duplicateString should allocate and return a copy for non-NULL input" );
   }
   if ( strcmp( ptrCopy, arySource ) != 0 )
   {
      fail_msg( "duplicateString result mismatch: expected '%s', got '%s'", arySource, ptrCopy );
   }

   ptrCopy[0] = 'w';
   if ( strcmp( arySource, "Watson. Come here. I need you." ) != 0 )
   {
      fail_msg( "duplicateString should create an independent copy, not alias source" );
   }

   free( ptrCopy );
}

static void extractName_WhenHeaderContainsSender_ReturnsSenderName( void **state )
{
   // Arrange
   const char *ptrName;

   (void)state;

   memset( aryLastName, 0, sizeof( aryLastName ) );

   // Act
   ptrName = extractName( "*** Message (old) from Dr Strange at 3:07 PM on Feb 19, 2026 ***" );

   // Assert
   if ( ptrName == NULL )
   {
      fail_msg( "extractName should return a name pointer for valid header input" );
   }
   if ( strcmp( ptrName, "Dr Strange" ) != 0 )
   {
      fail_msg( "extractName should parse 'Dr Strange'; got '%s'", ptrName );
   }
   if ( strcmp( aryLastName[0], "Dr Strange" ) != 0 )
   {
      fail_msg( "extractName should store parsed name at history slot 0; got '%s'", aryLastName[0] );
   }
}

static void extractName_WhenDuplicateSeen_MovesExistingEntryToFront( void **state )
{
   // Arrange
   const char *ptrName;

   (void)state;

   memset( aryLastName, 0, sizeof( aryLastName ) );
   extractName( "Jan 29, 2026 10:17 AM from Dr Strange to Stilgar" );
   extractName( "Jan 29, 2026 10:17 AM from Meatball to Stilgar" );

   // Act
   ptrName = extractName( "Jan 29, 2026 11:07 AM from Dr Strange to Stilgar" );

   // Assert
   if ( ptrName == NULL )
   {
      fail_msg( "extractName should return non-NULL for duplicate sender headers" );
   }
   if ( strcmp( aryLastName[0], "Dr Strange" ) != 0 || strcmp( aryLastName[1], "Meatball" ) != 0 )
   {
      fail_msg( "duplicate sender should move to front; expected [Dr Strange, Meatball], got ['%s', '%s']",
                aryLastName[0], aryLastName[1] );
   }
}

static void extractName_WhenHeaderDoesNotContainFrom_ReturnsNull( void **state )
{
   // Arrange
   const char *ptrName;

   (void)state;

   // Act
   ptrName = extractName( "No sender marker here" );

   // Assert
   if ( ptrName != NULL )
   {
      fail_msg( "extractName should return NULL when header lacks ' from '; got '%s'", ptrName );
   }
}

static void extractNumber_WhenHeaderContainsSingleDigit_ReturnsParsedNumber( void **state )
{
   // Arrange
   int number;

   (void)state;

   // Act
   number = extractNumber( "*** Message (new) (#7) from Dr Strange at 3:07 PM on Feb 19, 2026 ***" );

   // Assert
   if ( number != 7 )
   {
      fail_msg( "extractNumber should parse a single-digit number; expected 7, got %d", number );
   }
}

static void extractNumber_WhenHeaderHasNoMarker_ReturnsZero( void **state )
{
   // Arrange
   int number;

   (void)state;

   // Act
   number = extractNumber( "no id in this header" );

   // Assert
   if ( number != 0 )
   {
      fail_msg( "extractNumber should return 0 when '(#' marker is missing; got %d", number );
   }
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( findSubstring_WhenNeedleExists_ReturnsPointerToFirstMatch ),
      cmocka_unit_test( findSubstring_WhenNeedleMissing_ReturnsNull ),
      cmocka_unit_test( readFoldedKey_WhenInputIsUppercaseLetter_ReturnsLowercaseLetter ),
      cmocka_unit_test( readValidatedMenuKey_WhenInputIsUppercaseLetter_ReturnsLowercaseMatch ),
      cmocka_unit_test( readColorEditorAction_WhenArrowSequenceReceived_ReturnsNormalizedAction ),
      cmocka_unit_test( readColorEditorAction_WhenFallbackCommandReceived_ReturnsNormalizedAction ),
      cmocka_unit_test( readColorEditorAction_WhenShiftedFastStepAliasReceived_ReturnsFastStepAction ),
      cmocka_unit_test( readColorEditorAction_WhenApplicationKeypadOperatorReceived_ReturnsFastStepAction ),
      cmocka_unit_test( findChar_WhenTargetExists_ReturnsPointerToCharacter ),
      cmocka_unit_test( findChar_WhenTargetMissing_ReturnsNull ),
      cmocka_unit_test( duplicateString_WhenSourceProvided_ReturnsIndependentCopy ),
      cmocka_unit_test( extractName_WhenHeaderContainsSender_ReturnsSenderName ),
      cmocka_unit_test( extractName_WhenDuplicateSeen_MovesExistingEntryToFront ),
      cmocka_unit_test( extractName_WhenHeaderDoesNotContainFrom_ReturnsNull ),
      cmocka_unit_test( extractNumber_WhenHeaderContainsSingleDigit_ReturnsParsedNumber ),
      cmocka_unit_test( extractNumber_WhenHeaderHasNoMarker_ReturnsZero ),
      cmocka_unit_test( looper_WhenValidInputIsForwarded_FlushesImmediately ),
      cmocka_unit_test( sendTrackedBuffer_WhenBytesSent_PopulatesReplayBuffer ),
      cmocka_unit_test( sendTrackedCharWithoutReplay_WhenByteSent_MarksByteNonReplayable ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
