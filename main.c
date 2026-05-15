/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Program entry point.
 */
#include "config_file.h"
#include "client.h"
#include "client_globals.h"
#include "defs.h"
#include "utility.h"
/// @brief Entry point for the BBS client.
///
/// @param argc Argument count.
/// @param argv Argument vector.
///
/// @return Process exit status.
int main( int argc, char *argv[] )
{
   aryEscape[0] = '\033';
   aryEscape[1] = '\0';
   if ( *argv[0] == '-' )
   {
      isLoginShell = true;
   }
   else
   {
      isLoginShell = false;
   }
   initialize();
   findHome();
   readConfig();
   openTmpFile();
   arguments( argc, argv );
   connectBbs();
   sigInit();
   telInit();
   setTerm();
   looper();
   exit( 0 );
   return ( 0 );
}
