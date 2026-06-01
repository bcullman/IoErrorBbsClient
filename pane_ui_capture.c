/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "config_globals.h"
#include "network_globals.h"
#include "pane_ui_internal.h"
#include "utility.h"

static void appendCaptureChar( int inputChar );
static void appendCaptureText( const char *ptrText, size_t textLength );
static void appendObservedChar( int inputChar );
static void completeCapture( void );
static void discardCurrentCaptureLine( void );
static bool isCapturedSgrSequence( void );
static bool isForumInfoReturnPrompt( const char *ptrLine );
static bool isSafePromptLine( const char *ptrLine );
static void removeMorePromptMarkers( char *ptrLine );
static void sendHiddenSidebarChar( int inputChar );
static bool shouldSwallowPromptRemainderChar( int inputChar );
static void updateCurrentForumName( const char *ptrPrompt );

void paneUiResetObservedLine( void )
{
   paneUi.observedLineLength = 0;
   paneUi.observedSkippingAnsi = false;
   paneUi.aryObservedLine[0] = '\0';
}

static void appendObservedChar( int inputChar )
{
   if ( inputChar == '\033' )
   {
      paneUi.observedSkippingAnsi = true;
      paneUi.observedAnsiBytesRemaining = 8;
      return;
   }
   if ( paneUi.observedSkippingAnsi )
   {
      if ( isalpha( inputChar ) || --paneUi.observedAnsiBytesRemaining <= 0 )
      {
         paneUi.observedSkippingAnsi = false;
      }
      return;
   }
   if ( inputChar == '\r' || inputChar == '\n' )
   {
      paneUiResetObservedLine();
      return;
   }
   if ( inputChar < ASCII_PRINTABLE_MIN || inputChar >= ASCII_PRINTABLE_MAX )
   {
      return;
   }
   if ( paneUi.observedLineLength + 1 >= sizeof( paneUi.aryObservedLine ) )
   {
      paneUiResetObservedLine();
   }
   paneUi.aryObservedLine[paneUi.observedLineLength++] = (char)inputChar;
   paneUi.aryObservedLine[paneUi.observedLineLength] = '\0';
}

static bool isSafePromptLine( const char *ptrLine )
{
   const char *ptrCursor;
   const char *ptrEnd;
   size_t lineLength;
   const char *ptrReadCommandPrompt;
   size_t readCommandPromptLength;

   ptrEnd = ptrLine + strlen( ptrLine );
   while ( ptrEnd > ptrLine && ptrEnd[-1] == ' ' )
   {
      ptrEnd--;
   }
   lineLength = (size_t)( ptrEnd - ptrLine );
   readCommandPromptLength = strlen( "Read cmd ->" );
   if ( lineLength >= readCommandPromptLength )
   {
      ptrReadCommandPrompt = ptrEnd - readCommandPromptLength;
   }
   else
   {
      ptrReadCommandPrompt = ptrEnd;
   }
   if ( lineLength >= readCommandPromptLength &&
        strncmp( ptrReadCommandPrompt, "Read cmd ->", readCommandPromptLength ) ==
           0 &&
        ( ptrReadCommandPrompt == ptrLine ||
          ( ptrReadCommandPrompt >= ptrLine + 2 &&
            ptrReadCommandPrompt[-2] == ']' &&
            ptrReadCommandPrompt[-1] == ' ' ) ) )
   {
      return true;
   }
   if ( ptrEnd == ptrLine || ptrEnd[-1] != '>' )
   {
      return false;
   }
   if ( ptrEnd - ptrLine >= 2 && ptrEnd[-2] == '-' )
   {
      return false;
   }

   for ( ptrCursor = ptrLine; ptrCursor < ptrEnd - 1; ptrCursor++ )
   {
      if ( !isalnum( (unsigned char)*ptrCursor ) && *ptrCursor != ' ' &&
           *ptrCursor != '\'' && *ptrCursor != '&' && *ptrCursor != '-' &&
           *ptrCursor != '_' )
      {
         return false;
      }
   }
   return ptrCursor > ptrLine;
}

static bool isForumInfoReturnPrompt( const char *ptrLine )
{
   const char *ptrEnd;
   size_t forumNameLength;

   forumNameLength = strlen( paneUi.aryCaptureForumName );
   if ( forumNameLength == 0 )
   {
      return false;
   }

   ptrEnd = ptrLine + strlen( ptrLine );
   while ( ptrEnd > ptrLine && ptrEnd[-1] == ' ' )
   {
      ptrEnd--;
   }
   if ( (size_t)( ptrEnd - ptrLine ) == forumNameLength + 1 &&
        strncmp( ptrLine, paneUi.aryCaptureForumName, forumNameLength ) == 0 &&
        ptrLine[forumNameLength] == '>' )
   {
      return true;
   }
   return ptrLine[0] == '[' &&
          strncmp( ptrLine + 1, paneUi.aryCaptureForumName, forumNameLength ) == 0 &&
          ptrLine[forumNameLength + 1] == '>' &&
          strstr( ptrLine + forumNameLength + 2, "] Read cmd ->" ) != NULL;
}

