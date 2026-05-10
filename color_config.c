/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "color.h"
#include "defs.h"
#include "utility.h"
static const char *COLOR_MAIN_MENU_KEYS = "pgisoxq \n";
static const char *COLOR_GENERAL_MENU_KEYS = "befntq \n";
static const char *COLOR_INPUT_MENU_KEYS = "ctq \n";
static const char *COLOR_POST_MENU_KEYS = "dntq \n";
static const char *COLOR_EXPRESS_MENU_KEYS = "ntq \n";
static const char *COLOR_RESET_MENU_KEYS = "dbeflmchq \n";
static const char *COLOR_USER_OR_FRIEND_KEYS = "ufq \n";
static const char *COLOR_FOREGROUND_KEYS = "krgybmcw12345678";
static const char *COLOR_BACKGROUND_KEYS = "krgybmcwd12345678";

typedef struct
{
   int keyChar;
   int colorValue;
   const char *ptrDisplayName;
} PickerColorOption;

typedef struct
{
   const char *ptrLabel;
   int textColor;
   int accentColor;
   int backgroundColor;
   size_t mnemonicIndex;
} PresetMenuOption;

static const PickerColorOption aryForegroundPickerOptions[] =
   {
      { 'k', 0, "Black" },
      { 'r', 1, "Red" },
      { 'g', 2, "Green" },
      { 'y', 3, "Yellow" },
      { 'b', 4, "Blue" },
      { 'm', 5, "Magenta" },
      { 'c', 6, "Cyan" },
      { 'w', 7, "White" },
      { '1', 8, "Bright black" },
      { '2', 9, "Bright red" },
      { '3', 10, "Bright green" },
      { '4', 11, "Bright yellow" },
      { '5', 12, "Bright blue" },
      { '6', 13, "Bright magenta" },
      { '7', 14, "Bright cyan" },
      { '8', 15, "Bright white" } };

static const PickerColorOption aryBackgroundPickerOptions[] =
   {
      { 'k', 0, "Black" },
      { 'r', 1, "Red" },
      { 'g', 2, "Green" },
      { 'y', 3, "Yellow" },
      { 'b', 4, "Blue" },
      { 'm', 5, "Magenta" },
      { 'c', 6, "Cyan" },
      { 'w', 7, "White" },
      { '1', 8, "Bright black" },
      { '2', 9, "Bright red" },
      { '3', 10, "Bright green" },
      { '4', 11, "Bright yellow" },
      { '5', 12, "Bright blue" },
      { '6', 13, "Bright magenta" },
      { '7', 14, "Bright cyan" },
      { '8', 15, "Bright white" },
      { 'd', COLOR_VALUE_DEFAULT, "Default" } };

static const PresetMenuOption aryPresetMenuOptions[] =
   {
      { "Default", 2, 3, 0, 0 },
      { "Brilliant", 10, 11, 0, 0 },
      { "Everforest Dark", 187, 144, 236, 0 },
      { "Everforest Light", 242, 106, 230, 6 },
      { "Latte (Catppuccin)", 240, 27, 255, 0 },
      { "Macchiato (Catppuccin)", 189, 111, 236, 0 },
      { "Colorblind", 231, 75, 16, 0 },
      { "Hotdog stand", 220, 196, 16, 0 } };

static const char *A_FRIEND = "Example Friend";
static const char *A_USER = "Example User";

static void configureExpressColors( int *ptrTextColor, int *ptrNameColor,
                                    const char *ptrPreviewName );
static void configurePostColors( int *ptrDateColor, int *ptrTextColor,
                                 int *ptrNameColor, const char *ptrPreviewName );
static const PickerColorOption *findPickerColorOption( const PickerColorOption *ptrOptions,
                                                       size_t itemCount,
                                                       int keyChar );
static void postColorPreview( int dateColor, int textColor, int nameColor,
                              const char *ptrName );
static void presetColorConfig( void );
static void printBackgroundPickerMenu( void );
static void printExpressColorPreview( int textColor, int nameColor,
                                      const char *ptrName );
