/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "color.h"
#include "config_globals.h"
#include "defs.h"
#include "network_globals.h"
#include "pane_ui.h"
#include "unix.h"
#include "utility.h"

#define PANE_UI_LEFT_COLUMNS 80
#define PANE_UI_USERNAME_COLUMNS 21
#define PANE_UI_MIN_COLUMNS ( PANE_UI_LEFT_COLUMNS + 2 + PANE_UI_USERNAME_COLUMNS )
#define PANE_UI_REFRESH_SECONDS 5
#define PANE_UI_CAPTURE_TIMEOUT_SECONDS 5
#define PANE_UI_MAX_LINES 1000
#define PANE_UI_MAX_LINE_LENGTH 256
#define PANE_UI_LEFT_HISTORY_LINES 1000
#define PANE_UI_LEFT_LINE_LENGTH 1024
#define PANE_UI_MOUSE_INPUT_LENGTH 32
#define PANE_UI_SCROLL_ROWS 3

typedef enum
{
   PANE_UI_VIEW_NONE = 0,
   PANE_UI_VIEW_WHO,
   PANE_UI_VIEW_HELP,
   PANE_UI_VIEW_AIDES,
   PANE_UI_VIEW_FORUM_INFO
} PaneUiView;

typedef struct
{
   bool active;
   bool captureActive;
   bool captureCarriageReturnPending;
   bool captureForumPromptPending;
   bool captureHasForumInfoBody;
   bool captureTruncated;
   bool snapshotTruncated;
   bool resizePending;
   bool sessionReady;
   bool sidebarVisible;
   bool swallowingPromptRemainder;
   bool promptReady;
   bool promptRemainderSkippingAnsi;
   bool observedSkippingAnsi;
   bool skippingAnsi;
   bool leftSkippingAnsi;
   int ansiBytesRemaining;
   int observedAnsiBytesRemaining;
   int promptRemainderAnsiBytesRemaining;
   int rows;
   int columns;
   int captureLineCount;
   int snapshotLineCount;
   int sidebarScrollOffset;
   int leftLineCount;
   int leftScrollOffset;
   int leftVisibleColumn;
   int mouseInputLength;
   int pendingLocalInputIndex;
   int pendingLocalInputLength;
   size_t captureAnsiLength;
   size_t captureLineLength;
   size_t observedLineLength;
   time_t captureStarted;
   time_t nextRefresh;
   time_t snapshotRefreshedAt;
   PaneUiView activeView;
   PaneUiView captureView;
   char captureCommand;
   char aryCaptureLines[PANE_UI_MAX_LINES + 1][PANE_UI_MAX_LINE_LENGTH];
   char aryCaptureAnsi[16];
   char aryCaptureForumName[PANE_UI_MAX_LINE_LENGTH];
   char aryCurrentForumName[PANE_UI_MAX_LINE_LENGTH];
   char arySnapshotLines[PANE_UI_MAX_LINES][PANE_UI_MAX_LINE_LENGTH];
   char aryObservedLine[PANE_UI_MAX_LINE_LENGTH];
   char aryLeftLines[PANE_UI_LEFT_HISTORY_LINES][PANE_UI_LEFT_LINE_LENGTH];
   char aryMouseInput[PANE_UI_MOUSE_INPUT_LENGTH];
   char aryPendingLocalInput[PANE_UI_MOUSE_INPUT_LENGTH];
} PaneUiState;

static PaneUiState paneUi;

static void appendCaptureChar( int inputChar );
static void appendCaptureText( const char *ptrText, size_t textLength );
static void appendObservedChar( int inputChar );
static void appendLeftChar( int outputChar );
static void advanceLeftLine( void );
static bool canActivatePaneUi( void );
static bool canRefreshSidebar( void );
static void completeCapture( void );
static void discardCurrentCaptureLine( void );
static void drawSidebar( void );
static void drawSidebarFooter( int row, int sidebarWidth, int visibleRows );
static void drawSidebarLine( const char *ptrLine, int visibleWidth,
                             int *ptrForegroundColor );
