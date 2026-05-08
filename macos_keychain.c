/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "config_globals.h"
#include "filter_globals.h"
#include "macos_keychain.h"
#include "utility.h"

#ifdef ENABLE_KEYCHAIN
#include <Security/Security.h>
#endif

typedef enum
{
   KEYCHAIN_PASSWORD_PROMPT_NONE = 0,
   KEYCHAIN_PASSWORD_PROMPT_CHANGE_CONFIRM,
   KEYCHAIN_PASSWORD_PROMPT_CHANGE_CURRENT,
   KEYCHAIN_PASSWORD_PROMPT_CHANGE_NEW,
   KEYCHAIN_PASSWORD_PROMPT_LOGIN
} KeychainPasswordPromptType;

static char aryCurrentBbsUser[41];
static char aryPendingChangePassword[80];
static char aryPendingLoginPassword[80];
static bool isLookupSuppressed;
static bool isPendingLookupFromKeychain;
static bool shouldStorePendingChangePassword;
static bool shouldStorePendingLoginPassword;
static KeychainPasswordPromptType keychainPasswordPromptType;
static KeychainPasswordStoreFunction ptrKeychainPasswordStoreFunction;

#ifdef ENABLE_KEYCHAIN
static bool buildKeychainAccountName( const char *ptrHost, const char *ptrUser,
                                      char *ptrAccountName, size_t accountNameSize );
static OSStatus findKeychainItem( const char *ptrHost, const char *ptrUser,
                                  SecKeychainItemRef *ptrItemRef,
                                  void **ptrPasswordData,
                                  UInt32 *ptrPasswordLength );
#endif
static void clearPendingChangeKeychainPassword( void );
static void clearPendingLoginKeychainPassword( void );
static void clearStoredSecret( char *ptrBuffer, size_t bufferSize );
static const char *findCurrentBbsHost( void );
static const char *findCurrentBbsUser( void );
static bool hasCurrentBbsUser( void );
static bool doesContainDelimitedText( const char *ptrText, const char *ptrExpected );
static bool isChangePasswordSuccessLine( const char *ptrLine );
static bool isCurrentPasswordFailureLine( const char *ptrLine );
static bool isKeychainRuntimeEnabled( void );
static bool isLoginPasswordFailureLine( const char *ptrLine );
static bool lineEndsWith( const char *ptrText, const char *ptrSuffix );
static bool isNewPasswordMismatchLine( const char *ptrLine );
static bool isSkippableBbsUser( const char *ptrUser );
static bool parseBbsUserFromLegacyLoginBanner( const char *ptrLine, char *ptrUser,
                                               size_t userSize );
static bool parseBbsUserFromWelcomeLoginBanner( const char *ptrLine, char *ptrUser,
                                                size_t userSize );
static KeychainPasswordPromptType parseKeychainPasswordPromptType( const char *ptrLine );
static KeychainPasswordStoreFunction resolveKeychainPasswordStoreFunction( void );
static const char *skipLinePrefix( const char *ptrLine );
static const char *skipLeadingAnsiSequence( const char *ptrLine );
static bool tryStorePendingChangePassword( const char *ptrHost, const char *ptrUser,
                                           KeychainPasswordStoreFunction ptrStoreFunction );

#ifdef ENABLE_KEYCHAIN
static bool buildKeychainAccountName( const char *ptrHost, const char *ptrUser,
                                      char *ptrAccountName, size_t accountNameSize )
{
   int charsWritten;

   if ( ptrHost == NULL || ptrUser == NULL || ptrAccountName == NULL ||
        accountNameSize == 0 || *ptrHost == '\0' || *ptrUser == '\0' )
   {
      return false;
   }

   charsWritten = snprintf( ptrAccountName, accountNameSize, "%s:%s",
                            ptrHost, ptrUser );
   return charsWritten > 0 &&
          (size_t)charsWritten < accountNameSize;
}

