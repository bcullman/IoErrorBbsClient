/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client.h"
#include "test/cmocka_compat.h"
#include "client_globals.h"
#include "config_globals.h"
#include "filter_globals.h"
#include "macos_keychain.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

static unsigned int keychainStoreCallCount;
static char aryStoredHost[64];
static char aryStoredPassword[80];
static char aryStoredUser[41];

static bool captureStoredPassword( const char *ptrHost, const char *ptrUser,
                                   const char *ptrPassword )
{
   keychainStoreCallCount++;
   snprintf( aryStoredHost, sizeof( aryStoredHost ), "%s", ptrHost );
   snprintf( aryStoredPassword, sizeof( aryStoredPassword ), "%s", ptrPassword );
   snprintf( aryStoredUser, sizeof( aryStoredUser ), "%s", ptrUser );
   return true;
}

static void resetKeychainTestState( void )
{
   aryAutoName[0] = '\0';
   aryBbsHost[0] = '\0';
   aryCommandLineHost[0] = '\0';
   aryFilterLine[0] = '\0';
   aryStoredHost[0] = '\0';
   aryStoredPassword[0] = '\0';
   aryStoredUser[0] = '\0';
   keychainStoreCallCount = 0;
   flagsConfiguration.shouldUseKeychain = 1;
   clearKeychainSessionState();
   setKeychainPasswordStoreFunctionForTesting( captureStoredPassword );
}

static int setupTest( void **state )
{
   (void)state;

   resetKeychainTestState();
   return 0;
}

static int teardownTest( void **state )
{
   (void)state;

   setKeychainPasswordStoreFunctionForTesting( NULL );
   clearKeychainSessionState();
   flagsConfiguration.shouldUseKeychain = 0;
   return 0;
}

static void stageManualLoginPrompt( void )
{
   // Arrange
   char aryPassword[80];
   bool isAutoFilled;

   snprintf( aryFilterLine, sizeof( aryFilterLine ), "%s", "Name: Password: " );

   // Act
   isAutoFilled = tryGetKeychainPasswordForPrompt( aryPassword,
                                                   sizeof( aryPassword ) );

   // Assert
   if ( isAutoFilled )
   {
      fail_msg( "manual login prompt setup should not auto-fill from keychain during unit tests" );
   }
}

static void stagePasswordChangePrompt( const char *ptrPrompt )
{
   // Arrange
   char aryPassword[80];
   bool isAutoFilled;

   snprintf( aryFilterLine, sizeof( aryFilterLine ), "%s", ptrPrompt );

   // Act
   isAutoFilled = tryGetKeychainPasswordForPrompt( aryPassword,
                                                   sizeof( aryPassword ) );

   // Assert
   if ( isAutoFilled )
   {
      fail_msg( "password change prompt setup should not auto-fill from keychain during unit tests" );
   }
}

int stdPrintf( const char *format, ... )
{
   va_list argList;

   (void)format;
   va_start( argList, format );
   va_end( argList );
   return 0;
}

static void parseBbsUserFromLoginSuccessLine_WhenLegacyBannerPresent_ReturnsUser( void **state )
{
   // Arrange
   char aryUser[41];
   bool isParsed;

   (void)state;

   aryUser[0] = '\0';

   // Act
   isParsed = parseBbsUserFromLoginSuccessLine(
      "User Stilgar (#1234), Access level: Aide",
      aryUser,
      sizeof( aryUser ) );

   // Assert
   if ( !isParsed )
   {
      fail_msg( "legacy login banner should be recognized as a successful login line" );
   }
   if ( strcmp( aryUser, "Stilgar" ) != 0 )
   {
      fail_msg( "legacy login banner should parse user 'Stilgar'; got '%s'", aryUser );
   }
}

static void parseBbsUserFromLoginSuccessLine_WhenWelcomeBannerHasTrailingCarriageReturn_ReturnsUser( void **state )
{
   // Arrange
   char aryUser[41];
   bool isParsed;

   (void)state;

   aryUser[0] = '\0';

   // Act
   isParsed = parseBbsUserFromLoginSuccessLine(
      "Welcome to ISCABBS, Stilgar!\r",
      aryUser,
      sizeof( aryUser ) );

   // Assert
   if ( !isParsed )
   {
      fail_msg( "DOC welcome banner with trailing carriage return should be recognized as a successful login line" );
   }
   if ( strcmp( aryUser, "Stilgar" ) != 0 )
   {
      fail_msg( "DOC welcome banner should parse user 'Stilgar'; got '%s'", aryUser );
   }
}

