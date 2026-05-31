/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "browser.h"
#include "client.h"
#include "client_globals.h"
#include "config_globals.h"
#include "config_menu.h"
#include "defs.h"
#include "edit.h"
#include "network_globals.h"
#include "pane_ui.h"
#include "sysio.h"
#include "telnet.h"
#include "utility.h"
static int lastCarriageReturn = 0;

typedef enum
{
   GETKEY_RESULT_NONE = 0,
   GETKEY_RESULT_CONTINUE,
   GETKEY_RESULT_RETURN
} GetKeyResultKind;

typedef struct
{
   GetKeyResultKind kind;
   int inputChar;
} GetKeyResult;

static const char *currentConnectionHost( void );
static noreturn void failNetworkRead( int readErrno );
static void flushPendingOutput( void );
static GetKeyResult handleBufferedLocalInput( int *ptrIsCommandNext,
                                              int *ptrWasUndefinedCommand );
static GetKeyResult handleCommandKeyInput( int inputChar,
                                           int *ptrIsCommandNext,
                                           int *ptrWasUndefinedCommand );
static GetKeyResult handleWaitEvent( void );
static bool isLocalInputDescriptorReady( void );
static bool tryBufferReadyLocalInput( void );
static bool tryReplaySavedByte( int *ptrInputChar );

/// @brief Resolve the current host name used for network error reporting.
///
/// @return Host name string for the active connection target.
static const char *currentConnectionHost( void )
{
   if ( *aryCommandLineHost )
   {
      return aryCommandLineHost;
   }
   if ( *aryBbsHost )
   {
      return aryBbsHost;
   }

   return BBS_HOSTNAME;
}

/// @brief Abort with a detailed network read error message.
///
/// @param readErrno Saved `errno` value from the failed read.
///
/// @return This helper does not return.
static noreturn void failNetworkRead( int readErrno )
{
   char aryMessage[256];
   char aryPortString[8];
   const char *ptrHost;
   const char *ptrReason;
   unsigned short currentPort;

   ptrHost = currentConnectionHost();
   currentPort = cmdLinePort ? cmdLinePort : ( bbsPort ? bbsPort : BBS_PORT_NUMBER );
   snprintf( aryPortString, sizeof( aryPortString ), "%u", (unsigned int)currentPort );
   ptrReason = strerror( readErrno );

   switch ( readErrno )
   {
#ifdef ECONNRESET
      case ECONNRESET:
         snprintf( aryMessage, sizeof( aryMessage ),
                   "The connection to %s:%s was reset by the server (%s).",
                   ptrHost, aryPortString, ptrReason );
         break;
#endif
#ifdef ETIMEDOUT
      case ETIMEDOUT:
         snprintf( aryMessage, sizeof( aryMessage ),
                   "The connection to %s:%s timed out while waiting for data (%s).",
                   ptrHost, aryPortString, ptrReason );
         break;
#endif
#ifdef EHOSTUNREACH
      case EHOSTUNREACH:
         snprintf( aryMessage, sizeof( aryMessage ),
                   "The host %s:%s became unreachable while reading from the connection (%s).",
                   ptrHost, aryPortString, ptrReason );
         break;
#endif
#ifdef ENETUNREACH
      case ENETUNREACH:
         snprintf( aryMessage, sizeof( aryMessage ),
                   "The network path to %s:%s became unreachable while reading from the connection (%s).",
                   ptrHost, aryPortString, ptrReason );
         break;
#endif
#ifdef ENETDOWN
      case ENETDOWN:
         snprintf( aryMessage, sizeof( aryMessage ),
                   "The local network went down while reading from %s:%s (%s).",
                   ptrHost, aryPortString, ptrReason );
         break;
#endif
      default:
         snprintf( aryMessage, sizeof( aryMessage ),
                   "Reading from %s:%s failed (%s).",
                   ptrHost, aryPortString, ptrReason );
         break;
   }

   fatalExit( aryMessage, "Network error" );
}

