/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config_file.h"
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
static int tempFileErrorCallCount;

static void resetState( void )
{
   fatalPerrorCallCount = 0;
   tempFileErrorCallCount = 0;

   capture = 0;
   flagsConfiguration.isPosting = 0;
   flagsConfiguration.isMorePromptActive = 0;
   flagsConfiguration.shouldDisableBold = 0;
   lastColor = 0;

   if ( tempFile != NULL )
   {
      fclose( tempFile );
      tempFile = NULL;
   }
   if ( netOutputFile != NULL )
   {
      fclose( netOutputFile );
      netOutputFile = NULL;
   }
}

// sysio.c dependency stubs.
noreturn void fatalPerror( const char *message, const char *heading )
{
   (void)message;
   (void)heading;
   fatalPerrorCallCount++;
   abort();
}

void tempFileError( void )
{
   tempFileErrorCallCount++;
}

int colorValueFromLegacyDigit( int inputChar )
{
   if ( inputChar >= '0' && inputChar <= '9' )
   {
      return inputChar - '0';
   }

   return inputChar;
}

static void stripAnsi_WhenEscapeCodesPresent_RemovesAnsiSequences( void **state )
{
   // Arrange
   char aryText[128];

   (void)state;
   resetState();
   snprintf( aryText, sizeof( aryText ), "%s", "\033[32mHello\033[0m World" );

   // Act
   stripAnsi( aryText, sizeof( aryText ) );

   // Assert
   if ( strcmp( aryText, "Hello World" ) != 0 )
   {
      fail_msg( "stripAnsi should remove ANSI escapes; got '%s'", aryText );
   }
}

static void capPuts_WhenCaptureEnabled_WritesStrippedTextToTempFile( void **state )
{
   // Arrange
   char aryCaptured[256];

   (void)state;
   resetState();
   capture = 1;
   tempFile = tmpfile();
   if ( tempFile == NULL )
   {
      fail_msg( "tmpfile failed in capPuts capture test setup" );
   }

   // Act
   capPuts( "\033[31mAlert\033[0m message" );

   // Assert
   memset( aryCaptured, 0, sizeof( aryCaptured ) );
   if ( !tryReadFileIntoBuffer( tempFile, aryCaptured, sizeof( aryCaptured ) ) )
   {
      resetState();
      fail_msg( "Assert failed: unable to read captured output in capPuts test" );
      return;
   }
   if ( strcmp( aryCaptured, "Alert message" ) != 0 )
   {
      fail_msg( "capPuts should capture stripped text; got '%s'", aryCaptured );
   }
   if ( tempFileErrorCallCount != 0 )
   {
      fail_msg( "capPuts should not trigger tempFileError on normal write; got %d calls", tempFileErrorCallCount );
   }

   resetState();
}

static void capPutChar_WhenAnsiSequenceProvided_SkipsAnsiAndTracksLastColor( void **state )
{
   // Arrange
   char aryCaptured[64];
   const int arySequence[] = { '\033', '[', '3', '6', 'm', 'X' };
   size_t itemIndex;

   (void)state;
   resetState();
   capture = 1;
   tempFile = tmpfile();
   if ( tempFile == NULL )
   {
      fail_msg( "tmpfile failed in capPutChar ANSI test setup" );
   }

   // Act
   for ( itemIndex = 0; itemIndex < sizeof( arySequence ) / sizeof( arySequence[0] ); ++itemIndex )
   {
      capPutChar( arySequence[itemIndex] );
   }

   // Assert
   memset( aryCaptured, 0, sizeof( aryCaptured ) );
   if ( !tryReadFileIntoBuffer( tempFile, aryCaptured, sizeof( aryCaptured ) ) )
   {
      resetState();
      fail_msg( "Assert failed: unable to read captured output in capPutChar ANSI test" );
      return;
   }
   if ( strcmp( aryCaptured, "X" ) != 0 )
   {
      fail_msg( "capPutChar should not capture ANSI bytes; expected only payload 'X', got '%s'", aryCaptured );
   }
   if ( lastColor != 6 )
   {
      fail_msg( "capPutChar should update lastColor from ANSI code; expected 6, got %d", lastColor );
   }

   resetState();
}

static void capPutChar_WhenCarriageReturnReceived_DoesNotCaptureIt( void **state )
{
   // Arrange
   char aryCaptured[64];

   (void)state;
   resetState();
   capture = 1;
   tempFile = tmpfile();
   if ( tempFile == NULL )
   {
      fail_msg( "tmpfile failed in carriage-return capture test setup" );
   }

   // Act
   capPutChar( 'A' );
   capPutChar( '\r' );
   capPutChar( 'B' );

   // Assert
   memset( aryCaptured, 0, sizeof( aryCaptured ) );
   if ( !tryReadFileIntoBuffer( tempFile, aryCaptured, sizeof( aryCaptured ) ) )
   {
      resetState();
      fail_msg( "Assert failed: unable to read captured output in capPutChar carriage-return test" );
      return;
   }
   if ( strcmp( aryCaptured, "AB" ) != 0 )
   {
      fail_msg( "capPutChar should skip carriage return while capturing; got '%s'", aryCaptured );
   }

   resetState();
}

static void netPuts_WhenOutputFileSet_WritesAllCharacters( void **state )
{
   // Arrange
   char aryCaptured[64];

   (void)state;
   resetState();
   netOutputFile = tmpfile();
   if ( netOutputFile == NULL )
   {
      fail_msg( "tmpfile failed in netPuts test setup" );
   }

   // Act
   netPuts( "Watson" );

   // Assert
   fflush( netOutputFile );
   memset( aryCaptured, 0, sizeof( aryCaptured ) );
   if ( !tryReadFileIntoBuffer( netOutputFile, aryCaptured, sizeof( aryCaptured ) ) )
   {
      resetState();
      fail_msg( "Assert failed: unable to read network output in netPuts test" );
      return;
   }
   if ( strcmp( aryCaptured, "Watson" ) != 0 )
   {
      fail_msg( "netPuts should write full string to netOutputFile; got '%s'", aryCaptured );
   }

   resetState();
}

static void netPrintf_WhenFormattingText_WritesFormattedOutput( void **state )
{
   // Arrange
   char aryCaptured[64];

   (void)state;
   resetState();
   netOutputFile = tmpfile();
   if ( netOutputFile == NULL )
   {
      fail_msg( "tmpfile failed in netPrintf test setup" );
   }

   // Act
   netPrintf( "msg#%d", 42 );

   // Assert
   fflush( netOutputFile );
   memset( aryCaptured, 0, sizeof( aryCaptured ) );
   if ( !tryReadFileIntoBuffer( netOutputFile, aryCaptured, sizeof( aryCaptured ) ) )
   {
      resetState();
      fail_msg( "Assert failed: unable to read network output in netPrintf test" );
      return;
   }
   if ( strcmp( aryCaptured, "msg#42" ) != 0 )
   {
      fail_msg( "netPrintf should write formatted string; got '%s'", aryCaptured );
   }

   resetState();
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( stripAnsi_WhenEscapeCodesPresent_RemovesAnsiSequences ),
      cmocka_unit_test( capPuts_WhenCaptureEnabled_WritesStrippedTextToTempFile ),
      cmocka_unit_test( capPutChar_WhenAnsiSequenceProvided_SkipsAnsiAndTracksLastColor ),
      cmocka_unit_test( capPutChar_WhenCarriageReturnReceived_DoesNotCaptureIt ),
      cmocka_unit_test( netPuts_WhenOutputFileSet_WritesAllCharacters ),
      cmocka_unit_test( netPrintf_WhenFormattingText_WritesFormattedOutput ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