static void parseBbsUserFromLoginSuccessLine_WhenLineIsNotLoginSuccess_ReturnsFalse( void **state )
{
   // Arrange
   char aryUser[41];
   bool isParsed;

   (void)state;

   snprintf( aryUser, sizeof( aryUser ), "%s", "unchanged" );

   // Act
   isParsed = parseBbsUserFromLoginSuccessLine(
      "This is call 19190.  There are 20 users.",
      aryUser,
      sizeof( aryUser ) );

   // Assert
   if ( isParsed )
   {
      fail_msg( "non-login lines must not be treated as successful login lines" );
   }
   if ( strcmp( aryUser, "unchanged" ) != 0 )
   {
      fail_msg( "non-login lines should leave the destination buffer unchanged; got '%s'", aryUser );
   }
}

static void handleKeychainServerLine_WhenManualLoginSucceedsWithLegacyBanner_StoresPassword( void **state )
{
   // Arrange
   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );
   stageManualLoginPrompt();
   handleKeychainHiddenInput( "sietch" );

   // Act
   handleKeychainServerLine( "User Stilgar (#1234), Access level: Aide" );

   // Assert
   if ( keychainStoreCallCount != 1 )
   {
      fail_msg( "legacy login banner should trigger exactly one password store; got %u",
                keychainStoreCallCount );
   }
   if ( strcmp( aryStoredHost, "bbs.iscabbs.com" ) != 0 )
   {
      fail_msg( "legacy login banner should store against host 'bbs.iscabbs.com'; got '%s'",
                aryStoredHost );
   }
   if ( strcmp( aryStoredPassword, "sietch" ) != 0 )
   {
      fail_msg( "legacy login banner should store password 'sietch'; got '%s'",
                aryStoredPassword );
   }
   if ( strcmp( aryStoredUser, "Stilgar" ) != 0 )
   {
      fail_msg( "legacy login banner should store user 'Stilgar'; got '%s'",
                aryStoredUser );
   }
}

static void handleKeychainServerLine_WhenManualLoginSucceedsWithWelcomeBanner_StoresPassword( void **state )
{
   // Arrange
   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );
   stageManualLoginPrompt();
   handleKeychainHiddenInput( "sietch" );

   // Act
   handleKeychainServerLine( "Welcome to ISCABBS, Stilgar!\r" );

   // Assert
   if ( keychainStoreCallCount != 1 )
   {
      fail_msg( "DOC welcome banner should trigger exactly one password store; got %u",
                keychainStoreCallCount );
   }
   if ( strcmp( aryStoredHost, "bbs.iscabbs.com" ) != 0 )
   {
      fail_msg( "DOC welcome banner should store against host 'bbs.iscabbs.com'; got '%s'",
                aryStoredHost );
   }
   if ( strcmp( aryStoredPassword, "sietch" ) != 0 )
   {
      fail_msg( "DOC welcome banner should store password 'sietch'; got '%s'",
                aryStoredPassword );
   }
   if ( strcmp( aryStoredUser, "Stilgar" ) != 0 )
   {
      fail_msg( "DOC welcome banner should store user 'Stilgar'; got '%s'",
                aryStoredUser );
   }
}

static void handleKeychainServerLine_WhenNoPendingLoginPasswordExists_DoesNotStorePassword( void **state )
{
   // Arrange
   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );

   // Act
   handleKeychainServerLine( "Welcome to ISCABBS, Stilgar!\r" );

   // Assert
   if ( keychainStoreCallCount != 0 )
   {
      fail_msg( "login success without a pending manual password must not store anything; got %u store calls",
                keychainStoreCallCount );
   }
}

static void handleKeychainServerLine_WhenWrongPasswordSeen_DoesNotStorePasswordLater( void **state )
{
   // Arrange
   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );
   stageManualLoginPrompt();
   handleKeychainHiddenInput( "sietch" );

   // Act
   handleKeychainServerLine( "Wrong password..." );
   handleKeychainServerLine( "Welcome to ISCABBS, Stilgar!\r" );

   // Assert
   if ( keychainStoreCallCount != 0 )
   {
      fail_msg( "wrong-password flow must clear pending login save state; got %u store calls",
                keychainStoreCallCount );
   }
}

