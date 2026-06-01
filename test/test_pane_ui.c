/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client.h"
#include <cmocka.h>
#include "defs.h"
#include "ext.h"
#include "pane_ui.h"
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

static void readOutput( char *ptrBuffer, size_t bufferSize )
{
   fflush( stdout );
   if ( !tryReadFileIntoBuffer( terminalOutput, ptrBuffer, bufferSize ) )
   {
      fail_msg( "unable to read captured pane UI terminal output" );
   }
}

static void resetOutput( void )
{
   fflush( stdout );
   if ( ftruncate( fileno( terminalOutput ), 0 ) < 0 )
   {
      fail_msg( "unable to truncate captured pane UI terminal output" );
   }
   rewind( terminalOutput );
}

static void feedIncomingText( const char *ptrText, bool expectedHandled )
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

static void readNetOutput( char *ptrBuffer, size_t bufferSize )
{
   if ( !tryReadFileIntoBuffer( netOutputFile, ptrBuffer, bufferSize ) )
   {
      fail_msg( "unable to read captured pane UI network output" );
   }
}

static void feedMouseInput( const char *ptrText )
{
   while ( *ptrText != '\0' )
   {
      assert_true( paneUiHandleLocalInput( *ptrText, ptrText[1] != '\0' ) );
      ptrText++;
   }
}

static void setTerminalSize( int columns )
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

static void formatLocalTimestamp( char *ptrBuffer, size_t bufferSize, time_t timestamp )
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

static int setup( void **state )
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

static int teardown( void **state )
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

static void paneUiEnterIfEligible_WhenWideTerminal_UsesAlternateScreen( void **state )
{
   char aryOutput[4096];

   (void)state;
   paneUiEnterIfEligible();
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_true( paneUiIsActive() );
   assert_int_equal( paneUiTerminalContentColumns(), 0 );
   assert_non_null( strstr( aryOutput, "\033[?1049h" ) );
   assert_non_null( strstr( aryOutput, "\033[?1000h\033[?1006h" ) );
   assert_null( strstr( aryOutput, "|" ) );
   assert_null( strstr( aryOutput, " Online now (stale)" ) );
}

static void paneUiEnterIfEligible_WhenTerminalNarrow_UsesLegacyView( void **state )
{
   (void)state;
   setTerminalSize( 102 );
   paneUiEnterIfEligible();

   assert_false( paneUiIsActive() );
   assert_int_equal( paneUiTerminalContentColumns(), 0 );
}

static void paneUiRefresh_WhenUsernameFits_ClipsRemainingColumns( void **state )
{
   char aryTerminalOutput[8192];
   const char *ptrPrompt = "Lobby>";
   const char *ptrWhoOutput =
      "There is 1 user (0 queued)\r\n"
      "\033[33mUser Name\033[0m           \033[35mTime\033[31m Doing\033[0m"
      "             \033[37m5/30/26 7:10 PM\033[0m\r\n"
      "Alice               #    123:45 Writing\r\n"
      "Lobby> ";
   const char *ptrCursor;

   (void)state;
   setTerminalSize( 103 );
   paneUiEnterIfEligible();
   resetOutput();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   paneUiHandleTimerAt( time( NULL ) );
   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput, "User Name" ) );
   assert_non_null( strstr( aryTerminalOutput, "Alice               #" ) );
   assert_null( strstr( aryTerminalOutput, "Time" ) );
   assert_null( strstr( aryTerminalOutput, "5/30/26" ) );
   assert_null( strstr( aryTerminalOutput, "123:45" ) );
   assert_null( strstr( aryTerminalOutput, "Writing" ) );
}

static void paneUiRefresh_WhenSidebarNarrow_ClipsOriginalLines( void **state )
{
   char aryTerminalOutput[8192];
   const char *ptrPrompt = "Lobby>";
   const char *ptrWhoOutput =
      "There is 1 user (0 queued)\r\n"
      "User Name           Time Doing             5/30/26 7:10 PM\r\n"
      "Alice                   123:45 Writing\r\n"
      "Lobby> ";
   const char *ptrCursor;

   (void)state;
   setTerminalSize( 112 );
   paneUiEnterIfEligible();
   resetOutput();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   paneUiHandleTimerAt( time( NULL ) );
   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput, "Time Doing" ) );
   assert_non_null( strstr( aryTerminalOutput, "123:45" ) );
   assert_null( strstr( aryTerminalOutput, "Writing" ) );
}

