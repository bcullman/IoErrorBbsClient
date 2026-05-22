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
static unsigned char aryNetOutput[4096];
static size_t netOutputCount;
static int stubWindowRows;
static int filterDataCallCount;
static int filterExpressCallCount;
static int filterPostCallCount;
static int filterWhoListCallCount;
static int lastFilterDataChar;
static int lastFilterExpressChar;
static int getFiveLinesCallCount;
static int getFiveLinesArg;
static int getNameCallCount;
static int getNameArg;
static int makeMessageCallCount;
static int makeMessageArg;
static int configClientCallCount;
static int sendAnXCallCount;
static int morePromptHelperCallCount;
static char aryNameResponse[64];
static char aryStringResponse[256];

static void resetNetOutput( void )
{
   netOutputCount = 0;
   aryNetOutput[0] = 0;
}

static void syncTelnetStateToData( void )
{
   int syncIndex;

   for ( syncIndex = 0; syncIndex < 10; ++syncIndex )
   {
      (void)telReceive( 0 );
   }
   (void)telReceive( IAC );
   (void)telReceive( 0 );
   resetNetOutput();
}

static void resetState( void )
{
   int byteIndex;

   stubWindowRows = 24;
   snprintf( aryNameResponse, sizeof( aryNameResponse ), "%s", "Dr Strange" );
   aryStringResponse[0] = '\0';

   byte = 0;
   targetByte = 0;
   bytePosition = 0;
   lastInteractiveInputByte = -1;
   whoListProgress = 0;
   isExpressMessageInProgress = false;
   isExpressMessageHeaderActive = false;
   postProgressState = 0;
   postHeaderActive = 0;
   isPostJustEnded = false;
   shouldSendExpressMessage = false;
   ptrPostBuffer = 0;
   oldRows = 24;
   rows = 24;
   flagsConfiguration.shouldUseAnsi = 0;
   flagsConfiguration.isMorePromptActive = 0;
   aryExpressParsing[0] = '\0';
   aryExpressMessageBuffer[0] = '\0';
   ptrExpressMessageBuffer = aryExpressMessageBuffer;
   for ( byteIndex = 0; byteIndex < 1000; byteIndex++ )
   {
      arySavedByteCanReplay[byteIndex] = false;
      arySavedBytes[byteIndex] = 0;
   }

   if ( xlandQueue == NULL )
   {
      xlandQueue = newQueue( 21, 5 );
      if ( xlandQueue == NULL )
      {
         fail_msg( "newQueue failed while preparing xlandQueue for telnet tests" );
         return;
      }
   }

   syncTelnetStateToData();

   filterDataCallCount = 0;
   filterExpressCallCount = 0;
   filterPostCallCount = 0;
   filterWhoListCallCount = 0;
   lastFilterDataChar = 0;
   lastFilterExpressChar = 0;
   getFiveLinesCallCount = 0;
   getFiveLinesArg = 0;
   getNameCallCount = 0;
   getNameArg = 0;
   makeMessageCallCount = 0;
   makeMessageArg = 0;
   configClientCallCount = 0;
   sendAnXCallCount = 0;
   morePromptHelperCallCount = 0;
}

// telnet.c dependencies outside these tests.
void configClient( void )
{
   configClientCallCount++;
}

void filterData( int inputChar )
{
   filterDataCallCount++;
   lastFilterDataChar = inputChar;
}

void filterExpress( int inputChar )
{
   filterExpressCallCount++;
   lastFilterExpressChar = inputChar;
}

void filterPost( int inputChar )
{
   filterPostCallCount++;
   (void)inputChar;
}

void filterWhoList( int inputChar )
{
   filterWhoListCallCount++;
   (void)inputChar;
}

void getFiveLines( int which )
{
   getFiveLinesCallCount++;
   getFiveLinesArg = which;
}

char *getName( int quitPriv )
{
   getNameCallCount++;
   getNameArg = quitPriv;
   return aryNameResponse;
}

void getString( int length, char *result, int line )
{
   (void)length;
   (void)line;
   snprintf( result, 80, "%s", aryStringResponse );
}

int getWindowSize( void )
{
   rows = stubWindowRows;
   return stubWindowRows;
}

void makeMessage( int upload )
{
   makeMessageCallCount++;
   makeMessageArg = upload;
}

void morePromptHelper( void )
{
   morePromptHelperCallCount++;
}

int netPutChar( int inputChar )
{
   if ( netOutputCount < sizeof( aryNetOutput ) )
   {
      aryNetOutput[netOutputCount++] = (unsigned char)inputChar;
   }
   return inputChar;
}

