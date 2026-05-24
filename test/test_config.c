/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config_file.h"
#include "browser.h"
#include "client.h"
#include "test/cmocka_compat.h"
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
static int aryGetKeyQueue[128];
static size_t getKeyCount;
static size_t getKeyIndex;

static int aryYesNoQueue[32];
static size_t yesNoCount;
static size_t yesNoIndex;

static int aryPromptQueue[32];
static size_t promptCount;
static size_t promptIndex;

static const char *aryStringQueue[32];
static size_t stringCount;
static size_t stringIndex;
static int getStringCallCount;
static int sPromptCallCount;
static bool isKeychainPasswordContextAvailable;
static bool isKeychainPasswordDeleted;
static bool shouldDeleteKeychainPasswordSucceed;

static char aryStdPrintfLog[4096];

static void cleanupWriteConfigFixture( void )
{
   if ( ptrConfigFile != NULL )
   {
      fclose( ptrConfigFile );
      ptrConfigFile = NULL;
   }
   if ( friendList != NULL )
   {
      slistDestroyItems( friendList );
      slistDestroy( friendList );
      friendList = NULL;
   }
   if ( enemyList != NULL )
   {
      slistDestroyItems( enemyList );
      slistDestroy( enemyList );
      enemyList = NULL;
   }
}

static void resetState( void )
{
   getKeyCount = 0;
   getKeyIndex = 0;
   yesNoCount = 0;
   yesNoIndex = 0;
   promptCount = 0;
   promptIndex = 0;
   stringCount = 0;
   stringIndex = 0;
   getStringCallCount = 0;
   isKeychainPasswordContextAvailable = false;
   isKeychainPasswordDeleted = false;
   shouldDeleteKeychainPasswordSucceed = false;
   sPromptCallCount = 0;
   aryStdPrintfLog[0] = '\0';
   configuredColorOutputMode = COLOR_OUTPUT_MODE_AUTO;
   memset( &color, 0, sizeof( color ) );
   memset( &color256, 0, sizeof( color256 ) );
   memset( &colorTruecolor, 0, sizeof( colorTruecolor ) );
   useBlackThemeBackgrounds = false;
   useBlackThemeBackgrounds256 = false;
   useBlackThemeBackgroundsTruecolor = false;
}

static void setGetKeySequence( const int *aryValues, size_t valueCount )
{
   getKeyCount = copyIntArray( aryValues,
                               valueCount,
                               aryGetKeyQueue,
                               sizeof( aryGetKeyQueue ) / sizeof( aryGetKeyQueue[0] ) );
   getKeyIndex = 0;
}

static void setYesNoSequence( const int *aryValues, size_t valueCount )
{
   yesNoCount = copyIntArray( aryValues,
                              valueCount,
                              aryYesNoQueue,
                              sizeof( aryYesNoQueue ) / sizeof( aryYesNoQueue[0] ) );
   yesNoIndex = 0;
}

static void setPromptSequence( const int *aryValues, size_t valueCount )
{
   promptCount = copyIntArray( aryValues,
                               valueCount,
                               aryPromptQueue,
                               sizeof( aryPromptQueue ) / sizeof( aryPromptQueue[0] ) );
   promptIndex = 0;
}

static void setStringSequence( const char **aryValues, size_t valueCount )
{
   stringCount = copyStringPointerArray( aryValues,
                                         valueCount,
                                         aryStringQueue,
                                         sizeof( aryStringQueue ) / sizeof( aryStringQueue[0] ) );
   stringIndex = 0;
}

// config.c dependencies outside this test target's scope.
int capPrintf( const char *format, ... )
{
   va_list argList;

   (void)format;
   va_start( argList, format );
   va_end( argList );
   return 1;
}

void printAnsiDisplayStateValue( int foregroundColor, int backgroundColor )
{
   (void)foregroundColor;
   (void)backgroundColor;
}

int colorize( const char *ptrText )
{
   (void)ptrText;
   return 1;
}

bool tryDeleteSavedKeychainPasswordForCurrentBbs( void )
{
   isKeychainPasswordDeleted = true;
   return shouldDeleteKeychainPasswordSucceed;
}

bool hasSavedKeychainPasswordContextForCurrentBbs( void )
{
   return isKeychainPasswordContextAvailable;
}

void printAnsiForegroundColorValue( int colorValue )
{
   (void)colorValue;
}

void printThemedMnemonicText( const char *ptrText, int defaultColor )
{
   (void)ptrText;
   (void)defaultColor;
}

void copyColorTable( Color *ptrDestination, const Color *ptrSource )
{
   *ptrDestination = *ptrSource;
}

/// @brief Return one test color field from the internal color-field order.
///
/// @param colorIndex Field index in the internal color array.
///
/// @return Configured test color value at the requested index.
int colorFieldValue( int colorIndex )
{
   int *const aryTestColorFields[COLOR_FIELD_COUNT] =
      {
         &color.text,
         &color.forum,
         &color.number,
         &color.errorTextColor,
         &color.ansiBlackTextColor,
         &color.ansiBlueTextColor,
         &color.ansiMagentaTextColor,
         &color.postDate,
         &color.postName,
         &color.postText,
         &color.postFriendDate,
         &color.postFriendName,
         &color.postFriendText,
         &color.anonymous,
         &color.morePrompt,
         &color.ansiWhiteTextColor,
         &color.reserved5,
         &color.background,
         &color.inputText,
         &color.inputHighlight,
         &color.expressText,
         &color.expressName,
         &color.expressFriendText,
         &color.expressFriendName };

   assert( colorIndex >= 0 );
   assert( colorIndex < COLOR_FIELD_COUNT );

   return *aryTestColorFields[colorIndex];
}

int colorFieldValueForColor( const Color *ptrColor, int colorIndex )
{
   const int *ptrColorFields;

   assert( ptrColor != NULL );
   assert( colorIndex >= 0 );
   assert( colorIndex < COLOR_FIELD_COUNT );

   ptrColorFields = (const int *)ptrColor;
   return ptrColorFields[colorIndex];
}

const char *colorFieldTomlKeyName( int colorIndex )
{
   static const char *const aryTestColorTomlKeys[COLOR_FIELD_COUNT] =
      {
         "text",
         "forum_prompt",
         "number_prompt",
         "error_text",
         "incoming_ansi_black",
         "incoming_ansi_blue",
         "incoming_ansi_magenta",
         "post_date",
         "post_name",
         "post_text",
         "post_friend_date",
         "post_friend_name",
         "post_friend_text",
         "anonymous_post",
         "more_prompt",
         "incoming_ansi_white",
         NULL,
         "background",
         "input_text",
         "input_highlight",
         "express_text",
         "express_name",
         "express_friend_text",
         "express_friend_name" };

   assert( colorIndex >= 0 );
   assert( colorIndex < COLOR_FIELD_COUNT );

   return aryTestColorTomlKeys[colorIndex];
}

