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
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/stat.h>
#include "telnet.h"
#include "test_helpers.h"
#include <unistd.h>
#include "utility.h"
static int setupCallCount;
static int setupVersionArg;
static int perrorCallCount;
static int writeBbsRcCallCount;
static int promptForScreenReaderModeCallCount;
static int defaultNameAutocompleteIfUnsetCallCount;
static char aryLastPerrorMessage[128];
static char aryLastPerrorHeading[64];
static char aryStdPrintfLog[16384];

static void resetTracking( void )
{
   setupCallCount = 0;
   setupVersionArg = 0;
   perrorCallCount = 0;
   writeBbsRcCallCount = 0;
   promptForScreenReaderModeCallCount = 0;
   defaultNameAutocompleteIfUnsetCallCount = 0;
   aryLastPerrorMessage[0] = '\0';
   aryLastPerrorHeading[0] = '\0';
   aryStdPrintfLog[0] = '\0';
}

static void cleanupReadState( void )
{
   if ( ptrBbsRc != NULL )
   {
      fclose( ptrBbsRc );
      ptrBbsRc = NULL;
   }
   if ( bbsFriends != NULL )
   {
      fclose( bbsFriends );
      bbsFriends = NULL;
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
   if ( whoList != NULL )
   {
      slistDestroyItems( whoList );
      slistDestroy( whoList );
      whoList = NULL;
   }
   if ( xlandQueue != NULL )
   {
      deleteQueue( xlandQueue );
      xlandQueue = NULL;
   }
   if ( urlQueue != NULL )
   {
      deleteQueue( urlQueue );
      urlQueue = NULL;
   }
}

// bbsrc.c dependencies that are not under test here.
static int *const aryTestColorFields[COLOR_FIELD_COUNT] =
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

void configBbsRc( void )
{
   // Test stub: interactive config flow is not relevant in this test.
}

void defaultColors( int setall )
{
   (void)setall;
}

/// @brief Return one test color field in the legacy `.bbsrc` serialization order.
///
/// @param colorIndex Field index in the `.bbsrc` color line.
///
/// @return Configured test color value at the requested index.
int colorFieldValue( int colorIndex )
{
   assert( colorIndex >= 0 );
   assert( colorIndex < COLOR_FIELD_COUNT );

   return *aryTestColorFields[colorIndex];
}

int colorValueFromLegacyDigit( int inputChar )
{
   if ( inputChar >= '0' && inputChar <= '9' )
   {
      return inputChar - '0';
   }

   return inputChar;
}

int colorValueFromName( const char *ptrColorName )
{
   if ( strcmp( ptrColorName, "brightblack" ) == 0 )
   {
      return 8;
   }
   if ( strcmp( ptrColorName, "brightred" ) == 0 )
   {
      return 9;
   }
   if ( strcmp( ptrColorName, "brightgreen" ) == 0 )
   {
      return 10;
   }
   if ( strcmp( ptrColorName, "brightyellow" ) == 0 )
   {
      return 11;
   }
   if ( strcmp( ptrColorName, "brightblue" ) == 0 )
   {
      return 12;
   }
   if ( strcmp( ptrColorName, "brightmagenta" ) == 0 || strcmp( ptrColorName, "brightpurple" ) == 0 )
   {
      return 13;
   }
   if ( strcmp( ptrColorName, "brightcyan" ) == 0 )
   {
      return 14;
   }
   if ( strcmp( ptrColorName, "brightwhite" ) == 0 )
   {
      return 15;
   }
   if ( strcmp( ptrColorName, "black" ) == 0 )
   {
      return 16;
   }
   if ( strcmp( ptrColorName, "red" ) == 0 )
   {
      return 160;
   }
   if ( strcmp( ptrColorName, "green" ) == 0 )
   {
      return 34;
   }
   if ( strcmp( ptrColorName, "yellow" ) == 0 )
   {
      return 220;
   }
   if ( strcmp( ptrColorName, "blue" ) == 0 )
   {
      return 26;
   }
   if ( strcmp( ptrColorName, "magenta" ) == 0 || strcmp( ptrColorName, "purple" ) == 0 )
   {
      return 91;
   }
   if ( strcmp( ptrColorName, "cyan" ) == 0 )
   {
      return 44;
   }
   if ( strcmp( ptrColorName, "white" ) == 0 )
   {
      return 231;
   }
   if ( strcmp( ptrColorName, "default" ) == 0 )
   {
      return COLOR_VALUE_DEFAULT;
   }

   return -1;
}

/// @brief Set one test color field in the legacy `.bbsrc` serialization order.
///
/// @param colorIndex Field index in the `.bbsrc` color line.
/// @param colorValue New color value.
///
/// @return This function does not return a value.
void setColorFieldValue( int colorIndex, int colorValue )
{
   assert( colorIndex >= 0 );
   assert( colorIndex < COLOR_FIELD_COUNT );

   *aryTestColorFields[colorIndex] = colorValue;
}

int fSortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   const friend *const *ptrLeftFriend;
   const friend *const *ptrRightFriend;

   ptrLeftFriend = ptrLeft;
   ptrRightFriend = ptrRight;
   return strcmp( ( *ptrLeftFriend )->name, ( *ptrRightFriend )->name );
}

