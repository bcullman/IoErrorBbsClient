/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file parses .bbsrc and applies the configured settings.
 */
#include "bbsrc.h"
#include "client.h"
#include "client_globals.h"
#include "color.h"
#include "config_globals.h"
#include "config_menu.h"
#include "defs.h"
#include "filter_globals.h"
#include "utility.h"
#define MAX_LINE_LENGTH 512
#define FRIEND_COMMAND_PREFIX_LEN 7
#define FRIEND_NAME_PARSE_LENGTH 19
#define FRIEND_INFO_OFFSET 30
#define FRIEND_INFO_COPY_LENGTH 53
#define FRIEND_RECORD_MAGIC 0x3231

typedef enum
{
   BBRC_CMD_UNKNOWN = 0,
   BBRC_CMD_AWAYKEY,
   BBRC_CMD_AUTOANSI,
   BBRC_CMD_AUTONAME,
   BBRC_CMD_BOLD,
   BBRC_CMD_BROWSER,
   BBRC_CMD_CAPTURE,
   BBRC_CMD_CLICKABLE_URLS,
   BBRC_CMD_COLOR,
   BBRC_CMD_COMMANDKEY,
   BBRC_CMD_AUTOCOMPLETE,
   BBRC_CMD_EDITOR,
   BBRC_CMD_ENEMY,
   BBRC_CMD_FRIEND,
   BBRC_CMD_KEYCHAIN,
   BBRC_CMD_KEYMAP,
   BBRC_CMD_MACRO,
   BBRC_CMD_NEW_AWAY,
   BBRC_CMD_OLD_AWAY,
   BBRC_CMD_QUIT,
   BBRC_CMD_REREAD,
   BBRC_CMD_SHELL,
   BBRC_CMD_SITE,
   BBRC_CMD_SQUELCH,
   BBRC_CMD_SUSP,
   BBRC_CMD_TCP_KEEPALIVE,
   BBRC_CMD_TITLEBAR,
   BBRC_CMD_URL,
   BBRC_CMD_SCREENREADER,
   BBRC_CMD_VERSION,
   BBRC_CMD_XLAND,
   BBRC_CMD_XWRAP
} BbsRcCommandId;

typedef struct
{
   const char *ptrPrefix;
   size_t prefixLength;
   BbsRcCommandId commandId;
} BbsRcCommandSpec;

typedef enum
{
   BBRC_OPTION_INVALID = -1,
   BBRC_OPTION_DISABLED = 0,
   BBRC_OPTION_ENABLED = 1
} BbsRcOptionValue;

typedef struct
{
   int lineNumber;
   int reads;
   int tmpVersion;
   bool shouldShowBrowserMigrationNotice;
   bool shouldRewriteBbsRc;
} BbsRcReadState;

static bool addFriendFromLine( const char *ptrLine );
static void applyBbsRcKeyDefaults( void );
static int ctrl( const char *ptrToken );
static BbsRcCommandId detectBbsRcCommand( const char *ptrLine );
static void ensureDefaultAwayMessage( void );
static bool finalizeBbsRcRead( BbsRcReadState *ptrState );
static void initializeBbsRcDefaults( void );
static void initializeBbsRcLists( void );
static bool isNewAwayMessageCommand( const char *ptrLine );
static BbsRcOptionValue parseBooleanSettingValue( const char *ptrLine, size_t prefixLength, const char *ptrSettingName, bool shouldAllowAnyNonZeroValue );
static bool parseColorScheme( const char *ptrLine );
static bool parseNamedColorScheme( const char *ptrColorSpec );
static bool processBbsRcCommandLine( const char *ptrLine, BbsRcReadState *ptrState );
static bool processBbsRcConnectionCommand( BbsRcCommandId commandId,
                                           const char *ptrLine,
                                           BbsRcReadState *ptrState );
static bool processBbsRcHotkeyCommand( BbsRcCommandId commandId,
                                       const char *ptrLine );
static bool processBbsRcListCommand( BbsRcCommandId commandId,
                                     const char *ptrLine );
static bool processBbsRcMacroCommand( BbsRcCommandId commandId,
                                      const char *ptrLine,
                                      const BbsRcReadState *ptrState );
static bool processBbsRcSettingCommand( BbsRcCommandId commandId,
                                        const char *ptrLine,
                                        BbsRcReadState *ptrState );
static bool readLegacyBbsFriends( char *ptrLine, BbsRcReadState *ptrState );
static void writeDecodedBbsRcText( const char *ptrToken, char *ptrWriteBuffer,
                                   char ( *aryAwayBuffers )[80] );
static void warnAboutBbsRcConflicts( void );