static void printForegroundPickerMenu( void );
static void printGeneralColorPreview( void );
static void printInputColorPreview( void );
static void printPresetMenuItem( const PresetMenuOption *ptrOption );
static const PickerColorOption *readPickerSelection( const char *ptrAllowedKeys,
                                                     const PickerColorOption *ptrOptions,
                                                     size_t itemCount,
                                                     void ( *printMenu )( void ) );

/// @brief Prompt for a background color selection.
///
/// @return Selected background color value.
int backgroundPicker( void )
{
   const PickerColorOption *ptrOption;

   ptrOption = readPickerSelection( COLOR_BACKGROUND_KEYS,
                                    aryBackgroundPickerOptions,
                                    sizeof( aryBackgroundPickerOptions ) / sizeof( aryBackgroundPickerOptions[0] ),
                                    printBackgroundPickerMenu );
   if ( ptrOption == NULL )
   {
      return 0;
   }

   stdPrintf( "%s\r\n", ptrOption->ptrDisplayName );
   printAnsiBackgroundColorValue( ptrOption->colorValue );
   stdPrintf( "\n" );

   return ptrOption->colorValue;
}

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

      snprintf( aryPromptText, sizeof( aryPromptText ), "\r\n<P>resets  <G>eneral  <I>nput  po<S>ts  <O>ptions  <X>press  <Q>uit" );
      printThemedMnemonicText( aryPromptText, color.number );
      printThemedMnemonicText( "\r\nColor config -> ", color.forum );
      printAnsiForegroundColorValue( color.text );

      switch ( readValidatedMenuKey( COLOR_MAIN_MENU_KEYS ) )
      {
         case 'g':
            stdPrintf( "General\r\n\n" );
            generalColorConfig();
            break;
         case 'i':
            stdPrintf( "Input\r\n\n" );
            inputColorConfig();
            break;
         case 'o':
            stdPrintf( "Options\r\n\n" );
            colorOptions();
            break;
         case 'p':
            presetColorConfig();
            break;
         case 's':
            stdPrintf( "Post colors\r\n\n" );
            postColorConfig();
            break;
         case 'x':
            stdPrintf( "Express colors\r\n\n" );
            expressColorConfig();
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
   if ( flagsConfiguration.shouldUseAnsi )
   {
      printAnsiDisplayStateValue( lastColor, color.background );
   }
}

/// @brief Prompt for a foreground color selection.
///
/// @return Selected foreground color value.
int colorPicker( void )
{
   const PickerColorOption *ptrOption;

   ptrOption = readPickerSelection( COLOR_FOREGROUND_KEYS,
                                    aryForegroundPickerOptions,
                                    sizeof( aryForegroundPickerOptions ) / sizeof( aryForegroundPickerOptions[0] ),
                                    printForegroundPickerMenu );
   if ( ptrOption == NULL )
   {
      return 0;
   }

   stdPrintf( "%s\r\n\n", ptrOption->ptrDisplayName );
   return ptrOption->colorValue;
}

/// @brief Configure the text and name colors for express messages.
///
/// @param ptrTextColor Express text color to update.
/// @param ptrNameColor Express sender name color to update.
/// @param ptrPreviewName Preview name used while showing samples.
///
/// @return This helper does not return a value.
static void configureExpressColors( int *ptrTextColor, int *ptrNameColor,
                                    const char *ptrPreviewName )
{
   while ( true )
   {
      printExpressColorPreview( *ptrTextColor, *ptrNameColor, ptrPreviewName );

      switch ( expressColorMenu() )
      {
         case 'q':
         case ' ':
         case '\n':
            return;
         case 'n':
            *ptrNameColor = colorPicker();
            break;
         case 't':
            *ptrTextColor = colorPicker();
            break;
         default:
            break;
      }
   }
}

