/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file writes the client configuration back to .bbsrc.
 */
#include "bbsrc.h"
#include "client_globals.h"
#include "color.h"
#include "config_globals.h"
#include "config_menu.h"
#include "defs.h"
#include "filter_globals.h"
#include "utility.h"
static void writeAwayMessages( void );
static void writeColorSettings( void );
static void writeConnectionSettings( void );
static void writeFriendAndEnemyLists( void );
static void writeKeyMapOverrides( void );
static void writeMacros( void );
static void writeMiscSettings( void );
static void writeOptionSettings( void );

/// @brief Write the configured away-message lines to `.bbsrc`.
///
/// @return This function does not return a value.
static void writeAwayMessages( void )
{
   if ( **aryAwayMessageLines )
   {
      for ( int itemIndex = 0; itemIndex < 5 && *aryAwayMessageLines[itemIndex];
            itemIndex++ )
      {
         fprintf( ptrBbsRc, "a%d %s\n", itemIndex + 1, aryAwayMessageLines[itemIndex] );
      }
   }
}

/// @brief Rewrite the current in-memory configuration back to `.bbsrc`.
///
/// @return This function does not return a value.
void writeBbsRc( void )
{
   deleteFile( aryBbsFriendsName );
   rewind( ptrBbsRc );

   writeConnectionSettings();
   writeOptionSettings();
   writeColorSettings();
   writeAwayMessages();
   writeMiscSettings();
   writeFriendAndEnemyLists();
   writeMacros();
   writeKeyMapOverrides();

   fflush( ptrBbsRc );
   truncateBbsRc( ftell( ptrBbsRc ) );
}

/// @brief Write the serialized color configuration line.
///
/// @return This function does not return a value.
static void writeColorSettings( void )
{
   int itemIndex;

   fprintf( ptrBbsRc, "color" );
   for ( itemIndex = 0; itemIndex < COLOR_FIELD_COUNT; itemIndex++ )
   {
      const char *ptrColorName;
      int colorValue;

      colorValue = colorFieldValue( itemIndex );
      ptrColorName = colorNameFromValue( colorValue );
      if ( ptrColorName != NULL )
      {
         fprintf( ptrBbsRc, " %s", ptrColorName );
      }
      else
      {
         fprintf( ptrBbsRc, " %d", colorValue );
      }
   }
   fprintf( ptrBbsRc, "\n" );
}

/// @brief Write the configured site, port, and editor settings.
///
/// @return This function does not return a value.
static void writeConnectionSettings( void )
{
   fprintf( ptrBbsRc, "aryEditor %s\n", aryEditor );
   // Change:  site line will always be written
   fprintf( ptrBbsRc, "site %s %d\n", aryBbsHost, bbsPort );
}

/// @brief Write the saved friend and enemy lists.
///
/// @return This function does not return a value.
static void writeFriendAndEnemyLists( void )
{
   int itemIndex;

   for ( itemIndex = 0; itemIndex < (int)friendList->nitems; itemIndex++ )
   {
      const friend *ptrFriend = (friend *)friendList->items[itemIndex];
      fprintf( ptrBbsRc, "friend %-20s   %s\n", ptrFriend->name, ptrFriend->info );
   }
   for ( itemIndex = 0; itemIndex < (int)enemyList->nitems; itemIndex++ )
   {
      fprintf( ptrBbsRc, "enemy %s\n", (char *)enemyList->items[itemIndex] );
   }
}

/// @brief Write any non-default key map overrides.
///
/// @return This function does not return a value.
static void writeKeyMapOverrides( void )
{
   int itemIndex;

   for ( itemIndex = 33; itemIndex < 128; itemIndex++ )
   {
      if ( aryKeyMap[itemIndex] != itemIndex )
      {
         fprintf( ptrBbsRc, "aryKeyMap %c %c\n", itemIndex, aryKeyMap[itemIndex] );
      }
   }
}

/// @brief Write the configured keyboard macros.
///
/// @return This function does not return a value.
static void writeMacros( void )
{
   int itemIndex, innerIndex;

   for ( itemIndex = 0; itemIndex < 128; itemIndex++ )
   {
      if ( *aryMacro[itemIndex] )
      {
         fprintf( ptrBbsRc, "aryMacro %s ", strCtrl( itemIndex ) );
         for ( innerIndex = 0; aryMacro[itemIndex][innerIndex]; innerIndex++ )
         {
            fprintf( ptrBbsRc, "%s", strCtrl( aryMacro[itemIndex][innerIndex] ) );
         }
         fprintf( ptrBbsRc, "\n" );
      }
   }
}

/// @brief Write miscellaneous version and legacy behavior flags.
///
/// @return This function does not return a value.
static void writeMiscSettings( void )
{
   fprintf( ptrBbsRc, "version %d\n", version );
   if ( flagsConfiguration.shouldUseBold )
   {
      fprintf( ptrBbsRc, "bold\n" );
   }
   if ( !isXland )
   {
      fprintf( ptrBbsRc, "xland\n" );
   }
}

/// @brief Write the general option and hotkey settings.
///
/// @return This function does not return a value.
static void writeOptionSettings( void )
{
   fprintf( ptrBbsRc, "commandkey %s\n", strCtrl( commandKey ) );
   fprintf( ptrBbsRc, "quit %s\n", strCtrl( quitKey ) );
   fprintf( ptrBbsRc, "susp %s\n", strCtrl( suspKey ) );
   fprintf( ptrBbsRc, "shellkey %s\n", strCtrl( shellKey ) );
   fprintf( ptrBbsRc, "capture %s\n", strCtrl( captureKey ) );
   fprintf( ptrBbsRc, "awaykey %s\n", strCtrl( awayKey ) );
   fprintf( ptrBbsRc, "squelch %d\n",
            ( flagsConfiguration.shouldSquelchPost ? 2 : 0 ) +
               ( flagsConfiguration.shouldSquelchExpress ? 1 : 0 ) );
   fprintf( ptrBbsRc, "keepalive %d\n",
            flagsConfiguration.shouldUseTcpKeepalive ? 1 : 0 );
   fprintf( ptrBbsRc, "clickableurls %d\n",
            flagsConfiguration.shouldEnableClickableUrls ? 1 : 0 );
   fprintf( ptrBbsRc, "titlebar %d\n", flagsConfiguration.shouldEnableTitleBar ? 1 : 0 );
   fprintf( ptrBbsRc, "screenreader %d\n",
            flagsConfiguration.isScreenReaderModeEnabled ? 1 : 0 );
   fprintf( ptrBbsRc, "autocomplete %d\n",
            flagsConfiguration.shouldEnableNameAutocomplete ? 1 : 0 );
#ifdef ENABLE_KEYCHAIN
   fprintf( ptrBbsRc, "keychain %d\n",
            flagsConfiguration.shouldUseKeychain ? 1 : 0 );
#endif
   if ( *aryAutoName )
   {
      fprintf( ptrBbsRc, "aryAutoName %s\n", aryAutoName );
   }
   if ( flagsConfiguration.shouldAutoAnswerAnsiPrompt )
   {
      fprintf( ptrBbsRc, "autoansi\n" );
   }
}
