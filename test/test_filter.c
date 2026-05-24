/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config_file.h"
#include "browser.h"
#include "client.h"
#include "test/cmocka_compat.h"
#include "color.h"
#include "config_menu.h"
#include "defs.h"
#include "edit.h"
#include "ext.h"
#include "filter.h"
#include "getline_input.h"
#include "macos_keychain.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include "telnet.h"
#include "utility.h"
static char aryPrintLog[32768];
static char aryLastKeychainServerLine[320];
static int sendAnXCallCount;

static void resetState( void )
{
   aryPrintLog[0] = '\0';
   aryLastKeychainServerLine[0] = '\0';
   sendAnXCallCount = 0;

   isAway = false;
   isXland = false;
   isExpressMessageInProgress = false;
   shouldSendExpressMessage = false;
   highestExpressMessageId = 0;
   pendingLinesToEat = 0;
   postProgressState = 0;
   whoListProgress = 0;
   savedWhoCount = 0;
   byte = 0;

   flagsConfiguration.shouldSquelchExpress = 0;
   flagsConfiguration.shouldSquelchPost = 0;
   flagsConfiguration.shouldUseAnsi = 0;
   flagsConfiguration.isMorePromptActive = 0;
   flagsConfiguration.shouldDisableBold = 0;
   flagsConfiguration.shouldUseBold = 0;
   flagsConfiguration.shouldEnableClickableUrls = 1;
   flagsConfiguration.isScreenReaderModeEnabled = 0;
   unsetenv( "TERM" );
   unsetenv( "TERM_PROGRAM" );

   aryExpressMessageBuffer[0] = '\0';
   ptrExpressMessageBuffer = aryExpressMessageBuffer;
}

static void resetLists( void )
{
   if ( urlQueue != NULL )
   {
      deleteQueue( urlQueue );
      urlQueue = NULL;
   }
   if ( xlandQueue != NULL )
   {
      deleteQueue( xlandQueue );
      xlandQueue = NULL;
   }
   if ( enemyList != NULL )
   {
      slistDestroyItems( enemyList );
      slistDestroy( enemyList );
      enemyList = NULL;
   }
   if ( friendList != NULL )
   {
      slistDestroyItems( friendList );
      slistDestroy( friendList );
      friendList = NULL;
   }
   if ( whoList != NULL )
   {
      slistDestroyItems( whoList );
      slistDestroy( whoList );
      whoList = NULL;
   }
}

static void addFriend( const char *ptrName, const char *ptrInfo )
{
   friend *ptrFriend;

   if ( friendList == NULL )
   {
      friendList = slistCreate( 0, fSortCompareVoid );
      if ( friendList == NULL )
      {
         fail_msg( "slistCreate failed while preparing friendList for filter tests" );
         return;
      }
   }

   ptrFriend = calloc( 1, sizeof( friend ) );
   if ( ptrFriend == NULL )
   {
      fail_msg( "calloc failed while creating friend entry for filter tests" );
      return;
   }

   ptrFriend->magic = 0x3231;
   snprintf( ptrFriend->name, sizeof( ptrFriend->name ), "%s", ptrName );
   snprintf( ptrFriend->info, sizeof( ptrFriend->info ), "%s", ptrInfo );

   if ( !slistAddItem( friendList, ptrFriend, 1 ) )
   {
      free( ptrFriend );
      fail_msg( "slistAddItem failed while adding friend entry for filter tests" );
   }
}

// filter.c dependencies not under direct test in this file.
int ansiTransform( int inputChar )
{
   return inputChar;
}

void ansiTransformExpress( char *ptrText, size_t size )
{
   (void)ptrText;
   (void)size;
}

int formatTransformedAnsiForegroundSequence( char *ptrBuffer, size_t bufferSize,
                                             int inputChar, int isPostContext,
                                             int isFriend )
{
   int colorValue;

   (void)isPostContext;
   (void)isFriend;

   colorValue = inputChar == '6' ? 13 : colorValueFromLegacyDigit( inputChar );
   return formatAnsiForegroundSequence( ptrBuffer, bufferSize, colorValue );
}

int ansiTransformPost( int inputChar, int isFriend )
{
   (void)isFriend;
   return inputChar;
}

void printAnsiDisplayStateValue( int foregroundColor, int backgroundColor )
{
   char aryAnsiSequence[32];

   formatAnsiDisplayStateSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                   foregroundColor, backgroundColor,
                                   flagsConfiguration.shouldUseBold );
   stdPrintf( "%s", aryAnsiSequence );
}

void ansiTransformPostHeader( char *ptrText, size_t bufferSize, int isFriend )
{
   (void)ptrText;
   (void)bufferSize;
   (void)isFriend;
}

int colorize( const char *ptrText )
{
   (void)ptrText;
   return 1;
}

int colorValueFromLegacyDigit( int inputChar )
{
   if ( inputChar >= '0' && inputChar <= '9' )
   {
      return inputChar - '0';
   }

   return inputChar;
}

int colorValueToLegacyDigit( int colorValue )
{
   return colorValue + '0';
}

char *extractName( const char *ptrHeader )
{
   static char aryName[64];
   const char *ptrStart;
   const char *ptrEnd;
   size_t nameLength;

   ptrStart = strstr( ptrHeader, " from " );
   if ( ptrStart == NULL )
   {
      return NULL;
   }
   ptrStart += 6;

   ptrEnd = strstr( ptrStart, " at " );
   if ( ptrEnd == NULL )
   {
      ptrEnd = ptrStart + strlen( ptrStart );
   }

   nameLength = (size_t)( ptrEnd - ptrStart );
   if ( nameLength >= sizeof( aryName ) )
   {
      nameLength = sizeof( aryName ) - 1;
   }
   memcpy( aryName, ptrStart, nameLength );
   aryName[nameLength] = '\0';
   return aryName;
}

char *extractNameNoHistory( const char *header )
{
   return extractName( header );
}

int extractNumber( const char *ptrHeader )
{
   const char *ptrStart;
   int number;

   ptrStart = strstr( ptrHeader, "(#" );
   if ( ptrStart == NULL )
   {
      return 0;
   }
   ptrStart += 2;
   number = 0;
   while ( *ptrStart && isdigit( (unsigned char)*ptrStart ) )
   {
      number = number * 10 + ( *ptrStart - '0' );
      ptrStart++;
   }
   return number;
}

