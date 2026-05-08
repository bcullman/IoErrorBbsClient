/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "bbsrc.h"
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
#include "macos_keychain.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include "telnet.h"
#include "test_helpers.h"
#include "utility.h"
static int aryInputQueue[256];
static size_t inputCount;
static size_t inputIndex;
static unsigned int flushCount;
static unsigned int lastFlushValue;
static char aryCapturedString[256];
static int capPutsCallCount;
static char aryCapturedDots[256];
static size_t capturedDotCount;
static char aryHiddenKeychainInput[64];
static char aryRecordedBbsUser[64];
static char aryStubKeychainPassword[64];
static unsigned int processBufferedKeychainServerTextCallCount;
static bool shouldAutoFillFromKeychain;

static void setInputSequence( const int *aryKeys, size_t count )
{
   inputCount = copyIntArray( aryKeys, count, aryInputQueue, sizeof( aryInputQueue ) / sizeof( aryInputQueue[0] ) );
   inputIndex = 0;
}

static void resetTracking( void )
{
   byte = 0;
   inputCount = 0;
   inputIndex = 0;
   flushCount = 0;
   lastInteractiveInputByte = -1;
   lastFlushValue = 0;
   aryCapturedString[0] = '\0';
   capPutsCallCount = 0;
   aryCapturedDots[0] = '\0';
   capturedDotCount = 0;
   aryHiddenKeychainInput[0] = '\0';
   aryRecordedBbsUser[0] = '\0';
   aryStubKeychainPassword[0] = '\0';
   processBufferedKeychainServerTextCallCount = 0;
   shouldAutoFillFromKeychain = false;
}

static void setupWhoList( const char *ptrFirst, const char *ptrSecond )
{
   char *ptrFirstCopy;
   char *ptrSecondCopy;

   whoList = slistCreate( 0, compareStringPointer );
   if ( whoList == NULL )
   {
      fail_msg( "slistCreate failed for whoList setup" );
      return;
   }

   ptrFirstCopy = NULL;
   ptrSecondCopy = NULL;
   if ( !tryDuplicateString( ptrFirst, &ptrFirstCopy ) || !tryDuplicateString( ptrSecond, &ptrSecondCopy ) )
   {
      free( ptrFirstCopy );
      free( ptrSecondCopy );
      fail_msg( "Arrange failed: unable to duplicate whoList names for getline tests" );
      return;
   }
   if ( !slistAddItem( whoList, ptrFirstCopy, 1 ) )
   {
      free( ptrFirstCopy );
      free( ptrSecondCopy );
      fail_msg( "slistAddItem failed while preparing whoList for getline tests" );
      return;
   }
   if ( !slistAddItem( whoList, ptrSecondCopy, 1 ) )
   {
      free( ptrSecondCopy );
      fail_msg( "slistAddItem failed while preparing whoList for getline tests" );
      return;
   }
   slistSort( whoList );
}

static void teardownWhoList( void )
{
   if ( whoList != NULL )
   {
      slistDestroyItems( whoList );
      slistDestroy( whoList );
      whoList = NULL;
   }
}

// getline.c dependencies not under test here.
int capPutChar( int inputChar )
{
   if ( capturedDotCount < sizeof( aryCapturedDots ) - 1 )
   {
      aryCapturedDots[capturedDotCount++] = (char)inputChar;
      aryCapturedDots[capturedDotCount] = '\0';
   }
   return inputChar;
}

int capPuts( const char *ptrText )
{
   capPutsCallCount++;
   snprintf( aryCapturedString, sizeof( aryCapturedString ), "%s", ptrText );
   return 1;
}

void flushInput( unsigned int count )
{
   flushCount++;
   lastFlushValue = count;
}

void handleInvalidInput( unsigned int *ptrInvalidCount )
{
   if ( ( *ptrInvalidCount )++ )
   {
      flushInput( *ptrInvalidCount );
   }
}

int inKey( void )
{
   if ( inputIndex < inputCount )
   {
      return aryInputQueue[inputIndex++];
   }
   return '\n';
}

int netPutChar( int inputChar )
{
   return inputChar;
}

void printAnsiForegroundColorValue( int colorValue )
{
   (void)colorValue;
}

void sendTrackedBuffer( const char *ptrBuffer, size_t length )
{
   size_t itemIndex;

   for ( itemIndex = 0; itemIndex < length; ++itemIndex )
   {
      netPutChar( ptrBuffer[itemIndex] );
      byte++;
   }
}

