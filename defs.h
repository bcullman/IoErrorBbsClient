/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This header gathers the common project includes and shared definitions.
 * Pure C dependencies live here, and system-specific details live in unix.h.
 */
#ifndef DEFS_H_INCLUDED
#define DEFS_H_INCLUDED

#define INT_VERSION 2310
#define DEFAULT_EDITOR_CONFIG_VALUE "$EDITOR"

#include "config.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <string.h>
#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

// Use sigsetjmp/siglongjmp behavior when available so signal masks are
// preserved across jump boundaries.
#define USE_POSIX_SIGSETJMP 1

#include <errno.h>
// extern int errno;

#define COLOR_VALUE_DEFAULT 256
#define COLOR_VALUE_RGB_FLAG 0x01000000
#define COLOR_VALUE_RGB_MASK 0x00FFFFFF
#define ANSI_SEQUENCE_BUFFER_SIZE 64

typedef enum
{
   COLOR_OUTPUT_MODE_AUTO = 0,
   COLOR_OUTPUT_MODE_TRUECOLOR,
   COLOR_OUTPUT_MODE_256
} ColorOutputMode;

extern ColorOutputMode configuredColorOutputMode;
extern bool useBlackThemeBackgrounds;

static inline bool colorValueIsRgb( int colorValue )
{
   return ( colorValue & COLOR_VALUE_RGB_FLAG ) != 0;
}

static inline bool colorValueIsDefault( int colorValue )
{
   return colorValue == COLOR_VALUE_DEFAULT;
}

static inline bool colorValueIsPalette( int colorValue )
{
   return !colorValueIsDefault( colorValue ) && !colorValueIsRgb( colorValue );
}

static inline int colorValueFromRgb( int red, int green, int blue )
{
   return COLOR_VALUE_RGB_FLAG |
          ( ( red & 0xFF ) << 16 ) |
          ( ( green & 0xFF ) << 8 ) |
          ( blue & 0xFF );
}

static inline int colorValueRed( int colorValue )
{
   return ( colorValue & COLOR_VALUE_RGB_MASK ) >> 16;
}

static inline int colorValueGreen( int colorValue )
{
   return ( ( colorValue & COLOR_VALUE_RGB_MASK ) >> 8 ) & 0xFF;
}

static inline int colorValueBlue( int colorValue )
{
   return colorValue & 0xFF;
}

static inline bool textEqualsIgnoreCase( const char *ptrLeft, const char *ptrRight )
{
   while ( *ptrLeft != '\0' && *ptrRight != '\0' )
   {
      if ( tolower( (unsigned char)*ptrLeft ) != tolower( (unsigned char)*ptrRight ) )
      {
         return false;
      }
      ptrLeft++;
      ptrRight++;
   }

   return *ptrLeft == '\0' && *ptrRight == '\0';
}

static inline bool textStartsWith( const char *ptrText, const char *ptrPrefix )
{
   while ( *ptrPrefix != '\0' )
   {
      if ( *ptrText++ != *ptrPrefix++ )
      {
         return false;
      }
   }

   return true;
}

static inline bool terminalShouldUseTruecolor( void )
{
   const char *ptrColorTerm;
   const char *ptrTerm;
   const char *ptrTermProgram;

   switch ( configuredColorOutputMode )
   {
      case COLOR_OUTPUT_MODE_TRUECOLOR:
         return true;

      case COLOR_OUTPUT_MODE_256:
         return false;

      case COLOR_OUTPUT_MODE_AUTO:
      default:
         break;
   }

   ptrTermProgram = getenv( "TERM_PROGRAM" );
   if ( ptrTermProgram != NULL )
   {
      if ( strcmp( ptrTermProgram, "Apple_Terminal" ) == 0 )
      {
         return false;
      }
      if ( strcmp( ptrTermProgram, "iTerm.app" ) == 0 ||
           strcmp( ptrTermProgram, "WezTerm" ) == 0 ||
           strcmp( ptrTermProgram, "ghostty" ) == 0 ||
           strcmp( ptrTermProgram, "vscode" ) == 0 )
      {
         return true;
      }
   }

   ptrColorTerm = getenv( "COLORTERM" );
   if ( ptrColorTerm != NULL &&
        ( textEqualsIgnoreCase( ptrColorTerm, "truecolor" ) ||
          textEqualsIgnoreCase( ptrColorTerm, "24bit" ) ) )
   {
      return true;
   }

   ptrTerm = getenv( "TERM" );
   if ( ptrTerm != NULL )
   {
      size_t termLength;

      termLength = strlen( ptrTerm );
      if ( termLength >= 7 && strcmp( ptrTerm + termLength - 7, "-direct" ) == 0 )
      {
         return true;
      }
      if ( textStartsWith( ptrTerm, "wezterm" ) ||
           textStartsWith( ptrTerm, "xterm-kitty" ) ||
           textStartsWith( ptrTerm, "ghostty" ) )
      {
         return true;
      }
   }

   return false;
}