static void updateCurrentForumName( const char *ptrPrompt )
{
   const char *ptrEnd;
   const char *ptrStart;
   size_t nameLength;

   ptrEnd = strchr( ptrPrompt, '>' );
   if ( ptrEnd == NULL )
   {
      return;
   }
   ptrStart = ptrPrompt[0] == '[' ? ptrPrompt + 1 : ptrPrompt;
   if ( ptrEnd <= ptrStart )
   {
      return;
   }

   nameLength = (size_t)( ptrEnd - ptrStart );
   if ( nameLength >= sizeof( paneUi.aryCurrentForumName ) )
   {
      nameLength = sizeof( paneUi.aryCurrentForumName ) - 1;
   }
   memcpy( paneUi.aryCurrentForumName, ptrStart, nameLength );
   paneUi.aryCurrentForumName[nameLength] = '\0';
}

static bool shouldSwallowPromptRemainderChar( int inputChar )
{
   if ( !paneUi.swallowingPromptRemainder )
   {
      return false;
   }
   if ( paneUi.promptRemainderSkippingAnsi )
   {
      if ( isalpha( inputChar ) ||
           --paneUi.promptRemainderAnsiBytesRemaining <= 0 )
      {
         paneUi.promptRemainderSkippingAnsi = false;
      }
      return true;
   }
   if ( inputChar == '\033' )
   {
      paneUi.promptRemainderSkippingAnsi = true;
      paneUi.promptRemainderAnsiBytesRemaining = 8;
      return true;
   }
   if ( inputChar == ' ' )
   {
      return true;
   }

   paneUi.swallowingPromptRemainder = false;
   return false;
}

static bool isCapturedSgrSequence( void )
{
   size_t charIndex;

   if ( paneUi.captureAnsiLength < 3 ||
        paneUi.aryCaptureAnsi[1] != '[' ||
        paneUi.aryCaptureAnsi[paneUi.captureAnsiLength - 1] != 'm' )
   {
      return false;
   }
   for ( charIndex = 2; charIndex + 1 < paneUi.captureAnsiLength; charIndex++ )
   {
      if ( !isdigit( (unsigned char)paneUi.aryCaptureAnsi[charIndex] ) &&
           paneUi.aryCaptureAnsi[charIndex] != ';' )
      {
         return false;
      }
   }
   return true;
}

static void appendCaptureText( const char *ptrText, size_t textLength )
{
   char *ptrLine;
   size_t availableLength;

   ptrLine = paneUi.aryCaptureLines[paneUi.captureLineCount];
   availableLength = PANE_UI_MAX_LINE_LENGTH - paneUi.captureLineLength - 1;
   if ( textLength > availableLength )
   {
      textLength = availableLength;
   }
   memcpy( ptrLine + paneUi.captureLineLength, ptrText, textLength );
   paneUi.captureLineLength += textLength;
   ptrLine[paneUi.captureLineLength] = '\0';
}

static void appendCaptureChar( int inputChar )
{
   paneUi.captureStarted = time( NULL );
   if ( inputChar == '\r' )
   {
      paneUi.captureCarriageReturnPending = true;
      return;
   }
   if ( inputChar == '\n' )
   {
      paneUi.captureCarriageReturnPending = false;
      if ( paneUi.captureLineCount < PANE_UI_MAX_LINES )
      {
         paneUi.captureLineCount++;
      }
      else
      {
         paneUi.captureTruncated = true;
      }
      paneUi.captureLineLength = 0;
      paneUi.aryCaptureLines[paneUi.captureLineCount][0] = '\0';
      return;
   }
   if ( paneUi.captureCarriageReturnPending )
   {
      paneUi.captureCarriageReturnPending = false;
      paneUi.captureLineLength = 0;
      paneUi.aryCaptureLines[paneUi.captureLineCount][0] = '\0';
   }
   if ( inputChar == '\b' )
   {
      if ( paneUi.captureLineLength > 0 )
      {
         paneUi.aryCaptureLines[paneUi.captureLineCount]
                               [--paneUi.captureLineLength] = '\0';
      }
      return;
   }
   if ( inputChar == '\033' )
   {
      paneUi.skippingAnsi = true;
      paneUi.ansiBytesRemaining = (int)sizeof( paneUi.aryCaptureAnsi ) - 1;
      paneUi.captureAnsiLength = 0;
      paneUi.aryCaptureAnsi[paneUi.captureAnsiLength++] = (char)inputChar;
      return;
   }
   if ( paneUi.skippingAnsi )
   {
      if ( paneUi.captureAnsiLength + 1 < sizeof( paneUi.aryCaptureAnsi ) )
      {
         paneUi.aryCaptureAnsi[paneUi.captureAnsiLength++] = (char)inputChar;
      }
      if ( inputChar == 'm' )
      {
         if ( isCapturedSgrSequence() )
         {
            appendCaptureText( paneUi.aryCaptureAnsi,
                               paneUi.captureAnsiLength );
         }
         paneUi.skippingAnsi = false;
      }
      else if ( isalpha( inputChar ) || --paneUi.ansiBytesRemaining <= 0 )
      {
         paneUi.skippingAnsi = false;
      }
      return;
   }
   if ( paneUi.captureLineCount >= PANE_UI_MAX_LINES )
   {
      paneUi.captureTruncated = true;
      return;
   }
   if ( inputChar < ASCII_PRINTABLE_MIN || inputChar >= ASCII_PRINTABLE_MAX )
   {
      return;
   }
   {
      char textChar;

      textChar = (char)inputChar;
      appendCaptureText( &textChar, 1 );
   }
}