/// @brief Parse and add one `friend` directive from `.bbsrc`.
///
/// @param ptrLine Raw `friend` line from the config file.
///
/// @return `true` on success, otherwise `false`.
static bool addFriendFromLine( const char *ptrLine )
{
   friend *ptrFriend;

   if ( strlen( ptrLine ) == FRIEND_COMMAND_PREFIX_LEN )
   {
      stdPrintf( "Empty username in 'friend'.\n" );
      return true;
   }
   if ( slistFind( friendList, (void *)( ptrLine + FRIEND_COMMAND_PREFIX_LEN ), fStrCompareVoid ) != -1 )
   {
      stdPrintf( "Duplicate username in 'friend'.\n" );
      return true;
   }

   ptrFriend = (friend *)calloc( 1, sizeof( friend ) );
   if ( !ptrFriend )
   {
      fatalExit( "Out of memory adding 'friend'!\n", "Fatal error" );
      return false;
   }

   if ( strlen( ptrLine ) > FRIEND_INFO_OFFSET )
   {
      int nameLength;
      size_t nameLengthToCopy;

      strncpy( ptrFriend->info, ptrLine + FRIEND_INFO_OFFSET, FRIEND_INFO_COPY_LENGTH );
      nameLengthToCopy = 0;
      for ( nameLength = FRIEND_NAME_PARSE_LENGTH; nameLength > 0; nameLength-- )
      {
         if ( ptrLine[FRIEND_COMMAND_PREFIX_LEN + nameLength] == ' ' )
         {
            nameLengthToCopy = (size_t)nameLength;
         }
         else
         {
            break;
         }
      }
      strncpy( ptrFriend->name, ptrLine + FRIEND_COMMAND_PREFIX_LEN, nameLengthToCopy );
   }
   else
   {
      strncpy( ptrFriend->name, ptrLine + FRIEND_COMMAND_PREFIX_LEN, FRIEND_NAME_PARSE_LENGTH );
      snprintf( ptrFriend->info, sizeof( ptrFriend->info ), "%s", "(None)" );
   }

   ptrFriend->magic = FRIEND_RECORD_MAGIC;
   if ( !slistAddItem( friendList, ptrFriend, 1 ) )
   {
      fatalExit( "Can't add 'friend' to list!\n", "Fatal error" );
      return false;
   }
   return true;
}

/// @brief Apply default hotkey values when the config did not define them.
///
/// @return This helper does not return a value.
static void applyBbsRcKeyDefaults( void )
{
   if ( commandKey == -1 )
   {
      commandKey = ESC;
   }
   if ( awayKey == -1 )
   {
      awayKey = 'a';
   }
   if ( quitKey == -1 )
   {
      quitKey = CTRL_D;
   }
   if ( suspKey == -1 )
   {
      suspKey = CTRL_Z;
   }
   if ( captureKey == -1 )
   {
      captureKey = 'c';
   }
   if ( shellKey == -1 )
   {
      shellKey = '!';
   }
   if ( browserKey == -1 )
   {
      browserKey = 'w';
   }
}

/// @brief Decode a config token into a control-key value.
///
/// `^X` spellings and literal control characters are normalized to the same
/// control-key result.
///
/// @param ptrToken Token to decode.
///
/// @return Parsed control-key value.
static int ctrl( const char *ptrToken )
{
   int parseIndex = *ptrToken;

   if ( parseIndex == '^' )
   {
      if ( ( ( parseIndex = *++ptrToken ) >= '@' && parseIndex <= '_' ) || parseIndex == '?' )
      {
         parseIndex ^= 0x40;
      }
      else if ( parseIndex >= 'a' && parseIndex <= 'z' )
      {
         parseIndex ^= 0x60;
      }
   }
   if ( parseIndex == '\r' )
   {
      parseIndex = '\n';
   }
   return ( parseIndex );
}

/// @brief Detect which `.bbsrc` command family a raw line belongs to.
///
/// @param ptrLine Raw config line.
///
/// @return Matching command identifier.
static BbsRcCommandId detectBbsRcCommand( const char *ptrLine )
{
   static const BbsRcCommandSpec aryCommands[] =
      {
         { "reread ", 7, BBRC_CMD_REREAD },
         { "xwrap ", 6, BBRC_CMD_XWRAP },
         { "bold", 4, BBRC_CMD_BOLD },
         { "xland", 5, BBRC_CMD_XLAND },
         { "version ", 8, BBRC_CMD_VERSION },
         { "squelch ", 8, BBRC_CMD_SQUELCH },
         { "keepalive", 9, BBRC_CMD_TCP_KEEPALIVE },
         { "clickableurls", 13, BBRC_CMD_CLICKABLE_URLS },
         { "titlebar", 8, BBRC_CMD_TITLEBAR },
         { "screenreader", 12, BBRC_CMD_SCREENREADER },
         { "autocomplete", 12, BBRC_CMD_AUTOCOMPLETE },
         { "keychain", 8, BBRC_CMD_KEYCHAIN },
         { "urlsummary", 10, BBRC_CMD_CLICKABLE_URLS },
         { "color ", 6, BBRC_CMD_COLOR },
         { "aryAutoName ", sizeof( "aryAutoName " ) - 1, BBRC_CMD_AUTONAME },
         { "autoansi", 9, BBRC_CMD_AUTOANSI },
         { "aryBrowser ", sizeof( "aryBrowser " ) - 1, BBRC_CMD_BROWSER },
         { "aryEditor ", sizeof( "aryEditor " ) - 1, BBRC_CMD_EDITOR },
         { "site ", 5, BBRC_CMD_SITE },
         { "friend ", FRIEND_COMMAND_PREFIX_LEN, BBRC_CMD_FRIEND },
         { "enemy ", 6, BBRC_CMD_ENEMY },
         { "commandkey ", 11, BBRC_CMD_COMMANDKEY },
         { "macrokey ", 9, BBRC_CMD_COMMANDKEY },
         { "awaykey ", 8, BBRC_CMD_AWAYKEY },
         { "quit ", 5, BBRC_CMD_QUIT },
         { "susp ", 5, BBRC_CMD_SUSP },
         { "capture ", 8, BBRC_CMD_CAPTURE },
         { "aryKeyMap ", sizeof( "aryKeyMap " ) - 1, BBRC_CMD_KEYMAP },
         { "url ", 4, BBRC_CMD_URL },
         { "aryMacro ", sizeof( "aryMacro " ) - 1, BBRC_CMD_MACRO },
         { "aryAwayMessageLines ", sizeof( "aryAwayMessageLines " ) - 1, BBRC_CMD_OLD_AWAY },
         { "shellkey ", 9, BBRC_CMD_SHELL },
         { "aryShell ", sizeof( "aryShell " ) - 1, BBRC_CMD_SHELL } };
   size_t itemIndex;

   if ( isNewAwayMessageCommand( ptrLine ) )
   {
      return BBRC_CMD_NEW_AWAY;
   }
   for ( itemIndex = 0; itemIndex < sizeof( aryCommands ) / sizeof( aryCommands[0] ); itemIndex++ )
   {
      if ( !strncmp( ptrLine, aryCommands[itemIndex].ptrPrefix,
                     aryCommands[itemIndex].prefixLength ) )
      {
         return aryCommands[itemIndex].commandId;
      }
   }
   return BBRC_CMD_UNKNOWN;
}