const char *colorNameFromValue( int colorValue )
{
   if ( colorValueIsRgb( colorValue ) )
   {
      return NULL;
   }

   switch ( colorValue )
   {
      case 8:
         return "brightblack";
      case 9:
         return "brightred";
      case 10:
         return "brightgreen";
      case 11:
         return "brightyellow";
      case 12:
         return "brightblue";
      case 13:
         return "brightmagenta";
      case 14:
         return "brightcyan";
      case 15:
         return "brightwhite";
      case 16:
         return "black";
      case 160:
         return "red";
      case 34:
         return "green";
      case 220:
         return "yellow";
      case 26:
         return "blue";
      case 91:
         return "magenta";
      case 44:
         return "cyan";
      case 231:
         return "white";
      case COLOR_VALUE_DEFAULT:
         return "default";
      default:
         return NULL;
   }
}

const char *colorOutputModeName( ColorOutputMode outputMode )
{
   switch ( outputMode )
   {
      case COLOR_OUTPUT_MODE_TRUECOLOR:
         return "truecolor";

      case COLOR_OUTPUT_MODE_256:
         return "256";

      case COLOR_OUTPUT_MODE_AUTO:
      default:
         return "auto";
   }
}

int colorValueToLegacyDigit( int colorValue )
{
   return colorValue + '0';
}

int colorConfigCalled;
void colorConfig( void )
{
   colorConfigCalled++;
}

int deleteFile( const char *ptrPath )
{
   (void)ptrPath;
   return 0;
}

int fStrCompareVoid( const void *ptrName, const void *ptrFriend )
{
   const friend *ptrEntry;

   ptrEntry = ptrFriend;
   return strcmp( (const char *)ptrName, ptrEntry->name );
}

int fSortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   const friend *const *ptrLeftFriend;
   const friend *const *ptrRightFriend;

   ptrLeftFriend = ptrLeft;
   ptrRightFriend = ptrRight;
   return strcmp( ( *ptrLeftFriend )->name, ( *ptrRightFriend )->name );
}

noreturn void fatalExit( const char *message, const char *heading )
{
   fail_msg( "fatalExit invoked unexpectedly: %s (%s)", message, heading );
   abort();
}

char *findChar( const char *ptrString, int targetChar )
{
   return (char *)strchr( ptrString, targetChar );
}

void flushInput( unsigned int count )
{
   (void)count;
}

void handleInvalidInput( unsigned int *ptrInvalidCount )
{
   if ( ( *ptrInvalidCount )++ )
   {
      flushInput( *ptrInvalidCount );
   }
}

int getKey( void )
{
   if ( getKeyIndex < getKeyCount )
   {
      return aryGetKeyQueue[getKeyIndex++];
   }
   return '\n';
}

int readValidatedMenuKey( const char *allowedCharsLowercase )
{
   int inputChar;
   unsigned int invalid;

   invalid = 0;
   for ( ;; )
   {
      inputChar = getKey();
      if ( isalpha( inputChar ) )
      {
         inputChar = tolower( inputChar );
      }
      if ( findChar( allowedCharsLowercase, inputChar ) )
      {
         return inputChar;
      }
      handleInvalidInput( &invalid );
   }
}

char *getName( int quitPriv )
{
   (void)quitPriv;
   return "";
}

void getString( int length, char *result, int line )
{
   const char *ptrSource;

   (void)length;
   (void)line;
   getStringCallCount++;

   ptrSource = "";
   if ( stringIndex < stringCount )
   {
      ptrSource = aryStringQueue[stringIndex++];
   }
   snprintf( result, 80, "%s", ptrSource );
}

int inKey( void )
{
   return '\n';
}

void information( void )
{
   // Test stub: information-screen output is not relevant in this test.
}

int more( int *line, int percent )
{
   (void)line;
   (void)percent;
   return 0;
}

noreturn void myExit( void )
{
   fail_msg( "myExit invoked unexpectedly during config unit tests" );
   abort();
}

void resetTerm( void )
{
   // Test stub: terminal reset behavior is not relevant in this test.
}

void setTerm( void )
{
   // Test stub: terminal setup behavior is not relevant in this test.
}

void sInfo( const char *message, const char *heading )
{
   (void)message;
   (void)heading;
}

int sPrompt( const char *message, const char *heading, int defaultAnswer )
{
   (void)message;
   (void)heading;
   sPromptCallCount++;
   if ( promptIndex < promptCount )
   {
      return aryPromptQueue[promptIndex++];
   }
   return defaultAnswer;
}

int stdPrintf( const char *format, ... )
{
   va_list argList;
   char aryBuffer[512];
   size_t logLength;

   (void)format;
   va_start( argList, format );
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
   vsnprintf( aryBuffer, sizeof( aryBuffer ), format, argList );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
   va_end( argList );

   logLength = strlen( aryStdPrintfLog );
   if ( logLength < sizeof( aryStdPrintfLog ) - 1 )
   {
      snprintf( aryStdPrintfLog + logLength, sizeof( aryStdPrintfLog ) - logLength, "%s", aryBuffer );
   }
   return 1;
}

int strCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   return strcmp( (const char *)ptrLeft, (const char *)ptrRight );
}

int sortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   const char *const *ptrLeftString;
   const char *const *ptrRightString;

   ptrLeftString = ptrLeft;
   ptrRightString = ptrRight;
   return strcmp( *ptrLeftString, *ptrRightString );
}

void truncateConfigFile( long userNameLength )
{
   (void)userNameLength;
}

int yesNo( void )
{
   if ( yesNoIndex < yesNoCount )
   {
      return aryYesNoQueue[yesNoIndex++];
   }
   return 0;
}

int yesNoDefault( int defaultAnswer )
{
   if ( yesNoIndex < yesNoCount )
   {
      return aryYesNoQueue[yesNoIndex++];
   }
   return defaultAnswer;
}

static void strCtrl_WhenControlCharacter_ReturnsCaretNotation( void **state )
{
   // Arrange
   char *ptrResult;

   (void)state;
   resetState();

   // Act
   ptrResult = strCtrl( CTRL_D );

   // Assert
   if ( strcmp( ptrResult, "^D" ) != 0 )
   {
      fail_msg( "strCtrl should map CTRL_D to '^D'; got '%s'", ptrResult );
   }
}