static inline void xterm256RgbComponents( int colorValue, int *ptrRed,
                                          int *ptrGreen, int *ptrBlue )
{
   static const int aryCubeLevels[6] = { 0, 95, 135, 175, 215, 255 };
   int cubeIndex;

   if ( colorValue < 0 )
   {
      colorValue = 0;
   }
   if ( colorValue > 255 )
   {
      colorValue = 255;
   }
   if ( colorValue < 16 )
   {
      static const int aryBaseColors[16][3] =
         {
            { 0, 0, 0 },
            { 128, 0, 0 },
            { 0, 128, 0 },
            { 128, 128, 0 },
            { 0, 0, 128 },
            { 128, 0, 128 },
            { 0, 128, 128 },
            { 192, 192, 192 },
            { 128, 128, 128 },
            { 255, 0, 0 },
            { 0, 255, 0 },
            { 255, 255, 0 },
            { 0, 0, 255 },
            { 255, 0, 255 },
            { 0, 255, 255 },
            { 255, 255, 255 } };

      *ptrRed = aryBaseColors[colorValue][0];
      *ptrGreen = aryBaseColors[colorValue][1];
      *ptrBlue = aryBaseColors[colorValue][2];
      return;
   }
   if ( colorValue >= 232 )
   {
      int grayLevel;

      grayLevel = 8 + ( colorValue - 232 ) * 10;
      *ptrRed = grayLevel;
      *ptrGreen = grayLevel;
      *ptrBlue = grayLevel;
      return;
   }

   cubeIndex = colorValue - 16;
   *ptrRed = aryCubeLevels[cubeIndex / 36];
   *ptrGreen = aryCubeLevels[( cubeIndex / 6 ) % 6];
   *ptrBlue = aryCubeLevels[cubeIndex % 6];
}

static inline int xterm256ValueFromRgb( int red, int green, int blue )
{
   int bestDistanceSquared;
   int bestValue;
   int paletteBlue;
   int paletteGreen;
   int paletteRed;
   int paletteValue;

   bestValue = 0;
   bestDistanceSquared = INT_MAX;

   for ( paletteValue = 0; paletteValue <= 255; paletteValue++ )
   {
      int blueDistance;
      int currentDistanceSquared;
      int greenDistance;
      int redDistance;

      xterm256RgbComponents( paletteValue, &paletteRed, &paletteGreen, &paletteBlue );
      redDistance = paletteRed - red;
      greenDistance = paletteGreen - green;
      blueDistance = paletteBlue - blue;
      currentDistanceSquared = redDistance * redDistance +
                               greenDistance * greenDistance +
                               blueDistance * blueDistance;
      if ( currentDistanceSquared < bestDistanceSquared )
      {
         bestDistanceSquared = currentDistanceSquared;
         bestValue = paletteValue;
      }
   }

   return bestValue;
}

static inline size_t appendAnsiColorSelector( char *ptrBuffer, size_t bufferSize,
                                              size_t writeOffset, int colorValue,
                                              bool isBackground )
{
   if ( writeOffset >= bufferSize )
   {
      return writeOffset;
   }
   if ( colorValueIsDefault( colorValue ) )
   {
      return writeOffset + (size_t)snprintf( ptrBuffer + writeOffset,
                                             bufferSize - writeOffset,
                                             "%d",
                                             isBackground ? 49 : 39 );
   }
   if ( colorValueIsRgb( colorValue ) )
   {
      if ( terminalShouldUseTruecolor() )
      {
         return writeOffset + (size_t)snprintf( ptrBuffer + writeOffset,
                                                bufferSize - writeOffset,
                                                "%d;2;%d;%d;%d",
                                                isBackground ? 48 : 38,
                                                colorValueRed( colorValue ),
                                                colorValueGreen( colorValue ),
                                                colorValueBlue( colorValue ) );
      }
      if ( isBackground && useBlackThemeBackgrounds )
      {
         colorValue = 0;
      }
      else
      {
         colorValue = xterm256ValueFromRgb( colorValueRed( colorValue ),
                                            colorValueGreen( colorValue ),
                                            colorValueBlue( colorValue ) );
      }
   }
   if ( colorValue >= 0 && colorValue <= 7 )
   {
      return writeOffset + (size_t)snprintf( ptrBuffer + writeOffset,
                                             bufferSize - writeOffset,
                                             "%d",
                                             ( isBackground ? 40 : 30 ) + colorValue );
   }
   if ( colorValue >= 8 && colorValue <= 15 )
   {
      return writeOffset + (size_t)snprintf( ptrBuffer + writeOffset,
                                             bufferSize - writeOffset,
                                             "%d",
                                             ( isBackground ? 100 : 90 ) + ( colorValue - 8 ) );
   }

   return writeOffset + (size_t)snprintf( ptrBuffer + writeOffset,
                                          bufferSize - writeOffset,
                                          "%d;5;%d",
                                          isBackground ? 48 : 38,
                                          colorValue );
}