static void drawSidebarTimestamp( int row, const char *ptrLine, int visibleWidth );
static void drawThemedDisplayState( int foregroundColor );
static void drawThemedForeground( int foregroundColor );
static void formatSnapshotRefreshTime( char *ptrBuffer, size_t bufferSize );
static bool isCapturedSgrSequence( void );
static bool isSafePromptLine( const char *ptrLine );
static bool isForumInfoReturnPrompt( const char *ptrLine );
static void queuePendingLocalInput( void );
static void repaintLeftPane( void );
static void repaintVisibleLeftPane( void );
static void readTerminalSize( void );
static void removeMorePromptMarkers( char *ptrLine );
static void resetObservedLine( void );
static void scanSidebarForeground( const char *ptrLine, int *ptrForegroundColor );
static void scrollLeftPane( int rowDelta );
static void scrollSidebar( int rowDelta );
static void sendHiddenSidebarChar( int inputChar );
static void startCapture( time_t now, int command, PaneUiView view );
static bool shouldSwallowPromptRemainderChar( int inputChar );
static void updateCurrentForumName( const char *ptrPrompt );
static const char *viewName( PaneUiView view );
static int visibleHeaderLabelWidth( const char *ptrLine );
static void writeRaw( const char *ptrText );

static void writeRaw( const char *ptrText )
{
   (void)fputs( ptrText, stdout );
}

static void advanceLeftLine( void )
{
   if ( paneUi.leftLineCount == 0 )
   {
      paneUi.leftLineCount = 1;
   }
   if ( paneUi.leftLineCount == PANE_UI_LEFT_HISTORY_LINES )
   {
      memmove( paneUi.aryLeftLines, paneUi.aryLeftLines + 1,
               sizeof( paneUi.aryLeftLines ) - sizeof( paneUi.aryLeftLines[0] ) );
      paneUi.leftLineCount--;
   }
   paneUi.aryLeftLines[paneUi.leftLineCount++][0] = '\0';
   paneUi.leftVisibleColumn = 0;
}

static void appendLeftChar( int outputChar )
{
   char *ptrLine;
   size_t lineLength;

   paneUi.leftScrollOffset = 0;
   if ( paneUi.leftLineCount == 0 )
   {
      paneUi.leftLineCount = 1;
   }
   if ( outputChar == '\r' )
   {
      return;
   }
   if ( outputChar == '\n' )
   {
      advanceLeftLine();
      return;
   }
   if ( !paneUi.leftSkippingAnsi &&
        ( outputChar < ASCII_PRINTABLE_MIN || outputChar >= ASCII_PRINTABLE_MAX ) )
   {
      if ( outputChar != '\033' )
      {
         return;
      }
      paneUi.leftSkippingAnsi = true;
   }
   else if ( paneUi.leftSkippingAnsi && isalpha( outputChar ) )
   {
      paneUi.leftSkippingAnsi = false;
   }
   else if ( !paneUi.leftSkippingAnsi &&
             paneUi.leftVisibleColumn >= PANE_UI_LEFT_COLUMNS )
   {
      advanceLeftLine();
   }

   ptrLine = paneUi.aryLeftLines[paneUi.leftLineCount - 1];
   lineLength = strlen( ptrLine );
   if ( lineLength + 1 < PANE_UI_LEFT_LINE_LENGTH )
   {
      ptrLine[lineLength] = (char)outputChar;
      ptrLine[lineLength + 1] = '\0';
   }
   if ( !paneUi.leftSkippingAnsi && outputChar >= ASCII_PRINTABLE_MIN &&
        outputChar < ASCII_PRINTABLE_MAX )
   {
      paneUi.leftVisibleColumn++;
   }
}