/// @brief Configure the date, text, and name colors for posts.
///
/// @param ptrDateColor Post date color to update.
/// @param ptrTextColor Post body color to update.
/// @param ptrNameColor Post author color to update.
/// @param ptrPreviewName Preview name used while showing samples.
///
/// @return This helper does not return a value.
static void configurePostColors( int *ptrDateColor, int *ptrTextColor,
                                 int *ptrNameColor, const char *ptrPreviewName )
{
   while ( true )
   {
      postColorPreview( *ptrDateColor, *ptrTextColor, *ptrNameColor,
                        ptrPreviewName );

      switch ( postColorMenu() )
      {
         case 'q':
         case ' ':
         case '\n':
            return;
         case 'd':
            *ptrDateColor = colorPicker();
            break;
         case 'n':
            *ptrNameColor = colorPicker();
            break;
         case 't':
            *ptrTextColor = colorPicker();
            break;
         default:
            break;
      }
   }
}

/// @brief Run the express color configuration flow.
///
/// @return This function does not return a value.
void expressColorConfig( void )
{
   while ( true )
   {
      switch ( userOrFriend() )
      {
         case 'u':
            expressUserColorConfig();
            break;
         case 'f':
            expressFriendColorConfig();
            break;
         default:
            return;
      }
   }
}

/// @brief Prompt for an express color menu action.
///
/// @return Selected express color menu key.
char expressColorMenu( void )
{
   int inputChar;
   char aryPromptText[100];

   snprintf( aryPromptText, sizeof( aryPromptText ), "\r\n<N>ame  <T>ext  <Q>uit -> " );
   printThemedMnemonicText( aryPromptText, color.number );
   printAnsiForegroundColorValue( color.text );

   inputChar = readValidatedMenuKey( COLOR_EXPRESS_MENU_KEYS );

   switch ( inputChar )
   {
      case 'n':
         stdPrintf( "Name\r\n\n" );
         break;
      case 't':
         stdPrintf( "Text\r\n\n" );
         break;
      case 'q':
      case ' ':
      case '\n':
         stdPrintf( "Quit\r\n\n" );
         break;
      default:
         break;
   }

   return (char)inputChar;
}

/// @brief Configure express colors for friend messages.
///
/// @return This function does not return a value.
void expressFriendColorConfig( void )
{
   configureExpressColors( &color.expressFriendText, &color.expressFriendName,
                           A_FRIEND );
}

/// @brief Configure express colors for non-friend messages.
///
/// @return This function does not return a value.
void expressUserColorConfig( void )
{
   configureExpressColors( &color.expressText, &color.expressName, A_USER );
}

/// @brief Find a picker option by its menu key.
///
/// @param ptrOptions Picker option table.
/// @param itemCount Number of picker options.
/// @param keyChar Menu key to resolve.
///
/// @return Matching picker option, or `NULL` if the key is unknown.
static const PickerColorOption *findPickerColorOption( const PickerColorOption *ptrOptions,
                                                       size_t itemCount,
                                                       int keyChar )
{
   size_t itemIndex;

   for ( itemIndex = 0; itemIndex < itemCount; itemIndex++ )
   {
      if ( ptrOptions[itemIndex].keyChar == keyChar )
      {
         return &ptrOptions[itemIndex];
      }
   }

   return NULL;
}

/// @brief Configure the general theme colors.
///
/// @return This function does not return a value.
void generalColorConfig( void )
{
   char aryPromptText[100];

   while ( true )
   {
      printGeneralColorPreview();

      snprintf( aryPromptText, sizeof( aryPromptText ), "\r\n<B>ackground  <E>rror  <F>orum  <N>umber  <T>ext  <Q>uit -> " );
      printThemedMnemonicText( aryPromptText, color.number );
      printAnsiForegroundColorValue( color.text );

      switch ( readValidatedMenuKey( COLOR_GENERAL_MENU_KEYS ) )
      {
         case 'q':
         case ' ':
         case '\n':
            stdPrintf( "Quit\r\n" );
            return;
         case 'b':
            stdPrintf( "Background\r\n\n" );
            color.background = backgroundPicker();
            break;
         case 'e':
            stdPrintf( "Error\r\n\n" );
            color.errorTextColor = colorPicker();
            break;
         case 'f':
            stdPrintf( "Forum\r\n\n" );
            color.forum = colorPicker();
            break;
         case 'n':
            stdPrintf( "Number\r\n\n" );
            color.number = colorPicker();
            break;
         case 't':
            stdPrintf( "Text\r\n\n" );
            color.text = colorPicker();
            break;
         default:
            break;
      }
   }
}