static inline int formatAnsiColorSequence( char *ptrBuffer, size_t bufferSize,
                                           int colorValue, bool isBackground )
{
   size_t safeOffset;
   size_t writeOffset;

   if ( bufferSize == 0 )
   {
      return 0;
   }

   writeOffset = (size_t)snprintf( ptrBuffer, bufferSize, "\033[" );
   writeOffset = appendAnsiColorSelector( ptrBuffer, bufferSize, writeOffset,
                                          colorValue, isBackground );
   safeOffset = writeOffset < bufferSize ? writeOffset : bufferSize - 1;
   snprintf( ptrBuffer + safeOffset, writeOffset < bufferSize ? bufferSize - writeOffset : 0,
             "m" );
   return 1;
}

static inline int formatAnsiForegroundSequence( char *ptrBuffer, size_t bufferSize,
                                                int colorValue )
{
   return formatAnsiColorSequence( ptrBuffer, bufferSize, colorValue, false );
}

static inline int formatAnsiBackgroundSequence( char *ptrBuffer, size_t bufferSize,
                                                int colorValue )
{
   return formatAnsiColorSequence( ptrBuffer, bufferSize, colorValue, true );
}

static inline int formatAnsiDisplayStateSequence( char *ptrBuffer, size_t bufferSize,
                                                  int foregroundColor,
                                                  int backgroundColor,
                                                  bool shouldUseBold )
{
   size_t safeOffset;
   size_t writeOffset;

   if ( bufferSize == 0 )
   {
      return 0;
   }

   writeOffset = (size_t)snprintf( ptrBuffer, bufferSize, "\033[%d;",
                                   shouldUseBold ? 1 : 0 );
   writeOffset = appendAnsiColorSelector( ptrBuffer, bufferSize, writeOffset,
                                          foregroundColor, false );
   if ( writeOffset < bufferSize )
   {
      writeOffset += (size_t)snprintf( ptrBuffer + writeOffset,
                                       bufferSize - writeOffset,
                                       ";" );
   }
   else
   {
      writeOffset++;
   }
   writeOffset = appendAnsiColorSelector( ptrBuffer, bufferSize, writeOffset,
                                          backgroundColor, true );
   safeOffset = writeOffset < bufferSize ? writeOffset : bufferSize - 1;
   snprintf( ptrBuffer + safeOffset, writeOffset < bufferSize ? bufferSize - writeOffset : 0,
             "m" );
   return 1;
}

static inline int formatAnsiResetSequence( char *ptrBuffer, size_t bufferSize )
{
   if ( bufferSize == 0 )
   {
      return 0;
   }

   snprintf( ptrBuffer, bufferSize, "\033[0;39;49m" );
   return 1;
}

typedef struct
{
   int head;      // Index of current head
   int itemCount; // Number of objects queued
   int objsize;   // Size of one object
   int size;      // Number of objects queue can hold
   char *start;   // Pointer to beginning of queue
   int tail;      // Index of current tail
} queue;

#define CTRL_D 4
#define TAB 9
#define CTRL_R 18
#define CTRL_U 21
#define CTRL_W 23
#define CTRL_X 24
#define CTRL_Z 26
#define ESC 27
#define DEL 127

#define ASCII_PRINTABLE_MIN 32
#define ASCII_PRINTABLE_MAX 127
#define ALLOWED_INPUT_CONTROL_CHARS "\3\4\5\b\n\r\27\30\32"

