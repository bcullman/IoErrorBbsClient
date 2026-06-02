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
   char aryExpectedAfterSequence[64];
   char aryExpectedBeforeSequence[64];
   char aryTerminalOutput[8192];
   const char *ptrCursor;
   const char *ptrPrompt = "Lobby>";
   const char *ptrWhoOutput =
      "There is 1 user (0 queued)\r\n"
      "User Name           Time Doing             5/30/26 7:10 PM\r\n"
      "----------------------------------------\r\n"
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
   snprintf( aryExpectedBeforeSequence, sizeof( aryExpectedBeforeSequence ),
             "\033[2;%zuH", 122 - strlen( aryBeforeTimestamp ) + 1 );
   snprintf( aryExpectedAfterSequence, sizeof( aryExpectedAfterSequence ),
             "\033[2;%zuH", 122 - strlen( aryAfterTimestamp ) + 1 );

   assert_null( strstr( aryTerminalOutput, "5/30/26 7:10 PM" ) );
   assert_true( strstr( aryTerminalOutput, aryBeforeTimestamp ) != NULL ||
                strstr( aryTerminalOutput, aryAfterTimestamp ) != NULL );
   assert_true( strstr( aryTerminalOutput, aryExpectedBeforeSequence ) != NULL ||
                strstr( aryTerminalOutput, aryExpectedAfterSequence ) != NULL );
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
         paneUiRefresh_WhenUsernameFits_ClipsRemainingColumns, setup, teardown ),
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
         paneUiScroll_WhenMouseWheelMovesOverSidebar_RedrawsRightViewport, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenResponseExceedsLimit_MarksFooterTruncated, setup, teardown ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
