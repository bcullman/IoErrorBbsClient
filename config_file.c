/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file opens the client configuration file.
 */
#include "config_file.h"
#include "client.h"
#include "config_globals.h"
#include "defs.h"
#include <sys/stat.h>
#include "utility.h"
static int ensureConfigDirectoryExists( const char *ptrPath );

/// @brief Create the parent config directory tree for a config file path.
///
/// @param ptrPath Full config file path whose parent directories should exist.
///
/// @return `0` on success, otherwise `-1`.
static int ensureConfigDirectoryExists( const char *ptrPath )
{
   char aryDirectoryPath[PATH_MAX];
   char *ptrSlash;

   if ( ptrPath == NULL || *ptrPath == '\0' )
   {
      errno = EINVAL;
      return -1;
   }
   if ( strlen( ptrPath ) >= sizeof( aryDirectoryPath ) )
   {
      errno = ENAMETOOLONG;
      return -1;
   }

   snprintf( aryDirectoryPath, sizeof( aryDirectoryPath ), "%s", ptrPath );
   ptrSlash = strrchr( aryDirectoryPath, '/' );
   if ( ptrSlash == NULL )
   {
      return 0;
   }
   *ptrSlash = '\0';

   for ( ptrSlash = aryDirectoryPath + 1; *ptrSlash != '\0'; ptrSlash++ )
   {
      if ( *ptrSlash == '/' )
      {
         *ptrSlash = '\0';
         if ( mkdir( aryDirectoryPath, 0700 ) < 0 && errno != EEXIST )
         {
            return -1;
         }
         *ptrSlash = '/';
      }
   }
   if ( mkdir( aryDirectoryPath, 0700 ) < 0 && errno != EEXIST )
   {
      return -1;
   }

   return 0;
}

/// @brief Open the main client configuration file.
///
/// The function first tries read-write access, then creates the file if needed,
/// and finally falls back to read-only access with a warning.
///
/// @return A stream for `aryConfigFileName`, or `NULL` if the file could not be
/// opened at all.
FILE *openConfigFile( void )
{
   FILE *ptrFileHandle;
   int savedErrno;

   ptrFileHandle = fopen( aryConfigFileName, "r+" );
   if ( !ptrFileHandle )
   {
      savedErrno = errno;
      if ( ensureConfigDirectoryExists( aryConfigFileName ) < 0 )
      {
         savedErrno = errno;
      }
      ptrFileHandle = fopen( aryConfigFileName, "w+" );
   }
   if ( !ptrFileHandle )
   {
      ptrFileHandle = fopen( aryConfigFileName, "r" );
      if ( ptrFileHandle )
      {
         isConfigFileReadOnly = 1;
         errno = savedErrno;
         sPerror( "Configuration is read-only", "Warning" );
      }
      else
      {
         sPerror( "Can't open configuration file", "Warning" );
      }
   }
   return ( ptrFileHandle );
}