int fStrCompareVoid( const void *ptrName, const void *ptrFriend )
{
   return strcmp( (const char *)ptrName, ( (const friend *)ptrFriend )->name );
}

noreturn void fatalExit( const char *message, const char *heading )
{
   fail_msg( "fatalExit invoked unexpectedly: %s (%s)", message, heading );
   abort();
}

char *findChar( const char *ptrString, int targetChar )
{
   const char *ptrSearch;

   for ( ptrSearch = ptrString; *ptrSearch && *ptrSearch != targetChar; ++ptrSearch )
   {
      ;
   }
   if ( *ptrSearch == targetChar )
   {
      return (char *)ptrSearch;
   }
   return NULL;
}

FILE *findBbsRc( void )
{
   return openBbsRc();
}

FILE *findBbsFriends( void )
{
   return NULL;
}

void resetTerm( void )
{
   // Test stub: terminal reset behavior is not relevant in this test.
}

void setTerm( void )
{
   // Test stub: terminal setup behavior is not relevant in this test.
}

void setup( int oldversion )
{
   setupCallCount++;
   setupVersionArg = oldversion;
}

void writeBbsRc( void )
{
   writeBbsRcCallCount++;
}

void promptForScreenReaderModeIfUnset( void )
{
   promptForScreenReaderModeCallCount++;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   flagsConfiguration.hasScreenReaderModeSetting = 1;
}

void defaultNameAutocompleteIfUnset( void )
{
   defaultNameAutocompleteIfUnsetCallCount++;
   flagsConfiguration.shouldEnableNameAutocomplete =
      (unsigned int)!flagsConfiguration.isScreenReaderModeEnabled;
   flagsConfiguration.hasNameAutocompleteSetting = 1;
}

void sPerror( const char *message, const char *heading )
{
   perrorCallCount++;
   snprintf( aryLastPerrorMessage, sizeof( aryLastPerrorMessage ), "%s", message );
   snprintf( aryLastPerrorHeading, sizeof( aryLastPerrorHeading ), "%s", heading );
}

int sortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   const char *const *ptrLeftString;
   const char *const *ptrRightString;

   ptrLeftString = ptrLeft;
   ptrRightString = ptrRight;
   return strcmp( *ptrLeftString, *ptrRightString );
}

int strCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   return strcmp( (const char *)ptrLeft, (const char *)ptrRight );
}

int stdPrintf( const char *format, ... )
{
   va_list argList;
   char aryBuffer[512];
   size_t currentLength;

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

   currentLength = strlen( aryStdPrintfLog );
   if ( currentLength < sizeof( aryStdPrintfLog ) - 1 )
   {
      snprintf( aryStdPrintfLog + currentLength,
                sizeof( aryStdPrintfLog ) - currentLength,
                "%s",
                aryBuffer );
   }
   return 0;
}

int readNormalizedLine(
   FILE *ptrFileHandle, char *ptrLine, size_t lineSize, int *ptrLineNumber, int *ptrReadCount, const char *ptrLabel )
{
   char *ptrNewline;

   (void)ptrLabel;
   if ( ptrFileHandle == NULL || ptrLine == NULL || lineSize == 0 )
   {
      return 0;
   }

   if ( fgets( ptrLine, (int)lineSize, ptrFileHandle ) == NULL )
   {
      return 0;
   }
   ptrNewline = strchr( ptrLine, '\n' );
   if ( ptrNewline != NULL )
   {
      *ptrNewline = '\0';
   }
   ptrNewline = strchr( ptrLine, '\r' );
   if ( ptrNewline != NULL )
   {
      *ptrNewline = '\0';
   }
   if ( ptrLineNumber != NULL )
   {
      ( *ptrLineNumber )++;
   }
   if ( ptrReadCount != NULL )
   {
      ( *ptrReadCount )++;
   }
   return 1;
}

