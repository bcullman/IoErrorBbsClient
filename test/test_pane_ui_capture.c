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
#include "pane_ui_test_helpers.h"

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

static void paneUiView_WhenUserTypesNextPostAtRoomPrompt_CapturesHiddenSidebar( void **state )
{
   char aryNetOutput[16];
   char aryTerminalOutput[8192];
   const char *ptrBody;
   const char *ptrSpacer;
   const char *ptrTitle;

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Babble>", false );
   paneUiNoteUserInput();
   feedIncomingText(
      "Current post.\r\n"
      "[Babble> msg #57527 (2 remaining)] Read cmd ->",
      false );

   assert_true( paneUiHandleLocalInput( 'n', false ) );
   paneUiHandleMorePromptStateChanged( true );
   paneUiHandleMorePromptStateChanged( false );
   feedIncomingText(
      "Babble> Read New\r\n"
      "\033[32mNext post first page.\033[0m\r\n"
      "--MORE--(50%)\r\n"
      "Next post second page.\r\n"
      "[Babble> msg #57528 (1 remaining)] Read cmd ->",
      true );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_string_equal( aryNetOutput, "n " );
   ptrTitle = strstr( aryTerminalOutput, "Babble" );
   ptrBody = strstr( aryTerminalOutput, "Next post first page." );
   assert_non_null( ptrTitle );
   assert_non_null( ptrBody );
   assert_true( ptrTitle < ptrBody );
   assert_non_null( strstr( aryTerminalOutput,
                            "\033[38;5;220mBabble>\033[0;38;5;34;48;5;236m" ) );
   ptrSpacer = strstr( ptrTitle, "\033[2;81H" );
   assert_non_null( ptrSpacer );
   assert_true( ptrSpacer < ptrBody );
   assert_non_null( strstr( aryTerminalOutput, "Next post second page." ) );
   assert_null( strstr( aryTerminalOutput, "--MORE--" ) );
   assert_null( strstr( aryTerminalOutput, "Read New" ) );

   resetOutput();
   assert_true( paneUiHandleLocalInput( 'n', false ) );
   feedIncomingText(
      "Babble> Next\r\n"
      "Following post.\r\n"
      "[Babble> msg #57529 (0 remaining)] Read cmd ->",
      true );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_string_equal( aryNetOutput, "n n" );
   ptrTitle = strstr( aryTerminalOutput, "Babble" );
   ptrBody = strstr( aryTerminalOutput, "Following post." );
   assert_non_null( ptrTitle );
   assert_non_null( ptrBody );
   assert_true( ptrTitle < ptrBody );
   assert_non_null( strstr( aryTerminalOutput,
                            "\033[38;5;220mBabble>\033[0;38;5;34;48;5;236m" ) );
   ptrSpacer = strstr( ptrTitle, "\033[2;81H" );
   assert_non_null( ptrSpacer );
   assert_true( ptrSpacer < ptrBody );
   assert_null( strstr( aryTerminalOutput, "Babble> Next" ) );

   resetOutput();
   assert_true( paneUiHandleLocalInput( 'A', false ) );
   feedIncomingText(
      "Babble> Again\r\n"
      "Repeated post.\r\n"
      "[Babble> msg #57529 (0 remaining)] Read cmd ->",
      true );
   memset( aryNetOutput, 0, sizeof( aryNetOutput ) );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   readOutput( aryTerminalOutput, sizeof( aryTerminalOutput ) );

   assert_string_equal( aryNetOutput, "n nA" );
   ptrTitle = strstr( aryTerminalOutput, "Babble" );
   ptrBody = strstr( aryTerminalOutput, "Repeated post." );
   assert_non_null( ptrTitle );
   assert_non_null( ptrBody );
   assert_true( ptrTitle < ptrBody );
   assert_non_null( strstr( aryTerminalOutput,
                            "\033[38;5;220mBabble>\033[0;38;5;34;48;5;236m" ) );
   ptrSpacer = strstr( ptrTitle, "\033[2;81H" );
   assert_non_null( ptrSpacer );
   assert_true( ptrSpacer < ptrBody );
   assert_null( strstr( aryTerminalOutput, "Babble> Again" ) );
}

static void paneUiView_WhenNotAtRoomPrompt_LeavesNextPostUnhandled( void **state )
{
   char aryNetOutput[16];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Read cmd ->", false );

   assert_false( paneUiHandleLocalInput( 'n', false ) );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   assert_string_equal( aryNetOutput, "" );
}

static void paneUiView_WhenAtRoomPrompt_LeavesSpaceUnhandled( void **state )
{
   char aryNetOutput[16];

   (void)state;
   paneUiEnterIfEligible();
   feedIncomingText( "Lobby>", false );

   assert_false( paneUiHandleLocalInput( ' ', false ) );
   readNetOutput( aryNetOutput, sizeof( aryNetOutput ) );
   assert_string_equal( aryNetOutput, "" );
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

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test_setup_teardown(
         paneUiRefresh_WhenPromptReady_CapturesSidebarAndClipsDoing, setup,
         teardown ),
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
         paneUiView_WhenUserTypesNextPostAtRoomPrompt_CapturesHiddenSidebar, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenNotAtRoomPrompt_LeavesNextPostUnhandled, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenAtRoomPrompt_LeavesSpaceUnhandled, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenPromptIsUnsafe_LeavesHelpAndAidesUnhandled, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenUserTypesForumInfo_PrependsCurrentForumName, setup, teardown ),
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
         paneUiView_WhenPagedHelpCompletes_ClearsStaleMoreState, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenCapturedResponsePages_SendsHiddenSpaces, setup, teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenPagedResponseContainsMoreMarker_StripsOnlyMarker, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenPagedResponseOverwritesPrompt_KeepsAidesAligned, setup,
         teardown ),
      cmocka_unit_test_setup_teardown(
         paneUiView_WhenHelpPinned_DoesNotRefreshUntilWhoSelected, setup, teardown ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
