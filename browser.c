/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file handles selecting and launching URLs from the browser hotkey menu.
 */
#include "browser.h"
#include "config_globals.h"
#include "defs.h"
#include "filter.h"
#include "filter_globals.h"
#include "getline_input.h"
#include "network_globals.h"
#include <spawn.h>
#include "utility.h"
extern char **environ;

static bool launchBrowserUrl( const char *ptrUrl );

/// @brief Ask macOS to open a URL with the default browser handler.
///
/// @param ptrUrl URL to open.
///
/// @return `true` if the browser command was started successfully, otherwise `false`.
static bool launchBrowserUrl( const char *ptrUrl )
{
   char *aryArguments[3];
   pid_t browserProcessId;
   int spawnResult;

   if ( ptrUrl == NULL || !*ptrUrl )
   {
      return false;
   }
   aryArguments[0] = "open";
   aryArguments[1] = (char *)ptrUrl;
   aryArguments[2] = NULL;

   spawnResult = posix_spawnp( &browserProcessId, aryArguments[0], NULL, NULL, aryArguments, environ );
   if ( spawnResult != 0 )
   {
      stdPrintf( "Unable to launch macOS browser handler.\r\n" );
      return false;
   }

   return true;
}

/// @brief Let the user choose and open a queued URL from the browser menu.
///
/// @return This function does not return a value.
void openBrowser( void )
{
   int inputIndex;
   int originalCaptureState;
   char aryLine[4];
   char *ptrUrlEntry;

   if ( urlQueue->itemCount < 1 )
   {
      return;
   }
   if ( urlQueue->itemCount == 1 )
   {
      launchBrowserUrl( urlQueue->start + ( urlQueue->objsize * urlQueue->head ) );
      reprintLine();
      return;
   }

   originalCaptureState = capture;
   capture = 0;
   shouldIgnoreNetwork = true;
   printf( "\r\n\n" );
   ptrUrlEntry = urlQueue->start + ( urlQueue->objsize * urlQueue->head );
   for ( inputIndex = 0; inputIndex < urlQueue->itemCount; inputIndex++ )
   {
      if ( strlen( ptrUrlEntry ) > 72 )
      {
         char aryShortUrl[71];

         snprintf( aryShortUrl, sizeof( aryShortUrl ), "%.70s", ptrUrlEntry );
         printf( "%d. %-70s...\r\n", inputIndex + 1, aryShortUrl );
      }
      else
      {
         printf( "%d. %s\r\n", inputIndex + 1, ptrUrlEntry );
      }
      ptrUrlEntry += urlQueue->objsize;
      if ( ptrUrlEntry >= (char *)( urlQueue->start + ( urlQueue->objsize * urlQueue->size ) ) )
      {
         ptrUrlEntry = urlQueue->start;
      }
   }

   printf( "\r\nChoose the URL you want to view: " );
   getString( 3, aryLine, 1 );
   printf( "\r\n" );
   inputIndex = atoi( aryLine );
   ptrUrlEntry = urlQueue->start + ( urlQueue->objsize * urlQueue->head );
   if ( inputIndex > 0 && inputIndex <= urlQueue->itemCount )
   {
      int urlIndex;

      inputIndex -= 1;
      for ( urlIndex = 0; urlIndex < inputIndex; urlIndex++ )
      {
         ptrUrlEntry += urlQueue->objsize;
         if ( ptrUrlEntry >= (char *)( urlQueue->start + ( urlQueue->objsize * urlQueue->size ) ) )
         {
            ptrUrlEntry = urlQueue->start;
         }
      }
      launchBrowserUrl( ptrUrlEntry );
   }
   shouldIgnoreNetwork = false;
   reprintLine();
   capture = originalCaptureState;
}
