/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "bbsrc.h"
#include "browser.h"
#include "client.h"
#include <cmocka.h>
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
#include "test_helpers.h"
#include "utility.h"
static int fatalPerrorCallCount;
static int fatalExitCallCount;
static int myExitCallCount;
static bool shouldInjectPtyInputDuringNetworkDrain;
static int telReceiveCallCount;
static int waitNextEventCallCount;

static void resetState( void )
{
   int keyIndex;

   fatalPerrorCallCount = 0;
   fatalExitCallCount = 0;
   myExitCallCount = 0;
   shouldInjectPtyInputDuringNetworkDrain = false;
   telReceiveCallCount = 0;
   waitNextEventCallCount = 0;

   targetByte = 0;
   byte = 0;
   bytePosition = 0;
   childPid = 0;
   isAway = 0;
   isLoginShell = 0;
   capture = 0;

   flagsConfiguration.isConfigMode = 0;
   flagsConfiguration.isPosting = 0;
   flagsConfiguration.shouldCheckExpress = 0;
   flagsConfiguration.isLastSave = 0;

   commandKey = -1;
   awayKey = 'a';
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   browserKey = 'w';
   captureKey = 'c';

   for ( keyIndex = 0; keyIndex < 128; ++keyIndex )
   {
      aryMacro[keyIndex][0] = '\0';
   }
   for ( keyIndex = 0; keyIndex < 1000; ++keyIndex )
   {
      arySavedByteCanReplay[keyIndex] = false;
      arySavedBytes[keyIndex] = 0;
   }

   ptyInputLength = 0;
   ptrPtyInput = aryPtyInputBuffer;
   netInputLength = 0;
   ptrNetInput = aryNetInputBuffer;
}

static void setPtyInput( const int *aryInput, size_t inputCount )
{
   int aryCopiedInput[sizeof( aryPtyInputBuffer )];
   size_t copiedCount;
   size_t inputIndex;

   copiedCount = copyIntArray( aryInput,
                               inputCount,
                               aryCopiedInput,
                               sizeof( aryCopiedInput ) / sizeof( aryCopiedInput[0] ) );
   for ( inputIndex = 0; inputIndex < copiedCount; ++inputIndex )
   {
      aryPtyInputBuffer[inputIndex] = (unsigned char)aryCopiedInput[inputIndex];
   }
   ptyInputLength = (ssize_t)copiedCount;
   ptrPtyInput = aryPtyInputBuffer;
}

// inkey.c dependencies outside this test scope.
noreturn void fatalPerror( const char *message, const char *heading )
{
   (void)message;
   (void)heading;
   fatalPerrorCallCount++;
   abort();
}

noreturn void fatalExit( const char *message, const char *heading )
{
   (void)message;
   (void)heading;
   fatalExitCallCount++;
   abort();
}

noreturn void myExit( void )
{
   myExitCallCount++;
   abort();
}

void openBrowser( void )
{
   // Test stub: browser launching is not relevant in this test.
}

void run( const char *ptrCommand, const char *ptrArg )
{
   (void)ptrCommand;
   (void)ptrArg;
}

int stdPrintf( const char *format, ... )
{
   va_list argList;

   va_start( argList, format );
   va_end( argList );
   return 0;
}

void suspend( void )
{
   // Test stub: suspend handling is not relevant in this test.
}

int telReceive( int inputChar )
{
   telReceiveCallCount++;
   (void)inputChar;
   if ( shouldInjectPtyInputDuringNetworkDrain )
   {
      const int aryInput[] = { 'J' };

      shouldInjectPtyInputDuringNetworkDrain = false;
      setPtyInput( aryInput, sizeof( aryInput ) / sizeof( aryInput[0] ) );
   }

   return 0;
}

int waitNextEvent( void )
{
   waitNextEventCallCount++;
   return 0;
}

int yesNo( void )
{
   return 0;
}

/// @brief Verify that command macros return their configured text.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void getKey_WhenCommandMacroTriggered_ReturnsMacroText( void **state )
{
   const int aryInput[] = { ';', 'm' };
   int firstResult;
   int secondResult;

   (void)state;

   resetState();
   commandKey = ';';
   snprintf( aryMacro['m'], sizeof( aryMacro['m'] ), "%s", "Hi" );
   setPtyInput( aryInput, sizeof( aryInput ) / sizeof( aryInput[0] ) );

   firstResult = inKey();
   secondResult = inKey();

   if ( firstResult != 'H' || secondResult != 'i' )
   {
      fail_msg( "macro expansion should return 'H' then 'i'; got %d then %d", firstResult, secondResult );
   }
   if ( fatalPerrorCallCount != 0 || myExitCallCount != 0 )
   {
      fail_msg( "macro expansion should not hit fatal paths; fatalPerror=%d fatalExit=%d myExit=%d",
                fatalPerrorCallCount, fatalExitCallCount, myExitCallCount );
   }
}

