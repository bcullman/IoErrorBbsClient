/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config_file.h"
#include "browser.h"
#include "client.h"
#include <cmocka.h>
#include "color.h"
#include "config_menu.h"
#include "defs.h"
#include "edit.h"
#include "ext.h"
#include "filter.h"
#include "getline_input.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include "telnet.h"
#include "test_helpers.h"
#include "utility.h"
static int aryInputQueue[128];
static size_t inputCount;
static size_t inputIndex;
static unsigned int flushCount;
static unsigned int lastFlushValue;
static int lastDisplayStateBackground;
static int lastDisplayStateForeground;
static unsigned int printAnsiDisplayStateCallCount;
static char aryOutput[65536];
static size_t outputLength;

static void appendOutputCharacter( char outputChar )
{
   if ( outputLength >= sizeof( aryOutput ) - 1 )
   {
      return;
   }

   aryOutput[outputLength++] = outputChar;
   aryOutput[outputLength] = '\0';
}

static void appendOutputInteger( int value )
{
   char aryValue[32];

   snprintf( aryValue, sizeof( aryValue ), "%d", value );
   if ( outputLength >= sizeof( aryOutput ) - 1 )
   {
      return;
   }
   strncat( aryOutput, aryValue, sizeof( aryOutput ) - outputLength - 1 );
   outputLength = strlen( aryOutput );
}

static void appendOutputPaddedString( const char *ptrText, int width )
{
   size_t paddingCount;
   size_t textLength;

   textLength = strlen( ptrText );
   if ( outputLength < sizeof( aryOutput ) - 1 )
   {
      strncat( aryOutput, ptrText, sizeof( aryOutput ) - outputLength - 1 );
      outputLength = strlen( aryOutput );
   }
   if ( width <= 0 || (size_t)width <= textLength )
   {
      return;
   }
   paddingCount = (size_t)width - textLength;
   while ( paddingCount-- > 0 )
   {
      appendOutputCharacter( ' ' );
   }
}

static void appendOutputText( const char *ptrText )
{
   if ( outputLength >= sizeof( aryOutput ) - 1 )
   {
      return;
   }
   strncat( aryOutput, ptrText, sizeof( aryOutput ) - outputLength - 1 );
   outputLength = strlen( aryOutput );
}

static void resetState( void )
{
   inputCount = 0;
   inputIndex = 0;
   flushCount = 0;
   lastFlushValue = 0;
   lastDisplayStateBackground = -2;
   lastDisplayStateForeground = -2;
   printAnsiDisplayStateCallCount = 0;
   outputLength = 0;
   aryOutput[0] = '\0';

   flagsConfiguration.shouldUseAnsi = 0;
   configuredColorOutputMode = COLOR_OUTPUT_MODE_AUTO;
   useBlackThemeBackgrounds = false;
   lastColor = 0;
   unsetenv( "COLORTERM" );
   unsetenv( "TERM" );
   unsetenv( "TERM_PROGRAM" );

   if ( friendList != NULL )
   {
      slistDestroyItems( friendList );
      slistDestroy( friendList );
      friendList = NULL;
   }
}

static void setInputSequence( const int *aryKeys, size_t count )
{
   inputCount = copyIntArray( aryKeys, count, aryInputQueue, sizeof( aryInputQueue ) / sizeof( aryInputQueue[0] ) );
   inputIndex = 0;
}

static void addFriend( const char *ptrName )
{
   friend *ptrFriend;

   if ( friendList == NULL )
   {
      friendList = slistCreate( 0, fSortCompareVoid );
      if ( friendList == NULL )
      {
         fail_msg( "slistCreate failed while preparing friendList for color tests" );
         return;
      }
   }

   ptrFriend = calloc( 1, sizeof( friend ) );
   if ( ptrFriend == NULL )
   {
      fail_msg( "calloc failed while creating friend entry for color tests" );
      return;
   }
   ptrFriend->magic = 0x3231;
   snprintf( ptrFriend->name, sizeof( ptrFriend->name ), "%s", ptrName );
   snprintf( ptrFriend->info, sizeof( ptrFriend->info ), "%s", "Color test friend" );

   if ( !slistAddItem( friendList, ptrFriend, 1 ) )
   {
      free( ptrFriend );
      fail_msg( "slistAddItem failed while creating friendList for color tests" );
   }
}

// color.c dependencies outside the target behavior under test.
int colorize( const char *ptrText )
{
   (void)ptrText;
   return 1;
}

void printAnsiForegroundColorValue( int colorValue )
{
   appendOutputText( "<FG:" );
   appendOutputInteger( colorValue );
   appendOutputText( ">" );
}

void printAnsiBackgroundColorValue( int colorValue )
{
   appendOutputText( "<BG:" );
   appendOutputInteger( colorValue );
   appendOutputText( ">" );
}

void printAnsiDisplayStateValue( int foregroundColor, int backgroundColor )
{
   lastDisplayStateForeground = foregroundColor;
   lastDisplayStateBackground = backgroundColor;
   printAnsiDisplayStateCallCount++;
   appendOutputText( "<DS:" );
   appendOutputInteger( foregroundColor );
   appendOutputText( "," );
   appendOutputInteger( backgroundColor );
   appendOutputText( ">" );
}

void printThemedMnemonicText( const char *ptrText, int defaultColor )
{
   (void)defaultColor;
   appendOutputText( ptrText );
}

int fSortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   const friend *const *ptrLeftFriend;
   const friend *const *ptrRightFriend;

   ptrLeftFriend = ptrLeft;
   ptrRightFriend = ptrRight;
   return strcmp( ( *ptrLeftFriend )->name, ( *ptrRightFriend )->name );
}

int fStrCompareVoid( const void *ptrName, const void *ptrFriend )
{
   return strcmp( (const char *)ptrName, ( (const friend *)ptrFriend )->name );
}

char *findChar( const char *ptrString, int targetChar )
{
   return (char *)strchr( ptrString, targetChar );
}

char *findSubstring( const char *ptrString, const char *ptrSubstring )
{
   return (char *)strstr( ptrString, ptrSubstring );
}

void flushInput( unsigned int count )
{
   flushCount++;
   lastFlushValue = count;
}

void handleInvalidInput( unsigned int *ptrInvalidCount )
{
   if ( ( *ptrInvalidCount )++ )
   {
      flushInput( *ptrInvalidCount );
   }
}

int inKey( void )
{
   if ( inputIndex < inputCount )
   {
      return aryInputQueue[inputIndex++];
   }
   return '\n';
}

int readValidatedMenuKey( const char *allowedCharsLowercase )
{
   int inputChar;
   unsigned int invalid;

   invalid = 0;
   for ( ;; )
   {
      inputChar = inKey();
      if ( isalpha( inputChar ) )
      {
         inputChar = tolower( inputChar );
      }
      if ( findChar( allowedCharsLowercase, inputChar ) )
      {
         return inputChar;
      }
      handleInvalidInput( &invalid );
   }
}