static void paneUiRefresh_WhenSidebarWide_ShowsSuccessfulRefreshTimeWithSeconds( void **state )
{
   char aryAfterTimestamp[32];
   char aryBeforeTimestamp[32];
   char aryTerminalOutput[8192];
   const char *ptrCursor;
   const char *ptrPrompt = "Lobby>";
   const char *ptrWhoOutput =
      "There is 1 user (0 queued)\r\n"
      "User Name           Time Doing             5/30/26 7:10 PM\r\n"
      "Alice                   123:45 Writing\r\n"
      "Lobby> ";
   time_t afterTimestamp;
   time_t beforeTimestamp;

   (void)state;
   paneUiEnterIfEligible();
   resetOutput();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   paneUiHandleTimerAt( time( NULL ) );
   beforeTimestamp = time( NULL );
   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   afterTimestamp = time( NULL );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );
   formatLocalTimestamp( aryBeforeTimestamp, sizeof( aryBeforeTimestamp ),
                         beforeTimestamp );
   formatLocalTimestamp( aryAfterTimestamp, sizeof( aryAfterTimestamp ),
                         afterTimestamp );

   assert_null( strstr( aryTerminalOutput, "5/30/26 7:10 PM" ) );
   assert_true( strstr( aryTerminalOutput, aryBeforeTimestamp ) != NULL ||
                strstr( aryTerminalOutput, aryAfterTimestamp ) != NULL );
   assert_non_null( strstr( aryTerminalOutput, "\033[38;5;252m" ) );
}

static void paneUiRefresh_WhenSecondSnapshotCompletes_UpdatesRefreshTime( void **state )
{
   char aryFirstTimestamp[32];
   char arySecondTimestamp[32];
   char aryTerminalOutput[8192];
   const char *ptrCursor;
   const char *ptrPrompt = "Lobby>";
   const char *ptrWhoOutput =
      "There is 1 user (0 queued)\r\n"
      "User Name           Time Doing\r\n"
      "Alice                   123:45 Writing\r\n"
      "Lobby> ";
   time_t firstTimestamp;
   time_t secondTimestamp;

   (void)state;
   paneUiEnterIfEligible();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   paneUiHandleTimerAt( time( NULL ) );
   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   firstTimestamp = time( NULL );
   sleep( 1 );
   resetOutput();
   assert_true( paneUiHandleLocalInput( 'W', false ) );
   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   secondTimestamp = time( NULL );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );
   formatLocalTimestamp( aryFirstTimestamp, sizeof( aryFirstTimestamp ),
                         firstTimestamp );
   formatLocalTimestamp( arySecondTimestamp, sizeof( arySecondTimestamp ),
                         secondTimestamp );

   assert_string_not_equal( aryFirstTimestamp, arySecondTimestamp );
   assert_non_null( strstr( aryTerminalOutput, arySecondTimestamp ) );
}

static void paneUiRefresh_WhenCaptureTimesOut_RetainsRefreshTime( void **state )
{
   char aryTerminalOutput[8192];
   char aryTimestamp[32];
   const char *ptrCursor;
   const char *ptrPrompt = "Lobby>";
   const char *ptrWhoOutput =
      "There is 1 user (0 queued)\r\n"
      "User Name           Time Doing\r\n"
      "Alice                   123:45 Writing\r\n"
      "Lobby> ";
   time_t snapshotTimestamp;

   (void)state;
   paneUiEnterIfEligible();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   paneUiHandleTimerAt( time( NULL ) );
   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   snapshotTimestamp = time( NULL );
   formatLocalTimestamp( aryTimestamp, sizeof( aryTimestamp ),
                         snapshotTimestamp );
   resetOutput();
   assert_true( paneUiHandleLocalInput( 'W', false ) );
   paneUiHandleTimerAt( time( NULL ) + 5 );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput, aryTimestamp ) );
}

static void paneUiEnterIfEligible_WhenScreenReaderEnabled_UsesLegacyView( void **state )
{
   (void)state;
   flagsConfiguration.isScreenReaderModeEnabled = 1;
   paneUiEnterIfEligible();

   assert_false( paneUiIsActive() );
}

static void paneUiLeave_WhenActive_RestoresNormalScreen( void **state )
{
   char aryOutput[4096];

   (void)state;
   paneUiEnterIfEligible();
   resetOutput();
   paneUiLeave();
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_false( paneUiIsActive() );
   assert_non_null( strstr( aryOutput, "\033[?1000l\033[?1006l\033[?1049l" ) );
}