#define BBS_HOSTNAME "bbs.iscabbs.com"
#define BBS_IP_ADDRESS "206.217.131.27"
#define BBS_PORT_NUMBER 23

// sendingXState defines
#define SX_WANT_TO 5
#define SENDING_X_STATE_SENT_COMMAND_X 1
#define SX_SENT_NAME 2
#define SX_REPLYING 3
#define SX_SEND_NEXT 8
#define SX_NOT 0

// Color transform defines
#define CX_NORMAL 0
#define CX_POST 1
#define CX_EXPRESS 2
#define CX_INFO 3 // not yet used

#define MAX_USER_NAME_HISTORY_COUNT 20
#define WINDOW_ROWS_DEFAULT 24
#define WINDOW_ROWS_MIN 5
#define WINDOW_ROWS_MAX 120
#define NAWS_ROWS_MIN 10
#define NAWS_ROWS_MAX 110
typedef struct
{
   unsigned int hasNameAutocompleteSetting : 1;   // true if name autocomplete was set in config
   unsigned int hasScreenReaderModeSetting : 1;   // true if screen reader mode was set in config
   unsigned int hasTitleBarSetting : 1;           // true if title-bar setting was set in config
   unsigned int isConfigMode : 1;                 // true in client config functions
   unsigned int isLastSave : 1;                   // true if last time aryUser edited they saved
   unsigned int isMorePromptActive : 1;           // true inside a MORE prompt
   unsigned int isPosting : 1;                    // true if aryUser is currently posting
   unsigned int isScreenReaderModeEnabled : 1;    // true if screen reader friendly mode is enabled
   unsigned int shouldAutoAnswerAnsiPrompt : 1;   // true when the ANSI prompt should be answered automatically
   unsigned int shouldCheckExpress : 1;           // true if waiting to check BBS for X's
   unsigned int shouldDisableBold : 1;            // true when bold ANSI output must be forced off
   unsigned int shouldEnableClickableUrls : 1;    // true if OSC-8 clickable URL output is enabled
   unsigned int shouldEnableNameAutocomplete : 1; // true if name-entry autocomplete is enabled
   unsigned int shouldEnableTitleBar : 1;         // true if terminal title updates are enabled
   unsigned int shouldUseKeychain : 1;            // true if runtime keychain support is enabled
   unsigned int shouldSquelchExpress : 1;         // true when enemy express messages should be squelched
   unsigned int shouldSquelchPost : 1;            // true when enemy posts should be squelched
   unsigned int shouldUseAnsi : 1;                // true if BBS is in ANSI color mode
   unsigned int shouldUseBold : 1;                // true if using bold in ANSI color mode
   unsigned int shouldUseTcpKeepalive : 1;        // true if TCP keepalive probes are enabled
} Flags;

typedef struct
{
   void **items;                                  // dynamic array containing item pointers
   unsigned int nitems;                           // number of items in list
   int ( *sortfn )( const void *, const void * ); // function to sort list; see slist.c
} slist;

typedef struct
{
   char info[54]; // Friend description
   int magic;     // Magic number
   char name[21]; // User name
   time_t time;   // Time online
} friend;         // User list entry

#define COLOR_FIELD_COUNT 24
#define COLOR_BACKGROUND_INDEX 17

typedef struct
{
   int text;                 // Plain text color
   int forum;                // Forum prompt color
   int number;               // Numbers and Read cmd prompt color
   int errorTextColor;       // Warning/error messages color
   int ansiBlackTextColor;   // Incoming ANSI black fallback color
   int ansiBlueTextColor;    // Incoming ANSI blue fallback color
   int ansiMagentaTextColor; // Incoming ANSI magenta fallback color
   int postDate;             // Post date stamp color
   int postName;             // Post author name color
   int postText;             // Post text color
   int postFriendDate;       // Post friend date stamp color
   int postFriendName;       // Post friend name color
   int postFriendText;       // Post friend text color
   int anonymous;            // Anonymous post header color
   int morePrompt;           // More prompt color
   int ansiWhiteTextColor;   // Incoming ANSI white fallback color
   int reserved5;
   int background;        // Background color
   int inputText;         // Text input fields
   int inputHighlight;    // Text input fields (highlight)
   int expressText;       // X message text color
   int expressName;       // X message name color
   int expressFriendText; // X message from friend text color
   int expressFriendName; // X message from friend name color
} Color;

#endif // DEFS_H_INCLUDED