int stdPrintf( const char *format, ... )
{
   va_list argList;
   const char *ptrText;
   int width;

   va_start( argList, format );
   if ( strchr( format, '%' ) == NULL )
   {
      appendOutputText( format );
   }
   else if ( strcmp( format, "%s" ) == 0 )
   {
      ptrText = va_arg( argList, const char * );
      appendOutputText( ptrText );
   }
   else if ( strcmp( format, "%-*s" ) == 0 )
   {
      width = va_arg( argList, int );
      ptrText = va_arg( argList, const char * );
      appendOutputPaddedString( ptrText, width );
   }
   else if ( strcmp( format, "%c" ) == 0 )
   {
      appendOutputCharacter( (char)va_arg( argList, int ) );
   }
   va_end( argList );
   return 1;
}

int stdPutChar( int inputChar )
{
   return inputChar;
}

int yesNoDefault( int defaultAnswer )
{
   return defaultAnswer;
}

static void defaultColors_WhenClearAllApplied_SetsKnownDefaults( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );
   color.background = 7;

   // Act
   defaultColors( 1 );

   // Assert
   if ( color.text != colorValueFromRgb( 0x5f, 0xff, 0x87 ) ||
        color.forum != colorValueFromRgb( 0xff, 0xd7, 0x5f ) ||
        color.number != colorValueFromRgb( 0x5f, 0xd7, 0xff ) ||
        color.errorTextColor != colorValueFromRgb( 0xff, 0x5f, 0x5f ) )
   {
      fail_msg( "defaultColors(1) did not set general default colors as expected" );
   }
   if ( color.background != 0 )
   {
      fail_msg( "defaultColors(1) should reset background to 0; got %d", color.background );
   }
   if ( color.postName != colorValueFromRgb( 0x5f, 0xd7, 0xff ) ||
        color.postFriendName != colorValueFromRgb( 0xff, 0x5f, 0x5f ) ||
        color.expressName != colorValueFromRgb( 0x5f, 0xff, 0x87 ) )
   {
      fail_msg( "defaultColors(1) did not set post/express defaults as expected" );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0x5f, 0xff, 0x87 ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0x5f, 0x87, 0xff ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xd7, 0x87, 0xff ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0xe4, 0xe4, 0xe4 ) )
   {
      fail_msg( "defaultColors(1) did not set full ANSI fallback colors as expected; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void colorConfig_WhenPresetChangesBackground_RefreshesDisplayStateImmediately( void **state )
{
   const int aryKeys[] = { 't', '6', 'q' };

   // Arrange
   (void)state;

   resetState();
   flagsConfiguration.shouldUseAnsi = 1;
   color.background = 0;
   color.text = 2;
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   colorConfig();

   // Assert
   if ( color.background != colorValueFromRgb( 0xef, 0xf1, 0xf5 ) )
   {
      fail_msg( "Latte preset should change the configured background to white; got %d",
                color.background );
   }
   if ( printAnsiDisplayStateCallCount == 0 )
   {
      fail_msg( "colorConfig should refresh the full display state after a preset change" );
   }
   if ( lastDisplayStateBackground != colorValueFromRgb( 0xef, 0xf1, 0xf5 ) )
   {
      fail_msg( "colorConfig should refresh the terminal background immediately after a preset change; got %d",
                lastDisplayStateBackground );
   }
   if ( lastDisplayStateForeground != color.text )
   {
      fail_msg( "colorConfig should refresh the terminal using the active text color; got %d expected %d",
                lastDisplayStateForeground, color.text );
   }
}

static void colorConfig_WhenPresetMenuShown_UsesLiveThemeTextAndPaletteSwatches( void **state )
{
   const int aryKeys[] = { 't', 'q' };
   char aryExpectedLabelMarker[32];
   const char *ptrDefaultLabel;
   const char *ptrEndOfRow;
   const char *ptrRightColumnLabel;
   const char *ptrSwatchStrip;

   // Arrange
   (void)state;

   resetState();
   flagsConfiguration.shouldUseAnsi = true;
   color.background = 0;
   color.number = 14;
   color.text = 42;
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );
   snprintf( aryExpectedLabelMarker, sizeof( aryExpectedLabelMarker ), "<FG:%d>Default", color.text );

   // Act
   colorConfig();

   // Assert
   ptrDefaultLabel = findSubstring( aryOutput, aryExpectedLabelMarker );
   if ( ptrDefaultLabel == NULL )
   {
      fail_msg( "theme preset labels should use the current live theme text color; expected marker '%s' in output '%s'",
                aryExpectedLabelMarker, aryOutput );
   }
   ptrSwatchStrip = findSubstring( ptrDefaultLabel, "<BG:0>  <DS:42,0><FG:42> <BG:" );
   if ( ptrSwatchStrip == NULL )
   {
      fail_msg( "theme preset menu should print a background-color swatch strip after each label; output was '%s'",
                aryOutput );
   }
   ptrRightColumnLabel = findSubstring( ptrDefaultLabel, "Gruvbox Light" );
   ptrEndOfRow = findSubstring( ptrDefaultLabel, "\r\n" );
   if ( ptrRightColumnLabel == NULL || ptrEndOfRow == NULL || ptrRightColumnLabel > ptrEndOfRow )
   {
      fail_msg( "theme preset menu should render a second preset in the same row for the two-column layout; output was '%s'",
                aryOutput );
   }
}

static void colorOptions_WhenColorOutputModeSelected_UpdatesConfiguredMode( void **state )
{
   const int aryKeys[] = { 't' };

   // Arrange
   (void)state;

   resetState();
   configuredColorOutputMode = COLOR_OUTPUT_MODE_AUTO;
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   colorOptions();

   // Assert
   if ( configuredColorOutputMode != COLOR_OUTPUT_MODE_TRUECOLOR )
   {
      fail_msg( "colorOptions should update color output mode when truecolor is selected; got %d",
                configuredColorOutputMode );
   }
}

static void defaultColors_WhenClearAllDisabled_LeavesBackgroundUnchanged( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );
   color.text = -1;
   color.background = 4;

   // Act
   defaultColors( 0 );

   // Assert
   if ( color.text != colorValueFromRgb( 0x5f, 0xff, 0x87 ) )
   {
      fail_msg( "defaultColors(0) should repair negative text color to the default RGB green; got %d", color.text );
   }
   if ( color.background != 4 )
   {
      fail_msg( "defaultColors(0) should not overwrite existing background; got %d", color.background );
   }
}

static void colorValueFromName_WhenCanonicalNameProvided_ReturnsNamedPaletteValue( void **state )
{
   int colorValue;

   // Arrange
   (void)state;

   resetState();

   // Act
   colorValue = colorValueFromName( "green" );

   // Assert
   if ( colorValue != 34 )
   {
      fail_msg( "colorValueFromName should map green to palette value 34; got %d", colorValue );
   }
}

static void colorValueFromName_WhenAliasProvided_ReturnsCanonicalPaletteValue( void **state )
{
   int colorValue;

   // Arrange
   (void)state;

   resetState();

   // Act
   colorValue = colorValueFromName( "Purple" );

   // Assert
   if ( colorValue != 91 )
   {
      fail_msg( "colorValueFromName should map Purple to palette value 91; got %d", colorValue );
   }
}

static void colorValueFromName_WhenBrightAnsiNameProvided_ReturnsBrightAnsiValue( void **state )
{
   int colorValue;

   // Arrange
   (void)state;

   resetState();

   // Act
   colorValue = colorValueFromName( "BrightBlue" );

   // Assert
   if ( colorValue != 12 )
   {
      fail_msg( "colorValueFromName should map BrightBlue to ANSI 16 value 12; got %d", colorValue );
   }
}