void sendTrackedNewline( void )
{
   netPutChar( '\n' );
   byte++;
}

int popQueue( char *ptrObject, queue *ptrQueue )
{
   (void)ptrObject;
   (void)ptrQueue;
   return 0;
}

void replyMessage( void )
{
   // Test stub: away-message sending is not relevant in this test.
}

void clearKeychainSessionState( void )
{
   // Test stub: keychain session state is not relevant in this test.
}

bool tryDeleteKeychainPassword( const char *ptrHost, const char *ptrUser )
{
   (void)ptrHost;
   (void)ptrUser;
   return false;
}

bool tryGetKeychainPassword( const char *ptrHost, const char *ptrUser,
                             char *ptrPassword, size_t passwordSize )
{
   (void)ptrHost;
   (void)ptrPassword;
   (void)passwordSize;
   (void)ptrUser;
   return false;
}

void handleKeychainHiddenInput( const char *ptrPassword )
{
   snprintf( aryHiddenKeychainInput, sizeof( aryHiddenKeychainInput ),
             "%s", ptrPassword );
}

void handleKeychainServerLine( const char *ptrLine )
{
   (void)ptrLine;
}

void processBufferedKeychainServerText( void )
{
   processBufferedKeychainServerTextCallCount++;
}

void recordCurrentBbsUser( const char *ptrUser )
{
   snprintf( aryRecordedBbsUser, sizeof( aryRecordedBbsUser ), "%s", ptrUser );
}

bool trySetKeychainPassword( const char *ptrHost, const char *ptrUser,
                             const char *ptrPassword )
{
   (void)ptrHost;
   (void)ptrPassword;
   (void)ptrUser;
   return false;
}

void sendBlock( void )
{
   // Test stub: telnet block signaling is not relevant in this test.
}

int stdPrintf( const char *format, ... )
{
   va_list argList;

   va_start( argList, format );
   va_end( argList );
   return 0;
}

bool tryGetKeychainPasswordForPrompt( char *ptrPassword, size_t passwordSize )
{
   if ( !shouldAutoFillFromKeychain )
   {
      return false;
   }

   snprintf( ptrPassword, passwordSize, "%s", aryStubKeychainPassword );
   return true;
}

bool tryUpsertKeychainPassword( const char *ptrHost, const char *ptrUser,
                                const char *ptrPassword )
{
   (void)ptrHost;
   (void)ptrPassword;
   (void)ptrUser;
   return false;
}

void writeBbsRc( void )
{
   // Test stub: config persistence is not relevant in this test.
}

static void smartName_WhenUniquePrefix_ExpandsToFullName( void **state )
{
   // Arrange
   char aryBuffer[41];
   int found;

   (void)state;

   resetTracking();
   teardownWhoList();
   setupWhoList( "Dr Strange", "Meatball" );
   snprintf( aryBuffer, sizeof( aryBuffer ), "%s", "Dr S" );

   // Act
   found = smartName( aryBuffer, aryBuffer + strlen( aryBuffer ) );

   // Assert
   if ( found != 1 )
   {
      fail_msg( "smartName should return 1 for a unique prefix match; got %d", found );
   }
   if ( strcmp( aryBuffer, "Dr Strange" ) != 0 )
   {
      fail_msg( "smartName should expand to 'Dr Strange'; got '%s'", aryBuffer );
   }

   teardownWhoList();
}

static void smartName_WhenPrefixIsAmbiguous_ReturnsNoMatchAndRestoresTail( void **state )
{
   // Arrange
   char aryBuffer[41];
   int found;

   (void)state;

   resetTracking();
   teardownWhoList();
   setupWhoList( "Meatball", "Merlin" );
   snprintf( aryBuffer, sizeof( aryBuffer ), "%s", "MeX" );

   // Act
   found = smartName( aryBuffer, aryBuffer + 2 );

   // Assert
   if ( found != 0 )
   {
      fail_msg( "smartName should return 0 for ambiguous prefix; got %d", found );
   }
   if ( aryBuffer[2] != 'X' )
   {
      fail_msg( "smartName should restore tail character when no unique match; got '%c'", aryBuffer[2] );
   }

   teardownWhoList();
}