static void scrollLeftPane( int rowDelta )
{
   int maximumOffset;

   maximumOffset = paneUi.leftLineCount > paneUi.rows
                      ? paneUi.leftLineCount - paneUi.rows
                      : 0;
   paneUi.leftScrollOffset += rowDelta;
   if ( paneUi.leftScrollOffset < 0 )
   {
      paneUi.leftScrollOffset = 0;
   }
   else if ( paneUi.leftScrollOffset > maximumOffset )
   {
      paneUi.leftScrollOffset = maximumOffset;
   }
   repaintVisibleLeftPane();
}

static void scrollSidebar( int rowDelta )
{
   int maximumOffset;
   int visibleRows;

   visibleRows = paneUi.snapshotLineCount > paneUi.rows ? paneUi.rows - 1
                                                        : paneUi.rows;
   maximumOffset = paneUi.snapshotLineCount > visibleRows
                      ? paneUi.snapshotLineCount - visibleRows
                      : 0;
   paneUi.sidebarScrollOffset += rowDelta;
   if ( paneUi.sidebarScrollOffset < 0 )
   {
      paneUi.sidebarScrollOffset = 0;
   }
   else if ( paneUi.sidebarScrollOffset > maximumOffset )
   {
      paneUi.sidebarScrollOffset = maximumOffset;
   }
   drawSidebar();
}

static void repaintLeftPane( void )
{
   int lineIndex;
   int startLine;

   writeRaw( "\033[2J\033[H" );
   startLine = paneUi.leftLineCount > paneUi.rows + paneUi.leftScrollOffset
                  ? paneUi.leftLineCount - paneUi.rows - paneUi.leftScrollOffset
                  : 0;
   for ( lineIndex = startLine; lineIndex < paneUi.leftLineCount; lineIndex++ )
   {
      if ( lineIndex > startLine )
      {
         writeRaw( "\r\n" );
      }
      writeRaw( paneUi.aryLeftLines[lineIndex] );
   }
   fflush( stdout );
}

static void repaintVisibleLeftPane( void )
{
   int row;
   int startLine;
   char arySequence[64];

   if ( !paneUi.active )
   {
      repaintLeftPane();
      return;
   }

   startLine = paneUi.leftLineCount > paneUi.rows + paneUi.leftScrollOffset
                  ? paneUi.leftLineCount - paneUi.rows - paneUi.leftScrollOffset
                  : 0;
   writeRaw( "\0337" );
   for ( row = 1; row <= paneUi.rows; row++ )
   {
      int lineIndex;

      snprintf( arySequence, sizeof( arySequence ), "\033[%d;1H", row );
      writeRaw( arySequence );
      drawThemedDisplayState( color.text );
      writeRaw( "\033[80X" );
      lineIndex = startLine + row - 1;
      if ( lineIndex < paneUi.leftLineCount )
      {
         writeRaw( paneUi.aryLeftLines[lineIndex] );
      }
   }
   writeRaw( "\033[0m\0338" );
   fflush( stdout );
}

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

static void drawThemedForeground( int foregroundColor )
{
   char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

   formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                 foregroundColor );
   writeRaw( aryAnsiSequence );
}

static void drawThemedDisplayState( int foregroundColor )
{
   char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

   formatAnsiDisplayStateSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                   foregroundColor, color.background,
                                   flagsConfiguration.shouldUseBold );
   writeRaw( aryAnsiSequence );
}