/// @brief Flush pending network and terminal output before waiting for input.
///
/// @return This helper does not return a value.
static void flushPendingOutput( void )
{
   if ( netflush() < 0 )
   {
      stdPrintf( "\r\n" );
      fatalPerror( "send", "Network error" );
   }
   if ( fflush( stdout ) < 0 )
   {
      stdPrintf( "\r\n" );
      fatalPerror( "write", "Local error" );
   }
}

/// @brief Retrieve the next raw key for the client.
///
/// @return Next raw input byte, or `-1` if the connection closed.
int getKey( void )
{
   int inputChar = -1;
   static int isCommandNext = 0;
   static int wasUndefinedCommand = 0;

   // While a child process is running, standard input is ignored and only
   // network traffic is processed. The same applies when express-message
   // checks are pending so editor entry remains compatible with the BBS.

   while ( true )
   {
      GetKeyResult result;

      if ( tryReplaySavedByte( &inputChar ) )
      {
         return inputChar;
      }

      result = handleBufferedLocalInput( &isCommandNext, &wasUndefinedCommand );
      if ( result.kind == GETKEY_RESULT_RETURN )
      {
         return result.inputChar;
      }
      if ( result.kind == GETKEY_RESULT_CONTINUE )
      {
         continue;
      }

      // Handle any incoming traffic in the network input buffer
      if ( isNetworkInputAvailable() )
      {
         while ( isNetworkInputAvailable() )
         {
            if ( telReceive( netget() ) < 0 )
            {
               return ( -1 );
            }
            if ( isPtyInputAvailable() )
            {
               break;
            }
            if ( isLocalInputDescriptorReady() )
            {
               flushPendingOutput();
               result = handleWaitEvent();
               if ( result.kind == GETKEY_RESULT_RETURN )
               {
                  return result.inputChar;
               }
               break;
            }
         }
         continue;
      }

      flushPendingOutput();
      result = handleWaitEvent();
      if ( result.kind == GETKEY_RESULT_RETURN )
      {
         return result.inputChar;
      }
   }
}

/// @brief Handle already-buffered local terminal input.
///
/// @param ptrIsCommandNext Tracks whether the next byte is a command selector.
/// @param ptrWasUndefinedCommand Tracks whether the undefined-command banner is active.
///
/// @return Result describing whether input was consumed, should continue, or should return.
static GetKeyResult handleBufferedLocalInput( int *ptrIsCommandNext,
                                              int *ptrWasUndefinedCommand )
{
   GetKeyResult result;

   result.kind = GETKEY_RESULT_NONE;
   result.inputChar = -1;

   while ( ( paneUiHasPendingLocalInput() || isPtyInputAvailable() ) && !childPid &&
           !flagsConfiguration.shouldCheckExpress )
   {
      int inputChar;

      if ( !paneUiTakePendingLocalInput( &inputChar ) )
      {
         inputChar = ptyget() & 0x7f;
         if ( paneUiHandleLocalInput( inputChar, isPtyInputAvailable() ) )
         {
            result.kind = GETKEY_RESULT_CONTINUE;
            return result;
         }
      }
      paneUiNoteUserInput();
      if ( inputChar > 0 && isAway )
      {
         isAway = false;
         stdPrintf( "\r\n[No longer away]\r\n" );
      }

      if ( *ptrIsCommandNext )
      {
         return handleCommandKeyInput( inputChar, ptrIsCommandNext,
                                       ptrWasUndefinedCommand );
      }

      if ( *ptrWasUndefinedCommand )
      {
         *ptrWasUndefinedCommand = 0;
         printf( "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b                   \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b" );
      }
      if ( inputChar == commandKey && !flagsConfiguration.isConfigMode )
      {
         *ptrIsCommandNext = 1;
         printf( "[Command]" );
         result.kind = GETKEY_RESULT_CONTINUE;
         return result;
      }

      result.kind = GETKEY_RESULT_RETURN;
      result.inputChar = inputChar;
      return result;
   }

   return result;
}