void sendTrackedBuffer( const char *ptrBuffer, size_t length )
{
   size_t outputIndex;

   for ( outputIndex = 0; outputIndex < length; ++outputIndex )
   {
      netPutChar( ptrBuffer[outputIndex] );
      byte++;
   }
}

void sendTrackedNewline( void )
{
   netPutChar( '\n' );
   byte++;
}

int netPuts( const char *ptrText )
{
   const char *ptrRead;

   for ( ptrRead = ptrText; *ptrRead != '\0'; ++ptrRead )
   {
      netPutChar( (unsigned char)*ptrRead );
   }
   return 1;
}

void sendAnX( void )
{
   sendAnXCallCount++;
}

int stdPrintf( const char *format, ... )
{
   va_list argList;

   (void)format;
   va_start( argList, format );
   va_end( argList );
   return 1;
}

static void sendBlock_WhenCalled_WritesIacBlockSequence( void **state )
{
   // Arrange
   (void)state;

   resetState();

   // Act
   sendBlock();

   // Assert
   if ( netOutputCount != 2 || aryNetOutput[0] != IAC || aryNetOutput[1] != BLOCK )
   {
      fail_msg( "sendBlock should write IAC,BLOCK sequence; got count=%zu values=%u,%u",
                netOutputCount, aryNetOutput[0], aryNetOutput[1] );
   }
}

static void sendNaws_WhenWindowSizeChanged_SendsNawsPayload( void **state )
{
   // Arrange
   (void)state;

   resetState();
   oldRows = 10;
   rows = 200;
   stubWindowRows = 200;

   // Act
   sendNaws();

   // Assert
   if ( netOutputCount != 9 )
   {
      fail_msg( "sendNaws should emit 9-byte NAWS payload on size change; got %zu bytes", netOutputCount );
   }
   if ( aryNetOutput[0] != IAC || aryNetOutput[1] != SB || aryNetOutput[2] != TELOPT_NAWS ||
        aryNetOutput[7] != IAC || aryNetOutput[8] != SE )
   {
      fail_msg( "sendNaws payload framing is invalid" );
   }
   if ( rows != 24 )
   {
      fail_msg( "sendNaws should clamp invalid row counts to 24; got %d", rows );
   }
}

static void telReceive_WhenClientProbeReceived_RespondsWithClientAck( void **state )
{
   // Arrange
   int result;

   (void)state;

   resetState();

   // Act
   (void)telReceive( IAC );
   result = telReceive( CLIENT );

   // Assert
   if ( result != 0 )
   {
      fail_msg( "telReceive should return 0 for client keepalive handling; got %d", result );
   }
   if ( netOutputCount != 2 || aryNetOutput[0] != IAC || aryNetOutput[1] != CLIENT )
   {
      fail_msg( "CLIENT probe should be answered with IAC CLIENT; got count=%zu", netOutputCount );
   }
}

static void telReceive_WhenGetNameCommandArrives_SendsNameResponse( void **state )
{
   // Arrange
   int result;

   (void)state;

   resetState();
   byte = 3;
   snprintf( aryNameResponse, sizeof( aryNameResponse ), "%s", "Meatball" );

   // Act
   (void)telReceive( IAC );
   (void)telReceive( G_NAME );
   (void)telReceive( 2 );
   (void)telReceive( 0 );
   (void)telReceive( 0 );
   result = telReceive( 10 );

   // Assert
   if ( result != 0 )
   {
      fail_msg( "telReceive G_NAME flow should return 0; got %d", result );
   }
   if ( getNameCallCount != 1 || getNameArg != 2 )
   {
      fail_msg( "G_NAME flow should call getName once with arg 2; got count=%d arg=%d", getNameCallCount, getNameArg );
   }
   if ( netOutputCount < 2 || aryNetOutput[0] != IAC || aryNetOutput[1] != BLOCK )
   {
      fail_msg( "G_NAME flow should start by sending BLOCK sequence" );
   }
   if ( aryNetOutput[netOutputCount - 1] != '\n' )
   {
      fail_msg( "G_NAME flow should terminate with newline" );
   }
   if ( byte != 19 )
   {
      fail_msg( "G_NAME flow should update byte counter from parsed position + name length; got %ld", byte );
   }
}