static void openBbsRc_WhenPathMissing_CreatesWritableConfigurationFile( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];
   FILE *ptrFile;
   struct stat fileStats;

   (void)state;

   resetTracking();
   isBbsRcReadOnly = 0;
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for openBbsRc missing-path test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );

   // Act
   ptrFile = openBbsRc();

   // Assert
   if ( ptrFile == NULL )
   {
      fail_msg( "openBbsRc should create and open a missing configuration file" );
   }
   if ( isBbsRcReadOnly != 0 )
   {
      fail_msg( "openBbsRc should not mark a newly created file as read-only" );
   }
   if ( perrorCallCount != 0 )
   {
      fail_msg( "openBbsRc should not call sPerror on successful create/open; got %d calls", perrorCallCount );
   }
   if ( stat( aryPath, &fileStats ) != 0 )
   {
      fail_msg( "openBbsRc should create the configuration file on disk" );
   }

   fclose( ptrFile );
   unlink( aryPath );
}

static void openBbsRc_WhenParentDirectoriesMissing_CreatesConfigDirectoryTree( void **state )
{
   // Arrange
   char aryConfigPath[PATH_MAX];
   char aryDirectoryTemplate[] = "/tmp/iobbs_config_test_XXXXXX";
   char *ptrTempDirectory;
   FILE *ptrFile;
   struct stat fileStats;
   struct stat innerDirectoryStats;
   struct stat outerDirectoryStats;

   (void)state;

   resetTracking();
   isBbsRcReadOnly = 0;
   ptrTempDirectory = mkdtemp( aryDirectoryTemplate );
   if ( ptrTempDirectory == NULL )
   {
      fail_msg( "Arrange failed: unable to create temporary directory for openBbsRc config-tree test" );
      return;
   }
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config/bbs/config.toml",
             ptrTempDirectory );
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryConfigPath );

   // Act
   ptrFile = openBbsRc();

   // Assert
   if ( ptrFile == NULL )
   {
      fail_msg( "openBbsRc should create and open the config file when parent directories are missing" );
   }
   if ( stat( aryConfigPath, &fileStats ) != 0 )
   {
      fail_msg( "openBbsRc should create the config file on disk" );
   }
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config", ptrTempDirectory );
   if ( stat( aryConfigPath, &outerDirectoryStats ) != 0 || !S_ISDIR( outerDirectoryStats.st_mode ) )
   {
      fail_msg( "openBbsRc should create the outer config directory" );
   }
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config/bbs", ptrTempDirectory );
   if ( stat( aryConfigPath, &innerDirectoryStats ) != 0 || !S_ISDIR( innerDirectoryStats.st_mode ) )
   {
      fail_msg( "openBbsRc should create the app-specific config directory" );
   }

   // Cleanup
   fclose( ptrFile );
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config/bbs/config.toml",
             ptrTempDirectory );
   unlink( aryConfigPath );
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config/bbs", ptrTempDirectory );
   rmdir( aryConfigPath );
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config", ptrTempDirectory );
   rmdir( aryConfigPath );
   rmdir( ptrTempDirectory );
}

static void openBbsRc_WhenPathIsReadOnly_SetsReadOnlyAndWarns( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];
   FILE *ptrFile;

   (void)state;

   resetTracking();
   isBbsRcReadOnly = 0;
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for openBbsRc read-only test" );
      return;
   }
   if ( !tryWriteFileContents( aryPath, "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration file for openBbsRc read-only test" );
      return;
   }
   chmod( aryPath, S_IRUSR | S_IRGRP | S_IROTH );
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );

   // Act
   ptrFile = openBbsRc();

   // Assert
   if ( ptrFile == NULL )
   {
      fail_msg( "openBbsRc should fall back to read mode when file is read-only" );
   }
   if ( isBbsRcReadOnly != 1 )
   {
      fail_msg( "openBbsRc should set isBbsRcReadOnly when only read access is available" );
   }
   if ( perrorCallCount != 1 )
   {
      fail_msg( "openBbsRc should issue one warning for read-only configuration; got %d", perrorCallCount );
   }
   if ( strcmp( aryLastPerrorMessage, "Configuration is read-only" ) != 0 )
   {
      fail_msg( "openBbsRc warning text mismatch; got '%s'", aryLastPerrorMessage );
   }

   fclose( ptrFile );
   chmod( aryPath, S_IRUSR | S_IWUSR );
   unlink( aryPath );
}