static OSStatus findKeychainItem( const char *ptrHost, const char *ptrUser,
                                  SecKeychainItemRef *ptrItemRef,
                                  void **ptrPasswordData,
                                  UInt32 *ptrPasswordLength )
{
   char aryAccountName[160];

   if ( !buildKeychainAccountName( ptrHost, ptrUser, aryAccountName,
                                   sizeof( aryAccountName ) ) )
   {
      return errSecParam;
   }

#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
   return SecKeychainFindGenericPassword(
      NULL,
      (UInt32)strlen( "BbcClient" ),
      "BbcClient",
      (UInt32)strlen( aryAccountName ),
      aryAccountName,
      ptrPasswordLength,
      ptrPasswordData,
      ptrItemRef );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
}
#endif

void clearKeychainSessionState( void )
{
   *aryCurrentBbsUser = '\0';
   clearPendingChangeKeychainPassword();
   clearPendingLoginKeychainPassword();
   isLookupSuppressed = false;
   keychainPasswordPromptType = KEYCHAIN_PASSWORD_PROMPT_NONE;
}

static void clearPendingChangeKeychainPassword( void )
{
   clearStoredSecret( aryPendingChangePassword, sizeof( aryPendingChangePassword ) );
   isPendingLookupFromKeychain = false;
   shouldStorePendingChangePassword = false;
}

static void clearPendingLoginKeychainPassword( void )
{
   clearStoredSecret( aryPendingLoginPassword, sizeof( aryPendingLoginPassword ) );
   isPendingLookupFromKeychain = false;
   shouldStorePendingLoginPassword = false;
}

static void clearStoredSecret( char *ptrBuffer, size_t bufferSize )
{
   volatile char *ptrCursor;
   size_t bufferIndex;

   if ( ptrBuffer == NULL )
   {
      return;
   }

   ptrCursor = ptrBuffer;
   for ( bufferIndex = 0; bufferIndex < bufferSize; bufferIndex++ )
   {
      ptrCursor[bufferIndex] = '\0';
   }
}

void processBufferedKeychainServerText( void )
{
   if ( !isPendingLookupFromKeychain )
   {
      return;
   }

   if ( isLoginPasswordFailureLine( aryFilterLine ) )
   {
      isLookupSuppressed = true;
      clearPendingLoginKeychainPassword();
      return;
   }

   if ( isCurrentPasswordFailureLine( aryFilterLine ) )
   {
      isLookupSuppressed = true;
      clearPendingChangeKeychainPassword();
   }
}

static KeychainPasswordStoreFunction resolveKeychainPasswordStoreFunction( void )
{
   if ( ptrKeychainPasswordStoreFunction != NULL )
   {
      return ptrKeychainPasswordStoreFunction;
   }

   return upsertKeychainPassword;
}

static bool tryStorePendingChangePassword( const char *ptrHost, const char *ptrUser,
                                           KeychainPasswordStoreFunction ptrStoreFunction )
{
   if ( !isKeychainRuntimeEnabled() ||
        ptrHost == NULL ||
        ptrUser == NULL ||
        ptrStoreFunction == NULL ||
        !shouldStorePendingChangePassword ||
        *aryPendingChangePassword == '\0' ||
        isSkippableBbsUser( ptrUser ) )
   {
      return false;
   }

   return ptrStoreFunction( ptrHost, ptrUser, aryPendingChangePassword );
}

bool deleteKeychainPassword( const char *ptrHost, const char *ptrUser )
{
#ifdef ENABLE_KEYCHAIN
   OSStatus status;
   SecKeychainItemRef itemRef;

   itemRef = NULL;
   status = findKeychainItem( ptrHost, ptrUser, &itemRef, NULL, NULL );
   if ( status != errSecSuccess || itemRef == NULL )
   {
      return false;
   }

#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
   status = SecKeychainItemDelete( itemRef );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
   CFRelease( itemRef );
   return status == errSecSuccess;
#else
   (void)ptrHost;
   (void)ptrUser;
   return false;
#endif
}

