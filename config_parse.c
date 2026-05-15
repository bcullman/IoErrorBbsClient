/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file parses config.toml and applies the configured settings.
 */
#include "config_file.h"
#include "client.h"
#include "client_globals.h"
#include "color.h"
#include "config_globals.h"
#include "config_menu.h"
#include "defs.h"
#include "filter_globals.h"
#include "utility.h"

#define MAX_LINE_LENGTH 512
#define MAX_SECTION_NAME_LENGTH 64
#define MAX_VALUE_LENGTH 256

typedef enum
{
   TOML_SECTION_NONE = 0,
   TOML_SECTION_AWAY,
   TOML_SECTION_BEHAVIOR,
   TOML_SECTION_COLORS,
   TOML_SECTION_CONNECTION,
   TOML_SECTION_CONTACTS,
   TOML_SECTION_DEFAULTS,
   TOML_SECTION_METADATA,
   TOML_SECTION_LOCAL_COMMAND_KEYS,
   TOML_SECTION_UNKNOWN
} TomlSectionId;

typedef struct
{
   bool hasVersionSetting;
   int lineNumber;
   int reads;
   bool shouldRewriteConfig;
} ConfigReadState;

static void applyConfigKeyDefaults( void );
static void applyDefaultUppercasePreference( int lowerKey,
                                             bool shouldUseUppercaseByDefault );
static void ensureDefaultAwayMessage( void );
static bool tryFinalizeConfigRead( ConfigReadState *ptrState );
static void initializeConfigDefaults( void );
static void initializeConfigLists( void );
static bool isIllegalCommandKeyValue( int inputChar );
static void replaceEnemyListItems( slist *ptrParsedEnemyList );
static void replaceFriendListItems( slist *ptrParsedFriendList );
static bool tryAddEnemyNameToList( slist *ptrList,
                                   const char *ptrEnemyName );
static bool tryAddFriendEntryToList( slist *ptrList,
                                     const char *ptrFriendName,
                                     const char *ptrFriendInfo );
static bool tryParseAwayMessagesValue( const char *ptrValue );
static bool tryParseBooleanValue( const char *ptrValue,
                                  const char *ptrKeyName,
                                  bool *ptrOutValue );
static bool tryParseContactEnemiesValue( const char *ptrValue );
static bool tryParseContactFriendsValue( const char *ptrValue );
static bool tryParseColorValue( const char *ptrValue,
                                const char *ptrKeyName,
                                int *ptrOutValue );
static bool tryParseIntegerValue( const char *ptrValue,
                                  const char *ptrKeyName,
                                  int minimumValue,
                                  int maximumValue,
                                  int *ptrOutValue );
static bool tryParseLocalCommandKeyValue( const char *ptrValue,
                                          const char *ptrKeyName,
                                          bool isCommandKey,
                                          int *ptrOutValue );
static bool tryParseTomlKeyValueLine( const char *ptrLine,
                                      char *aryKeyName,
                                      size_t keyNameSize,
                                      char *aryValue,
                                      size_t valueSize );
static bool tryParseTomlStringToken( const char *ptrText,
                                     size_t *ptrConsumedLength,
                                     char *aryOutput,
                                     size_t outputSize );
static bool tryParseTomlQuotedString( const char *ptrValue,
                                      char *aryOutput,
                                      size_t outputSize );
static TomlSectionId parseTomlSectionLine( const char *ptrLine,
                                           char *arySectionName,
                                           size_t sectionNameSize );
static bool tryProcessTomlKeyValue( TomlSectionId currentSection,
                                    const char *ptrKeyName,
                                    const char *ptrValue,
                                    ConfigReadState *ptrState );
static void stripInlineTomlComment( char *ptrText );
static char *trimWhitespace( char *ptrText );
static void warnAboutConfigConflicts( void );

/// @brief Apply default local-command key values when the config omits them.
///
/// @return This helper does not return a value.
static void applyConfigKeyDefaults( void )
{
   if ( awayKey == -1 )
   {
      awayKey = 'a';
   }
   if ( browserKey == -1 )
   {
      browserKey = 'w';
   }
   if ( captureKey == -1 )
   {
      captureKey = 'c';
   }
   if ( commandKey == -1 )
   {
      commandKey = ESC;
   }
   if ( quitKey == -1 )
   {
      quitKey = CTRL_D;
   }
   if ( shellKey == -1 )
   {
      shellKey = '!';
   }
   if ( suspKey == -1 )
   {
      suspKey = CTRL_Z;
   }
}

/// @brief Flip a two-key command mapping between lowercase and uppercase defaults.
///
/// @param lowerKey Lowercase command character in the key map.
/// @param shouldUseUppercaseByDefault Non-zero to map the lowercase key to the
/// uppercase action by default, zero to restore the normal lowercase default.
///
/// @return This helper does not return a value.
static void applyDefaultUppercasePreference( int lowerKey,
                                             bool shouldUseUppercaseByDefault )
{
   int upperKey;

   upperKey = toupper( lowerKey );
   if ( shouldUseUppercaseByDefault )
   {
      aryKeyMap[lowerKey] = (char)upperKey;
      aryKeyMap[upperKey] = (char)lowerKey;
   }
   else
   {
      aryKeyMap[lowerKey] = (char)lowerKey;
      aryKeyMap[upperKey] = (char)upperKey;
   }
}

/// @brief Ensure there is at least one away-message line configured.
///
/// @return This helper does not return a value.
static void ensureDefaultAwayMessage( void )
{
   if ( !**aryAwayMessageLines )
   {
      snprintf( aryAwayMessageLines[0],
                sizeof( aryAwayMessageLines[0] ),
                "%s",
                "I'm away from my keyboard right now." );
      *aryAwayMessageLines[1] = '\0';
   }
}

