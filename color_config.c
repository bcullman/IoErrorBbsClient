/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "color.h"
#include "defs.h"
#include "utility.h"
static const char *COLOR_MAIN_MENU_KEYS = "tcoq \n";
static const char *COLOR_OUTPUT_MODE_KEYS = "at2 \n";
static const char *COLOR_RESET_MENU_KEYS = "abcdefghijklq\n";
#define RGB_CONST( red, green, blue ) \
   ( COLOR_VALUE_RGB_FLAG | ( ( red ) << 16 ) | ( ( green ) << 8 ) | ( blue ) )
#define COLOR_EDITOR_FAST_STEP 16
#define COLOR_EDITOR_FIELD_LABEL_WIDTH 20
#define COLOR_EDITOR_VISIBLE_FIELD_COUNT 11
#define PRESET_COLUMN_GAP " "
#define PRESET_LABEL_WIDTH 22
#define PRESET_SWATCH_COUNT 4

typedef struct
{
   int colorValue;
   const char *ptrDisplayName;
} PaletteNameOption;

typedef struct
{
   void ( *applyPresetColors )( void );
   int arySwatchColors[PRESET_SWATCH_COUNT];
   int keyChar;
   const char *ptrLabel;
   const char *ptrSelectionLabel;
} PresetMenuOption;

typedef struct
{
   int colorIndex;
   const char *ptrLabel;
} ColorEditorFieldSpec;

typedef enum
{
   COLOR_EDITOR_PREVIEW_EXPRESS = 0,
   COLOR_EDITOR_PREVIEW_FULL,
   COLOR_EDITOR_PREVIEW_GENERAL,
   COLOR_EDITOR_PREVIEW_INPUT,
   COLOR_EDITOR_PREVIEW_POST
} ColorEditorPreviewKind;

typedef struct
{
   size_t componentIndex;
   size_t fieldIndex;
   size_t fieldCount;
   const ColorEditorFieldSpec *ptrFields;
   const char *ptrSectionTitle;
   ColorEditorPreviewKind previewKind;
} ColorEditorContext;

static const PaletteNameOption aryPaletteNameOptions[] =
   {
      { 0, "Black" },
      { 1, "Red" },
      { 2, "Green" },
      { 3, "Yellow" },
      { 4, "Blue" },
      { 5, "Magenta" },
      { 6, "Cyan" },
      { 7, "White" },
      { 8, "Bright black" },
      { 9, "Bright red" },
      { 10, "Bright green" },
      { 11, "Bright yellow" },
      { 12, "Bright blue" },
      { 13, "Bright magenta" },
      { 14, "Bright cyan" },
      { 15, "Bright white" },
      { COLOR_VALUE_DEFAULT, "Default" } };

static const ColorEditorFieldSpec aryExpressAllColorFields[] =
   {
      { COLOR_FIELD_EXPRESS_NAME, "User name" },
      { COLOR_FIELD_EXPRESS_TEXT, "User text" },
      { COLOR_FIELD_EXPRESS_FRIEND_NAME, "Friend name" },
      { COLOR_FIELD_EXPRESS_FRIEND_TEXT, "Friend text" } };

static const ColorEditorFieldSpec aryGeneralColorFields[] =
   {
      { COLOR_FIELD_TEXT, "Text" },
      { COLOR_FIELD_FORUM, "Forum" },
      { COLOR_FIELD_NUMBER, "Number" },
      { COLOR_FIELD_ERROR_TEXT, "Error" },
      { COLOR_FIELD_BACKGROUND, "Background" } };

static const ColorEditorFieldSpec aryInputColorFields[] =
   {
      { COLOR_FIELD_INPUT_TEXT, "Text" },
      { COLOR_FIELD_INPUT_HIGHLIGHT, "Completion" } };

static const ColorEditorFieldSpec aryPostAllColorFields[] =
   {
      { COLOR_FIELD_POST_DATE, "User date" },
      { COLOR_FIELD_POST_NAME, "User name" },
      { COLOR_FIELD_POST_TEXT, "User text" },
      { COLOR_FIELD_POST_FRIEND_DATE, "Friend date" },
      { COLOR_FIELD_POST_FRIEND_NAME, "Friend name" },
      { COLOR_FIELD_POST_FRIEND_TEXT, "Friend text" } };

static const ColorEditorFieldSpec aryFullColorFields[] =
   {
      { COLOR_FIELD_TEXT, "General text" },
      { COLOR_FIELD_FORUM, "Forum prompt" },
      { COLOR_FIELD_NUMBER, "Number" },
      { COLOR_FIELD_ERROR_TEXT, "Error" },
      { COLOR_FIELD_BACKGROUND, "Background" },
      { COLOR_FIELD_INPUT_TEXT, "Input text" },
      { COLOR_FIELD_INPUT_HIGHLIGHT, "Input completion" },
      { COLOR_FIELD_POST_DATE, "Post date" },
      { COLOR_FIELD_POST_NAME, "Post name" },
      { COLOR_FIELD_POST_TEXT, "Post text" },
      { COLOR_FIELD_POST_FRIEND_DATE, "Friend post date" },
      { COLOR_FIELD_POST_FRIEND_NAME, "Friend post name" },
      { COLOR_FIELD_POST_FRIEND_TEXT, "Friend post text" },
      { COLOR_FIELD_EXPRESS_NAME, "Express name" },
      { COLOR_FIELD_EXPRESS_TEXT, "Express text" },
      { COLOR_FIELD_EXPRESS_FRIEND_NAME, "Friend X name" },
      { COLOR_FIELD_EXPRESS_FRIEND_TEXT, "Friend X text" } };