bool deleteSavedKeychainPasswordForCurrentBbs( void )
{
   const char *ptrHost;
   const char *ptrUser;

   ptrHost = findCurrentBbsHost();
   ptrUser = findCurrentBbsUser();
   if ( ptrHost == NULL || ptrUser == NULL )
   {
      return false;
   }

   return deleteKeychainPassword( ptrHost, ptrUser );
}

static const char *findCurrentBbsHost( void )
{
   if ( *aryCommandLineHost != '\0' )
   {
      return aryCommandLineHost;
   }
   if ( *aryBbsHost != '\0' )
   {
      return aryBbsHost;
   }
   return NULL;
}

static const char *findCurrentBbsUser( void )
{
   if ( hasCurrentBbsUser() )
   {
      return aryCurrentBbsUser;
   }
   if ( *aryAutoName != '\0' && strcmp( aryAutoName, "NONE" ) != 0 &&
        !isSkippableBbsUser( aryAutoName ) )
   {
      return aryAutoName;
   }
   return NULL;
}

bool getKeychainPassword( const char *ptrHost, const char *ptrUser,
                          char *ptrPassword, size_t passwordSize )
{
#ifdef ENABLE_KEYCHAIN
   void *ptrPasswordData;
   OSStatus status;
   UInt32 keychainPasswordLength;
   SecKeychainItemRef itemRef;

   if ( ptrPassword == NULL || passwordSize == 0 )
   {
      return false;
   }

   ptrPasswordData = NULL;
   itemRef = NULL;
   keychainPasswordLength = 0;
   status = findKeychainItem( ptrHost, ptrUser, &itemRef, &ptrPasswordData,
                              &keychainPasswordLength );
   if ( status != errSecSuccess || ptrPasswordData == NULL )
   {
      return false;
   }

   if ( keychainPasswordLength == 0 ||
        (size_t)keychainPasswordLength >= passwordSize )
   {
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
      SecKeychainItemFreeContent( NULL, ptrPasswordData );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
      if ( itemRef != NULL )
      {
         CFRelease( itemRef );
      }
      return false;
   }

   memcpy( ptrPassword, ptrPasswordData, (size_t)keychainPasswordLength );
   ptrPassword[keychainPasswordLength] = '\0';
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
   SecKeychainItemFreeContent( NULL, ptrPasswordData );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
   if ( itemRef != NULL )
   {
      CFRelease( itemRef );
   }
   return true;
#else
   (void)ptrHost;
   (void)ptrPassword;
   (void)passwordSize;
   (void)ptrUser;
   return false;
#endif
}

void setKeychainPasswordStoreFunctionForTesting(
   KeychainPasswordStoreFunction ptrStoreFunction )
{
   ptrKeychainPasswordStoreFunction = ptrStoreFunction;
}

void setPendingKeychainLookupForTesting( bool isPendingLookup )
{
   isPendingLookupFromKeychain = isPendingLookup;
}

bool tryStorePendingLoginPassword( const char *ptrHost, const char *ptrUser,
                                   KeychainPasswordStoreFunction ptrStoreFunction )
{
   if ( !isKeychainRuntimeEnabled() ||
        ptrHost == NULL ||
        ptrUser == NULL ||
        ptrStoreFunction == NULL ||
        !shouldStorePendingLoginPassword ||
        *aryPendingLoginPassword == '\0' ||
        isSkippableBbsUser( ptrUser ) )
   {
      return false;
   }

   return ptrStoreFunction( ptrHost, ptrUser, aryPendingLoginPassword );
}

