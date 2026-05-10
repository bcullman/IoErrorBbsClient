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

static void resetState( void )
{
   inputCount = 0;
   inputIndex = 0;
   flushCount = 0;
   lastFlushValue = 0;
   lastDisplayStateBackground = -2;
   lastDisplayStateForeground = -2;
   printAnsiDisplayStateCallCount = 0;

   flagsConfiguration.shouldUseAnsi = 0;
   lastColor = 0;

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
   (void)colorValue;
}

void printAnsiBackgroundColorValue( int colorValue )
{
   (void)colorValue;
}

void printAnsiDisplayStateValue( int foregroundColor, int backgroundColor )
{
   lastDisplayStateForeground = foregroundColor;
   lastDisplayStateBackground = backgroundColor;
   printAnsiDisplayStateCallCount++;
}

void printThemedMnemonicText( const char *ptrText, int defaultColor )
{
   (void)ptrText;
   (void)defaultColor;
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

   va_start( argList, format );
   va_end( argList );
   return 1;
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
   if ( color.text != 2 || color.forum != 3 || color.number != 6 || color.errorTextColor != 1 )
   {
      fail_msg( "defaultColors(1) did not set general default colors as expected" );
   }
   if ( color.background != 0 )
   {
      fail_msg( "defaultColors(1) should reset background to 0; got %d", color.background );
   }
   if ( color.postName != 6 || color.postFriendName != 1 || color.expressName != 2 )
   {
      fail_msg( "defaultColors(1) did not set post/express defaults as expected" );
   }
   if ( color.ansiBlackTextColor != 2 || color.ansiBlueTextColor != 4 ||
        color.ansiMagentaTextColor != 5 || color.ansiWhiteTextColor != 7 )
   {
      fail_msg( "defaultColors(1) did not set full ANSI fallback colors as expected; got black=%d blue=%d magenta=%d white=%d",
                color.ansiBlackTextColor, color.ansiBlueTextColor,
                color.ansiMagentaTextColor, color.ansiWhiteTextColor );
   }
}