/// @brief Finalize config state after parsing completes.
///
/// @param ptrState Running read state to finalize.
///
/// @return `true` on success, otherwise `false`.
static bool tryFinalizeConfigRead( ConfigReadState *ptrState )
{
   applyConfigKeyDefaults();
   ensureDefaultAwayMessage();
   defaultColors( 0 );
   warnAboutConfigConflicts();

   slistSort( friendList );
   slistSort( enemyList );
   slistSort( whoList );

   if ( !*aryBbsHost )
   {
      snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", BBS_HOSTNAME );
   }
   if ( bbsPort == 0 )
   {
      bbsPort = BBS_PORT_NUMBER;
   }
   if ( !*aryEditor )
   {
      snprintf( aryEditor, sizeof( aryEditor ), "%s", DEFAULT_EDITOR_CONFIG_VALUE );
   }
   if ( ptrState->reads == 0 )
   {
      version = INT_VERSION;
      setup( -1 );
   }
   else if ( !ptrState->hasVersionSetting )
   {
      version = INT_VERSION;
      ptrState->shouldRewriteConfig = true;
   }
   else if ( version != INT_VERSION )
   {
      int previousVersion;

      previousVersion = version;
      version = INT_VERSION;
      setup( previousVersion );
   }
   if ( !flagsConfiguration.hasScreenReaderModeSetting )
   {
      flagsConfiguration.hasScreenReaderModeSetting = 1;
      ptrState->shouldRewriteConfig = true;
   }
   if ( !flagsConfiguration.hasTitleBarSetting )
   {
      flagsConfiguration.hasTitleBarSetting = 1;
      ptrState->shouldRewriteConfig = true;
   }
   if ( !flagsConfiguration.hasNameAutocompleteSetting )
   {
      defaultNameAutocompleteIfUnset();
      ptrState->shouldRewriteConfig = true;
   }
   if ( ptrState->shouldRewriteConfig && !isConfigFileReadOnly )
   {
      writeConfig();
   }
   if ( isLoginShell )
   {
      setTerm();
      configClient();
      resetTerm();
   }

   return true;
}

/// @brief Initialize default config values before parsing begins.
///
/// @return This helper does not return a value.
static void initializeConfigDefaults( void )
{
   int parseIndex;

   version = INT_VERSION;
   awayKey = -1;
   browserKey = -1;
   captureKey = -1;
   commandKey = -1;
   quitKey = -1;
   shellKey = -1;
   suspKey = -1;

   for ( parseIndex = 0; parseIndex <= 127; parseIndex++ )
   {
      aryKeyMap[parseIndex] = (char)parseIndex;
   }

   isXland = false;
   xlandQueue = newQueue( 21, MAX_USER_NAME_HISTORY_COUNT );
   if ( !xlandQueue )
   {
      isXland = false;
   }
   urlQueue = newQueue( 1024, 10 );

   isAutoLoggedIn = false;
   *aryAutoName = 0;
   *aryAwayMessageLines[0] = 0;
   *aryBbsHost = 0;
   *aryEditor = 0;
   bbsPort = 0;
   ptrConfigFile = findConfigFile();

   flagsConfiguration.hasNameAutocompleteSetting = 0;
   flagsConfiguration.hasScreenReaderModeSetting = 0;
   flagsConfiguration.hasTitleBarSetting = 0;
   flagsConfiguration.isMorePromptActive = 0;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   flagsConfiguration.shouldAutoAnswerAnsiPrompt = 0;
   flagsConfiguration.shouldDisableBold = 0;
   flagsConfiguration.shouldEnableClickableUrls = 1;
   flagsConfiguration.shouldEnableNameAutocomplete = 1;
   flagsConfiguration.shouldEnableTitleBar = 1;
   flagsConfiguration.shouldSquelchExpress = 0;
   flagsConfiguration.shouldSquelchPost = 0;
   flagsConfiguration.shouldUseAnsi = 0;
   flagsConfiguration.shouldUseBold = 0;
   flagsConfiguration.shouldUseKeychain = 0;
   flagsConfiguration.shouldUseTcpKeepalive = 1;

   defaultColors( 1 );

   whoListProgress = 0;
   ptrPostBuffer = 0;
   postHeaderActive = 0;
   highestExpressMessageId = 0;
   isExpressMessageHeaderActive = false;
   postProgressState = 0;
   isPostJustEnded = false;
   isExpressMessageInProgress = false;
   shouldSendExpressMessage = false;
   pendingLinesToEat = 0;
   ptrExpressMessageBuffer = aryExpressMessageBuffer;
}

/// @brief Create the list structures used while reading config.
///
/// @return This helper does not return a value.
static void initializeConfigLists( void )
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

/// @brief Reject command-prefix values that collide with reserved local keys.
///
/// @param inputChar Parsed key value to validate.
///
/// @return `true` when the key is illegal for the command prefix.
static bool isIllegalCommandKeyValue( int inputChar )
{
   static const char aryIllegalKeys[] =
      { '\0', 1, 3, CTRL_D, 5, '\b', '\n', '\r', 17, 19, CTRL_U, CTRL_W, CTRL_X, 25, CTRL_Z, DEL };
   size_t itemIndex;

   for ( itemIndex = 0; itemIndex < sizeof( aryIllegalKeys ); itemIndex++ )
   {
      if ( inputChar == aryIllegalKeys[itemIndex] )
      {
         return true;
      }
   }

   return inputChar >= ' ';
}

/// @brief Add one enemy name to the configured list.
///
/// @param ptrEnemyName Enemy name to add.
///
/// @return `true` on success, otherwise `false`.
static void replaceEnemyListItems( slist *ptrParsedEnemyList )
{
   slistDestroyItems( enemyList );
   free( enemyList->items );
   enemyList->items = ptrParsedEnemyList->items;
   enemyList->nitems = ptrParsedEnemyList->nitems;
   ptrParsedEnemyList->items = NULL;
   ptrParsedEnemyList->nitems = 0;
}

/// @brief Replace the configured friend list with parsed entries.
///
/// @param ptrParsedFriendList Parsed list whose items should become the active list.
///
/// @return This helper does not return a value.
static void replaceFriendListItems( slist *ptrParsedFriendList )
{
   slistDestroyItems( friendList );
   free( friendList->items );
   friendList->items = ptrParsedFriendList->items;
   friendList->nitems = ptrParsedFriendList->nitems;
   ptrParsedFriendList->items = NULL;
   ptrParsedFriendList->nitems = 0;
}

