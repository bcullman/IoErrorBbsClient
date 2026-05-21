/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file handles editing and displaying the friend and enemy lists from
 * the client configuration menu.
 */
#include "client.h"
#include "client_globals.h"
#include "color.h"
#include "config_menu.h"
#include "defs.h"
#include "getline_input.h"
#include "utility.h"
typedef enum
{
   USER_LIST_FRIEND,
   USER_LIST_ENEMY
} UserListKind;

static void addEnemyUser( slist *list, const char *ptrUserName );
static void addFriendUser( slist *list, const char *ptrUserName );
static void editFriendUser( slist *list,
                            int ( *findfn )( const void *, const void * ) );
static void printEnemyList( const slist *list );
static void printFriendList( const slist *list );
static void printThemedFriendListEntry( const friend *ptrFriend );
static void printUserListMenu( const char *name, UserListKind userListKind );
static void showEnemyOptions( void );
static UserListKind userListKindFromName( const char *ptrName );

/// @brief Add a new enemy name to the supplied list.
///
/// @param list Enemy list being edited.
/// @param ptrUserName Name to duplicate and store.
///
/// @return This function does not return a value.
static void addEnemyUser( slist *list, const char *ptrUserName )
{
   size_t enemyNameLength;
   char *ptrEnemyName;

   enemyNameLength = strlen( ptrUserName ) + 1;
   ptrEnemyName = (char *)calloc( 1, enemyNameLength );
   if ( !ptrEnemyName )
   {
      fatalExit( "Out of memory adding 'enemy'!\r\n", "Fatal error" );
      return;
   }

   snprintf( ptrEnemyName, enemyNameLength, "%s", ptrUserName );
   if ( !slistAddItem( list, ptrEnemyName, 0 ) )
   {
      fatalExit( "Can't add 'enemy'!\r\n", "Fatal error" );
   }
}

/// @brief Add a new friend record to the supplied list.
///
/// @param list Friend list being edited.
/// @param ptrUserName Friend name to store.
///
/// @return This function does not return a value.
static void addFriendUser( slist *list, const char *ptrUserName )
{
   char aryInfo[50];
   friend *ptrFriend;

   ptrFriend = (friend *)calloc( 1, sizeof( friend ) );
   if ( !ptrFriend )
   {
      fatalExit( "Out of memory adding 'friend'!\n", "Fatal error" );
      return;
   }

   snprintf( ptrFriend->name, sizeof( ptrFriend->name ), "%s", ptrUserName );
   stdPrintf( "Enter info for %s: ", ptrUserName );
   getString( 48, aryInfo, -999 );
   snprintf( ptrFriend->info, sizeof( ptrFriend->info ), "%s",
             ( *aryInfo ) ? aryInfo : "(None)" );
   ptrFriend->magic = 0x3231;
   if ( !slistAddItem( list, ptrFriend, 0 ) )
   {
      fatalExit( "Can't add 'friend'!\n", "Fatal error" );
   }
}

/// @brief Edit the info text attached to an existing friend entry.
///
/// @param list Friend list being edited.
/// @param findfn Comparison callback used to locate the target entry.
///
/// @return This function does not return a value.
static void editFriendUser( slist *list, int ( *findfn )( const void *, const void * ) )
{
   char aryInfo[50];
   char *ptrUserName;
   friend *ptrFriend;
   int itemIndex;

   stdPrintf( "Edit\r\nName of user to edit: " );
   ptrUserName = getName( -999 );
   if ( !*ptrUserName )
   {
      return;
   }

   itemIndex = slistFind( list, ptrUserName, findfn );
   if ( itemIndex == -1 )
   {
      stdPrintf( "\r\n%s is not in your friend list.\r\n", ptrUserName );
      return;
   }

   ptrFriend = list->items[itemIndex];
   if ( strcmp( ptrFriend->name, ptrUserName ) != 0 )
   {
      return;
   }

   stdPrintf( "Current info: %s\r\n", ptrFriend->info );
   stdPrintf( "Return to leave unchanged, NONE to erase.\r\n" );
   stdPrintf( "Enter new info: " );
   getString( 48, aryInfo, -999 );
   if ( !*aryInfo )
   {
      return;
   }

   if ( strcmp( aryInfo, "NONE" ) == 0 )
   {
      snprintf( ptrFriend->info, sizeof( ptrFriend->info ), "%s", "(None)" );
   }
   else
   {
      snprintf( ptrFriend->info, sizeof( ptrFriend->info ), "%s", aryInfo );
   }
}