static void paneUiResize_WhenTerminalBecomesNarrow_RepaintsLeftPane( void **state )
{
   char aryOutput[4096];
   const char *ptrLeftText = "Lobby> Left pane text";

   (void)state;
   paneUiEnterIfEligible();
   resetOutput();
   fputs( ptrLeftText, stdout );
   paneUiAfterOutputText( ptrLeftText );
   setTerminalSize( 102 );
   paneUiMarkResizePending();
   paneUiHandleTimer();
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_false( paneUiIsActive() );
   assert_non_null( strstr( aryOutput,
                            "\033[?1049l\033[2J\033[HLobby> Left pane text" ) );
}

static void paneUiLeaveForExit_WhenActive_ClearsRestoredNormalScreen( void **state )
{
   char aryOutput[4096];

   (void)state;
   paneUiEnterIfEligible();
   resetOutput();
   paneUiLeaveForExit();
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_false( paneUiIsActive() );
   assert_non_null( strstr( aryOutput, "\033[?1049l\033[2J\033[H" ) );
}

static void paneUiScroll_WhenMouseWheelMovesOverLeftPane_RedrawsLeftHistory( void **state )
{
   char aryLine[32];
   char aryOutput[32768];
   const char *ptrCursor;
   const char *ptrMouseWheelUp = "\033[<64;10;10M";
   int lineIndex;

   (void)state;
   paneUiEnterIfEligible();
   resetOutput();
   for ( lineIndex = 0; lineIndex < 30; lineIndex++ )
   {
      snprintf( aryLine, sizeof( aryLine ), "Line %02d\r\n", lineIndex );
      fputs( aryLine, stdout );
      paneUiAfterOutputText( aryLine );
   }
   resetOutput();
   for ( ptrCursor = ptrMouseWheelUp; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleLocalInput( *ptrCursor, ptrCursor[1] != '\0' ) );
   }
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_non_null( strstr( aryOutput, "\033[1;1H" ) );
   assert_non_null( strstr( aryOutput, "Line 04" ) );
   assert_null( strstr( aryOutput, "\033[1;81H" ) );
}

static void paneUiScroll_WhenEscapeSequenceIsNotMouse_ReplaysInput( void **state )
{
   int inputChar;

   (void)state;
   paneUiEnterIfEligible();

   assert_true( paneUiHandleLocalInput( '\033', true ) );
   assert_true( paneUiHandleLocalInput( '[', true ) );
   assert_true( paneUiHandleLocalInput( 'A', false ) );
   assert_true( paneUiTakePendingLocalInput( &inputChar ) );
   assert_int_equal( inputChar, '\033' );
   assert_true( paneUiTakePendingLocalInput( &inputChar ) );
   assert_int_equal( inputChar, '[' );
   assert_true( paneUiTakePendingLocalInput( &inputChar ) );
   assert_int_equal( inputChar, 'A' );
   assert_false( paneUiTakePendingLocalInput( &inputChar ) );
}

static void paneUiRefresh_WhenPromptReady_CapturesSidebarAndClipsDoing( void **state )
{
   char aryNetOutput[16];
   char aryTerminalOutput[8192];
   const char *ptrFirstRow;
   const char *ptrPrompt = "[Forum> msg #1] Read cmd ->";
   const char *ptrRowCursor;
   const char *ptrSecondRow;
   const char *ptrSummary;
   const char *ptrWhoOutput =
      "\r\n"
      "There are 2 users (0 queued)\r\n"
      "User Name           Time Doing\r\n"
      "\033[33mAlice\033[0m              0:01 \033[35mWriting an intentionally long activity description with an overflow marker\033[0m\r\n"
      "Bob                0:02 Reading\r\n"
      "[Forum> msg #1] Read cmd ->";
   const char *ptrCursor;

   (void)state;
   paneUiEnterIfEligible();
   resetOutput();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   paneUiHandleTimerAt( time( NULL ) + 5 );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   assert_true( tryReadFileIntoBuffer( netOutputFile, aryNetOutput,
                                       sizeof( aryNetOutput ) ) );
   assert_string_equal( aryNetOutput, "W" );

   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );
   ptrSummary = strstr( aryTerminalOutput, "There are 2 users" );
   assert_non_null( ptrSummary );
   ptrFirstRow = NULL;
   ptrRowCursor = aryTerminalOutput;
   while ( ( ptrRowCursor = strstr( ptrRowCursor, "\033[1;81H" ) ) != NULL &&
           ptrRowCursor < ptrSummary )
   {
      ptrFirstRow = ptrRowCursor++;
   }
   ptrSecondRow = strstr( ptrSummary, "\033[2;81H" );
   assert_non_null( ptrFirstRow );
   assert_non_null( ptrSecondRow );
   assert_true( ptrFirstRow < ptrSummary );
   assert_true( ptrSummary < ptrSecondRow );
   assert_null( strstr( aryTerminalOutput, " Online now" ) );
   assert_non_null( strstr( aryTerminalOutput, "Alice" ) );
   assert_non_null( strstr( aryTerminalOutput,
                            "\033[0;38;5;34;48;5;236mThere are 2 users" ) );
   assert_non_null( strstr( aryTerminalOutput,
                            "\033[38;5;220mAlice\033[0;38;5;34;48;5;236m" ) );
   assert_non_null( strstr( aryTerminalOutput, "\033[38;5;91mWriting" ) );
   assert_non_null( strstr( aryTerminalOutput, "Bob" ) );
   assert_null( strstr( aryTerminalOutput, "overflow marker" ) );
}