static void applyDefaultPresetColors( void );

static const PresetMenuOption aryPresetMenuOptions[] =
   {
      { applyDefaultPresetColors,
        { RGB_CONST( 0x00, 0x80, 0x00 ), RGB_CONST( 0x80, 0x80, 0x00 ),
          RGB_CONST( 0x00, 0x80, 0x80 ), RGB_CONST( 0x80, 0x00, 0x00 ) },
        'A',
        "Default",
        "Default" },
      { brilliantColors,
        { RGB_CONST( 0x00, 0xff, 0x00 ), RGB_CONST( 0xff, 0xff, 0x00 ),
          RGB_CONST( 0x00, 0xff, 0xff ), RGB_CONST( 0xff, 0x00, 0x00 ) },
        'B',
        "Brilliant",
        "Brilliant" },
      { everforestDarkColors,
        { RGB_CONST( 0xd3, 0xc6, 0xaa ), RGB_CONST( 0x7f, 0xbb, 0xb3 ),
          RGB_CONST( 0x83, 0xc0, 0x92 ), RGB_CONST( 0xe6, 0x7e, 0x80 ) },
        'C',
        "Everforest Dark",
        "Everforest Dark" },
      { everforestLightColors,
        { RGB_CONST( 0x5c, 0x6a, 0x72 ), RGB_CONST( 0x35, 0x8f, 0xa2 ),
          RGB_CONST( 0x3a, 0x94, 0x84 ), RGB_CONST( 0xf8, 0x55, 0x52 ) },
        'D',
        "Everforest Light",
        "Everforest Light" },
      { gruvboxDarkColors,
        { RGB_CONST( 0xeb, 0xdb, 0xb2 ), RGB_CONST( 0x83, 0xa5, 0x98 ),
          RGB_CONST( 0x8e, 0xc0, 0x7c ), RGB_CONST( 0xfe, 0x80, 0x19 ) },
        'E',
        "Gruvbox Dark",
        "Gruvbox Dark" },
      { gruvboxLightColors,
        { RGB_CONST( 0x3c, 0x38, 0x36 ), RGB_CONST( 0x45, 0x85, 0x88 ),
          RGB_CONST( 0x79, 0x74, 0x0e ), RGB_CONST( 0x9d, 0x00, 0x06 ) },
        'F',
        "Gruvbox Light",
        "Gruvbox Light" },
      { draculaProColors,
        { RGB_CONST( 0xe3, 0xe2, 0xe9 ), RGB_CONST( 0x73, 0x59, 0xf8 ),
          RGB_CONST( 0x5c, 0xf5, 0xdb ), RGB_CONST( 0xf8, 0x73, 0x59 ) },
        'G',
        "Dracula",
        "Dracula" },
      { catppuccinLatteColors,
        { RGB_CONST( 0x4c, 0x4f, 0x69 ), RGB_CONST( 0x1e, 0x66, 0xf5 ),
          RGB_CONST( 0x20, 0x9f, 0xb5 ), RGB_CONST( 0xd2, 0x0f, 0x39 ) },
        'H',
        "Latte (Catppuccin)",
        "Catppuccin Latte" },
      { catppuccinMacchiatoColors,
        { RGB_CONST( 0xca, 0xd3, 0xf5 ), RGB_CONST( 0x8a, 0xad, 0xf4 ),
          RGB_CONST( 0x7d, 0xc4, 0xe4 ), RGB_CONST( 0xed, 0x87, 0x96 ) },
        'I',
        "Macchiato (Catppuccin)",
        "Catppuccin Macchiato" },
      { colorblindColors,
        { RGB_CONST( 0xff, 0xff, 0xff ), RGB_CONST( 0x5f, 0xaf, 0xff ),
          RGB_CONST( 0xff, 0xaf, 0x00 ), RGB_CONST( 0xd7, 0x5f, 0x00 ) },
        'J',
        "Colorblind",
        "Colorblind" },
      { hotDogColors,
        { RGB_CONST( 0xff, 0xd7, 0x00 ), RGB_CONST( 0xff, 0x00, 0x00 ),
          RGB_CONST( 0xff, 0xff, 0xff ), RGB_CONST( 0xff, 0xd7, 0x00 ) },
        'K',
        "Hotdog stand",
        "Hotdog Stand" },
      { tidalReefColors,
        { RGB_CONST( 0xea, 0xf6, 0xad ), RGB_CONST( 0x1b, 0x77, 0x8c ),
          RGB_CONST( 0x6a, 0x2f, 0xee ), RGB_CONST( 0xb6, 0xdb, 0x00 ) },
        'L',
        "Tidal Reef",
        "Tidal Reef" } };

static const char *A_FRIEND = "Example Friend";
static const char *A_USER = "Example User";

static void applyColorEditorAction( ColorEditorContext *ptrContext,
                                    ColorEditorAction inputAction );
static bool applyPresetMenuSelection( int inputChar );
static bool colorEditorFieldAllowsDefaultValue( int colorIndex );
static const char *colorEditorModeName( void );
static const char *colorEditorPaletteName( int colorValue );
static int colorEditorResolvedRgbValue( int colorValue, int colorIndex );
static void printColorEditor( const ColorEditorContext *ptrContext );
static void printColorEditorControls( void );
static void printColorEditorFieldList( const ColorEditorContext *ptrContext );
static void printColorEditorFieldDisplayState( int colorIndex, int colorValue );
static void printColorEditorFieldValueSummary( const ColorEditorContext *ptrContext,
                                               size_t fieldIndex );