/// @brief Ensure there is at least one away-message line configured.
///
/// @return This helper does not return a value.
static void ensureDefaultAwayMessage( void )
{
   if ( !**aryAwayMessageLines )
   {
      snprintf( aryAwayMessageLines[0], sizeof( aryAwayMessageLines[0] ),
                "%s", "I'm away from my keyboard right now." );
      *aryAwayMessageLines[1] = 0;
   }
}

/// @brief Finalize config state after `.bbsrc` parsing completes.
///
/// @param ptrState Running read state to finalize.
///
/// @return `true` on success, otherwise `false`.
static bool finalizeBbsRcRead( BbsRcReadState *ptrState )
{
   applyBbsRcKeyDefaults();
   ensureDefaultAwayMessage();

   defaultColors( 0 );
   warnAboutBbsRcConflicts();

   for ( unsigned int friendIndex = 0; friendIndex < friendList->nitems; friendIndex++ )
   {
      const friend *ptrFriend = friendList->items[friendIndex];
      char *ptrNameCopy;

      if ( !( ptrNameCopy = (char *)calloc( 1, strlen( ptrFriend->name ) + 1 ) ) )
      {
         fatalExit( "Out of memory for list copy!\r\n", "Fatal error" );
         return false;
      }
      snprintf( ptrNameCopy, strlen( ptrFriend->name ) + 1, "%s", ptrFriend->name );
      if ( !( slistAddItem( whoList, ptrNameCopy, 1 ) ) )
      {
         fatalExit( "Out of memory adding item in list copy!\r\n", "Fatal error" );
         return false;
      }
   }

   slistSort( friendList );
   slistSort( enemyList );
   slistSort( whoList );

   if ( !*aryBbsHost )
   {
      snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", BBS_HOSTNAME );
      bbsPort = BBS_PORT_NUMBER;
   }
   if ( !*aryEditor )
   {
      snprintf( aryEditor, sizeof( aryEditor ), "%s", DEFAULT_EDITOR_CONFIG_VALUE );
   }
   if ( version != ptrState->tmpVersion )
   {
      if ( ptrState->reads )
      {
         setup( ptrState->tmpVersion );
      }
      else
      {
         setup( -1 );
      }
   }
   else if ( !flagsConfiguration.hasScreenReaderModeSetting )
   {
      promptForScreenReaderModeIfUnset();
      ptrState->shouldRewriteBbsRc = true;
   }
   if ( !flagsConfiguration.hasTitleBarSetting )
   {
      flagsConfiguration.hasTitleBarSetting = 1;
      ptrState->shouldRewriteBbsRc = true;
   }
   if ( !flagsConfiguration.hasNameAutocompleteSetting )
   {
      defaultNameAutocompleteIfUnset();
      ptrState->shouldRewriteBbsRc = true;
   }
   if ( ptrState->shouldShowBrowserMigrationNotice )
   {
      stdPrintf( "IMPORTANT: Your browser preference was removed due to client updates.\n" );
      stdPrintf( "The client now relies on terminal links and the macOS default browser.\n" );
   }
   if ( ptrState->shouldRewriteBbsRc && !isBbsRcReadOnly )
   {
      writeBbsRc();
   }
   if ( isLoginShell )
   {
      setTerm();
      configBbsRc();
      resetTerm();
   }

   return true;
}