static void strCtrl_WhenPrintableCharacter_ReturnsSameCharacter( void **state )
{
   // Arrange
   char *ptrResult;

   (void)state;
   resetState();

   // Act
   ptrResult = strCtrl( 'Q' );

   // Assert
   if ( strcmp( ptrResult, "Q" ) != 0 )
   {
      fail_msg( "strCtrl should preserve printable characters; got '%s'", ptrResult );
   }
}

static void newKey_WhenConflictingKeyChosen_RetriesUntilAvailable( void **state )
{
   // Arrange
   const int aryKeys[] = { 'k', 'm' };
   int result;

   (void)state;
   resetState();

   commandKey = 'k';
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   captureKey = 'c';
   awayKey = 'a';
   browserKey = 'w';

   setGetKeySequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   result = newKey( 'q' );

   // Assert
   if ( result != 'm' )
   {
      fail_msg( "newKey should reject conflicting key and return next valid key; got %d", result );
   }
   if ( strstr( aryStdPrintfLog, "already in use for another hotkey" ) == NULL )
   {
      fail_msg( "newKey should print conflict guidance message when key is already used" );
   }
}

static void newKey_WhenUserEntersSpace_ReturnsOldKey( void **state )
{
   // Arrange
   const int aryKeys[] = { ' ' };
   int result;

   (void)state;
   resetState();
   setGetKeySequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   result = newKey( 'q' );

   // Assert
   if ( result != 'q' )
   {
      fail_msg( "newKey should keep old key on space/newline default; got %d", result );
   }
}

static void newAwayMessage_WhenUserDeclinesChange_PreservesExistingMessage( void **state )
{
   // Arrange
   const int aryAnswers[] = { 0 };

   (void)state;
   resetState();
   snprintf( aryAwayMessageLines[0], sizeof( aryAwayMessageLines[0] ), "%s", "Current away text." );
   aryAwayMessageLines[1][0] = '\0';
   setYesNoSequence( aryAnswers, sizeof( aryAnswers ) / sizeof( aryAnswers[0] ) );

   // Act
   newAwayMessage();

   // Assert
   if ( strcmp( aryAwayMessageLines[0], "Current away text." ) != 0 )
   {
      fail_msg( "newAwayMessage should preserve text when user declines; got '%s'", aryAwayMessageLines[0] );
   }
   if ( getStringCallCount != 0 )
   {
      fail_msg( "newAwayMessage should not prompt for new lines when user declines; getString calls=%d", getStringCallCount );
   }
}

static void newAwayMessage_WhenUserAcceptsChange_ReplacesWithEnteredLines( void **state )
{
   // Arrange
   const int aryAnswers[] = { 1 };
   const char *aryLines[] = { "Gone to lunch.", "Back by 2pm.", "" };

   (void)state;
   resetState();
   snprintf( aryAwayMessageLines[0], sizeof( aryAwayMessageLines[0] ), "%s", "Old away text." );
   aryAwayMessageLines[1][0] = '\0';
   setYesNoSequence( aryAnswers, sizeof( aryAnswers ) / sizeof( aryAnswers[0] ) );
   setStringSequence( aryLines, sizeof( aryLines ) / sizeof( aryLines[0] ) );

   // Act
   newAwayMessage();

   // Assert
   if ( strcmp( aryAwayMessageLines[0], "Gone to lunch." ) != 0 || strcmp( aryAwayMessageLines[1], "Back by 2pm." ) != 0 )
   {
      fail_msg( "newAwayMessage should save entered lines; got ['%s', '%s']", aryAwayMessageLines[0], aryAwayMessageLines[1] );
   }
   if ( aryAwayMessageLines[2][0] != '\0' )
   {
      fail_msg( "newAwayMessage should stop at blank line and leave trailing lines empty" );
   }
   if ( getStringCallCount != 3 )
   {
      fail_msg( "newAwayMessage should call getString until blank line; expected 3 calls, got %d", getStringCallCount );
   }
}

static void setup_WhenScreenReaderModeIsUnset_PromptsAndStoresAnswer( void **state )
{
   // Arrange
   const int aryPromptAnswers[] = { 1, 0 };

   (void)state;
   resetState();

   flagsConfiguration.hasScreenReaderModeSetting = 0;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   setPromptSequence( aryPromptAnswers, sizeof( aryPromptAnswers ) / sizeof( aryPromptAnswers[0] ) );

   cleanupWriteConfigFixture();
   ptrConfigFile = tmpfile();
   friendList = slistCreate( 0, fSortCompareVoid );
   enemyList = slistCreate( 0, sortCompareVoid );
   if ( ptrConfigFile == NULL || friendList == NULL || enemyList == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to initialize setup fixture" );
      return;
   }

   snprintf( aryEditor, sizeof( aryEditor ), "%s", "nano" );
   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.example.net" );
   bbsPort = 23;
   commandKey = ESC;
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   captureKey = 'c';
   awayKey = 'a';

   // Act
   setup( INT_VERSION );

   // Assert
   if ( !flagsConfiguration.hasScreenReaderModeSetting )
   {
      cleanupWriteConfigFixture();
      fail_msg( "setup should mark the screen reader mode setting as present after prompting" );
      return;
   }
   if ( !flagsConfiguration.isScreenReaderModeEnabled )
   {
      cleanupWriteConfigFixture();
      fail_msg( "setup should store the user's screen reader mode choice when they answer yes" );
      return;
   }
   if ( !flagsConfiguration.hasNameAutocompleteSetting )
   {
      cleanupWriteConfigFixture();
      fail_msg( "setup should mark the autocomplete setting as present after choosing a screen reader mode" );
      return;
   }
   if ( flagsConfiguration.shouldEnableNameAutocomplete )
   {
      cleanupWriteConfigFixture();
      fail_msg( "setup should default autocomplete off when screen reader mode is enabled" );
      return;
   }
   if ( sPromptCallCount < 2 )
   {
      cleanupWriteConfigFixture();
      fail_msg( "setup should prompt for screen reader mode before asking about advanced options; got %d prompts",
                sPromptCallCount );
      return;
   }

   cleanupWriteConfigFixture();
}