/// @brief Configure input prompt and completion colors.
///
/// @return This function does not return a value.
void inputColorConfig( void )
{
   char aryPromptText[100];

   while ( true )
   {
      printInputColorPreview();

      snprintf( aryPromptText, sizeof( aryPromptText ), "\r\n<T>ext  <C>ompletion  <Q>uit -> " );
      printThemedMnemonicText( aryPromptText, color.number );
      printAnsiForegroundColorValue( color.text );

      switch ( readValidatedMenuKey( COLOR_INPUT_MENU_KEYS ) )
      {
         case 'q':
         case ' ':
         case '\n':
            stdPrintf( "Quit\r\n" );
            return;
         case 'c':
            stdPrintf( "Completion\r\n\n" );
            color.inputHighlight = colorPicker();
            break;
         case 't':
            stdPrintf( "Text\r\n\n" );
            color.inputText = colorPicker();
            break;
         default:
            break;
      }
   }
}

/// @brief Run the post color configuration flow.
///
/// @return This function does not return a value.
void postColorConfig( void )
{
   while ( true )
   {
      switch ( userOrFriend() )
      {
         case 'u':
            postUserColorConfig();
            break;
         case 'f':
            postFriendColorConfig();
            break;
         default:
            return;
      }
   }
}

/// @brief Prompt for a post color menu action.
///
/// @return Selected post color menu key.
char postColorMenu( void )
{
   int inputChar;
   char aryPromptText[100];

   snprintf( aryPromptText, sizeof( aryPromptText ), "\r\n<D>ate  <N>ame  <T>ext  <Q>uit -> " );
   printThemedMnemonicText( aryPromptText, color.number );
   printAnsiForegroundColorValue( color.text );

   inputChar = readValidatedMenuKey( COLOR_POST_MENU_KEYS );

   switch ( inputChar )
   {
      case 'd':
         stdPrintf( "Date\r\n\n" );
         break;
      case 'n':
         stdPrintf( "Name\r\n\n" );
         break;
      case 't':
         stdPrintf( "Text\r\n\n" );
         break;
      case 'q':
      case ' ':
      case '\n':
         stdPrintf( "Quit\r\n\n" );
         break;
      default:
         break;
   }

   return (char)inputChar;
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
   stdPrintf( "Jan  1, 2000 11:01" );
   printAnsiForegroundColorValue( textColor );
   stdPrintf( " from " );
   printAnsiForegroundColorValue( nameColor );
   stdPrintf( "%s", ptrName );
   printAnsiForegroundColorValue( textColor );
   stdPrintf( "\r\nHi there!\r\n" );
   printAnsiForegroundColorValue( color.forum );
   stdPrintf( "[Lobby> msg #1]\r\n" );
}

/// @brief Configure post colors for friend posts.
///
/// @return This function does not return a value.
void postFriendColorConfig( void )
{
   configurePostColors( &color.postFriendDate, &color.postFriendText,
                        &color.postFriendName, A_FRIEND );
}

/// @brief Configure post colors for non-friend posts.
///
/// @return This function does not return a value.
void postUserColorConfig( void )
{
   configurePostColors( &color.postDate, &color.postText,
                        &color.postName, A_USER );
}

