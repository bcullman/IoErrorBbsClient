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

static int defaultNameAutocompleteIfUnsetCallCount;
static int perrorCallCount;
static int promptForScreenReaderModeCallCount;
static int setupCallCount;
static int setupVersionArg;
static int writeBbsRcCallCount;

static char aryLastPerrorHeading[64];
static char aryLastPerrorMessage[128];
static char aryStdPrintfLog[16384];

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

static void resetTracking( void )
{
   defaultNameAutocompleteIfUnsetCallCount = 0;
   perrorCallCount = 0;
   promptForScreenReaderModeCallCount = 0;
   setupCallCount = 0;
   setupVersionArg = 0;
   writeBbsRcCallCount = 0;
   aryLastPerrorHeading[0] = '\0';
   aryLastPerrorMessage[0] = '\0';
   aryStdPrintfLog[0] = '\0';
}

// bbsrc.c and bbsrc_parse.c dependencies that are outside this test target's scope.
void configBbsRc( void )
{
   // Test stub: interactive config flow is not relevant in this test.
}

void defaultColors( int setall )
{
   (void)setall;
}

noreturn void fatalExit( const char *message, const char *heading )
{
   fail_msg( "fatalExit invoked unexpectedly: %s (%s)", message, heading );
   abort();
}

FILE *findBbsFriends( void )
{
   return NULL;
}

FILE *findBbsRc( void )
{
   return openBbsRc();
}

int fSortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   const friend *const *ptrLeftFriend;
   const friend *const *ptrRightFriend;

   ptrLeftFriend = ptrLeft;
   ptrRightFriend = ptrRight;
   return strcmp( ( *ptrLeftFriend )->name, ( *ptrRightFriend )->name );
}

char *findChar( const char *ptrString, int targetChar )
{
   const char *ptrSearch;

   for ( ptrSearch = ptrString; *ptrSearch != '\0' && *ptrSearch != targetChar; ptrSearch++ )
   {
      ;
   }
   if ( *ptrSearch == targetChar )
   {
      return (char *)ptrSearch;
   }

   return NULL;
}