/// @brief Initialize default config values before `.bbsrc` parsing begins.
///
/// @return This helper does not return a value.
static void initializeBbsRcDefaults( void )
{
   int parseIndex;

   version = INT_VERSION;
   commandKey = -1;
   shellKey = -1;
   captureKey = -1;
   suspKey = -1;
   quitKey = -1;
   awayKey = -1;
   browserKey = -1;

   for ( parseIndex = 0; parseIndex <= 127; parseIndex++ )
   {
      aryKeyMap[parseIndex] = (char)parseIndex;
      *aryMacro[parseIndex] = 0;
   }

   isXland = 1;
   xlandQueue = newQueue( 21, MAX_USER_NAME_HISTORY_COUNT );
   if ( !xlandQueue )
   {
      isXland = 0;
   }
   urlQueue = newQueue( 1024, 10 );

   isAutoLoggedIn = 0;
   *aryAutoName = 0;

   *aryEditor = 0;
   *aryBbsHost = 0;
   ptrBbsRc = findBbsRc();
   bbsFriends = findBbsFriends();
   flagsConfiguration.shouldUseAnsi = 0;
   flagsConfiguration.shouldUseBold = 0;
   flagsConfiguration.shouldDisableBold = 0;
   flagsConfiguration.isMorePromptActive = 0;
   flagsConfiguration.shouldAutoAnswerAnsiPrompt = 0;
   flagsConfiguration.shouldUseTcpKeepalive = 1;
   flagsConfiguration.shouldEnableClickableUrls = 1;
   flagsConfiguration.shouldEnableTitleBar = 1;
   flagsConfiguration.shouldUseKeychain = 0;
   flagsConfiguration.hasTitleBarSetting = 0;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   flagsConfiguration.hasScreenReaderModeSetting = 0;
   flagsConfiguration.shouldEnableNameAutocomplete = 1;
   flagsConfiguration.hasNameAutocompleteSetting = 0;

   defaultColors( 1 );

   whoListProgress = 0;
   ptrPostBuffer = 0;
   postHeaderActive = 0;
   highestExpressMessageId = 0;
   isExpressMessageHeaderActive = 0;
   postProgressState = 0;
   isPostJustEnded = 0;
   isExpressMessageInProgress = 0;
   shouldSendExpressMessage = 0;
   pendingLinesToEat = 0;
   ptrExpressMessageBuffer = aryExpressMessageBuffer;
}

/// @brief Create the list structures used while reading `.bbsrc`.
///
/// @return This helper does not return a value.
static void initializeBbsRcLists( void )
{
   if ( !( friendList = slistCreate( 0, fSortCompareVoid ) ) )
   {
      fatalExit( "Can't create 'friend' list!\n", "Fatal error" );
   }
   if ( !( enemyList = slistCreate( 0, sortCompareVoid ) ) )
   {
      fatalExit( "Can't create 'enemy' list!\n", "Fatal error" );
   }
   if ( !( whoList = slistCreate( 0, sortCompareVoid ) ) )
   {
      fatalExit( "Can't create saved who list!\n", "Fatal error" );
   }
}

/// @brief Check whether a line is one of the numbered away-message commands.
///
/// @param ptrLine Raw config line.
///
/// @return `true` if the line is a numbered away-message command, otherwise `false`.
static bool isNewAwayMessageCommand( const char *ptrLine )
{
   return ptrLine[0] == 'a' && ptrLine[1] >= '1' &&
          ptrLine[1] <= '5' && ptrLine[2] == ' ';
}

/// @brief Parse an on/off-style `.bbsrc` setting value.
///
/// @param ptrLine Raw config line.
/// @param prefixLength Length of the directive prefix.
/// @param ptrSettingName Setting name used in error messages.
/// @param shouldAllowAnyNonZeroValue Non-zero to treat any non-zero digit as enabled.
///
/// @return Parsed setting value enum.
static BbsRcOptionValue parseBooleanSettingValue( const char *ptrLine, size_t prefixLength, const char *ptrSettingName, bool shouldAllowAnyNonZeroValue )
{
   const char *ptrValue;

   if ( strlen( ptrLine ) == prefixLength )
   {
      return BBRC_OPTION_ENABLED;
   }
   if ( ptrLine[prefixLength] != ' ' )
   {
      stdPrintf( "Invalid definition of %s ignored.\n", ptrSettingName );
      return BBRC_OPTION_INVALID;
   }

   ptrValue = ptrLine + prefixLength + 1;
   while ( *ptrValue != '\0' && isspace( (unsigned char)*ptrValue ) )
   {
      ptrValue++;
   }

   if ( *ptrValue == '\0' || *ptrValue == '1' )
   {
      return BBRC_OPTION_ENABLED;
   }
   if ( *ptrValue == '0' )
   {
      return BBRC_OPTION_DISABLED;
   }
   if ( shouldAllowAnyNonZeroValue && isdigit( (unsigned char)*ptrValue ) )
   {
      return (BbsRcOptionValue)( atoi( ptrValue ) != 0 );
   }

   stdPrintf( "Invalid definition of %s ignored.\n", ptrSettingName );
   return BBRC_OPTION_INVALID;
}

/// @brief Parse a full `.bbsrc` color scheme into the active color fields.
///
/// @param ptrLine Raw `color` directive line.
///
/// @return `true` on success, otherwise `false`.
static bool parseColorScheme( const char *ptrLine )
{
   if ( strlen( ptrLine ) == 6 + COLOR_FIELD_COUNT )
   {
      int colorIndex;

      for ( colorIndex = 0; colorIndex < COLOR_FIELD_COUNT; colorIndex++ )
      {
         setColorFieldValue( colorIndex, colorValueFromLegacyDigit( ptrLine[6 + colorIndex] ) );
      }
      if ( ptrLine[6 + COLOR_BACKGROUND_INDEX] == '9' )
      {
         setColorFieldValue( COLOR_BACKGROUND_INDEX, COLOR_VALUE_DEFAULT );
      }
      return true;
   }

   return parseNamedColorScheme( ptrLine + 6 );
}