noreturn void fatalExit( const char *message, const char *heading )
{
   fail_msg( "fatalExit was called unexpectedly: %s (%s)", message, heading );
   abort();
}

char *findChar( const char *ptrString, int targetChar )
{
   const char *ptrFound;

   ptrFound = strchr( ptrString, targetChar );
   return (char *)ptrFound;
}

char *findSubstring( const char *ptrString, const char *ptrSubstring )
{
   const char *ptrFound;

   ptrFound = strstr( ptrString, ptrSubstring );
   return (char *)ptrFound;
}

int fStrCompareVoid( const void *ptrName, const void *ptrFriend )
{
   const friend *ptrEntry;

   ptrEntry = ptrFriend;
   return strcmp( (const char *)ptrName, ptrEntry->name );
}

int netPrintf( const char *format, ... )
{
   va_list argList;

   (void)format;
   va_start( argList, format );
   va_end( argList );
   return 1;
}

int netPutChar( int inputChar )
{
   return inputChar;
}

void clearKeychainSessionState( void )
{
   // Test stub: keychain session state is not relevant in this test.
}

bool tryDeleteKeychainPassword( const char *ptrHost, const char *ptrUser )
{
   (void)ptrHost;
   (void)ptrUser;
   return false;
}

bool tryGetKeychainPassword( const char *ptrHost, const char *ptrUser,
                             char *ptrPassword, size_t passwordSize )
{
   (void)ptrHost;
   (void)ptrPassword;
   (void)passwordSize;
   (void)ptrUser;
   return false;
}

void handleKeychainHiddenInput( const char *ptrPassword )
{
   (void)ptrPassword;
}

void handleKeychainServerLine( const char *ptrLine )
{
   snprintf( aryLastKeychainServerLine, sizeof( aryLastKeychainServerLine ),
             "%s", ptrLine );
}

void recordCurrentBbsUser( const char *ptrUser )
{
   (void)ptrUser;
}

bool trySetKeychainPassword( const char *ptrHost, const char *ptrUser,
                             const char *ptrPassword )
{
   (void)ptrHost;
   (void)ptrPassword;
   (void)ptrUser;
   return false;
}

void sPerror( const char *message, const char *heading )
{
   (void)message;
   (void)heading;
}

void sendAnX( void )
{
   sendAnXCallCount++;
}

int sortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   const char *const *ptrLeftString;
   const char *const *ptrRightString;

   ptrLeftString = ptrLeft;
   ptrRightString = ptrRight;
   return strcmp( *ptrLeftString, *ptrRightString );
}

int stdPrintf( const char *format, ... )
{
   va_list argList;
   char aryBuffer[1024];
   size_t logLength;

   (void)format;
   va_start( argList, format );
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
   vsnprintf( aryBuffer, sizeof( aryBuffer ), format, argList );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
   va_end( argList );

   logLength = strlen( aryPrintLog );
   if ( logLength < sizeof( aryPrintLog ) - 1 )
   {
      snprintf( aryPrintLog + logLength, sizeof( aryPrintLog ) - logLength, "%s", aryBuffer );
   }

   return 1;
}

int stdPutChar( int inputChar )
{
   return inputChar;
}

bool tryGetKeychainPasswordForPrompt( char *ptrPassword, size_t passwordSize )
{
   (void)ptrPassword;
   (void)passwordSize;
   return false;
}

bool tryUpsertKeychainPassword( const char *ptrHost, const char *ptrUser,
                                const char *ptrPassword )
{
   (void)ptrHost;
   (void)ptrPassword;
   (void)ptrUser;
   return false;
}

void getString( int length, char *result, int line )
{
   (void)line;
   if ( length > 0 && result != NULL )
   {
      result[0] = '\0';
   }
}

int strCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   return strcmp( (const char *)ptrLeft, (const char *)ptrRight );
}

int fSortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   const friend *const *ptrLeftFriend;
   const friend *const *ptrRightFriend;

   ptrLeftFriend = ptrLeft;
   ptrRightFriend = ptrRight;
   return strcmp( ( *ptrLeftFriend )->name, ( *ptrRightFriend )->name );
}

static void isAutomaticReply_WhenReplyPrefixPresent_ReturnsTrue( void **state )
{
   // Arrange
   int result;

   (void)state;

   resetState();

   // Act
   result = isAutomaticReply( "*** Message (#4) from Dr Strange at 8:30 PM\r\n>+!R Back soon" );

   // Assert
   if ( result != 1 )
   {
      fail_msg( "isAutomaticReply should detect '+!R' reply marker; got %d", result );
   }
}

static void replyCodeTransformExpress_WhenReplyCodePresent_RemovesReplyCodePrefix( void **state )
{
   // Arrange
   char aryMessage[256];

   (void)state;

   resetState();
   snprintf( aryMessage, sizeof( aryMessage ), "%s", "*** Message (#9) from Meatball at 9:12 PM\r\n>+!R Copy that" );

   // Act
   replyCodeTransformExpress( aryMessage, sizeof( aryMessage ) );

   // Assert
   if ( strstr( aryMessage, ">+!R " ) != NULL )
   {
      fail_msg( "replyCodeTransformExpress should remove '+!R ' marker; got '%s'", aryMessage );
   }
   if ( strstr( aryMessage, ">Copy that" ) == NULL )
   {
      fail_msg( "replyCodeTransformExpress should preserve message body; got '%s'", aryMessage );
   }
}

static void notReplyingTransformExpress_WhenHeaderContainsAt_InsertsNotReplyingLabel( void **state )
{
   // Arrange
   char aryMessage[256];

   (void)state;

   resetState();
   snprintf( aryMessage, sizeof( aryMessage ), "%s", "*** Message (#9) from Dr Strange at 9:12 PM" );

   // Act
   notReplyingTransformExpress( aryMessage, sizeof( aryMessage ) );

   // Assert
   if ( strstr( aryMessage, "(not replying)" ) == NULL )
   {
      fail_msg( "notReplyingTransformExpress should insert '(not replying)'; got '%s'", aryMessage );
   }
}