void handleKeychainHiddenInput( const char *ptrPassword )
{
   size_t passwordLength;

   if ( ptrPassword == NULL )
   {
      return;
   }

   passwordLength = strlen( ptrPassword );
   switch ( keychainPasswordPromptType )
   {
      case KEYCHAIN_PASSWORD_PROMPT_CHANGE_CONFIRM:
         shouldStorePendingChangePassword =
            *aryPendingChangePassword != '\0' &&
            strcmp( aryPendingChangePassword, ptrPassword ) == 0;
         if ( !shouldStorePendingChangePassword )
         {
            clearPendingChangeKeychainPassword();
         }
         return;

      case KEYCHAIN_PASSWORD_PROMPT_CHANGE_NEW:
         clearPendingChangeKeychainPassword();
         if ( passwordLength > 0 &&
              passwordLength < sizeof( aryPendingChangePassword ) )
         {
            snprintf( aryPendingChangePassword, sizeof( aryPendingChangePassword ),
                      "%s", ptrPassword );
         }
         return;

      case KEYCHAIN_PASSWORD_PROMPT_LOGIN:
         if ( !isPendingLookupFromKeychain )
         {
            clearPendingLoginKeychainPassword();
            if ( passwordLength > 0 &&
                 passwordLength < sizeof( aryPendingLoginPassword ) )
            {
               snprintf( aryPendingLoginPassword, sizeof( aryPendingLoginPassword ),
                         "%s", ptrPassword );
               shouldStorePendingLoginPassword = true;
            }
         }
         return;

      case KEYCHAIN_PASSWORD_PROMPT_CHANGE_CURRENT:
      case KEYCHAIN_PASSWORD_PROMPT_NONE:
         return;
   }
}

void handleKeychainServerLine( const char *ptrLine )
{
   char aryBbsUser[41];
   const char *ptrHost;

   if ( ptrLine == NULL )
   {
      return;
   }

   if ( isLoginPasswordFailureLine( ptrLine ) )
   {
      if ( isPendingLookupFromKeychain )
      {
         isLookupSuppressed = true;
      }
      clearPendingLoginKeychainPassword();
      keychainPasswordPromptType = KEYCHAIN_PASSWORD_PROMPT_NONE;
      return;
   }

   if ( isCurrentPasswordFailureLine( ptrLine ) )
   {
      if ( isPendingLookupFromKeychain )
      {
         isLookupSuppressed = true;
      }
      clearPendingChangeKeychainPassword();
      keychainPasswordPromptType = KEYCHAIN_PASSWORD_PROMPT_NONE;
      return;
   }

   if ( isNewPasswordMismatchLine( ptrLine ) )
   {
      clearPendingChangeKeychainPassword();
      keychainPasswordPromptType = KEYCHAIN_PASSWORD_PROMPT_NONE;
      return;
   }

   if ( isChangePasswordSuccessLine( ptrLine ) )
   {
      ptrHost = findCurrentBbsHost();
      (void)tryStorePendingChangePassword(
         ptrHost,
         aryCurrentBbsUser,
         resolveKeychainPasswordStoreFunction() );
      clearPendingChangeKeychainPassword();
      isLookupSuppressed = false;
      keychainPasswordPromptType = KEYCHAIN_PASSWORD_PROMPT_NONE;
      return;
   }

   if ( parseBbsUserFromLoginSuccessLine( ptrLine, aryBbsUser,
                                          sizeof( aryBbsUser ) ) )
   {
      recordCurrentBbsUser( aryBbsUser );
      ptrHost = findCurrentBbsHost();
      (void)tryStorePendingLoginPassword(
         ptrHost,
         aryBbsUser,
         resolveKeychainPasswordStoreFunction() );
      clearPendingLoginKeychainPassword();
      isLookupSuppressed = false;
      keychainPasswordPromptType = KEYCHAIN_PASSWORD_PROMPT_NONE;
   }
}

static bool hasCurrentBbsUser( void )
{
   return *aryCurrentBbsUser != '\0' &&
          !isSkippableBbsUser( aryCurrentBbsUser );
}

bool hasSavedKeychainPasswordContextForCurrentBbs( void )
{
   return findCurrentBbsHost() != NULL &&
          findCurrentBbsUser() != NULL;
}