/// @brief Parse a named-color `.bbsrc` color scheme.
///
/// @param ptrColorSpec Color specification text after the directive prefix.
///
/// @return `true` on success, otherwise `false`.
static bool parseNamedColorScheme( const char *ptrColorSpec )
{
   int colorIndex;

   colorIndex = 0;
   while ( *ptrColorSpec != '\0' )
   {
      char aryToken[16];
      int colorValue;
      size_t tokenLength;
      const char *ptrTokenStart;

      while ( isspace( (unsigned char)*ptrColorSpec ) )
      {
         ptrColorSpec++;
      }
      if ( *ptrColorSpec == '\0' )
      {
         break;
      }
      if ( colorIndex >= COLOR_FIELD_COUNT )
      {
         return false;
      }

      ptrTokenStart = ptrColorSpec;
      while ( *ptrColorSpec != '\0' && !isspace( (unsigned char)*ptrColorSpec ) )
      {
         ptrColorSpec++;
      }
      tokenLength = (size_t)( ptrColorSpec - ptrTokenStart );
      if ( tokenLength == 0 || tokenLength >= sizeof( aryToken ) )
      {
         return false;
      }

      memcpy( aryToken, ptrTokenStart, tokenLength );
      aryToken[tokenLength] = '\0';

      if ( isdigit( (unsigned char)aryToken[0] ) )
      {
         size_t digitIndex;

         colorValue = 0;
         for ( digitIndex = 0; digitIndex < tokenLength; digitIndex++ )
         {
            if ( !isdigit( (unsigned char)aryToken[digitIndex] ) )
            {
               return false;
            }
            colorValue = colorValue * 10 + ( aryToken[digitIndex] - '0' );
         }
      }
      else
      {
         colorValue = colorValueFromName( aryToken );
      }
      if ( colorValue < 0 )
      {
         return false;
      }

      setColorFieldValue( colorIndex++, colorValue );
   }

   return colorIndex == COLOR_FIELD_COUNT;
}

/// @brief Dispatch one normalized `.bbsrc` line to the correct command handler.
///
/// @param ptrLine Normalized config line.
/// @param ptrState Running `.bbsrc` read state.
///
/// @return `true` to continue parsing, otherwise `false`.
static bool processBbsRcCommandLine( const char *ptrLine, BbsRcReadState *ptrState )
{
   int parseIndex;
   const char *ptrToken;
   BbsRcCommandId commandId = detectBbsRcCommand( ptrLine );
   bool isHandled = true;

   if ( processBbsRcSettingCommand( commandId, ptrLine, ptrState ) )
   {
      return true;
   }
   if ( processBbsRcConnectionCommand( commandId, ptrLine, ptrState ) )
   {
      return true;
   }
   if ( processBbsRcListCommand( commandId, ptrLine ) )
   {
      return true;
   }
   if ( processBbsRcHotkeyCommand( commandId, ptrLine ) )
   {
      return true;
   }
   if ( processBbsRcMacroCommand( commandId, ptrLine, ptrState ) )
   {
      return true;
   }

   switch ( commandId )
   {
      case BBRC_CMD_REREAD:
      case BBRC_CMD_XWRAP:
         break;

      case BBRC_CMD_KEYMAP:
         parseIndex = *( ptrLine + ( sizeof( "aryKeyMap " ) - 1 ) );
         ptrToken = ptrLine + sizeof( "aryKeyMap " );
         if ( *ptrToken++ == ' ' && parseIndex > 32 && parseIndex < 128 )
         {
            aryKeyMap[parseIndex] = *ptrToken;
         }
         else
         {
            stdPrintf( "Invalid value for 'aryKeyMap' ignored.\n" );
         }
         break;

      case BBRC_CMD_UNKNOWN:
      default:
         isHandled = false;
         break;
   }

   if ( !isHandled && *ptrLine != '#' && *ptrLine &&
        strncmp( ptrLine, "friend ", FRIEND_COMMAND_PREFIX_LEN ) )
   {
      stdPrintf( "Syntax error in .bbsrc file in line %d.\n", ptrState->lineNumber );
   }

   return true;
}

/// @brief Handle connection, editor, and site-related `.bbsrc` commands.
///
/// @param commandId Parsed command identifier.
/// @param ptrLine Raw config line.
/// @param ptrState Running `.bbsrc` read state.
///
/// @return `true` if the command was handled, otherwise `false`.
static bool processBbsRcConnectionCommand( BbsRcCommandId commandId,
                                           const char *ptrLine,
                                           BbsRcReadState *ptrState )
{
   switch ( commandId )
   {
      case BBRC_CMD_BROWSER:
         ptrState->shouldShowBrowserMigrationNotice = true;
         ptrState->shouldRewriteBbsRc = true;
         return true;

      case BBRC_CMD_EDITOR:
         if ( *aryEditor )
         {
            stdPrintf( "Multiple definition of 'aryEditor' ignored.\n" );
         }
         else
         {
            strncpy( aryEditor, ptrLine + ( sizeof( "aryEditor " ) - 1 ), 72 );
         }
         return true;

      case BBRC_CMD_SITE:
         if ( *aryBbsHost )
         {
            stdPrintf( "Multiple definition of 'site' ignored.\n" );
         }
         else
         {
            int parseIndex;

            for ( parseIndex = 5;
                  ( aryBbsHost[parseIndex - 5] = ptrLine[parseIndex] ) &&
                  ptrLine[parseIndex] != ' ' && parseIndex < 68;
                  parseIndex++ )
            {
               ;
            }
            if ( parseIndex == 68 || parseIndex == 5 )
            {
               stdPrintf( "Illegal hostname in 'site', using default.\n" );
               *aryBbsHost = 0;
            }
            else
            {
               const char *ptrPortSpec;

               aryBbsHost[parseIndex - 5] = 0;
               ptrPortSpec = ptrLine + parseIndex;
               while ( *ptrPortSpec != '\0' && isspace( (unsigned char)*ptrPortSpec ) )
               {
                  ptrPortSpec++;
               }
               if ( isdigit( (unsigned char)*ptrPortSpec ) )
               {
                  bbsPort = (unsigned short)atoi( ptrPortSpec );
               }
               else
               {
                  bbsPort = BBS_PORT_NUMBER;
               }
            }
            if ( !strcmp( aryBbsHost, "206.217.131.27" ) )
            {
               snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", BBS_HOSTNAME );
            }
         }
         return true;

      default:
         return false;
   }
}