static void printColorEditorPreview( const ColorEditorContext *ptrContext );
static void printColorEditorPreviewByKind( ColorEditorPreviewKind previewKind,
                                           const ColorEditorContext *ptrContext );
static void printColorEditorRgbValue( int colorValue, size_t componentIndex,
                                      int colorIndex );
static ColorOutputMode pickColorOutputMode( void );
static void printPaletteColorValue( int colorValue );
static void postColorPreview( int dateColor, int textColor, int nameColor,
                              const char *ptrName );
static void presetColorConfig( void );
static void printExpressColorPreview( int textColor, int nameColor,
                                      const char *ptrName );
static void printGeneralColorPreview( void );
static void printInputColorPreview( const ColorEditorContext *ptrContext );
static void printPresetMenuItem( const PresetMenuOption *ptrOption );
static void printPresetMenuRow( const PresetMenuOption *ptrLeftOption,
                                const PresetMenuOption *ptrRightOption );
static void printPresetSwatches( const PresetMenuOption *ptrOption );
static void printThemePreviewContent( void );
static bool runColorEditor( const ColorEditorFieldSpec *ptrFields,
                            size_t fieldCount, size_t initialFieldIndex,
                            const char *ptrSectionTitle,
                            ColorEditorPreviewKind previewKind );

/// @brief Run the top-level interactive color configuration menu.
///
/// @return This function does not return a value.
void colorConfig( void )
{
   char aryPromptText[110];

   stdPrintf( "Color\r\n" );
   if ( !flagsConfiguration.shouldUseAnsi )
   {
      stdPrintf( "\r\nWARNING:  Color is off.  You will not be able to preview your selections." );
   }
   while ( true )
   {
      printAnsiDisplayStateValue( color.text, color.background );

      snprintf( aryPromptText, sizeof( aryPromptText ), "\r\n<T>hemes  <C>ustomize  <O>ptions  <Q>uit" );
      printThemedMnemonicText( aryPromptText, color.number );
      printThemedMnemonicText( "\r\nColor config -> ", color.forum );
      printAnsiForegroundColorValue( color.text );

      switch ( readValidatedMenuKey( COLOR_MAIN_MENU_KEYS ) )
      {
         case 'c':
            stdPrintf( "Customize\r\n\n" );
            runColorEditor( aryFullColorFields,
                            sizeof( aryFullColorFields ) / sizeof( aryFullColorFields[0] ),
                            0, "Customize", COLOR_EDITOR_PREVIEW_FULL );
            break;
         case 'o':
            stdPrintf( "Options\r\n\n" );
            colorOptions();
            break;
         case 't':
            presetColorConfig();
            break;
         case 'q':
         case ' ':
         case '\n':
            stdPrintf( "Quit\r\n" );
            return;
         default:
            break;
      }
   }
}

/// @brief Configure general color-related display options.
///
/// @return This function does not return a value.
void colorOptions( void )
{
   stdPrintf( "Automatically answer the ANSI terminal question? (%s) -> ",
              flagsConfiguration.shouldAutoAnswerAnsiPrompt ? "Yes" : "No" );
   flagsConfiguration.shouldAutoAnswerAnsiPrompt = (unsigned int)yesNoDefault( flagsConfiguration.shouldAutoAnswerAnsiPrompt );
   stdPrintf( "Use bold ANSI colors when ANSI is enabled? (%s) -> ",
              flagsConfiguration.shouldUseBold ? "Yes" : "No" );
   flagsConfiguration.shouldUseBold = (unsigned int)yesNoDefault( flagsConfiguration.shouldUseBold );
   stdPrintf( "Color output mode [A]uto/[T]ruecolor/[2]56 (%s) -> ",
              colorOutputModeName( configuredColorOutputMode ) );
   configuredColorOutputMode = pickColorOutputMode();
   refreshActiveColorTable();
   if ( flagsConfiguration.shouldUseAnsi )
   {
      printAnsiDisplayStateValue( lastColor, color.background );
   }
}

/// @brief Prompt for the preferred ANSI color output mode.
///
/// @return Selected color output mode.
static ColorOutputMode pickColorOutputMode( void )
{
   int inputChar;

   inputChar = readValidatedMenuKey( COLOR_OUTPUT_MODE_KEYS );
   switch ( inputChar )
   {
      case 'a':
         return COLOR_OUTPUT_MODE_AUTO;

      case 't':
         return COLOR_OUTPUT_MODE_TRUECOLOR;

      case '2':
         return COLOR_OUTPUT_MODE_256;

      case ' ':
      case '\n':
      default:
         return configuredColorOutputMode;
   }
}

/// @brief Run the express color configuration editor.
///
/// @return This function does not return a value.
void expressColorConfig( void )
{
   runColorEditor( aryExpressAllColorFields,
                   sizeof( aryExpressAllColorFields ) / sizeof( aryExpressAllColorFields[0] ),
                   0, "Express", COLOR_EDITOR_PREVIEW_EXPRESS );
}

/// @brief Configure the general theme colors.
///
/// @return This function does not return a value.
void generalColorConfig( void )
{
   runColorEditor( aryGeneralColorFields,
                   sizeof( aryGeneralColorFields ) / sizeof( aryGeneralColorFields[0] ),
                   0, "General", COLOR_EDITOR_PREVIEW_GENERAL );
}

