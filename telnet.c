/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "browser.h"
#include "client.h"
#include "client_globals.h"
#include "config_menu.h"
#include "defs.h"
#include "edit.h"
#include "filter.h"
#include "filter_globals.h"
#include "getline_input.h"
#include "network_globals.h"
#include "telnet.h"
#include "utility.h"
static int handleTelnetDataState( int inputByte, int *ptrState );
static int handleTelnetGetCommand( unsigned char *aryTelnetBuffer );
static int handleTelnetGetState( int inputByte, int *ptrState,
                                 unsigned char *aryTelnetBuffer,
                                 int *ptrTelnetBufferPos );
static int handleTelnetIacState( int inputByte, int *ptrState,
                                 unsigned char *aryTelnetBuffer,
                                 int *ptrTelnetBufferPos );
static int handleTelnetVoidState( int inputByte, int *ptrState );
static void markPromptTriggerNonReplayable( unsigned char commandType );
static void markReplayByteNonReplayable( long replayPosition );

/// @brief Handle normal telnet data bytes outside IAC command parsing.
///
/// @param inputByte Incoming protocol byte.
/// @param ptrState Current telnet parser state.
///
/// @return `0` after dispatching the byte.
static int handleTelnetDataState( int inputByte, int *ptrState )
{
   if ( inputByte == IAC )
   {
      *ptrState = TS_IAC;
      return 0;
   }
   if ( whoListProgress )
   {
      filterWhoList( inputByte );
   }
   else if ( isExpressMessageInProgress )
   {
      filterExpress( inputByte );
   }
   else if ( postProgressState )
   {
      filterPost( inputByte );
   }
   else
   {
      filterData( inputByte );
   }

   return 0;
}

/// @brief Execute a completed telnet GET command from the protocol buffer.
///
/// @param aryTelnetBuffer Completed GET command buffer.
///
/// @return `0` on success, or `-1` when the caller should abort the current path.
static int handleTelnetGetCommand( unsigned char *aryTelnetBuffer )
{
   int outputIndex;
   const char *ptrInputString;

   switch ( *aryTelnetBuffer )
   {
      case G_POST:
         if ( flagsConfiguration.isPosting )
         {
            flagsConfiguration.shouldCheckExpress = 0;
            return -1;
         }
         makeMessage( aryTelnetBuffer[1] );
         break;

      case G_FIVE:
         getFiveLines( aryTelnetBuffer[1] );
         if ( aryTelnetBuffer[1] == 1 && xlandQueue->itemCount > 0 )
         {
            sendAnX();
         }
         break;

      case G_NAME:
         sendBlock();
         ptrInputString = getName( aryTelnetBuffer[1] );
         outputIndex = (int)strlen( ptrInputString );
         sendTrackedBuffer( ptrInputString, (size_t)outputIndex );
         if ( *ptrInputString != CTRL_D )
         {
            sendTrackedNewline();
         }
         break;

      case G_STR:
         sendBlock();
         getString( aryTelnetBuffer[1], (char *)aryTelnetBuffer, -1 );
         outputIndex = (int)strlen( (char *)aryTelnetBuffer );
         sendTrackedBuffer( (char *)aryTelnetBuffer, (size_t)outputIndex );
         sendTrackedNewline();
         break;

      case CONFIG:
         sendBlock();
         configClient();
         sendTrackedNewline();
         break;
   }

   return 0;
}

