/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file handles fatal errors and shutdown cleanup.
 */
#include "client.h"
#include "client_globals.h"
#include "config_globals.h"
#include "defs.h"
#include "pane_ui.h"
#include "utility.h"
static void reopenTempFileForShutdown( void );
static void waitForChildShutdown( void );

/// @brief Print a fatal error heading and message, then terminate the client.
///
/// @param message Error text to display.
/// @param heading Heading used by the message UI.
///
/// @return This function does not return to the caller.
noreturn void fatalExit( const char *message, const char *heading )
{
   fflush( stdout );
   sError( message, heading );
   myExit();
}

/// @brief Print a fatal `errno`-based error, then terminate the client.
///
/// @param error Short operation label passed to `perror`-style reporting.
/// @param heading Heading used by the message UI.
///
/// @return This function does not return to the caller.
noreturn void fatalPerror( const char *error, const char *heading )
{
   int savedErrno = errno;

   fflush( stdout );
   errno = savedErrno;
   sPerror( error, heading );
   myExit();
}

/// @brief Shut down the client cleanly and exit the process.
///
/// @return This function does not return to the caller.
noreturn void myExit( void )
{
   fflush( stdout );
   waitForChildShutdown();
   paneUiLeaveForExit();
   resetTerm();
   reopenTempFileForShutdown();
   deinitialize();
   exit( 0 );
}

/// @brief Reopen the temp file in truncate mode before shutdown when needed.
///
/// @return This function does not return a value.
static void reopenTempFileForShutdown( void )
{
   if ( flagsConfiguration.isLastSave )
   {
      if ( !( tempFile = freopen( aryTempFileName, "w+", tempFile ) ) )
      {
         sPerror( "myExit: reopen temp file before exit", "Shutdown warning" );
      }
   }
}

/// @brief Report a temp-file write error unless it was an interrupted system call.
///
/// @return This function does not return a value.
void tempFileError( void )
{
   if ( errno == EINTR )
   {
      return;
   }
   fprintf( stderr, "\r\n" );
   sPerror( "writing tempfile", "Local error" );
}

/// @brief Wait for an active child process to terminate during shutdown.
///
/// @return This function does not return a value.
static void waitForChildShutdown( void )
{
   if ( childPid )
   {
      // Wait for child to terminate
      sigOff();
      childPid = ( -childPid );
      while ( childPid )
      {
         sigpause( 0 );
      }
   }
}
