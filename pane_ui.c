/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "config_globals.h"
#include "pane_ui_internal.h"
#include "unix.h"

PaneUiState paneUi;

static bool canActivatePaneUi( void );
static void queuePendingLocalInput( void );
static void readTerminalSize( void );

static void readTerminalSize( void )
{
#ifdef TIOCGWINSZ
   struct winsize terminalSize;

   if ( ioctl( 0, TIOCGWINSZ, (char *)&terminalSize ) == 0 )
   {
      paneUi.rows = terminalSize.ws_row;
      paneUi.columns = terminalSize.ws_col;
      return;
   }
#endif
   paneUi.rows = WINDOW_ROWS_DEFAULT;
   paneUi.columns = PANE_UI_LEFT_COLUMNS;
}

static bool canActivatePaneUi( void )
{
   return flagsConfiguration.shouldUsePaneUi &&
          !flagsConfiguration.isScreenReaderModeEnabled &&
          paneUi.columns >= PANE_UI_MIN_COLUMNS &&
          paneUi.rows >= WINDOW_ROWS_MIN;
}

void paneUiEnterIfEligible( void )
{
   bool shouldActivate;

   readTerminalSize();
   shouldActivate = canActivatePaneUi();
   paneUi.resizePending = false;
   if ( shouldActivate == paneUi.active )
   {
      if ( paneUi.active )
      {
         paneUiDrawSidebar();
      }
      return;
   }
   if ( !shouldActivate )
   {
      paneUiLeave();
      paneUiRepaintLeftPane();
      return;
   }

   paneUi.active = true;
   paneUi.captureActive = false;
   paneUi.promptReady = false;
   paneUi.sidebarVisible = paneUi.sessionReady;
   paneUi.swallowingPromptRemainder = false;
   paneUi.nextRefresh = 0;
   paneUiResetObservedLine();
   paneUiWriteRaw( "\033[?1049h\033[?1000h\033[?1006h\033[2J\033[H" );
   paneUiRepaintVisibleLeftPane();
   paneUiDrawSidebar();
}

void paneUiLeave( void )
{
   if ( !paneUi.active )
   {
      return;
   }
   paneUi.active = false;
   paneUi.captureActive = false;
   paneUi.promptReady = false;
   paneUi.sidebarVisible = false;
   paneUi.swallowingPromptRemainder = false;
   paneUi.leftScrollOffset = 0;
   paneUi.sidebarScrollOffset = 0;
   paneUi.mouseInputLength = 0;
   paneUi.pendingLocalInputIndex = 0;
   paneUi.pendingLocalInputLength = 0;
   paneUiWriteRaw( "\033[?1000l\033[?1006l\033[?1049l" );
   fflush( stdout );
}

void paneUiLeaveForExit( void )
{
   bool wasActive;

   wasActive = paneUi.active;
   paneUiLeave();
   if ( wasActive )
   {
      paneUiWriteRaw( "\033[2J\033[H" );
      fflush( stdout );
   }
}

void paneUiResetSession( void )
{
   paneUi.sessionReady = false;
   paneUi.sidebarVisible = false;
   paneUi.captureActive = false;
   paneUi.promptReady = false;
   paneUi.swallowingPromptRemainder = false;
   paneUi.nextRefresh = 0;
   paneUi.snapshotRefreshedAt = 0;
   paneUi.activeView = PANE_UI_VIEW_NONE;
   paneUi.captureView = PANE_UI_VIEW_NONE;
   paneUi.captureCommand = '\0';
   paneUi.captureCarriageReturnPending = false;
   paneUi.captureTruncated = false;
   paneUi.snapshotTruncated = false;
   paneUi.sidebarScrollOffset = 0;
   paneUi.leftLineCount = 1;
   paneUi.leftScrollOffset = 0;
   paneUi.leftVisibleColumn = 0;
   paneUi.leftSkippingAnsi = false;
   paneUi.mouseInputLength = 0;
   paneUi.pendingLocalInputIndex = 0;
   paneUi.pendingLocalInputLength = 0;
   paneUi.aryCaptureForumName[0] = '\0';
   paneUi.aryCurrentForumName[0] = '\0';
   memset( paneUi.aryLeftLines, 0, sizeof( paneUi.aryLeftLines ) );
   paneUiResetObservedLine();
}

bool paneUiIsActive( void )
{
   return paneUi.active;
}

size_t paneUiTerminalContentColumns( void )
{
   return paneUi.active && paneUi.sidebarVisible ? PANE_UI_LEFT_COLUMNS : 0;
}

void paneUiMarkResizePending( void )
{
   paneUi.resizePending = true;
}

void paneUiNoteUserInput( void )
{
   paneUi.promptReady = false;
   paneUi.swallowingPromptRemainder = false;
}

static void queuePendingLocalInput( void )
{
   memcpy( paneUi.aryPendingLocalInput, paneUi.aryMouseInput,
           (size_t)paneUi.mouseInputLength );
   paneUi.pendingLocalInputIndex = 0;
   paneUi.pendingLocalInputLength = paneUi.mouseInputLength;
   paneUi.mouseInputLength = 0;
}

bool paneUiHasPendingLocalInput( void )
{
   return paneUi.pendingLocalInputIndex < paneUi.pendingLocalInputLength;
}