static void paneUiRefresh_WhenInitialForumPromptReady_ImmediatelyCapturesSidebar( void **state )
{
   char aryNetOutput[16];
   char aryTerminalOutput[8192];
   const char *ptrPrompt = "\033[33mLobby>\033[0m ";
   const char *ptrWhoOutput =
      "There is 1 user (0 queued)\r\n"
      "User Name           Time Doing\r\n"
      "Alice              0:01 Reading\r\n"
      "Lobby>\033[0m ";
   const char *ptrCursor;

   (void)state;
   paneUiEnterIfEligible();
   resetOutput();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );
   assert_int_equal( paneUiTerminalContentColumns(), 80 );
   assert_null( strstr( aryTerminalOutput, " Online now" ) );
   resetOutput();
   paneUiHandleTimerAt( time( NULL ) );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   assert_true( tryReadFileIntoBuffer( netOutputFile, aryNetOutput,
                                       sizeof( aryNetOutput ) ) );
   assert_string_equal( aryNetOutput, "W" );

   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );
   assert_non_null( strstr( aryTerminalOutput, "Alice" ) );
   assert_null( strstr( aryTerminalOutput, " Online now" ) );
}

static void paneUiRefresh_WhenUserTyped_DoesNotInjectWhoCommand( void **state )
{
   char aryNetOutput[16];
   const char *ptrPrompt = "Read cmd ->";
   const char *ptrCursor;

   (void)state;
   paneUiEnterIfEligible();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      (void)paneUiHandleIncomingChar( *ptrCursor );
   }
   paneUiNoteUserInput();
   paneUiHandleTimerAt( time( NULL ) + 5 );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   assert_true( tryReadFileIntoBuffer( netOutputFile, aryNetOutput,
                                       sizeof( aryNetOutput ) ) );
   assert_string_equal( aryNetOutput, "" );
}

static void paneUiRefresh_WhenWhoInjected_TracksNonReplayableProtocolByte( void **state )
{
   const char *ptrPrompt = "Lobby>";
   const char *ptrCursor;

   (void)state;
   paneUiEnterIfEligible();
   byte = 18;
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   paneUiHandleTimerAt( time( NULL ) );

   assert_int_equal( byte, 19 );
   assert_int_equal( arySavedBytes[18], 'W' );
   assert_false( arySavedByteCanReplay[18] );
}

static void paneUiRefresh_WhenUserTypesWhoAtSafePrompt_CapturesSidebar( void **state )
{
   char aryNetOutput[16];
   char aryTerminalOutput[8192];
   const char *ptrPrompt = "Lobby>";
   const char *ptrWhoOutput =
      "There is 1 user (0 queued)\r\n"
      "User Name           Time Doing\r\n"
      "Alice              0:01 Reading\r\n"
      "Lobby> ";
   const char *ptrCursor;

   (void)state;
   paneUiEnterIfEligible();
   resetOutput();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   assert_true( paneUiHandleLocalInput( 'W', false ) );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   assert_true( tryReadFileIntoBuffer( netOutputFile, aryNetOutput,
                                       sizeof( aryNetOutput ) ) );
   assert_string_equal( aryNetOutput, "W" );

   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );
   assert_non_null( strstr( aryTerminalOutput, "Alice" ) );
}