static void colorValueFromName_WhenNameUnknown_ReturnsInvalidSentinel( void **state )
{
   int colorValue;

   // Arrange
   (void)state;

   resetState();

   // Act
   colorValue = colorValueFromName( "chartreuse" );

   // Assert
   if ( colorValue != -1 )
   {
      fail_msg( "colorValueFromName should reject unknown names with -1; got %d", colorValue );
   }
}

static void colorNameFromValue_WhenPaletteValueMatchesAlias_ReturnsCanonicalName( void **state )
{
   const char *ptrColorName;

   // Arrange
   (void)state;

   resetState();

   // Act
   ptrColorName = colorNameFromValue( 91 );

   // Assert
   if ( ptrColorName == NULL || strcmp( ptrColorName, "magenta" ) != 0 )
   {
      fail_msg( "colorNameFromValue should return canonical name 'magenta' for value 91; got '%s'",
                ptrColorName == NULL ? "(null)" : ptrColorName );
   }
}

static void colorNameFromValue_WhenBrightAnsiValueProvided_ReturnsBrightCanonicalName( void **state )
{
   const char *ptrColorName;

   // Arrange
   (void)state;

   resetState();

   // Act
   ptrColorName = colorNameFromValue( 13 );

   // Assert
   if ( ptrColorName == NULL || strcmp( ptrColorName, "brightmagenta" ) != 0 )
   {
      fail_msg( "colorNameFromValue should return canonical name 'brightmagenta' for value 13; got '%s'",
                ptrColorName == NULL ? "(null)" : ptrColorName );
   }
}

static void colorNameFromValue_WhenPaletteValueUnknown_ReturnsNull( void **state )
{
   const char *ptrColorName;

   // Arrange
   (void)state;

   resetState();

   // Act
   ptrColorName = colorNameFromValue( 999 );

   // Assert
   if ( ptrColorName != NULL )
   {
      fail_msg( "colorNameFromValue should return NULL for unknown values; got '%s'", ptrColorName );
   }
}

static void formatAnsiForegroundSequence_WhenClassicColorRequested_UsesClassicAnsiCode( void **state )
{
   char arySequence[32];

   // Arrange
   (void)state;

   resetState();

   // Act
   formatAnsiForegroundSequence( arySequence, sizeof( arySequence ), 2 );

   // Assert
   if ( strcmp( arySequence, "\033[32m" ) != 0 )
   {
      fail_msg( "formatAnsiForegroundSequence should encode classic green as '\\033[32m'; got '%s'",
                arySequence );
   }
}

static void formatAnsiForegroundSequence_WhenBrightColorRequested_UsesBrightAnsiCode( void **state )
{
   char arySequence[32];

   // Arrange
   (void)state;

   resetState();

   // Act
   formatAnsiForegroundSequence( arySequence, sizeof( arySequence ), 10 );

   // Assert
   if ( strcmp( arySequence, "\033[92m" ) != 0 )
   {
      fail_msg( "formatAnsiForegroundSequence should encode bright green as '\\033[92m'; got '%s'",
                arySequence );
   }
}

static void formatAnsiForegroundSequence_WhenExtendedColorRequested_Uses256ColorCode( void **state )
{
   char arySequence[32];

   // Arrange
   (void)state;

   resetState();

   // Act
   formatAnsiForegroundSequence( arySequence, sizeof( arySequence ), 34 );

   // Assert
   if ( strcmp( arySequence, "\033[38;5;34m" ) != 0 )
   {
      fail_msg( "formatAnsiForegroundSequence should encode extended palette value 34 as '\\033[38;5;34m'; got '%s'",
                arySequence );
   }
}

static void formatAnsiDisplayStateSequence_WhenDefaultBackgroundRequested_UsesCombinedSelectors( void **state )
{
   char arySequence[32];

   // Arrange
   (void)state;

   resetState();

   // Act
   formatAnsiDisplayStateSequence( arySequence, sizeof( arySequence ), 7,
                                   COLOR_VALUE_DEFAULT, false );

   // Assert
   if ( strcmp( arySequence, "\033[0;37;49m" ) != 0 )
   {
      fail_msg( "formatAnsiDisplayStateSequence should encode white-on-default without bold as '\\033[0;37;49m'; got '%s'",
                arySequence );
   }
}

static void formatAnsiForegroundSequence_WhenRgbColorAndTruecolorEnabled_Uses24BitCode( void **state )
{
   char arySequence[32];

   // Arrange
   (void)state;

   resetState();
   configuredColorOutputMode = COLOR_OUTPUT_MODE_TRUECOLOR;

   // Act
   formatAnsiForegroundSequence( arySequence, sizeof( arySequence ),
                                 colorValueFromRgb( 0x8a, 0xad, 0xf4 ) );

   // Assert
   if ( strcmp( arySequence, "\033[38;2;138;173;244m" ) != 0 )
   {
      fail_msg( "formatAnsiForegroundSequence should emit truecolor foreground escapes for RGB values; got '%s'",
                arySequence );
   }
}

static void formatAnsiBackgroundSequence_WhenRgbColorAndAutoModeInAppleTerminal_Uses256Fallback( void **state )
{
   char aryExpected[32];
   char arySequence[32];
   int fallbackColor;

   // Arrange
   (void)state;

   resetState();
   setenv( "TERM_PROGRAM", "Apple_Terminal", 1 );
   fallbackColor = xterm256ValueFromRgb( 0x8a, 0xad, 0xf4 );
   snprintf( aryExpected, sizeof( aryExpected ), "\033[48;5;%dm", fallbackColor );

   // Act
   formatAnsiBackgroundSequence( arySequence, sizeof( arySequence ),
                                 colorValueFromRgb( 0x8a, 0xad, 0xf4 ) );

   // Assert
   if ( strcmp( arySequence, aryExpected ) != 0 )
   {
      fail_msg( "formatAnsiBackgroundSequence should downgrade RGB backgrounds for Apple Terminal; expected '%s' got '%s'",
                aryExpected, arySequence );
   }
}

static void formatAnsiBackgroundSequence_WhenDarkThemeFallbackEnabled_UsesBlackIn256Mode( void **state )
{
   char arySequence[32];

   // Arrange
   (void)state;

   resetState();
   useBlackThemeBackgrounds = true;
   setenv( "TERM_PROGRAM", "Apple_Terminal", 1 );

   // Act
   formatAnsiBackgroundSequence( arySequence, sizeof( arySequence ),
                                 colorValueFromRgb( 0x24, 0x27, 0x3a ) );

   // Assert
   if ( strcmp( arySequence, "\033[40m" ) != 0 )
   {
      fail_msg( "formatAnsiBackgroundSequence should force black for dark-theme RGB backgrounds in non-truecolor output; got '%s'",
                arySequence );
   }
}