bool paneUiTakePendingLocalInput( int *ptrInputChar )
{
   if ( !paneUiHasPendingLocalInput() )
   {
      return false;
   }

   *ptrInputChar = paneUi.aryPendingLocalInput[paneUi.pendingLocalInputIndex++];
   return true;
}

bool paneUiHandleLocalInput( int inputChar, bool hasMoreLocalInput )
{
   PaneUiView view;

   if ( paneUi.captureActive )
   {
      return true;
   }

   if ( paneUi.active && ( paneUi.mouseInputLength > 0 ||
                           ( inputChar == '\033' && hasMoreLocalInput ) ) )
   {
      int button;
      int column;
      int consumedLength;
      int row;
      char finalChar;

      if ( paneUi.mouseInputLength >= (int)sizeof( paneUi.aryMouseInput ) - 1 )
      {
         queuePendingLocalInput();
         return true;
      }
      paneUi.aryMouseInput[paneUi.mouseInputLength++] = (char)inputChar;
      paneUi.aryMouseInput[paneUi.mouseInputLength] = '\0';
      if ( paneUi.mouseInputLength == 1 )
      {
         return true;
      }
      if ( paneUi.mouseInputLength == 2 && inputChar == '[' )
      {
         return true;
      }
      if ( paneUi.mouseInputLength == 3 && inputChar == '<' )
      {
         return true;
      }
      if ( paneUi.mouseInputLength > 3 &&
           ( isdigit( inputChar ) || inputChar == ';' ) )
      {
         return true;
      }
      if ( ( inputChar == 'M' || inputChar == 'm' ) &&
           sscanf( paneUi.aryMouseInput, "\033[<%d;%d;%d%c%n",
                   &button, &column, &row, &finalChar,
                   &consumedLength ) == 4 &&
           consumedLength == paneUi.mouseInputLength )
      {
         paneUi.mouseInputLength = 0;
         if ( ( button & 64 ) != 0 )
         {
            if ( column <= PANE_UI_LEFT_COLUMNS )
            {
               paneUiScrollLeftPane( ( button & 1 ) == 0
                                        ? PANE_UI_SCROLL_ROWS
                                        : -PANE_UI_SCROLL_ROWS );
            }
            else
            {
               paneUiScrollSidebar( ( button & 1 ) == 0
                                       ? -PANE_UI_SCROLL_ROWS
                                       : PANE_UI_SCROLL_ROWS );
            }
         }
         return true;
      }

      queuePendingLocalInput();
      return true;
   }

   if ( inputChar == 'W' || ( inputChar == 'w' && aryKeyMap['w'] == 'W' ) )
   {
      inputChar = 'W';
      view = PANE_UI_VIEW_WHO;
   }
   else if ( inputChar == '?' )
   {
      view = PANE_UI_VIEW_HELP;
   }
   else if ( inputChar == '@' )
   {
      view = PANE_UI_VIEW_AIDES;
   }
   else if ( inputChar == 'i' || inputChar == 'I' )
   {
      inputChar = 'i';
      view = PANE_UI_VIEW_FORUM_INFO;
   }
   else
   {
      return false;
   }
   if ( !paneUiCanRefreshSidebar() )
   {
      return false;
   }

   paneUiStartCapture( time( NULL ), inputChar, view );
   return true;
}

void paneUiHandleTimer( void )
{
   if ( paneUi.resizePending )
   {
      paneUiEnterIfEligible();
   }
   if ( !paneUi.active )
   {
      return;
   }

   paneUiHandleTimerAt( time( NULL ) );
}

void paneUiHandleTimerAt( time_t now )
{
   if ( paneUi.captureActive && now - paneUi.captureStarted >= PANE_UI_CAPTURE_TIMEOUT_SECONDS )
   {
      paneUi.captureActive = false;
      paneUi.promptReady = false;
      paneUi.nextRefresh = paneUi.activeView == PANE_UI_VIEW_WHO
                              ? now + PANE_UI_REFRESH_SECONDS
                              : 0;
      paneUiDrawSidebar();
      return;
   }
   if ( paneUi.activeView != PANE_UI_VIEW_NONE &&
        paneUi.activeView != PANE_UI_VIEW_WHO )
   {
      return;
   }
   if ( paneUi.nextRefresh == 0 || now < paneUi.nextRefresh )
   {
      return;
   }
   paneUi.nextRefresh = now + PANE_UI_REFRESH_SECONDS;
   if ( !paneUiCanRefreshSidebar() )
   {
      paneUi.promptReady = false;
      return;
   }

   paneUiStartCapture( now, 'W', PANE_UI_VIEW_WHO );
}

bool paneUiSelectTimeout( struct timeval *ptrTimeout )
{
   time_t now;
   time_t wakeTime;

   if ( paneUi.resizePending )
   {
      ptrTimeout->tv_sec = 0;
      ptrTimeout->tv_usec = 0;
      return true;
   }
   if ( !paneUi.active )
   {
      return false;
   }

   now = time( NULL );
   wakeTime = paneUi.captureActive
                 ? paneUi.captureStarted + PANE_UI_CAPTURE_TIMEOUT_SECONDS
                 : paneUi.nextRefresh;
   if ( wakeTime == 0 )
   {
      return false;
   }
   if ( wakeTime <= now )
   {
      ptrTimeout->tv_sec = 0;
   }
   else
   {
      ptrTimeout->tv_sec = wakeTime - now;
   }
   ptrTimeout->tv_usec = 0;
   return true;
}