/// @brief Handle hotkey-related `.bbsrc` commands.
///
/// @param commandId Parsed command identifier.
/// @param ptrLine Raw config line.
///
/// @return `true` if the command was handled, otherwise `false`.
static bool processBbsRcHotkeyCommand( BbsRcCommandId commandId,
                                       const char *ptrLine )
{
   switch ( commandId )
   {
      case BBRC_CMD_COMMANDKEY:
         if ( commandKey >= 0 )
         {
            stdPrintf( "Additional definition for 'commandkey' ignored.\n" );
         }
         else
         {
            if ( !strncmp( ptrLine, "macrokey ", 9 ) )
            {
               commandKey = ctrl( ptrLine + 9 );
            }
            else
            {
               commandKey = ctrl( ptrLine + 11 );
            }
            if ( findChar( "\0x01\0x03\0x04\0x05\b\n\r\0x11\0x13\0x15\0x17\0x18\0x19\0x1a\0x7f",
                           commandKey ) ||
                 commandKey >= ' ' )
            {
               stdPrintf( "Illegal value for 'commandkey', using default of 'Esc'.\n" );
               commandKey = 0x1b;
            }
         }
         return true;

      case BBRC_CMD_AWAYKEY:
         if ( awayKey >= 0 )
         {
            stdPrintf( "Additional definition for 'awaykey' ignored.\n" );
         }
         else
         {
            awayKey = ctrl( ptrLine + 8 );
         }
         return true;

      case BBRC_CMD_QUIT:
         if ( quitKey >= 0 )
         {
            stdPrintf( "Additional definition for 'quit' ignored.\n" );
         }
         else
         {
            quitKey = ctrl( ptrLine + 5 );
         }
         return true;

      case BBRC_CMD_SUSP:
         if ( suspKey >= 0 )
         {
            stdPrintf( "Additional definition for 'susp' ignored.\n" );
         }
         else
         {
            suspKey = ctrl( ptrLine + 5 );
         }
         return true;

      case BBRC_CMD_CAPTURE:
         if ( captureKey >= 0 )
         {
            stdPrintf( "Additional definition for 'capture' ignored.\n" );
         }
         else
         {
            captureKey = ctrl( ptrLine + 8 );
         }
         return true;

      case BBRC_CMD_URL:
         if ( browserKey >= 0 )
         {
            stdPrintf( "Additional definition for 'url' ignored.\n" );
         }
         else
         {
            browserKey = ctrl( ptrLine + 4 );
         }
         return true;

      case BBRC_CMD_SHELL:
         if ( shellKey >= 0 )
         {
            stdPrintf( "Additional definition for 'shellkey' ignored.\n" );
         }
         else
         {
            if ( !strncmp( ptrLine, "shellkey ", 9 ) )
            {
               shellKey = ctrl( ptrLine + 9 );
            }
            else
            {
               shellKey = ctrl( ptrLine + ( sizeof( "aryShell " ) - 1 ) );
            }
         }
         return true;

      default:
         return false;
   }
}

/// @brief Handle friend and enemy list `.bbsrc` commands.
///
/// @param commandId Parsed command identifier.
/// @param ptrLine Raw config line.
///
/// @return `true` if the command was handled, otherwise `false`.
static bool processBbsRcListCommand( BbsRcCommandId commandId,
                                     const char *ptrLine )
{
   switch ( commandId )
   {
      case BBRC_CMD_FRIEND:
         if ( bbsFriends )
         {
            char aryScratchLine[MAX_LINE_LENGTH + 1];

            if ( fgets( aryScratchLine, MAX_LINE_LENGTH + 1, bbsFriends ) )
            {
               return true;
            }
         }
         return addFriendFromLine( ptrLine );

      case BBRC_CMD_ENEMY:
         if ( strlen( ptrLine ) == 6 )
         {
            stdPrintf( "Empty username in 'enemy'.\n" );
         }
         else if ( slistFind( enemyList, (void *)( ptrLine + 6 ), strCompareVoid ) != -1 )
         {
            stdPrintf( "Duplicate username in 'enemy'.\n" );
         }
         else
         {
            char *ptrNameCopy;

            ptrNameCopy = (char *)calloc( 1, strlen( ptrLine + 6 ) + 1 );
            if ( !ptrNameCopy )
            {
               fatalExit( "Out of memory adding 'enemy'!\n", "Fatal error" );
               return false;
            }
            snprintf( ptrNameCopy, strlen( ptrLine + 6 ) + 1, "%s", ptrLine + 6 );
            if ( !slistAddItem( enemyList, (void *)ptrNameCopy, 1 ) )
            {
               fatalExit( "Can't add 'enemy' to list!\n", "Fatal error" );
               return false;
            }
         }
         return true;

      default:
         return false;
   }
}