/// @brief Collect and finish a multi-byte telnet GET command.
///
/// @param inputByte Incoming protocol byte.
/// @param ptrState Current telnet parser state.
/// @param aryTelnetBuffer Shared telnet command buffer.
/// @param ptrTelnetBufferPos Current write position in the command buffer.
///
/// @return `0` on success, or the result from the completed GET command.
static int handleTelnetGetState( int inputByte, int *ptrState,
                                 unsigned char *aryTelnetBuffer,
                                 int *ptrTelnetBufferPos )
{
   aryTelnetBuffer[( *ptrTelnetBufferPos )++] = (unsigned char)inputByte;
   if ( *ptrTelnetBufferPos != 5 )
   {
      return 0;
   }

   targetByte = byte;
   bytePosition = ( (long)aryTelnetBuffer[2] << 16 ) +
                  ( (long)aryTelnetBuffer[3] << 8 ) +
                  aryTelnetBuffer[4];
   byte = bytePosition;
   markPromptTriggerNonReplayable( aryTelnetBuffer[0] );

   if ( byte < targetByte - (int)( sizeof arySavedBytes ) - 1 )
   {
      stdPrintf( "\r\n[Error:  characters lost during transmission]\r\n" );
   }

   *ptrState = TS_DATA;
   *ptrTelnetBufferPos = 0;
   return handleTelnetGetCommand( aryTelnetBuffer );
}

/// @brief Handle an IAC-prefixed telnet command byte.
///
/// @param inputByte Incoming protocol byte.
/// @param ptrState Current telnet parser state.
/// @param aryTelnetBuffer Shared telnet command buffer.
/// @param ptrTelnetBufferPos Current write position in the command buffer.
///
/// @return `0` after handling the command.
static int handleTelnetIacState( int inputByte, int *ptrState,
                                 unsigned char *aryTelnetBuffer,
                                 int *ptrTelnetBufferPos )
{
   switch ( inputByte )
   {
      case CLIENT:
         *ptrState = TS_DATA;
         netPutChar( IAC );
         netPutChar( CLIENT );
         break;

      case S_WHO:
         *ptrState = TS_DATA;
         whoListProgress = 1;
         break;

      case G_POST:
      case G_FIVE:
      case G_NAME:
      case G_STR:
      case CONFIG:
         *ptrState = TS_GET;
         aryTelnetBuffer[( *ptrTelnetBufferPos )++] = (unsigned char)inputByte;
         break;

      case START:
         *ptrState = TS_DATA;
         byte = 1;
         netPutChar( IAC );
         netPutChar( START3 );
         break;

      case POST_S:
         *ptrState = TS_DATA;
         postHeaderActive = 1;
         postProgressState = 1;
         filterPost( -1 );
         break;

      case POST_E:
         *ptrState = TS_DATA;
         postHeaderActive = 0;
         ptrPostBuffer = 0;
         postProgressState = 0;
         isPostJustEnded = 1;
         filterPost( -1 );
         break;

      case MORE_M:
         *ptrState = TS_DATA;
         flagsConfiguration.isMorePromptActive ^= 1;
         if ( !flagsConfiguration.isMorePromptActive &&
              flagsConfiguration.shouldUseAnsi )
         {
            morePromptHelper();
         }
         break;

      case XMSG_S:
         *ptrState = TS_DATA;
         *aryExpressParsing = 0;
         isExpressMessageHeaderActive = 1;
         isExpressMessageInProgress = 1;
         filterExpress( -1 );
         break;

      case XMSG_E:
         *ptrState = TS_DATA;
         *aryExpressParsing = 0;
         isExpressMessageHeaderActive = 0;
         isExpressMessageInProgress = 0;
         ptrExpressMessageBuffer = aryExpressMessageBuffer;
         filterExpress( -1 );
         if ( shouldSendExpressMessage )
         {
            sendAnX();
            shouldSendExpressMessage = 0;
         }
         break;

      case DO:
      case DONT:
      case WILL:
      case WONT:
         *ptrState = TS_VOID;
         break;

      default:
         *ptrState = TS_DATA;
         break;
   }

   return 0;
}

/// @brief Reject a telnet option negotiation that is not supported.
///
/// @param inputByte Incoming telnet option byte.
/// @param ptrState Current telnet parser state.
///
/// @return `0` after sending the refusal.
static int handleTelnetVoidState( int inputByte, int *ptrState )
{
   netPutChar( IAC );
   netPutChar( WONT );
   netPutChar( inputByte );
   *ptrState = TS_DATA;
   return 0;
}