static void drawSidebarLine( const char *ptrLine, int visibleWidth,
                             int *ptrForegroundColor )
{
   int writtenLength;

   drawThemedDisplayState( *ptrForegroundColor );
   writtenLength = 0;
   while ( *ptrLine != '\0' && writtenLength < visibleWidth )
   {
      if ( ptrLine[0] == '\033' && ptrLine[1] == '[' )
      {
         const char *ptrSequenceEnd;

         ptrSequenceEnd = ptrLine + 2;
         while ( *ptrSequenceEnd != '\0' &&
                 ( isdigit( (unsigned char)*ptrSequenceEnd ) ||
                   *ptrSequenceEnd == ';' ) )
         {
            ptrSequenceEnd++;
         }
         if ( *ptrSequenceEnd == 'm' )
         {
            if ( ptrSequenceEnd - ptrLine == 4 && ptrLine[2] == '3' )
            {
               *ptrForegroundColor = ansiTransform( ptrLine[3] );
               drawThemedForeground( *ptrForegroundColor );
            }
            else
            {
               if ( ptrSequenceEnd - ptrLine == 2 ||
                    ( ptrSequenceEnd - ptrLine == 3 && ptrLine[2] == '0' ) )
               {
                  *ptrForegroundColor = color.text;
                  drawThemedDisplayState( *ptrForegroundColor );
               }
               else
               {
                  fwrite( ptrLine, 1,
                          (size_t)( ptrSequenceEnd - ptrLine + 1 ),
                          stdout );
               }
            }
            ptrLine = ptrSequenceEnd + 1;
            continue;
         }
      }

      putchar( *ptrLine );
      writtenLength++;
      ptrLine++;
   }
   writeRaw( "\033[0m" );
}

static void scanSidebarForeground( const char *ptrLine, int *ptrForegroundColor )
{
   while ( *ptrLine != '\0' )
   {
      if ( ptrLine[0] == '\033' && ptrLine[1] == '[' )
      {
         const char *ptrSequenceEnd;

         ptrSequenceEnd = ptrLine + 2;
         while ( *ptrSequenceEnd != '\0' &&
                 ( isdigit( (unsigned char)*ptrSequenceEnd ) ||
                   *ptrSequenceEnd == ';' ) )
         {
            ptrSequenceEnd++;
         }
         if ( *ptrSequenceEnd == 'm' )
         {
            if ( ptrSequenceEnd - ptrLine == 4 && ptrLine[2] == '3' )
            {
               *ptrForegroundColor = ansiTransform( ptrLine[3] );
            }
            else if ( ptrSequenceEnd - ptrLine == 2 ||
                      ( ptrSequenceEnd - ptrLine == 3 && ptrLine[2] == '0' ) )
            {
               *ptrForegroundColor = color.text;
            }
            ptrLine = ptrSequenceEnd + 1;
            continue;
         }
      }
      ptrLine++;
   }
}

static int visibleHeaderLabelWidth( const char *ptrLine )
{
   const char *ptrDoingEnd;
   int visibleWidth;

   if ( strstr( ptrLine, "User Name" ) == NULL ||
        strstr( ptrLine, "Time" ) == NULL ||
        ( ptrDoingEnd = strstr( ptrLine, "Doing" ) ) == NULL )
   {
      return 0;
   }

   ptrDoingEnd += strlen( "Doing" );
   visibleWidth = 0;
   while ( ptrLine < ptrDoingEnd )
   {
      if ( *ptrLine == '\033' )
      {
         while ( ptrLine < ptrDoingEnd && !isalpha( (unsigned char)*ptrLine ) )
         {
            ptrLine++;
         }
      }
      else
      {
         visibleWidth++;
      }
      ptrLine++;
   }
   return visibleWidth;
}

static void formatSnapshotRefreshTime( char *ptrBuffer, size_t bufferSize )
{
   struct tm localTime;
   int hour;

   localtime_r( &paneUi.snapshotRefreshedAt, &localTime );
   hour = localTime.tm_hour % 12;
   if ( hour == 0 )
   {
      hour = 12;
   }
   snprintf( ptrBuffer, bufferSize, "%d/%d/%02d %d:%02d:%02d %s",
             localTime.tm_mon + 1, localTime.tm_mday,
             ( localTime.tm_year + 1900 ) % 100, hour,
             localTime.tm_min, localTime.tm_sec,
             localTime.tm_hour < 12 ? "AM" : "PM" );
}