/// @brief Add one enemy name to the supplied list.
///
/// @param ptrList Target list to modify.
/// @param ptrEnemyName Enemy name to add.
///
/// @return `true` on success, otherwise `false`.
static bool tryAddEnemyNameToList( slist *ptrList,
                                   const char *ptrEnemyName )
{
   char *ptrEnemyNameCopy;

   if ( ptrEnemyName == NULL || *ptrEnemyName == '\0' )
   {
      stdPrintf( "Empty enemy name ignored.\n" );
      return true;
   }
   if ( slistFind( ptrList, (void *)ptrEnemyName, strCompareVoid ) != -1 )
   {
      stdPrintf( "Duplicate enemy name ignored.\n" );
      return true;
   }

   ptrEnemyNameCopy = (char *)calloc( strlen( ptrEnemyName ) + 1, sizeof( char ) );
   if ( ptrEnemyNameCopy == NULL )
   {
      fatalExit( "Out of memory adding 'enemy'!\n", "Fatal error" );
      return false;
   }

   snprintf( ptrEnemyNameCopy, strlen( ptrEnemyName ) + 1, "%s", ptrEnemyName );
   if ( !slistAddItem( ptrList, ptrEnemyNameCopy, 1 ) )
   {
      fatalExit( "Can't add 'enemy' to list!\n", "Fatal error" );
      return false;
   }

   return true;
}

/// @brief Add one friend entry to the configured list.
///
/// @param ptrFriendName Friend name to add.
/// @param ptrFriendInfo Friend info text, or `NULL` to use the default.
///
/// @return `true` on success, otherwise `false`.
static bool tryAddFriendEntryToList( slist *ptrList,
                                     const char *ptrFriendName,
                                     const char *ptrFriendInfo )
{
   friend *ptrFriend;

   if ( ptrFriendName == NULL || *ptrFriendName == '\0' )
   {
      stdPrintf( "Empty friend name ignored.\n" );
      return true;
   }
   if ( slistFind( ptrList, (void *)ptrFriendName, fStrCompareVoid ) != -1 )
   {
      stdPrintf( "Duplicate friend name ignored.\n" );
      return true;
   }

   ptrFriend = (friend *)calloc( 1, sizeof( friend ) );
   if ( ptrFriend == NULL )
   {
      fatalExit( "Out of memory adding 'friend'!\n", "Fatal error" );
      return false;
   }

   snprintf( ptrFriend->name, sizeof( ptrFriend->name ), "%s", ptrFriendName );
   snprintf( ptrFriend->info,
             sizeof( ptrFriend->info ),
             "%s",
             ( ptrFriendInfo != NULL && *ptrFriendInfo != '\0' ) ? ptrFriendInfo : "(None)" );
   ptrFriend->magic = 0x3231;
   if ( !slistAddItem( ptrList, ptrFriend, 1 ) )
   {
      fatalExit( "Can't add 'friend' to list!\n", "Fatal error" );
      return false;
   }

   return true;
}

/// @brief Parse an away-message string array.
///
/// @param ptrValue TOML value text for `[away].messages`.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseAwayMessagesValue( const char *ptrValue )
{
   char aryParsedMessages[5][80];
   char aryMessageText[80];
   const char *ptrCursor;
   int messageCount;

   if ( ptrValue == NULL || *ptrValue != '[' )
   {
      stdPrintf( "Invalid array value for 'messages' ignored.\n" );
      return false;
   }

   ptrCursor = ptrValue + 1;
   messageCount = 0;
   for ( int messageIndex = 0; messageIndex < 5; messageIndex++ )
   {
      aryParsedMessages[messageIndex][0] = '\0';
   }
   while ( true )
   {
      size_t consumedLength;

      while ( isspace( (unsigned char)*ptrCursor ) )
      {
         ptrCursor++;
      }
      if ( *ptrCursor == ']' )
      {
         return true;
      }
      if ( *ptrCursor == '\0' )
      {
         break;
      }
      if ( !tryParseTomlStringToken( ptrCursor,
                                     &consumedLength,
                                     aryMessageText,
                                     sizeof( aryMessageText ) ) )
      {
         break;
      }
      if ( messageCount < 5 )
      {
         snprintf( aryParsedMessages[messageCount],
                   sizeof( aryParsedMessages[messageCount] ),
                   "%s",
                   aryMessageText );
      }
      else
      {
         stdPrintf( "Extra away-message lines ignored after the first five entries.\n" );
      }
      messageCount++;
      ptrCursor += consumedLength;
      while ( isspace( (unsigned char)*ptrCursor ) )
      {
         ptrCursor++;
      }
      if ( *ptrCursor == ',' )
      {
         ptrCursor++;
         continue;
      }
      if ( *ptrCursor == ']' )
      {
         for ( int messageIndex = 0; messageIndex < 5; messageIndex++ )
         {
            snprintf( aryAwayMessageLines[messageIndex],
                      sizeof( aryAwayMessageLines[messageIndex] ),
                      "%s",
                      aryParsedMessages[messageIndex] );
         }
         return true;
      }
      break;
   }

   stdPrintf( "Invalid array value for 'messages' ignored.\n" );
   return false;
}

/// @brief Parse a TOML boolean value.
///
/// @param ptrValue Raw value text to decode.
/// @param ptrKeyName Key name used in error messages.
/// @param ptrOutValue Destination for the decoded boolean.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseBooleanValue( const char *ptrValue,
                                  const char *ptrKeyName,
                                  bool *ptrOutValue )
{
   if ( strcmp( ptrValue, "false" ) == 0 )
   {
      *ptrOutValue = false;
      return true;
   }
   if ( strcmp( ptrValue, "true" ) == 0 )
   {
      *ptrOutValue = true;
      return true;
   }

   stdPrintf( "Invalid boolean value for '%s' ignored.\n", ptrKeyName );
   return false;
}