static void readBbsRc_WhenConfigIsEmpty_AppliesDefaults( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for readBbsRc empty-config test" );
      return;
   }
   if ( !tryWriteFileContents( aryPath, "" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write empty configuration for readBbsRc empty-config test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( commandKey != ESC )
   {
      fail_msg( "default commandKey should be ESC; got %d", commandKey );
   }
   if ( awayKey != 'a' || quitKey != CTRL_D || suspKey != CTRL_Z || captureKey != 'c' || shellKey != '!' || browserKey != 'w' )
   {
      fail_msg( "default key bindings were not applied as expected" );
   }
   if ( strcmp( aryBbsHost, BBS_HOSTNAME ) != 0 || bbsPort != BBS_PORT_NUMBER )
   {
      fail_msg( "default site should be %s:%d, got %s:%u", BBS_HOSTNAME, BBS_PORT_NUMBER, aryBbsHost, bbsPort );
   }
   if ( strcmp( aryEditor, DEFAULT_EDITOR_CONFIG_VALUE ) != 0 )
   {
      fail_msg( "default editor should use the symbolic $EDITOR value; got '%s'", aryEditor );
   }
   if ( strcmp( aryAwayMessageLines[0], "I'm away from my keyboard right now." ) != 0 )
   {
      fail_msg( "default away message mismatch; got '%s'", aryAwayMessageLines[0] );
   }
   if ( setupCallCount != 1 || setupVersionArg != -1 )
   {
      fail_msg( "empty config should trigger setup(-1); got count=%d arg=%d", setupCallCount, setupVersionArg );
   }
   if ( !flagsConfiguration.shouldUseTcpKeepalive )
   {
      fail_msg( "default config should enable TCP keepalive" );
   }
   if ( !flagsConfiguration.shouldEnableClickableUrls )
   {
      fail_msg( "default config should enable clickable URL summaries" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigHasEntries_ParsesValuesAndIgnoresDuplicateEnemy( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];
   const friend *ptrFriendEntry;

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for readBbsRc parse-config test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "site bbs.example.net 2323 secure\n"
           "commandkey ^[\n"
           "quit ^D\n"
           "susp ^Z\n"
           "capture c\n"
           "awaykey a\n"
           "shellkey !\n"
           "aryEditor nano\n"
           "a1 Back in five.\n"
           "enemy Meatball\n"
           "enemy Meatball\n"
           "friend Dr Strange           Time traveler\n"
           "version 2310\n"
           "xland\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for readBbsRc parse-config test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "vi" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strcmp( aryBbsHost, "bbs.example.net" ) != 0 || bbsPort != 2323 )
   {
      fail_msg( "site parsing should ignore legacy secure suffixes; expected bbs.example.net:2323, got %s:%u",
                aryBbsHost, bbsPort );
   }
   if ( commandKey != ESC || quitKey != CTRL_D || suspKey != CTRL_Z || captureKey != 'c' || awayKey != 'a' || shellKey != '!' )
   {
      fail_msg( "configured key bindings were not parsed correctly" );
   }
   if ( isXland != 0 )
   {
      fail_msg( "xland directive should disable xland mode; got isXland=%d", isXland );
   }
   if ( strcmp( aryEditor, "nano" ) != 0 )
   {
      fail_msg( "aryEditor parsing failed; expected 'nano', got '%s'", aryEditor );
   }
   if ( strcmp( aryAwayMessageLines[0], "Back in five." ) != 0 )
   {
      fail_msg( "away message parsing failed; got '%s'", aryAwayMessageLines[0] );
   }
   if ( enemyList == NULL || enemyList->nitems != 1 )
   {
      fail_msg( "duplicate enemy entries should be ignored; expected 1 enemy, got %u",
                enemyList == NULL ? 0u : enemyList->nitems );
   }
   if ( friendList == NULL || friendList->nitems != 1 || whoList == NULL || whoList->nitems != 1 )
   {
      fail_msg( "friend parsing/copy failed; expected 1 friend and 1 who entry" );
   }
   ptrFriendEntry = friendList->items[0];
   if ( strcmp( ptrFriendEntry->name, "Dr Strange" ) != 0 )
   {
      fail_msg( "friend name parsing failed; expected 'Dr Strange', got '%s'", ptrFriendEntry->name );
   }
   if ( setupCallCount != 0 )
   {
      fail_msg( "version-matched config should not call setup(); got %d calls", setupCallCount );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsObsoleteBrowserSetting_RewritesAndWarns( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for legacy browser migration test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "aryBrowser 1 netscape -remote\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write obsolete browser configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "IMPORTANT: Your browser preference was removed due to client updates." ) == NULL )
   {
      fail_msg( "obsolete browser setting should emit migration warning; log was: %s", aryStdPrintfLog );
   }
   if ( strstr( aryStdPrintfLog, "The client now relies on terminal links and the macOS default browser." ) == NULL )
   {
      fail_msg( "obsolete browser setting should emit updated migration guidance; log was: %s", aryStdPrintfLog );
   }
   if ( writeBbsRcCallCount != 1 )
   {
      fail_msg( "obsolete browser setting should trigger one .bbsrc rewrite; got %d",
                writeBbsRcCallCount );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigUsesLegacyIscaIp_RewritesToCanonicalHostname( void **state )
{
   char aryPath[PATH_MAX];
   FILE *ptrConfig;

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for legacy ISCA IP migration test" );
      return;
   }

   ptrConfig = fopen( aryPath, "w" );
   if ( ptrConfig == NULL )
   {
      fail_msg( "Arrange failed: unable to open temporary config file" );
      unlink( aryPath );
      return;
   }
   fputs( "site 206.217.131.27 23\n", ptrConfig );
   fclose( ptrConfig );

   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strcmp( aryBbsHost, BBS_HOSTNAME ) != 0 )
   {
      fail_msg( "legacy ISCA IP should migrate to %s; got %s", BBS_HOSTNAME, aryBbsHost );
   }
   if ( bbsPort != BBS_PORT_NUMBER )
   {
      fail_msg( "legacy ISCA IP should keep the default port; got %u", bbsPort );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigSetsKeepaliveZero_DisablesTcpKeepalive( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for keepalive=0 test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "keepalive 0\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for keepalive=0 test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( flagsConfiguration.shouldUseTcpKeepalive )
   {
      fail_msg( "keepalive 0 should disable TCP keepalive" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigSetsScreenReaderOne_EnablesScreenReaderMode( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for screenreader=1 test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "screenreader 1\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for screenreader=1 test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( !flagsConfiguration.isScreenReaderModeEnabled )
   {
      fail_msg( "screenreader 1 should enable screen reader mode" );
   }
   if ( !flagsConfiguration.hasScreenReaderModeSetting )
   {
      fail_msg( "screenreader 1 should mark the setting as present" );
   }
   if ( promptForScreenReaderModeCallCount != 0 )
   {
      fail_msg( "screenreader 1 should not trigger the missing-setting prompt helper; got %d calls",
                promptForScreenReaderModeCallCount );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigSetsTitleBarZero_DisablesTitleBarUpdates( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for titlebar=0 test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "titlebar 0\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for titlebar=0 test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( flagsConfiguration.shouldEnableTitleBar )
   {
      fail_msg( "titlebar 0 should disable terminal title updates" );
   }
   if ( !flagsConfiguration.hasTitleBarSetting )
   {
      fail_msg( "titlebar 0 should mark the title bar setting as present" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigSetsAutocompleteZero_DisablesAutocomplete( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for autocomplete=0 test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "autocomplete 0\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for autocomplete=0 test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( flagsConfiguration.shouldEnableNameAutocomplete )
   {
      fail_msg( "autocomplete 0 should disable name autocomplete" );
   }
   if ( !flagsConfiguration.hasNameAutocompleteSetting )
   {
      fail_msg( "autocomplete 0 should mark the setting as present" );
   }

   cleanupReadState();
   unlink( aryPath );
}

#ifndef ENABLE_KEYCHAIN
static void readBbsRc_WhenConfigSetsKeychainOneWithoutBuildSupport_IgnoresSetting( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for keychain=1 test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "keychain 1\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for keychain=1 test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( flagsConfiguration.shouldUseKeychain )
   {
      fail_msg( "keychain 1 should be ignored when keychain support is not compiled in" );
   }
   if ( strstr( aryStdPrintfLog, "Invalid definition of keychain option ignored." ) != NULL )
   {
      fail_msg( "keychain 1 should be accepted syntactically even when keychain support is not compiled in; log was: %s",
                aryStdPrintfLog );
   }

   cleanupReadState();
   unlink( aryPath );
}
#endif

#ifdef ENABLE_KEYCHAIN
static void readBbsRc_WhenConfigSetsKeychainOne_EnablesKeychain( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for keychain=1 test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "keychain 1\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for keychain=1 test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( !flagsConfiguration.shouldUseKeychain )
   {
      fail_msg( "keychain 1 should enable runtime keychain support when compiled in" );
   }
   if ( strstr( aryStdPrintfLog, "Invalid definition of keychain option ignored." ) != NULL )
   {
      fail_msg( "keychain 1 should parse cleanly when keychain support is compiled in; log was: %s",
                aryStdPrintfLog );
   }

   cleanupReadState();
   unlink( aryPath );
}
#endif

static void readBbsRc_WhenConfigMissingScreenReaderSetting_PromptsAndRewrites( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for missing screenreader test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "keepalive 1\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for missing screenreader test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( promptForScreenReaderModeCallCount != 1 )
   {
      fail_msg( "missing screenreader setting should trigger one prompt helper call; got %d",
                promptForScreenReaderModeCallCount );
   }
   if ( writeBbsRcCallCount != 1 )
   {
      fail_msg( "missing screenreader setting should rewrite .bbsrc once; got %d",
                writeBbsRcCallCount );
   }
   if ( !flagsConfiguration.hasScreenReaderModeSetting )
   {
      fail_msg( "prompt helper should mark the screen reader setting as present" );
   }
   if ( defaultNameAutocompleteIfUnsetCallCount != 1 )
   {
      fail_msg( "missing autocomplete setting should trigger one default helper call; got %d",
                defaultNameAutocompleteIfUnsetCallCount );
   }
   if ( !flagsConfiguration.hasNameAutocompleteSetting )
   {
      fail_msg( "autocomplete default helper should mark the setting as present" );
   }
   if ( !flagsConfiguration.shouldEnableNameAutocomplete )
   {
      fail_msg( "autocomplete should default on when screen reader mode is disabled" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenAutocompleteMissingAndScreenReaderEnabled_DefaultsAutocompleteOff( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for missing autocomplete test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "screenreader 1\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for missing autocomplete test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( defaultNameAutocompleteIfUnsetCallCount != 1 )
   {
      fail_msg( "missing autocomplete setting should trigger one default helper call; got %d",
                defaultNameAutocompleteIfUnsetCallCount );
   }
   if ( !flagsConfiguration.hasNameAutocompleteSetting )
   {
      fail_msg( "missing autocomplete setting should be marked present after defaulting" );
   }
   if ( flagsConfiguration.shouldEnableNameAutocomplete )
   {
      fail_msg( "autocomplete should default off when screen reader mode is enabled" );
   }
   if ( writeBbsRcCallCount != 1 )
   {
      fail_msg( "missing autocomplete setting should rewrite .bbsrc once; got %d",
                writeBbsRcCallCount );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigSetsClickableUrlsZero_DisablesClickableUrls( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for clickableurls=0 test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "clickableurls 0\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for clickableurls=0 test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( flagsConfiguration.shouldEnableClickableUrls )
   {
      fail_msg( "clickableurls 0 should disable clickable URL summaries" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigSetsClickableUrlsOne_EnablesClickableUrls( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for clickableurls=1 test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "clickableurls 1\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for clickableurls=1 test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( !flagsConfiguration.shouldEnableClickableUrls )
   {
      fail_msg( "clickableurls 1 should enable clickable URL summaries" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsInvalidClickableUrlsDefinition_PrintsWarningAndKeepsDefault( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for invalid clickableurls test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "clickableurls maybe\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write invalid clickableurls configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "Invalid definition of clickable URL option ignored." ) == NULL )
   {
      fail_msg( "invalid clickable URL definition should emit warning; log was: %s", aryStdPrintfLog );
   }
   if ( !flagsConfiguration.shouldEnableClickableUrls )
   {
      fail_msg( "invalid clickable URL definition should keep default enabled state" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigSetsKeepaliveOne_EnablesTcpKeepalive( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for keepalive=1 test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "keepalive 1\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for keepalive=1 test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( !flagsConfiguration.shouldUseTcpKeepalive )
   {
      fail_msg( "keepalive 1 should enable TCP keepalive" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsMalformedKeepalive_PrintsWarningAndKeepsDefault( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for malformed keepalive test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "keepaliveX\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write malformed keepalive configuration" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "Invalid definition of 'keepalive' ignored." ) == NULL )
   {
      fail_msg( "malformed keepalive definition should emit warning; log was: %s", aryStdPrintfLog );
   }
   if ( !flagsConfiguration.shouldUseTcpKeepalive )
   {
      fail_msg( "malformed keepalive definition should leave default enabled state unchanged" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsInvalidDirective_PrintsSyntaxError( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for invalid-directive test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "site bbs.example.net 2323\n"
           "mysterysetting 42\n"
           "quit ^D\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for invalid-directive test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "Syntax error in .bbsrc file in line 2." ) == NULL )
   {
      fail_msg( "invalid directive should emit syntax error with correct line number; log was: %s", aryStdPrintfLog );
   }
   if ( quitKey != CTRL_D )
   {
      fail_msg( "parser should continue after bad line; expected quit key Ctrl-D, got %d", quitKey );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsInvalidColor_PrintsWarning( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for invalid-color test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "color 12345\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for invalid-color test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "Invalid 'color' scheme on line 1, ignored." ) == NULL )
   {
      fail_msg( "invalid color definition should emit warning; log was: %s", aryStdPrintfLog );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigUsesNamedColors_ParsesExtendedPaletteValues( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for named-color test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "color green yellow cyan red black black black magenta blue white red green yellow blue cyan black black default white green yellow magenta cyan purple\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write named-color configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( color.text != 34 || color.forum != 220 || color.number != 44 || color.errorTextColor != 160 )
   {
      fail_msg( "named color parsing should set general colors to extended palette values; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.postDate != 91 || color.postName != 26 || color.postText != 231 )
   {
      fail_msg( "named color parsing should set post colors; got date=%d name=%d text=%d",
                color.postDate, color.postName, color.postText );
   }
   if ( color.postFriendDate != 160 || color.postFriendName != 34 || color.postFriendText != 220 )
   {
      fail_msg( "named color parsing should set friend post colors; got date=%d name=%d text=%d",
                color.postFriendDate, color.postFriendName, color.postFriendText );
   }
   if ( color.anonymous != 26 || color.morePrompt != 44 ||
        color.background != COLOR_VALUE_DEFAULT )
   {
      fail_msg( "named color parsing should set anonymous/more/background colors; got anonymous=%d more=%d background=%d",
                color.anonymous, color.morePrompt, color.background );
   }
   if ( color.inputText != 231 || color.inputHighlight != 34 || color.expressText != 220 ||
        color.expressName != 91 || color.expressFriendText != 44 || color.expressFriendName != 91 )
   {
      fail_msg( "named color parsing should set input/express colors; got inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendText=%d expressFriendName=%d",
                color.inputText, color.inputHighlight, color.expressText, color.expressName,
                color.expressFriendText, color.expressFriendName );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigUsesBrightAnsiColorNames_ParsesAnsi16Values( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for bright ANSI color test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "color brightgreen brightyellow brightcyan brightred brightblack brightblack brightblack brightmagenta brightblue brightwhite brightred brightgreen brightyellow brightblue brightcyan brightblack brightblack default brightwhite brightgreen brightyellow brightmagenta brightcyan brightpurple\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write bright ANSI color configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( color.text != 10 || color.forum != 11 || color.number != 14 || color.errorTextColor != 9 )
   {
      fail_msg( "bright ANSI color parsing should set general colors to ANSI 16 values; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.postDate != 13 || color.postName != 12 || color.postText != 15 )
   {
      fail_msg( "bright ANSI color parsing should set post colors; got date=%d name=%d text=%d",
                color.postDate, color.postName, color.postText );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigUsesLongNamedColorLine_ParsesWithoutLineTooLongWarning( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for long named-color test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "color brightgreen brightyellow brightcyan brightred brightgreen brightblue brightmagenta brightmagenta brightcyan brightgreen brightmagenta brightred brightgreen brightyellow brightyellow brightwhite brightwhite 0 brightgreen brightcyan brightgreen brightgreen brightgreen brightgreen\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write long named-color configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "too long" ) != NULL )
   {
      fail_msg( "long named color lines should not trigger a line-too-long warning; log was: %s",
                aryStdPrintfLog );
   }
   if ( color.text != 10 || color.forum != 11 || color.number != 14 || color.errorTextColor != 9 )
   {
      fail_msg( "long named color parsing should still set general colors correctly; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.postDate != 13 || color.postName != 14 || color.postText != 10 )
   {
      fail_msg( "long named color parsing should still set post colors; got date=%d name=%d text=%d",
                color.postDate, color.postName, color.postText );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigUsesMixedNamedAndNumericColors_ParsesBothForms( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for mixed-color test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "color green 123 cyan red black black black magenta blue white red green yellow blue cyan black black default white green yellow magenta cyan purple\n"
           "version 2310\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write mixed color configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( color.text != 34 || color.forum != 123 || color.number != 44 )
   {
      fail_msg( "mixed named/numeric parsing should preserve both forms; got text=%d forum=%d number=%d",
                color.text, color.forum, color.number );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigFileMissing_CreatesFileAndUsesDefaults( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];
   struct stat fileStats;

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for missing-config test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isLoginShell = 0;
   isBbsRcReadOnly = 0;

   // Act
   readBbsRc();

   // Assert
   if ( stat( aryPath, &fileStats ) != 0 )
   {
      fail_msg( "readBbsRc should create configuration file when it does not exist" );
   }
   if ( setupCallCount != 1 || setupVersionArg != -1 )
   {
      fail_msg( "missing config should trigger setup(-1); got count=%d arg=%d", setupCallCount, setupVersionArg );
   }
   if ( strcmp( aryBbsHost, BBS_HOSTNAME ) != 0 || bbsPort != BBS_PORT_NUMBER )
   {
      fail_msg( "missing config should still apply default host/port" );
   }

   cleanupReadState();
   unlink( aryPath );
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( openBbsRc_WhenPathMissing_CreatesWritableConfigurationFile ),
      cmocka_unit_test( openBbsRc_WhenParentDirectoriesMissing_CreatesConfigDirectoryTree ),
      cmocka_unit_test( openBbsRc_WhenPathIsReadOnly_SetsReadOnlyAndWarns ),
      cmocka_unit_test( readBbsRc_WhenConfigIsEmpty_AppliesDefaults ),
      cmocka_unit_test( readBbsRc_WhenConfigHasEntries_ParsesValuesAndIgnoresDuplicateEnemy ),
      cmocka_unit_test( readBbsRc_WhenConfigContainsObsoleteBrowserSetting_RewritesAndWarns ),
      cmocka_unit_test( readBbsRc_WhenConfigUsesLegacyIscaIp_RewritesToCanonicalHostname ),
      cmocka_unit_test( readBbsRc_WhenConfigSetsKeepaliveZero_DisablesTcpKeepalive ),
      cmocka_unit_test( readBbsRc_WhenConfigSetsTitleBarZero_DisablesTitleBarUpdates ),
      cmocka_unit_test( readBbsRc_WhenConfigSetsScreenReaderOne_EnablesScreenReaderMode ),
      cmocka_unit_test( readBbsRc_WhenConfigSetsAutocompleteZero_DisablesAutocomplete ),
#ifndef ENABLE_KEYCHAIN
      cmocka_unit_test( readBbsRc_WhenConfigSetsKeychainOneWithoutBuildSupport_IgnoresSetting ),
#endif
#ifdef ENABLE_KEYCHAIN
      cmocka_unit_test( readBbsRc_WhenConfigSetsKeychainOne_EnablesKeychain ),
#endif
      cmocka_unit_test( readBbsRc_WhenConfigMissingScreenReaderSetting_PromptsAndRewrites ),
      cmocka_unit_test( readBbsRc_WhenAutocompleteMissingAndScreenReaderEnabled_DefaultsAutocompleteOff ),
      cmocka_unit_test( readBbsRc_WhenConfigSetsClickableUrlsZero_DisablesClickableUrls ),
      cmocka_unit_test( readBbsRc_WhenConfigSetsClickableUrlsOne_EnablesClickableUrls ),
      cmocka_unit_test( readBbsRc_WhenConfigContainsInvalidClickableUrlsDefinition_PrintsWarningAndKeepsDefault ),
      cmocka_unit_test( readBbsRc_WhenConfigSetsKeepaliveOne_EnablesTcpKeepalive ),
      cmocka_unit_test( readBbsRc_WhenConfigContainsMalformedKeepalive_PrintsWarningAndKeepsDefault ),
      cmocka_unit_test( readBbsRc_WhenConfigContainsInvalidDirective_PrintsSyntaxError ),
      cmocka_unit_test( readBbsRc_WhenConfigContainsInvalidColor_PrintsWarning ),
      cmocka_unit_test( readBbsRc_WhenConfigUsesNamedColors_ParsesExtendedPaletteValues ),
      cmocka_unit_test( readBbsRc_WhenConfigUsesBrightAnsiColorNames_ParsesAnsi16Values ),
      cmocka_unit_test( readBbsRc_WhenConfigUsesLongNamedColorLine_ParsesWithoutLineTooLongWarning ),
      cmocka_unit_test( readBbsRc_WhenConfigUsesMixedNamedAndNumericColors_ParsesBothForms ),
      cmocka_unit_test( readBbsRc_WhenConfigFileMissing_CreatesFileAndUsesDefaults ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