/// @brief Show preset themes and apply the selected preset.
///
/// @return This helper does not return a value.
static void presetColorConfig( void )
{
   size_t optionIndex;

   stdPrintf( "Color presets\r\n\n" );
   for ( optionIndex = 0;
         optionIndex < sizeof( aryPresetMenuOptions ) / sizeof( aryPresetMenuOptions[0] );
         optionIndex++ )
   {
      printPresetMenuItem( &aryPresetMenuOptions[optionIndex] );
   }
   printAnsiDisplayStateValue( color.text, color.background );
   printThemedMnemonicText( " Quit\r\n", color.number );
   printThemedMnemonicText( "Select preset -> ", color.forum );
   printAnsiForegroundColorValue( color.text );

   switch ( readValidatedMenuKey( COLOR_RESET_MENU_KEYS ) )
   {
      case 'd':
         stdPrintf( "Default\r\n" );
         defaultColors( 1 );
         break;
      case 'b':
         stdPrintf( "Brilliant\r\n" );
         brilliantColors();
         break;
      case 'e':
         stdPrintf( "Everforest Dark\r\n" );
         everforestDarkColors();
         break;
      case 'f':
         stdPrintf( "Everforest Light\r\n" );
         everforestLightColors();
         break;
      case 'l':
         stdPrintf( "Catppuccin Latte\r\n" );
         catppuccinLatteColors();
         break;
      case 'm':
         stdPrintf( "Catppuccin Macchiato\r\n" );
         catppuccinMacchiatoColors();
         break;
      case 'c':
         stdPrintf( "Colorblind\r\n" );
         colorblindColors();
         break;
      case 'h':
         stdPrintf( "Hotdog Stand\r\n" );
         hotDogColors();
         break;
      case 'q':
      case ' ':
      case '\n':
         stdPrintf( "Quit\r\n" );
         break;
      default:
         break;
   }
}

/// @brief Print the background picker menu.
///
/// @return This helper does not return a value.
static void printBackgroundPickerMenu( void )
{
   printThemedMnemonicText( "\r\n[<K>] Black           [<R>] Red             [<G>] Green           [<Y>] Yellow\r\n", color.number );
   printThemedMnemonicText( "[<B>] Blue            [<M>] Magenta         [<C>] Cyan            [<W>] White\r\n", color.number );
   printThemedMnemonicText( "[<1>] Bright black   [<2>] Bright red      [<3>] Bright green    [<4>] Bright yellow\r\n", color.number );
   printThemedMnemonicText( "[<5>] Bright blue    [<6>] Bright magenta  [<7>] Bright cyan     [<8>] Bright white\r\n", color.number );
   printThemedMnemonicText( "[<D>] Default\r\n", color.number );
   printThemedMnemonicText( "Select background -> ", color.forum );
   printAnsiForegroundColorValue( color.text );
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
   stdPrintf( "*** Message (#1) from " );
   printAnsiForegroundColorValue( nameColor );
   stdPrintf( "%s", ptrName );
   printAnsiForegroundColorValue( textColor );
   stdPrintf( " at 11:01 ***\r\n>Hi there!\r\n" );
}

/// @brief Print the foreground picker menu.
///
/// @return This helper does not return a value.
static void printForegroundPickerMenu( void )
{
   printThemedMnemonicText( "\r\n[<K>] Black           [<R>] Red             [<G>] Green           [<Y>] Yellow\r\n", color.number );
   printThemedMnemonicText( "[<B>] Blue            [<M>] Magenta         [<C>] Cyan            [<W>] White\r\n", color.number );
   printThemedMnemonicText( "[<1>] Bright black   [<2>] Bright red      [<3>] Bright green    [<4>] Bright yellow\r\n", color.number );
   printThemedMnemonicText( "[<5>] Bright blue    [<6>] Bright magenta  [<7>] Bright cyan     [<8>] Bright white\r\n", color.number );
   printThemedMnemonicText( "Select color -> ", color.forum );
   printAnsiForegroundColorValue( color.text );
}