static void setup_WhenFirstRun_SkipsLegacyUpgradePrompts( void **state )
{
   // Arrange
   const int aryPromptAnswers[] = { 1 };

   (void)state;
   resetState();

   flagsConfiguration.hasNameAutocompleteSetting = 0;
   flagsConfiguration.hasScreenReaderModeSetting = 0;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   setPromptSequence( aryPromptAnswers, sizeof( aryPromptAnswers ) / sizeof( aryPromptAnswers[0] ) );

   cleanupWriteConfigFixture();
   ptrConfigFile = tmpfile();
   friendList = slistCreate( 0, fSortCompareVoid );
   enemyList = slistCreate( 0, sortCompareVoid );
   if ( ptrConfigFile == NULL || friendList == NULL || enemyList == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to initialize first-run setup fixture" );
      return;
   }

   snprintf( aryEditor, sizeof( aryEditor ), "%s", "nano" );
   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.example.net" );
   bbsPort = 23;
   commandKey = ESC;
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   captureKey = 'c';
   awayKey = 'a';
   browserKey = 'w';

   // Act
   setup( -1 );

   // Assert
   if ( sPromptCallCount != 1 )
   {
      cleanupWriteConfigFixture();
      fail_msg( "first-run setup should only prompt for screen reader mode; got %d prompts",
                sPromptCallCount );
      return;
   }
   if ( !flagsConfiguration.hasScreenReaderModeSetting ||
        !flagsConfiguration.isScreenReaderModeEnabled )
   {
      cleanupWriteConfigFixture();
      fail_msg( "first-run setup should still store the screen reader selection" );
      return;
   }
   if ( !flagsConfiguration.hasNameAutocompleteSetting ||
        flagsConfiguration.shouldEnableNameAutocomplete )
   {
      cleanupWriteConfigFixture();
      fail_msg( "first-run setup should still derive autocomplete defaults from screen reader mode" );
      return;
   }

   cleanupWriteConfigFixture();
}

static void configClient_WhenOptionsToggleScreenReaderMode_UpdatesFlags( void **state )
{
   // Arrange
   const int aryMenuKeys[] = { 'o', 'q' };
   const int aryYesNoAnswers[] = { 1, 0, 1, 1, 0 };

   (void)state;
   resetState();

   cleanupWriteConfigFixture();
   ptrConfigFile = tmpfile();
   friendList = slistCreate( 0, fSortCompareVoid );
   enemyList = slistCreate( 0, sortCompareVoid );
   if ( ptrConfigFile == NULL || friendList == NULL || enemyList == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to initialize configClient fixture" );
      return;
   }

   snprintf( aryEditor, sizeof( aryEditor ), "%s", "nano" );
   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.example.net" );
   bbsPort = 23;
   commandKey = ESC;
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   captureKey = 'c';
   awayKey = 'a';
   browserKey = 'w';
   rows = 24;
   isLoginShell = false;
   isConfigFileReadOnly = false;
   flagsConfiguration.shouldUseAnsi = 0;
   flagsConfiguration.shouldUseTcpKeepalive = 1;
   flagsConfiguration.shouldEnableClickableUrls = 1;
   flagsConfiguration.shouldEnableTitleBar = 1;
   flagsConfiguration.hasTitleBarSetting = 0;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   flagsConfiguration.hasScreenReaderModeSetting = 0;
   flagsConfiguration.shouldEnableNameAutocomplete = 1;
   flagsConfiguration.hasNameAutocompleteSetting = 0;

   setGetKeySequence( aryMenuKeys, sizeof( aryMenuKeys ) / sizeof( aryMenuKeys[0] ) );
   setYesNoSequence( aryYesNoAnswers, sizeof( aryYesNoAnswers ) / sizeof( aryYesNoAnswers[0] ) );

   // Act
   configClient();

   // Assert
   if ( !flagsConfiguration.isScreenReaderModeEnabled )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should enable screen reader mode when the option is answered yes" );
      return;
   }
   if ( !flagsConfiguration.hasScreenReaderModeSetting )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should mark screen reader mode as configured after toggling it" );
      return;
   }
   if ( !flagsConfiguration.shouldEnableClickableUrls )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should preserve the saved OSC-8 link setting when screen reader mode is enabled" );
      return;
   }
   if ( !flagsConfiguration.shouldEnableNameAutocomplete )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should preserve the saved autocomplete setting when screen reader mode is enabled" );
      return;
   }
   if ( !flagsConfiguration.shouldEnableTitleBar )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should preserve the saved title bar setting when screen reader mode is enabled" );
      return;
   }
   if ( !flagsConfiguration.hasNameAutocompleteSetting )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should mark autocomplete as configured after toggling it" );
      return;
   }
   if ( strstr( aryStdPrintfLog, "Use screen reader friendly mode?" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should display the screen reader mode option in the Options menu" );
      return;
   }
   if ( strstr( aryStdPrintfLog,
                "Update terminal title bar? (Off)\r\nForced off by screen reader mode.\r\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should show title bar updates as forced off when screen reader mode is enabled" );
      return;
   }
   if ( strstr( aryStdPrintfLog,
                "Enable OSC-8 links in posts and mail? (Off)\r\nForced off by screen reader mode.\r\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should show OSC-8 links as forced off when screen reader mode is enabled" );
      return;
   }
   if ( strstr( aryStdPrintfLog,
                "Autocomplete username in recipient prompts? (Off)\r\nForced off by screen reader mode.\r\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should show autocomplete as forced off when screen reader mode is enabled" );
      return;
   }
#ifndef ENABLE_KEYCHAIN
   if ( strstr( aryStdPrintfLog, "Use macOS Keychain for password storage?" ) != NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should hide the keychain option when keychain support is not compiled in" );
      return;
   }
#else
   if ( strstr( aryStdPrintfLog,
                "Use macOS Keychain for password storage? (No) -> " ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should display the keychain option when keychain support is compiled in" );
      return;
   }
#endif

   cleanupWriteConfigFixture();
}

#ifdef ENABLE_KEYCHAIN
static void configClient_WhenKeychainEnabled_ShowsNextLoginMessage( void **state )
{
   // Arrange
   const int aryMenuKeys[] = { 'o', 'q' };
   const int aryYesNoAnswers[] = { 0, 0, 0, 1, 1, 1, 1, 1 };

   (void)state;
   resetState();

   cleanupWriteConfigFixture();
   ptrConfigFile = tmpfile();
   friendList = slistCreate( 0, fSortCompareVoid );
   enemyList = slistCreate( 0, sortCompareVoid );
   if ( ptrConfigFile == NULL || friendList == NULL || enemyList == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to initialize configClient fixture" );
      return;
   }

   snprintf( aryEditor, sizeof( aryEditor ), "%s", "nano" );
   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.example.net" );
   bbsPort = 23;
   commandKey = ESC;
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   captureKey = 'c';
   awayKey = 'a';
   aryKeyMap['P'] = 'P';
   aryKeyMap['W'] = 'W';
   aryKeyMap['p'] = 'p';
   aryKeyMap['w'] = 'w';
   browserKey = 'w';
   rows = 24;
   isLoginShell = false;
   isConfigFileReadOnly = false;
   flagsConfiguration.shouldUseAnsi = 0;
   flagsConfiguration.shouldUseTcpKeepalive = 1;
   flagsConfiguration.shouldEnableClickableUrls = 1;
   flagsConfiguration.shouldEnableTitleBar = 1;
   flagsConfiguration.hasTitleBarSetting = 0;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   flagsConfiguration.hasScreenReaderModeSetting = 0;
   flagsConfiguration.shouldEnableNameAutocomplete = 1;
   flagsConfiguration.hasNameAutocompleteSetting = 1;
   flagsConfiguration.shouldUseKeychain = 0;

   setGetKeySequence( aryMenuKeys, sizeof( aryMenuKeys ) / sizeof( aryMenuKeys[0] ) );
   setYesNoSequence( aryYesNoAnswers, sizeof( aryYesNoAnswers ) / sizeof( aryYesNoAnswers[0] ) );

   // Act
   configClient();

   // Assert
   if ( !flagsConfiguration.shouldUseKeychain )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should enable keychain storage when the option is answered yes" );
      return;
   }
   if ( strstr( aryStdPrintfLog,
                "Keychain password storage will start after your next successful" ) == NULL ||
        strstr( aryStdPrintfLog,
                "Saved password autofill will be available on the login after that." ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should explain that keychain storage starts after the next successful login and autofill is available on the one after that; log was:\n%s",
                aryStdPrintfLog );
      return;
   }

   cleanupWriteConfigFixture();
}