/// @brief Run the interactive editor for the friend or enemy list.
///
/// @param list List being edited.
/// @param findfn Comparison callback used to search the list.
/// @param name Display name for the current list, typically `friend` or `enemy`.
///
/// @return This function does not return a value.
void editUsers( slist *list, int ( *findfn )( const void *, const void * ), const char *name )
{
   unsigned int invalid = 0;
   char *ptrUserName;
   UserListKind userListKind;

   userListKind = userListKindFromName( name );

   while ( true )
   {
      int inputChar;

      printUserListMenu( name, userListKind );

      inputChar = inKey();
      switch ( inputChar )
      {
         case 'a':
            stdPrintf( "Add\r\n" );
            stdPrintf( "\r\nUser to add to your %s list -> ", name );
            ptrUserName = getName( -999 );
            if ( !*ptrUserName )
            {
               break;
            }

            if ( slistFind( list, ptrUserName, findfn ) != -1 )
            {
               stdPrintf( "\r\n%s is already on your %s list.\r\n", ptrUserName,
                          name );
               break;
            }

            if ( userListKind == USER_LIST_FRIEND )
            {
               addFriendUser( list, ptrUserName );
            }
            else
            {
               addEnemyUser( list, ptrUserName );
            }
            stdPrintf( "\r\n%s was added to your %s list.\r\n", ptrUserName, name );
            break;

         case 'd':
            stdPrintf( "Delete\r\n\nUser to delete from your %s list -> ", name );
            ptrUserName = getName( -999 );
            if ( *ptrUserName )
            {
               int itemIndex;

               itemIndex = slistFind( list, ptrUserName, findfn );
               if ( itemIndex != -1 )
               {
                  free( list->items[itemIndex] );
                  if ( !slistRemoveItem( list, itemIndex ) )
                  {
                     fatalExit( "Can't remove user!\r\n", "Fatal error" );
                  }
                  stdPrintf( "\r\n%s was deleted from your %s list.\r\n",
                             ptrUserName, name );
               }
               else
               {
                  stdPrintf( "\r\n%s is not in your %s list.\r\n", ptrUserName,
                             name );
               }
            }
            break;

         case 'e':
            if ( userListKind == USER_LIST_ENEMY )
            {
               handleInvalidInput( &invalid );
               continue;
            }
            editFriendUser( list, findfn );
            break;

         case 'l':
            stdPrintf( "List\r\n\n" );
            if ( userListKind == USER_LIST_FRIEND )
            {
               printFriendList( list );
            }
            else
            {
               printEnemyList( list );
            }
            break;

         case 'o':
            if ( userListKind == USER_LIST_ENEMY )
            {
               showEnemyOptions();
               break;
            }
            handleInvalidInput( &invalid );
            continue;

         case 'q':
         case '\n':
         case ' ':
            stdPrintf( "Quit\r\n" );
            return;

         default:
            handleInvalidInput( &invalid );
            continue;
      }
      invalid = 0;
   }
}