/// @brief Tell the BBS that a client-side input block is about to follow.
///
/// @return This function does not return a value.
void sendBlock( void )
{
   netPutChar( IAC );
   netPutChar( BLOCK );
}

/// @brief Send an updated NAWS window-size record to the BBS.
///
/// @return This function does not return a value.
void sendNaws( void )
{
   if ( oldRows != getWindowSize() )
   {
      char aryString[10];
      register int outputIndex;

      // Old window max was 70
      if ( rows > NAWS_ROWS_MAX || rows < NAWS_ROWS_MIN )
      {
         rows = WINDOW_ROWS_DEFAULT;
      }
      else
      {
         oldRows = rows;
      }
      snprintf( aryString, sizeof( aryString ), "%c%c%c%c%c%c%c%c%c", IAC, SB, TELOPT_NAWS, 0, 0, 0, rows, IAC, SE );
      for ( outputIndex = 0; outputIndex < 9; outputIndex++ )
      {
         netPutChar( aryString[outputIndex] );
      }
   }
}

/// @brief Mark command bytes that opened a BBS input request as non-replayable.
///
/// @param commandType BBS GET command being handled.
///
/// @return This helper does not return a value.
static void markPromptTriggerNonReplayable( unsigned char commandType )
{
   long replayPosition;

   if ( commandType == G_NAME || commandType == G_STR )
   {
      if ( lastInteractiveInputByte >= bytePosition &&
           lastInteractiveInputByte < targetByte )
      {
         markReplayByteNonReplayable( lastInteractiveInputByte );
      }
      return;
   }
   if ( commandType != G_POST )
   {
      return;
   }

   for ( replayPosition = bytePosition; replayPosition < targetByte; replayPosition++ )
   {
      if ( arySavedByteCanReplay[(size_t)( replayPosition % (long)sizeof arySavedBytes )] )
      {
         markReplayByteNonReplayable( replayPosition );
      }
   }
}

/// @brief Mark one saved replay byte position as non-replayable.
///
/// @param replayPosition Saved-byte position to update.
///
/// @return This helper does not return a value.
static void markReplayByteNonReplayable( long replayPosition )
{
   arySavedByteCanReplay[(size_t)( replayPosition % (long)sizeof arySavedBytes )] = false;
}

/// @brief Send the initial telnet and client-identification sequence to the BBS.
///
/// @return This function does not return a value.
void telInit( void )
{
   netPutChar( IAC );
   netPutChar( CLIENT2 );
   netPutChar( IAC );
   netPutChar( SB );
   netPutChar( TELOPT_ENVIRON );
   netPutChar( 0 );
   netPutChar( 1 );
   netPutChar( 0 );
   netPuts( "USER" );
   netPutChar( 0 );
   netPuts( aryUser );
   netPutChar( IAC );
   netPutChar( SE );
   sendNaws();
}

/// @brief Feed one incoming byte through the telnet protocol state machine.
///
/// @param inputByte Incoming protocol byte.
///
/// @return `0` on success, or a negative value if the caller should abort.
int telReceive( int inputByte )
{
   static int state = TS_DATA;               // Current state of telnet state machine
   static unsigned char aryTelnetBuffer[80]; // Generic buffer
   static int telnetBufferPos = 0;           // Pointer into generic buffer

   switch ( state )
   {
      case TS_DATA:
         return handleTelnetDataState( inputByte, &state );

      case TS_IAC:
         return handleTelnetIacState( inputByte, &state, aryTelnetBuffer,
                                      &telnetBufferPos );

      case TS_GET:
         return handleTelnetGetState( inputByte, &state, aryTelnetBuffer,
                                      &telnetBufferPos );

      case TS_VOID:
         return handleTelnetVoidState( inputByte, &state );

      default:
         state = TS_DATA;
         break;
   }
   return ( 0 );
}