static void handleKeychainServerLine_WhenIncorrectLoginFollowsAutofill_ManualRetryStoresNewPassword( void **state )
{
   // Arrange
   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );
   stageManualLoginPrompt();
   setPendingKeychainLookupForTesting( true );

   // Act
   handleKeychainServerLine( "Incorrect login.\r" );
   stageManualLoginPrompt();
   handleKeychainHiddenInput( "arrakis" );
   handleKeychainServerLine( "Welcome to ISCABBS, Stilgar!\r" );

   // Assert
   if ( keychainStoreCallCount != 1 )
   {
      fail_msg( "failed keychain autofill followed by a successful manual retry should store exactly one replacement password; got %u",
                keychainStoreCallCount );
   }
   if ( strcmp( aryStoredHost, "bbs.iscabbs.com" ) != 0 )
   {
      fail_msg( "manual retry after failed autofill should store against host 'bbs.iscabbs.com'; got '%s'",
                aryStoredHost );
   }
   if ( strcmp( aryStoredPassword, "arrakis" ) != 0 )
   {
      fail_msg( "manual retry after failed autofill should store replacement password 'arrakis'; got '%s'",
                aryStoredPassword );
   }
   if ( strcmp( aryStoredUser, "Stilgar" ) != 0 )
   {
      fail_msg( "manual retry after failed autofill should store user 'Stilgar'; got '%s'",
                aryStoredUser );
   }
}

static void tryGetKeychainPasswordForPrompt_WhenIncorrectLoginAndRepromptShareBuffer_SuppressesAutoFill( void **state )
{
   // Arrange
   char aryPassword[80];
   bool isAutoFilled;

   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );
   recordCurrentBbsUser( "Stilgar" );
   snprintf( aryFilterLine, sizeof( aryFilterLine ), "%s",
             "Incorrect login.\rName: Password: " );
   setPendingKeychainLookupForTesting( true );

   // Act
   isAutoFilled = tryGetKeychainPasswordForPrompt( aryPassword,
                                                   sizeof( aryPassword ) );

   // Assert
   if ( isAutoFilled )
   {
      fail_msg( "buffered incorrect-login reprompt must suppress keychain autofill for the next prompt" );
   }

   // Arrange
   handleKeychainHiddenInput( "arrakis" );

   // Act
   handleKeychainServerLine( "Welcome to ISCABBS, Stilgar!\r" );

   // Assert
   if ( keychainStoreCallCount != 1 )
   {
      fail_msg( "manual retry after buffered incorrect-login reprompt should store exactly one replacement password; got %u",
                keychainStoreCallCount );
   }
   if ( strcmp( aryStoredHost, "bbs.iscabbs.com" ) != 0 )
   {
      fail_msg( "buffered incorrect-login reprompt should store against host 'bbs.iscabbs.com'; got '%s'",
                aryStoredHost );
   }
   if ( strcmp( aryStoredPassword, "arrakis" ) != 0 )
   {
      fail_msg( "buffered incorrect-login reprompt should store replacement password 'arrakis'; got '%s'",
                aryStoredPassword );
   }
   if ( strcmp( aryStoredUser, "Stilgar" ) != 0 )
   {
      fail_msg( "buffered incorrect-login reprompt should store user 'Stilgar'; got '%s'",
                aryStoredUser );
   }
}

static void processBufferedKeychainServerText_WhenIncorrectLoginAndNamePromptShareBuffer_SuppressesNextAutoFill( void **state )
{
   // Arrange
   char aryPassword[80];
   bool isAutoFilled;

   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );
   recordCurrentBbsUser( "Stilgar" );
   snprintf( aryFilterLine, sizeof( aryFilterLine ), "%s",
             "Incorrect login.\rName: " );
   setPendingKeychainLookupForTesting( true );

   // Act
   processBufferedKeychainServerText();

   snprintf( aryFilterLine, sizeof( aryFilterLine ), "%s", "Password: " );
   isAutoFilled = tryGetKeychainPasswordForPrompt( aryPassword,
                                                   sizeof( aryPassword ) );

   // Assert
   if ( isAutoFilled )
   {
      fail_msg( "buffered incorrect-login name prompt must suppress the following password autofill" );
   }

   // Arrange
   handleKeychainHiddenInput( "arrakis" );

   // Act
   handleKeychainServerLine( "Welcome to ISCABBS, Stilgar!\r" );

   // Assert
   if ( keychainStoreCallCount != 1 )
   {
      fail_msg( "manual retry after buffered incorrect-login name prompt should store exactly one replacement password; got %u",
                keychainStoreCallCount );
   }
   if ( strcmp( aryStoredPassword, "arrakis" ) != 0 )
   {
      fail_msg( "buffered incorrect-login name prompt should store replacement password 'arrakis'; got '%s'",
                aryStoredPassword );
   }
}

