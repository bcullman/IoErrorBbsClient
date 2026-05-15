/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file handles hotkey configuration from the client configuration menu.
 */
#include "client_globals.h"
#include "config_globals.h"
#include "config_menu.h"
#include "defs.h"
#include "utility.h"

/// @brief Configure the client's hotkeys from the options menu.
///
/// @return This function does not return a value.
void configureHotkeys( void )
{
   stdPrintf( "Hotkeys\r\n\n" );
   stdPrintf( "Enter command key (%s) -> ", strCtrl( commandKey ) );
   while ( true )
   {
      stdPrintf( "%s\r\n", strCtrl( commandKey = newKey( commandKey ) ) );
      if ( commandKey < ' ' )
      {
         break;
      }
      stdPrintf( "You must use a control character for your command key, try again -> " );
   }
   stdPrintf( "Enter key to quit client (%s) -> ", strCtrl( quitKey ) );
   stdPrintf( "%s\r\n", strCtrl( quitKey = newKey( quitKey ) ) );
   if ( !isLoginShell )
   {
      stdPrintf( "Enter key to suspend client (%s) -> ", strCtrl( suspKey ) );
      stdPrintf( "%s\r\n", strCtrl( suspKey = newKey( suspKey ) ) );
      stdPrintf( "Enter key to start a new shell (%s) -> ", strCtrl( shellKey ) );
      stdPrintf( "%s\r\n", strCtrl( shellKey = newKey( shellKey ) ) );
   }
   stdPrintf( "Enter key to toggle capture mode (%s) -> ", strCtrl( captureKey ) );
   stdPrintf( "%s\r\n", strCtrl( captureKey = newKey( captureKey ) ) );
   stdPrintf( "Enter key to enable away from keyboard (%s) -> ", strCtrl( awayKey ) );
   stdPrintf( "%s\r\n", strCtrl( awayKey = newKey( awayKey ) ) );
   stdPrintf( "Enter key to browse a website (%s) -> ", strCtrl( browserKey ) );
   stdPrintf( "%s\r\n", strCtrl( browserKey = newKey( browserKey ) ) );
}

/// @brief Read a replacement hotkey while avoiding collisions with other hotkeys.
///
/// The raw key is read with `getKey()` so terminal translations do not change
/// the configured value.
///
/// @param oldkey Existing hotkey value, or `-1` to skip conflict checks against
/// the current key being replaced.
///
/// @return The accepted replacement key, or `oldkey` if the user keeps the
/// current binding.
int newKey( int oldkey )
{
   while ( true )
   {
      int inputChar;

      inputChar = getKey();
      if ( ( ( inputChar == ' ' || inputChar == '\n' || inputChar == '\r' ) &&
             oldkey >= 0 ) ||
           inputChar == oldkey )
      {
         return oldkey;
      }
      if ( oldkey >= 0 &&
           ( inputChar == commandKey || inputChar == suspKey ||
             inputChar == quitKey || inputChar == shellKey ||
             inputChar == captureKey || inputChar == awayKey ||
             inputChar == browserKey ) )
      {
         stdPrintf( "\r\nThat key is already in use for another hotkey, try again -> " );
      }
      else
      {
         return inputChar;
      }
   }
}

/// @brief Format a key value in printable control-key notation.
///
/// @param inputChar Key code to format.
///
/// @return A pointer to a static buffer containing either the literal printable
/// character or a caret-style control representation.
char *strCtrl( int inputChar )
{
   static char aryControlText[3];

   if ( inputChar <= 31 || inputChar == DEL )
   {
      aryControlText[0] = '^';
      aryControlText[1] = (char)( inputChar == 10 ? 'M' : ( inputChar ^ 0x40 ) );
   }
   else
   {
      aryControlText[0] = (char)inputChar;
      aryControlText[1] = 0;
   }
   aryControlText[2] = 0;
   return aryControlText;
}
