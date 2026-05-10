/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config_file.h"
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
static int writeConfigCallCount;

static char aryLastPerrorHeading[64];
static char aryLastPerrorMessage[128];
static char aryStdPrintfLog[16384];

static void cleanupReadState( void )
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
   writeConfigCallCount = 0;
   aryLastPerrorHeading[0] = '\0';
   aryLastPerrorMessage[0] = '\0';
   aryStdPrintfLog[0] = '\0';
}

// config_file.c and config_parse.c dependencies that are outside this test target's scope.
void configClient( void )
{
   // Test stub: interactive config flow is not relevant in this test.
}

void defaultColors( int setall )
{
   (void)setall;
}

int colorValueFromName( const char *ptrColorName )
{
   if ( ptrColorName == NULL )
   {
      return -1;
   }
   if ( strcmp( ptrColorName, "brightblue" ) == 0 )
   {
      return 12;
   }
   if ( strcmp( ptrColorName, "brightgreen" ) == 0 )
   {
      return 10;
   }
   if ( strcmp( ptrColorName, "default" ) == 0 )
   {
      return COLOR_VALUE_DEFAULT;
   }
   if ( strcmp( ptrColorName, "yellow" ) == 0 )
   {
      return 220;
   }

   return -1;
}

noreturn void fatalExit( const char *message, const char *heading )
{
   fail_msg( "fatalExit invoked unexpectedly: %s (%s)", message, heading );
   abort();
}

FILE *findConfigFile( void )
{
   const char *ptrHomeDirectory;
   const char *ptrXdgConfigHome;

   if ( aryConfigFileName[0] != '\0' )
   {
      return openConfigFile();
   }

   ptrXdgConfigHome = getenv( "XDG_CONFIG_HOME" );
   if ( ptrXdgConfigHome != NULL && *ptrXdgConfigHome != '\0' )
   {
      snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s/bbs/config.toml",
                ptrXdgConfigHome );
      return openConfigFile();
   }

   ptrHomeDirectory = getenv( "HOME" );
   if ( ptrHomeDirectory == NULL || *ptrHomeDirectory == '\0' )
   {
      return NULL;
   }

   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s/.config/bbs/config.toml",
             ptrHomeDirectory );
   return openConfigFile();
}

void setColorFieldValue( int colorIndex, int colorValue )
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
   *aryTestColorFields[colorIndex] = colorValue;
}

bool tryFindColorFieldIndexByTomlKeyName( const char *ptrKeyName,
                                          int *ptrOutColorIndex )
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
   int itemIndex;

   if ( ptrKeyName == NULL || ptrOutColorIndex == NULL )
   {
      return false;
   }

   for ( itemIndex = 0; itemIndex < COLOR_FIELD_COUNT; itemIndex++ )
   {
      if ( aryTestColorTomlKeys[itemIndex] != NULL &&
           strcmp( ptrKeyName, aryTestColorTomlKeys[itemIndex] ) == 0 )
      {
         *ptrOutColorIndex = itemIndex;
         return true;
      }
   }

   return false;
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

void writeConfig( void )
{
   writeConfigCallCount++;
}

static void openConfigFile_WhenPathMissing_CreatesWritableConfigurationFile( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];
   struct stat fileStats;
   FILE *ptrFile;
   mode_t oldUmask;

   (void)state;

   resetTracking();
   isConfigFileReadOnly = 0;
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for openConfigFile missing-path test" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   oldUmask = umask( 0022 );

   // Act
   ptrFile = openConfigFile();
   umask( oldUmask );

   // Assert
   if ( ptrFile == NULL )
   {
      fail_msg( "openConfigFile should create and open a missing configuration file" );
   }
   if ( isConfigFileReadOnly != 0 )
   {
      fail_msg( "openConfigFile should not mark a newly created file as read-only" );
   }
   if ( perrorCallCount != 0 )
   {
      fail_msg( "openConfigFile should not call sPerror on successful create/open; got %d calls", perrorCallCount );
   }
   if ( stat( aryPath, &fileStats ) != 0 )
   {
      fail_msg( "openConfigFile should create the configuration file on disk" );
   }
   if ( ( fileStats.st_mode & 0777 ) != 0600 )
   {
      fail_msg( "openConfigFile should create the configuration file with mode 0600; got %03o",
                fileStats.st_mode & 0777 );
   }

   fclose( ptrFile );
   unlink( aryPath );
}