/// @brief Parse the `[contacts].enemies` string array.
///
/// @param ptrValue TOML value text for the enemy array.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseContactEnemiesValue( const char *ptrValue )
{
   char aryEnemyName[21];
   const char *ptrCursor;
   slist *ptrParsedEnemyList;

   if ( ptrValue == NULL || *ptrValue != '[' )
   {
      stdPrintf( "Invalid array value for 'enemies' ignored.\n" );
      return false;
   }

   ptrParsedEnemyList = slistCreate( 0, sortCompareVoid );
   if ( ptrParsedEnemyList == NULL )
   {
      fatalExit( "Can't create parsed 'enemy' list!\n", "Fatal error" );
      return false;
   }

   ptrCursor = ptrValue + 1;
   while ( true )
   {
      size_t consumedLength;

      while ( isspace( (unsigned char)*ptrCursor ) )
      {
         ptrCursor++;
      }
      if ( *ptrCursor == ']' )
      {
         replaceEnemyListItems( ptrParsedEnemyList );
         slistDestroy( ptrParsedEnemyList );
         return true;
      }
      if ( *ptrCursor == '\0' )
      {
         break;
      }
      if ( !tryParseTomlStringToken( ptrCursor,
                                     &consumedLength,
                                     aryEnemyName,
                                     sizeof( aryEnemyName ) ) )
      {
         break;
      }
      if ( !tryAddEnemyNameToList( ptrParsedEnemyList, aryEnemyName ) )
      {
         slistDestroyItems( ptrParsedEnemyList );
         slistDestroy( ptrParsedEnemyList );
         return false;
      }
      ptrCursor += consumedLength;
      while ( isspace( (unsigned char)*ptrCursor ) )
      {
         ptrCursor++;
      }
      if ( *ptrCursor == ',' )
      {
         ptrCursor++;
         continue;
      }
      if ( *ptrCursor == ']' )
      {
         replaceEnemyListItems( ptrParsedEnemyList );
         slistDestroy( ptrParsedEnemyList );
         return true;
      }
      break;
   }

   slistDestroyItems( ptrParsedEnemyList );
   slistDestroy( ptrParsedEnemyList );
   stdPrintf( "Invalid array value for 'enemies' ignored.\n" );
   return false;
}

/// @brief Parse the `[contacts].friends` inline-table array.
///
/// @param ptrValue TOML value text for the friend array.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseContactFriendsValue( const char *ptrValue )
{
   const char *ptrCursor;
   slist *ptrParsedFriendList;

   if ( ptrValue == NULL || *ptrValue != '[' )
   {
      stdPrintf( "Invalid array value for 'friends' ignored.\n" );
      return false;
   }

   ptrParsedFriendList = slistCreate( 0, fSortCompareVoid );
   if ( ptrParsedFriendList == NULL )
   {
      fatalExit( "Can't create parsed 'friend' list!\n", "Fatal error" );
      return false;
   }

   ptrCursor = ptrValue + 1;
   while ( true )
   {
      char aryFriendInfo[54];
      char aryFriendName[21];
      bool hasInfoField;
      bool hasNameField;

      while ( isspace( (unsigned char)*ptrCursor ) )
      {
         ptrCursor++;
      }
      if ( *ptrCursor == ']' )
      {
         replaceFriendListItems( ptrParsedFriendList );
         slistDestroy( ptrParsedFriendList );
         return true;
      }
      if ( *ptrCursor != '{' )
      {
         break;
      }

      aryFriendInfo[0] = '\0';
      aryFriendName[0] = '\0';
      hasInfoField = false;
      hasNameField = false;
      ptrCursor++;
      while ( true )
      {
         char aryFieldName[16];
         char aryFieldValue[80];
         const char *ptrEquals;
         size_t fieldNameLength;
         size_t consumedLength;

         while ( isspace( (unsigned char)*ptrCursor ) )
         {
            ptrCursor++;
         }
         if ( *ptrCursor == '}' )
         {
            ptrCursor++;
            break;
         }

         ptrEquals = strchr( ptrCursor, '=' );
         if ( ptrEquals == NULL )
         {
            stdPrintf( "Invalid friend entry ignored.\n" );
            slistDestroyItems( ptrParsedFriendList );
            slistDestroy( ptrParsedFriendList );
            return false;
         }
         fieldNameLength = (size_t)( ptrEquals - ptrCursor );
         if ( fieldNameLength == 0 || fieldNameLength >= sizeof( aryFieldName ) )
         {
            stdPrintf( "Invalid friend entry ignored.\n" );
            slistDestroyItems( ptrParsedFriendList );
            slistDestroy( ptrParsedFriendList );
            return false;
         }
         memcpy( aryFieldName, ptrCursor, fieldNameLength );
         aryFieldName[fieldNameLength] = '\0';
         snprintf( aryFieldName, sizeof( aryFieldName ), "%s", trimWhitespace( aryFieldName ) );
         ptrCursor = ptrEquals + 1;
         while ( isspace( (unsigned char)*ptrCursor ) )
         {
            ptrCursor++;
         }
         if ( !tryParseTomlStringToken( ptrCursor,
                                        &consumedLength,
                                        aryFieldValue,
                                        sizeof( aryFieldValue ) ) )
         {
            stdPrintf( "Invalid friend entry ignored.\n" );
            slistDestroyItems( ptrParsedFriendList );
            slistDestroy( ptrParsedFriendList );
            return false;
         }
         if ( strcmp( aryFieldName, "info" ) == 0 )
         {
            snprintf( aryFriendInfo, sizeof( aryFriendInfo ), "%s", aryFieldValue );
            hasInfoField = true;
         }
         else if ( strcmp( aryFieldName, "name" ) == 0 )
         {
            snprintf( aryFriendName, sizeof( aryFriendName ), "%s", aryFieldValue );
            hasNameField = true;
         }
         else
         {
            stdPrintf( "Unknown friend field '%s' ignored.\n", aryFieldName );
         }
         ptrCursor += consumedLength;
         while ( isspace( (unsigned char)*ptrCursor ) )
         {
            ptrCursor++;
         }
         if ( *ptrCursor == ',' )
         {
            ptrCursor++;
            continue;
         }
         if ( *ptrCursor == '}' )
         {
            ptrCursor++;
            break;
         }

         stdPrintf( "Invalid friend entry ignored.\n" );
         slistDestroyItems( ptrParsedFriendList );
         slistDestroy( ptrParsedFriendList );
         return false;
      }

      if ( !hasNameField )
      {
         stdPrintf( "Friend entry without a name ignored.\n" );
      }
      else if ( !tryAddFriendEntryToList( ptrParsedFriendList,
                                          aryFriendName,
                                          hasInfoField ? aryFriendInfo : NULL ) )
      {
         slistDestroyItems( ptrParsedFriendList );
         slistDestroy( ptrParsedFriendList );
         return false;
      }

      while ( isspace( (unsigned char)*ptrCursor ) )
      {
         ptrCursor++;
      }
      if ( *ptrCursor == ',' )
      {
         ptrCursor++;
         continue;
      }
      if ( *ptrCursor == ']' )
      {
         replaceFriendListItems( ptrParsedFriendList );
         slistDestroy( ptrParsedFriendList );
         return true;
      }
      break;
   }

   slistDestroyItems( ptrParsedFriendList );
   slistDestroy( ptrParsedFriendList );
   stdPrintf( "Invalid array value for 'friends' ignored.\n" );
   return false;
}