/// @brief Verify that prompted string input marks the exact command byte that opened the prompt.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void telReceive_WhenGetStringCommandArrives_MarksExactPromptTriggerByteNonReplayable( void **state )
{
   int result;

   // Arrange
   (void)state;

   resetState();
   byte = 11;
   lastInteractiveInputByte = 10;
   arySavedBytes[8] = 'x';
   arySavedBytes[9] = 'y';
   arySavedBytes[10] = 'J';
   arySavedByteCanReplay[8] = true;
   arySavedByteCanReplay[9] = true;
   arySavedByteCanReplay[10] = true;
   snprintf( aryStringResponse, sizeof( aryStringResponse ), "%s", "Forum" );

   // Act
   (void)telReceive( IAC );
   (void)telReceive( G_STR );
   (void)telReceive( 20 );
   (void)telReceive( 0 );
   (void)telReceive( 0 );
   result = telReceive( 8 );

   // Assert
   if ( result != 0 )
   {
      fail_msg( "telReceive G_STR flow should return 0; got %d", result );
   }
   if ( arySavedByteCanReplay[10] )
   {
      fail_msg( "G_STR flow should mark the prompt trigger byte as non-replayable" );
   }
   if ( !arySavedByteCanReplay[8] || !arySavedByteCanReplay[9] )
   {
      fail_msg( "G_STR flow should leave earlier replayable bytes untouched when the trigger byte is later" );
   }
   if ( byte != 14 )
   {
      fail_msg( "G_STR flow should count response and newline bytes from parsed position; got %ld", byte );
   }
}

/// @brief Verify that message entry blocks stale replay bytes from entering the editor.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void telReceive_WhenPostCommandArrives_MarksReplayWindowNonReplayable( void **state )
{
   int result;

   (void)state;

   resetState();
   byte = 22;
   arySavedBytes[19] = 'x';
   arySavedBytes[20] = 'y';
   arySavedBytes[21] = 'E';
   arySavedByteCanReplay[19] = true;
   arySavedByteCanReplay[20] = true;
   arySavedByteCanReplay[21] = true;

   (void)telReceive( IAC );
   (void)telReceive( G_POST );
   (void)telReceive( 0 );
   (void)telReceive( 0 );
   (void)telReceive( 0 );
   result = telReceive( 19 );

   if ( result != 0 )
   {
      fail_msg( "telReceive G_POST flow should return 0; got %d", result );
   }
   if ( makeMessageCallCount != 1 )
   {
      fail_msg( "G_POST flow should call makeMessage once; got %d", makeMessageCallCount );
   }
   if ( arySavedByteCanReplay[19] ||
        arySavedByteCanReplay[20] ||
        arySavedByteCanReplay[21] )
   {
      fail_msg( "G_POST flow should mark the current replay window as non-replayable" );
   }
}

static void telReceive_WhenXMessageEndsAndPendingSend_TriggersSendAnX( void **state )
{
   // Arrange
   (void)state;

   resetState();
   shouldSendExpressMessage = true;
   isExpressMessageInProgress = true;

   // Act
   (void)telReceive( IAC );
   (void)telReceive( XMSG_E );

   // Assert
   if ( filterExpressCallCount == 0 || lastFilterExpressChar != -1 )
   {
      fail_msg( "XMSG_E should signal filterExpress(-1) at end of X transfer" );
   }
   if ( sendAnXCallCount != 1 )
   {
      fail_msg( "XMSG_E with shouldSendExpressMessage should trigger sendAnX once; got %d", sendAnXCallCount );
   }
   if ( shouldSendExpressMessage || isExpressMessageInProgress )
   {
      fail_msg( "XMSG_E should clear pending-send and in-progress flags" );
   }
}

static void telReceive_WhenDataByteReceived_RoutesToCorrectFilter( void **state )
{
   // Arrange
   (void)state;

   resetState();
   whoListProgress = 1;

   // Act
   (void)telReceive( 'A' );

   // Assert
   if ( filterWhoListCallCount != 1 || filterDataCallCount != 0 )
   {
      fail_msg( "when whoListProgress is active, data should route to filterWhoList only" );
   }

   // Arrange
   resetState();
   isExpressMessageInProgress = true;

   // Act
   (void)telReceive( 'B' );

   // Assert
   if ( filterExpressCallCount != 1 || lastFilterExpressChar != 'B' )
   {
      fail_msg( "when express transfer is active, data should route to filterExpress with byte payload" );
   }
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( sendBlock_WhenCalled_WritesIacBlockSequence ),
      cmocka_unit_test( sendNaws_WhenWindowSizeChanged_SendsNawsPayload ),
      cmocka_unit_test( telReceive_WhenClientProbeReceived_RespondsWithClientAck ),
      cmocka_unit_test( telReceive_WhenGetNameCommandArrives_SendsNameResponse ),
      cmocka_unit_test( telReceive_WhenGetStringCommandArrives_MarksExactPromptTriggerByteNonReplayable ),
      cmocka_unit_test( telReceive_WhenPostCommandArrives_MarksReplayWindowNonReplayable ),
      cmocka_unit_test( telReceive_WhenXMessageEndsAndPendingSend_TriggersSendAnX ),
      cmocka_unit_test( telReceive_WhenDataByteReceived_RoutesToCorrectFilter ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
