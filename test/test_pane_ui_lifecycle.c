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
         paneUiLeaveForExit_WhenActive_ClearsRestoredNormalScreen, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiScroll_WhenMouseWheelMovesOverLeftPane_RedrawsLeftHistory, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiScroll_WhenEscapeSequenceIsNotMouse_ReplaysInput, setup, teardown ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