static void discardCurrentCaptureLine( void )
{
   paneUi.captureCarriageReturnPending = false;
   paneUi.captureForumPromptPending = false;
   paneUi.captureLineLength = 0;
   paneUi.aryCaptureLines[paneUi.captureLineCount][0] = '\0';
}

static void removeMorePromptMarkers( char *ptrLine )
{
   char *ptrMarker;

   while ( ( ptrMarker = strstr( ptrLine, "--MORE--(" ) ) != NULL )
   {
      char *ptrEnd;
      const char *ptrPercentage;

      ptrEnd = ptrMarker + strlen( "--MORE--(" );
      ptrPercentage = ptrEnd;
      while ( isdigit( (unsigned char)*ptrEnd ) )
      {
         ptrEnd++;
      }
      if ( ptrEnd == ptrPercentage || ptrEnd[0] != '%' || ptrEnd[1] != ')' )
      {
         ptrLine = ptrEnd;
         continue;
      }
      ptrEnd += 2;
      memmove( ptrMarker, ptrEnd, strlen( ptrEnd ) + 1 );
      ptrLine = ptrMarker;
   }
}

static void completeCapture( void )
{
   int firstLine;
   int lineIndex;
   int lineCount;
   int snapshotContentLineCount;
   int snapshotLineOffset;

   lineCount = paneUi.captureLineCount;
   for ( lineIndex = 0; lineIndex < lineCount; lineIndex++ )
   {
      removeMorePromptMarkers( paneUi.aryCaptureLines[lineIndex] );
   }
   while ( lineCount > 0 && paneUi.aryCaptureLines[lineCount - 1][0] == '\0' )
   {
      lineCount--;
   }
   firstLine = 0;
   while ( firstLine < lineCount && paneUi.aryCaptureLines[firstLine][0] == '\0' )
   {
      firstLine++;
   }
   snapshotLineOffset = paneUi.captureView == PANE_UI_VIEW_FORUM_INFO &&
                              paneUi.aryCaptureForumName[0] != '\0'
                           ? 1
                           : 0;
   snapshotContentLineCount = lineCount - firstLine;
   if ( snapshotContentLineCount > PANE_UI_MAX_LINES - snapshotLineOffset )
   {
      snapshotContentLineCount = PANE_UI_MAX_LINES - snapshotLineOffset;
      paneUi.captureTruncated = true;
   }
   paneUi.snapshotLineCount = snapshotContentLineCount + snapshotLineOffset;
   paneUi.snapshotTruncated = paneUi.captureTruncated;
   memset( paneUi.arySnapshotLines, 0, sizeof( paneUi.arySnapshotLines ) );
   if ( snapshotLineOffset )
   {
      snprintf( paneUi.arySnapshotLines[0], sizeof( paneUi.arySnapshotLines[0] ),
                "%s", paneUi.aryCaptureForumName );
   }
   memcpy( paneUi.arySnapshotLines + snapshotLineOffset,
           paneUi.aryCaptureLines + firstLine,
           (size_t)snapshotContentLineCount * sizeof( paneUi.arySnapshotLines[0] ) );
   paneUi.captureActive = false;
   paneUi.activeView = paneUi.captureView;
   paneUi.captureView = PANE_UI_VIEW_NONE;
   paneUi.captureCommand = '\0';
   flagsConfiguration.isMorePromptActive = false;
   paneUi.sidebarScrollOffset = 0;
   paneUi.snapshotRefreshedAt = paneUi.activeView == PANE_UI_VIEW_WHO
                                   ? time( NULL )
                                   : 0;
   paneUi.promptReady = true;
   paneUi.nextRefresh = paneUi.activeView == PANE_UI_VIEW_WHO
                           ? time( NULL ) + PANE_UI_REFRESH_SECONDS
                           : 0;
   paneUi.swallowingPromptRemainder = true;
   paneUiResetObservedLine();
   paneUiDrawSidebar();
}