static void handleKeychainServerLine_WhenPasswordChangeSucceeds_StoresNewPassword( void **state )
{
   // Arrange
   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );
   recordCurrentBbsUser( "Stilgar" );
   stagePasswordChangePrompt( "Please enter a password: " );
   handleKeychainHiddenInput( "arrakis" );
   stagePasswordChangePrompt( "Please enter it again: " );
   handleKeychainHiddenInput( "arrakis" );

   // Act
   handleKeychainServerLine( "Password changed." );

   // Assert
   if ( keychainStoreCallCount != 1 )
   {
      fail_msg( "successful password change should trigger exactly one password store; got %u",
                keychainStoreCallCount );
   }
   if ( strcmp( aryStoredHost, "bbs.iscabbs.com" ) != 0 )
   {
      fail_msg( "password change should store against host 'bbs.iscabbs.com'; got '%s'",
                aryStoredHost );
   }
   if ( strcmp( aryStoredPassword, "arrakis" ) != 0 )
   {
      fail_msg( "password change should store password 'arrakis'; got '%s'",
                aryStoredPassword );
   }
   if ( strcmp( aryStoredUser, "Stilgar" ) != 0 )
   {
      fail_msg( "password change should store user 'Stilgar'; got '%s'",
                aryStoredUser );
   }
}

static void handleKeychainServerLine_WhenDocPasswordChangePromptsMismatch_DoesNotStorePassword( void **state )
{
   // Arrange
   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );
   recordCurrentBbsUser( "Stilgar" );
   stagePasswordChangePrompt( "New Password: " );
   handleKeychainHiddenInput( "arrakis" );
   stagePasswordChangePrompt( "Again for verification: " );
   handleKeychainHiddenInput( "caladan" );

   // Act
   handleKeychainServerLine( "Your passwords didn't match.  Please try again." );
   handleKeychainServerLine( "So be it." );

   // Assert
   if ( keychainStoreCallCount != 0 )
   {
      fail_msg( "DOC password mismatch must clear staged password and store nothing; got %u store calls",
                keychainStoreCallCount );
   }
}

static void handleKeychainServerLine_WhenDocPasswordChangeSucceeds_StoresNewPassword( void **state )
{
   // Arrange
   (void)state;

   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.iscabbs.com" );
   recordCurrentBbsUser( "Stilgar" );
   stagePasswordChangePrompt( "Old Password: " );
   handleKeychainHiddenInput( "sietch" );
   stagePasswordChangePrompt( "New Password: " );
   handleKeychainHiddenInput( "arrakis" );
   stagePasswordChangePrompt( "Again for verification: " );
   handleKeychainHiddenInput( "arrakis" );

   // Act
   handleKeychainServerLine( "So be it." );

   // Assert
   if ( keychainStoreCallCount != 1 )
   {
      fail_msg( "DOC password change success should trigger exactly one password store; got %u",
                keychainStoreCallCount );
   }
   if ( strcmp( aryStoredHost, "bbs.iscabbs.com" ) != 0 )
   {
      fail_msg( "DOC password change should store against host 'bbs.iscabbs.com'; got '%s'",
                aryStoredHost );
   }
   if ( strcmp( aryStoredPassword, "arrakis" ) != 0 )
   {
      fail_msg( "DOC password change should store password 'arrakis'; got '%s'",
                aryStoredPassword );
   }
   if ( strcmp( aryStoredUser, "Stilgar" ) != 0 )
   {
      fail_msg( "DOC password change should store user 'Stilgar'; got '%s'",
                aryStoredUser );
   }
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test_setup_teardown(
         parseBbsUserFromLoginSuccessLine_WhenLegacyBannerPresent_ReturnsUser,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         parseBbsUserFromLoginSuccessLine_WhenWelcomeBannerHasTrailingCarriageReturn_ReturnsUser,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         parseBbsUserFromLoginSuccessLine_WhenLineIsNotLoginSuccess_ReturnsFalse,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         handleKeychainServerLine_WhenManualLoginSucceedsWithLegacyBanner_StoresPassword,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         handleKeychainServerLine_WhenManualLoginSucceedsWithWelcomeBanner_StoresPassword,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         handleKeychainServerLine_WhenNoPendingLoginPasswordExists_DoesNotStorePassword,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         handleKeychainServerLine_WhenWrongPasswordSeen_DoesNotStorePasswordLater,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         handleKeychainServerLine_WhenIncorrectLoginFollowsAutofill_ManualRetryStoresNewPassword,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         tryGetKeychainPasswordForPrompt_WhenIncorrectLoginAndRepromptShareBuffer_SuppressesAutoFill,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         processBufferedKeychainServerText_WhenIncorrectLoginAndNamePromptShareBuffer_SuppressesNextAutoFill,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         handleKeychainServerLine_WhenPasswordChangeSucceeds_StoresNewPassword,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         handleKeychainServerLine_WhenDocPasswordChangePromptsMismatch_DoesNotStorePassword,
         setupTest,
         teardownTest ),
      cmocka_unit_test_setup_teardown(
         handleKeychainServerLine_WhenDocPasswordChangeSucceeds_StoresNewPassword,
         setupTest,
         teardownTest ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