static void openConfigFile_WhenParentDirectoriesMissing_CreatesConfigDirectoryTree( void **state )
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
   isConfigFileReadOnly = 0;
   ptrTempDirectory = mkdtemp( aryDirectoryTemplate );
   if ( ptrTempDirectory == NULL )
   {
      fail_msg( "Arrange failed: unable to create temporary directory for openConfigFile config-tree test" );
      return;
   }
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config/bbs/config.toml",
             ptrTempDirectory );
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryConfigPath );

   // Act
   ptrFile = openConfigFile();

   // Assert
   if ( ptrFile == NULL )
   {
      fail_msg( "openConfigFile should create and open the config file when parent directories are missing" );
   }
   if ( stat( aryConfigPath, &fileStats ) != 0 )
   {
      fail_msg( "openConfigFile should create the config file on disk" );
   }
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config", ptrTempDirectory );
   if ( stat( aryConfigPath, &outerDirectoryStats ) != 0 ||
        !S_ISDIR( outerDirectoryStats.st_mode ) )
   {
      fail_msg( "openConfigFile should create the outer config directory" );
   }
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config/bbs", ptrTempDirectory );
   if ( stat( aryConfigPath, &innerDirectoryStats ) != 0 ||
        !S_ISDIR( innerDirectoryStats.st_mode ) )
   {
      fail_msg( "openConfigFile should create the app-specific config directory" );
   }
   if ( ( innerDirectoryStats.st_mode & 0777 ) != 0700 )
   {
      fail_msg( "openConfigFile should create the app-specific config directory with mode 0700; got %03o",
                innerDirectoryStats.st_mode & 0777 );
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

static void openConfigFile_WhenAppConfigDirectoryExists_RestrictsItsPermissions( void **state )
{
   // Arrange
   char aryConfigDirectory[PATH_MAX];
   char aryConfigPath[PATH_MAX];
   char aryDirectoryTemplate[] = "/tmp/iobbs_config_test_XXXXXX";
   struct stat directoryStats;
   FILE *ptrFile;
   mode_t previousUmask;
   char *ptrTempDirectory;

   (void)state;

   resetTracking();
   isConfigFileReadOnly = 0;
   ptrTempDirectory = mkdtemp( aryDirectoryTemplate );
   if ( ptrTempDirectory == NULL )
   {
      fail_msg( "Arrange failed: unable to create temporary directory for config-directory permission test" );
      return;
   }
   snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config", ptrTempDirectory );
   if ( mkdir( aryConfigDirectory, 0700 ) != 0 )
   {
      rmdir( ptrTempDirectory );
      fail_msg( "Arrange failed: unable to create outer config directory" );
      return;
   }
   snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config/bbs", ptrTempDirectory );
   if ( mkdir( aryConfigDirectory, 0755 ) != 0 )
   {
      snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config", ptrTempDirectory );
      rmdir( aryConfigDirectory );
      rmdir( ptrTempDirectory );
      fail_msg( "Arrange failed: unable to create app-specific config directory" );
      return;
   }
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/config.toml", aryConfigDirectory );
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryConfigPath );

   // Act
   previousUmask = umask( 0022 );
   ptrFile = openConfigFile();
   umask( previousUmask );

   // Assert
   if ( ptrFile == NULL )
   {
      snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config/bbs", ptrTempDirectory );
      rmdir( aryConfigDirectory );
      snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config", ptrTempDirectory );
      rmdir( aryConfigDirectory );
      rmdir( ptrTempDirectory );
      fail_msg( "openConfigFile should create and open the config file when the app-specific directory already exists" );
      return;
   }
   if ( stat( aryConfigDirectory, &directoryStats ) != 0 || !S_ISDIR( directoryStats.st_mode ) )
   {
      fclose( ptrFile );
      unlink( aryConfigPath );
      snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config/bbs", ptrTempDirectory );
      rmdir( aryConfigDirectory );
      snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config", ptrTempDirectory );
      rmdir( aryConfigDirectory );
      rmdir( ptrTempDirectory );
      fail_msg( "openConfigFile should preserve the app-specific config directory" );
      return;
   }
   if ( ( directoryStats.st_mode & 0777 ) != 0700 )
   {
      fclose( ptrFile );
      unlink( aryConfigPath );
      snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config/bbs", ptrTempDirectory );
      rmdir( aryConfigDirectory );
      snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config", ptrTempDirectory );
      rmdir( aryConfigDirectory );
      rmdir( ptrTempDirectory );
      fail_msg( "openConfigFile should restrict an existing app-specific config directory to mode 0700; got %03o",
                directoryStats.st_mode & 0777 );
      return;
   }

   // Cleanup
   fclose( ptrFile );
   unlink( aryConfigPath );
   snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config/bbs", ptrTempDirectory );
   rmdir( aryConfigDirectory );
   snprintf( aryConfigDirectory, sizeof( aryConfigDirectory ), "%s/.config", ptrTempDirectory );
   rmdir( aryConfigDirectory );
   rmdir( ptrTempDirectory );
}