/// @brief Print the enemy list in a compact multi-column format.
///
/// @param list Enemy list to display.
///
/// @return This function does not return a value.
static void printEnemyList( const slist *list )
{
   int itemIndex;
   int lines;

   lines = 1;
   for ( itemIndex = 0; itemIndex < (int)list->nitems; itemIndex++ )
   {
      stdPrintf( "%-19s%s", list->items[itemIndex],
                 ( itemIndex % 4 ) == 3 ? "\r\n" : " " );
      if ( ( itemIndex % 4 ) == 3 )
      {
         lines++;
      }
      if ( lines == rows - 1 && more( &lines, -1 ) < 0 )
      {
         break;
      }
   }
   if ( itemIndex % 4 )
   {
      stdPrintf( "\r\n" );
   }
}

/// @brief Print the friend list with themed friend metadata.
///
/// @param list Friend list to display.
///
/// @return This function does not return a value.
static void printFriendList( const slist *list )
{
   int itemIndex;
   int lines;

   lines = 1;
   for ( itemIndex = 0; itemIndex < (int)list->nitems; itemIndex++ )
   {
      const friend *ptrFriend = list->items[itemIndex];
      printThemedFriendListEntry( ptrFriend );
      lines++;
      if ( lines == rows - 1 && more( &lines, -1 ) < 0 )
      {
         break;
      }
   }
}

/// @brief Print one themed friend-list entry.
///
/// @param ptrFriend Friend record to display.
///
/// @return This function does not return a value.
static void printThemedFriendListEntry( const friend *ptrFriend )
{
   if ( flagsConfiguration.shouldUseAnsi )
   {
      char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];

      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.postFriendName );
      stdPrintf( "%s", aryAnsiSequence );
      stdPrintf( "%-20s ", ptrFriend->name );
      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.postFriendText );
      stdPrintf( "%s", aryAnsiSequence );
      stdPrintf( "%s\r\n", ptrFriend->info );
      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.text );
      stdPrintf( "%s", aryAnsiSequence );
      return;
   }

   stdPrintf( "%-20s %s\r\n", ptrFriend->name, ptrFriend->info );
}

/// @brief Print the prompt line for the friend or enemy list editor.
///
/// @param name Display name for the current list.
/// @param userListKind Kind of list being edited.
///
/// @return This function does not return a value.
static void printUserListMenu( const char *name, UserListKind userListKind )
{
   char aryDisplayLine[80];

   if ( userListKind == USER_LIST_ENEMY )
   {
      printThemedMnemonicText( "\r\n<A>dd  <D>elete  <L>ist  <O>ptions  <Q>uit",
                               color.number );
   }
   else
   {
      printThemedMnemonicText( "\r\n<A>dd  <D>elete  <E>dit  <L>ist  <Q>uit",
                               color.number );
   }

   snprintf( aryDisplayLine, sizeof( aryDisplayLine ), "\r\n%c%s list -> ",
             toupper( name[0] ), name + 1 );
   printThemedMnemonicText( aryDisplayLine, color.forum );
   printAnsiForegroundColorValue( color.text );
}

/// @brief Configure whether killed enemy content still generates notifications.
///
/// @return This function does not return a value.
static void showEnemyOptions( void )
{
   stdPrintf( "Options\r\n\nNotify when an enemy's post is killed? (%s) -> ",
              flagsConfiguration.shouldSquelchPost ? "No" : "Yes" );
   flagsConfiguration.shouldSquelchPost =
      !yesNoDefault( !flagsConfiguration.shouldSquelchPost );
   stdPrintf( "Notify when an enemy's eXpress message is killed? (%s) -> ",
              flagsConfiguration.shouldSquelchExpress ? "No" : "Yes" );
   flagsConfiguration.shouldSquelchExpress =
      !yesNoDefault( !flagsConfiguration.shouldSquelchExpress );
}

/// @brief Map a list name string to its internal list kind.
///
/// @param ptrName List name to classify.
///
/// @return `USER_LIST_ENEMY` for `enemy`; otherwise `USER_LIST_FRIEND`.
static UserListKind userListKindFromName( const char *ptrName )
{
   if ( strcmp( ptrName, "enemy" ) == 0 )
   {
      return USER_LIST_ENEMY;
   }

   return USER_LIST_FRIEND;
}