static void getString_WhenSimpleInputProvided_ReturnsTypedString( void **state )
{
   // Arrange
   char aryResult[64];
   const int aryKeys[] = { 'H', 'i', '\n' };

   (void)state;

   resetTracking();
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   getString( 20, aryResult, 0 );

   // Assert
   if ( strcmp( aryResult, "Hi" ) != 0 )
   {
      fail_msg( "getString should return typed string 'Hi'; got '%s'", aryResult );
   }
   if ( capPutsCallCount != 1 || strcmp( aryCapturedString, "Hi" ) != 0 )
   {
      fail_msg( "getString should capture plain input via capPuts; got calls=%d text='%s'",
                capPutsCallCount, aryCapturedString );
   }
}

static void getName_WhenAutoLoginUsed_ProcessesBufferedKeychainServerText( void **state )
{
   // Arrange
   char *ptrResult;

   (void)state;

   resetTracking();
   isAutoLoggedIn = 0;
   snprintf( aryAutoName, sizeof( aryAutoName ), "%s", "Stilgar" );

   // Act
   ptrResult = getName( 1 );

   // Assert
   if ( strcmp( ptrResult, "Stilgar" ) != 0 )
   {
      fail_msg( "auto-login name prompt should return 'Stilgar'; got '%s'", ptrResult );
   }
   if ( processBufferedKeychainServerTextCallCount != 1 )
   {
      fail_msg( "auto-login name prompt should process buffered keychain server text exactly once; got %u calls",
                processBufferedKeychainServerTextCallCount );
   }
}

static void getString_WhenCtrlWUsed_RemovesPreviousWord( void **state )
{
   // Arrange
   char aryResult[64];
   const int aryKeys[] = { 'D', 'r', ' ', 'S', 't', 'r', 'a', 'n', 'g', 'e', CTRL_W, 'W', 'h', 'o', '\n' };

   (void)state;

   resetTracking();
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   getString( 30, aryResult, 0 );

   // Assert
   if ( strcmp( aryResult, "Dr Who" ) != 0 )
   {
      fail_msg( "CTRL_W should erase previous word; expected 'Dr Who', got '%s'", aryResult );
   }
}

static void getString_WhenHiddenInputUsed_CapturesDotsInsteadOfPlainText( void **state )
{
   // Arrange
   char aryResult[64];
   const int aryKeys[] = { 's', 'e', 'c', 'r', 'e', 't', '\n' };

   (void)state;

   resetTracking();
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   getString( -20, aryResult, 0 );

   // Assert
   if ( strcmp( aryResult, "secret" ) != 0 )
   {
      fail_msg( "hidden getString should still store clear text in result; got '%s'", aryResult );
   }
   if ( capPutsCallCount != 0 )
   {
      fail_msg( "hidden getString should not call capPuts directly; got %d calls", capPutsCallCount );
   }
   if ( strcmp( aryCapturedDots, "......" ) != 0 )
   {
      fail_msg( "hidden getString should capture one dot per character; got '%s'", aryCapturedDots );
   }
   if ( strcmp( aryHiddenKeychainInput, "secret" ) != 0 )
   {
      fail_msg( "hidden getString should forward manual hidden input to keychain handler; got '%s'",
                aryHiddenKeychainInput );
   }
}

static void getString_WhenKeychainReturnsPassword_UsesHiddenAutofill( void **state )
{
   // Arrange
   char aryResult[64];
   const int aryKeys[] = { 'x', 'y', 'z', '\n' };

   (void)state;

   resetTracking();
   shouldAutoFillFromKeychain = true;
   snprintf( aryStubKeychainPassword, sizeof( aryStubKeychainPassword ),
             "%s", "stored-secret" );
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   getString( -20, aryResult, 0 );

   // Assert
   if ( strcmp( aryResult, "stored-secret" ) != 0 )
   {
      fail_msg( "hidden getString should use keychain password when available; got '%s'", aryResult );
   }
   if ( inputIndex != 0 )
   {
      fail_msg( "keychain autofill should not consume queued keystrokes; consumed %zu of %zu inputs",
                inputIndex, inputCount );
   }
   if ( strcmp( aryCapturedDots, "............." ) != 0 )
   {
      fail_msg( "keychain autofill should capture one dot per character; got '%s'", aryCapturedDots );
   }
   if ( aryHiddenKeychainInput[0] != '\0' )
   {
      fail_msg( "keychain autofill should not call hidden-input handler; got '%s'", aryHiddenKeychainInput );
   }
}