static void paneUiRefresh_WhenUserTypesWhoWithoutSafePrompt_LeavesInputUnhandled( void **state )
{
   char aryNetOutput[16];

   (void)state;
   paneUiEnterIfEligible();
   assert_false( paneUiHandleLocalInput( 'W', false ) );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   assert_true( tryReadFileIntoBuffer( netOutputFile, aryNetOutput,
                                       sizeof( aryNetOutput ) ) );
   assert_string_equal( aryNetOutput, "" );
}

static void paneUiRefresh_WhenLowercaseWhoDefaultsToLongWho_CapturesSidebar( void **state )
{
   char aryNetOutput[16];
   const char *ptrPrompt = "Lobby>";
   const char *ptrCursor;

   (void)state;
   paneUiEnterIfEligible();
   aryKeyMap['w'] = 'W';
   aryKeyMap['W'] = 'w';
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   assert_true( paneUiHandleLocalInput( 'w', false ) );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   assert_true( tryReadFileIntoBuffer( netOutputFile, aryNetOutput,
                                       sizeof( aryNetOutput ) ) );
   assert_string_equal( aryNetOutput, "W" );
}

static void paneUiRefresh_WhenLowercaseWhoDefaultsToShortWho_LeavesInputUnhandled( void **state )
{
   char aryNetOutput[16];
   const char *ptrPrompt = "Lobby>";
   const char *ptrCursor;

   (void)state;
   paneUiEnterIfEligible();
   aryKeyMap['w'] = 'w';
   aryKeyMap['W'] = 'W';
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   assert_false( paneUiHandleLocalInput( 'w', false ) );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   assert_true( tryReadFileIntoBuffer( netOutputFile, aryNetOutput,
                                       sizeof( aryNetOutput ) ) );
   assert_string_equal( aryNetOutput, "" );
}

static void paneUiRefresh_WhenPostingOrPaging_DoesNotInjectWhoCommand( void **state )
{
   char aryNetOutput[16];
   const char *ptrPrompt = "Read cmd ->";
   const char *ptrCursor;

   (void)state;
   paneUiEnterIfEligible();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      (void)paneUiHandleIncomingChar( *ptrCursor );
   }
   flagsConfiguration.isPosting = 1;
   paneUiHandleTimerAt( time( NULL ) + 5 );
   flagsConfiguration.isPosting = 0;
   flagsConfiguration.isMorePromptActive = 1;
   paneUiHandleTimerAt( time( NULL ) + 10 );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   assert_true( tryReadFileIntoBuffer( netOutputFile, aryNetOutput,
                                       sizeof( aryNetOutput ) ) );
   assert_string_equal( aryNetOutput, "" );
}

static void paneUiRefresh_WhenCaptureTimesOut_DoesNotSendCatchUpBurst( void **state )
{
   char aryNetOutput[16];
   const char *ptrPrompt = "Read cmd ->";
   const char *ptrCursor;
   time_t refreshTime;

   (void)state;
   paneUiEnterIfEligible();
   resetOutput();
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      (void)paneUiHandleIncomingChar( *ptrCursor );
   }
   refreshTime = time( NULL ) + 5;
   paneUiHandleTimerAt( refreshTime );
   paneUiHandleTimerAt( refreshTime + 5 );
   paneUiHandleTimerAt( refreshTime + 10 );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   assert_true( tryReadFileIntoBuffer( netOutputFile, aryNetOutput,
                                       sizeof( aryNetOutput ) ) );
   assert_string_equal( aryNetOutput, "W" );
}

static void paneUiView_WhenUserTypesHelpAtSafePrompt_CapturesHiddenSidebar( void **state )
{
   char aryNetOutput[16];
   char aryTerminalOutput[8192];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );

   assert_true( paneUiHandleLocalInput( '?', false ) );
   feedIncomingText( "Help first\r\nHelp second\r\nLobby> ", true );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_string_equal( aryNetOutput, "?" );
   assert_non_null( strstr( aryTerminalOutput, "Help first" ) );
   assert_non_null( strstr( aryTerminalOutput, "Help second" ) );
   assert_null( strstr( aryTerminalOutput, "Lobby>" ) );
}