/// @brief Verify that local input interrupts buffered network draining.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void getKey_WhenLocalInputArrivesDuringNetworkDrain_ReturnsLocalInput( void **state )
{
   int result;

   (void)state;

   resetState();
   aryNetInputBuffer[0] = 'A';
   aryNetInputBuffer[1] = 'B';
   netInputLength = 2;
   ptrNetInput = aryNetInputBuffer;
   shouldInjectPtyInputDuringNetworkDrain = true;

   result = getKey();

   if ( result != 'J' )
   {
      fail_msg( "getKey should return local input that arrives during network draining; got %d", result );
   }
   if ( telReceiveCallCount != 1 )
   {
      fail_msg( "getKey should stop draining network bytes once local input is buffered; got %d network bytes",
                telReceiveCallCount );
   }
}

/// @brief Verify that saved bytes replay before normal input.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void getKey_WhenTargetByteActive_ReturnsSavedByteAndAdvancesPosition( void **state )
{
   int result;

   (void)state;

   resetState();
   arySavedBytes[0] = (unsigned char)'Q';
   arySavedByteCanReplay[0] = true;
   bytePosition = 0;
   targetByte = 1;

   result = getKey();

   if ( result != 'Q' )
   {
      fail_msg( "getKey should return byte from arySavedBytes when targetByte is active; got %d", result );
   }
   if ( bytePosition != 1 )
   {
      fail_msg( "getKey should advance bytePosition when replaying saved byte; got %ld", bytePosition );
   }
}

/// @brief Verify that non-replayable saved bytes are skipped during protocol replay.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void getKey_WhenTargetByteIncludesNonReplayableBytes_SkipsToReplayableByte( void **state )
{
   int result;

   (void)state;

   resetState();
   arySavedBytes[0] = (unsigned char)'M';
   arySavedBytes[1] = (unsigned char)'J';
   arySavedByteCanReplay[0] = false;
   arySavedByteCanReplay[1] = true;
   byte = 0;
   bytePosition = 0;
   targetByte = 2;

   result = getKey();

   if ( result != 'J' )
   {
      fail_msg( "getKey should skip non-replayable bytes and return the next replayable byte; got %d", result );
   }
   if ( bytePosition != 2 )
   {
      fail_msg( "getKey should advance past skipped and replayed bytes; got %ld", bytePosition );
   }
   if ( byte != 1 )
   {
      fail_msg( "getKey should count skipped non-replayable bytes before replaying; got %ld", byte );
   }
}

/// @brief Verify that a line feed after a carriage return is skipped.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void inKey_WhenCarriageReturnThenLineFeed_SkipsSecondNewline( void **state )
{
   const int aryInput[] = { '\r', '\n', 'M' };
   int firstResult;
   int secondResult;

   (void)state;

   resetState();
   setPtyInput( aryInput, sizeof( aryInput ) / sizeof( aryInput[0] ) );

   firstResult = inKey();
   secondResult = inKey();

   if ( firstResult != '\n' )
   {
      fail_msg( "first result should normalize CR to newline; got %d", firstResult );
   }
   if ( secondResult != 'M' )
   {
      fail_msg( "second result should skip LF after CR and return next char; got %d", secondResult );
   }
   if ( waitNextEventCallCount != 0 )
   {
      fail_msg( "inKey should not call waitNextEvent when PTY input already exists; got %d calls", waitNextEventCallCount );
   }
}

/// @brief Verify that local delete and `CTRL_U` input are normalized.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void inKey_WhenDeleteAndCtrlU_AppliesKeyTranslations( void **state )
{
   const int aryInput[] = { 127, CTRL_U };
   int ctrlUResult;
   int deleteResult;

   (void)state;

   resetState();
   setPtyInput( aryInput, sizeof( aryInput ) / sizeof( aryInput[0] ) );

   deleteResult = inKey();
   ctrlUResult = inKey();

   if ( deleteResult != '\b' )
   {
      fail_msg( "delete key should map to backspace; got %d", deleteResult );
   }
   if ( ctrlUResult != CTRL_X )
   {
      fail_msg( "CTRL_U should map to CTRL_X; got %d", ctrlUResult );
   }
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( getKey_WhenCommandMacroTriggered_ReturnsMacroText ),
      cmocka_unit_test( getKey_WhenLocalInputArrivesDuringNetworkDrain_ReturnsLocalInput ),
      cmocka_unit_test( getKey_WhenTargetByteActive_ReturnsSavedByteAndAdvancesPosition ),
      cmocka_unit_test( getKey_WhenTargetByteIncludesNonReplayableBytes_SkipsToReplayableByte ),
      cmocka_unit_test( inKey_WhenCarriageReturnThenLineFeed_SkipsSecondNewline ),
      cmocka_unit_test( inKey_WhenDeleteAndCtrlU_AppliesKeyTranslations ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
