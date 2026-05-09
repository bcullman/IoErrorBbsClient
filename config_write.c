/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file writes the client configuration back to config.toml.
 */
#include "bbsrc.h"
#include "client_globals.h"
#include "config_globals.h"
#include "defs.h"
#include "filter_globals.h"
#include "utility.h"

static bool isUppercaseDefaultEnabled( int lowerKey );
static const char *localCommandKeyName( int inputChar );
static void printTomlEscapedString( const char *ptrText );
static void writeBehaviorSettings( void );
static void writeConnectionSettings( void );
static void writeDefaultSettings( void );
static void writeLocalCommandKeySettings( void );

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
   fputc( '"', ptrBbsRc );
   while ( *ptrText != '\0' )
   {
      switch ( *ptrText )
      {
         case '"':
            fputs( "\\\"", ptrBbsRc );
            break;

         case '\\':
            fputs( "\\\\", ptrBbsRc );
            break;

         case '\n':
            fputs( "\\n", ptrBbsRc );
            break;

         case '\r':
            fputs( "\\r", ptrBbsRc );
            break;

         case '\t':
            fputs( "\\t", ptrBbsRc );
            break;

         default:
            fputc( *ptrText, ptrBbsRc );
            break;
      }
      ptrText++;
   }
   fputc( '"', ptrBbsRc );
}

/// @brief Write the scalar behavior settings.
///
/// @return This helper does not return a value.
static void writeBehaviorSettings( void )
{
   fprintf( ptrBbsRc, "[behavior]\n" );
   fprintf( ptrBbsRc, "auto_answer_ansi = %s\n",
            flagsConfiguration.shouldAutoAnswerAnsiPrompt ? "true" : "false" );
   fprintf( ptrBbsRc, "autocomplete_recipients = %s\n",
            flagsConfiguration.shouldEnableNameAutocomplete ? "true" : "false" );
   fprintf( ptrBbsRc, "clickable_url_summaries = %s\n",
            flagsConfiguration.shouldEnableClickableUrls ? "true" : "false" );
   fprintf( ptrBbsRc, "screen_reader_mode = %s\n",
            flagsConfiguration.isScreenReaderModeEnabled ? "true" : "false" );
   fprintf( ptrBbsRc, "suppress_enemy_express = %s\n",
            flagsConfiguration.shouldSquelchExpress ? "true" : "false" );
   fprintf( ptrBbsRc, "suppress_enemy_posts = %s\n",
            flagsConfiguration.shouldSquelchPost ? "true" : "false" );
   fprintf( ptrBbsRc, "tcp_keepalive = %s\n",
            flagsConfiguration.shouldUseTcpKeepalive ? "true" : "false" );
   fprintf( ptrBbsRc, "update_title_bar = %s\n",
            flagsConfiguration.shouldEnableTitleBar ? "true" : "false" );
#ifdef ENABLE_KEYCHAIN
   fprintf( ptrBbsRc, "use_keychain = %s\n",
            flagsConfiguration.shouldUseKeychain ? "true" : "false" );
#endif
   fprintf( ptrBbsRc, "\n" );
}

/// @brief Write the configured site, port, editor, and auto-login settings.
///
/// @return This helper does not return a value.
static void writeConnectionSettings( void )
{
   fprintf( ptrBbsRc, "[connection]\n" );
   fprintf( ptrBbsRc, "editor = " );
   printTomlEscapedString( aryEditor );
   fprintf( ptrBbsRc, "\n" );
   fprintf( ptrBbsRc, "host = " );
   printTomlEscapedString( aryBbsHost );
   fprintf( ptrBbsRc, "\n" );
   fprintf( ptrBbsRc, "port = %d\n", bbsPort );
   if ( *aryAutoName != '\0' )
   {
      fprintf( ptrBbsRc, "auto_login_name = " );
      printTomlEscapedString( aryAutoName );
      fprintf( ptrBbsRc, "\n" );
   }
   fprintf( ptrBbsRc, "\n" );
}

/// @brief Write the semantic uppercase-default toggles.
///
/// @return This helper does not return a value.
static void writeDefaultSettings( void )
{
   fprintf( ptrBbsRc, "[defaults]\n" );
   fprintf( ptrBbsRc, "show_full_profile_by_default = %s\n",
            isUppercaseDefaultEnabled( 'p' ) ? "true" : "false" );
   fprintf( ptrBbsRc, "show_long_who_by_default = %s\n",
            isUppercaseDefaultEnabled( 'w' ) ? "true" : "false" );
   fprintf( ptrBbsRc, "\n" );
}

/// @brief Write the configured local command-sequence keys.
///
/// @return This helper does not return a value.
static void writeLocalCommandKeySettings( void )
{
   fprintf( ptrBbsRc, "[local_command_keys]\n" );
   fprintf( ptrBbsRc, "away = " );
   printTomlEscapedString( localCommandKeyName( awayKey ) );
   fprintf( ptrBbsRc, "\n" );
   fprintf( ptrBbsRc, "browser = " );
   printTomlEscapedString( localCommandKeyName( browserKey ) );
   fprintf( ptrBbsRc, "\n" );
   fprintf( ptrBbsRc, "capture = " );
   printTomlEscapedString( localCommandKeyName( captureKey ) );
   fprintf( ptrBbsRc, "\n" );
   fprintf( ptrBbsRc, "command = " );
   printTomlEscapedString( localCommandKeyName( commandKey ) );
   fprintf( ptrBbsRc, "\n" );
   fprintf( ptrBbsRc, "quit = " );
   printTomlEscapedString( localCommandKeyName( quitKey ) );
   fprintf( ptrBbsRc, "\n" );
   fprintf( ptrBbsRc, "shell = " );
   printTomlEscapedString( localCommandKeyName( shellKey ) );
   fprintf( ptrBbsRc, "\n" );
   fprintf( ptrBbsRc, "suspend = " );
   printTomlEscapedString( localCommandKeyName( suspKey ) );
   fprintf( ptrBbsRc, "\n" );
}

/// @brief Rewrite the current in-memory configuration back to config.toml.
///
/// @return This function does not return a value.
void writeBbsRc( void )
{
   rewind( ptrBbsRc );

   writeConnectionSettings();
   writeLocalCommandKeySettings();
   writeDefaultSettings();
   writeBehaviorSettings();

   fflush( ptrBbsRc );
   truncateBbsRc( ftell( ptrBbsRc ) );
}
