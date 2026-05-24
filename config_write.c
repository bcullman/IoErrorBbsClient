/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file writes the client configuration back to config.toml.
 */
#include "config_file.h"
#include "client_globals.h"
#include "color.h"
#include "config_globals.h"
#include "defs.h"
#include "filter_globals.h"
#include "utility.h"

static bool isUppercaseDefaultEnabled( int lowerKey );
static const char *localCommandKeyName( int inputChar );
static void printTomlEscapedString( const char *ptrText );
static void printTomlHexColor( int colorValue );
static void writeAwaySettings( void );
static void writeBehaviorSettings( void );
static void writeColorSettings( const char *ptrSectionName,
                                const Color *ptrColorTable );
static void writeConnectionSettings( void );
static void writeContactSettings( void );
static void writeDefaultSettings( void );
static void writeLocalCommandKeySettings( void );
static void writeMetadataSettings( void );

/// @brief Check whether a lowercase action key is swapped with its uppercase variant.
///
/// @param lowerKey Lowercase action key to inspect.
///
/// @return `true` when the lowercase key defaults to the uppercase action.
static bool isUppercaseDefaultEnabled( int lowerKey )
{
   int upperKey;

   upperKey = toupper( lowerKey );
   return aryKeyMap[lowerKey] == upperKey && aryKeyMap[upperKey] == lowerKey;
}

/// @brief Return the canonical TOML name for one configured local-command key.
///
/// @param inputChar Key code to format.
///
/// @return Canonical TOML string name for the key.
static const char *localCommandKeyName( int inputChar )
{
   static char aryPrintableKey[2];

   switch ( inputChar )
   {
      case '\b':
         return "backspace";

      case '\n':
      case '\r':
         return "enter";

      case '\t':
         return "tab";

      case ESC:
         return "esc";

      case ' ':
         return "space";

      case DEL:
         return "del";

      default:
         if ( inputChar >= 1 && inputChar <= CTRL_Z )
         {
            static char aryControlKey[7];

            snprintf( aryControlKey, sizeof( aryControlKey ), "ctrl-%c",
                      (char)( inputChar + '@' + 32 ) );
            return aryControlKey;
         }

         aryPrintableKey[0] = (char)inputChar;
         aryPrintableKey[1] = '\0';
         return aryPrintableKey;
   }
}

/// @brief Write one TOML double-quoted string with basic escapes.
///
/// @param ptrText Raw string value to serialize.
///
/// @return This helper does not return a value.
static void printTomlEscapedString( const char *ptrText )
{
   fputc( '"', ptrConfigFile );
   while ( *ptrText != '\0' )
   {
      switch ( *ptrText )
      {
         case '"':
            fputs( "\\\"", ptrConfigFile );
            break;

         case '\\':
            fputs( "\\\\", ptrConfigFile );
            break;

         case '\n':
            fputs( "\\n", ptrConfigFile );
            break;

         case '\r':
            fputs( "\\r", ptrConfigFile );
            break;

         case '\t':
            fputs( "\\t", ptrConfigFile );
            break;

         default:
            fputc( *ptrText, ptrConfigFile );
            break;
      }
      ptrText++;
   }
   fputc( '"', ptrConfigFile );
}

/// @brief Write one RGB color value as a TOML hex string.
///
/// @param colorValue Encoded RGB color value.
///
/// @return This helper does not return a value.
static void printTomlHexColor( int colorValue )
{
   fprintf( ptrConfigFile, "\"#%02x%02x%02x\"",
            colorValueRed( colorValue ),
            colorValueGreen( colorValue ),
            colorValueBlue( colorValue ) );
}

/// @brief Write config metadata used for future schema upgrades.
///
/// @return This helper does not return a value.
static void writeMetadataSettings( void )
{
   fprintf( ptrConfigFile, "[metadata]\n" );
   fprintf( ptrConfigFile, "version = %d\n\n", INT_VERSION );
}

/// @brief Write the configured away-message array.
///
/// @return This helper does not return a value.
static void writeAwaySettings( void )
{
   int itemIndex;
   bool shouldWriteComma;

   fprintf( ptrConfigFile, "[away]\n" );
   fprintf( ptrConfigFile, "messages = [" );
   shouldWriteComma = false;
   for ( itemIndex = 0; itemIndex < 5 && *aryAwayMessageLines[itemIndex] != '\0'; itemIndex++ )
   {
      if ( shouldWriteComma )
      {
         fprintf( ptrConfigFile, ", " );
      }
      printTomlEscapedString( aryAwayMessageLines[itemIndex] );
      shouldWriteComma = true;
   }
   fprintf( ptrConfigFile, "]\n\n" );
}