static void paneUiView_WhenUserTypesAidesAtSafePrompt_CapturesHiddenSidebar( void **state )
{
   char aryNetOutput[16];
   char aryTerminalOutput[8192];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );

   assert_true( paneUiHandleLocalInput( '@', false ) );
   feedIncomingText( "Sysops\r\nRoomaides\r\nLobby> ", true );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_string_equal( aryNetOutput, "@" );
   assert_non_null( strstr( aryTerminalOutput, "Sysops" ) );
   assert_non_null( strstr( aryTerminalOutput, "Roomaides" ) );
}

static void paneUiView_WhenPromptIsUnsafe_LeavesHelpAndAidesUnhandled( void **state )
{
   char aryNetOutput[16];

   (void)state;
   paneUiEnterIfEligible();

   assert_false( paneUiHandleLocalInput( '?', false ) );
   assert_false( paneUiHandleLocalInput( '@', false ) );
   assert_false( paneUiHandleLocalInput( 'i', false ) );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   assert_string_equal( aryNetOutput, "" );
}

static void paneUiView_WhenUserTypesForumInfo_PrependsCurrentForumName( void **state )
{
   char aryNetOutput[16];
   char aryTerminalOutput[8192];
   const char *ptrBody;
   const char *ptrTitle;

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "[Intel PCs And Clones> msg #12795] Read cmd ->", false );

   assert_true( paneUiHandleLocalInput( 'i', false ) );
   paneUiHandleMorePromptStateChanged( true );
   paneUiHandleMorePromptStateChanged( false );
   feedIncomingText(
      "\033[32mForum moderator is Big Dan.\033[0m\r\n"
      "--MORE--(12%)\r\n"
      "\033[35mForum information body.\033[0m\r\n"
      "Intel PCs And Clones> ",
      true );
   paneUiHandleNetworkIdle();
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );
   ptrTitle = strstr( aryTerminalOutput, "Intel PCs And Clones" );
   ptrBody = strstr( aryTerminalOutput, "Forum moderator is Big Dan." );

   assert_string_equal( aryNetOutput, "i " );
   assert_non_null( ptrTitle );
   assert_non_null( ptrBody );
   assert_true( ptrTitle < ptrBody );
   assert_non_null( strstr( aryTerminalOutput,
                            "\033[38;5;91mForum information body." ) );
   assert_null( strstr( aryTerminalOutput, "--MORE--" ) );
}

static void paneUiView_WhenLobbyInfoDocumentsCommandPrompt_KeepsCapturing( void **state )
{
   char aryTerminalOutput[8192];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );

   assert_true( paneUiHandleLocalInput( 'i', false ) );
   feedIncomingText( "Forum Info\r\n", true );
   feedIncomingText( "Forum moderator is (Sysop).\r\n", true );
   feedIncomingText( "Documentation mentions Read cmd -> Next command.\r\n",
                     true );
   feedIncomingText( "Still part of Lobby forum information.\r\n", true );
   feedIncomingText( "Lobby> ", true );
   paneUiHandleNetworkIdle();
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput, "Lobby" ) );
   assert_non_null( strstr( aryTerminalOutput, "Still part of Lobby" ) );
}

static void paneUiView_WhenForumInfoHasPreliminaryPrompt_WaitsForBody( void **state )
{
   char aryTerminalOutput[8192];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );

   assert_true( paneUiHandleLocalInput( 'i', false ) );
   feedIncomingText( "Forum Info\r\nLobby> \r\n", true );
   feedIncomingText( "Forum moderator is (Sysop).\r\n", true );
   paneUiHandleMorePromptStateChanged( true );
   paneUiHandleMorePromptStateChanged( false );
   feedIncomingText( "Lobby forum information.\r\nLobby> ", true );
   paneUiHandleNetworkIdle();
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput, "Forum moderator is (Sysop)." ) );
   assert_non_null( strstr( aryTerminalOutput, "Lobby forum information." ) );
}

static void paneUiView_WhenUserTypesUppercaseForumInfo_CapturesSidebar( void **state )
{
   char aryNetOutput[16];
   char aryTerminalOutput[8192];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Weird>", false );

   assert_true( paneUiHandleLocalInput( 'I', false ) );
   feedIncomingText(
      "Forum moderator is KAM.\r\n"
      "Welcome to Weird! You are encouraged to be WEIRD!\r\n"
      "Weird> ",
      true );
   paneUiHandleNetworkIdle();
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_string_equal( aryNetOutput, "i" );
   assert_non_null( strstr( aryTerminalOutput, "Weird" ) );
   assert_non_null( strstr( aryTerminalOutput, "Welcome to Weird!" ) );
}