static void filterUrl_WhenDuplicateSeen_QueuesOnlyOnce( void **state )
{
   // Arrange
   char aryUrl[1024];
   int firstPopResult;
   int secondPopResult;

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for urlQueue setup in duplicate URL test" );
   }

   // Act
   filterUrl( "Did I leave it here? https://bbs.example.net/threads/42" );
   filterUrl( "Again: https://bbs.example.net/threads/42" );

   // Assert
   if ( urlQueue->itemCount != 1 )
   {
      fail_msg( "duplicate URLs should be queued only once; queue count is %d", urlQueue->itemCount );
   }
   memset( aryUrl, 0, sizeof( aryUrl ) );
   firstPopResult = popQueue( aryUrl, urlQueue );
   secondPopResult = popQueue( aryUrl, urlQueue );
   if ( firstPopResult != 1 || secondPopResult != 0 )
   {
      fail_msg( "URL queue should contain exactly one element; pop results were %d then %d", firstPopResult, secondPopResult );
   }
   if ( strcmp( aryUrl, "https://bbs.example.net/threads/42" ) != 0 )
   {
      fail_msg( "queued URL mismatch; got '%s'", aryUrl );
   }

   resetLists();
}

static void filterUrl_WhenQueueIsFull_EvictsOldestUrl( void **state )
{
   // Arrange
   char aryFirst[1024];
   char arySecond[1024];

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 2 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for URL overflow test setup" );
   }

   // Act
   filterUrl( "One https://example.net/one" );
   filterUrl( "Two https://example.net/two" );
   filterUrl( "Three https://example.net/three" );

   // Assert
   if ( urlQueue->itemCount != 2 )
   {
      fail_msg( "URL queue size should stay capped at 2; got %d", urlQueue->itemCount );
   }
   memset( aryFirst, 0, sizeof( aryFirst ) );
   memset( arySecond, 0, sizeof( arySecond ) );
   popQueue( aryFirst, urlQueue );
   popQueue( arySecond, urlQueue );
   if ( strcmp( aryFirst, "https://example.net/two" ) != 0 || strcmp( arySecond, "https://example.net/three" ) != 0 )
   {
      fail_msg( "URL queue eviction order mismatch; got '%s' then '%s'", aryFirst, arySecond );
   }

   resetLists();
}

static void filterUrl_WhenHttpsAndTrailingPunctuationPresent_ExtractsCleanUrl( void **state )
{
   // Arrange
   char aryUrl[1024];
   int popResult;

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for HTTPS URL punctuation test setup" );
   }

   // Act
   filterUrl( "Read this: https://example.dev/path?q=1#frag)." );

   // Assert
   memset( aryUrl, 0, sizeof( aryUrl ) );
   popResult = popQueue( aryUrl, urlQueue );
   if ( popResult != 1 )
   {
      fail_msg( "HTTPS URL should be queued exactly once; pop returned %d", popResult );
   }
   if ( strcmp( aryUrl, "https://example.dev/path?q=1#frag" ) != 0 )
   {
      fail_msg( "HTTPS URL should trim trailing punctuation; got '%s'", aryUrl );
   }

   resetLists();
}

static void filterUrl_WhenWwwUrlPresent_QueuesUrlWithoutTldList( void **state )
{
   // Arrange
   char aryUrl[1024];
   int popResult;

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for www URL test setup" );
   }

   // Act
   filterUrl( "Mirror is at www.example.photography/section/alpha, check it out." );

   // Assert
   memset( aryUrl, 0, sizeof( aryUrl ) );
   popResult = popQueue( aryUrl, urlQueue );
   if ( popResult != 1 )
   {
      fail_msg( "www URL should be queued exactly once; pop returned %d", popResult );
   }
   if ( strcmp( aryUrl, "www.example.photography/section/alpha" ) != 0 )
   {
      fail_msg( "www URL parsing should support modern TLDs and trim punctuation; got '%s'", aryUrl );
   }

   resetLists();
}

static void filterUrl_WhenInvalidWwwBoundaryPrecedesHttps_QueuesLaterHttpsUrl( void **state )
{
   // Arrange
   char aryUrl[1024];
   int popResult;

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for invalid www boundary regression test setup" );
   }

   // Act
   filterUrl( "notwww.example then https://example.com" );

   // Assert
   memset( aryUrl, 0, sizeof( aryUrl ) );
   popResult = popQueue( aryUrl, urlQueue );
   if ( popResult != 1 )
   {
      fail_msg( "later HTTPS URL should still be queued when earlier www. has an invalid boundary; pop returned %d",
                popResult );
   }
   if ( strcmp( aryUrl, "https://example.com" ) != 0 )
   {
      fail_msg( "later HTTPS URL should be preserved after skipping invalid www. prefix; got '%s'",
                aryUrl );
   }

   resetLists();
}

static void filterUrl_WhenHttpOrFtpPresent_DoesNotQueueUnsupportedSchemes( void **state )
{
   // Arrange
   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for unsupported scheme test setup" );
   }

   // Act
   filterUrl( "Legacy link http://example.net/legacy should be ignored." );
   filterUrl( "Another legacy link ftp://example.net/pub/file should be ignored." );

   // Assert
   if ( urlQueue->itemCount != 0 )
   {
      fail_msg( "unsupported schemes should not be queued; queue count is %d", urlQueue->itemCount );
   }

   resetLists();
}

static void filterUrl_WhenHttpsSchemeWrapsAfterSlashes_CombinesIntoSingleUrl( void **state )
{
   // Arrange
   char aryUrl[1024];
   int firstPopResult;
   int secondPopResult;

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for split HTTPS scheme test setup" );
   }

   // Act
   filterUrl( "Read this https://" );
   filterUrl( "www.whatever.com/" );
   filterUrl( "Done." );

   // Assert
   memset( aryUrl, 0, sizeof( aryUrl ) );
   firstPopResult = popQueue( aryUrl, urlQueue );
   secondPopResult = popQueue( aryUrl, urlQueue );
   if ( firstPopResult != 1 || secondPopResult != 0 )
   {
      fail_msg( "split HTTPS scheme should queue exactly one URL; pop results were %d then %d",
                firstPopResult, secondPopResult );
   }
   if ( strcmp( aryUrl, "https://www.whatever.com/" ) != 0 )
   {
      fail_msg( "split HTTPS scheme should reconstruct one URL; got '%s'", aryUrl );
   }

   resetLists();
}