void promptForScreenReaderModeIfUnset( void )
{
   promptForScreenReaderModeCallCount++;
   flagsConfiguration.hasScreenReaderModeSetting = 1;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
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

void resetTerm( void )
{
   // Test stub: terminal reset behavior is not relevant in this test.
}

void defaultNameAutocompleteIfUnset( void )
{
   defaultNameAutocompleteIfUnsetCallCount++;
   flagsConfiguration.hasNameAutocompleteSetting = 1;
   flagsConfiguration.shouldEnableNameAutocomplete =
      (unsigned int)!flagsConfiguration.isScreenReaderModeEnabled;
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

void sPerror( const char *message, const char *heading )
{
   perrorCallCount++;
   snprintf( aryLastPerrorHeading, sizeof( aryLastPerrorHeading ), "%s", heading );
   snprintf( aryLastPerrorMessage, sizeof( aryLastPerrorMessage ), "%s", message );
}

int sortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   const char *const *ptrLeftString;
   const char *const *ptrRightString;

   ptrLeftString = ptrLeft;
   ptrRightString = ptrRight;
   return strcmp( *ptrLeftString, *ptrRightString );
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

int strCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   return strcmp( (const char *)ptrLeft, (const char *)ptrRight );
}

void writeBbsRc( void )
{
   writeBbsRcCallCount++;
}

static void openBbsRc_WhenPathMissing_CreatesWritableConfigurationFile( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];
   struct stat fileStats;
   FILE *ptrFile;

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
   struct stat innerDirectoryStats;
   struct stat outerDirectoryStats;
   struct stat fileStats;
   FILE *ptrFile;
   char *ptrTempDirectory;

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
   if ( stat( aryConfigPath, &outerDirectoryStats ) != 0 ||
        !S_ISDIR( outerDirectoryStats.st_mode ) )
   {
      fail_msg( "openBbsRc should create the outer config directory" );
   }
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config/bbs", ptrTempDirectory );
   if ( stat( aryConfigPath, &innerDirectoryStats ) != 0 ||
        !S_ISDIR( innerDirectoryStats.st_mode ) )
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
   if ( !tryWriteFileContents( aryPath, "read-only\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write initial read-only test content" );
      return;
   }
   if ( chmod( aryPath, 0400 ) != 0 )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to chmod read-only test file" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );

   // Act
   ptrFile = openBbsRc();

   // Assert
   if ( ptrFile == NULL )
   {
      chmod( aryPath, 0600 );
      unlink( aryPath );
      fail_msg( "openBbsRc should still open a read-only configuration file" );
      return;
   }
   if ( isBbsRcReadOnly == 0 )
   {
      fclose( ptrFile );
      chmod( aryPath, 0600 );
      unlink( aryPath );
      fail_msg( "openBbsRc should mark a read-only file as read-only" );
      return;
   }
   if ( perrorCallCount == 0 ||
        strcmp( aryLastPerrorMessage, "Configuration is read-only" ) != 0 )
   {
      fclose( ptrFile );
      chmod( aryPath, 0600 );
      unlink( aryPath );
      fail_msg( "openBbsRc should warn when falling back to read-only access; got %d warnings and last message '%s'",
                perrorCallCount, aryLastPerrorMessage );
      return;
   }

   fclose( ptrFile );
   chmod( aryPath, 0600 );
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsCoreToml_ParsesValues( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for core TOML test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[connection]\n"
           "auto_login_name = \"Alice\"\n"
           "editor = \"vim\"\n"
           "host = \"bbs.example.net\"\n"
           "port = 2323\n"
           "\n"
           "[local_command_keys]\n"
           "away = \"b\"\n"
           "browser = \"space\"\n"
           "capture = \"tab\"\n"
           "command = \"esc\"\n"
           "quit = \"ctrl-k\"\n"
           "shell = \"!\"\n"
           "suspend = \"ctrl-z\"\n"
           "\n"
           "[defaults]\n"
           "show_full_profile_by_default = true\n"
           "show_long_who_by_default = true\n"
           "\n"
           "[behavior]\n"
           "auto_answer_ansi = true\n"
           "autocomplete_recipients = false\n"
           "clickable_url_summaries = false\n"
           "screen_reader_mode = true\n"
           "suppress_enemy_express = true\n"
           "suppress_enemy_posts = true\n"
           "tcp_keepalive = false\n"
           "update_title_bar = false\n"
           "use_keychain = true\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write core TOML configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isBbsRcReadOnly = 0;
   isLoginShell = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strcmp( aryAutoName, "Alice" ) != 0 )
   {
      fail_msg( "expected auto-login name 'Alice', got '%s'", aryAutoName );
   }
   if ( strcmp( aryBbsHost, "bbs.example.net" ) != 0 || bbsPort != 2323 )
   {
      fail_msg( "expected parsed host/port bbs.example.net:2323, got %s:%u",
                aryBbsHost, bbsPort );
   }
   if ( strcmp( aryEditor, "vim" ) != 0 )
   {
      fail_msg( "expected editor 'vim', got '%s'", aryEditor );
   }
   if ( awayKey != 'b' || browserKey != ' ' || captureKey != '\t' ||
        commandKey != ESC || quitKey != 11 || shellKey != '!' || suspKey != CTRL_Z )
   {
      fail_msg( "unexpected parsed local command keys %d/%d/%d/%d/%d/%d/%d",
                awayKey, browserKey, captureKey, commandKey, quitKey, shellKey, suspKey );
   }
   if ( aryKeyMap['p'] != 'P' || aryKeyMap['P'] != 'p' ||
        aryKeyMap['w'] != 'W' || aryKeyMap['W'] != 'w' )
   {
      fail_msg( "semantic defaults should swap lowercase and uppercase W/P mappings" );
   }
   if ( !flagsConfiguration.shouldAutoAnswerAnsiPrompt ||
        flagsConfiguration.shouldEnableClickableUrls ||
        flagsConfiguration.shouldEnableNameAutocomplete ||
        !flagsConfiguration.isScreenReaderModeEnabled ||
        !flagsConfiguration.shouldSquelchExpress ||
        !flagsConfiguration.shouldSquelchPost ||
        flagsConfiguration.shouldUseTcpKeepalive ||
        flagsConfiguration.shouldEnableTitleBar )
   {
      fail_msg( "expected parsed behavior settings were not applied correctly" );
   }
#ifdef ENABLE_KEYCHAIN
   if ( !flagsConfiguration.shouldUseKeychain )
   {
      fail_msg( "use_keychain = true should enable keychain support when compiled in" );
   }
#else
   if ( flagsConfiguration.shouldUseKeychain )
   {
      fail_msg( "use_keychain should be ignored when keychain support is not compiled in" );
   }
#endif

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsInvalidBoolean_PrintsWarningAndKeepsDefault( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for invalid-boolean test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[behavior]\n"
           "clickable_url_summaries = maybe\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write invalid-boolean configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isBbsRcReadOnly = 0;
   isLoginShell = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog,
                "Invalid boolean value for 'clickable_url_summaries' ignored." ) == NULL )
   {
      fail_msg( "invalid boolean should emit warning; log was: %s", aryStdPrintfLog );
   }
   if ( !flagsConfiguration.shouldEnableClickableUrls )
   {
      fail_msg( "invalid boolean should keep the default clickable-URL setting enabled" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsInvalidLocalCommandKey_PrintsWarningAndKeepsDefault( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for invalid-key test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[local_command_keys]\n"
           "command = \"space\"\n"
           "quit = \"banana\"\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write invalid-key configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isBbsRcReadOnly = 0;
   isLoginShell = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "Illegal value for 'command', using default of 'esc'." ) == NULL )
   {
      fail_msg( "illegal command key should emit warning; log was: %s", aryStdPrintfLog );
   }
   if ( strstr( aryStdPrintfLog, "Invalid key value for 'quit' ignored." ) == NULL )
   {
      fail_msg( "invalid key string should emit warning; log was: %s", aryStdPrintfLog );
   }
   if ( commandKey != ESC || quitKey != CTRL_D )
   {
      fail_msg( "invalid local-command keys should fall back to Esc and Ctrl-D" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsInvalidPort_PrintsWarningAndKeepsDefault( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for invalid-port test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[connection]\n"
           "port = 99999\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write invalid-port configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isBbsRcReadOnly = 0;
   isLoginShell = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "Invalid integer value for 'port' ignored." ) == NULL )
   {
      fail_msg( "invalid port should emit warning; log was: %s", aryStdPrintfLog );
   }
   if ( bbsPort != BBS_PORT_NUMBER )
   {
      fail_msg( "invalid port should keep default port %d, got %u",
                BBS_PORT_NUMBER, bbsPort );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsKeyOutsideSection_PrintsWarning( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for key-outside-section test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "host = \"bbs.example.net\"\n"
           "[connection]\n"
           "port = 2323\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write key-outside-section configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isBbsRcReadOnly = 0;
   isLoginShell = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "TOML key 'host' must appear inside a section." ) == NULL )
   {
      fail_msg( "key outside a section should emit warning; log was: %s", aryStdPrintfLog );
   }
   if ( bbsPort != 2323 )
   {
      fail_msg( "parser should continue after key-outside-section warning and parse the remaining section" );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readBbsRc_WhenConfigContainsUnknownSection_PrintsWarningAndContinues( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbsrc_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for unknown-section test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[mystery]\n"
           "value = 42\n"
           "[connection]\n"
           "port = 2323\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write unknown-section configuration content" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isBbsRcReadOnly = 0;
   isLoginShell = 0;

   // Act
   readBbsRc();

   // Assert
   if ( strstr( aryStdPrintfLog, "Unknown TOML section 'mystery' ignored." ) == NULL )
   {
      fail_msg( "unknown section should emit warning; log was: %s", aryStdPrintfLog );
   }
   if ( bbsPort != 2323 )
   {
      fail_msg( "parser should continue after unknown section and parse the remaining section" );
   }

   cleanupReadState();
   unlink( aryPath );
}

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
           "[behavior]\n"
           "tcp_keepalive = true\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for missing screenreader test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isBbsRcReadOnly = 0;
   isLoginShell = 0;

   // Act
   readBbsRc();

   // Assert
   if ( promptForScreenReaderModeCallCount != 1 )
   {
      fail_msg( "missing screen reader setting should trigger one prompt helper call; got %d",
                promptForScreenReaderModeCallCount );
   }
   if ( writeBbsRcCallCount != 1 )
   {
      fail_msg( "missing screen reader setting should rewrite config.toml once; got %d",
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
           "[behavior]\n"
           "screen_reader_mode = true\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write configuration content for missing autocomplete test" );
      return;
   }
   snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isBbsRcReadOnly = 0;
   isLoginShell = 0;

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
      fail_msg( "missing autocomplete setting should rewrite config.toml once; got %d",
                writeBbsRcCallCount );
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
   isBbsRcReadOnly = 0;
   isLoginShell = 0;

   // Act
   readBbsRc();

   // Assert
   if ( stat( aryPath, &fileStats ) != 0 )
   {
      fail_msg( "readBbsRc should create the configuration file when it does not exist" );
   }
   if ( setupCallCount != 1 || setupVersionArg != -1 )
   {
      fail_msg( "missing config should trigger setup(-1); got count=%d arg=%d",
                setupCallCount, setupVersionArg );
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
   const struct CMUnitTest aryTests[] =
      {
         cmocka_unit_test( openBbsRc_WhenPathMissing_CreatesWritableConfigurationFile ),
         cmocka_unit_test( openBbsRc_WhenParentDirectoriesMissing_CreatesConfigDirectoryTree ),
         cmocka_unit_test( openBbsRc_WhenPathIsReadOnly_SetsReadOnlyAndWarns ),
         cmocka_unit_test( readBbsRc_WhenConfigContainsCoreToml_ParsesValues ),
         cmocka_unit_test( readBbsRc_WhenConfigContainsInvalidBoolean_PrintsWarningAndKeepsDefault ),
         cmocka_unit_test( readBbsRc_WhenConfigContainsInvalidLocalCommandKey_PrintsWarningAndKeepsDefault ),
         cmocka_unit_test( readBbsRc_WhenConfigContainsInvalidPort_PrintsWarningAndKeepsDefault ),
         cmocka_unit_test( readBbsRc_WhenConfigContainsKeyOutsideSection_PrintsWarning ),
         cmocka_unit_test( readBbsRc_WhenConfigContainsUnknownSection_PrintsWarningAndContinues ),
         cmocka_unit_test( readBbsRc_WhenConfigMissingScreenReaderSetting_PromptsAndRewrites ),
         cmocka_unit_test( readBbsRc_WhenAutocompleteMissingAndScreenReaderEnabled_DefaultsAutocompleteOff ),
         cmocka_unit_test( readBbsRc_WhenConfigFileMissing_CreatesFileAndUsesDefaults ),
      };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