/// @brief Handle a command-key sequence.
///
/// @param inputChar Command character that followed the command key.
/// @param ptrIsCommandNext Tracks whether the next byte is a command selector.
/// @param ptrWasUndefinedCommand Tracks whether the undefined-command banner is active.
///
/// @return Result describing whether input was consumed, should continue, or should return.
static GetKeyResult handleCommandKeyInput( int inputChar,
                                           int *ptrIsCommandNext,
                                           int *ptrWasUndefinedCommand )
{
   GetKeyResult result;

   result.kind = GETKEY_RESULT_CONTINUE;
   result.inputChar = -1;

   printf( "\b\b\b\b\b\b\b\b\b         \b\b\b\b\b\b\b\b\b" );
   *ptrIsCommandNext = 0;

   if ( inputChar == awayKey )
   {
      isAway = !isAway;
      stdPrintf( "\r\n[%s away]\r\n", ( isAway ) ? "Now" : "No longer" );
      return result;
   }
   if ( inputChar == quitKey )
   {
      stdPrintf( "\r\n[Quitting]\r\n" );
      myExit();
   }
   if ( inputChar == suspKey )
   {
      if ( !isLoginShell )
      {
         printf( "\r\n[Suspended]\r\n" );
         fflush( stdout );
         suspend();
      }
      return result;
   }
   if ( inputChar == shellKey )
   {
      if ( !isLoginShell )
      {
         printf( "\r\n[New shell]\r\n" );
         run( aryShell, 0 );
         printf( "\r\n[Continue]\r\n" );
      }
      return result;
   }
   if ( inputChar == browserKey && !isLoginShell )
   {
      openBrowser();
      return result;
   }
   if ( inputChar == captureKey )
   {
      if ( capture < 0 || flagsConfiguration.isPosting )
      {
         printf( "[ Cannot capture! ]" );
         *ptrWasUndefinedCommand = 1;
         return result;
      }
      capture ^= 1;
      printf( "\r\n[Capture to temp file turned O%s]\r\n", capture ? "N" : "FF" );
      if ( capture )
      {
         rewind( tempFile );
         if ( flagsConfiguration.isLastSave )
         {
            if ( !( tempFile = freopen( aryTempFileName, "w+", tempFile ) ) )
            {
               fatalPerror( "toggle capture: reopen temp file for truncate",
                            "Capture file error" );
            }
            flagsConfiguration.isLastSave = 0;
         }
         else if ( getc( tempFile ) >= 0 )
         {
            printf( "There is text in your edit file.  Do you wish to erase it? (Y/N) -> " );
            capture = -1;
            if ( yesNo() )
            {
               if ( !( tempFile = freopen( aryTempFileName, "w+", tempFile ) ) )
               {
                  fatalPerror( "capture prompt: reopen temp file for truncate",
                               "Capture file error" );
               }
            }
            else
            {
               fseek( tempFile, 0L, SEEK_END );
            }
            capture = 1;
         }
         flagsConfiguration.isLastSave = 0;
      }
      else
      {
         fflush( tempFile );
      }
      return result;
   }
   printf( "[Undefined command]" );
   *ptrWasUndefinedCommand = 1;
   return result;
}