static void filterUrl_WhenHttpsSchemeWrapsBeforeSlashes_CombinesIntoSingleUrl( void **state )
{
   // Arrange
   char aryUrl[1024];
   int popResult;

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for split HTTPS scheme test setup" );
   }

   // Act
   filterUrl( "Read this https:" );
   filterUrl( "//www.whatever.com/" );
   filterUrl( "Done." );

   // Assert
   memset( aryUrl, 0, sizeof( aryUrl ) );
   popResult = popQueue( aryUrl, urlQueue );
   if ( popResult != 1 )
   {
      fail_msg( "split HTTPS scheme should be queued once; pop returned %d", popResult );
   }
   if ( strcmp( aryUrl, "https://www.whatever.com/" ) != 0 )
   {
      fail_msg( "split HTTPS scheme should reconstruct one URL; got '%s'", aryUrl );
   }

   resetLists();
}

static void filterUrl_WhenHttpsWrapsAcrossLines_CombinesIntoSingleUrl( void **state )
{
   // Arrange
   char aryUrl[1024];
   int popResult;

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for wrapped URL test setup" );
   }

   // Act
   filterUrl( "https://arstechnica.com/gadgets/2026/01/inside-nvidias-10-year-effort-to-make-t" );
   filterUrl( "he-shield-tv-the-most-updated-android-device-ever/" );
   filterUrl( "Also still gets software updates!" );

   // Assert
   memset( aryUrl, 0, sizeof( aryUrl ) );
   popResult = popQueue( aryUrl, urlQueue );
   if ( popResult != 1 )
   {
      fail_msg( "wrapped HTTPS URL should be queued once; pop returned %d", popResult );
   }
   if ( strcmp( aryUrl, "https://arstechnica.com/gadgets/2026/01/inside-nvidias-10-year-effort-to-make-the-shield-tv-the-most-updated-android-device-ever/" ) != 0 )
   {
      fail_msg( "wrapped URL should be reconstructed into one URL; got '%s'", aryUrl );
   }

   resetLists();
}

static void filterUrl_WhenHttpsWrapsAcrossLinesWithShortFirstFragment_CombinesIntoSingleUrl( void **state )
{
   // Arrange
   char aryUrl[1024];
   int popResult;

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for shorter wrapped URL test setup" );
   }

   // Act
   filterUrl( "Read this: https://cybernews.com/security/google-chrome-ai-model-" );
   filterUrl( "device-no-consent/" );
   filterUrl( "This is the rest of the sentence." );

   // Assert
   memset( aryUrl, 0, sizeof( aryUrl ) );
   popResult = popQueue( aryUrl, urlQueue );
   if ( popResult != 1 )
   {
      fail_msg( "wrapped Cybernews URL should be queued once; pop returned %d", popResult );
   }
   if ( strcmp( aryUrl, "https://cybernews.com/security/google-chrome-ai-model-device-no-consent/" ) != 0 )
   {
      fail_msg( "wrapped Cybernews URL should be reconstructed into one URL; got '%s'", aryUrl );
   }

   resetLists();
}

static void filterUrl_WhenHttpsStartsLateOnFullWidthLine_CombinesIntoSingleUrl( void **state )
{
   // Arrange
   char aryFirstLine[128];
   char aryUrl[1024];
   const char *ptrFirstFragment;
   int popResult;
   size_t prefixLength;

   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for late-start wrapped URL test setup" );
   }

   ptrFirstFragment = "https://example.dev/abc";
   prefixLength = 80 - strlen( ptrFirstFragment );
   memset( aryFirstLine, 'x', prefixLength - 1 );
   aryFirstLine[prefixLength - 1] = ' ';
   snprintf( aryFirstLine + prefixLength, sizeof( aryFirstLine ) - prefixLength,
             "%s", ptrFirstFragment );

   // Act
   filterUrl( aryFirstLine );
   filterUrl( "defghijklmnopqrstuvwxyz" );
   filterUrl( "This is the rest of the sentence." );

   // Assert
   memset( aryUrl, 0, sizeof( aryUrl ) );
   popResult = popQueue( aryUrl, urlQueue );
   if ( popResult != 1 )
   {
      fail_msg( "late-start wrapped HTTPS URL should be queued once; pop returned %d",
                popResult );
   }
   if ( strcmp( aryUrl, "https://example.dev/abcdefghijklmnopqrstuvwxyz" ) != 0 )
   {
      fail_msg( "late-start wrapped URL should be reconstructed into one URL; got '%s'",
                aryUrl );
   }

   resetLists();
}

static void filterUrl_WhenIncompleteHttpsSchemeDoesNotContinue_QueuesNoUrl( void **state )
{
   // Arrange
   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for incomplete HTTPS scheme test setup" );
   }

   // Act
   filterUrl( "This line ends with https:" );
   filterUrl( "but the next line is normal text." );
   filterUrl( " " );

   // Assert
   if ( urlQueue->itemCount != 0 )
   {
      fail_msg( "incomplete HTTPS scheme should not be queued; queue count is %d",
                urlQueue->itemCount );
   }

   resetLists();
}