static void configClient_WhenKeychainDisabled_DeletesCurrentBbsPassword( void **state )
{
   // Arrange
   const int aryMenuKeys[] = { 'o', 'q' };
   const int aryYesNoAnswers[] = { 0, 0, 0, 1, 1, 1, 1, 0 };

   (void)state;
   resetState();

   cleanupWriteConfigFixture();
   ptrConfigFile = tmpfile();
   friendList = slistCreate( 0, fSortCompareVoid );
   enemyList = slistCreate( 0, sortCompareVoid );
   if ( ptrConfigFile == NULL || friendList == NULL || enemyList == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to initialize configClient fixture" );
      return;
   }

   snprintf( aryEditor, sizeof( aryEditor ), "%s", "nano" );
   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.example.net" );
   bbsPort = 23;
   commandKey = ESC;
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   captureKey = 'c';
   awayKey = 'a';
   aryKeyMap['P'] = 'P';
   aryKeyMap['W'] = 'W';
   aryKeyMap['p'] = 'p';
   aryKeyMap['w'] = 'w';
   browserKey = 'w';
   rows = 24;
   isLoginShell = false;
   isConfigFileReadOnly = false;
   flagsConfiguration.shouldUseAnsi = 0;
   flagsConfiguration.shouldUseTcpKeepalive = 1;
   flagsConfiguration.shouldEnableClickableUrls = 1;
   flagsConfiguration.shouldEnableTitleBar = 1;
   flagsConfiguration.hasTitleBarSetting = 0;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   flagsConfiguration.hasScreenReaderModeSetting = 0;
   flagsConfiguration.shouldEnableNameAutocomplete = 1;
   flagsConfiguration.hasNameAutocompleteSetting = 1;
   flagsConfiguration.shouldUseKeychain = 1;
   isKeychainPasswordContextAvailable = true;
   shouldDeleteKeychainPasswordSucceed = true;

   setGetKeySequence( aryMenuKeys, sizeof( aryMenuKeys ) / sizeof( aryMenuKeys[0] ) );
   setYesNoSequence( aryYesNoAnswers, sizeof( aryYesNoAnswers ) / sizeof( aryYesNoAnswers[0] ) );

   // Act
   configClient();

   // Assert
   if ( flagsConfiguration.shouldUseKeychain )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should disable keychain storage when the option is answered no" );
      return;
   }
   if ( !isKeychainPasswordDeleted )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should delete the saved keychain password for the current BBS when keychain storage is turned off" );
      return;
   }
   if ( strstr( aryStdPrintfLog,
                "Saved Keychain password for this BBS deleted because Keychain" ) == NULL ||
        strstr( aryStdPrintfLog,
                "storage was turned off." ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should explain that the saved keychain password was deleted when keychain storage is turned off; log was:\n%s",
                aryStdPrintfLog );
      return;
   }
   if ( strstr( aryStdPrintfLog,
                "Forget saved Keychain password for this BBS? (No) -> " ) != NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should not show the manual keychain delete prompt after automatically deleting the saved password; log was:\n%s",
                aryStdPrintfLog );
      return;
   }

   cleanupWriteConfigFixture();
}

static void configClient_WhenForgetKeychainPasswordSelected_DeletesCurrentBbsPassword( void **state )
{
   // Arrange
   const int aryMenuKeys[] = { 'o', 'q' };
   const int aryYesNoAnswers[] = { 0, 0, 0, 1, 1, 1, 0, 1, 1 };

   (void)state;
   resetState();

   cleanupWriteConfigFixture();
   ptrConfigFile = tmpfile();
   friendList = slistCreate( 0, fSortCompareVoid );
   enemyList = slistCreate( 0, sortCompareVoid );
   if ( ptrConfigFile == NULL || friendList == NULL || enemyList == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to initialize configClient fixture" );
      return;
   }

   snprintf( aryEditor, sizeof( aryEditor ), "%s", "nano" );
   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.example.net" );
   bbsPort = 23;
   commandKey = ESC;
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   captureKey = 'c';
   awayKey = 'a';
   aryKeyMap['P'] = 'P';
   aryKeyMap['W'] = 'W';
   aryKeyMap['p'] = 'p';
   aryKeyMap['w'] = 'w';
   browserKey = 'w';
   rows = 24;
   isLoginShell = false;
   isConfigFileReadOnly = false;
   flagsConfiguration.shouldUseAnsi = 0;
   flagsConfiguration.shouldUseTcpKeepalive = 1;
   flagsConfiguration.shouldEnableClickableUrls = 1;
   flagsConfiguration.shouldEnableTitleBar = 1;
   flagsConfiguration.hasTitleBarSetting = 0;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   flagsConfiguration.hasScreenReaderModeSetting = 0;
   flagsConfiguration.shouldEnableNameAutocomplete = 1;
   flagsConfiguration.hasNameAutocompleteSetting = 1;
   flagsConfiguration.shouldUseKeychain = 0;
   isKeychainPasswordContextAvailable = true;
   shouldDeleteKeychainPasswordSucceed = true;

   setGetKeySequence( aryMenuKeys, sizeof( aryMenuKeys ) / sizeof( aryMenuKeys[0] ) );
   setYesNoSequence( aryYesNoAnswers, sizeof( aryYesNoAnswers ) / sizeof( aryYesNoAnswers[0] ) );

   // Act
   configClient();

   // Assert
   if ( !isKeychainPasswordDeleted )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should delete the saved keychain password for the current BBS when the user answers yes" );
      return;
   }
   if ( strstr( aryStdPrintfLog,
                "Forget saved Keychain password for this BBS? (No) -> " ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should display the per-BBS keychain delete prompt when keychain password context exists" );
      return;
   }
   if ( strstr( aryStdPrintfLog,
                "Saved Keychain password for this BBS deleted." ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "configClient should confirm that the saved keychain password was deleted; log was:\n%s",
                aryStdPrintfLog );
      return;
   }

   cleanupWriteConfigFixture();
}
#endif