static void colorConfig_WhenPresetChangesBackground_RefreshesDisplayStateImmediately( void **state )
{
   const int aryKeys[] = { 'p', 'l', 'q' };

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
   if ( color.background != 255 )
   {
      fail_msg( "Latte preset should change the configured background to white; got %d",
                color.background );
   }
   if ( printAnsiDisplayStateCallCount == 0 )
   {
      fail_msg( "colorConfig should refresh the full display state after a preset change" );
   }
   if ( lastDisplayStateBackground != 255 )
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
   if ( color.text != 2 )
   {
      fail_msg( "defaultColors(0) should repair negative text color to default 2; got %d", color.text );
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

static void colorblindColors_WhenApplied_SetsAccessiblePalette( void **state )
{
   // Arrange
   (void)state;

   resetState();
   memset( &color, 0, sizeof( color ) );

   // Act
   colorblindColors();

   // Assert
   if ( color.text != 231 || color.forum != 75 || color.number != 214 ||
        color.errorTextColor != 166 )
   {
      fail_msg( "colorblindColors should set general accessible colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != 16 )
   {
      fail_msg( "colorblindColors should keep a dark background; got %d", color.background );
   }
   if ( color.postDate != 75 || color.postFriendDate != 25 ||
        color.postName != 214 || color.postFriendName != 175 ||
        color.inputHighlight != 214 || color.morePrompt != 221 ||
        color.expressName != 214 || color.expressFriendName != 175 )
   {
      fail_msg( "colorblindColors should map post, input, and express roles onto the preset palette; got postDate=%d frienddate=%d postName=%d friendname=%d inputHighlight=%d morePrompt=%d expressName=%d expressFriendName=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.inputHighlight, color.morePrompt,
                color.expressName, color.expressFriendName );
   }
   if ( color.ansiBlackTextColor != 146 || color.ansiBlueTextColor != 75 ||
        color.ansiMagentaTextColor != 175 || color.ansiWhiteTextColor != 221 )
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
   if ( color.text != 10 || color.forum != 11 || color.number != 14 ||
        color.errorTextColor != 9 )
   {
      fail_msg( "brilliantColors should set bright general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != 0 )
   {
      fail_msg( "brilliantColors should keep a black background; got %d", color.background );
   }
   if ( color.postDate != 13 || color.postFriendDate != 13 ||
        color.postName != 14 || color.postFriendName != 9 ||
        color.postText != 10 || color.postFriendText != 10 ||
        color.anonymous != 11 || color.morePrompt != 11 ||
        color.inputText != 10 || color.inputHighlight != 14 ||
        color.expressText != 10 || color.expressName != 10 ||
        color.expressFriendName != 10 || color.expressFriendText != 10 )
   {
      fail_msg( "brilliantColors should map the default roles onto bright ANSI values; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != 10 || color.ansiBlueTextColor != 12 ||
        color.ansiMagentaTextColor != 13 || color.ansiWhiteTextColor != 15 )
   {
      fail_msg( "brilliantColors should theme the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
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
   if ( color.text != 240 || color.forum != 27 || color.number != 37 ||
        color.errorTextColor != 161 )
   {
      fail_msg( "catppuccinLatteColors should set the light palette general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != 255 )
   {
      fail_msg( "catppuccinLatteColors should use a light background; got %d", color.background );
   }
   if ( color.postDate != 37 || color.postFriendDate != 30 ||
        color.postName != 69 || color.postFriendName != 70 ||
        color.postText != 240 || color.postFriendText != 240 ||
        color.anonymous != 172 || color.morePrompt != 172 ||
        color.inputText != 240 || color.inputHighlight != 27 ||
        color.expressText != 240 || color.expressName != 27 ||
        color.expressFriendName != 70 || color.expressFriendText != 240 )
   {
      fail_msg( "catppuccinLatteColors should map posts, prompts, input, and express roles onto the light palette; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != 246 || color.ansiBlueTextColor != 27 ||
        color.ansiMagentaTextColor != 99 || color.ansiWhiteTextColor != 60 )
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
   if ( color.text != 189 || color.forum != 111 || color.number != 116 ||
        color.errorTextColor != 210 )
   {
      fail_msg( "catppuccinMacchiatoColors should set the dark palette general colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != 236 )
   {
      fail_msg( "catppuccinMacchiatoColors should keep a dark background; got %d", color.background );
   }
   if ( color.postDate != 116 || color.postFriendDate != 116 ||
        color.postName != 147 || color.postFriendName != 150 ||
        color.postText != 189 || color.postFriendText != 189 ||
        color.anonymous != 223 || color.morePrompt != 223 ||
        color.inputText != 189 || color.inputHighlight != 111 ||
        color.expressText != 189 || color.expressName != 147 ||
        color.expressFriendName != 150 || color.expressFriendText != 189 )
   {
      fail_msg( "catppuccinMacchiatoColors should map posts, prompts, input, and express roles onto the dark palette; got postDate=%d frienddate=%d postName=%d friendname=%d postText=%d friendposttext=%d anonymous=%d morePrompt=%d inputText=%d inputHighlight=%d expressText=%d expressName=%d expressFriendName=%d expressFriendText=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.postText, color.postFriendText,
                color.anonymous, color.morePrompt, color.inputText, color.inputHighlight,
                color.expressText, color.expressName,
                color.expressFriendName, color.expressFriendText );
   }
   if ( color.ansiBlackTextColor != 103 || color.ansiBlueTextColor != 111 ||
        color.ansiMagentaTextColor != 183 || color.ansiWhiteTextColor != 189 )
   {
      fail_msg( "catppuccinMacchiatoColors should theme the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
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
   if ( color.text != 220 || color.forum != 196 || color.number != 220 ||
        color.errorTextColor != 231 )
   {
      fail_msg( "hotDogColors should set general hot dog colors; got text=%d forum=%d number=%d error=%d",
                color.text, color.forum, color.number, color.errorTextColor );
   }
   if ( color.background != 16 )
   {
      fail_msg( "hotDogColors should keep a black background; got %d", color.background );
   }
   if ( color.postText != 214 || color.postFriendText != 214 ||
        color.expressText != 214 || color.expressFriendText != 214 )
   {
      fail_msg( "hotDogColors should keep only post and eXpress bodies orange; got postText=%d friendposttext=%d expressText=%d friendexpresstext=%d",
                color.postText, color.postFriendText, color.expressText,
                color.expressFriendText );
   }

   if ( color.postDate != 226 || color.postFriendDate != 226 ||
        color.postName != 226 || color.postFriendName != 226 ||
        color.anonymous != 226 || color.morePrompt != 220 ||
        color.inputText != 220 || color.expressName != 226 ||
        color.expressFriendName != 226 )
   {
      fail_msg( "hotDogColors should keep date and name headers yellow while leaving only bodies orange; got postDate=%d frienddate=%d postName=%d friendname=%d anonymous=%d morePrompt=%d inputText=%d expressName=%d expressFriendName=%d",
                color.postDate, color.postFriendDate, color.postName,
                color.postFriendName, color.anonymous, color.morePrompt,
                color.inputText, color.expressName, color.expressFriendName );
   }
   if ( color.ansiBlackTextColor != 130 || color.ansiBlueTextColor != 214 ||
        color.ansiMagentaTextColor != 130 || color.ansiWhiteTextColor != 220 )
   {
      fail_msg( "hotDogColors should eliminate stray gray and purple by theming the full incoming ANSI palette; got black=%d blue=%d magenta=%d white=%d",
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
   if ( transformedBlack != 130 || transformedBlue != 214 ||
        transformedMagenta != 130 || transformedWhite != 220 )
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
      cmocka_unit_test( formatAnsiDisplayStateSequence_WhenDefaultBackgroundRequested_UsesCombinedSelectors ),
      cmocka_unit_test( brilliantColors_WhenApplied_SetsBrightDefaultPalette ),
      cmocka_unit_test( catppuccinLatteColors_WhenApplied_SetsLightPalette ),
      cmocka_unit_test( catppuccinMacchiatoColors_WhenApplied_SetsDarkPalette ),
      cmocka_unit_test( colorblindColors_WhenApplied_SetsAccessiblePalette ),
      cmocka_unit_test( hotDogColors_WhenApplied_SetsClassicHotDogPalette ),
      cmocka_unit_test( ansiTransform_WhenHotDogPaletteApplied_UsesThemeColorsForAllAnsiDigits ),
      cmocka_unit_test( ansiTransformExpress_WhenFriendSender_UsesFriendColorCodes ),
      cmocka_unit_test( ansiTransformExpress_WhenAnsiDisabled_LeavesTextUnchanged ),
      cmocka_unit_test( ansiTransformPostHeader_WhenFriendPost_RewritesHeaderDigitsAndTracksColor ),
      cmocka_unit_test( colorPicker_WhenInvalidThenValidInput_ReturnsMappedColorAndFlushes ),
      cmocka_unit_test( colorPicker_WhenBrightAnsiDigitSelected_ReturnsBrightAnsiValue ),
      cmocka_unit_test( backgroundPicker_WhenDefaultSelected_ReturnsDefaultCode ),
      cmocka_unit_test( backgroundPicker_WhenBrightAnsiDigitSelected_ReturnsBrightAnsiValue ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