static void findConfigFile_WhenLegacyHomeRootFilesExist_IgnoresThemAndUsesXdgPath( void **state )
{
   // Arrange
   char aryConfigPath[PATH_MAX];
   char aryExpectedConfigPath[PATH_MAX];
   char aryFriendsPath[PATH_MAX];
   char aryLegacyConfigPath[PATH_MAX];
   char aryOriginalHome[PATH_MAX];
   char aryOriginalXdgConfigHome[PATH_MAX];
   const char *ptrOriginalHome;
   const char *ptrOriginalXdgConfigHome;
   FILE *ptrFile;
   char *ptrTempDirectory;
   struct stat fileStats;
   char aryDirectoryTemplate[] = "/tmp/iobbs_config_home_XXXXXX";

   (void)state;

   resetTracking();
   ptrOriginalHome = getenv( "HOME" );
   ptrOriginalXdgConfigHome = getenv( "XDG_CONFIG_HOME" );
   if ( ptrOriginalHome != NULL )
   {
      snprintf( aryOriginalHome, sizeof( aryOriginalHome ), "%s", ptrOriginalHome );
   }
   else
   {
      aryOriginalHome[0] = '\0';
   }
   if ( ptrOriginalXdgConfigHome != NULL )
   {
      snprintf( aryOriginalXdgConfigHome, sizeof( aryOriginalXdgConfigHome ), "%s",
                ptrOriginalXdgConfigHome );
   }
   else
   {
      aryOriginalXdgConfigHome[0] = '\0';
   }
   ptrTempDirectory = mkdtemp( aryDirectoryTemplate );
   if ( ptrTempDirectory == NULL )
   {
      fail_msg( "Arrange failed: unable to create temporary HOME directory for legacy-ignore test" );
      return;
   }
   if ( setenv( "HOME", ptrTempDirectory, 1 ) != 0 )
   {
      rmdir( ptrTempDirectory );
      fail_msg( "Arrange failed: unable to set temporary HOME for legacy-ignore test" );
      return;
   }
   unsetenv( "XDG_CONFIG_HOME" );
   snprintf( aryLegacyConfigPath, sizeof( aryLegacyConfigPath ), "%s/.bbsrc", ptrTempDirectory );
   snprintf( aryFriendsPath, sizeof( aryFriendsPath ), "%s/.bbsfriends", ptrTempDirectory );
   if ( !tryWriteFileContents( aryLegacyConfigPath, "legacy = true\n" ) ||
        !tryWriteFileContents( aryFriendsPath, "someone\n" ) )
   {
      unlink( aryLegacyConfigPath );
      unlink( aryFriendsPath );
      if ( aryOriginalHome[0] != '\0' )
      {
         setenv( "HOME", aryOriginalHome, 1 );
      }
      else
      {
         unsetenv( "HOME" );
      }
      if ( aryOriginalXdgConfigHome[0] != '\0' )
      {
         setenv( "XDG_CONFIG_HOME", aryOriginalXdgConfigHome, 1 );
      }
      else
      {
         unsetenv( "XDG_CONFIG_HOME" );
      }
      rmdir( ptrTempDirectory );
      fail_msg( "Arrange failed: unable to write legacy config files for legacy-ignore test" );
      return;
   }
   aryConfigFileName[0] = '\0';
   isConfigFileReadOnly = 0;

   // Act
   ptrFile = findConfigFile();

   // Assert
   snprintf( aryExpectedConfigPath, sizeof( aryExpectedConfigPath ), "%s/.config/bbs/config.toml",
             ptrTempDirectory );
   if ( ptrFile == NULL )
   {
      if ( aryOriginalHome[0] != '\0' )
      {
         setenv( "HOME", aryOriginalHome, 1 );
      }
      else
      {
         unsetenv( "HOME" );
      }
      if ( aryOriginalXdgConfigHome[0] != '\0' )
      {
         setenv( "XDG_CONFIG_HOME", aryOriginalXdgConfigHome, 1 );
      }
      else
      {
         unsetenv( "XDG_CONFIG_HOME" );
      }
      unlink( aryLegacyConfigPath );
      unlink( aryFriendsPath );
      rmdir( ptrTempDirectory );
      fail_msg( "findConfigFile should open the XDG-style config path even when legacy files exist" );
      return;
   }
   if ( strcmp( aryConfigFileName, aryExpectedConfigPath ) != 0 )
   {
      fclose( ptrFile );
      unlink( aryExpectedConfigPath );
      unlink( aryLegacyConfigPath );
      unlink( aryFriendsPath );
      snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config/bbs", ptrTempDirectory );
      rmdir( aryConfigPath );
      snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config", ptrTempDirectory );
      rmdir( aryConfigPath );
      if ( aryOriginalHome[0] != '\0' )
      {
         setenv( "HOME", aryOriginalHome, 1 );
      }
      else
      {
         unsetenv( "HOME" );
      }
      if ( aryOriginalXdgConfigHome[0] != '\0' )
      {
         setenv( "XDG_CONFIG_HOME", aryOriginalXdgConfigHome, 1 );
      }
      else
      {
         unsetenv( "XDG_CONFIG_HOME" );
      }
      rmdir( ptrTempDirectory );
      fail_msg( "findConfigFile should resolve %s; got %s", aryExpectedConfigPath, aryConfigFileName );
      return;
   }
   if ( stat( aryExpectedConfigPath, &fileStats ) != 0 )
   {
      fclose( ptrFile );
      unlink( aryLegacyConfigPath );
      unlink( aryFriendsPath );
      if ( aryOriginalHome[0] != '\0' )
      {
         setenv( "HOME", aryOriginalHome, 1 );
      }
      else
      {
         unsetenv( "HOME" );
      }
      if ( aryOriginalXdgConfigHome[0] != '\0' )
      {
         setenv( "XDG_CONFIG_HOME", aryOriginalXdgConfigHome, 1 );
      }
      else
      {
         unsetenv( "XDG_CONFIG_HOME" );
      }
      rmdir( ptrTempDirectory );
      fail_msg( "findConfigFile should create %s instead of consulting legacy files", aryExpectedConfigPath );
      return;
   }

   // Cleanup
   fclose( ptrFile );
   unlink( aryExpectedConfigPath );
   unlink( aryLegacyConfigPath );
   unlink( aryFriendsPath );
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config/bbs", ptrTempDirectory );
   rmdir( aryConfigPath );
   snprintf( aryConfigPath, sizeof( aryConfigPath ), "%s/.config", ptrTempDirectory );
   rmdir( aryConfigPath );
   if ( aryOriginalHome[0] != '\0' )
   {
      setenv( "HOME", aryOriginalHome, 1 );
   }
   else
   {
      unsetenv( "HOME" );
   }
   if ( aryOriginalXdgConfigHome[0] != '\0' )
   {
      setenv( "XDG_CONFIG_HOME", aryOriginalXdgConfigHome, 1 );
   }
   else
   {
      unsetenv( "XDG_CONFIG_HOME" );
   }
   rmdir( ptrTempDirectory );
}