bool paneUiHandleIncomingChar( int inputChar )
{
   if ( !paneUi.active )
   {
      return false;
   }
   if ( shouldSwallowPromptRemainderChar( inputChar ) )
   {
      return true;
   }

   appendObservedChar( inputChar );
   if ( paneUi.captureActive )
   {
      appendCaptureChar( inputChar );
      if ( paneUi.captureView == PANE_UI_VIEW_FORUM_INFO &&
           strstr( paneUi.aryCaptureLines[paneUi.captureLineCount],
                   "Forum moderator is " ) != NULL )
      {
         paneUi.captureHasForumInfoBody = true;
      }
      if ( isSafePromptLine( paneUi.aryObservedLine ) )
      {
         if ( paneUi.captureView == PANE_UI_VIEW_FORUM_INFO &&
              !paneUi.captureHasForumInfoBody )
         {
            discardCurrentCaptureLine();
            paneUiResetObservedLine();
            return true;
         }
         if ( paneUi.captureView == PANE_UI_VIEW_FORUM_INFO )
         {
            paneUi.captureForumPromptPending =
               isForumInfoReturnPrompt( paneUi.aryObservedLine );
            return true;
         }
         updateCurrentForumName( paneUi.aryObservedLine );
         completeCapture();
      }
      else
      {
         paneUi.captureForumPromptPending = false;
      }
      return true;
   }
   if ( isSafePromptLine( paneUi.aryObservedLine ) )
   {
      updateCurrentForumName( paneUi.aryObservedLine );
      paneUi.sessionReady = true;
      if ( !paneUi.sidebarVisible )
      {
         paneUi.sidebarVisible = true;
         paneUiDrawSidebar();
      }
      paneUi.promptReady = true;
      if ( paneUi.nextRefresh == 0 )
      {
         paneUi.nextRefresh = time( NULL );
      }
   }
   return false;
}

bool paneUiHandleCapturedIncomingChar( int inputChar )
{
   if ( !paneUi.captureActive )
   {
      return false;
   }

   return paneUiHandleIncomingChar( inputChar );
}

bool paneUiCanRefreshSidebar( void )
{
   return paneUi.active && paneUi.sidebarVisible && paneUi.promptReady &&
          !paneUi.captureActive &&
          !flagsConfiguration.isPosting &&
          !flagsConfiguration.isMorePromptActive &&
          !flagsConfiguration.shouldCheckExpress && !childPid;
}

static void sendHiddenSidebarChar( int inputChar )
{
   sendTrackedCharWithoutReplay( inputChar );
   if ( fflush( netOutputFile ) < 0 )
   {
      paneUi.captureActive = false;
   }
}

void paneUiStartCapture( time_t now, int command, PaneUiView view )
{
   memset( paneUi.aryCaptureLines, 0, sizeof( paneUi.aryCaptureLines ) );
   paneUi.captureLineCount = 0;
   paneUi.captureLineLength = 0;
   paneUi.captureAnsiLength = 0;
   paneUi.captureCarriageReturnPending = false;
   paneUi.captureForumPromptPending = false;
   paneUi.captureHasForumInfoBody = false;
   snprintf( paneUi.aryCaptureForumName, sizeof( paneUi.aryCaptureForumName ),
             "%s", paneUi.aryCurrentForumName );
   paneUi.captureStarted = now;
   paneUi.captureActive = true;
   paneUi.captureTruncated = false;
   paneUi.captureView = view;
   paneUi.captureCommand = (char)command;
   paneUi.promptReady = false;
   paneUi.skippingAnsi = false;
   paneUiResetObservedLine();
   sendHiddenSidebarChar( command );
}

void paneUiHandleMorePromptStateChanged( bool isActive )
{
   if ( !paneUi.captureActive || !isActive )
   {
      return;
   }

   paneUi.captureStarted = time( NULL );
   sendHiddenSidebarChar( ' ' );
}

void paneUiHandleNetworkIdle( void )
{
   if ( !paneUi.captureActive || !paneUi.captureForumPromptPending )
   {
      return;
   }

   updateCurrentForumName( paneUi.aryObservedLine );
   completeCapture();
}