static void paneUiView_WhenForumInfoBodyStartsWithPromptText_KeepsCapturing( void **state )
{
   char aryTerminalOutput[8192];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Weird>", false );

   assert_true( paneUiHandleLocalInput( 'i', false ) );
   feedIncomingText(
      "Forum moderator is KAM.\r\n"
      "Weird> even if they have been self-deleted.\r\n"
      "Still part of Weird forum information.\r\n"
      "Weird> ",
      true );
   paneUiHandleNetworkIdle();
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput,
                            "Weird> even if they have been self-deleted." ) );
   assert_non_null( strstr( aryTerminalOutput,
                            "Still part of Weird forum information." ) );
}

static void paneUiView_WhenPagedHelpCompletes_ClearsStaleMoreState( void **state )
{
   char aryNetOutput[16];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );

   assert_true( paneUiHandleLocalInput( '?', false ) );
   flagsConfiguration.isMorePromptActive = true;
   paneUiHandleMorePromptStateChanged( true );
   feedIncomingText( "Help contents\r\nLobby> ", true );

   assert_false( flagsConfiguration.isMorePromptActive );
   assert_true( paneUiHandleLocalInput( 'i', false ) );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   assert_string_equal( aryNetOutput, "? i" );
}

static void paneUiView_WhenCapturedResponsePages_SendsHiddenSpaces( void **state )
{
   char aryNetOutput[16];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );
   byte = 18;

   assert_true( paneUiHandleLocalInput( '?', false ) );
   paneUiHandleMorePromptStateChanged( true );
   paneUiHandleMorePromptStateChanged( false );
   paneUiHandleMorePromptStateChanged( true );
   assert_true( paneUiHandleLocalInput( 'x', false ) );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );

   assert_string_equal( aryNetOutput, "?  " );
   assert_int_equal( byte, 21 );
   assert_int_equal( arySavedBytes[18], '?' );
   assert_int_equal( arySavedBytes[19], ' ' );
   assert_int_equal( arySavedBytes[20], ' ' );
   assert_false( arySavedByteCanReplay[18] );
   assert_false( arySavedByteCanReplay[19] );
   assert_false( arySavedByteCanReplay[20] );
}

static void paneUiView_WhenPagedResponseContainsMoreMarker_StripsOnlyMarker( void **state )
{
   char aryTerminalOutput[8192];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );

   assert_true( paneUiHandleLocalInput( '@', false ) );
   feedIncomingText( "Before marker\r\n--MORE--(112%)After marker\r\nLobby> ",
                     true );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput, "Before marker" ) );
   assert_non_null( strstr( aryTerminalOutput, "After marker" ) );
   assert_null( strstr( aryTerminalOutput, "--MORE--" ) );
}

static void paneUiView_WhenPagedResponseOverwritesPrompt_KeepsAidesAligned( void **state )
{
   char aryTerminalOutput[8192];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );

   assert_true( paneUiHandleLocalInput( '@', false ) );
   feedIncomingText(
      "--MORE--(112%)        \rBiological Sciences\r\n"
      "Elon xxxx\b\b\b\bMusk\r\nLobby> ",
      true );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput, "Biological Sciences" ) );
   assert_non_null( strstr( aryTerminalOutput, "Elon Musk" ) );
   assert_null( strstr( aryTerminalOutput, "        Biological Sciences" ) );
   assert_null( strstr( aryTerminalOutput, "--MORE--" ) );
   assert_null( strstr( aryTerminalOutput, "Elon xxxx" ) );
}

static void paneUiView_WhenHelpPinned_DoesNotRefreshUntilWhoSelected( void **state )
{
   char aryNetOutput[16];
   time_t futureTime;

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );
   assert_true( paneUiHandleLocalInput( '?', false ) );
   feedIncomingText( "Help\r\nLobby> ", true );
   futureTime = time( NULL ) + 30;

   paneUiHandleTimerAt( futureTime );
   assert_true( paneUiHandleLocalInput( 'W', false ) );
   feedIncomingText( "Who\r\nLobby> ", true );
   paneUiHandleTimerAt( futureTime );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );

   assert_string_equal( aryNetOutput, "?WW" );
}