/// @brief Handle macro and away-message `.bbsrc` commands.
///
/// @param commandId Parsed command identifier.
/// @param ptrLine Raw config line.
/// @param ptrState Running `.bbsrc` read state.
///
/// @return `true` if the command was handled, otherwise `false`.
static bool processBbsRcMacroCommand( BbsRcCommandId commandId,
                                      const char *ptrLine,
                                      const BbsRcReadState *ptrState )
{
   switch ( commandId )
   {
      case BBRC_CMD_MACRO:
         {
            int macroIndex;
            const char *ptrToken;

            macroIndex = ctrl( ptrLine + ( sizeof( "aryMacro " ) - 1 ) );
            ptrToken = ptrLine + sizeof( "aryMacro " ) +
                       ( ptrLine[sizeof( "aryMacro " ) - 1] == '^' );
            if ( *ptrToken++ != ' ' )
            {
               stdPrintf( "Syntax error in 'aryMacro', ignored.\n" );
               return true;
            }
            if ( *aryMacro[macroIndex] )
            {
               stdPrintf( "Additional definition of same 'aryMacro' value ignored.\n" );
               return true;
            }
            if ( macroIndex == 'i' && !awayKey && ptrState->tmpVersion < 220 )
            {
               awayKey = 'i';
               writeDecodedBbsRcText( ptrToken, aryAwayMessageLines[0],
                                      aryAwayMessageLines );
               return true;
            }

            writeDecodedBbsRcText( ptrToken, aryMacro[macroIndex], NULL );
            return true;
         }

      case BBRC_CMD_OLD_AWAY:
         writeDecodedBbsRcText( ptrLine + ( sizeof( "aryAwayMessageLines " ) - 1 ),
                                aryAwayMessageLines[0], aryAwayMessageLines );
         return true;

      case BBRC_CMD_NEW_AWAY:
         snprintf( aryAwayMessageLines[ptrLine[1] - '1'],
                   sizeof( aryAwayMessageLines[0] ), "%s", ptrLine + 3 );
         return true;

      default:
         return false;
   }
}

/// @brief Handle boolean and setting-style `.bbsrc` commands.
///
/// @param commandId Parsed command identifier.
/// @param ptrLine Raw config line.
/// @param ptrState Running `.bbsrc` read state.
///
/// @return `true` if the command was handled, otherwise `false`.
static bool processBbsRcSettingCommand( BbsRcCommandId commandId,
                                        const char *ptrLine,
                                        BbsRcReadState *ptrState )
{
   BbsRcOptionValue optionValue;

   switch ( commandId )
   {
      case BBRC_CMD_BOLD:
         flagsConfiguration.shouldUseBold = 1;
         return true;

      case BBRC_CMD_XLAND:
         isXland = 0;
         return true;

      case BBRC_CMD_VERSION:
         ptrState->tmpVersion = atoi( ptrLine + 8 );
         return true;

      case BBRC_CMD_SQUELCH:
         switch ( atoi( ptrLine + 8 ) )
         {
            case 1:
               flagsConfiguration.shouldSquelchExpress = 1;
               break;
            case 2:
               flagsConfiguration.shouldSquelchPost = 1;
               break;
            case 3:
               flagsConfiguration.shouldSquelchPost = 1;
               flagsConfiguration.shouldSquelchExpress = 1;
               break;
            default:
               break;
         }
         return true;

      case BBRC_CMD_TCP_KEEPALIVE:
         optionValue = parseBooleanSettingValue( ptrLine, 9, "'keepalive'", true );
         if ( optionValue != BBRC_OPTION_INVALID )
         {
            flagsConfiguration.shouldUseTcpKeepalive = (unsigned int)optionValue;
         }
         return true;

      case BBRC_CMD_CLICKABLE_URLS:
         optionValue = parseBooleanSettingValue( ptrLine, strlen( "clickableurls" ),
                                                 "clickable URL option", false );
         if ( optionValue != BBRC_OPTION_INVALID )
         {
            flagsConfiguration.shouldEnableClickableUrls = (unsigned int)optionValue;
         }
         return true;

      case BBRC_CMD_TITLEBAR:
         optionValue = parseBooleanSettingValue( ptrLine, strlen( "titlebar" ),
                                                 "title bar option", false );
         if ( optionValue != BBRC_OPTION_INVALID )
         {
            flagsConfiguration.shouldEnableTitleBar = (unsigned int)optionValue;
            flagsConfiguration.hasTitleBarSetting = 1;
         }
         return true;

      case BBRC_CMD_SCREENREADER:
         optionValue = parseBooleanSettingValue( ptrLine, strlen( "screenreader" ),
                                                 "screen reader option", false );
         if ( optionValue != BBRC_OPTION_INVALID )
         {
            flagsConfiguration.isScreenReaderModeEnabled = (unsigned int)optionValue;
            flagsConfiguration.hasScreenReaderModeSetting = 1;
         }
         return true;

      case BBRC_CMD_AUTOCOMPLETE:
         optionValue = parseBooleanSettingValue( ptrLine, strlen( "autocomplete" ),
                                                 "autocomplete option", false );
         if ( optionValue != BBRC_OPTION_INVALID )
         {
            flagsConfiguration.shouldEnableNameAutocomplete = (unsigned int)optionValue;
            flagsConfiguration.hasNameAutocompleteSetting = 1;
         }
         return true;

      case BBRC_CMD_KEYCHAIN:
         optionValue = parseBooleanSettingValue( ptrLine, strlen( "keychain" ),
                                                 "keychain option", false );
         if ( optionValue != BBRC_OPTION_INVALID )
         {
#ifdef ENABLE_KEYCHAIN
            flagsConfiguration.shouldUseKeychain = (unsigned int)optionValue;
#endif
         }
         return true;

      case BBRC_CMD_COLOR:
         if ( !parseColorScheme( ptrLine ) )
         {
            stdPrintf( "Invalid 'color' scheme on line %d, ignored.\n",
                       ptrState->lineNumber );
         }
         return true;

      case BBRC_CMD_AUTONAME:
         if ( strncmp( ptrLine + ( sizeof( "aryAutoName " ) - 1 ), "Guest", 5 ) )
         {
            strncpy( aryAutoName, ptrLine + ( sizeof( "aryAutoName " ) - 1 ), 21 );
            aryAutoName[20] = 0;
         }
         return true;

      case BBRC_CMD_AUTOANSI:
         if ( strlen( ptrLine ) <= 9 || ptrLine[9] != 'N' )
         {
            flagsConfiguration.shouldAutoAnswerAnsiPrompt = 1;
         }
         return true;

      default:
         return false;
   }
}

