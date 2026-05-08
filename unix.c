/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Everything Unix/system specific lives in this file. Most porting work is
 * expected to stay here.
 *
 * This file covers Unix/macOS builds.
 */
#define _IN_UNIX_C
#include "bbsrc.h"
#include "client.h"
#include "client_globals.h"
#include "config_globals.h"
#include "defs.h"
#include "network_globals.h"
#include "telnet.h"
#include "unix.h"
#include "utility.h"
#include <wordexp.h>
static struct passwd *pw;

static void execCommandWithOptionalArg( const char *ptrCommand, const char *ptrArg );

/// @brief Exit the client cleanly from a signal handler.
///
/// @param signalNumber Signal number being handled.
///
/// @return This signal handler does not return a useful value.
RETSIGTYPE bye( int signalNumber )
{
   (void)signalNumber;
   myExit();
}

/// @brief Parse a command string and execute it with an optional trailing argument.
///
/// The command string is expanded with `wordexp()` using `WRDE_NOCMD` so
/// command substitution is rejected before `execvp()` is called.
///
/// @param ptrCommand Command string to execute.
/// @param ptrArg Optional argument appended after the parsed command words.
///
/// @return This function does not return to the caller.
static void execCommandWithOptionalArg( const char *ptrCommand, const char *ptrArg )
{
   wordexp_t parsedCommand;
   char **aryArguments;
   size_t argumentCount;
   int parseResult;

   parseResult = wordexp( ptrCommand, &parsedCommand, WRDE_NOCMD );
   if ( parseResult != 0 || parsedCommand.we_wordc == 0 )
   {
      fprintf( stderr, "\r\n[Unable to parse command: %s]\r\n", ptrCommand );
      _exit( 1 );
   }

   argumentCount = parsedCommand.we_wordc + ( ptrArg ? 1U : 0U );
   aryArguments = calloc( argumentCount + 1, sizeof( char * ) );
   if ( aryArguments != NULL )
   {
      for ( size_t argumentIndex = 0; argumentIndex < parsedCommand.we_wordc; argumentIndex++ )
      {
         aryArguments[argumentIndex] = parsedCommand.we_wordv[argumentIndex];
      }
      if ( ptrArg )
      {
         aryArguments[parsedCommand.we_wordc] = (char *)ptrArg;
      }

      execvp( aryArguments[0], aryArguments );
      fprintf( stderr, "\r\n" );
      sPerror( "exec", "Local error" );
      free( aryArguments );
      wordfree( &parsedCommand );
      _exit( 1 );
   }

   fprintf( stderr, "\r\n" );
   sPerror( "calloc", "Local error" );
   wordfree( &parsedCommand );
   _exit( 1 );
}

/// @brief Resolve and open the legacy friends file path.
///
/// @return A stream for the resolved friends file.
FILE *findBbsFriends( void )
{
   if ( isLoginShell )
   {
      snprintf( aryBbsFriendsName, sizeof( aryBbsFriendsName ), "/tmp/bbsfriends.%d", getpid() );
   }
   else
   {
      if ( getenv( "BBSFRIENDS" ) )
      {
         snprintf( aryBbsFriendsName, sizeof( aryBbsFriendsName ), "%s", getenv( "BBSFRIENDS" ) );
      }
      else if ( pw )
      {
         snprintf( aryBbsFriendsName, sizeof( aryBbsFriendsName ), "%s/.bbsfriends", pw->pw_dir );
      }
      else if ( getenv( "HOME" ) )
      {
         snprintf( aryBbsFriendsName, sizeof( aryBbsFriendsName ), "%s/.bbsfriends", getenv( "HOME" ) );
      }
      else
      {
         fatalExit( "findBbsFriends: You don't exist, go away.", "Local error" );
      }
   }
   chmod( aryBbsFriendsName, 0600 );
   return ( openBbsFriends() );
}

/// @brief Resolve and open the main `.bbsrc` path.
///
/// Environment overrides and login-shell temp paths are handled before the file
/// is opened through `openBbsRc()`.
///
/// @return A stream for the resolved configuration file.
FILE *findBbsRc( void )
{
   FILE *ptrFileHandle;

   if ( isLoginShell )
   {
      snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "/tmp/bbsrc.%d", getpid() );
   }
   else
   {
      if ( getenv( "BBSRC" ) )
      {
         snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s", getenv( "BBSRC" ) );
      }
      else if ( pw )
      {
         snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s/.bbsrc", pw->pw_dir );
      }
      else if ( getenv( "HOME" ) )
      {
         snprintf( aryBbsRcName, sizeof( aryBbsRcName ), "%s/.bbsrc", getenv( "HOME" ) );
      }
      else
      {
         fatalExit( "findbbsrc: You don't exist, go away.", "Local error" );
      }
   }
   if ( ( ptrFileHandle = fopen( aryBbsRcName, "r" ) ) && chmod( aryBbsRcName, 0600 ) < 0 )
   {
      sPerror( "Can't set access on bbsrc file", "Warning" );
   }
   if ( ptrFileHandle )
   {
      fclose( ptrFileHandle );
   }
   return ( openBbsRc() );
}

