/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client.h"
#include <cmocka.h>
#include "defs.h"
#include "ext.h"
#include "pane_ui.h"
#include "pane_ui_test_helpers.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include "test_helpers.h"
#include "unix.h"
#include <util.h>

static int savedStdin;
static int savedStdout;
static int terminalInput;
static int terminalMaster;
static FILE *terminalOutput;

int netPutChar( int inputChar )
{
   return putc( inputChar, netOutputFile );
}

void sendTrackedCharWithoutReplay( int inputChar )
{
   netPutChar( inputChar );
   arySavedBytes[byte] = (char)inputChar;
   arySavedByteCanReplay[byte] = false;
   byte++;
}

void readOutput( char *ptrBuffer, size_t bufferSize )
{
   fflush( stdout );
   if ( !tryReadFileIntoBuffer( terminalOutput, ptrBuffer, bufferSize ) )
   {
      fail_msg( "unable to read captured pane UI terminal output" );
   }
}

void resetOutput( void )
{
   fflush( stdout );
   if ( ftruncate( fileno( terminalOutput ), 0 ) < 0 )
   {
      fail_msg( "unable to truncate captured pane UI terminal output" );
   }
   rewind( terminalOutput );
}

void feedIncomingText( const char *ptrText, bool expectedHandled )
{
   while ( *ptrText != '\0' )
   {
      int inputChar = *ptrText++;
      bool handled = paneUiHandleIncomingChar( inputChar );

      if ( handled != expectedHandled )
      {
         fail_msg( "incoming character %d ('%c') handled=%d, expected=%d, "
                   "remaining input: %s",
                   inputChar, inputChar, handled, expectedHandled, ptrText );
      }
   }
}

void readNetOutput( char *ptrBuffer, size_t bufferSize )
{
   if ( !tryReadFileIntoBuffer( netOutputFile, ptrBuffer, bufferSize ) )
   {
      fail_msg( "unable to read captured pane UI network output" );
   }
}

void feedMouseInput( const char *ptrText )
{
   while ( *ptrText != '\0' )
   {
      assert_true( paneUiHandleLocalInput( *ptrText, ptrText[1] != '\0' ) );
      ptrText++;
   }
}

void setTerminalSize( int columns )
{
   struct winsize terminalSize;

   memset( &terminalSize, 0, sizeof( terminalSize ) );
   terminalSize.ws_row = 24;
   terminalSize.ws_col = (unsigned short)columns;
   if ( ioctl( terminalInput, TIOCSWINSZ, &terminalSize ) < 0 )
   {
      fail_msg( "unable to configure pane UI test terminal size" );
   }
}

void formatLocalTimestamp( char *ptrBuffer, size_t bufferSize, time_t timestamp )
{
   struct tm localTime;
   int hour;

   localtime_r( &timestamp, &localTime );
   hour = localTime.tm_hour % 12;
   if ( hour == 0 )
   {
      hour = 12;
   }
   snprintf( ptrBuffer, bufferSize, "%d/%d/%02d %d:%02d:%02d %s",
             localTime.tm_mon + 1, localTime.tm_mday,
             ( localTime.tm_year + 1900 ) % 100, hour,
             localTime.tm_min, localTime.tm_sec,
             localTime.tm_hour < 12 ? "AM" : "PM" );
}

int setup( void **state )
{
   int savedByteIndex;
   struct winsize terminalSize;

   (void)state;
   paneUiLeave();
   paneUiResetSession();
   savedStdin = dup( STDIN_FILENO );
   savedStdout = dup( STDOUT_FILENO );
   memset( &terminalSize, 0, sizeof( terminalSize ) );
   terminalSize.ws_row = 24;
   terminalSize.ws_col = 146;
   terminalMaster = -1;
   terminalInput = -1;
   if ( openpty( &terminalMaster, &terminalInput, NULL, NULL, &terminalSize ) < 0 )
   {
      fail_msg( "unable to prepare pane UI test terminal" );
      return -1;
   }
   terminalOutput = tmpfile();
   netOutputFile = tmpfile();
   if ( savedStdin < 0 || savedStdout < 0 ||
        terminalOutput == NULL || netOutputFile == NULL )
   {
      fail_msg( "unable to prepare pane UI test descriptors" );
      return -1;
   }
   if ( dup2( terminalInput, STDIN_FILENO ) < 0 ||
        dup2( fileno( terminalOutput ), STDOUT_FILENO ) < 0 )
   {
      fail_msg( "unable to redirect pane UI test descriptors" );
      return -1;
   }
   setTerminalSize( 146 );
   flagsConfiguration.shouldUsePaneUi = 1;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   flagsConfiguration.isPosting = 0;
   flagsConfiguration.isMorePromptActive = 0;
   flagsConfiguration.shouldCheckExpress = 0;
   color.text = 34;
   color.forum = 220;
   color.ansiMagentaTextColor = 91;
   color.ansiWhiteTextColor = 252;
   color.background = 236;
   aryKeyMap['w'] = 'w';
   aryKeyMap['W'] = 'W';
   byte = 0;
   for ( savedByteIndex = 0;
         savedByteIndex < (int)sizeof arySavedByteCanReplay;
         savedByteIndex++ )
   {
      arySavedByteCanReplay[savedByteIndex] = false;
      arySavedBytes[savedByteIndex] = 0;
   }
   childPid = 0;
   return 0;
}

int teardown( void **state )
{
   (void)state;
   paneUiLeave();
   fflush( stdout );
   dup2( savedStdin, STDIN_FILENO );
   dup2( savedStdout, STDOUT_FILENO );
   close( savedStdin );
   close( savedStdout );
   close( terminalMaster );
   close( terminalInput );
   fclose( terminalOutput );
   fclose( netOutputFile );
   terminalOutput = NULL;
   netOutputFile = NULL;
   return 0;
}