static void paneUiScroll_WhenMouseWheelMovesOverSidebar_RedrawsRightViewport( void **state )
{
   char aryLine[32];
   char aryTerminalOutput[32768];
   int lineIndex;

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );
   assert_true( paneUiHandleLocalInput( '?', false ) );
   for ( lineIndex = 0; lineIndex < 30; lineIndex++ )
   {
      snprintf( aryLine, sizeof( aryLine ), "%sHelp line %02d\r\n",
                lineIndex == 0 ? "\033[33m" : "", lineIndex );
      feedIncomingText( aryLine, true );
   }
   feedIncomingText( "Lobby> ", true );
   resetOutput();

   feedMouseInput( "\033[<65;90;10M" );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput, "Help line 03" ) );
   assert_non_null( strstr( aryTerminalOutput,
                            "\033[0;38;5;220;48;5;236mHelp line 03" ) );
   assert_non_null( strstr( aryTerminalOutput, "Help lines 4-26 of 30" ) );
   assert_null( strstr( aryTerminalOutput, "\033[1;1H" ) );
}

static void paneUiView_WhenResponseExceedsLimit_MarksFooterTruncated( void **state )
{
   char aryLine[32];
   char aryTerminalOutput[32768];
   int lineIndex;

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );
   assert_true( paneUiHandleLocalInput( '?', false ) );
   for ( lineIndex = 0; lineIndex < 1001; lineIndex++ )
   {
      snprintf( aryLine, sizeof( aryLine ), "Help line %04d\r\n", lineIndex );
      feedIncomingText( aryLine, true );
   }
   resetOutput();
   feedIncomingText( "Lobby> ", true );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_non_null( strstr( aryTerminalOutput,
                            "Help lines 1-23 of 1000 truncated" ) );
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test_setup_teardown(
         paneUiEnterIfEligible_WhenWideTerminal_UsesAlternateScreen, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiEnterIfEligible_WhenTerminalNarrow_UsesLegacyView, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenUsernameFits_ClipsRemainingColumns, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenSidebarNarrow_ClipsOriginalLines, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenSidebarWide_ShowsSuccessfulRefreshTimeWithSeconds, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenSecondSnapshotCompletes_UpdatesRefreshTime, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenCaptureTimesOut_RetainsRefreshTime, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiEnterIfEligible_WhenScreenReaderEnabled_UsesLegacyView, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiLeave_WhenActive_RestoresNormalScreen, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiResize_WhenTerminalBecomesNarrow_RepaintsLeftPane, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiLeaveForExit_WhenActive_ClearsRestoredNormalScreen, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiScroll_WhenMouseWheelMovesOverLeftPane_RedrawsLeftHistory, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiScroll_WhenEscapeSequenceIsNotMouse_ReplaysInput, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenPromptReady_CapturesSidebarAndClipsDoing, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenInitialForumPromptReady_ImmediatelyCapturesSidebar, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenUserTyped_DoesNotInjectWhoCommand, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenWhoInjected_TracksNonReplayableProtocolByte, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenUserTypesWhoAtSafePrompt_CapturesSidebar, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenUserTypesWhoWithoutSafePrompt_LeavesInputUnhandled, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenLowercaseWhoDefaultsToLongWho_CapturesSidebar, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenLowercaseWhoDefaultsToShortWho_LeavesInputUnhandled, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenPostingOrPaging_DoesNotInjectWhoCommand, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenCaptureTimesOut_DoesNotSendCatchUpBurst, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenUserTypesHelpAtSafePrompt_CapturesHiddenSidebar, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenUserTypesAidesAtSafePrompt_CapturesHiddenSidebar, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenPromptIsUnsafe_LeavesHelpAndAidesUnhandled, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenUserTypesForumInfo_PrependsCurrentForumName, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenLobbyInfoDocumentsCommandPrompt_KeepsCapturing, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenForumInfoHasPreliminaryPrompt_WaitsForBody, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenUserTypesUppercaseForumInfo_CapturesSidebar, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenForumInfoBodyStartsWithPromptText_KeepsCapturing, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenPagedHelpCompletes_ClearsStaleMoreState, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenCapturedResponsePages_SendsHiddenSpaces, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenPagedResponseContainsMoreMarker_StripsOnlyMarker, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenPagedResponseOverwritesPrompt_KeepsAidesAligned, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenHelpPinned_DoesNotRefreshUntilWhoSelected, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiScroll_WhenMouseWheelMovesOverSidebar_RedrawsRightViewport, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenResponseExceedsLimit_MarksFooterTruncated, setup,
         teardown ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