static bool isChangePasswordSuccessLine( const char *ptrLine )
{
   return doesContainDelimitedText( ptrLine, "Password changed." ) ||
          doesContainDelimitedText( ptrLine, "So be it." );
}

static bool isCurrentPasswordFailureLine( const char *ptrLine )
{
   return doesContainDelimitedText( ptrLine, "Password unchanged." );
}

static bool isKeychainRuntimeEnabled( void )
{
   return flagsConfiguration.shouldUseKeychain != 0;
}

static bool isLoginPasswordFailureLine( const char *ptrLine )
{
   return doesContainDelimitedText( ptrLine, "Incorrect login." ) ||
          doesContainDelimitedText( ptrLine, "Wrong password..." );
}

static bool doesContainDelimitedText( const char *ptrText, const char *ptrExpected )
{
   size_t expectedLength;

   if ( ptrText == NULL || ptrExpected == NULL )
   {
      return false;
   }

   expectedLength = strlen( ptrExpected );
   while ( *ptrText != '\0' )
   {
      const char *ptrSegmentEnd;
      const char *ptrSegmentStart;
      size_t segmentLength;

      ptrSegmentStart = skipLinePrefix( ptrText );
      ptrSegmentEnd = ptrSegmentStart;
      while ( *ptrSegmentEnd != '\0' && *ptrSegmentEnd != '\r' &&
              *ptrSegmentEnd != '\n' )
      {
         ptrSegmentEnd++;
      }

      segmentLength = (size_t)( ptrSegmentEnd - ptrSegmentStart );
      if ( segmentLength == expectedLength &&
           strncmp( ptrSegmentStart, ptrExpected, expectedLength ) == 0 )
      {
         return true;
      }

      ptrText = ptrSegmentEnd;
      while ( *ptrText == '\r' || *ptrText == '\n' )
      {
         ptrText++;
      }
   }

   return false;
}

static bool lineEndsWith( const char *ptrText, const char *ptrSuffix )
{
   size_t suffixLength;
   size_t textLength;

   if ( ptrText == NULL || ptrSuffix == NULL )
   {
      return false;
   }

   textLength = strlen( ptrText );
   suffixLength = strlen( ptrSuffix );
   if ( textLength < suffixLength )
   {
      return false;
   }

   return strcmp( ptrText + textLength - suffixLength, ptrSuffix ) == 0;
}

static bool isNewPasswordMismatchLine( const char *ptrLine )
{
   return doesContainDelimitedText(
             ptrLine,
             "Your passwords didn't match.  Please try again." ) ||
          doesContainDelimitedText(
             ptrLine,
             "The passwords you typed didn't match.  Please try again." );
}

static bool isSkippableBbsUser( const char *ptrUser )
{
   return ptrUser == NULL || *ptrUser == '\0' ||
          strcmp( ptrUser, "Guest" ) == 0 ||
          strcmp( ptrUser, "NONE" ) == 0;
}

static bool parseBbsUserFromLegacyLoginBanner( const char *ptrLine, char *ptrUser,
                                               size_t userSize )
{
   size_t userLength;
   const char *ptrStart;
   const char *ptrStop;

   if ( ptrLine == NULL || ptrUser == NULL || userSize == 0 )
   {
      return false;
   }

   ptrStart = skipLinePrefix( ptrLine );
   if ( strncmp( ptrStart, "User ", 5 ) != 0 )
   {
      return false;
   }

   ptrStart += 5;
   ptrStop = strstr( ptrStart, " (#" );
   if ( ptrStop == NULL )
   {
      return false;
   }

   userLength = (size_t)( ptrStop - ptrStart );
   if ( userLength == 0 || userLength >= userSize )
   {
      return false;
   }

   memcpy( ptrUser, ptrStart, userLength );
   ptrUser[userLength] = '\0';
   return true;
}