/// @brief Configure input prompt and completion colors.
///
/// @return This function does not return a value.
void inputColorConfig( void )
{
   runColorEditor( aryInputColorFields,
                   sizeof( aryInputColorFields ) / sizeof( aryInputColorFields[0] ),
                   0, "Input", COLOR_EDITOR_PREVIEW_INPUT );
}

/// @brief Run the post color configuration editor.
///
/// @return This function does not return a value.
void postColorConfig( void )
{
   runColorEditor( aryPostAllColorFields,
                   sizeof( aryPostAllColorFields ) / sizeof( aryPostAllColorFields[0] ),
                   0, "Posts", COLOR_EDITOR_PREVIEW_POST );
}

/// @brief Print a preview line for a post color combination.
///
/// @param dateColor Preview date color.
/// @param textColor Preview post body color.
/// @param nameColor Preview author color.
/// @param ptrName Preview author name.
///
/// @return This helper does not return a value.
static void postColorPreview( int dateColor, int textColor, int nameColor,
                              const char *ptrName )
{
   printAnsiForegroundColorValue( dateColor );
   stdPrintf( "Jan  1 11:01 " );
   printAnsiForegroundColorValue( nameColor );
   stdPrintf( "%s  ", ptrName );
   printAnsiForegroundColorValue( textColor );
   stdPrintf( "Hi there!\r\n" );
}

/// @brief Show built-in themes and apply the selected theme.
///
/// @return This helper does not return a value.
static void presetColorConfig( void )
{
   bool savedUseBlackThemeBackgrounds;
   Color savedColorTable;

   snapshotActiveColorEditorState( &savedColorTable,
                                   &savedUseBlackThemeBackgrounds );
   while ( true )
   {
      size_t columnBreakIndex;
      size_t optionCount;
      size_t optionIndex;
      int inputChar;

      optionCount = sizeof( aryPresetMenuOptions ) / sizeof( aryPresetMenuOptions[0] );
      columnBreakIndex = ( optionCount + 1 ) / 2;

      printAnsiDisplayStateValue( color.text, color.background );
      stdPrintf( "\033[H\033[2J" );
      stdPrintf( "Color themes\r\n\n" );
      for ( optionIndex = 0;
            optionIndex < columnBreakIndex;
            optionIndex++ )
      {
         if ( optionIndex + columnBreakIndex < optionCount )
         {
            printPresetMenuRow( &aryPresetMenuOptions[optionIndex],
                                &aryPresetMenuOptions[optionIndex + columnBreakIndex] );
         }
         else
         {
            printPresetMenuRow( &aryPresetMenuOptions[optionIndex], NULL );
         }
      }
      printAnsiDisplayStateValue( color.text, color.background );
      printThemedMnemonicText( "\r\nTheme preview\r\n", color.number );
      printThemePreviewContent();
      printAnsiDisplayStateValue( color.text, color.background );
      printThemedMnemonicText( "Select theme (A-L, Q-Quit, Return-Save & Quit) -> ",
                               color.forum );
      printAnsiForegroundColorValue( color.text );

      inputChar = readValidatedMenuKey( COLOR_RESET_MENU_KEYS );
      if ( applyPresetMenuSelection( inputChar ) )
      {
         continue;
      }

      switch ( inputChar )
      {
         case 'q':
            restoreActiveColorEditorState( &savedColorTable,
                                           savedUseBlackThemeBackgrounds );
            stdPrintf( "Quit\r\n" );
            return;
         case '\n':
            commitActiveColorEditorState();
            stdPrintf( "Save\r\n" );
            return;
         default:
            break;
      }
   }
}

/// @brief Apply the built-in default preset from the preset menu.
///
/// @return This helper does not return a value.
static void applyDefaultPresetColors( void )
{
   defaultColors( 1 );
}

/// @brief Apply a preset menu option matching the supplied key.
///
/// @param inputChar Validated menu key to resolve.
///
/// @return `true` when a preset was matched and applied, otherwise `false`.
static bool applyPresetMenuSelection( int inputChar )
{
   size_t optionIndex;

   for ( optionIndex = 0;
         optionIndex < sizeof( aryPresetMenuOptions ) / sizeof( aryPresetMenuOptions[0] );
         optionIndex++ )
   {
      const PresetMenuOption *ptrOption;

      ptrOption = &aryPresetMenuOptions[optionIndex];
      if ( inputChar != tolower( ptrOption->keyChar ) )
      {
         continue;
      }

      stdPrintf( "%s\r\n\n", ptrOption->ptrSelectionLabel );
      ptrOption->applyPresetColors();
      return true;
   }

   return false;
}