/// @brief Parse one TOML color value from a name or integer palette entry.
///
/// @param ptrValue Raw TOML value text to decode.
/// @param ptrKeyName Key name used in warnings.
/// @param ptrOutValue Destination for the decoded color value.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseColorValue( const char *ptrValue,
                                const char *ptrKeyName,
                                int *ptrOutValue )
{
   int parsedColorValue;

   if ( ptrValue != NULL && *ptrValue == '"' )
   {
      char aryParsedText[MAX_VALUE_LENGTH];

      if ( tryParseTomlQuotedString( ptrValue, aryParsedText, sizeof( aryParsedText ) ) )
      {
         parsedColorValue = colorValueFromName( aryParsedText );
         if ( parsedColorValue >= 0 )
         {
            *ptrOutValue = parsedColorValue;
            return true;
         }
      }
   }
   else if ( tryParseIntegerValue( ptrValue, ptrKeyName, 0, COLOR_VALUE_DEFAULT,
                                   &parsedColorValue ) )
   {
      *ptrOutValue = parsedColorValue;
      return true;
   }

   stdPrintf( "Invalid color value for '%s' ignored.\n", ptrKeyName );
   return false;
}

/// @brief Parse a TOML integer value within a fixed range.
///
/// @param ptrValue Raw value text to decode.
/// @param ptrKeyName Key name used in error messages.
/// @param minimumValue Minimum accepted value.
/// @param maximumValue Maximum accepted value.
/// @param ptrOutValue Destination for the decoded integer.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseIntegerValue( const char *ptrValue,
                                  const char *ptrKeyName,
                                  int minimumValue,
                                  int maximumValue,
                                  int *ptrOutValue )
{
   char *ptrEnd;
   long parsedValue;

   if ( ptrValue == NULL || *ptrValue == '\0' )
   {
      stdPrintf( "Invalid integer value for '%s' ignored.\n", ptrKeyName );
      return false;
   }

   errno = 0;
   parsedValue = strtol( ptrValue, &ptrEnd, 10 );
   if ( errno != 0 || ptrEnd == ptrValue || *ptrEnd != '\0' ||
        parsedValue < minimumValue || parsedValue > maximumValue )
   {
      stdPrintf( "Invalid integer value for '%s' ignored.\n", ptrKeyName );
      return false;
   }

   *ptrOutValue = (int)parsedValue;
   return true;
}

/// @brief Parse one local-command key string.
///
/// @param ptrValue TOML string literal for the key.
/// @param ptrKeyName Key name used in error messages.
/// @param isCommandKey Non-zero when parsing the command-prefix key.
/// @param ptrOutValue Destination for the decoded key code.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseLocalCommandKeyValue( const char *ptrValue,
                                          const char *ptrKeyName,
                                          bool isCommandKey,
                                          int *ptrOutValue )
{
   char aryDecodedValue[32];
   int parsedKeyValue;

   if ( !tryParseTomlQuotedString( ptrValue, aryDecodedValue, sizeof( aryDecodedValue ) ) )
   {
      stdPrintf( "Invalid key value for '%s' ignored.\n", ptrKeyName );
      return false;
   }
   if ( strcmp( aryDecodedValue, "backspace" ) == 0 )
   {
      parsedKeyValue = '\b';
   }
   else if ( strcmp( aryDecodedValue, "del" ) == 0 )
   {
      parsedKeyValue = DEL;
   }
   else if ( strcmp( aryDecodedValue, "enter" ) == 0 )
   {
      parsedKeyValue = '\n';
   }
   else if ( strcmp( aryDecodedValue, "esc" ) == 0 )
   {
      parsedKeyValue = ESC;
   }
   else if ( strcmp( aryDecodedValue, "space" ) == 0 )
   {
      parsedKeyValue = ' ';
   }
   else if ( strcmp( aryDecodedValue, "tab" ) == 0 )
   {
      parsedKeyValue = '\t';
   }
   else if ( strncmp( aryDecodedValue, "ctrl-", 5 ) == 0 &&
             aryDecodedValue[5] != '\0' &&
             aryDecodedValue[6] == '\0' &&
             isalpha( (unsigned char)aryDecodedValue[5] ) )
   {
      parsedKeyValue = toupper( (unsigned char)aryDecodedValue[5] ) ^ 0x40;
   }
   else if ( aryDecodedValue[0] != '\0' && aryDecodedValue[1] == '\0' &&
             isprint( (unsigned char)aryDecodedValue[0] ) )
   {
      parsedKeyValue = aryDecodedValue[0];
   }
   else
   {
      stdPrintf( "Invalid key value for '%s' ignored.\n", ptrKeyName );
      return false;
   }

   if ( isCommandKey && isIllegalCommandKeyValue( parsedKeyValue ) )
   {
      stdPrintf( "Illegal value for '%s', using default of 'esc'.\n", ptrKeyName );
      return false;
   }

   *ptrOutValue = parsedKeyValue;
   return true;
}