static void openConfigFile_WhenPathIsReadOnly_SetsReadOnlyAndWarns( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];
   FILE *ptrFile;

   (void)state;

   resetTracking();
   isConfigFileReadOnly = 0;
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for openConfigFile read-only test" );
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
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );

   // Act
   ptrFile = openConfigFile();

   // Assert
   if ( ptrFile == NULL )
   {
      chmod( aryPath, 0600 );
      unlink( aryPath );
      fail_msg( "openConfigFile should still open a read-only configuration file" );
      return;
   }
   if ( isConfigFileReadOnly == 0 )
   {
      fclose( ptrFile );
      chmod( aryPath, 0600 );
      unlink( aryPath );
      fail_msg( "openConfigFile should mark a read-only file as read-only" );
      return;
   }
   if ( perrorCallCount == 0 ||
        strcmp( aryLastPerrorMessage, "Configuration is read-only" ) != 0 )
   {
      fclose( ptrFile );
      chmod( aryPath, 0600 );
      unlink( aryPath );
      fail_msg( "openConfigFile should warn when falling back to read-only access; got %d warnings and last message '%s'",
                perrorCallCount, aryLastPerrorMessage );
      return;
   }

   fclose( ptrFile );
   chmod( aryPath, 0600 );
   unlink( aryPath );
}