static void formatAnsiForegroundSequence_WhenRgbColorAndAutoModeWithTruecolorTerminal_Uses24BitCode( void **state )
{
   char arySequence[32];

   // Arrange
   (void)state;

   resetState();
   setenv( "COLORTERM", "truecolor", 1 );

   // Act
   formatAnsiForegroundSequence( arySequence, sizeof( arySequence ),
                                 colorValueFromRgb( 0x8a, 0xad, 0xf4 ) );

   // Assert
   if ( strcmp( arySequence, "\033[38;2;138;173;244m" ) != 0 )
   {
      fail_msg( "formatAnsiForegroundSequence should auto-detect truecolor terminals from COLORTERM; got '%s'",
                arySequence );
   }
}

static void formatAnsiForegroundSequence_WhenRgbColorAnd256ModeRequested_IgnoresTruecolorTerminal( void **state )
{
   char aryExpected[32];
   char arySequence[32];
   int fallbackColor;

   // Arrange
   (void)state;

   resetState();
   configuredColorOutputMode = COLOR_OUTPUT_MODE_256;
   setenv( "COLORTERM", "truecolor", 1 );
   fallbackColor = xterm256ValueFromRgb( 0x8a, 0xad, 0xf4 );
   snprintf( aryExpected, sizeof( aryExpected ), "\033[38;5;%dm", fallbackColor );

   // Act
   formatAnsiForegroundSequence( arySequence, sizeof( arySequence ),
                                 colorValueFromRgb( 0x8a, 0xad, 0xf4 ) );

   // Assert
   if ( strcmp( arySequence, aryExpected ) != 0 )
   {
      fail_msg( "formatAnsiForegroundSequence should honor explicit 256-color mode over terminal truecolor hints; expected '%s' got '%s'",
                aryExpected, arySequence );
   }
}

static void formatAnsiDisplayStateSequence_WhenRgbColorsRequested_UsesCombinedTruecolorSelectors( void **state )
{
   char arySequence[64];

   // Arrange
   (void)state;

   resetState();
   configuredColorOutputMode = COLOR_OUTPUT_MODE_TRUECOLOR;

   // Act
   formatAnsiDisplayStateSequence( arySequence, sizeof( arySequence ),
                                   colorValueFromRgb( 0xca, 0xd3, 0xf5 ),
                                   colorValueFromRgb( 0x24, 0x27, 0x3a ),
                                   true );

   // Assert
   if ( strcmp( arySequence, "\033[1;38;2;202;211;245;48;2;36;39;58m" ) != 0 )
   {
      fail_msg( "formatAnsiDisplayStateSequence should emit combined truecolor state; got '%s'",
                arySequence );
   }
}