static bool parseBbsUserFromWelcomeLoginBanner( const char *ptrLine, char *ptrUser,
                                                size_t userSize )
{
   size_t userLength;
   const char *ptrCursor;
   const char *ptrStart;
   const char *ptrStop;
   const char *ptrUserStart;

   if ( ptrLine == NULL || ptrUser == NULL || userSize == 0 )
   {
      return false;
   }

   ptrStart = skipLinePrefix( ptrLine );
   if ( strncmp( ptrStart, "Welcome to ", 11 ) != 0 )
   {
      return false;
   }

   ptrUserStart = strstr( ptrStart, ", " );
   if ( ptrUserStart == NULL )
   {
      return false;
   }

   ptrUserStart += 2;
   ptrStop = strrchr( ptrUserStart, '!' );
   if ( ptrStop == NULL )
   {
      return false;
   }

   ptrCursor = ptrStop + 1;
   while ( *ptrCursor == '\r' )
   {
      ptrCursor++;
   }
   if ( *ptrCursor != '\0' )
   {
      return false;
   }

   userLength = (size_t)( ptrStop - ptrUserStart );
   if ( userLength == 0 || userLength >= userSize )
   {
      return false;
   }

   memcpy( ptrUser, ptrUserStart, userLength );
   ptrUser[userLength] = '\0';
   return true;
}

bool parseBbsUserFromLoginSuccessLine( const char *ptrLine, char *ptrUser,
                                       size_t userSize )
{
   if ( parseBbsUserFromLegacyLoginBanner( ptrLine, ptrUser, userSize ) )
   {
      return true;
   }

   return parseBbsUserFromWelcomeLoginBanner( ptrLine, ptrUser, userSize );
}

static KeychainPasswordPromptType parseKeychainPasswordPromptType( const char *ptrLine )
{
   const char *ptrText;

   ptrText = skipLinePrefix( ptrLine );
   if ( lineEndsWith( ptrText, "Old Password: " ) )
   {
      return KEYCHAIN_PASSWORD_PROMPT_CHANGE_CURRENT;
   }
   if ( lineEndsWith( ptrText, "New Password: " ) )
   {
      return KEYCHAIN_PASSWORD_PROMPT_CHANGE_NEW;
   }
   if ( lineEndsWith( ptrText, "Again for verification: " ) )
   {
      return KEYCHAIN_PASSWORD_PROMPT_CHANGE_CONFIRM;
   }
   if ( lineEndsWith( ptrText, "Please enter a password: " ) )
   {
      return KEYCHAIN_PASSWORD_PROMPT_CHANGE_NEW;
   }
   if ( lineEndsWith( ptrText, "Please enter it again: " ) )
   {
      return KEYCHAIN_PASSWORD_PROMPT_CHANGE_CONFIRM;
   }
   if ( lineEndsWith( ptrText, "Please enter your CURRENT password: " ) )
   {
      return KEYCHAIN_PASSWORD_PROMPT_CHANGE_CURRENT;
   }
   if ( lineEndsWith( ptrText, "Please enter your password: " ) )
   {
      return KEYCHAIN_PASSWORD_PROMPT_LOGIN;
   }
   if ( lineEndsWith( ptrText, "Name: Password: " ) )
   {
      return KEYCHAIN_PASSWORD_PROMPT_LOGIN;
   }
   if ( lineEndsWith( ptrText, "Password: " ) )
   {
      return KEYCHAIN_PASSWORD_PROMPT_LOGIN;
   }

   return KEYCHAIN_PASSWORD_PROMPT_NONE;
}

void recordCurrentBbsUser( const char *ptrUser )
{
   if ( ptrUser == NULL || isSkippableBbsUser( ptrUser ) )
   {
      *aryCurrentBbsUser = '\0';
      return;
   }

   snprintf( aryCurrentBbsUser, sizeof( aryCurrentBbsUser ), "%s", ptrUser );
}