/// @brief Discover the current username and mark login-shell sessions.
///
/// @return This function does not return a value.
void findHome( void )
{
   if ( ( pw = getpwuid( getuid() ) ) )
   {
      snprintf( aryUser, sizeof( aryUser ), "%s", pw->pw_name );
   }
   else if ( getenv( "USER" ) )
   {
      snprintf( aryUser, sizeof( aryUser ), "%s", getenv( "USER" ) );
   }
   else
   {
      fatalExit( "findHome: You don't exist, go away.", "Local error" );
   }
   if ( isLoginShell )
   {
      size_t userNameLength = strlen( aryUser );
      if ( userNameLength < sizeof( aryUser ) - 1 )
      {
         snprintf( aryUser + userNameLength, sizeof( aryUser ) - userNameLength, "  (login shell)" );
      }
   }
}

/// @brief Handle terminal resize signals and send an updated NAWS record.
///
/// @param signalNumber Signal number being handled.
///
/// @return This signal handler does not return a useful value.
RETSIGTYPE naws( int signalNumber )
{
   (void)signalNumber;
   if ( oldRows != -1 )
   {
      sendNaws();
   }
#ifdef SIGWINCH
   signal( SIGWINCH, naws );
#endif
}

/// @brief Resolve and open the message temp file used by the editor paths.
///
/// @return This function does not return a value.
void openTmpFile( void )
{
   if ( isLoginShell )
   {
      snprintf( aryTempFileName, sizeof( aryTempFileName ), "/tmp/bbstmp.%d", getpid() );
   }
   else
   {
      if ( getenv( "BBSTMP" ) )
      {
         snprintf( aryTempFileName, sizeof( aryTempFileName ), "%s", getenv( "BBSTMP" ) );
      }
      else if ( pw )
      {
         snprintf( aryTempFileName, sizeof( aryTempFileName ), "%s/.bbstmp", pw->pw_dir );
      }
      else if ( getenv( "HOME" ) )
      {
         snprintf( aryTempFileName, sizeof( aryTempFileName ), "%s/.bbstmp", getenv( "HOME" ) );
      }
      else
      {
         fatalExit( "openTmpFile: You don't exist, go away.", "Local error" );
      }
   }
   if ( !( tempFile = fopen( aryTempFileName, "a+" ) ) )
   {
      fatalPerror( "openTmpFile: fopen", "Local error" );
   }
   if ( chmod( aryTempFileName, 0600 ) < 0 )
   {
      sPerror( "openTmpFile: chmod", "Warning" );
   }
}

/// @brief Handle child-process exit and return control to the parent flow.
///
/// @param signalNumber Signal number being handled.
///
/// @return This signal handler does not return a useful value.
RETSIGTYPE reapChild( int signalNumber )
{
   (void)signalNumber;
   wait( 0 );
   titleBar();
   if ( kill( childPid, SIGCONT ) < 0 )
   {
#ifdef USE_POSIX_SIGSETJMP
      siglongjmp( jumpEnv, 1 );
#else
      longjmp( jumpEnv, 1 );
#endif // USE_POSIX_SIGSETJMP
   }
}