static void printWithOsc8Links_WhenTextContainsHttpsUrl_EmitsHyperlinkEscapes( void **state )
{
   // Arrange
   const char *ptrExpectedTarget;
   const char *ptrMessage;

   (void)state;

   resetState();
   ptrMessage = "Read this https://example.dev/path?q=1 now";
   ptrExpectedTarget = ";https://example.dev/path?q=1\033\\";

   // Act
   printWithOsc8Links( ptrMessage );

   // Assert
   if ( strstr( aryPrintLog, "\033]8;id=" ) == NULL )
   {
      fail_msg( "OSC-8 opening sequence should include an explicit hyperlink ID; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, ptrExpectedTarget ) == NULL )
   {
      fail_msg( "OSC-8 opening sequence for HTTPS URL should include the full target and ST terminator; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, "\033]8;;\033\\" ) == NULL )
   {
      fail_msg( "OSC-8 closing sequence should use the ST terminator; log was: %s", aryPrintLog );
   }
}

static void printWithOsc8Links_WhenTextContainsWwwUrl_UsesHttpsTarget( void **state )
{
   // Arrange
   const char *ptrExpectedTarget;
   const char *ptrMessage;

   (void)state;

   resetState();
   ptrMessage = "Mirror: www.example.photography/alpha";
   ptrExpectedTarget = ";https://www.example.photography/alpha\033\\";

   // Act
   printWithOsc8Links( ptrMessage );

   // Assert
   if ( strstr( aryPrintLog, ptrExpectedTarget ) == NULL )
   {
      fail_msg( "OSC-8 target for www URL should be normalized to https://...; log was: %s", aryPrintLog );
   }
}

static void printWithOsc8Links_WhenTextContainsHttpsWwwUrl_EmitsSingleHyperlink( void **state )
{
   // Arrange
   const char *ptrExpectedTarget;
   const char *ptrMessage;
   const char *ptrOpeningSequence;
   const char *ptrVisibleText;

   (void)state;

   resetState();
   ptrMessage = "Visit https://www.example.com/ now";
   ptrExpectedTarget = ";https://www.example.com/\033\\";

   // Act
   printWithOsc8Links( ptrMessage );

   // Assert
   if ( strstr( aryPrintLog, ptrExpectedTarget ) == NULL )
   {
      fail_msg( "OSC-8 target for https://www URL should include the full URL; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, "https://\033]8;" ) != NULL )
   {
      fail_msg( "OSC-8 output should not split https://www URLs at the scheme boundary; log was: %s", aryPrintLog );
   }
   ptrOpeningSequence = strstr( aryPrintLog, "\033]8;id=" );
   if ( ptrOpeningSequence == NULL )
   {
      fail_msg( "OSC-8 opening sequence should be present; log was: %s", aryPrintLog );
   }
   ptrVisibleText = strstr( ptrOpeningSequence, "\033\\" );
   if ( ptrVisibleText == NULL )
   {
      fail_msg( "OSC-8 opening sequence should end before visible text; log was: %s", aryPrintLog );
   }
   ptrVisibleText += 2;
   if ( strncmp( ptrVisibleText, "https://www.example.com/", 24 ) != 0 )
   {
      fail_msg( "OSC-8 visible link text should start with the full HTTPS URL; log was: %s", aryPrintLog );
   }
}

static void printWithOsc8Links_WhenInvalidWwwBoundaryPrecedesHttps_EmitsLaterHttpsHyperlink( void **state )
{
   // Arrange
   const char *ptrExpectedTarget;
   const char *ptrMessage;

   (void)state;

   resetState();
   ptrMessage = "notwww.example then https://example.com";
   ptrExpectedTarget = ";https://example.com\033\\";

   // Act
   printWithOsc8Links( ptrMessage );

   // Assert
   if ( strstr( aryPrintLog, ptrExpectedTarget ) == NULL )
   {
      fail_msg( "OSC-8 output should skip invalid www. boundaries and link the later HTTPS URL; log was: %s",
                aryPrintLog );
   }
}

static void printWithOsc8Links_WhenHttpsUrlExceedsPrintfBuffer_StillClosesHyperlink( void **state )
{
   // Arrange
   char aryMessage[900];
   size_t messageLength;

   (void)state;

   resetState();
   snprintf( aryMessage, sizeof( aryMessage ), "%s", "https://www.example.com/" );
   messageLength = strlen( aryMessage );
   while ( messageLength < 700 && messageLength + 1 < sizeof( aryMessage ) )
   {
      aryMessage[messageLength++] = 'a';
      aryMessage[messageLength] = '\0';
   }

   // Act
   printWithOsc8Links( aryMessage );

   // Assert
   if ( strstr( aryPrintLog, "\033]8;id=" ) == NULL )
   {
      fail_msg( "long OSC-8 URL should include an opening sequence; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, "\033]8;;\033\\" ) == NULL )
   {
      fail_msg( "long OSC-8 URL should not truncate the closing sequence; log was: %s", aryPrintLog );
   }
}

static void printWithOsc8Links_WhenGhosttyAndTextEqualsHttpsTarget_StillEmitsOsc8Hyperlink( void **state )
{
   // Arrange
   const char *ptrExpectedTarget;
   const char *ptrMessage;

   (void)state;

   resetState();
   setenv( "TERM_PROGRAM", "ghostty", 1 );
   ptrMessage = "Visit https://www.example.com/ now";
   ptrExpectedTarget = ";https://www.example.com/\033\\";

   // Act
   printWithOsc8Links( ptrMessage );

   // Assert
   if ( strstr( aryPrintLog, "\033]8;id=" ) == NULL )
   {
      fail_msg( "Ghostty should still receive an OSC-8 hyperlink with an explicit ID; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, ptrExpectedTarget ) == NULL )
   {
      fail_msg( "Ghostty OSC-8 hyperlink should include the full HTTPS target with ST terminator; log was: %s", aryPrintLog );
   }
}

static void printWithOsc8Links_WhenClickableUrlsDisabled_PrintsPlainText( void **state )
{
   // Arrange
   const char *ptrMessage;

   (void)state;

   resetState();
   flagsConfiguration.shouldEnableClickableUrls = 0;
   ptrMessage = "Read this https://example.dev/path";

   // Act
   printWithOsc8Links( ptrMessage );

   // Assert
   if ( strstr( aryPrintLog, "\033]8;;" ) != NULL )
   {
      fail_msg( "OSC-8 escapes should not be emitted when clickable URLs are disabled; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, ptrMessage ) == NULL )
   {
      fail_msg( "plain message text should be printed when clickable URLs are disabled; log was: %s", aryPrintLog );
   }
}

static void printWithOsc8Links_WhenScreenReaderModeEnabled_PrintsPlainText( void **state )
{
   // Arrange
   const char *ptrMessage;

   (void)state;

   resetState();
   flagsConfiguration.isScreenReaderModeEnabled = 1;
   ptrMessage = "Read this https://example.dev/path";

   // Act
   printWithOsc8Links( ptrMessage );

   // Assert
   if ( strstr( aryPrintLog, "\033]8;;" ) != NULL )
   {
      fail_msg( "OSC-8 escapes should not be emitted when screen reader mode is enabled; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, ptrMessage ) == NULL )
   {
      fail_msg( "plain message text should be printed when screen reader mode is enabled; log was: %s", aryPrintLog );
   }
}

static void emitUrlDetectionReport_WhenUrlsCollected_PrintsClickableSummary( void **state )
{
   // Arrange
   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for URL detection report test setup" );
   }

   beginUrlDetectionReport();
   filterUrl( "Read this https://example.dev/alpha" );
   filterUrl( "and this www.example.photography/beta" );

   // Act
   emitUrlDetectionReport();

   // Assert
   if ( strstr( aryPrintLog, "[Clickable URL(s) detected by BBS client]" ) == NULL )
   {
      fail_msg( "URL detection report header was not emitted; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, ";https://example.dev/alpha\033\\" ) == NULL )
   {
      fail_msg( "URL detection report should include clickable HTTPS URL; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, ";https://www.example.photography/beta\033\\" ) == NULL )
   {
      fail_msg( "URL detection report should include clickable normalized www URL; log was: %s", aryPrintLog );
   }

   resetLists();
}

static void emitUrlDetectionReport_WhenWrappedUrlStillPending_FlushesBeforePrintingSummary( void **state )
{
   // Arrange
   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for pending wrapped URL report test setup" );
   }

   beginUrlDetectionReport();
   filterUrl( "Read this: https://cybernews.com/security/google-chrome-ai-model-" );
   filterUrl( "device-no-consent/" );

   // Act
   emitUrlDetectionReport();

   // Assert
   if ( strstr( aryPrintLog, "[Clickable URL(s) detected by BBS client]" ) == NULL )
   {
      fail_msg( "URL detection report header was not emitted for pending wrapped URL; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog,
                ";https://cybernews.com/security/google-chrome-ai-model-device-no-consent/\033\\" ) == NULL )
   {
      fail_msg( "pending wrapped URL should be flushed into the clickable summary; log was: %s", aryPrintLog );
   }

   resetLists();
}

static void beginUrlDetectionReport_WhenPriorWrappedUrlWasPending_ClearsPendingState( void **state )
{
   // Arrange
   (void)state;

   resetState();
   resetLists();
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for pending URL reset test setup" );
   }

   beginUrlDetectionReport();
   filterUrl( "Read this: https://example.dev/wrapped-" );

   // Act
   beginUrlDetectionReport();
   filterUrl( "No URLs here." );
   emitUrlDetectionReport();

   // Assert
   if ( strstr( aryPrintLog, "https://example.dev/wrapped-" ) != NULL )
   {
      fail_msg( "new URL detection report should clear pending wrapped URL state; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, "[Clickable URL(s) detected by BBS client]" ) != NULL )
   {
      fail_msg( "report with no URLs should stay empty after clearing pending state; log was: %s", aryPrintLog );
   }

   resetLists();
}

static void emitUrlDetectionReport_WhenAnsiEnabled_UsesConfiguredColorState( void **state )
{
   // Arrange
   char aryExpectedBodyColor[32];
   char aryExpectedHeaderColor[32];

   (void)state;

   resetState();
   resetLists();
   flagsConfiguration.shouldUseAnsi = 1;
   flagsConfiguration.shouldUseBold = 0;
   color.number = 6;
   color.text = 2;
   color.background = 4;
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for ANSI URL detection report test setup" );
   }

   beginUrlDetectionReport();
   filterUrl( "Read this https://example.dev/alpha" );

   // Act
   emitUrlDetectionReport();

   // Assert
   formatAnsiDisplayStateSequence( aryExpectedHeaderColor, sizeof( aryExpectedHeaderColor ),
                                   color.number, color.background, false );
   formatAnsiDisplayStateSequence( aryExpectedBodyColor, sizeof( aryExpectedBodyColor ),
                                   color.text, color.background, false );
   if ( strstr( aryPrintLog, aryExpectedHeaderColor ) == NULL )
   {
      fail_msg( "URL detection report header should use configured number/background colors; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, aryExpectedBodyColor ) == NULL )
   {
      fail_msg( "URL detection report body should use configured text/background colors; log was: %s", aryPrintLog );
   }

   resetLists();
}

static void emitUrlDetectionReport_WhenClickableUrlsDisabled_EmitsNoSummary( void **state )
{
   // Arrange
   (void)state;

   resetState();
   resetLists();
   flagsConfiguration.shouldEnableClickableUrls = 0;
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for disabled URL detection report test setup" );
   }

   beginUrlDetectionReport();
   filterUrl( "Read this https://example.dev/alpha" );

   // Act
   emitUrlDetectionReport();

   // Assert
   if ( strstr( aryPrintLog, "[Clickable URL(s) detected by BBS client]" ) != NULL )
   {
      fail_msg( "URL detection report should be suppressed when clickable URLs are disabled; log was: %s", aryPrintLog );
   }

   resetLists();
}

static void emitUrlDetectionReport_WhenScreenReaderModeEnabled_EmitsNoSummary( void **state )
{
   // Arrange
   (void)state;

   resetState();
   resetLists();
   flagsConfiguration.isScreenReaderModeEnabled = 1;
   urlQueue = newQueue( 1024, 5 );
   if ( urlQueue == NULL )
   {
      fail_msg( "newQueue failed for screen reader URL detection report test setup" );
   }

   beginUrlDetectionReport();
   filterUrl( "Read this https://example.dev/alpha" );

   // Act
   emitUrlDetectionReport();

   // Assert
   if ( strstr( aryPrintLog, "[Clickable URL(s) detected by BBS client]" ) != NULL )
   {
      fail_msg( "URL detection report should be suppressed when screen reader mode is enabled; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, "\033]8;;" ) != NULL )
   {
      fail_msg( "OSC-8 escapes should not be emitted in screen reader mode; log was: %s", aryPrintLog );
   }

   resetLists();
}

static void filterWhoList_WhenSavedFriendsRendered_UsesThemeColors( void **state )
{
   // Arrange
   char aryExpectedHeaderColor[32];
   char aryExpectedNameColor[32];
   char aryExpectedTimeColor[32];
   char aryExpectedInfoColor[32];
   char aryExpectedResetColor[32];

   (void)state;

   resetState();
   flagsConfiguration.shouldUseAnsi = 1;
   color.forum = 33;
   color.postFriendName = 196;
   color.postFriendDate = 220;
   color.postFriendText = 214;
   color.text = 231;
   savedWhoCount = 1;
   arySavedWhoNames[0][0] = 65;
   snprintf( (char *)arySavedWhoNames[0] + 1, sizeof( arySavedWhoNames[0] ) - 1, "%c%s",
             (char)0x80 | 'S', "tilgar" );
   snprintf( (char *)arySavedWhoInfo[0], sizeof( arySavedWhoInfo[0] ), "%s",
             "Color-themed friend" );

   // Act
   filterWhoList( 0 );

   // Assert
   formatAnsiForegroundSequence( aryExpectedHeaderColor, sizeof( aryExpectedHeaderColor ),
                                 color.forum );
   formatAnsiForegroundSequence( aryExpectedNameColor, sizeof( aryExpectedNameColor ),
                                 color.postFriendName );
   formatAnsiForegroundSequence( aryExpectedTimeColor, sizeof( aryExpectedTimeColor ),
                                 color.postFriendDate );
   formatAnsiForegroundSequence( aryExpectedInfoColor, sizeof( aryExpectedInfoColor ),
                                 color.postFriendText );
   formatAnsiForegroundSequence( aryExpectedResetColor, sizeof( aryExpectedResetColor ),
                                 color.text );
   if ( strstr( aryPrintLog, aryExpectedHeaderColor ) == NULL )
   {
      fail_msg( "saved friends-online header should use forum color; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, aryExpectedNameColor ) == NULL )
   {
      fail_msg( "saved friends-online entry should use friend name color; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, aryExpectedTimeColor ) == NULL )
   {
      fail_msg( "saved friends-online entry should use friend time color; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, aryExpectedInfoColor ) == NULL )
   {
      fail_msg( "saved friends-online entry should use friend info color; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, aryExpectedResetColor ) == NULL )
   {
      fail_msg( "saved friends-online output should restore text color afterward; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, "tilgar" ) == NULL )
   {
      fail_msg( "saved friends-online output should include the saved friend name; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, "Color-themed friend" ) == NULL )
   {
      fail_msg( "saved friends-online output should include the saved friend info; log was: %s", aryPrintLog );
   }
}

static void filterWhoList_WhenLiveFriendRendered_UsesThemeColors( void **state )
{
   // Arrange
   char aryExpectedNameColor[32];
   char aryExpectedTimeColor[32];
   char aryExpectedInfoColor[32];
   char aryExpectedResetColor[32];
   const unsigned char aryWhoBytes[] = { 66, 'S', 't', 'i', 'l', 'g', 'a', 'r' };
   size_t byteIndex;

   (void)state;

   resetState();
   resetLists();
   flagsConfiguration.shouldUseAnsi = 1;
   color.postFriendName = 196;
   color.postFriendDate = 220;
   color.postFriendText = 214;
   color.text = 231;
   whoListProgress = 1;
   whoList = slistCreate( 0, sortCompareVoid );
   if ( whoList == NULL )
   {
      fail_msg( "slistCreate failed while preparing whoList for filter tests" );
      return;
   }
   addFriend( "Stilgar", "Live themed friend" );

   // Act
   for ( byteIndex = 0; byteIndex < sizeof( aryWhoBytes ); byteIndex++ )
   {
      filterWhoList( aryWhoBytes[byteIndex] );
   }
   filterWhoList( 0 );
   filterWhoList( 0 );

   // Assert
   formatAnsiForegroundSequence( aryExpectedNameColor, sizeof( aryExpectedNameColor ),
                                 color.postFriendName );
   formatAnsiForegroundSequence( aryExpectedTimeColor, sizeof( aryExpectedTimeColor ),
                                 color.postFriendDate );
   formatAnsiForegroundSequence( aryExpectedInfoColor, sizeof( aryExpectedInfoColor ),
                                 color.postFriendText );
   formatAnsiForegroundSequence( aryExpectedResetColor, sizeof( aryExpectedResetColor ),
                                 color.text );
   if ( strstr( aryPrintLog, "Your friends online (new)" ) == NULL )
   {
      fail_msg( "live friends-online output should include the new-list header; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, aryExpectedNameColor ) == NULL )
   {
      fail_msg( "live friends-online entry should use friend name color; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, aryExpectedTimeColor ) == NULL )
   {
      fail_msg( "live friends-online entry should use friend time color; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, aryExpectedInfoColor ) == NULL )
   {
      fail_msg( "live friends-online entry should use friend info color; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, aryExpectedResetColor ) == NULL )
   {
      fail_msg( "live friends-online output should restore text color afterward; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, "Stilgar" ) == NULL )
   {
      fail_msg( "live friends-online output should include the live friend name; log was: %s", aryPrintLog );
   }
   if ( strstr( aryPrintLog, "Live themed friend" ) == NULL )
   {
      fail_msg( "live friends-online output should include the live friend info; log was: %s", aryPrintLog );
   }

   resetLists();
}

static void filterData_WhenAnsiColorMapsToBrightValue_EmitsFullAnsiSequence( void **state )
{
   // Arrange
   (void)state;

   resetState();
   color.background = 0;

   // Act
   filterData( '\033' );
   filterData( '[' );
   filterData( '3' );
   filterData( '6' );
   filterData( 'm' );

   // Assert
   if ( strstr( aryPrintLog, "\033[95m" ) == NULL )
   {
      fail_msg( "filterData should emit a full bright ANSI sequence for remapped colors; log was: %s", aryPrintLog );
   }
}

static void filterData_WhenLineCompletes_ForwardsServerLineToKeychainHandler( void **state )
{
   // Arrange
   const char *ptrLine;

   (void)state;

   resetState();
   ptrLine = "Wrong password...\n";

   // Act
   while ( *ptrLine != '\0' )
   {
      filterData( *ptrLine++ );
   }

   // Assert
   if ( strstr( aryLastKeychainServerLine, "Wrong password..." ) == NULL )
   {
      fail_msg( "filterData should forward completed lines to the keychain handler; got '%s'",
                aryLastKeychainServerLine );
   }
}

static void filterExpress_WhenAwayAndIncomingNewMessage_QueuesSender( void **state )
{
   // Arrange
   const char *ptrHeader;

   (void)state;

   resetState();
   resetLists();
   isAway = true;
   xlandQueue = newQueue( 21, 5 );
   enemyList = slistCreate( 0, sortCompareVoid );
   if ( xlandQueue == NULL || enemyList == NULL )
   {
      fail_msg( "failed to allocate xlandQueue/enemyList for filterExpress test" );
   }
   ptrHeader = "*** Message (#5) from Dr Strange at 3:07 PM on Feb 19, 2026 ***\r";

   // Act
   isExpressMessageInProgress = true;
   filterExpress( -1 );
   for ( ; *ptrHeader != '\0'; ++ptrHeader )
   {
      filterExpress( *ptrHeader );
   }
   isExpressMessageInProgress = false;
   filterExpress( -1 );

   // Assert
   if ( !shouldSendExpressMessage )
   {
      fail_msg( "new incoming X while away should set shouldSendExpressMessage=1; got %d", shouldSendExpressMessage );
   }
   if ( highestExpressMessageId != 5 )
   {
      fail_msg( "highestExpressMessageId should update to parsed ID 5; got %d", highestExpressMessageId );
   }
   if ( xlandQueue->itemCount != 1 || !isQueued( "Dr Strange", xlandQueue ) )
   {
      fail_msg( "sender should be queued exactly once for auto-reply flow" );
   }

   resetLists();
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( isAutomaticReply_WhenReplyPrefixPresent_ReturnsTrue ),
      cmocka_unit_test( replyCodeTransformExpress_WhenReplyCodePresent_RemovesReplyCodePrefix ),
      cmocka_unit_test( notReplyingTransformExpress_WhenHeaderContainsAt_InsertsNotReplyingLabel ),
      cmocka_unit_test( filterUrl_WhenDuplicateSeen_QueuesOnlyOnce ),
      cmocka_unit_test( filterUrl_WhenQueueIsFull_EvictsOldestUrl ),
      cmocka_unit_test( filterUrl_WhenHttpsAndTrailingPunctuationPresent_ExtractsCleanUrl ),
      cmocka_unit_test( filterUrl_WhenInvalidWwwBoundaryPrecedesHttps_QueuesLaterHttpsUrl ),
      cmocka_unit_test( filterUrl_WhenWwwUrlPresent_QueuesUrlWithoutTldList ),
      cmocka_unit_test( filterUrl_WhenHttpOrFtpPresent_DoesNotQueueUnsupportedSchemes ),
      cmocka_unit_test( filterUrl_WhenHttpsSchemeWrapsAfterSlashes_CombinesIntoSingleUrl ),
      cmocka_unit_test( filterUrl_WhenHttpsSchemeWrapsBeforeSlashes_CombinesIntoSingleUrl ),
      cmocka_unit_test( filterUrl_WhenHttpsWrapsAcrossLines_CombinesIntoSingleUrl ),
      cmocka_unit_test( filterUrl_WhenHttpsWrapsAcrossLinesWithShortFirstFragment_CombinesIntoSingleUrl ),
      cmocka_unit_test( filterUrl_WhenHttpsStartsLateOnFullWidthLine_CombinesIntoSingleUrl ),
      cmocka_unit_test( filterUrl_WhenIncompleteHttpsSchemeDoesNotContinue_QueuesNoUrl ),
      cmocka_unit_test( printWithOsc8Links_WhenTextContainsHttpsUrl_EmitsHyperlinkEscapes ),
      cmocka_unit_test( printWithOsc8Links_WhenTextContainsWwwUrl_UsesHttpsTarget ),
      cmocka_unit_test( printWithOsc8Links_WhenTextContainsHttpsWwwUrl_EmitsSingleHyperlink ),
      cmocka_unit_test( printWithOsc8Links_WhenInvalidWwwBoundaryPrecedesHttps_EmitsLaterHttpsHyperlink ),
      cmocka_unit_test( printWithOsc8Links_WhenHttpsUrlExceedsPrintfBuffer_StillClosesHyperlink ),
      cmocka_unit_test( printWithOsc8Links_WhenGhosttyAndTextEqualsHttpsTarget_StillEmitsOsc8Hyperlink ),
      cmocka_unit_test( printWithOsc8Links_WhenClickableUrlsDisabled_PrintsPlainText ),
      cmocka_unit_test( printWithOsc8Links_WhenScreenReaderModeEnabled_PrintsPlainText ),
      cmocka_unit_test( emitUrlDetectionReport_WhenUrlsCollected_PrintsClickableSummary ),
      cmocka_unit_test( emitUrlDetectionReport_WhenWrappedUrlStillPending_FlushesBeforePrintingSummary ),
      cmocka_unit_test( beginUrlDetectionReport_WhenPriorWrappedUrlWasPending_ClearsPendingState ),
      cmocka_unit_test( emitUrlDetectionReport_WhenAnsiEnabled_UsesConfiguredColorState ),
      cmocka_unit_test( emitUrlDetectionReport_WhenClickableUrlsDisabled_EmitsNoSummary ),
      cmocka_unit_test( emitUrlDetectionReport_WhenScreenReaderModeEnabled_EmitsNoSummary ),
      cmocka_unit_test( filterWhoList_WhenSavedFriendsRendered_UsesThemeColors ),
      cmocka_unit_test( filterWhoList_WhenLiveFriendRendered_UsesThemeColors ),
      cmocka_unit_test( filterData_WhenAnsiColorMapsToBrightValue_EmitsFullAnsiSequence ),
      cmocka_unit_test( filterData_WhenLineCompletes_ForwardsServerLineToKeychainHandler ),
      cmocka_unit_test( filterExpress_WhenAwayAndIncomingNewMessage_QueuesSender ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