/// @brief Write the scalar behavior settings.
///
/// @return This helper does not return a value.
static void writeBehaviorSettings( void )
{
   fprintf( ptrConfigFile, "[behavior]\n" );
   fprintf( ptrConfigFile, "auto_answer_ansi = %s\n",
            flagsConfiguration.shouldAutoAnswerAnsiPrompt ? "true" : "false" );
   fprintf( ptrConfigFile, "auto_reply_to_x_messages = %s\n",
            isXland ? "true" : "false" );
   fprintf( ptrConfigFile, "dark_theme_black_background_fallback = %s\n",
            useBlackThemeBackgrounds ? "true" : "false" );
   fprintf( ptrConfigFile, "color_output_mode = " );
   printTomlEscapedString( colorOutputModeName( configuredColorOutputMode ) );
   fprintf( ptrConfigFile, "\n" );
   fprintf( ptrConfigFile, "screen_reader_mode = %s\n",
            flagsConfiguration.isScreenReaderModeEnabled ? "true" : "false" );
   fprintf( ptrConfigFile, "# When screen_reader_mode is true, the client forces some settings off at runtime.\n" );
   fprintf( ptrConfigFile, "# Saved values are preserved here, but screen reader mode takes precedence.\n" );
   fprintf( ptrConfigFile, "autocomplete_recipients = %s\n",
            flagsConfiguration.shouldEnableNameAutocomplete ? "true" : "false" );
   fprintf( ptrConfigFile, "clickable_url_summaries = %s\n",
            flagsConfiguration.shouldEnableClickableUrls ? "true" : "false" );
   fprintf( ptrConfigFile, "suppress_enemy_express = %s\n",
            flagsConfiguration.shouldSquelchExpress ? "true" : "false" );
   fprintf( ptrConfigFile, "suppress_enemy_posts = %s\n",
            flagsConfiguration.shouldSquelchPost ? "true" : "false" );
   fprintf( ptrConfigFile, "tcp_keepalive = %s\n",
            flagsConfiguration.shouldUseTcpKeepalive ? "true" : "false" );
   fprintf( ptrConfigFile, "update_title_bar = %s\n",
            flagsConfiguration.shouldEnableTitleBar ? "true" : "false" );
#ifdef ENABLE_KEYCHAIN
   fprintf( ptrConfigFile, "use_keychain = %s\n",
            flagsConfiguration.shouldUseKeychain ? "true" : "false" );
#endif
   fprintf( ptrConfigFile, "\n" );
}

/// @brief Write the themed color settings using stable TOML keys.
///
/// @return This helper does not return a value.
static void writeColorSettings( const char *ptrSectionName,
                                const Color *ptrColorTable )
{
   int colorFieldIndex;

   fprintf( ptrConfigFile, "[%s]\n", ptrSectionName );
   for ( colorFieldIndex = 0; colorFieldIndex < COLOR_FIELD_COUNT; colorFieldIndex++ )
   {
      const char *ptrColorKeyName;
      const char *ptrColorName;
      int colorValue;

      ptrColorKeyName = colorFieldTomlKeyName( colorFieldIndex );
      if ( ptrColorKeyName == NULL )
      {
         continue;
      }

      colorValue = colorFieldValueForColor( ptrColorTable, colorFieldIndex );
      fprintf( ptrConfigFile, "%s = ", ptrColorKeyName );
      ptrColorName = colorNameFromValue( colorValue );
      if ( colorValueIsRgb( colorValue ) )
      {
         printTomlHexColor( colorValue );
      }
      else if ( ptrColorName != NULL )
      {
         printTomlEscapedString( ptrColorName );
      }
      else
      {
         fprintf( ptrConfigFile, "%d", colorValue );
      }
      fprintf( ptrConfigFile, "\n" );
   }
   fprintf( ptrConfigFile, "\n" );
}