/// @brief Launch a local command such as the shell or external editor.
///
/// The child exit path returns here through the existing setjmp/longjmp signal
/// flow.
///
/// @param aryCommand Command string to execute.
/// @param arg Optional trailing argument for the command.
///
/// @return This function does not return a value.
void run( const char *aryCommand, const char *arg )
{
   fflush( stdout );
#ifdef USE_POSIX_SIGSETJMP
   if ( sigsetjmp( jumpEnv, 1 ) )
   {
#else
   if ( setjmp( jumpEnv ) )
   {
#endif // USE_POSIX_SIGSETJMP
      signal( SIGCHLD, SIG_DFL );
      if ( childPid < 0 )
      {
         childPid = 0;
         myExit();
      }
      else
      {
         setTerm();
         childPid = 0;
      }
   }
   else
   {
      signal( SIGCHLD, reapChild );
      noTitleBar();
      resetTerm();

      if ( !( childPid = fork() ) )
      {
         execCommandWithOptionalArg( aryCommand, arg );
      }
      else if ( childPid > 0 )
      {

         // Flush any stdio data copied into the child process so it is not
         // still pending after the child exits.
         flushInput( 0 );
         (void)inKey();
      }
      else
      {
         fatalPerror( "fork", "Local error" );
      }
   }
}

/// @brief Show the technical information screen.
///
/// @return This function does not return a value.
void techInfo( void )
{
   char aryRuntimeInfo[256];
   char aryRuntimeAccessibility[256];

   snprintf( aryRuntimeInfo,
             sizeof( aryRuntimeInfo ),
             "Runtime: keepalive %s, title bar %s, clickable URLs %s\r\n",
             flagsConfiguration.shouldUseTcpKeepalive ? "on" : "off",
             flagsConfiguration.shouldEnableTitleBar ? "on" : "off",
             flagsConfiguration.shouldEnableClickableUrls ? "on" : "off" );
   snprintf( aryRuntimeAccessibility,
             sizeof( aryRuntimeAccessibility ),
             "Accessibility: screen reader %s, autocomplete %s\r\n",
             flagsConfiguration.isScreenReaderModeEnabled ? "on" : "off",
             flagsConfiguration.shouldEnableNameAutocomplete ? "on" : "off" );

   stdPrintf( "Technical information\r\n\n" );

   feedPager( 3,
              "ISCA BBS Client " BUILD_VERSION " (" BUILD_PLATFORM ")\r\n",
              "Built on: " HOSTTYPE "\r\n",
              "Compiler: " BUILD_COMPILER "\r\n",
              "Build mode: " BUILD_MODE "\r\n",
              "Optimization: " BUILD_OPTIMIZATION_MODE "\r\n",
              "Platform support: " BUILD_PLATFORM_SUPPORT_MODE "\r\n",
              "Universal binary: " BUILD_UNIVERSAL_MODE "\r\n",
              "Sanitizers: " BUILD_SANITIZER_MODE "\r\n",
              "Native optimizations: " BUILD_NATIVE_OPTIMIZATION_MODE "\r\n",
              "Stack protector: " BUILD_STACK_PROTECTOR_MODE "\r\n",
              aryRuntimeInfo,
              aryRuntimeAccessibility,
              (char *)NULL );
}

/// @brief Initialize Unix-side runtime state before connecting to the BBS.
///
/// @return This function does not return a value.
void initialize( void )
{
   if ( !isatty( 0 ) || !isatty( 1 ) || !isatty( 2 ) )
   {
      exit( 0 );
   }

   ptrPtyInput = aryPtyInputBuffer;
   ptrNetInput = aryNetInputBuffer;

   isAway = 0;

#ifdef _IOLBF
   setvbuf( stdout, NULL, _IOLBF, 0 );
#endif

   stdPrintf( "\nISCA BBS Client %s (%s)\n", BUILD_VERSION, "macOS/Unix" );
   stdPrintf( "Copyright (C) 2024-2026 Stilgar, 1995-2003 Michael Hampton\n" );
   stdPrintf( "\nhttps://github.com/StilgarISCA/IoErrorBbsClient\n" );
   stdPrintf( "GPL-2.0-or-later (see LICENSE)\n\n" );
   fflush( stdout );
   xlandQueue = newQueue( 21, MAX_USER_NAME_HISTORY_COUNT );
   if ( !xlandQueue )
   {
      isXland = 0;
   }
   if ( isLoginShell )
   {
      snprintf( aryShell, sizeof( aryShell ), "%s", "/bin/true" );
   }
   else
   {
      if ( getenv( "SHELL" ) )
      {
         snprintf( aryShell, sizeof( aryShell ), "%s", getenv( "SHELL" ) );
      }
      else
      {
         snprintf( aryShell, sizeof( aryShell ), "%s", "/bin/sh" );
      }
   }
   if ( isLoginShell )
   {
      snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "" );
   }
   else
   {
      if ( getenv( "EDITOR" ) )
      {
         snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", getenv( "EDITOR" ) );
      }
      else
      {
         snprintf( aryMyEditor, sizeof( aryMyEditor ), "%s", "nano" );
      }
   }
}

/// @brief Tear down Unix-side temporary files and title-bar state.
///
/// @return This function does not return a value.
void deinitialize( void )
{
   char aryTempFile[PATH_MAX];

   noTitleBar();
   // Remove the backup file Emacs leaves behind.
   snprintf( aryTempFile, sizeof( aryTempFile ), "%s~", aryTempFileName );
   unlink( aryTempFile );
   if ( isLoginShell )
   {
      unlink( aryTempFileName );
      unlink( aryBbsRcName );
      unlink( aryBbsFriendsName );
   }
}

/// @brief Delete a file path using the Unix unlink call.
///
/// @param pathname File path to delete.
///
/// @return Result from `unlink()`.
int deleteFile( const char *pathname )
{
   return unlink( pathname );
}

