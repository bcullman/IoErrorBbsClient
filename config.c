/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file handles client configuration.
 */
#include "config_file.h"
#include "client.h"
#include "client_globals.h"
#include "color.h"
#include "config_globals.h"
#include "config_menu.h"
#include "defs.h"
#include "filter_globals.h"
#include "utility.h"
static const char *CONFIG_MAIN_MENU_KEYS = "cefhikoqx \n";

#define UPGRADE \
   "Thank you for upgrading to the latest version of IO ERROR's ISCA BBS Client!\r\nPlease take a moment to familiarize yourself with our new features."
#define DOWNGRADE \
   "You appear to have downgraded your version of IO ERROR's ISCA BBS Client.\r\nIf you continue running this client, you may lose some of your preferences and\r\nfeatures you are accustomed to.  Please visit the above website to upgrade\r\nto the latest version of IO ERROR's ISCA BBS Client."
#define COLOR_INFO \
   "IO ERROR's ISCA BBS Client allows you to choose what colors posts and express\r\nmessages are displayed with.  Use the <C>olor menu in the client configuration\r\nmenu to create your customized color scheme."
#define ENEMY_INFO \
   "You can now turn off the notification of killed posts and express messages\r\nfrom people on your enemy list.\r\n\nSelect Yes to be notified, or No to not be notified."
#define ADVANCED_OPTIONS \
   "Advanced users may wish to use the configuration menu now to change options\r\nbefore logging in."

static const char *describeKeyForHelp( int inputChar );

/// @brief Run the top-level client configuration menu.
///
/// The selected submenus update the in-memory configuration, and quitting the
/// menu writes the changes back to `config.toml` when the file is writable.
///
/// @return This function does not return a value.
void configClient( void )
{
   flagsConfiguration.isConfigMode = 1;
   if ( isConfigFileReadOnly )
   {
      stdPrintf( "\r\nConfiguration file is read-only, unable to save configuration for next session.\r\n" );
   }
   else if ( !ptrConfigFile )
   {
      stdPrintf( "\r\nNo configuration file, unable to save configuration for next session.\r\n" );
   }
   while ( true )
   {
      int inputChar;

      printThemedMnemonicText( "\r\n<C>olor  <E>nemy list  <F>riend list  <H>otkeys\r\n<I>nfo  <O>ptions  <X>press  <Q>uit", color.number );
      printThemedMnemonicText( "\r\nClient config -> ", color.forum );
      printAnsiForegroundColorValue( color.text );
      inputChar = readValidatedMenuKey( CONFIG_MAIN_MENU_KEYS );
      switch ( inputChar )
      {
         case 'c':
            colorConfig();
            break;

         case 'x':
            expressConfig();
            break;

         case 'i':
            information();
            break;

         case 'o':
            configureOptionsMenu();
            break;

         case 'h':
            configureHotkeys();
            break;

         case 'f':
            stdPrintf( "Friend list\r\n" );
            editUsers( friendList, fStrCompareVoid, "friend" );
            break;

         case 'e':
            stdPrintf( "Enemy list\r\n" );
            editUsers( enemyList, strCompareVoid, "enemy" );
            break;

         case 'q':
         case ' ':
         case '\n':
            stdPrintf( "Quit\r\n" );
            flagsConfiguration.isConfigMode = 0;
            if ( isConfigFileReadOnly || !ptrConfigFile )
            {
               return;
            }
            writeConfig();
            return;
            // NOTREACHED

         default:
            break;
      }
   }
}

/// @brief Describe a configured key in a user-facing format.
///
/// @param inputChar Key value to describe.
///
/// @return A printable name for the key, such as `Esc`, `Space`, `Return`, or
/// the control-key form returned by `strCtrl()`.
static const char *describeKeyForHelp( int inputChar )
{
   switch ( inputChar )
   {
      case ESC:
         return "Esc";

      case ' ':
         return "Space";

      case '\n':
      case '\r':
         return "Return";

      case '\t':
         return "Tab";

      default:
         return strCtrl( inputChar );
   }
}

/// @brief Perform version-gated setup prompts and initialize new defaults.
///
/// This setup flow carries forward the legacy first-run and upgrade prompts
/// borrowed from Client 9 with permission, then writes the resulting
/// configuration back to disk.
///
/// @param newVersion Previously stored client configuration version.
///
/// @return This function does not return a value.
void setup( int newVersion )
{
   setTerm();
   if ( newVersion > INT_VERSION )
   {
      sInfo( DOWNGRADE, "Downgrade" );
   }
   else if ( newVersion >= 1 )
   {
      sInfo( UPGRADE, "Upgrade" );
   }
   fflush( stdout );

   if ( newVersion < 220 )
   {
      if ( sPrompt( ENEMY_INFO, "Notify when posts and express messages from enemies are killed?", 1 ) )
      {
         flagsConfiguration.shouldSquelchPost = 0;
         flagsConfiguration.shouldSquelchExpress = 0;
      }
      else
      {
         flagsConfiguration.shouldSquelchPost = 1;
         flagsConfiguration.shouldSquelchExpress = 1;
      }

      fflush( stdout );
      sInfo( COLOR_INFO, "Colors" );
   }
   if ( newVersion < 237 )
   {
      char aryUrlInfo[512];

      snprintf( aryUrlInfo,
                sizeof( aryUrlInfo ),
                "You can go directly to a website address you see in a post or express\r\nmessage by pressing <%s> then <%s>.  You can also change these keys in\r\nthe client configuration.  Clickable URLs are also emitted directly to modern\r\nmacOS terminals using OSC 8 links.",
                describeKeyForHelp( commandKey ),
                describeKeyForHelp( browserKey ) );
      sInfo( aryUrlInfo, "Websites" );
   }
   promptForScreenReaderModeIfUnset();
   defaultNameAutocompleteIfUnset();
   if ( sPrompt( ADVANCED_OPTIONS, "Configure the client now?", 0 ) )
   {
      configClient();
   }
   else
   {
      writeConfig();
   }
   resetTerm();
   return;
}