static void getString_WhenRepeatedInvalidControlInputReceived_FlushesInput( void **state )
{
   // Arrange
   char aryResult[64];
   const int aryKeys[] = { 1, 2, 'A', '\n' };

   (void)state;

   resetTracking();
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   getString( 20, aryResult, 0 );

   // Assert
   if ( strcmp( aryResult, "A" ) != 0 )
   {
      fail_msg( "getString should ignore invalid controls and keep valid chars; got '%s'", aryResult );
   }
   if ( flushCount == 0 || lastFlushValue != 2 )
   {
      fail_msg( "repeated invalid controls should trigger flushInput with incremented invalid counter; got count=%u last=%u",
                flushCount, lastFlushValue );
   }
}

static void getString_WhenCtrlRReceived_IgnoresItAsInvalidInput( void **state )
{
   // Arrange
   char aryResult[64];
   const int aryKeys[] = { 'A', CTRL_R, 'B', '\n' };

   (void)state;

   resetTracking();
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   getString( 20, aryResult, 0 );

   // Assert
   if ( strcmp( aryResult, "AB" ) != 0 )
   {
      fail_msg( "getString should ignore CTRL_R and keep surrounding text; got '%s'", aryResult );
   }
}

static void getName_WhenAutocompleteEnabled_ExpandsUniqueName( void **state )
{
   // Arrange
   char *ptrResult;
   const int aryKeys[] = { 'D', 'r', ' ', 'S', '\n' };

   (void)state;

   resetTracking();
   teardownWhoList();
   setupWhoList( "Dr Strange", "Meatball" );
   flagsConfiguration.shouldEnableNameAutocomplete = 1;

   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   ptrResult = getName( 2 );

   // Assert
   if ( strcmp( ptrResult, "Dr Strange" ) != 0 )
   {
      fail_msg( "getName should expand unique prefixes when autocomplete is enabled; got '%s'", ptrResult );
   }

   teardownWhoList();
}

static void getName_WhenLoginHandleEntered_RecordsCurrentBbsUser( void **state )
{
   // Arrange
   char *ptrResult;
   const int aryKeys[] = { 'D', 'o', 'c', '\n' };

   (void)state;

   resetTracking();
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   ptrResult = getName( 1 );

   // Assert
   if ( strcmp( ptrResult, "Doc" ) != 0 )
   {
      fail_msg( "getName should preserve the typed login handle; got '%s'", ptrResult );
   }
   if ( strcmp( aryRecordedBbsUser, "Doc" ) != 0 )
   {
      fail_msg( "getName should record the current BBS user for login prompts; got '%s'",
                aryRecordedBbsUser );
   }
}

static void getName_WhenAutocompleteDisabled_LeavesTypedPrefixUnchanged( void **state )
{
   // Arrange
   char *ptrResult;
   const int aryKeys[] = { 'D', 'r', ' ', 'S', '\n' };

   (void)state;

   resetTracking();
   teardownWhoList();
   setupWhoList( "Dr Strange", "Meatball" );
   flagsConfiguration.shouldEnableNameAutocomplete = 0;

   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   ptrResult = getName( 2 );

   // Assert
   if ( strcmp( ptrResult, "Dr S" ) != 0 )
   {
      fail_msg( "getName should keep typed text unchanged when autocomplete is disabled; got '%s'", ptrResult );
   }

   teardownWhoList();
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( smartName_WhenUniquePrefix_ExpandsToFullName ),
      cmocka_unit_test( smartName_WhenPrefixIsAmbiguous_ReturnsNoMatchAndRestoresTail ),
      cmocka_unit_test( getString_WhenSimpleInputProvided_ReturnsTypedString ),
      cmocka_unit_test( getString_WhenCtrlWUsed_RemovesPreviousWord ),
      cmocka_unit_test( getString_WhenHiddenInputUsed_CapturesDotsInsteadOfPlainText ),
      cmocka_unit_test( getString_WhenKeychainReturnsPassword_UsesHiddenAutofill ),
      cmocka_unit_test( getString_WhenRepeatedInvalidControlInputReceived_FlushesInput ),
      cmocka_unit_test( getString_WhenCtrlRReceived_IgnoresItAsInvalidInput ),
      cmocka_unit_test( getName_WhenAutoLoginUsed_ProcessesBufferedKeychainServerText ),
      cmocka_unit_test( getName_WhenLoginHandleEntered_RecordsCurrentBbsUser ),
      cmocka_unit_test( getName_WhenAutocompleteEnabled_ExpandsUniqueName ),
      cmocka_unit_test( getName_WhenAutocompleteDisabled_LeavesTypedPrefixUnchanged ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