/// @brief Apply one editing action to the live runtime palette.
///
/// @param ptrContext Active editor context.
/// @param inputAction Action to apply.
///
/// @return This helper does not return a value.
static void applyColorEditorAction( ColorEditorContext *ptrContext,
                                    ColorEditorAction inputAction )
{
   int colorIndex;
   int colorValue;

   colorIndex = ptrContext->ptrFields[ptrContext->fieldIndex].colorIndex;
   colorValue = colorFieldValue( colorIndex );
   if ( terminalShouldUseTruecolor() )
   {
      int blue;
      int channelDelta;
      int green;
      int red;

      colorValue = colorEditorResolvedRgbValue( colorValue, colorIndex );
      red = colorValueRed( colorValue );
      green = colorValueGreen( colorValue );
      blue = colorValueBlue( colorValue );
      if ( inputAction == COLOR_EDITOR_ACTION_INCREASE_FAST ||
           inputAction == COLOR_EDITOR_ACTION_DECREASE_FAST )
      {
         channelDelta = COLOR_EDITOR_FAST_STEP;
      }
      else
      {
         channelDelta = 1;
      }
      if ( inputAction == COLOR_EDITOR_ACTION_DECREASE_FAST ||
           inputAction == COLOR_EDITOR_ACTION_DECREASE_SMALL )
      {
         channelDelta = -channelDelta;
      }

      switch ( ptrContext->componentIndex )
      {
         case 0:
            red = colorEditorChannelValueWithinRgbRange( red + channelDelta );
            break;

         case 1:
            green = colorEditorChannelValueWithinRgbRange( green + channelDelta );
            break;

         default:
            blue = colorEditorChannelValueWithinRgbRange( blue + channelDelta );
            break;
      }
      colorValue = colorValueFromRgb( red, green, blue );
   }
   else
   {
      int paletteDelta;

      if ( inputAction == COLOR_EDITOR_ACTION_INCREASE_FAST ||
           inputAction == COLOR_EDITOR_ACTION_DECREASE_FAST )
      {
         paletteDelta = COLOR_EDITOR_FAST_STEP;
      }
      else
      {
         paletteDelta = 1;
      }
      if ( inputAction == COLOR_EDITOR_ACTION_DECREASE_FAST ||
           inputAction == COLOR_EDITOR_ACTION_DECREASE_SMALL )
      {
         paletteDelta = -paletteDelta;
      }

      colorValue = cycleColorEditorPaletteValue(
         colorValue, paletteDelta, colorEditorFieldAllowsDefaultValue( colorIndex ) );
   }

   setColorFieldValue( colorIndex, colorValue );
   if ( colorIndex == COLOR_FIELD_BACKGROUND )
   {
      useBlackThemeBackgrounds = false;
   }
}

/// @brief Return whether one field may use the `default` palette sentinel.
///
/// @param colorIndex Edited color field index.
///
/// @return `true` when the field may use `default`, otherwise `false`.
static bool colorEditorFieldAllowsDefaultValue( int colorIndex )
{
   return colorIndex == COLOR_FIELD_BACKGROUND;
}

/// @brief Return a short name for the currently active configured color table.
///
/// @return Human-readable active color table name.
static const char *colorEditorModeName( void )
{
   if ( terminalShouldUseTruecolor() )
   {
      return "truecolor";
   }

   return "256-color";
}

/// @brief Return a friendly palette name for one palette value.
///
/// @param colorValue Palette value to describe.
///
/// @return Friendly palette name, or `NULL` when no short name exists.
static const char *colorEditorPaletteName( int colorValue )
{
   size_t itemIndex;

   for ( itemIndex = 0; itemIndex < sizeof( aryPaletteNameOptions ) / sizeof( aryPaletteNameOptions[0] );
         itemIndex++ )
   {
      if ( aryPaletteNameOptions[itemIndex].colorValue == colorValue )
      {
         return aryPaletteNameOptions[itemIndex].ptrDisplayName;
      }
   }

   return colorNameFromValue( colorValue );
}

/// @brief Resolve one field value into an editable RGB color.
///
/// @param colorValue Current field color value.
/// @param colorIndex Edited color field index.
///
/// @return RGB color value suitable for truecolor editing.
static int colorEditorResolvedRgbValue( int colorValue, int colorIndex )
{
   int blue;
   int green;
   int red;

   if ( colorValueIsRgb( colorValue ) )
   {
      return colorValue;
   }
   if ( colorValueIsDefault( colorValue ) )
   {
      if ( colorIndex == COLOR_FIELD_BACKGROUND )
      {
         return colorValueFromRgb( 0x00, 0x00, 0x00 );
      }
      return colorValueFromRgb( 0xc0, 0xc0, 0xc0 );
   }

   xterm256RgbComponents( colorValue, &red, &green, &blue );
   return colorValueFromRgb( red, green, blue );
}

/// @brief Print the interactive custom color editor screen.
///
/// @param ptrContext Current editor context.
///
/// @return This helper does not return a value.
static void printColorEditor( const ColorEditorContext *ptrContext )
{
   char aryHeader[160];

   printAnsiDisplayStateValue( color.text, color.background );
   stdPrintf( "\033[H\033[2J" );
   snprintf( aryHeader, sizeof( aryHeader ), "Color editor: %s (%s)\r\n",
             ptrContext->ptrSectionTitle, colorEditorModeName() );
   stdPrintf( "%s", aryHeader );
   printColorEditorControls();
   printColorEditorFieldList( ptrContext );
   printColorEditorPreview( ptrContext );
}

/// @brief Print the visible fallback controls for the custom color editor.
///
/// @return This helper does not return a value.
static void printColorEditorControls( void )
{
   stdPrintf( "Up/Down-Field  Left/Right-Part  W/S-Adjust  +/-x16\r\n" );
   stdPrintf( "P/N-Field      A/D-Part         R-Reset     Return-Save & Quit\r\n" );
   stdPrintf( "                                            Q-Quit\r\n" );
}