static void drawSidebarTimestamp( int row, const char *ptrLine, int visibleWidth )
{
   char arySequence[64];
   char aryTimestamp[32];
   int labelWidth;
   int timestampLength;

   labelWidth = visibleHeaderLabelWidth( ptrLine );
   if ( labelWidth == 0 || paneUi.snapshotRefreshedAt == 0 )
   {
      return;
   }

   formatSnapshotRefreshTime( aryTimestamp, sizeof( aryTimestamp ) );
   timestampLength = (int)strlen( aryTimestamp );
   if ( labelWidth + 1 + timestampLength > visibleWidth )
   {
      return;
   }

   snprintf( arySequence, sizeof( arySequence ), "\033[%d;%dH",
             row, paneUi.columns - timestampLength + 1 );
   writeRaw( arySequence );
   drawThemedForeground( ansiTransform( '7' ) );
   writeRaw( aryTimestamp );
   writeRaw( "\033[0m" );
}

static const char *viewName( PaneUiView view )
{
   switch ( view )
   {
      case PANE_UI_VIEW_WHO:
         return "Who";

      case PANE_UI_VIEW_HELP:
         return "Help";

      case PANE_UI_VIEW_AIDES:
         return "Aides";

      case PANE_UI_VIEW_FORUM_INFO:
         return "Forum Info";

      default:
         return "Sidebar";
   }
}

static void drawSidebarFooter( int row, int sidebarWidth, int visibleRows )
{
   char aryFooter[PANE_UI_MAX_LINE_LENGTH];
   char arySequence[64];
   int firstLine;
   int footerForegroundColor;
   int lastLine;

   firstLine = paneUi.sidebarScrollOffset + 1;
   lastLine = paneUi.sidebarScrollOffset + visibleRows;
   if ( lastLine > paneUi.snapshotLineCount )
   {
      lastLine = paneUi.snapshotLineCount;
   }
   snprintf( aryFooter, sizeof( aryFooter ), "%s lines %d-%d of %d%s",
             viewName( paneUi.activeView ), firstLine, lastLine,
             paneUi.snapshotLineCount,
             paneUi.snapshotTruncated ? " truncated" : "" );
   snprintf( arySequence, sizeof( arySequence ), "\033[%d;%dH",
             row, PANE_UI_LEFT_COLUMNS + 1 );
   writeRaw( arySequence );
   drawThemedDisplayState( color.text );
   writeRaw( "|\033[K " );
   footerForegroundColor = color.text;
   drawSidebarLine( aryFooter, sidebarWidth - 1, &footerForegroundColor );
}

static void drawSidebar( void )
{
   int row;
   int sidebarWidth;
   int snapshotForegroundColor;
   int visibleRows;
   char arySequence[64];

   if ( !paneUi.active || !paneUi.sidebarVisible )
   {
      return;
   }

   sidebarWidth = paneUi.columns - PANE_UI_LEFT_COLUMNS - 1;
   visibleRows = paneUi.snapshotLineCount > paneUi.rows ? paneUi.rows - 1
                                                        : paneUi.rows;
   snapshotForegroundColor = color.text;
   for ( row = 0; row < paneUi.sidebarScrollOffset; row++ )
   {
      scanSidebarForeground( paneUi.arySnapshotLines[row],
                             &snapshotForegroundColor );
   }
   writeRaw( "\0337" );
   for ( row = 1; row <= paneUi.rows; row++ )
   {
      snprintf( arySequence, sizeof( arySequence ), "\033[%d;%dH",
                row, PANE_UI_LEFT_COLUMNS + 1 );
      writeRaw( arySequence );
      drawThemedDisplayState( color.text );
      writeRaw( "|\033[K" );
      if ( row <= visibleRows &&
           paneUi.sidebarScrollOffset + row - 1 < paneUi.snapshotLineCount )
      {
         const char *ptrLine;
         int labelWidth;

         ptrLine = paneUi.arySnapshotLines[paneUi.sidebarScrollOffset + row - 1];
         putchar( ' ' );
         labelWidth = visibleHeaderLabelWidth( ptrLine );
         if ( labelWidth > sidebarWidth - 1 )
         {
            labelWidth = sidebarWidth - 1;
         }
         drawSidebarLine( ptrLine, labelWidth ? labelWidth : sidebarWidth - 1,
                          &snapshotForegroundColor );
         drawSidebarTimestamp( row, ptrLine, sidebarWidth - 1 );
      }
      else
      {
         writeRaw( "\033[0m" );
      }
   }
   if ( paneUi.snapshotLineCount > paneUi.rows )
   {
      drawSidebarFooter( paneUi.rows, sidebarWidth, visibleRows );
   }
   writeRaw( "\0338" );
   fflush( stdout );
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
         drawSidebar();
      }
      return;
   }
   if ( !shouldActivate )
   {
      paneUiLeave();
      repaintLeftPane();
      return;
   }

   paneUi.active = true;
   paneUi.captureActive = false;
   paneUi.promptReady = false;
   paneUi.sidebarVisible = paneUi.sessionReady;
   paneUi.swallowingPromptRemainder = false;
   paneUi.nextRefresh = 0;
   resetObservedLine();
   writeRaw( "\033[?1049h\033[?1000h\033[?1006h\033[2J\033[H" );
   repaintVisibleLeftPane();
   drawSidebar();
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
   writeRaw( "\033[?1000l\033[?1006l\033[?1049l" );
   fflush( stdout );
}