/// @brief Write the configured site, port, editor, and auto-login settings.
///
/// @return This helper does not return a value.
static void writeConnectionSettings( void )
{
   fprintf( ptrConfigFile, "[connection]\n" );
   fprintf( ptrConfigFile, "editor = " );
   printTomlEscapedString( aryEditor );
   fprintf( ptrConfigFile, "\n" );
   fprintf( ptrConfigFile, "host = " );
   printTomlEscapedString( aryBbsHost );
   fprintf( ptrConfigFile, "\n" );
   fprintf( ptrConfigFile, "port = %d\n", bbsPort );
   if ( *aryAutoName != '\0' )
   {
      fprintf( ptrConfigFile, "auto_login_name = " );
      printTomlEscapedString( aryAutoName );
      fprintf( ptrConfigFile, "\n" );
   }
   fprintf( ptrConfigFile, "\n" );
}

/// @brief Write the configured friend and enemy contacts.
///
/// @return This helper does not return a value.
static void writeContactSettings( void )
{
   unsigned int itemIndex;

   fprintf( ptrConfigFile, "[contacts]\n" );
   fprintf( ptrConfigFile, "enemies = [" );
   for ( itemIndex = 0; itemIndex < enemyList->nitems; itemIndex++ )
   {
      if ( itemIndex != 0 )
      {
         fprintf( ptrConfigFile, ", " );
      }
      printTomlEscapedString( (const char *)enemyList->items[itemIndex] );
   }
   fprintf( ptrConfigFile, "]\n" );

   fprintf( ptrConfigFile, "friends = [" );
   for ( itemIndex = 0; itemIndex < friendList->nitems; itemIndex++ )
   {
      const friend *ptrFriend;

      ptrFriend = friendList->items[itemIndex];
      if ( itemIndex != 0 )
      {
         fprintf( ptrConfigFile, ", " );
      }
      fprintf( ptrConfigFile, "{ name = " );
      printTomlEscapedString( ptrFriend->name );
      fprintf( ptrConfigFile, ", info = " );
      printTomlEscapedString( ptrFriend->info );
      fprintf( ptrConfigFile, " }" );
   }
   fprintf( ptrConfigFile, "]\n\n" );
}

/// @brief Write the semantic uppercase-default toggles.
///
/// @return This helper does not return a value.
static void writeDefaultSettings( void )
{
   fprintf( ptrConfigFile, "[defaults]\n" );
   fprintf( ptrConfigFile, "show_full_profile_by_default = %s\n",
            isUppercaseDefaultEnabled( 'p' ) ? "true" : "false" );
   fprintf( ptrConfigFile, "show_long_who_by_default = %s\n",
            isUppercaseDefaultEnabled( 'w' ) ? "true" : "false" );
   fprintf( ptrConfigFile, "\n" );
}

/// @brief Write the configured local command-sequence keys.
///
/// @return This helper does not return a value.
static void writeLocalCommandKeySettings( void )
{
   fprintf( ptrConfigFile, "[local_command_keys]\n" );
   fprintf( ptrConfigFile, "away = " );
   printTomlEscapedString( localCommandKeyName( awayKey ) );
   fprintf( ptrConfigFile, "\n" );
   fprintf( ptrConfigFile, "browser = " );
   printTomlEscapedString( localCommandKeyName( browserKey ) );
   fprintf( ptrConfigFile, "\n" );
   fprintf( ptrConfigFile, "capture = " );
   printTomlEscapedString( localCommandKeyName( captureKey ) );
   fprintf( ptrConfigFile, "\n" );
   fprintf( ptrConfigFile, "command = " );
   printTomlEscapedString( localCommandKeyName( commandKey ) );
   fprintf( ptrConfigFile, "\n" );
   fprintf( ptrConfigFile, "quit = " );
   printTomlEscapedString( localCommandKeyName( quitKey ) );
   fprintf( ptrConfigFile, "\n" );
   fprintf( ptrConfigFile, "shell = " );
   printTomlEscapedString( localCommandKeyName( shellKey ) );
   fprintf( ptrConfigFile, "\n" );
   fprintf( ptrConfigFile, "suspend = " );
   printTomlEscapedString( localCommandKeyName( suspKey ) );
   fprintf( ptrConfigFile, "\n" );
}

/// @brief Rewrite the current in-memory configuration back to config.toml.
///
/// @return This function does not return a value.
void writeConfig( void )
{
   rewind( ptrConfigFile );

   writeMetadataSettings();
   writeConnectionSettings();
   writeLocalCommandKeySettings();
   writeDefaultSettings();
   writeBehaviorSettings();
   writeAwaySettings();
   writeContactSettings();
   writeColorSettings( "colors_256", &color256 );
   writeColorSettings( "colors_truecolor", &colorTruecolor );

   fflush( ptrConfigFile );
   truncateConfigFile( ftell( ptrConfigFile ) );
}