/// @brief Wait for the next terminal or network event when no buffered input exists.
///
/// @return Result describing whether input was consumed, should continue, or should return.
static GetKeyResult handleWaitEvent( void )
{
   int eventResult;
   GetKeyResult result;

   result.kind = GETKEY_RESULT_NONE;
   result.inputChar = -1;

   eventResult = waitNextEvent();
   if ( eventResult & 1 )
   {
      if ( !tryBufferReadyLocalInput() )
      {
         stdPrintf( "\r\n" );
         fatalPerror( "read", "Local error" );
      }
      result.kind = GETKEY_RESULT_CONTINUE;
      return result;
   }
   if ( eventResult & 2 )
   {
      int inputChar;

      errno = 0;
      inputChar = netget();
      if ( inputChar < 0 )
      {
         if ( errno )
         {
            int readErrno = errno;

            stdPrintf( "\r\n" );
            failNetworkRead( readErrno );
         }
         if ( childPid )
         {
            stdPrintf( "\r\n\n\007[DISCONNECTED]\r\n\n\007" );
         }
         else
         {
            stdPrintf( "\r\n[Disconnected]\r\n" );
         }
         myExit();
      }
      if ( telReceive( inputChar ) < 0 )
      {
         result.kind = GETKEY_RESULT_RETURN;
         result.inputChar = -1;
         return result;
      }
   }

   return result;
}

/// @brief Read one ready local byte into the PTY buffer without consuming it.
///
/// When `waitNextEvent()` reports local input, the byte still needs to be read
/// from standard input before the normal buffered-input path can process it.
///
/// @return `true` when a byte was buffered successfully, otherwise `false`.
static bool tryBufferReadyLocalInput( void )
{
   int inputChar;

   inputChar = ptyget();
   if ( inputChar < 0 )
   {
      return false;
   }

   ptrPtyInput--;
   return true;
}

/// @brief Return the next normalized user input key.
///
/// Carriage returns, delete, and `CTRL_U` are translated into the common local
/// editing equivalents expected by the rest of the client.
///
/// @return Next normalized input byte, or `-1` if the connection closed.
int inKey( void )
{
   register int inputChar;

   while ( true )
   {
      inputChar = getKey();
      if ( !lastCarriageReturn || inputChar != '\n' )
      {
         break;
      }
      lastCarriageReturn = 0;
   }
   lastCarriageReturn = 0;
   if ( inputChar == '\r' )
   {
      inputChar = '\n';
      lastCarriageReturn = 1;
   }
   else if ( inputChar == 127 )
   {
      inputChar = '\b';
   }
   else if ( inputChar == CTRL_U )
   {
      inputChar = CTRL_X;
   }
   return ( inputChar );
}

/// @brief Check whether unread local terminal input is waiting on standard input.
///
/// @return `true` if standard input is readable without blocking, otherwise `false`.
static bool isLocalInputDescriptorReady( void )
{
   fd_set fdr;
   bool isReady;
   struct timeval timeout;

   if ( childPid || flagsConfiguration.shouldCheckExpress )
   {
      return false;
   }

   FD_ZERO( &fdr );
   FD_SET( 0, &fdr );
   timeout.tv_sec = 0;
   timeout.tv_usec = 0;

   isReady = select( 1, &fdr, 0, 0, &timeout ) > 0 && FD_ISSET( 0, &fdr );
   return isReady;
}

/// @brief Replay a saved byte from local buffering when protocol state requires it.
///
/// @param ptrInputChar Receives the replayed byte.
///
/// @return `true` if a saved byte was replayed, otherwise `false`.
static bool tryReplaySavedByte( int *ptrInputChar )
{
   if ( !targetByte || childPid || flagsConfiguration.shouldCheckExpress )
   {
      return false;
   }
   while ( bytePosition < targetByte )
   {
      size_t index = (size_t)( bytePosition % (long)sizeof arySavedBytes );

      if ( !arySavedByteCanReplay[index] )
      {
         bytePosition++;
         byte++;
         continue;
      }

      lastCarriageReturn = 0;
      *ptrInputChar = arySavedBytes[index];
      bytePosition++;
      return true;
   }

   if ( bytePosition > targetByte )
   {
      stdPrintf( "[Internal error: byte synch lost!  %ld > %ld]\r\n",
                 bytePosition, targetByte );
      exit( 1 );
   }

   targetByte = 0;
   return false;
}