/// @brief Print the field list for the active editor section.
///
/// @param ptrContext Current editor context.
///
/// @return This helper does not return a value.
static void printColorEditorFieldList( const ColorEditorContext *ptrContext )
{
   char aryLine[80];
   size_t firstVisibleFieldIndex;
   size_t lastVisibleFieldIndex;
   size_t fieldIndex;
   size_t visibleFieldCount;

   visibleFieldCount = ptrContext->fieldCount;
   if ( visibleFieldCount > COLOR_EDITOR_VISIBLE_FIELD_COUNT )
   {
      visibleFieldCount = COLOR_EDITOR_VISIBLE_FIELD_COUNT;
   }

   if ( ptrContext->fieldCount <= visibleFieldCount ||
        ptrContext->fieldIndex < visibleFieldCount / 2 )
   {
      firstVisibleFieldIndex = 0;
   }
   else if ( ptrContext->fieldIndex + visibleFieldCount / 2 >=
             ptrContext->fieldCount )
   {
      firstVisibleFieldIndex = ptrContext->fieldCount - visibleFieldCount;
   }
   else
   {
      firstVisibleFieldIndex = ptrContext->fieldIndex - visibleFieldCount / 2;
   }
   lastVisibleFieldIndex = firstVisibleFieldIndex + visibleFieldCount;

   stdPrintf( "\r\n" );
   for ( fieldIndex = firstVisibleFieldIndex;
         fieldIndex < lastVisibleFieldIndex;
         fieldIndex++ )
   {
      int colorIndex;
      int colorValue;

      colorIndex = ptrContext->ptrFields[fieldIndex].colorIndex;
      colorValue = colorFieldValue( colorIndex );
      printColorEditorFieldDisplayState( colorIndex, colorValue );
      snprintf( aryLine, sizeof( aryLine ), "%c %-*s ",
                fieldIndex == ptrContext->fieldIndex ? '>' : ' ',
                COLOR_EDITOR_FIELD_LABEL_WIDTH,
                ptrContext->ptrFields[fieldIndex].ptrLabel );
      stdPrintf( "%s", aryLine );
      printColorEditorFieldValueSummary( ptrContext, fieldIndex );
      stdPrintf( "\r\n" );
   }
}

/// @brief Apply the display state used for one editor field row.
///
/// @param colorIndex Edited field index for this row.
/// @param colorValue Current configured color value for this row.
///
/// @return This helper does not return a value.
static void printColorEditorFieldDisplayState( int colorIndex, int colorValue )
{
   if ( colorIndex == COLOR_FIELD_BACKGROUND )
   {
      printAnsiDisplayStateValue( color.text, colorValue );
      return;
   }

   printAnsiForegroundColorValue( colorValue );
}

/// @brief Print one inline value summary beside a field row.
///
/// @param ptrContext Current editor context.
/// @param fieldIndex Field row being rendered.
///
/// @return This helper does not return a value.
static void printColorEditorFieldValueSummary( const ColorEditorContext *ptrContext,
                                               size_t fieldIndex )
{
   int colorIndex;
   int colorValue;

   colorIndex = ptrContext->ptrFields[fieldIndex].colorIndex;
   colorValue = colorFieldValue( colorIndex );
   if ( terminalShouldUseTruecolor() )
   {
      if ( fieldIndex == ptrContext->fieldIndex )
      {
         printColorEditorRgbValue( colorValue, ptrContext->componentIndex,
                                   colorIndex );
      }
      else
      {
         char aryBuffer[32];

         colorValue = colorEditorResolvedRgbValue( colorValue, colorIndex );
         snprintf( aryBuffer, sizeof( aryBuffer ), "#%02X%02X%02X",
                   colorValueRed( colorValue ),
                   colorValueGreen( colorValue ),
                   colorValueBlue( colorValue ) );
         stdPrintf( "%s", aryBuffer );
      }
      return;
   }

   printPaletteColorValue( colorValue );
}

/// @brief Print the current preview pane for the custom color editor.
///
/// @param ptrContext Current editor context.
///
/// @return This helper does not return a value.
static void printColorEditorPreview( const ColorEditorContext *ptrContext )
{
   printAnsiDisplayStateValue( color.text, color.background );
   stdPrintf( "\r\n" );
   if ( ptrContext->previewKind == COLOR_EDITOR_PREVIEW_FULL )
   {
      printThemedMnemonicText( "Theme preview\r\n", color.number );
   }
   else
   {
      printThemedMnemonicText( "Preview\r\n", color.number );
   }
   printColorEditorPreviewByKind( ptrContext->previewKind, ptrContext );
}

/// @brief Print preview content for one editor preview kind.
///
/// @param previewKind Preview variant to print.
/// @param ptrContext Current editor context.
///
/// @return This helper does not return a value.
static void printColorEditorPreviewByKind( ColorEditorPreviewKind previewKind,
                                           const ColorEditorContext *ptrContext )
{
   switch ( previewKind )
   {
      case COLOR_EDITOR_PREVIEW_EXPRESS:
         printExpressColorPreview( color.expressText, color.expressName, A_USER );
         printExpressColorPreview( color.expressFriendText, color.expressFriendName,
                                   A_FRIEND );
         break;

      case COLOR_EDITOR_PREVIEW_FULL:
         printThemePreviewContent();
         break;

      case COLOR_EDITOR_PREVIEW_GENERAL:
         printGeneralColorPreview();
         break;

      case COLOR_EDITOR_PREVIEW_INPUT:
         printInputColorPreview( ptrContext );
         break;

      case COLOR_EDITOR_PREVIEW_POST:
         postColorPreview( color.postDate, color.postText, color.postName, A_USER );
         postColorPreview( color.postFriendDate, color.postFriendText,
                           color.postFriendName, A_FRIEND );
         break;

      default:
         printGeneralColorPreview();
         break;
   }
}

