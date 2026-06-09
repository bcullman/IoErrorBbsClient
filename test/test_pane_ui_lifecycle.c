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

static void paneUiPrepareLocalRedraw_WhenActive_ClearsLeftPane( void **state )
{
   char aryLine[32];
   char aryOutput[32768];
   int lineIndex;

   (void)state;
   paneUiEnterIfEligible();
   for ( lineIndex = 0; lineIndex < 30; lineIndex++ )
   {
      snprintf( aryLine, sizeof( aryLine ), "Old left text %02d\r\n", lineIndex );
      paneUiAfterOutputText( aryLine );
   }
   resetOutput();

   paneUiPrepareLocalRedraw();
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_true( paneUiIsActive() );
   assert_non_null( strstr( aryOutput, "\033[1;1H" ) );
   assert_non_null( strstr( aryOutput, "\033[80X" ) );
   assert_null( strstr( aryOutput, "Old left text" ) );

   paneUiAfterOutputText( "New local text\r\n" );
   resetOutput();
   feedMouseInput( "\033[<64;10;10M" );
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_non_null( strstr( aryOutput, "Old left text" ) );
}

static void paneUiWriteLocalOutput_WhenPosting_KeepsPaneLayoutAndScrollback( void **state )
{
   char aryOutput[32768];
   char aryLine[32];
   const char *ptrCursor;
   const char *ptrPrompt = "Lobby>";
   const char *ptrWhoOutput =
      "There are 1 user (0 queued)\r\n"
      "User Name           Time Doing\r\n"
      "Alice              0:01 Reading\r\n"
      "Lobby> ";

   (void)state;
   paneUiEnterIfEligible();
   for ( int lineIndex = 0; lineIndex < 30; lineIndex++ )
   {
      snprintf( aryLine, sizeof( aryLine ), "Before draft %02d\r\n", lineIndex );
      paneUiAfterOutputText( aryLine );
   }
   for ( ptrCursor = ptrPrompt; *ptrCursor; ptrCursor++ )
   {
      assert_false( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   paneUiHandleTimerAt( time( NULL ) );
   for ( ptrCursor = ptrWhoOutput; *ptrCursor; ptrCursor++ )
   {
      assert_true( paneUiHandleIncomingChar( *ptrCursor ) );
   }
   resetOutput();

   flagsConfiguration.isPosting = 1;
   assert_true( paneUiWriteLocalOutputText(
      "Draft header\r\n"
      "This local draft line should stay on the left side while the sidebar remains visible.\r\n" ) );
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_non_null( strstr( aryOutput, "Draft header" ) );
   assert_non_null( strstr( aryOutput, "\033[1;81H" ) );
   assert_non_null( strstr( aryOutput, "There are 1 user" ) );

   resetOutput();
   feedMouseInput( "\033[<64;10;10M" );
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_non_null( strstr( aryOutput, "Before draft" ) );
   assert_non_null( strstr( aryOutput, "Draft header" ) );
}

static void paneUiPrepareLocalRedraw_WhenAnchored_HidesPreviousDraft( void **state )
{
   char aryOutput[32768];

   (void)state;
   paneUiEnterIfEligible();
   flagsConfiguration.isPosting = 1;
   assert_true( paneUiWriteLocalOutputText(
      "Previous draft\r\n"
      "Previous draft body\r\n"
      "Abort  Continue  Edit  Print  Save  Xpress -> Edit\r\n" ) );
   resetOutput();
   paneUiForgetRecentLocalOutput( 4 );
   paneUiPrepareLocalRedraw();
   assert_true( paneUiWriteLocalOutputText(
      "[Editing complete]\r\n"
      "Print formatted\r\n"
      "Returned draft body\r\n"
      "Abort  Continue  Edit  Print  Save  Xpress -> " ) );
   readOutput( aryOutput, sizeof( aryOutput ) );

   assert_non_null( strstr( aryOutput, "[Editing complete]" ) );
   assert_non_null( strstr( aryOutput, "Returned draft body" ) );
   assert_null( strstr( aryOutput, "Previous draft body" ) );
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

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test_setup_teardown(
         paneUiEnterIfEligible_WhenWideTerminal_UsesAlternateScreen, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiEnterIfEligible_WhenTerminalNarrow_UsesLegacyView, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiEnterIfEligible_WhenScreenReaderEnabled_UsesLegacyView, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiLeave_WhenActive_RestoresNormalScreen, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiResize_WhenTerminalBecomesNarrow_RepaintsLeftPane, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiPrepareLocalRedraw_WhenActive_ClearsLeftPane, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiWriteLocalOutput_WhenPosting_KeepsPaneLayoutAndScrollback,
         setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiPrepareLocalRedraw_WhenAnchored_HidesPreviousDraft, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiLeaveForExit_WhenActive_ClearsRestoredNormalScreen, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiScroll_WhenMouseWheelMovesOverLeftPane_RedrawsLeftHistory, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiScroll_WhenEscapeSequenceIsNotMouse_ReplaysInput, setup, teardown ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
