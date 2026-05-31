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
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