/// @brief Print a preview of the general theme colors.
///
/// @return This helper does not return a value.
static void printGeneralColorPreview( void )
{
   printAnsiDisplayStateValue( color.forum, color.background );
   stdPrintf( "Lobby> " );
   printAnsiForegroundColorValue( color.text );
   stdPrintf( "Enter message\r\n\n" );
   printAnsiForegroundColorValue( color.errorTextColor );
   stdPrintf( "Only Sysops may post to the lobby\r\n\n" );
   printAnsiForegroundColorValue( color.forum );
   stdPrintf( "Lobby> " );
   printAnsiForegroundColorValue( color.text );
   stdPrintf( "Goto " );
   printAnsiForegroundColorValue( color.forum );
   stdPrintf( "[Babble]  " );
   printAnsiForegroundColorValue( color.number );
   stdPrintf( "150" );
   printAnsiForegroundColorValue( color.text );
   stdPrintf( " messages, " );
   printAnsiForegroundColorValue( color.number );
   stdPrintf( "1" );
   printAnsiForegroundColorValue( color.text );
   stdPrintf( " new\r\n" );
}

/// @brief Print a preview of the input and completion colors.
///
/// @return This helper does not return a value.
static void printInputColorPreview( void )
{
   printAnsiForegroundColorValue( color.text );
   stdPrintf( "Message eXpress\r\nRecipient: " );
   printAnsiForegroundColorValue( color.inputText );
   stdPrintf( "Exam" );
   printAnsiForegroundColorValue( color.inputHighlight );
   stdPrintf( "ple User\r\n" );
   printAnsiForegroundColorValue( color.inputText );
   stdPrintf( ">Hi there!\r\n" );
   printAnsiForegroundColorValue( color.text );
   stdPrintf( "Message received by Example User.\r\n" );
}

/// @brief Print one preset theme menu entry.
///
/// @param ptrOption Preset option to display.
///
/// @return This helper does not return a value.
static void printPresetMenuItem( const PresetMenuOption *ptrOption )
{
   size_t labelIndex;
   size_t labelLength;

   stdPrintf( " " );
   printAnsiDisplayStateValue( ptrOption->textColor, ptrOption->backgroundColor );
   labelLength = strlen( ptrOption->ptrLabel );
   for ( labelIndex = 0; labelIndex < labelLength; labelIndex++ )
   {
      if ( labelIndex == ptrOption->mnemonicIndex )
      {
         printAnsiForegroundColorValue( ptrOption->accentColor );
      }
      else
      {
         printAnsiForegroundColorValue( ptrOption->textColor );
      }
      stdPutChar( ptrOption->ptrLabel[labelIndex] );
   }
   stdPrintf( "\r\n" );
}

/// @brief Read and resolve one picker selection from a color menu.
///
/// @param ptrAllowedKeys Allowed menu key set.
/// @param ptrOptions Picker option table.
/// @param itemCount Number of picker options.
/// @param printMenu Menu printer to call before reading input.
///
/// @return Matching picker option, or `NULL` if the selection was not resolved.
static const PickerColorOption *readPickerSelection( const char *ptrAllowedKeys,
                                                     const PickerColorOption *ptrOptions,
                                                     size_t itemCount,
                                                     void ( *printMenu )( void ) )
{
   int inputChar;

   printMenu();
   inputChar = readValidatedMenuKey( ptrAllowedKeys );

   return findPickerColorOption( ptrOptions, itemCount, inputChar );
}

/// @brief Prompt for whether to configure user or friend preview colors.
///
/// @return Selected menu key.
char userOrFriend( void )
{
   int inputChar;

   printThemedMnemonicText( "Configure for ", color.text );
   printThemedMnemonicText( "<U>ser", color.number );
   printThemedMnemonicText( " or ", color.text );
   printThemedMnemonicText( "<F>riend", color.number );
   printThemedMnemonicText( " -> ", color.forum );
   printAnsiForegroundColorValue( color.text );

   inputChar = readValidatedMenuKey( COLOR_USER_OR_FRIEND_KEYS );

   switch ( inputChar )
   {
      case 'u':
         stdPrintf( "User\r\n\n" );
         break;
      case 'f':
         stdPrintf( "Friend\r\n\n" );
         break;
      default:
         stdPrintf( "Quit\r\n" );
         break;
   }

   return (char)inputChar;
}