/// @brief Split a TOML assignment line into key and value text.
///
/// @param ptrLine Raw line content.
/// @param aryKeyName Destination for the decoded key name.
/// @param keyNameSize Capacity of `aryKeyName`.
/// @param aryValue Destination for the trimmed value text.
/// @param valueSize Capacity of `aryValue`.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseTomlKeyValueLine( const char *ptrLine,
                                      char *aryKeyName,
                                      size_t keyNameSize,
                                      char *aryValue,
                                      size_t valueSize )
{
   const char *ptrEquals;
   char aryKeyBuffer[MAX_SECTION_NAME_LENGTH];
   char aryValueBuffer[MAX_VALUE_LENGTH];
   const char *ptrTrimmedKey;
   const char *ptrTrimmedValue;
   size_t keyLength;
   size_t valueLength;

   ptrEquals = strchr( ptrLine, '=' );
   if ( ptrEquals == NULL )
   {
      return false;
   }

   keyLength = (size_t)( ptrEquals - ptrLine );
   valueLength = strlen( ptrEquals + 1 );
   if ( keyLength >= sizeof( aryKeyBuffer ) || valueLength >= sizeof( aryValueBuffer ) )
   {
      return false;
   }

   memcpy( aryKeyBuffer, ptrLine, keyLength );
   aryKeyBuffer[keyLength] = '\0';
   memcpy( aryValueBuffer, ptrEquals + 1, valueLength + 1 );

   ptrTrimmedKey = trimWhitespace( aryKeyBuffer );
   stripInlineTomlComment( aryValueBuffer );
   ptrTrimmedValue = trimWhitespace( aryValueBuffer );
   if ( *ptrTrimmedKey == '\0' || *ptrTrimmedValue == '\0' ||
        strlen( ptrTrimmedKey ) >= keyNameSize || strlen( ptrTrimmedValue ) >= valueSize )
   {
      return false;
   }

   snprintf( aryKeyName, keyNameSize, "%s", ptrTrimmedKey );
   snprintf( aryValue, valueSize, "%s", ptrTrimmedValue );
   return true;
}

/// @brief Remove a TOML inline comment from a value buffer.
///
/// `#` starts a comment only when it appears outside a quoted string.
///
/// @param ptrText Mutable TOML value text.
///
/// @return This helper does not return a value.
static void stripInlineTomlComment( char *ptrText )
{
   bool isEscaped;
   bool isInsideString;

   if ( ptrText == NULL )
   {
      return;
   }

   isEscaped = false;
   isInsideString = false;
   while ( *ptrText != '\0' )
   {
      if ( isInsideString )
      {
         if ( isEscaped )
         {
            isEscaped = false;
         }
         else if ( *ptrText == '\\' )
         {
            isEscaped = true;
         }
         else if ( *ptrText == '"' )
         {
            isInsideString = false;
         }
      }
      else if ( *ptrText == '"' )
      {
         isInsideString = true;
      }
      else if ( *ptrText == '#' )
      {
         *ptrText = '\0';
         return;
      }

      ptrText++;
   }
}

/// @brief Parse one TOML double-quoted string token from the start of a buffer.
///
/// @param ptrText Text that begins with a TOML string token.
/// @param ptrConsumedLength Receives the number of input characters consumed.
/// @param aryOutput Destination buffer for the decoded string.
/// @param outputSize Capacity of `aryOutput`.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseTomlStringToken( const char *ptrText,
                                     size_t *ptrConsumedLength,
                                     char *aryOutput,
                                     size_t outputSize )
{
   size_t textIndex;

   if ( ptrText == NULL || ptrConsumedLength == NULL || aryOutput == NULL || *ptrText != '"' )
   {
      return false;
   }

   for ( textIndex = 1; ptrText[textIndex] != '\0'; textIndex++ )
   {
      if ( ptrText[textIndex] == '\\' && ptrText[textIndex + 1] != '\0' )
      {
         textIndex++;
         continue;
      }
      if ( ptrText[textIndex] == '"' )
      {
         char aryQuotedValue[MAX_VALUE_LENGTH];

         if ( textIndex + 1 >= sizeof( aryQuotedValue ) )
         {
            return false;
         }
         memcpy( aryQuotedValue, ptrText, textIndex + 1 );
         aryQuotedValue[textIndex + 1] = '\0';
         if ( !tryParseTomlQuotedString( aryQuotedValue, aryOutput, outputSize ) )
         {
            return false;
         }
         *ptrConsumedLength = textIndex + 1;
         return true;
      }
   }

   return false;
}

/// @brief Decode a TOML double-quoted string.
///
/// @param ptrValue Raw value text to decode.
/// @param aryOutput Destination buffer for decoded text.
/// @param outputSize Capacity of `aryOutput`.
///
/// @return `true` on success, otherwise `false`.
static bool tryParseTomlQuotedString( const char *ptrValue,
                                      char *aryOutput,
                                      size_t outputSize )
{
   size_t inputIndex;
   size_t outputIndex;
   size_t valueLength;

   valueLength = strlen( ptrValue );
   if ( valueLength < 2 || ptrValue[0] != '"' || ptrValue[valueLength - 1] != '"' )
   {
      return false;
   }

   outputIndex = 0;
   for ( inputIndex = 1; inputIndex + 1 < valueLength; inputIndex++ )
   {
      int decodedChar;

      decodedChar = (unsigned char)ptrValue[inputIndex];
      if ( decodedChar == '\\' )
      {
         inputIndex++;
         if ( inputIndex + 1 > valueLength )
         {
            return false;
         }
         switch ( ptrValue[inputIndex] )
         {
            case '"':
               decodedChar = '"';
               break;

            case '\\':
               decodedChar = '\\';
               break;

            case 'n':
               decodedChar = '\n';
               break;

            case 'r':
               decodedChar = '\r';
               break;

            case 't':
               decodedChar = '\t';
               break;

            default:
               return false;
         }
      }

      if ( outputIndex + 1 >= outputSize )
      {
         return false;
      }
      aryOutput[outputIndex++] = (char)decodedChar;
   }
   aryOutput[outputIndex] = '\0';
   return true;
}

/// @brief Decode a TOML section header.
///
/// @param ptrLine Raw line content.
/// @param arySectionName Destination for the decoded section name.
/// @param sectionNameSize Capacity of `arySectionName`.
///
/// @return Matching section enum.
static TomlSectionId parseTomlSectionLine( const char *ptrLine,
                                           char *arySectionName,
                                           size_t sectionNameSize )
{
   size_t sectionNameLength;

   sectionNameLength = strlen( ptrLine );
   if ( sectionNameLength < 3 || ptrLine[0] != '[' || ptrLine[sectionNameLength - 1] != ']' )
   {
      return TOML_SECTION_NONE;
   }
   sectionNameLength -= 2;
   if ( sectionNameLength >= sectionNameSize )
   {
      return TOML_SECTION_NONE;
   }

   memcpy( arySectionName, ptrLine + 1, sectionNameLength );
   arySectionName[sectionNameLength] = '\0';
   trimWhitespace( arySectionName );

   if ( strcmp( arySectionName, "behavior" ) == 0 )
   {
      return TOML_SECTION_BEHAVIOR;
   }
   if ( strcmp( arySectionName, "colors" ) == 0 )
   {
      return TOML_SECTION_COLORS;
   }
   if ( strcmp( arySectionName, "away" ) == 0 )
   {
      return TOML_SECTION_AWAY;
   }
   if ( strcmp( arySectionName, "connection" ) == 0 )
   {
      return TOML_SECTION_CONNECTION;
   }
   if ( strcmp( arySectionName, "contacts" ) == 0 )
   {
      return TOML_SECTION_CONTACTS;
   }
   if ( strcmp( arySectionName, "defaults" ) == 0 )
   {
      return TOML_SECTION_DEFAULTS;
   }
   if ( strcmp( arySectionName, "metadata" ) == 0 )
   {
      return TOML_SECTION_METADATA;
   }
   if ( strcmp( arySectionName, "local_command_keys" ) == 0 )
   {
      return TOML_SECTION_LOCAL_COMMAND_KEYS;
   }

   return TOML_SECTION_UNKNOWN;
}

