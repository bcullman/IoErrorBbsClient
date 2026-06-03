/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "color.h"
#include "config_globals.h"
#include "pane_ui_internal.h"
#include "utility.h"

static void advanceLeftLine( void );
static void drawSidebarFooter( int row, int sidebarWidth, int visibleRows );
static void drawSidebarLine( const char *ptrLine, int visibleWidth,
                             int *ptrForegroundColor );
static void drawSidebarTimestamp( int row, int snapshotLineIndex,
                                  const char *ptrLine, int visibleWidth );
static void drawThemedDisplayState( int foregroundColor );
static void drawThemedForeground( int foregroundColor );
static void formatSnapshotRefreshTime( char *ptrBuffer, size_t bufferSize );
static void scanSidebarForeground( const char *ptrLine, int *ptrForegroundColor );
static const char *viewName( PaneUiView view );
static int visibleHeaderLabelWidth( const char *ptrLine );

void paneUiWriteRaw( const char *ptrText )
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

void paneUiAppendLeftChar( int outputChar )
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

void paneUiScrollLeftPane( int rowDelta )
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
   paneUiRepaintVisibleLeftPane();
}

void paneUiScrollSidebar( int rowDelta )
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
   paneUiDrawSidebar();
}

void paneUiRepaintLeftPane( void )
{
   int lineIndex;
   int startLine;

   paneUiWriteRaw( "\033[2J\033[H" );
   startLine = paneUi.leftLineCount > paneUi.rows + paneUi.leftScrollOffset
                  ? paneUi.leftLineCount - paneUi.rows - paneUi.leftScrollOffset
                  : 0;
   for ( lineIndex = startLine; lineIndex < paneUi.leftLineCount; lineIndex++ )
   {
      if ( lineIndex > startLine )
      {
         paneUiWriteRaw( "\r\n" );
      }
      paneUiWriteRaw( paneUi.aryLeftLines[lineIndex] );
   }
   fflush( stdout );
}

void paneUiRepaintVisibleLeftPane( void )
{
   int row;
   int startLine;
   char arySequence[64];

   if ( !paneUi.active )
   {
      paneUiRepaintLeftPane();
      return;
   }

   startLine = paneUi.leftLineCount > paneUi.rows + paneUi.leftScrollOffset
                  ? paneUi.leftLineCount - paneUi.rows - paneUi.leftScrollOffset
                  : 0;
   paneUiWriteRaw( "\0337" );
   for ( row = 1; row <= paneUi.rows; row++ )
   {
      int lineIndex;

      snprintf( arySequence, sizeof( arySequence ), "\033[%d;1H", row );
      paneUiWriteRaw( arySequence );
      drawThemedDisplayState( color.text );
      paneUiWriteRaw( "\033[80X" );
      lineIndex = startLine + row - 1;
      if ( lineIndex < paneUi.leftLineCount )
      {
         paneUiWriteRaw( paneUi.aryLeftLines[lineIndex] );
      }
   }
   paneUiWriteRaw( "\033[0m\0338" );
   fflush( stdout );
}

static void drawThemedForeground( int foregroundColor )
{
   char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

   formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                 foregroundColor );
   paneUiWriteRaw( aryAnsiSequence );
}

static void drawThemedDisplayState( int foregroundColor )
{
   char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

   formatAnsiDisplayStateSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                   foregroundColor, color.background,
                                   flagsConfiguration.shouldUseBold );
   paneUiWriteRaw( aryAnsiSequence );
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
   paneUiWriteRaw( "\033[0m" );
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

static int visibleLastDashOffset( const char *ptrLine )
{
   int lastDashOffset;
   int visibleOffset;

   lastDashOffset = 0;
   visibleOffset = 0;
   while ( *ptrLine != '\0' )
   {
      if ( *ptrLine == '\033' )
      {
         while ( *ptrLine != '\0' && !isalpha( (unsigned char)*ptrLine ) )
         {
            ptrLine++;
         }
      }
      else
      {
         visibleOffset++;
         if ( *ptrLine == '-' )
         {
            lastDashOffset = visibleOffset;
         }
      }
      if ( *ptrLine != '\0' )
      {
         ptrLine++;
      }
   }
   return lastDashOffset;
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

static void drawSidebarTimestamp( int row, int snapshotLineIndex,
                                  const char *ptrLine, int visibleWidth )
{
   char arySequence[64];
   char aryTimestamp[32];
   int labelWidth;
   int rightColumn;
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

   rightColumn = paneUi.columns;
   if ( snapshotLineIndex + 1 < paneUi.snapshotLineCount )
   {
      int separatorWidth;

      separatorWidth =
         visibleLastDashOffset( paneUi.arySnapshotLines[snapshotLineIndex + 1] );
      if ( separatorWidth > visibleWidth )
      {
         separatorWidth = visibleWidth;
      }
      if ( separatorWidth > 0 )
      {
         rightColumn = PANE_UI_LEFT_COLUMNS + 2 + separatorWidth;
      }
   }
   snprintf( arySequence, sizeof( arySequence ), "\033[%d;%dH",
             row, rightColumn - timestampLength + 1 );
   paneUiWriteRaw( arySequence );
   drawThemedForeground( ansiTransform( '7' ) );
   paneUiWriteRaw( aryTimestamp );
   paneUiWriteRaw( "\033[0m" );
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
      case PANE_UI_VIEW_NEXT_POST:
         return "Next Post";
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
   paneUiWriteRaw( arySequence );
   drawThemedDisplayState( color.text );
   paneUiWriteRaw( "|\033[K " );
   footerForegroundColor = color.text;
   drawSidebarLine( aryFooter, sidebarWidth - 1, &footerForegroundColor );
}

void paneUiDrawSidebar( void )
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
   paneUiWriteRaw( "\0337" );
   for ( row = 1; row <= paneUi.rows; row++ )
   {
      snprintf( arySequence, sizeof( arySequence ), "\033[%d;%dH",
                row, PANE_UI_LEFT_COLUMNS + 1 );
      paneUiWriteRaw( arySequence );
      drawThemedDisplayState( color.text );
      paneUiWriteRaw( "|\033[K" );
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
         drawSidebarTimestamp( row, paneUi.sidebarScrollOffset + row - 1,
                               ptrLine, sidebarWidth - 1 );
      }
      else
      {
         paneUiWriteRaw( "\033[0m" );
      }
   }
   if ( paneUi.snapshotLineCount > paneUi.rows )
   {
      drawSidebarFooter( paneUi.rows, sidebarWidth, visibleRows );
   }
   paneUiWriteRaw( "\0338" );
   fflush( stdout );
}

void paneUiAfterOutputChar( int outputChar )
{
   bool wasScrolled;

   wasScrolled = paneUi.leftScrollOffset > 0;
   paneUiAppendLeftChar( outputChar );
   if ( paneUi.active && wasScrolled )
   {
      paneUiRepaintVisibleLeftPane();
   }
   if ( paneUi.active && outputChar == '\n' )
   {
      paneUiDrawSidebar();
   }
}

void paneUiAfterOutputText( const char *ptrText )
{
   bool wasScrolled;
   const char *ptrCursor;

   wasScrolled = paneUi.leftScrollOffset > 0;
   for ( ptrCursor = ptrText; *ptrCursor != '\0'; ptrCursor++ )
   {
      paneUiAppendLeftChar( *ptrCursor );
   }
   if ( paneUi.active && wasScrolled )
   {
      paneUiRepaintVisibleLeftPane();
   }
   if ( paneUi.active && strchr( ptrText, '\n' ) != NULL )
   {
      paneUiDrawSidebar();
   }
}