static void colorblindColors_WhenApplied_SetsAccessiblePalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   colorblindColors();

   // Assert
   if ( color.text != colorValueFromRgb( 0xff, 0xff, 0xff ) ||
        color.forum != colorValueFromRgb( 0x5f, 0xaf, 0xff ) ||
        color.number != colorValueFromRgb( 0xff, 0xaf, 0x00 ) ||
        color.errorTextColor != colorValueFromRgb( 0xd7, 0x5f, 0x00 ) )
   {
      fail_msg( "colorblindColors should set general accessible colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != 0 )
   {
      fail_msg( "colorblindColors should keep a dark background; got %d", color.background );
   }
   if ( color.postDate != colorValueFromRgb( 0x5f, 0xaf, 0xff ) ||
        color.postFriendDate != colorValueFromRgb( 0x00, 0x5f, 0xaf ) ||
        color.postName != colorValueFromRgb( 0xff, 0xaf, 0x00 ) ||
        color.postFriendName != colorValueFromRgb( 0xd7, 0x87, 0xaf ) ||
        color.inputHighlight != colorValueFromRgb( 0xff, 0xaf, 0x00 ) ||
        color.morePrompt != colorValueFromRgb( 0xff, 0xd7, 0x5f ) ||
        color.expressName != colorValueFromRgb( 0xff, 0xaf, 0x00 ) ||
        color.expressFriendName != colorValueFromRgb( 0xd7, 0x87, 0xaf ) )
   {
      fail_msg( "colorblindColors should map post, input, and express roles onto the preset palette; got postDate=%d frienddate=%d postName=%d friendname=%d inputHighlight=%d morePrompt=%d expressName=%d expressFriendName=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.inputHighlight, color.morePrompt,
                color.expressName, color.expressFriendName );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0xaf, 0xaf, 0xd7 ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0x5f, 0xaf, 0xff ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xd7, 0x87, 0xaf ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0xff, 0xd7, 0x5f ) )
   {
      fail_msg( "colorblindColors should theme the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void brilliantColors_WhenApplied_SetsBrightDefaultPalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   brilliantColors();

   // Assert
   if ( color.text != colorValueFromRgb( 0x00, 0xff, 0x00 ) ||
        color.forum != colorValueFromRgb( 0xff, 0xff, 0x00 ) ||
        color.number != colorValueFromRgb( 0x00, 0xff, 0xff ) ||
        color.errorTextColor != colorValueFromRgb( 0xff, 0x00, 0x00 ) )
   {
      fail_msg( "brilliantColors should set bright general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != 0 )
   {
      fail_msg( "brilliantColors should keep a black background; got %d", color.background );
   }
   if ( color.postDate != colorValueFromRgb( 0xff, 0x00, 0xff ) ||
        color.postFriendDate != colorValueFromRgb( 0xff, 0x00, 0xff ) ||
        color.postName != colorValueFromRgb( 0x00, 0xff, 0xff ) ||
        color.postFriendName != colorValueFromRgb( 0xff, 0x00, 0x00 ) ||
        color.postText != colorValueFromRgb( 0x00, 0xff, 0x00 ) ||
        color.postFriendText != colorValueFromRgb( 0x00, 0xff, 0x00 ) ||
        color.anonymous != colorValueFromRgb( 0xff, 0xff, 0x00 ) ||
        color.morePrompt != colorValueFromRgb( 0xff, 0xff, 0x00 ) ||
        color.inputText != colorValueFromRgb( 0x00, 0xff, 0x00 ) ||
        color.inputHighlight != colorValueFromRgb( 0x00, 0xff, 0xff ) ||
        color.expressText != colorValueFromRgb( 0x00, 0xff, 0x00 ) ||
        color.expressName != colorValueFromRgb( 0x00, 0xff, 0x00 ) ||
        color.expressFriendName != colorValueFromRgb( 0x00, 0xff, 0x00 ) ||
        color.expressFriendText != colorValueFromRgb( 0x00, 0xff, 0x00 ) )
   {
      fail_msg( "brilliantColors should map the default roles onto bright RGB values; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0x00, 0xff, 0x00 ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0x00, 0x00, 0xff ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xff, 0x00, 0xff ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0xff, 0xff, 0xff ) )
   {
      fail_msg( "brilliantColors should theme the full incoming ANSI palette with bright RGB values; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void catppuccinLatteColors_WhenApplied_SetsLightPalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   catppuccinLatteColors();

   // Assert
   if ( color.text != colorValueFromRgb( 0x4c, 0x4f, 0x69 ) ||
        color.forum != colorValueFromRgb( 0x1e, 0x66, 0xf5 ) ||
        color.number != colorValueFromRgb( 0x20, 0x9f, 0xb5 ) ||
        color.errorTextColor != colorValueFromRgb( 0xd2, 0x0f, 0x39 ) )
   {
      fail_msg( "catppuccinLatteColors should set the light palette general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != colorValueFromRgb( 0xef, 0xf1, 0xf5 ) )
   {
      fail_msg( "catppuccinLatteColors should use a light background; got %d", color.background );
   }
   if ( color.postDate != colorValueFromRgb( 0x20, 0x9f, 0xb5 ) ||
        color.postFriendDate != colorValueFromRgb( 0x17, 0x92, 0x99 ) ||
        color.postName != colorValueFromRgb( 0x72, 0x87, 0xfd ) ||
        color.postFriendName != colorValueFromRgb( 0x40, 0xa0, 0x2b ) ||
        color.postText != colorValueFromRgb( 0x4c, 0x4f, 0x69 ) ||
        color.postFriendText != colorValueFromRgb( 0x4c, 0x4f, 0x69 ) ||
        color.anonymous != colorValueFromRgb( 0xfe, 0x64, 0x0b ) ||
        color.morePrompt != colorValueFromRgb( 0xdf, 0x8e, 0x1d ) ||
        color.inputText != colorValueFromRgb( 0x4c, 0x4f, 0x69 ) ||
        color.inputHighlight != colorValueFromRgb( 0x1e, 0x66, 0xf5 ) ||
        color.expressText != colorValueFromRgb( 0x4c, 0x4f, 0x69 ) ||
        color.expressName != colorValueFromRgb( 0x1e, 0x66, 0xf5 ) ||
        color.expressFriendName != colorValueFromRgb( 0x40, 0xa0, 0x2b ) ||
        color.expressFriendText != colorValueFromRgb( 0x4c, 0x4f, 0x69 ) )
   {
      fail_msg( "catppuccinLatteColors should map posts, prompts, input, and express roles onto the light palette; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0x9c, 0xa0, 0xb0 ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0x1e, 0x66, 0xf5 ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xea, 0x76, 0xcb ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0x5c, 0x5f, 0x77 ) )
   {
      fail_msg( "catppuccinLatteColors should theme the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void catppuccinMacchiatoColors_WhenApplied_SetsDarkPalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   catppuccinMacchiatoColors();

   // Assert
   if ( color.text != colorValueFromRgb( 0xca, 0xd3, 0xf5 ) ||
        color.forum != colorValueFromRgb( 0x8a, 0xad, 0xf4 ) ||
        color.number != colorValueFromRgb( 0x7d, 0xc4, 0xe4 ) ||
        color.errorTextColor != colorValueFromRgb( 0xed, 0x87, 0x96 ) )
   {
      fail_msg( "catppuccinMacchiatoColors should set the dark palette general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != colorValueFromRgb( 0x24, 0x27, 0x3a ) )
   {
      fail_msg( "catppuccinMacchiatoColors should use the original Macchiato background; got %d", color.background );
   }
   if ( !useBlackThemeBackgrounds )
   {
      fail_msg( "catppuccinMacchiatoColors should enable black background fallback in non-truecolor output" );
   }
   if ( color.postDate != colorValueFromRgb( 0x7d, 0xc4, 0xe4 ) ||
        color.postFriendDate != colorValueFromRgb( 0x8b, 0xd5, 0xca ) ||
        color.postName != colorValueFromRgb( 0xb7, 0xbd, 0xf8 ) ||
        color.postFriendName != colorValueFromRgb( 0xa6, 0xda, 0x95 ) ||
        color.postText != colorValueFromRgb( 0xca, 0xd3, 0xf5 ) ||
        color.postFriendText != colorValueFromRgb( 0xca, 0xd3, 0xf5 ) ||
        color.anonymous != colorValueFromRgb( 0xf5, 0xa9, 0x7f ) ||
        color.morePrompt != colorValueFromRgb( 0xee, 0xd4, 0x9f ) ||
        color.inputText != colorValueFromRgb( 0xca, 0xd3, 0xf5 ) ||
        color.inputHighlight != colorValueFromRgb( 0x8a, 0xad, 0xf4 ) ||
        color.expressText != colorValueFromRgb( 0xca, 0xd3, 0xf5 ) ||
        color.expressName != colorValueFromRgb( 0xb7, 0xbd, 0xf8 ) ||
        color.expressFriendName != colorValueFromRgb( 0xa6, 0xda, 0x95 ) ||
        color.expressFriendText != colorValueFromRgb( 0xca, 0xd3, 0xf5 ) )
   {
      fail_msg( "catppuccinMacchiatoColors should map posts, prompts, input, and express roles onto the dark palette; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0x80, 0x84, 0x9d ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0x8a, 0xad, 0xf4 ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xc6, 0xa0, 0xf6 ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0xca, 0xd3, 0xf5 ) )
   {
      fail_msg( "catppuccinMacchiatoColors should theme the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void everforestDarkColors_WhenApplied_SetsDarkPalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   everforestDarkColors();

   // Assert
   if ( color.text != colorValueFromRgb( 0xd3, 0xc6, 0xaa ) ||
        color.forum != colorValueFromRgb( 0x7f, 0xbb, 0xb3 ) ||
        color.number != colorValueFromRgb( 0x83, 0xc0, 0x92 ) ||
        color.errorTextColor != colorValueFromRgb( 0xe6, 0x7e, 0x80 ) )
   {
      fail_msg( "everforestDarkColors should set the dark palette general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != colorValueFromRgb( 0x2f, 0x38, 0x3e ) )
   {
      fail_msg( "everforestDarkColors should use the original Everforest dark medium background; got %d", color.background );
   }
   if ( !useBlackThemeBackgrounds )
   {
      fail_msg( "everforestDarkColors should enable black background fallback in non-truecolor output" );
   }
   if ( color.postDate != colorValueFromRgb( 0x83, 0xc0, 0x92 ) ||
        color.postFriendDate != colorValueFromRgb( 0x7f, 0xbb, 0xb3 ) ||
        color.postName != colorValueFromRgb( 0xa7, 0xc0, 0x80 ) ||
        color.postFriendName != colorValueFromRgb( 0xdb, 0xbc, 0x7f ) ||
        color.postText != colorValueFromRgb( 0xd3, 0xc6, 0xaa ) ||
        color.postFriendText != colorValueFromRgb( 0xd3, 0xc6, 0xaa ) ||
        color.anonymous != colorValueFromRgb( 0xe6, 0x98, 0x75 ) ||
        color.morePrompt != colorValueFromRgb( 0xdb, 0xbc, 0x7f ) ||
        color.inputText != colorValueFromRgb( 0xd3, 0xc6, 0xaa ) ||
        color.inputHighlight != colorValueFromRgb( 0x7f, 0xbb, 0xb3 ) ||
        color.expressText != colorValueFromRgb( 0xd3, 0xc6, 0xaa ) ||
        color.expressName != colorValueFromRgb( 0xa7, 0xc0, 0x80 ) ||
        color.expressFriendName != colorValueFromRgb( 0xdb, 0xbc, 0x7f ) ||
        color.expressFriendText != colorValueFromRgb( 0xd3, 0xc6, 0xaa ) )
   {
      fail_msg( "everforestDarkColors should map posts, prompts, input, and express roles onto the dark palette; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0x85, 0x92, 0x89 ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0x7f, 0xbb, 0xb3 ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xd6, 0x99, 0xb6 ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0xe5, 0xdd, 0xc9 ) )
   {
      fail_msg( "everforestDarkColors should theme the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void everforestLightColors_WhenApplied_SetsLightPalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   everforestLightColors();

   // Assert
   if ( color.text != colorValueFromRgb( 0x5c, 0x6a, 0x72 ) ||
        color.forum != colorValueFromRgb( 0x35, 0x8f, 0xa2 ) ||
        color.number != colorValueFromRgb( 0x3a, 0x94, 0x84 ) ||
        color.errorTextColor != colorValueFromRgb( 0xf8, 0x55, 0x52 ) )
   {
      fail_msg( "everforestLightColors should set the light palette general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != colorValueFromRgb( 0xfd, 0xf6, 0xe3 ) )
   {
      fail_msg( "everforestLightColors should use a light background; got %d", color.background );
   }
   if ( color.postDate != colorValueFromRgb( 0x3a, 0x94, 0x84 ) ||
        color.postFriendDate != colorValueFromRgb( 0x35, 0x8f, 0xa2 ) ||
        color.postName != colorValueFromRgb( 0x8d, 0xb8, 0x61 ) ||
        color.postFriendName != colorValueFromRgb( 0xda, 0xa5, 0x20 ) ||
        color.postText != colorValueFromRgb( 0x5c, 0x6a, 0x72 ) ||
        color.postFriendText != colorValueFromRgb( 0x5c, 0x6a, 0x72 ) ||
        color.anonymous != colorValueFromRgb( 0xf5, 0x7d, 0x26 ) ||
        color.morePrompt != colorValueFromRgb( 0xbf, 0x98, 0x3d ) ||
        color.inputText != colorValueFromRgb( 0x5c, 0x6a, 0x72 ) ||
        color.inputHighlight != colorValueFromRgb( 0x35, 0x8f, 0xa2 ) ||
        color.expressText != colorValueFromRgb( 0x5c, 0x6a, 0x72 ) ||
        color.expressName != colorValueFromRgb( 0x8d, 0xb8, 0x61 ) ||
        color.expressFriendName != colorValueFromRgb( 0xda, 0xa5, 0x20 ) ||
        color.expressFriendText != colorValueFromRgb( 0x5c, 0x6a, 0x72 ) )
   {
      fail_msg( "everforestLightColors should map posts, prompts, input, and express roles onto the light palette; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0xa6, 0xb0, 0x9f ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0x35, 0x8f, 0xa2 ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xdf, 0x69, 0xba ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0x4f, 0x5b, 0x58 ) )
   {
      fail_msg( "everforestLightColors should theme the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void hotDogColors_WhenApplied_SetsClassicHotDogPalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   hotDogColors();

   // Assert
   if ( color.text != colorValueFromRgb( 0xff, 0xd7, 0x00 ) ||
        color.forum != colorValueFromRgb( 0xff, 0x00, 0x00 ) ||
        color.number != colorValueFromRgb( 0xff, 0xd7, 0x00 ) ||
        color.errorTextColor != colorValueFromRgb( 0xff, 0xff, 0xff ) )
   {
      fail_msg( "hotDogColors should set general hot dog colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != 0 )
   {
      fail_msg( "hotDogColors should keep a black background; got %d", color.background );
   }
   if ( color.postText != colorValueFromRgb( 0xff, 0xaf, 0x00 ) ||
        color.postFriendText != colorValueFromRgb( 0xff, 0xaf, 0x00 ) ||
        color.expressText != colorValueFromRgb( 0xff, 0xaf, 0x00 ) ||
        color.expressFriendText != colorValueFromRgb( 0xff, 0xaf, 0x00 ) )
   {
      fail_msg( "hotDogColors should keep only post and eXpress bodies orange; got postText=%d friendposttext=%d expressText=%d friendexpresstext=%d",
                color.postText, color.postFriendText, color.expressText,
                color.expressFriendText );
   }

   if ( color.postDate != colorValueFromRgb( 0xff, 0xff, 0x00 ) ||
        color.postFriendDate != colorValueFromRgb( 0xff, 0xff, 0x00 ) ||
        color.postName != colorValueFromRgb( 0xff, 0xff, 0x00 ) ||
        color.postFriendName != colorValueFromRgb( 0xff, 0xff, 0x00 ) ||
        color.anonymous != colorValueFromRgb( 0xff, 0xff, 0x00 ) ||
        color.morePrompt != colorValueFromRgb( 0xff, 0xd7, 0x00 ) ||
        color.inputText != colorValueFromRgb( 0xff, 0xd7, 0x00 ) ||
        color.expressName != colorValueFromRgb( 0xff, 0xff, 0x00 ) ||
        color.expressFriendName != colorValueFromRgb( 0xff, 0xff, 0x00 ) )
   {
      fail_msg( "hotDogColors should keep date and name headers yellow while leaving only bodies orange; got postDate=%d frienddate=%d postName=%d friendname=%d anonymous=%d morePrompt=%d inputText=%d expressName=%d expressFriendName=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.anonymous, color.morePrompt,
                color.inputText, color.expressName, color.expressFriendName );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0xaf, 0x5f, 0x00 ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0xff, 0xaf, 0x00 ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xaf, 0x5f, 0x00 ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0xff, 0xd7, 0x00 ) )
   {
      fail_msg( "hotDogColors should eliminate stray gray and purple by theming the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void gruvboxDarkColors_WhenApplied_SetsDarkPalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   gruvboxDarkColors();

   // Assert
   if ( color.text != colorValueFromRgb( 0xeb, 0xdb, 0xb2 ) ||
        color.forum != colorValueFromRgb( 0x83, 0xa5, 0x98 ) ||
        color.number != colorValueFromRgb( 0x8e, 0xc0, 0x7c ) ||
        color.errorTextColor != colorValueFromRgb( 0xfe, 0x80, 0x19 ) )
   {
      fail_msg( "gruvboxDarkColors should set the dark palette general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != colorValueFromRgb( 0x1d, 0x20, 0x21 ) )
   {
      fail_msg( "gruvboxDarkColors should use the original Gruvbox dark hard background; got %d", color.background );
   }
   if ( !useBlackThemeBackgrounds )
   {
      fail_msg( "gruvboxDarkColors should enable black background fallback in non-truecolor output" );
   }
   if ( color.postDate != colorValueFromRgb( 0x83, 0xa5, 0x98 ) ||
        color.postFriendDate != colorValueFromRgb( 0xd3, 0x86, 0x9b ) ||
        color.postName != colorValueFromRgb( 0xb8, 0xbb, 0x26 ) ||
        color.postFriendName != colorValueFromRgb( 0x8e, 0xc0, 0x7c ) ||
        color.postText != colorValueFromRgb( 0xeb, 0xdb, 0xb2 ) ||
        color.postFriendText != colorValueFromRgb( 0xeb, 0xdb, 0xb2 ) ||
        color.anonymous != colorValueFromRgb( 0xfe, 0x80, 0x19 ) ||
        color.morePrompt != colorValueFromRgb( 0xfa, 0xbd, 0x2f ) ||
        color.inputText != colorValueFromRgb( 0xeb, 0xdb, 0xb2 ) ||
        color.inputHighlight != colorValueFromRgb( 0x83, 0xa5, 0x98 ) ||
        color.expressText != colorValueFromRgb( 0xeb, 0xdb, 0xb2 ) ||
        color.expressName != colorValueFromRgb( 0xb8, 0xbb, 0x26 ) ||
        color.expressFriendName != colorValueFromRgb( 0x8e, 0xc0, 0x7c ) ||
        color.expressFriendText != colorValueFromRgb( 0xeb, 0xdb, 0xb2 ) )
   {
      fail_msg( "gruvboxDarkColors should map posts, prompts, input, and express roles onto the dark palette; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0x92, 0x83, 0x74 ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0x83, 0xa5, 0x98 ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xd3, 0x86, 0x9b ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0xfb, 0xf1, 0xc7 ) )
   {
      fail_msg( "gruvboxDarkColors should theme the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void gruvboxLightColors_WhenApplied_SetsLightPalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   gruvboxLightColors();

   // Assert
   if ( color.text != colorValueFromRgb( 0x3c, 0x38, 0x36 ) ||
        color.forum != colorValueFromRgb( 0x45, 0x85, 0x88 ) ||
        color.number != colorValueFromRgb( 0x68, 0x9d, 0x6a ) ||
        color.errorTextColor != colorValueFromRgb( 0xd6, 0x5d, 0x0e ) )
   {
      fail_msg( "gruvboxLightColors should set the light palette general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != colorValueFromRgb( 0xf9, 0xf5, 0xd7 ) )
   {
      fail_msg( "gruvboxLightColors should use a light background; got %d", color.background );
   }
   if ( color.postDate != colorValueFromRgb( 0x45, 0x85, 0x88 ) ||
        color.postFriendDate != colorValueFromRgb( 0xb1, 0x62, 0x86 ) ||
        color.postName != colorValueFromRgb( 0x79, 0x74, 0x0e ) ||
        color.postFriendName != colorValueFromRgb( 0x68, 0x9d, 0x6a ) ||
        color.postText != colorValueFromRgb( 0x3c, 0x38, 0x36 ) ||
        color.postFriendText != colorValueFromRgb( 0x3c, 0x38, 0x36 ) ||
        color.anonymous != colorValueFromRgb( 0xaf, 0x3a, 0x03 ) ||
        color.morePrompt != colorValueFromRgb( 0xd7, 0x99, 0x21 ) ||
        color.inputText != colorValueFromRgb( 0x3c, 0x38, 0x36 ) ||
        color.inputHighlight != colorValueFromRgb( 0x45, 0x85, 0x88 ) ||
        color.expressText != colorValueFromRgb( 0x3c, 0x38, 0x36 ) ||
        color.expressName != colorValueFromRgb( 0x79, 0x74, 0x0e ) ||
        color.expressFriendName != colorValueFromRgb( 0x68, 0x9d, 0x6a ) ||
        color.expressFriendText != colorValueFromRgb( 0x3c, 0x38, 0x36 ) )
   {
      fail_msg( "gruvboxLightColors should map posts, prompts, input, and express roles onto the light palette; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != colorValueFromRgb( 0xa8, 0x99, 0x84 ) ||
        color.ansiBlueTextColor != colorValueFromRgb( 0x45, 0x85, 0x88 ) ||
        color.ansiMagentaTextColor != colorValueFromRgb( 0xb1, 0x62, 0x86 ) ||
        color.ansiWhiteTextColor != colorValueFromRgb( 0x28, 0x28, 0x28 ) )
   {
      fail_msg( "gruvboxLightColors should theme the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void ansiTransform_WhenHotDogPaletteApplied_UsesThemeColorsForAllAnsiDigits( void **state )
{
   int transformedBlack;
   int transformedBlue;
   int transformedMagenta;
   int transformedWhite;

   // Arrange
   (void)state;

   resetState();
   hotDogColors();

   // Act
   transformedBlack = ansiTransform( '0' );
   transformedBlue = ansiTransform( '4' );
   transformedMagenta = ansiTransform( '5' );
   transformedWhite = ansiTransform( '7' );

   // Assert
   if ( transformedBlack != colorValueFromRgb( 0xaf, 0x5f, 0x00 ) ||
        transformedBlue != colorValueFromRgb( 0xff, 0xaf, 0x00 ) ||
        transformedMagenta != colorValueFromRgb( 0xaf, 0x5f, 0x00 ) ||
        transformedWhite != colorValueFromRgb( 0xff, 0xd7, 0x00 ) )
   {
      fail_msg( "ansiTransform should map all incoming ANSI digits through the active Hotdog stand palette; got black=%d blue=%d magenta=%d white=%d",
                transformedBlack, transformedBlue, transformedMagenta,
                transformedWhite );
   }
}

static void ansiTransformExpress_WhenFriendSender_UsesFriendColorCodes( void **state )
{
   // Arrange
   char aryMessage[256];

   (void)state;

   resetState();
   flagsConfiguration.shouldUseAnsi = 1;
   color.expressFriendText = 3;
   color.expressFriendName = 5;
   color.text = 7;
   color.expressText = 2;
   color.expressName = 6;
   addFriend( "Dr Strange" );
   snprintf( aryMessage, sizeof( aryMessage ), "%s", "*** Message (#1) from Dr Strange at 11:01 ***" );

   // Act
   ansiTransformExpress( aryMessage, sizeof( aryMessage ) );

   // Assert
   if ( strstr( aryMessage, "\033[33m" ) == NULL || strstr( aryMessage, "\033[35m" ) == NULL )
   {
      fail_msg( "friend X message should use friend express color codes; got '%s'", aryMessage );
   }
   if ( lastColor != color.text )
   {
      fail_msg( "ansiTransformExpress should set lastColor to text color; got %d", lastColor );
   }
}

static void ansiTransformExpress_WhenAnsiDisabled_LeavesTextUnchanged( void **state )
{
   // Arrange
   char aryMessage[256];
   char aryOriginal[256];

   (void)state;

   resetState();
   flagsConfiguration.shouldUseAnsi = 0;
   snprintf( aryMessage, sizeof( aryMessage ), "%s", "*** Message (#2) from Meatball at 11:07 ***" );
   snprintf( aryOriginal, sizeof( aryOriginal ), "%s", aryMessage );

   // Act
   ansiTransformExpress( aryMessage, sizeof( aryMessage ) );

   // Assert
   if ( strcmp( aryMessage, aryOriginal ) != 0 )
   {
      fail_msg( "ansiTransformExpress should leave message unchanged when ANSI is off; got '%s'", aryMessage );
   }
}

static void ansiTransformPostHeader_WhenFriendPost_RewritesHeaderDigitsAndTracksColor( void **state )
{
   // Arrange
   char aryHeader[256];

   (void)state;

   resetState();
   color.postFriendDate = 4;
   color.postFriendName = 13;
   color.postFriendText = 2;
   snprintf( aryHeader, sizeof( aryHeader ), "%s",
             "\033[35mMar 1, 2026 3:34 PM\033[32m from \033[36mSkankhunt Four Two\033[32m" );

   // Act
   ansiTransformPostHeader( aryHeader, sizeof( aryHeader ), 1 );

   // Assert
   if ( strstr( aryHeader, "\033[34mMar 1, 2026 3:34 PM" ) == NULL )
   {
      fail_msg( "ansiTransformPostHeader should remap friend post date color; got '%s'", aryHeader );
   }
   if ( strstr( aryHeader, "\033[95mSkankhunt Four Two" ) == NULL )
   {
      fail_msg( "ansiTransformPostHeader should remap friend post name color; got '%s'", aryHeader );
   }
   if ( lastColor != color.postFriendText )
   {
      fail_msg( "ansiTransformPostHeader should set lastColor to friend post text color; got %d", lastColor );
   }
}

static void colorPicker_WhenInvalidThenValidInput_ReturnsMappedColorAndFlushes( void **state )
{
   // Arrange
   const int aryKeys[] = { 'z', 'x', 'R' };
   int result;

   (void)state;

   resetState();
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   result = colorPicker();

   // Assert
   if ( result != 1 )
   {
      fail_msg( "colorPicker should map 'R' to color code 1; got %d", result );
   }
   if ( flushCount != 1 || lastFlushValue != 2 )
   {
      fail_msg( "repeated invalid color picker input should flush once with incremented invalid count; got count=%u last=%u",
                flushCount, lastFlushValue );
   }
}

static void colorPicker_WhenBrightAnsiDigitSelected_ReturnsBrightAnsiValue( void **state )
{
   // Arrange
   const int aryKeys[] = { '6' };
   int result;

   (void)state;

   resetState();
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   result = colorPicker();

   // Assert
   if ( result != 13 )
   {
      fail_msg( "colorPicker should map '6' to bright magenta value 13; got %d", result );
   }
}

static void backgroundPicker_WhenDefaultSelected_ReturnsDefaultCode( void **state )
{
   // Arrange
   const int aryKeys[] = { 'x', 'x', 'D' };
   int result;

   (void)state;

   resetState();
   color.background = 2;
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   result = backgroundPicker();

   // Assert
   if ( result != COLOR_VALUE_DEFAULT )
   {
      fail_msg( "backgroundPicker should map 'D' to the default color sentinel; got %d", result );
   }
   if ( flushCount != 1 || lastFlushValue != 2 )
   {
      fail_msg( "repeated invalid background input should flush once with incremented invalid count; got count=%u last=%u",
                flushCount, lastFlushValue );
   }
}

static void backgroundPicker_WhenBrightAnsiDigitSelected_ReturnsBrightAnsiValue( void **state )
{
   // Arrange
   const int aryKeys[] = { '8' };
   int result;

   (void)state;

   resetState();
   color.background = 2;
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );

   // Act
   result = backgroundPicker();

   // Assert
   if ( result != 15 )
   {
      fail_msg( "backgroundPicker should map '8' to bright white value 15; got %d", result );
   }
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( colorConfig_WhenPresetChangesBackground_RefreshesDisplayStateImmediately ),
      cmocka_unit_test( colorConfig_WhenPresetMenuShown_UsesLiveThemeTextAndPaletteSwatches ),
      cmocka_unit_test( defaultColors_WhenClearAllApplied_SetsKnownDefaults ),
      cmocka_unit_test( defaultColors_WhenClearAllDisabled_LeavesBackgroundUnchanged ),
      cmocka_unit_test( colorValueFromName_WhenCanonicalNameProvided_ReturnsNamedPaletteValue ),
      cmocka_unit_test( colorValueFromName_WhenAliasProvided_ReturnsCanonicalPaletteValue ),
      cmocka_unit_test( colorValueFromName_WhenBrightAnsiNameProvided_ReturnsBrightAnsiValue ),
      cmocka_unit_test( colorValueFromName_WhenNameUnknown_ReturnsInvalidSentinel ),
      cmocka_unit_test( colorNameFromValue_WhenPaletteValueMatchesAlias_ReturnsCanonicalName ),
      cmocka_unit_test( colorNameFromValue_WhenBrightAnsiValueProvided_ReturnsBrightCanonicalName ),
      cmocka_unit_test( colorNameFromValue_WhenPaletteValueUnknown_ReturnsNull ),
      cmocka_unit_test( formatAnsiForegroundSequence_WhenClassicColorRequested_UsesClassicAnsiCode ),
      cmocka_unit_test( formatAnsiForegroundSequence_WhenBrightColorRequested_UsesBrightAnsiCode ),
      cmocka_unit_test( formatAnsiForegroundSequence_WhenExtendedColorRequested_Uses256ColorCode ),
      cmocka_unit_test( formatAnsiForegroundSequence_WhenRgbColorAndTruecolorEnabled_Uses24BitCode ),
      cmocka_unit_test( formatAnsiBackgroundSequence_WhenRgbColorAndAutoModeInAppleTerminal_Uses256Fallback ),
      cmocka_unit_test( formatAnsiBackgroundSequence_WhenDarkThemeFallbackEnabled_UsesBlackIn256Mode ),
      cmocka_unit_test( formatAnsiForegroundSequence_WhenRgbColorAndAutoModeWithTruecolorTerminal_Uses24BitCode ),
      cmocka_unit_test( formatAnsiForegroundSequence_WhenRgbColorAnd256ModeRequested_IgnoresTruecolorTerminal ),
      cmocka_unit_test( formatAnsiDisplayStateSequence_WhenDefaultBackgroundRequested_UsesCombinedSelectors ),
      cmocka_unit_test( formatAnsiDisplayStateSequence_WhenRgbColorsRequested_UsesCombinedTruecolorSelectors ),
      cmocka_unit_test( brilliantColors_WhenApplied_SetsBrightDefaultPalette ),
      cmocka_unit_test( catppuccinLatteColors_WhenApplied_SetsLightPalette ),
      cmocka_unit_test( catppuccinMacchiatoColors_WhenApplied_SetsDarkPalette ),
      cmocka_unit_test( colorblindColors_WhenApplied_SetsAccessiblePalette ),
      cmocka_unit_test( everforestDarkColors_WhenApplied_SetsDarkPalette ),
      cmocka_unit_test( everforestLightColors_WhenApplied_SetsLightPalette ),
      cmocka_unit_test( gruvboxDarkColors_WhenApplied_SetsDarkPalette ),
      cmocka_unit_test( gruvboxLightColors_WhenApplied_SetsLightPalette ),
      cmocka_unit_test( hotDogColors_WhenApplied_SetsClassicHotDogPalette ),
      cmocka_unit_test( ansiTransform_WhenHotDogPaletteApplied_UsesThemeColorsForAllAnsiDigits ),
      cmocka_unit_test( ansiTransformExpress_WhenFriendSender_UsesFriendColorCodes ),
      cmocka_unit_test( ansiTransformExpress_WhenAnsiDisabled_LeavesTextUnchanged ),
      cmocka_unit_test( ansiTransformPostHeader_WhenFriendPost_RewritesHeaderDigitsAndTracksColor ),
      cmocka_unit_test( colorOptions_WhenColorOutputModeSelected_UpdatesConfiguredMode ),
      cmocka_unit_test( colorPicker_WhenInvalidThenValidInput_ReturnsMappedColorAndFlushes ),
      cmocka_unit_test( colorPicker_WhenBrightAnsiDigitSelected_ReturnsBrightAnsiValue ),
      cmocka_unit_test( backgroundPicker_WhenDefaultSelected_ReturnsDefaultCode ),
      cmocka_unit_test( backgroundPicker_WhenBrightAnsiDigitSelected_ReturnsBrightAnsiValue ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