bool setKeychainPassword( const char *ptrHost, const char *ptrUser,
                          const char *ptrPassword )
{
#ifdef ENABLE_KEYCHAIN
   char aryAccountName[160];
   OSStatus status;

   if ( ptrPassword == NULL ||
        !buildKeychainAccountName( ptrHost, ptrUser, aryAccountName,
                                   sizeof( aryAccountName ) ) )
   {
      return false;
   }

#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
   status = SecKeychainAddGenericPassword(
      NULL,
      (UInt32)strlen( "BbcClient" ),
      "BbcClient",
      (UInt32)strlen( aryAccountName ),
      aryAccountName,
      (UInt32)strlen( ptrPassword ),
      ptrPassword,
      NULL );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
   return status == errSecSuccess;
#else
   (void)ptrHost;
   (void)ptrPassword;
   (void)ptrUser;
   return false;
#endif
}

static const char *skipLinePrefix( const char *ptrLine )
{
   while ( ptrLine != NULL )
   {
      while ( *ptrLine == '\r' || *ptrLine == '\n' )
      {
         ptrLine++;
      }
      if ( *ptrLine != '\033' )
      {
         break;
      }
      ptrLine = skipLeadingAnsiSequence( ptrLine );
   }
   return ptrLine == NULL ? "" : ptrLine;
}

static const char *skipLeadingAnsiSequence( const char *ptrLine )
{
   if ( ptrLine == NULL || ptrLine[0] != '\033' || ptrLine[1] != '[' )
   {
      return ptrLine;
   }

   ptrLine += 2;
   while ( *ptrLine != '\0' &&
           ( ( *ptrLine >= '0' && *ptrLine <= '9' ) ||
             *ptrLine == ';' || *ptrLine == '?' ) )
   {
      ptrLine++;
   }
   if ( *ptrLine != '\0' )
   {
      ptrLine++;
   }
   return ptrLine;
}

bool tryGetKeychainPasswordForPrompt( char *ptrPassword, size_t passwordSize )
{
   const char *ptrHost;

   processBufferedKeychainServerText();
   keychainPasswordPromptType = parseKeychainPasswordPromptType( aryFilterLine );
   isPendingLookupFromKeychain = false;

   if ( ptrPassword == NULL || passwordSize == 0 ||
        !isKeychainRuntimeEnabled() ||
        !hasCurrentBbsUser() ||
        isLookupSuppressed )
   {
      return false;
   }

   if ( keychainPasswordPromptType != KEYCHAIN_PASSWORD_PROMPT_CHANGE_CURRENT &&
        keychainPasswordPromptType != KEYCHAIN_PASSWORD_PROMPT_LOGIN )
   {
      return false;
   }

   ptrHost = findCurrentBbsHost();
   if ( ptrHost == NULL )
   {
      return false;
   }

   if ( !getKeychainPassword( ptrHost, aryCurrentBbsUser, ptrPassword,
                              passwordSize ) )
   {
      return false;
   }

   isPendingLookupFromKeychain = true;
   isLookupSuppressed = true;
   return true;
}

bool upsertKeychainPassword( const char *ptrHost, const char *ptrUser,
                             const char *ptrPassword )
{
#ifdef ENABLE_KEYCHAIN
   OSStatus status;
   SecKeychainItemRef itemRef;

   if ( ptrPassword == NULL )
   {
      return false;
   }

   itemRef = NULL;
   status = findKeychainItem( ptrHost, ptrUser, &itemRef, NULL, NULL );
   if ( status == errSecItemNotFound )
   {
      return setKeychainPassword( ptrHost, ptrUser, ptrPassword );
   }
   if ( status != errSecSuccess || itemRef == NULL )
   {
      return false;
   }

#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
   status = SecKeychainItemModifyAttributesAndData(
      itemRef,
      NULL,
      (UInt32)strlen( ptrPassword ),
      ptrPassword );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
   CFRelease( itemRef );

   if ( status == errSecDuplicateItem )
   {
      return setKeychainPassword( ptrHost, ptrUser, ptrPassword );
   }

   return status == errSecSuccess;
#else
   (void)ptrHost;
   (void)ptrPassword;
   (void)ptrUser;
   return false;
#endif
}