/// @brief Read `.bbsrc` and apply the configured client settings.
///
/// @return This function does not return a value.
void readBbsRc( void )
{
   char aryLine[MAX_LINE_LENGTH + 1];
   BbsRcReadState state = { 0 };

   initializeBbsRcLists();
   initializeBbsRcDefaults();

   while ( readNormalizedLine( ptrBbsRc, aryLine, sizeof( aryLine ),
                               &state.lineNumber, &state.reads, ".bbsrc" ) )
   {
      if ( !processBbsRcCommandLine( aryLine, &state ) )
      {
         return;
      }
   }

   if ( !readLegacyBbsFriends( aryLine, &state ) )
   {
      return;
   }

   finalizeBbsRcRead( &state );
}

/// @brief Read legacy `.bbsfriends` entries after `.bbsrc` parsing.
///
/// @param ptrLine Reusable line buffer.
/// @param ptrState Running `.bbsrc` read state.
///
/// @return `true` on success, otherwise `false`.
static bool readLegacyBbsFriends( char *ptrLine, BbsRcReadState *ptrState )
{
   if ( bbsFriends )
   {
      rewind( bbsFriends );
   }
   while ( readNormalizedLine( bbsFriends, ptrLine, sizeof( char[MAX_LINE_LENGTH + 1] ),
                               &ptrState->lineNumber, &ptrState->reads, ".bbsfriends" ) )
   {
      if ( !strncmp( ptrLine, "friend ", FRIEND_COMMAND_PREFIX_LEN ) )
      {
         if ( !addFriendFromLine( ptrLine ) )
         {
            return false;
         }
      }
   }

   return true;
}

/// @brief Warn about conflicting `.bbsrc` hotkey and macro definitions.
///
/// @return This helper does not return a value.
static void warnAboutBbsRcConflicts( void )
{
   if ( quitKey >= 0 && *aryMacro[quitKey] )
   {
      stdPrintf( "Warning: duplicate definition of 'aryMacro' and 'quit'\n" );
   }
   if ( suspKey >= 0 && *aryMacro[suspKey] )
   {
      stdPrintf( "Warning: duplicate definition of 'aryMacro' and 'susp'\n" );
   }
   if ( captureKey >= 0 && *aryMacro[captureKey] )
   {
      stdPrintf( "Warning: duplicate definition of 'aryMacro' and 'capture'\n" );
   }
   if ( shellKey >= 0 && *aryMacro[captureKey] )
   {
      stdPrintf( "Warning: duplicate definition of 'aryMacro' and 'shellkey'\n" );
   }
   if ( quitKey >= 0 && quitKey == suspKey )
   {
      stdPrintf( "Warning: duplicate definition of 'quit' and 'susp'\n" );
   }
   if ( quitKey >= 0 && quitKey == captureKey )
   {
      stdPrintf( "Warning: duplicate definition of 'quit' and 'capture'\n" );
   }
   if ( quitKey >= 0 && quitKey == shellKey )
   {
      stdPrintf( "Warning: duplicate definition of 'quit' and 'shellkey'\n" );
   }
   if ( suspKey >= 0 && suspKey == captureKey )
   {
      stdPrintf( "Warning: duplicate definition of 'susp' and 'capture'\n" );
   }
   if ( suspKey >= 0 && suspKey == shellKey )
   {
      stdPrintf( "Warning: duplicate definition of 'susp' and 'shellkey'\n" );
   }
   if ( captureKey >= 0 && captureKey == shellKey )
   {
      stdPrintf( "Warning: duplicate definition of 'capture' and 'shellkey'\n" );
   }
}

/// @brief Decode a `.bbsrc` escaped text field into its destination buffer.
///
/// @param ptrToken Encoded config token to decode.
/// @param ptrWriteBuffer Destination buffer for decoded text.
/// @param aryAwayBuffers Optional away-message buffer array for multi-line decoding.
///
/// @return This helper does not return a value.
static void writeDecodedBbsRcText( const char *ptrToken, char *ptrWriteBuffer,
                                   char ( *aryAwayBuffers )[80] )
{
   int parseIndex;
   int messageLineIndex;

   messageLineIndex = 0;
   while ( ( parseIndex = *ptrToken++ ) )
   {
      if ( parseIndex == '^' && *ptrToken != '^' )
      {
         parseIndex = ctrl( ptrToken++ - 1 );
      }
      if ( parseIndex == '\r' )
      {
         parseIndex = '\n';
      }
      if ( parseIndex == '\n' && aryAwayBuffers != NULL )
      {
         ptrWriteBuffer = aryAwayBuffers[++messageLineIndex];
      }
      else if ( !iscntrl( parseIndex ) )
      {
         *ptrWriteBuffer++ = (char)parseIndex;
      }
   }
}