/// @brief Show a yes/no prompt using the Unix text UI.
///
/// @param info Informational text shown before the prompt.
/// @param question Prompt question text.
/// @param def Default yes/no choice.
///
/// @return `1` for yes, or `0` for no.
int sPrompt( const char *info, const char *question, int def )
{
   stdPrintf( "\r\n%s\r\n\n", info );
   stdPrintf( "%s (%s) -> ", question, def ? "Yes" : "No" );
   if ( yesNoDefault( def ) )
   {
      return 1;
   }
   return 0;
}

/// @brief Show an informational message using the Unix text UI.
///
/// @param info Informational text to print.
/// @param heading Unused heading parameter kept for interface compatibility.
///
/// @return This function does not return a value.
void sInfo( const char *info, const char *heading )
{
   (void)heading;
   // Ignore the heading on Unix.
   stdPrintf( "\r\n%s\r\n\n", info );
   return;
}

/// @brief Print a `perror`-style message using the Unix text UI.
///
/// @param message Error text to append to the heading.
/// @param heading Heading text for the error line.
///
/// @return This function does not return a value.
void sPerror( const char *message, const char *heading )
{
   char aryErrorBuffer[4096];
   int savedErrno = errno;

   snprintf( aryErrorBuffer, sizeof( aryErrorBuffer ), "%s: %s", heading, message );
   errno = savedErrno;
   perror( aryErrorBuffer );
   fprintf( stderr, "\r" );
   return;
}

/// @brief Print a plain error message using the Unix text UI.
///
/// @param message Error text to print.
/// @param heading Heading text for the error line.
///
/// @return This function does not return a value.
void sError( const char *message, const char *heading )
{
   char aryErrorBuffer[4096];
   snprintf( aryErrorBuffer, sizeof( aryErrorBuffer ), "%s: %s", heading, message );
   fflush( stdout );
   fprintf( stderr, "%s\r\n", aryErrorBuffer );
}

/// @brief Move a file to a new path if the old file exists and the new file is missing or empty.
///
/// @param oldpath Existing source path.
/// @param newpath Destination path.
///
/// @return This function does not return a value.
void moveIfNeeded( const char *oldpath, const char *newpath )
{
   FILE *ptrOldFile;
   FILE *ptrNewFile;
   struct stat targetFileStatus;
   bool shouldCopy;

   ptrOldFile = fopen( oldpath, "r" );
   if ( !ptrOldFile )
   {
      return;
   }

   shouldCopy = ( stat( newpath, &targetFileStatus ) != 0 || targetFileStatus.st_size == 0 );
   if ( !shouldCopy )
   {
      fclose( ptrOldFile );
      unlink( oldpath );
      return;
   }

   ptrNewFile = fopen( newpath, "a" );
   if ( !ptrNewFile )
   {
      fclose( ptrOldFile );
      return;
   }

   {
      char aryCopyBuffer[BUFSIZ];
      size_t bytesRead;

      while ( ( bytesRead = fread( aryCopyBuffer, 1, sizeof( aryCopyBuffer ), ptrOldFile ) ) > 0 )
      {
         if ( fwrite( aryCopyBuffer, 1, bytesRead, ptrNewFile ) != bytesRead )
         {
            break;
         }
      }
   }

   fclose( ptrOldFile );
   fclose( ptrNewFile );
   unlink( oldpath );
   return;
}

/// @brief Install the signal handlers used during normal client runtime.
///
/// @return This function does not return a value.
void sigInit( void )
{
   oldRows = -1;

   signal( SIGINT, SIG_IGN );
   signal( SIGQUIT, SIG_IGN );
   signal( SIGPIPE, SIG_IGN );
#ifdef SIGTSTP
   signal( SIGTSTP, SIG_IGN );
#endif
#ifdef SIGTTOU
   signal( SIGTTOU, SIG_IGN );
#endif
   signal( SIGHUP, bye );
   signal( SIGTERM, bye );
#ifdef SIGWINCH
   signal( SIGWINCH, naws );
#endif
}

/// @brief Disable the runtime signal handlers during shutdown.
///
/// @return This function does not return a value.
void sigOff( void )
{
   signal( SIGALRM, SIG_IGN );
#ifdef SIGWINCH
   signal( SIGWINCH, SIG_IGN );
#endif
   signal( SIGHUP, SIG_IGN );
   signal( SIGTERM, SIG_IGN );
}

/// @brief Truncate the `.bbsrc` file to a given length.
///
/// @param userNameLength New file length.
///
/// @return This function does not return a value.
void truncateBbsRc( long userNameLength )
{
   if ( ftruncate( fileno( ptrBbsRc ), userNameLength ) < 0 )
   {
      fatalExit( "ftruncate", "Local error" );
   }
}