/// @brief Apply one TOML key/value pair for the active section.
///
/// @param currentSection Current section enum.
/// @param ptrKeyName Parsed TOML key name.
/// @param ptrValue Parsed TOML value text.
/// @param ptrState Running read state.
///
/// @return `true` when the key was recognized, otherwise `false`.
static bool tryProcessTomlKeyValue( TomlSectionId currentSection,
                                    const char *ptrKeyName,
                                    const char *ptrValue,
                                    ConfigReadState *ptrState )
{
   bool parsedBooleanValue;
   int parsedIntegerValue;
   int parsedKeyValue;
   char aryParsedText[MAX_VALUE_LENGTH];

   switch ( currentSection )
   {
      case TOML_SECTION_AWAY:
         if ( strcmp( ptrKeyName, "messages" ) == 0 )
         {
            return tryParseAwayMessagesValue( ptrValue );
         }
         return false;

      case TOML_SECTION_BEHAVIOR:
         if ( strcmp( ptrKeyName, "auto_answer_ansi" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               flagsConfiguration.shouldAutoAnswerAnsiPrompt =
                  (unsigned int)parsedBooleanValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "auto_reply_to_x_messages" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               isXland = parsedBooleanValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "autocomplete_recipients" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               flagsConfiguration.shouldEnableNameAutocomplete =
                  (unsigned int)parsedBooleanValue;
               flagsConfiguration.hasNameAutocompleteSetting = 1;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "clickable_url_summaries" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               flagsConfiguration.shouldEnableClickableUrls =
                  (unsigned int)parsedBooleanValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "screen_reader_mode" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               flagsConfiguration.isScreenReaderModeEnabled =
                  (unsigned int)parsedBooleanValue;
               flagsConfiguration.hasScreenReaderModeSetting = 1;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "suppress_enemy_express" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               flagsConfiguration.shouldSquelchExpress =
                  (unsigned int)parsedBooleanValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "suppress_enemy_posts" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               flagsConfiguration.shouldSquelchPost =
                  (unsigned int)parsedBooleanValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "tcp_keepalive" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               flagsConfiguration.shouldUseTcpKeepalive =
                  (unsigned int)parsedBooleanValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "update_title_bar" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               flagsConfiguration.shouldEnableTitleBar =
                  (unsigned int)parsedBooleanValue;
               flagsConfiguration.hasTitleBarSetting = 1;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "use_keychain" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
#ifdef ENABLE_KEYCHAIN
               flagsConfiguration.shouldUseKeychain =
                  (unsigned int)parsedBooleanValue;
#endif
            }
            return true;
         }
         return false;

      case TOML_SECTION_COLORS:
         {
            int colorFieldIndex;

            if ( tryFindColorFieldIndexByTomlKeyName( ptrKeyName, &colorFieldIndex ) )
            {
               if ( tryParseColorValue( ptrValue, ptrKeyName, &parsedIntegerValue ) )
               {
                  setColorFieldValue( colorFieldIndex, parsedIntegerValue );
               }
               return true;
            }
            return false;
         }

      case TOML_SECTION_CONNECTION:
         if ( strcmp( ptrKeyName, "auto_login_name" ) == 0 )
         {
            if ( tryParseTomlQuotedString( ptrValue, aryParsedText, sizeof( aryParsedText ) ) &&
                 strncmp( aryParsedText, "Guest", 5 ) != 0 )
            {
               snprintf( aryAutoName, sizeof( aryAutoName ), "%s", aryParsedText );
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "editor" ) == 0 )
         {
            if ( tryParseTomlQuotedString( ptrValue, aryParsedText, sizeof( aryParsedText ) ) )
            {
               snprintf( aryEditor, sizeof( aryEditor ), "%s", aryParsedText );
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "host" ) == 0 )
         {
            if ( tryParseTomlQuotedString( ptrValue, aryParsedText, sizeof( aryParsedText ) ) &&
                 *aryParsedText != '\0' )
            {
               snprintf( aryBbsHost, sizeof( aryBbsHost ), "%s", aryParsedText );
            }
            else
            {
               stdPrintf( "Invalid string value for '%s' ignored.\n", ptrKeyName );
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "port" ) == 0 )
         {
            if ( tryParseIntegerValue( ptrValue, ptrKeyName, 1, 65535, &parsedIntegerValue ) )
            {
               bbsPort = (unsigned short)parsedIntegerValue;
            }
            return true;
         }
         return false;

      case TOML_SECTION_DEFAULTS:
         if ( strcmp( ptrKeyName, "show_full_profile_by_default" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               applyDefaultUppercasePreference( 'p', parsedBooleanValue );
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "show_long_who_by_default" ) == 0 )
         {
            if ( tryParseBooleanValue( ptrValue, ptrKeyName, &parsedBooleanValue ) )
            {
               applyDefaultUppercasePreference( 'w', parsedBooleanValue );
            }
            return true;
         }
         return false;

      case TOML_SECTION_METADATA:
         if ( strcmp( ptrKeyName, "version" ) == 0 )
         {
            if ( tryParseIntegerValue( ptrValue, ptrKeyName, 0, INT_MAX, &parsedIntegerValue ) )
            {
               version = parsedIntegerValue;
               ptrState->hasVersionSetting = true;
            }
            return true;
         }
         return false;

      case TOML_SECTION_CONTACTS:
         if ( strcmp( ptrKeyName, "enemies" ) == 0 )
         {
            return tryParseContactEnemiesValue( ptrValue );
         }
         if ( strcmp( ptrKeyName, "friends" ) == 0 )
         {
            return tryParseContactFriendsValue( ptrValue );
         }
         return false;

      case TOML_SECTION_LOCAL_COMMAND_KEYS:
         if ( strcmp( ptrKeyName, "away" ) == 0 )
         {
            if ( tryParseLocalCommandKeyValue( ptrValue, ptrKeyName, false, &parsedKeyValue ) )
            {
               awayKey = parsedKeyValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "browser" ) == 0 )
         {
            if ( tryParseLocalCommandKeyValue( ptrValue, ptrKeyName, false, &parsedKeyValue ) )
            {
               browserKey = parsedKeyValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "capture" ) == 0 )
         {
            if ( tryParseLocalCommandKeyValue( ptrValue, ptrKeyName, false, &parsedKeyValue ) )
            {
               captureKey = parsedKeyValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "command" ) == 0 )
         {
            if ( tryParseLocalCommandKeyValue( ptrValue, ptrKeyName, true, &parsedKeyValue ) )
            {
               commandKey = parsedKeyValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "quit" ) == 0 )
         {
            if ( tryParseLocalCommandKeyValue( ptrValue, ptrKeyName, false, &parsedKeyValue ) )
            {
               quitKey = parsedKeyValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "shell" ) == 0 )
         {
            if ( tryParseLocalCommandKeyValue( ptrValue, ptrKeyName, false, &parsedKeyValue ) )
            {
               shellKey = parsedKeyValue;
            }
            return true;
         }
         if ( strcmp( ptrKeyName, "suspend" ) == 0 )
         {
            if ( tryParseLocalCommandKeyValue( ptrValue, ptrKeyName, false, &parsedKeyValue ) )
            {
               suspKey = parsedKeyValue;
            }
            return true;
         }
         return false;

      case TOML_SECTION_NONE:
      case TOML_SECTION_UNKNOWN:
      default:
         (void)ptrState;
         return false;
   }
}