/// @brief Print the active truecolor value with one highlighted component.
///
/// @param colorValue Current field color value.
/// @param componentIndex Selected component index.
/// @param colorIndex Edited color field index.
///
/// @return This helper does not return a value.
static void printColorEditorRgbValue( int colorValue, size_t componentIndex,
                                      int colorIndex )
{
   char aryBuffer[128];
   int blue;
   int green;
   int red;

   colorValue = colorEditorResolvedRgbValue( colorValue, colorIndex );
   red = colorValueRed( colorValue );
   green = colorValueGreen( colorValue );
   blue = colorValueBlue( colorValue );
   snprintf(
      aryBuffer, sizeof( aryBuffer ),
      "RGB #%02X%02X%02X  %s%03d%s %s%03d%s %s%03d%s",
      red, green, blue,
      componentIndex == 0 ? "R[" : "R ", red, componentIndex == 0 ? "]" : "",
      componentIndex == 1 ? "G[" : "G ", green, componentIndex == 1 ? "]" : "",
      componentIndex == 2 ? "B[" : "B ", blue, componentIndex == 2 ? "]" : "" );
   stdPrintf( "%s", aryBuffer );
}

/// @brief Print an express message preview using the supplied colors.
///
/// @param textColor Preview express text color.
/// @param nameColor Preview express sender name color.
/// @param ptrName Preview sender name.
///
/// @return This helper does not return a value.
static void printExpressColorPreview( int textColor, int nameColor,
                                      const char *ptrName )
{
   printAnsiForegroundColorValue( textColor );
   stdPrintf( "X: " );
   printAnsiForegroundColorValue( nameColor );
   stdPrintf( "%s", ptrName );
   printAnsiForegroundColorValue( textColor );
   stdPrintf( "  >Hi there!\r\n" );
}

/// @brief Print a preview of the general theme colors.
///
/// @return This helper does not return a value.
static void printGeneralColorPreview( void )
{
   printAnsiDisplayStateValue( color.forum, color.background );
   stdPrintf( "Lobby> " );
   printAnsiForegroundColorValue( color.text );
   stdPrintf( "Enter message  " );
   printAnsiForegroundColorValue( color.number );
   stdPrintf( "150" );
   printAnsiForegroundColorValue( color.text );
   stdPrintf( " msgs  " );
   printAnsiForegroundColorValue( color.errorTextColor );
   stdPrintf( "Error\r\n" );
}

/// @brief Print a preview of the input and completion colors.
///
/// When editing one of the input fields, use the live edited color across the
/// whole sample name so the preview does not mix old and new colors.
///
/// @param ptrContext Current editor context.
///
/// @return This helper does not return a value.
static void printInputColorPreview( const ColorEditorContext *ptrContext )
{
   int activeFieldIndex;
   int completionColor;
   int inputTextColor;

   activeFieldIndex = -1;
   if ( ptrContext != NULL )
   {
      activeFieldIndex = ptrContext->ptrFields[ptrContext->fieldIndex].colorIndex;
   }
   inputTextColor = color.inputText;
   completionColor = color.inputHighlight;
   if ( activeFieldIndex == COLOR_FIELD_INPUT_TEXT ||
        activeFieldIndex == COLOR_FIELD_INPUT_HIGHLIGHT )
   {
      inputTextColor = colorFieldValue( activeFieldIndex );
      completionColor = inputTextColor;
   }

   printAnsiForegroundColorValue( color.text );
   stdPrintf( "Recipient: " );
   printAnsiForegroundColorValue( inputTextColor );
   stdPrintf( "Example User" );
   printAnsiForegroundColorValue( completionColor );
   stdPrintf( "  " );
   printAnsiForegroundColorValue( inputTextColor );
   stdPrintf( ">Hi there!\r\n" );
}

/// @brief Print a compact whole-theme preview shared by theme selection screens.
///
/// @return This helper does not return a value.
static void printThemePreviewContent( void )
{
   printGeneralColorPreview();
   printInputColorPreview( NULL );
   postColorPreview( color.postDate, color.postText, color.postName, A_USER );
   printAnsiForegroundColorValue( color.expressText );
   stdPrintf( "X: " );
   printAnsiForegroundColorValue( color.expressName );
   stdPrintf( "%s", A_USER );
   printAnsiForegroundColorValue( color.expressText );
   stdPrintf( "  >Hello  ANSI: " );
   printAnsiForegroundColorValue( color.ansiBlackTextColor );
   stdPrintf( "blk " );
   printAnsiForegroundColorValue( color.ansiBlueTextColor );
   stdPrintf( "blu " );
   printAnsiForegroundColorValue( color.ansiMagentaTextColor );
   stdPrintf( "mag " );
   printAnsiForegroundColorValue( color.ansiWhiteTextColor );
   stdPrintf( "wht\r\n\r\n" );
}

/// @brief Print one palette value for the custom color editor.
///
/// @param colorValue Current palette value.
///
/// @return This helper does not return a value.
static void printPaletteColorValue( int colorValue )
{
   char aryBuffer[96];
   const char *ptrPaletteName;

   if ( colorValueIsDefault( colorValue ) )
   {
      stdPrintf( "Palette [default]" );
      return;
   }

   ptrPaletteName = colorEditorPaletteName( colorValue );
   if ( ptrPaletteName != NULL )
   {
      snprintf( aryBuffer, sizeof( aryBuffer ), "Palette [%03d] (%s)", colorValue,
                ptrPaletteName );
   }
   else
   {
      snprintf( aryBuffer, sizeof( aryBuffer ), "Palette [%03d]", colorValue );
   }
   stdPrintf( "%s", aryBuffer );
}

