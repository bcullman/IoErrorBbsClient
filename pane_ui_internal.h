/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PANE_UI_INTERNAL_H_INCLUDED
#define PANE_UI_INTERNAL_H_INCLUDED

#include "pane_ui.h"

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

extern PaneUiState paneUi;

bool paneUiCanRefreshSidebar( void );
void paneUiAppendLeftChar( int outputChar );
void paneUiDrawSidebar( void );
void paneUiRepaintLeftPane( void );
void paneUiRepaintVisibleLeftPane( void );
void paneUiResetObservedLine( void );
void paneUiScrollLeftPane( int rowDelta );
void paneUiScrollSidebar( int rowDelta );
void paneUiStartCapture( time_t now, int command, PaneUiView view );
void paneUiWriteRaw( const char *ptrText );

#endif // PANE_UI_INTERNAL_H_INCLUDED