static void writeConfig_WhenCoreSettingsEnabled_WritesTomlTrueValues( void **state )
{
   // Arrange
   char aryOutput[4096];
   char *ptrEnemyName;
   friend *ptrFriend;

   (void)state;
   resetState();

   cleanupWriteConfigFixture();
   ptrConfigFile = tmpfile();
   friendList = slistCreate( 0, fSortCompareVoid );
   enemyList = slistCreate( 0, sortCompareVoid );
   if ( ptrConfigFile == NULL || friendList == NULL || enemyList == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to initialize writeConfig fixture" );
      return;
   }

   snprintf( aryEditor, sizeof( aryEditor ), "%s", "nano" );
   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.example.net" );
   snprintf( aryAutoName, sizeof( aryAutoName ), "%s", "Alice" );
   bbsPort = 23;
   version = INT_VERSION;
   commandKey = ESC;
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   captureKey = 'c';
   awayKey = 'a';
   browserKey = 'w';
   aryKeyMap['p'] = 'P';
   aryKeyMap['P'] = 'p';
   aryKeyMap['w'] = 'W';
   aryKeyMap['W'] = 'w';
   configuredColorOutputMode = COLOR_OUTPUT_MODE_TRUECOLOR;
   useBlackThemeBackgrounds = true;
   color.text = 10;
   color.forum = 11;
   color.number = 220;
   color.errorTextColor = 9;
   color.ansiBlackTextColor = 16;
   color.ansiBlueTextColor = 26;
   color.ansiMagentaTextColor = 91;
   color.postDate = 34;
   color.postName = colorValueFromRgb( 0x8a, 0xad, 0xf4 );
   color.postText = 231;
   color.postFriendDate = 14;
   color.postFriendName = 12;
   color.postFriendText = 15;
   color.anonymous = 8;
   color.morePrompt = 13;
   color.ansiWhiteTextColor = 220;
   color.reserved5 = 999;
   color.background = COLOR_VALUE_DEFAULT;
   color.inputText = 44;
   color.inputHighlight = 166;
   color.expressText = 7;
   color.expressName = 12;
   color.expressFriendText = 214;
   color.expressFriendName = 231;
   copyColorTable( &color256, &color );
   copyColorTable( &colorTruecolor, &color );
   colorTruecolor.postName = colorValueFromRgb( 0x8a, 0xad, 0xf4 );
   useBlackThemeBackgrounds256 = true;
   useBlackThemeBackgroundsTruecolor = true;
   snprintf( aryAwayMessageLines[0], sizeof( aryAwayMessageLines[0] ), "%s", "Gone to lunch." );
   snprintf( aryAwayMessageLines[1], sizeof( aryAwayMessageLines[1] ), "%s", "Back by 2pm." );
   aryAwayMessageLines[2][0] = '\0';
   flagsConfiguration.shouldUseTcpKeepalive = true;
   flagsConfiguration.shouldAutoAnswerAnsiPrompt = true;
   flagsConfiguration.shouldEnableClickableUrls = true;
   flagsConfiguration.shouldEnableTitleBar = true;
   flagsConfiguration.isScreenReaderModeEnabled = true;
   flagsConfiguration.shouldEnableNameAutocomplete = false;
   flagsConfiguration.shouldSquelchExpress = true;
   flagsConfiguration.shouldSquelchPost = true;
   flagsConfiguration.shouldUseKeychain = false;
   isXland = false;
   ptrEnemyName = (char *)calloc( 1, strlen( "Mallory" ) + 1 );
   ptrFriend = (friend *)calloc( 1, sizeof( friend ) );
   if ( ptrEnemyName == NULL || ptrFriend == NULL )
   {
      free( ptrEnemyName );
      free( ptrFriend );
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to allocate contact fixtures for writeConfig test" );
      return;
   }
   snprintf( ptrEnemyName, strlen( "Mallory" ) + 1, "%s", "Mallory" );
   snprintf( ptrFriend->name, sizeof( ptrFriend->name ), "%s", "Bob" );
   snprintf( ptrFriend->info, sizeof( ptrFriend->info ), "%s", "A buddy" );
   ptrFriend->magic = 0x3231;
   if ( !slistAddItem( enemyList, ptrEnemyName, 1 ) ||
        !slistAddItem( friendList, ptrFriend, 1 ) )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to populate contact fixtures for writeConfig test" );
      return;
   }

   // Act
   writeConfig();

   // Assert
   if ( !tryReadFileIntoBuffer( ptrConfigFile, aryOutput, sizeof( aryOutput ) ) )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Assert failed: unable to read generated config.toml output" );
      return;
   }
   if ( strstr( aryOutput, "[connection]\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit a [connection] section; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "[metadata]\n" ) == NULL ||
        strstr( aryOutput, "version = 2310\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit a metadata version section; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "auto_login_name = \"Alice\"\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit auto_login_name as a TOML string; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "editor = \"nano\"\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit editor as a TOML string; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "host = \"bbs.example.net\"\n" ) == NULL ||
        strstr( aryOutput, "port = 23\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit the configured host and port in TOML form; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "[local_command_keys]\n" ) == NULL ||
        strstr( aryOutput, "away = \"a\"\n" ) == NULL ||
        strstr( aryOutput, "browser = \"w\"\n" ) == NULL ||
        strstr( aryOutput, "capture = \"c\"\n" ) == NULL ||
        strstr( aryOutput, "command = \"esc\"\n" ) == NULL ||
        strstr( aryOutput, "quit = \"ctrl-d\"\n" ) == NULL ||
        strstr( aryOutput, "shell = \"!\"\n" ) == NULL ||
        strstr( aryOutput, "suspend = \"ctrl-z\"\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit canonical TOML local-command key strings; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "[defaults]\n" ) == NULL ||
        strstr( aryOutput, "show_full_profile_by_default = true\n" ) == NULL ||
        strstr( aryOutput, "show_long_who_by_default = true\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit the semantic default-action toggles; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "[behavior]\n" ) == NULL ||
        strstr( aryOutput, "auto_answer_ansi = true\n" ) == NULL ||
        strstr( aryOutput, "auto_reply_to_x_messages = false\n" ) == NULL ||
        strstr( aryOutput, "autocomplete_recipients = false\n" ) == NULL ||
        strstr( aryOutput, "clickable_url_summaries = true\n" ) == NULL ||
        strstr( aryOutput, "dark_theme_black_background_fallback = true\n" ) == NULL ||
        strstr( aryOutput, "color_output_mode = \"truecolor\"\n" ) == NULL ||
        strstr( aryOutput, "screen_reader_mode = true\n" ) == NULL ||
        strstr( aryOutput, "suppress_enemy_express = true\n" ) == NULL ||
        strstr( aryOutput, "suppress_enemy_posts = true\n" ) == NULL ||
        strstr( aryOutput, "tcp_keepalive = true\n" ) == NULL ||
        strstr( aryOutput, "update_title_bar = true\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit TOML boolean behavior settings; output was:\n%s", aryOutput );
      return;
   }
#ifndef ENABLE_KEYCHAIN
   if ( strstr( aryOutput, "use_keychain = " ) != NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should omit use_keychain when keychain support is not compiled in; output was:\n%s",
                aryOutput );
      return;
   }
#else
   if ( strstr( aryOutput, "use_keychain = false\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit use_keychain = false when keychain support is compiled in and disabled; output was:\n%s",
                aryOutput );
      return;
   }
#endif
   if ( strstr( aryOutput, "[away]\n" ) == NULL ||
        strstr( aryOutput, "messages = [\"Gone to lunch.\", \"Back by 2pm.\"]\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit TOML away-message lines; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "[contacts]\n" ) == NULL ||
        strstr( aryOutput, "enemies = [\"Mallory\"]\n" ) == NULL ||
        strstr( aryOutput, "friends = [{ name = \"Bob\", info = \"A buddy\" }]\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit TOML contacts; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "[colors_256]\n" ) == NULL ||
        strstr( aryOutput, "text = \"brightgreen\"\n" ) == NULL ||
        strstr( aryOutput, "forum_prompt = \"brightyellow\"\n" ) == NULL ||
        strstr( aryOutput, "background = \"default\"\n" ) == NULL ||
        strstr( aryOutput, "input_text = \"cyan\"\n" ) == NULL ||
        strstr( aryOutput, "express_friend_name = \"white\"\n" ) == NULL ||
        strstr( aryOutput, "[colors_truecolor]\n" ) == NULL ||
        strstr( aryOutput, "post_name = \"#8aadf4\"\n" ) == NULL ||
        strstr( aryOutput, "reserved5" ) != NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit split TOML color tables with stable keys and omit reserved5; output was:\n%s", aryOutput );
      return;
   }

   cleanupWriteConfigFixture();
}

static void writeConfig_WhenCoreSettingsDisabled_WritesTomlFalseValues( void **state )
{
   // Arrange
   char aryOutput[4096];
   char *ptrEnemyName;
   friend *ptrFriend;

   (void)state;
   resetState();

   cleanupWriteConfigFixture();
   ptrConfigFile = tmpfile();
   friendList = slistCreate( 0, fSortCompareVoid );
   enemyList = slistCreate( 0, sortCompareVoid );
   if ( ptrConfigFile == NULL || friendList == NULL || enemyList == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to initialize writeConfig fixture" );
      return;
   }

   snprintf( aryEditor, sizeof( aryEditor ), "%s", "nano" );
   snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", "bbs.example.net" );
   aryAutoName[0] = '\0';
   bbsPort = 23;
   version = INT_VERSION;
   commandKey = ESC;
   quitKey = CTRL_D;
   suspKey = CTRL_Z;
   shellKey = '!';
   captureKey = 'c';
   awayKey = 'a';
   browserKey = 'w';
   aryKeyMap['p'] = 'p';
   aryKeyMap['P'] = 'P';
   aryKeyMap['w'] = 'w';
   aryKeyMap['W'] = 'W';
   configuredColorOutputMode = COLOR_OUTPUT_MODE_256;
   useBlackThemeBackgrounds = false;
   color.text = 2;
   color.forum = 3;
   color.number = 6;
   color.errorTextColor = 1;
   color.ansiBlackTextColor = 2;
   color.ansiBlueTextColor = 4;
   color.ansiMagentaTextColor = 5;
   color.postDate = 6;
   color.postName = 3;
   color.postText = 2;
   color.postFriendDate = 6;
   color.postFriendName = 3;
   color.postFriendText = 2;
   color.anonymous = 3;
   color.morePrompt = 3;
   color.ansiWhiteTextColor = 7;
   color.reserved5 = 1234;
   color.background = 0;
   color.inputText = 2;
   color.inputHighlight = 6;
   color.expressText = 2;
   color.expressName = 3;
   color.expressFriendText = 2;
   color.expressFriendName = 3;
   copyColorTable( &color256, &color );
   copyColorTable( &colorTruecolor, &color );
   useBlackThemeBackgrounds256 = false;
   useBlackThemeBackgroundsTruecolor = false;
   snprintf( aryAwayMessageLines[0], sizeof( aryAwayMessageLines[0] ), "%s", "Heads down coding." );
   aryAwayMessageLines[1][0] = '\0';
   flagsConfiguration.shouldUseTcpKeepalive = false;
   flagsConfiguration.shouldAutoAnswerAnsiPrompt = false;
   flagsConfiguration.shouldEnableClickableUrls = false;
   flagsConfiguration.shouldEnableTitleBar = false;
   flagsConfiguration.isScreenReaderModeEnabled = false;
   flagsConfiguration.shouldEnableNameAutocomplete = true;
   flagsConfiguration.shouldSquelchExpress = false;
   flagsConfiguration.shouldSquelchPost = false;
   flagsConfiguration.shouldUseKeychain = false;
   isXland = true;
#ifdef ENABLE_KEYCHAIN
   flagsConfiguration.shouldUseKeychain = true;
#endif
   ptrEnemyName = (char *)calloc( 1, strlen( "Eve" ) + 1 );
   ptrFriend = (friend *)calloc( 1, sizeof( friend ) );
   if ( ptrEnemyName == NULL || ptrFriend == NULL )
   {
      free( ptrEnemyName );
      free( ptrFriend );
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to allocate contact fixtures for disabled writeConfig test" );
      return;
   }
   snprintf( ptrEnemyName, strlen( "Eve" ) + 1, "%s", "Eve" );
   snprintf( ptrFriend->name, sizeof( ptrFriend->name ), "%s", "Carol" );
   snprintf( ptrFriend->info, sizeof( ptrFriend->info ), "%s", "(None)" );
   ptrFriend->magic = 0x3231;
   if ( !slistAddItem( enemyList, ptrEnemyName, 1 ) ||
        !slistAddItem( friendList, ptrFriend, 1 ) )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Arrange failed: unable to populate contact fixtures for disabled writeConfig test" );
      return;
   }

   // Act
   writeConfig();

   // Assert
   if ( !tryReadFileIntoBuffer( ptrConfigFile, aryOutput, sizeof( aryOutput ) ) )
   {
      cleanupWriteConfigFixture();
      fail_msg( "Assert failed: unable to read generated config.toml output" );
      return;
   }
   if ( strstr( aryOutput, "auto_login_name = " ) != NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should omit auto_login_name when no value is configured; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "[metadata]\n" ) == NULL ||
        strstr( aryOutput, "version = 2310\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit a metadata version section even in disabled-state fixtures; output was:\n%s",
                aryOutput );
      return;
   }
   if ( strstr( aryOutput, "show_full_profile_by_default = false\n" ) == NULL ||
        strstr( aryOutput, "show_long_who_by_default = false\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit false semantic default toggles when key remapping is disabled; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "auto_answer_ansi = false\n" ) == NULL ||
        strstr( aryOutput, "auto_reply_to_x_messages = true\n" ) == NULL ||
        strstr( aryOutput, "autocomplete_recipients = true\n" ) == NULL ||
        strstr( aryOutput, "clickable_url_summaries = false\n" ) == NULL ||
        strstr( aryOutput, "dark_theme_black_background_fallback = false\n" ) == NULL ||
        strstr( aryOutput, "color_output_mode = \"256\"\n" ) == NULL ||
        strstr( aryOutput, "screen_reader_mode = false\n" ) == NULL ||
        strstr( aryOutput, "suppress_enemy_express = false\n" ) == NULL ||
        strstr( aryOutput, "suppress_enemy_posts = false\n" ) == NULL ||
        strstr( aryOutput, "tcp_keepalive = false\n" ) == NULL ||
        strstr( aryOutput, "update_title_bar = false\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit TOML false values for disabled behavior settings; output was:\n%s", aryOutput );
      return;
   }
#ifndef ENABLE_KEYCHAIN
   if ( strstr( aryOutput, "use_keychain = " ) != NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should omit use_keychain when keychain support is not compiled in; output was:\n%s",
                aryOutput );
      return;
   }
#else
   if ( strstr( aryOutput, "use_keychain = true\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit use_keychain = true when keychain support is compiled in and enabled; output was:\n%s",
                aryOutput );
      return;
   }
#endif
   if ( strstr( aryOutput, "[away]\n" ) == NULL ||
        strstr( aryOutput, "messages = [\"Heads down coding.\"]\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit TOML away-message lines when a single line is configured; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "[contacts]\n" ) == NULL ||
        strstr( aryOutput, "enemies = [\"Eve\"]\n" ) == NULL ||
        strstr( aryOutput, "friends = [{ name = \"Carol\", info = \"(None)\" }]\n" ) == NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit TOML contacts for the disabled-state fixture; output was:\n%s", aryOutput );
      return;
   }
   if ( strstr( aryOutput, "[colors_256]\n" ) == NULL ||
        strstr( aryOutput, "text = 2\n" ) == NULL ||
        strstr( aryOutput, "incoming_ansi_blue = 4\n" ) == NULL ||
        strstr( aryOutput, "background = 0\n" ) == NULL ||
        strstr( aryOutput, "[colors_truecolor]\n" ) == NULL ||
        strstr( aryOutput, "reserved5" ) != NULL )
   {
      cleanupWriteConfigFixture();
      fail_msg( "writeConfig should emit split numeric TOML color tables when no named form exists and omit reserved5; output was:\n%s", aryOutput );
      return;
   }

   cleanupWriteConfigFixture();
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( strCtrl_WhenControlCharacter_ReturnsCaretNotation ),
      cmocka_unit_test( strCtrl_WhenPrintableCharacter_ReturnsSameCharacter ),
      cmocka_unit_test( newKey_WhenConflictingKeyChosen_RetriesUntilAvailable ),
      cmocka_unit_test( newKey_WhenUserEntersSpace_ReturnsOldKey ),
      cmocka_unit_test( newAwayMessage_WhenUserDeclinesChange_PreservesExistingMessage ),
      cmocka_unit_test( newAwayMessage_WhenUserAcceptsChange_ReplacesWithEnteredLines ),
      cmocka_unit_test( setup_WhenScreenReaderModeIsUnset_PromptsAndStoresAnswer ),
      cmocka_unit_test( setup_WhenFirstRun_SkipsLegacyUpgradePrompts ),
      cmocka_unit_test( configClient_WhenOptionsToggleScreenReaderMode_UpdatesFlags ),
#ifdef ENABLE_KEYCHAIN
      cmocka_unit_test( configClient_WhenKeychainEnabled_ShowsNextLoginMessage ),
      cmocka_unit_test( configClient_WhenKeychainDisabled_DeletesCurrentBbsPassword ),
      cmocka_unit_test( configClient_WhenForgetKeychainPasswordSelected_DeletesCurrentBbsPassword ),
#endif
      cmocka_unit_test( writeConfig_WhenCoreSettingsEnabled_WritesTomlTrueValues ),
      cmocka_unit_test( writeConfig_WhenCoreSettingsDisabled_WritesTomlFalseValues ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