void paneUiLeaveForExit( void )
{
   bool wasActive;

   wasActive = paneUi.active;
   paneUiLeave();
   if ( wasActive )
   {
      writeRaw( "\033[2J\033[H" );
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
   resetObservedLine();
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

static void resetObservedLine( void )
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
      resetObservedLine();
      return;
   }
   if ( inputChar < ASCII_PRINTABLE_MIN || inputChar >= ASCII_PRINTABLE_MAX )
   {
      return;
   }
   if ( paneUi.observedLineLength + 1 >= sizeof( paneUi.aryObservedLine ) )
   {
      resetObservedLine();
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
   resetObservedLine();
   drawSidebar();
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
            resetObservedLine();
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
         drawSidebar();
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

static bool canRefreshSidebar( void )
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

static void startCapture( time_t now, int command, PaneUiView view )
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
   resetObservedLine();
   sendHiddenSidebarChar( command );
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
               scrollLeftPane( ( button & 1 ) == 0
                                  ? PANE_UI_SCROLL_ROWS
                                  : -PANE_UI_SCROLL_ROWS );
            }
            else
            {
               scrollSidebar( ( button & 1 ) == 0
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
   if ( !canRefreshSidebar() )
   {
      return false;
   }

   startCapture( time( NULL ), inputChar, view );
   return true;
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
      drawSidebar();
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
   if ( !canRefreshSidebar() )
   {
      paneUi.promptReady = false;
      return;
   }

   startCapture( now, 'W', PANE_UI_VIEW_WHO );
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

void paneUiAfterOutputChar( int outputChar )
{
   bool wasScrolled;

   wasScrolled = paneUi.leftScrollOffset > 0;
   appendLeftChar( outputChar );
   if ( paneUi.active && wasScrolled )
   {
      repaintVisibleLeftPane();
   }
   if ( paneUi.active && outputChar == '\n' )
   {
      drawSidebar();
   }
}

void paneUiAfterOutputText( const char *ptrText )
{
   bool wasScrolled;
   const char *ptrCursor;

   wasScrolled = paneUi.leftScrollOffset > 0;
   for ( ptrCursor = ptrText; *ptrCursor != '\0'; ptrCursor++ )
   {
      appendLeftChar( *ptrCursor );
   }
   if ( paneUi.active && wasScrolled )
   {
      repaintVisibleLeftPane();
   }
   if ( paneUi.active && strchr( ptrText, '\n' ) != NULL )
   {
      drawSidebar();
   }
}