/// @brief Trim leading and trailing ASCII whitespace in place.
///
/// @param ptrText Mutable string buffer to normalize.
///
/// @return Pointer to the trimmed view inside `ptrText`.
static char *trimWhitespace( char *ptrText )
{
   char *ptrEnd;

   while ( *ptrText != '\0' && isspace( (unsigned char)*ptrText ) )
   {
      ptrText++;
   }
   if ( *ptrText == '\0' )
   {
      return ptrText;
   }

   ptrEnd = ptrText + strlen( ptrText ) - 1;
   while ( ptrEnd > ptrText && isspace( (unsigned char)*ptrEnd ) )
   {
      *ptrEnd-- = '\0';
   }

   return ptrText;
}

/// @brief Warn about conflicting local-command key definitions.
///
/// @return This helper does not return a value.
static void warnAboutConfigConflicts( void )
{
   if ( captureKey >= 0 && captureKey == shellKey )
   {
      stdPrintf( "Warning: duplicate definition of 'capture' and 'shell'\n" );
   }
   if ( quitKey >= 0 && quitKey == captureKey )
   {
      stdPrintf( "Warning: duplicate definition of 'quit' and 'capture'\n" );
   }
   if ( quitKey >= 0 && quitKey == shellKey )
   {
      stdPrintf( "Warning: duplicate definition of 'quit' and 'shell'\n" );
   }
   if ( quitKey >= 0 && quitKey == suspKey )
   {
      stdPrintf( "Warning: duplicate definition of 'quit' and 'suspend'\n" );
   }
   if ( suspKey >= 0 && suspKey == captureKey )
   {
      stdPrintf( "Warning: duplicate definition of 'suspend' and 'capture'\n" );
   }
   if ( suspKey >= 0 && suspKey == shellKey )
   {
      stdPrintf( "Warning: duplicate definition of 'suspend' and 'shell'\n" );
   }
}

/// @brief Read config.toml and apply the configured client settings.
///
/// @return This function does not return a value.
void readConfig( void )
{
   char aryLine[MAX_LINE_LENGTH + 1];
   char aryKeyName[MAX_SECTION_NAME_LENGTH];
   char arySectionName[MAX_SECTION_NAME_LENGTH];
   char aryValue[MAX_VALUE_LENGTH];
   ConfigReadState state = { 0 };
   TomlSectionId currentSection;

   initializeConfigLists();
   initializeConfigDefaults();

   currentSection = TOML_SECTION_NONE;
   while ( readNormalizedLine( ptrConfigFile, aryLine, sizeof( aryLine ),
                               &state.lineNumber, &state.reads, "config.toml" ) )
   {
      const char *ptrTrimmedLine;

      ptrTrimmedLine = trimWhitespace( aryLine );
      if ( *ptrTrimmedLine == '\0' || *ptrTrimmedLine == '#' )
      {
         continue;
      }
      if ( ptrTrimmedLine[0] == '[' )
      {
         currentSection = parseTomlSectionLine( ptrTrimmedLine, arySectionName,
                                                sizeof( arySectionName ) );
         if ( currentSection == TOML_SECTION_NONE )
         {
            stdPrintf( "Invalid TOML section header on line %d.\n", state.lineNumber );
         }
         else if ( currentSection == TOML_SECTION_UNKNOWN )
         {
            stdPrintf( "Unknown TOML section '%s' ignored.\n", arySectionName );
         }
         continue;
      }
      if ( !tryParseTomlKeyValueLine( ptrTrimmedLine, aryKeyName, sizeof( aryKeyName ),
                                      aryValue, sizeof( aryValue ) ) )
      {
         stdPrintf( "Invalid TOML syntax on line %d.\n", state.lineNumber );
         continue;
      }
      if ( !tryProcessTomlKeyValue( currentSection, aryKeyName, aryValue, &state ) )
      {
         if ( currentSection == TOML_SECTION_NONE )
         {
            stdPrintf( "TOML key '%s' must appear inside a section.\n", aryKeyName );
         }
         else if ( currentSection != TOML_SECTION_UNKNOWN )
         {
            stdPrintf( "Unknown config key '%s' ignored.\n", aryKeyName );
         }
      }
   }

   (void)tryFinalizeConfigRead( &state );
}