/// @brief Print one preset theme menu entry.
///
/// @param ptrOption Preset option to display.
///
/// @return This helper does not return a value.
static void printPresetMenuItem( const PresetMenuOption *ptrOption )
{
   printAnsiDisplayStateValue( color.number, color.background );
   stdPrintf( " %c.) ", ptrOption->keyChar );
   printAnsiDisplayStateValue( color.text, color.background );
   printAnsiForegroundColorValue( color.text );
   stdPrintf( "%-*s", PRESET_LABEL_WIDTH, ptrOption->ptrLabel );
   printPresetSwatches( ptrOption );
}

/// @brief Print one or two preset theme menu entries on a single row.
///
/// @param ptrLeftOption Left-column preset option.
/// @param ptrRightOption Right-column preset option, or `NULL` when absent.
///
/// @return This helper does not return a value.
static void printPresetMenuRow( const PresetMenuOption *ptrLeftOption,
                                const PresetMenuOption *ptrRightOption )
{
   printPresetMenuItem( ptrLeftOption );
   if ( ptrRightOption != NULL )
   {
      stdPrintf( PRESET_COLUMN_GAP );
      printPresetMenuItem( ptrRightOption );
   }
   stdPrintf( "\r\n" );
}

/// @brief Print representative color swatches for a preset theme.
///
/// @param ptrOption Preset option whose palette strip should be shown.
///
/// @return This helper does not return a value.
static void printPresetSwatches( const PresetMenuOption *ptrOption )
{
   bool savedUseBlackThemeBackgrounds;
   size_t swatchIndex;

   savedUseBlackThemeBackgrounds = useBlackThemeBackgrounds;
   useBlackThemeBackgrounds = false;

   stdPrintf( " " );
   for ( swatchIndex = 0; swatchIndex < PRESET_SWATCH_COUNT; swatchIndex++ )
   {
      if ( swatchIndex > 0 )
      {
         stdPrintf( " " );
      }
      printAnsiBackgroundColorValue( ptrOption->arySwatchColors[swatchIndex] );
      stdPrintf( "  " );
      printAnsiDisplayStateValue( color.text, color.background );
   }

   useBlackThemeBackgrounds = savedUseBlackThemeBackgrounds;
}

/// @brief Run one interactive custom color editor session.
///
/// @param ptrFields Ordered field table for this editor.
/// @param fieldCount Number of editable fields.
/// @param initialFieldIndex Initially selected field index.
/// @param ptrSectionTitle Human-readable section name.
/// @param previewKind Preview pane kind for this editor.
///
/// @return `true` when the edit was saved, otherwise `false`.
static bool runColorEditor( const ColorEditorFieldSpec *ptrFields,
                            size_t fieldCount, size_t initialFieldIndex,
                            const char *ptrSectionTitle,
                            ColorEditorPreviewKind previewKind )
{
   bool savedUseBlackThemeBackgrounds;
   Color savedColorTable;
   ColorEditorContext context;

   snapshotActiveColorEditorState( &savedColorTable,
                                   &savedUseBlackThemeBackgrounds );
   context.componentIndex = 0;
   context.fieldCount = fieldCount;
   context.fieldIndex = initialFieldIndex;
   context.ptrFields = ptrFields;
   context.ptrSectionTitle = ptrSectionTitle;
   context.previewKind = previewKind;

   while ( true )
   {
      ColorEditorAction inputAction;

      printColorEditor( &context );
      inputAction = readColorEditorAction();
      switch ( inputAction )
      {
         case COLOR_EDITOR_ACTION_CANCEL:
            restoreActiveColorEditorState( &savedColorTable,
                                           savedUseBlackThemeBackgrounds );
            stdPrintf( "Cancel\r\n" );
            return false;

         case COLOR_EDITOR_ACTION_MOVE_LEFT:
            if ( terminalShouldUseTruecolor() )
            {
               if ( context.componentIndex == 0 )
               {
                  context.componentIndex = 2;
               }
               else
               {
                  context.componentIndex--;
               }
            }
            break;

         case COLOR_EDITOR_ACTION_MOVE_RIGHT:
            if ( terminalShouldUseTruecolor() )
            {
               context.componentIndex = ( context.componentIndex + 1 ) % 3;
            }
            break;

         case COLOR_EDITOR_ACTION_NEXT_FIELD:
            context.fieldIndex = ( context.fieldIndex + 1 ) % context.fieldCount;
            context.componentIndex = 0;
            break;

         case COLOR_EDITOR_ACTION_PREVIOUS_FIELD:
            if ( context.fieldIndex == 0 )
            {
               context.fieldIndex = context.fieldCount - 1;
            }
            else
            {
               context.fieldIndex--;
            }
            context.componentIndex = 0;
            break;

         case COLOR_EDITOR_ACTION_RESET_FIELD:
            {
               int colorIndex;

               colorIndex = context.ptrFields[context.fieldIndex].colorIndex;
               setColorFieldValue( colorIndex,
                                   colorFieldValueForColor( &savedColorTable,
                                                            colorIndex ) );
               if ( colorIndex == COLOR_FIELD_BACKGROUND )
               {
                  useBlackThemeBackgrounds = savedUseBlackThemeBackgrounds;
               }
               break;
            }

         case COLOR_EDITOR_ACTION_SAVE:
            commitActiveColorEditorState();
            stdPrintf( "Save\r\n" );
            return true;

         case COLOR_EDITOR_ACTION_DECREASE_FAST:
         case COLOR_EDITOR_ACTION_DECREASE_SMALL:
         case COLOR_EDITOR_ACTION_INCREASE_FAST:
         case COLOR_EDITOR_ACTION_INCREASE_SMALL:
            applyColorEditorAction( &context, inputAction );
            break;
      }
   }
}