static void readConfig_WhenConfigContainsCoreToml_ParsesValues( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for core TOML test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[metadata]\n"
           "version = 42\n"
           "\n"
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
           "use_keychain = true\n"
           "\n"
           "[away]\n"
           "messages = [\"Gone to lunch.\", \"Back by 2pm.\"]\n"
           "\n"
           "[contacts]\n"
           "enemies = [\"Mallory\", \"Eve\"]\n"
           "friends = [{ name = \"Bob\", info = \"A buddy\" }, { name = \"Carol\" }]\n"
           "\n"
           "[colors]\n"
           "text = \"brightgreen\"\n"
           "background = \"default\"\n"
           "post_name = 201\n"
           "incoming_ansi_white = \"yellow\"\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write core TOML configuration content" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( strcmp( aryAutoName, "Alice" ) != 0 )
   {
      fail_msg( "expected auto-login name 'Alice', got '%s'", aryAutoName );
   }
   if ( version != INT_VERSION )
   {
      fail_msg( "expected setup flow to restore current config version %d, got %d",
                INT_VERSION, version );
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
   if ( strcmp( aryAwayMessageLines[0], "Gone to lunch." ) != 0 ||
        strcmp( aryAwayMessageLines[1], "Back by 2pm." ) != 0 ||
        aryAwayMessageLines[2][0] != '\0' )
   {
      fail_msg( "expected away-message lines ['Gone to lunch.', 'Back by 2pm.']; got ['%s', '%s', '%s']",
                aryAwayMessageLines[0], aryAwayMessageLines[1], aryAwayMessageLines[2] );
   }
   if ( enemyList == NULL || enemyList->nitems != 2 )
   {
      fail_msg( "expected two parsed enemy entries" );
   }
   if ( friendList == NULL || friendList->nitems != 2 )
   {
      fail_msg( "expected two parsed friend entries" );
   }
   if ( strcmp( (const char *)enemyList->items[0], "Eve" ) != 0 ||
        strcmp( (const char *)enemyList->items[1], "Mallory" ) != 0 )
   {
      fail_msg( "expected sorted enemy list ['Eve', 'Mallory']" );
   }
   {
      const friend *ptrFirstFriend;
      const friend *ptrSecondFriend;

      ptrFirstFriend = friendList->items[0];
      ptrSecondFriend = friendList->items[1];
      if ( strcmp( ptrFirstFriend->name, "Bob" ) != 0 ||
           strcmp( ptrFirstFriend->info, "A buddy" ) != 0 ||
           strcmp( ptrSecondFriend->name, "Carol" ) != 0 ||
           strcmp( ptrSecondFriend->info, "(None)" ) != 0 )
      {
         fail_msg( "unexpected parsed friend entries '%s/%s' and '%s/%s'",
                   ptrFirstFriend->name, ptrFirstFriend->info,
                   ptrSecondFriend->name, ptrSecondFriend->info );
      }
   }
   if ( color.text != 10 || color.background != COLOR_VALUE_DEFAULT ||
        color.postName != 201 || color.ansiWhiteTextColor != 220 )
   {
      fail_msg( "expected parsed colors text=10 background=%d postName=201 incomingWhite=220; got text=%d background=%d postName=%d incomingWhite=%d",
                COLOR_VALUE_DEFAULT, color.text, color.background,
                color.postName, color.ansiWhiteTextColor );
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
   if ( setupCallCount != 1 || setupVersionArg != 42 )
   {
      fail_msg( "older config version should trigger setup(42); got count=%d arg=%d",
                setupCallCount, setupVersionArg );
   }

   cleanupReadState();
   unlink( aryPath );
}

/// @brief Verify that nonempty TOML without metadata is rewritten with the current version.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void readConfig_WhenVersionMissing_RewritesConfigWithCurrentVersion( void **state )
{
   char aryPath[PATH_MAX];

   // Arrange
   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for missing-version test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[connection]\n"
           "host = \"bbs.example.net\"\n"
           "port = 2323\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write missing-version configuration content" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( setupCallCount != 0 )
   {
      fail_msg( "missing metadata version should not trigger setup; got count=%d arg=%d",
                setupCallCount, setupVersionArg );
      return;
   }
   if ( writeConfigCallCount != 1 )
   {
      fail_msg( "missing metadata version should rewrite config.toml once; got %d",
                writeConfigCallCount );
      return;
   }
   if ( version != INT_VERSION )
   {
      fail_msg( "missing metadata version should default to current version %d, got %d",
                INT_VERSION, version );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readConfig_WhenColorsContainInvalidValue_PrintsWarningAndKeepsDefault( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   color.text = 2;
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for invalid-color test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[colors]\n"
           "text = \"banana\"\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write invalid-color configuration content" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( strstr( aryStdPrintfLog, "Invalid color value for 'text' ignored." ) == NULL )
   {
      fail_msg( "invalid color should emit warning; log was: %s", aryStdPrintfLog );
   }
   if ( color.text != 2 )
   {
      fail_msg( "invalid color should keep the prior/default text color; got %d", color.text );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readConfig_WhenAwayMessagesExceedFive_IgnoresExtraEntries( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for away-message overflow test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[away]\n"
           "messages = [\"one\", \"two\", \"three\", \"four\", \"five\", \"six\"]\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write away-message overflow configuration content" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( strcmp( aryAwayMessageLines[0], "one" ) != 0 ||
        strcmp( aryAwayMessageLines[4], "five" ) != 0 ||
        strstr( aryStdPrintfLog,
                "Extra away-message lines ignored after the first five entries." ) == NULL )
   {
      fail_msg( "away-message overflow should keep the first five lines and warn; log was: %s",
                aryStdPrintfLog );
   }

   cleanupReadState();
   unlink( aryPath );
}

/// @brief Verify that malformed away-message arrays leave defaults unchanged.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void readConfig_WhenAwayMessagesArrayIsMalformed_KeepsDefaultMessage( void **state )
{
   char aryPath[PATH_MAX];

   // Arrange
   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for malformed-away test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[away]\n"
           "messages = [\"changed\", \"broken\"\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write malformed away-message configuration content" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( strcmp( aryAwayMessageLines[0], "I'm away from my keyboard right now." ) != 0 )
   {
      fail_msg( "malformed away-message array should leave the default message untouched; got '%s'",
                aryAwayMessageLines[0] );
   }
   if ( strstr( aryStdPrintfLog, "Invalid array value for 'messages' ignored." ) == NULL )
   {
      fail_msg( "malformed away-message array should emit a warning; log was: %s", aryStdPrintfLog );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readConfig_WhenContactsContainDuplicates_IgnoresLaterDuplicates( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for duplicate-contacts test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[contacts]\n"
           "enemies = [\"Mallory\", \"Mallory\"]\n"
           "friends = [{ name = \"Bob\", info = \"First\" }, { name = \"Bob\", info = \"Second\" }]\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write duplicate-contacts configuration content" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( enemyList == NULL || enemyList->nitems != 1 ||
        friendList == NULL || friendList->nitems != 1 )
   {
      fail_msg( "duplicate contacts should be ignored after the first entry" );
   }
   if ( strstr( aryStdPrintfLog, "Duplicate enemy name ignored." ) == NULL ||
        strstr( aryStdPrintfLog, "Duplicate friend name ignored." ) == NULL )
   {
      fail_msg( "duplicate contacts should emit warnings; log was: %s", aryStdPrintfLog );
   }

   cleanupReadState();
   unlink( aryPath );
}

/// @brief Verify that malformed contact arrays leave parsed lists unchanged.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void readConfig_WhenContactArraysAreMalformed_KeepListsEmpty( void **state )
{
   char aryPath[PATH_MAX];

   // Arrange
   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for malformed-contact test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[contacts]\n"
           "enemies = [\"Mallory\", \"Eve\"\n"
           "friends = [{ name = \"Bob\", info = \"First\" }\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write malformed contact configuration content" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( enemyList == NULL || enemyList->nitems != 0 )
   {
      fail_msg( "malformed enemy array should leave enemy list empty" );
   }
   if ( friendList == NULL || friendList->nitems != 0 )
   {
      fail_msg( "malformed friend array should leave friend list empty" );
   }
   if ( strstr( aryStdPrintfLog, "Invalid array value for 'enemies' ignored." ) == NULL ||
        strstr( aryStdPrintfLog, "Invalid array value for 'friends' ignored." ) == NULL )
   {
      fail_msg( "malformed contact arrays should emit warnings; log was: %s", aryStdPrintfLog );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readConfig_WhenConfigContainsInvalidBoolean_PrintsWarningAndKeepsDefault( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
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
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

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

/// @brief Verify that inline TOML comments are ignored after valid values.
///
/// @param state CMocka test state.
///
/// @return This test does not return a value.
static void readConfig_WhenValuesHaveInlineComments_ParsesThemNormally( void **state )
{
   char aryPath[PATH_MAX];

   // Arrange
   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for inline-comment test" );
      return;
   }
   if ( !tryWriteFileContents(
           aryPath,
           "[behavior]\n"
           "tcp_keepalive = true # keep sockets alive\n"
           "[contacts]\n"
           "enemies = [\"Mallory\"] # single entry\n" ) )
   {
      unlink( aryPath );
      fail_msg( "Arrange failed: unable to write inline-comment configuration content" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( !flagsConfiguration.shouldUseTcpKeepalive )
   {
      fail_msg( "inline-comment boolean should still parse as true" );
   }
   if ( enemyList == NULL || enemyList->nitems != 1 ||
        strcmp( (const char *)enemyList->items[0], "Mallory" ) != 0 )
   {
      fail_msg( "inline-comment array should still parse the enemy entry" );
   }
   if ( strstr( aryStdPrintfLog, "Invalid" ) != NULL )
   {
      fail_msg( "inline-comment values should not emit invalid-value warnings; log was: %s", aryStdPrintfLog );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readConfig_WhenConfigContainsInvalidLocalCommandKey_PrintsWarningAndKeepsDefault( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
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
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

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

static void readConfig_WhenConfigContainsInvalidPort_PrintsWarningAndKeepsDefault( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
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
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

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

static void readConfig_WhenConfigContainsKeyOutsideSection_PrintsWarning( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
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
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

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

static void readConfig_WhenConfigContainsUnknownSection_PrintsWarningAndContinues( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
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
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

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

static void readConfig_WhenConfigMissingScreenReaderSetting_PromptsAndRewrites( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
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
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( promptForScreenReaderModeCallCount != 1 )
   {
      fail_msg( "missing screen reader setting should trigger one prompt helper call; got %d",
                promptForScreenReaderModeCallCount );
   }
   if ( writeConfigCallCount != 1 )
   {
      fail_msg( "missing screen reader setting should rewrite config.toml once; got %d",
                writeConfigCallCount );
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

static void readConfig_WhenAutocompleteMissingAndScreenReaderEnabled_DefaultsAutocompleteOff( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
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
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

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
   if ( writeConfigCallCount != 1 )
   {
      fail_msg( "missing autocomplete setting should rewrite config.toml once; got %d",
                writeConfigCallCount );
   }

   cleanupReadState();
   unlink( aryPath );
}

static void readConfig_WhenConfigFileMissing_CreatesFileAndUsesDefaults( void **state )
{
   // Arrange
   char aryPath[PATH_MAX];
   struct stat fileStats;

   (void)state;

   cleanupReadState();
   resetTracking();
   if ( !tryCreateTempPath( aryPath, sizeof( aryPath ), "/tmp/iobbs_config_test_XXXXXX" ) )
   {
      fail_msg( "Arrange failed: unable to create temporary path for missing-config test" );
      return;
   }
   snprintf( aryConfigFileName, sizeof( aryConfigFileName ), "%s", aryPath );
   snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
   isConfigFileReadOnly = 0;
   isLoginShell = 0;

   // Act
   readConfig();

   // Assert
   if ( stat( aryPath, &fileStats ) != 0 )
   {
      fail_msg( "readConfig should create the configuration file when it does not exist" );
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
   if ( strcmp( aryAwayMessageLines[0], "I'm away from my keyboard right now." ) != 0 )
   {
      fail_msg( "missing config should restore the default away message; got '%s'",
                aryAwayMessageLines[0] );
   }

   cleanupReadState();
   unlink( aryPath );
}

int main( void )
{
   const struct CMUnitTest aryTests[] =
      {
         cmocka_unit_test( openConfigFile_WhenPathMissing_CreatesWritableConfigurationFile ),
         cmocka_unit_test( openConfigFile_WhenParentDirectoriesMissing_CreatesConfigDirectoryTree ),
         cmocka_unit_test( openConfigFile_WhenAppConfigDirectoryExists_RestrictsItsPermissions ),
         cmocka_unit_test( openConfigFile_WhenPathIsReadOnly_SetsReadOnlyAndWarns ),
         cmocka_unit_test( findConfigFile_WhenLegacyHomeRootFilesExist_IgnoresThemAndUsesXdgPath ),
         cmocka_unit_test( readConfig_WhenConfigContainsCoreToml_ParsesValues ),
         cmocka_unit_test( readConfig_WhenVersionMissing_RewritesConfigWithCurrentVersion ),
         cmocka_unit_test( readConfig_WhenAwayMessagesExceedFive_IgnoresExtraEntries ),
         cmocka_unit_test( readConfig_WhenAwayMessagesArrayIsMalformed_KeepsDefaultMessage ),
         cmocka_unit_test( readConfig_WhenColorsContainInvalidValue_PrintsWarningAndKeepsDefault ),
         cmocka_unit_test( readConfig_WhenContactArraysAreMalformed_KeepListsEmpty ),
         cmocka_unit_test( readConfig_WhenContactsContainDuplicates_IgnoresLaterDuplicates ),
         cmocka_unit_test( readConfig_WhenConfigContainsInvalidBoolean_PrintsWarningAndKeepsDefault ),
         cmocka_unit_test( readConfig_WhenValuesHaveInlineComments_ParsesThemNormally ),
         cmocka_unit_test( readConfig_WhenConfigContainsInvalidLocalCommandKey_PrintsWarningAndKeepsDefault ),
         cmocka_unit_test( readConfig_WhenConfigContainsInvalidPort_PrintsWarningAndKeepsDefault ),
         cmocka_unit_test( readConfig_WhenConfigContainsKeyOutsideSection_PrintsWarning ),
         cmocka_unit_test( readConfig_WhenConfigContainsUnknownSection_PrintsWarningAndContinues ),
         cmocka_unit_test( readConfig_WhenConfigMissingScreenReaderSetting_PromptsAndRewrites ),
         cmocka_unit_test( readConfig_WhenAutocompleteMissingAndScreenReaderEnabled_DefaultsAutocompleteOff ),
         cmocka_unit_test( readConfig_WhenConfigFileMissing_CreatesFileAndUsesDefaults ),
      };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
