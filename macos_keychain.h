/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MACOS_KEYCHAIN_H_INCLUDED
#define MACOS_KEYCHAIN_H_INCLUDED

#include "defs.h"

typedef bool ( *KeychainPasswordStoreFunction )( const char *ptrHost,
                                                 const char *ptrUser,
                                                 const char *ptrPassword );

/// @brief Clear per-session keychain state for the current BBS connection.
void clearKeychainSessionState( void );

/// @brief Delete the saved password for the current BBS host and known user.
///
/// @return `true` if the saved password was deleted, otherwise `false`.
bool tryDeleteSavedKeychainPasswordForCurrentBbs( void );

/// @brief Delete a stored password for a specific host and user.
///
/// @param ptrHost BBS host name.
/// @param ptrUser BBS user name.
/// @return `true` if the stored password was deleted, otherwise `false`.
bool tryDeleteKeychainPassword( const char *ptrHost, const char *ptrUser );

/// @brief Report whether the current BBS host and user identify one saved password.
///
/// @return `true` when the client knows which saved password belongs to this BBS.
bool hasSavedKeychainPasswordContextForCurrentBbs( void );

/// @brief Read a stored password for a specific host and user.
///
/// @param ptrHost BBS host name.
/// @param ptrUser BBS user name.
/// @param ptrPassword Destination buffer for the password.
/// @param passwordSize Size of `ptrPassword`.
/// @return `true` if a password was loaded, otherwise `false`.
bool tryGetKeychainPassword( const char *ptrHost, const char *ptrUser,
                             char *ptrPassword, size_t passwordSize );

/// @brief Parse a successful-login server line and extract the BBS user.
///
/// @param ptrLine Completed server output line.
/// @param ptrUser Destination buffer for the parsed user name.
/// @param userSize Size of `ptrUser`.
/// @return `true` if the line is a recognized login-success line.
bool parseBbsUserFromLoginSuccessLine( const char *ptrLine, char *ptrUser,
                                       size_t userSize );

/// @brief Record a hidden password field that was entered manually.
///
/// @param ptrPassword Hidden input captured from the user.
void handleKeychainHiddenInput( const char *ptrPassword );

/// @brief Inspect a completed server output line for keychain events.
///
/// @param ptrLine Completed server output line.
void handleKeychainServerLine( const char *ptrLine );

/// @brief Inspect the current buffered server text for keychain failure events.
void processBufferedKeychainServerText( void );

/// @brief Record the current BBS user name for host-scoped lookups.
///
/// @param ptrUser BBS user name.
void recordCurrentBbsUser( const char *ptrUser );

/// @brief Override the keychain password store callback for internal tests.
///
/// @param ptrStoreFunction Replacement callback, or `NULL` to restore the default backend.
void setKeychainPasswordStoreFunctionForTesting(
   KeychainPasswordStoreFunction ptrStoreFunction );

/// @brief Mark the current login attempt as having used a keychain-loaded password in tests.
///
/// @param isPendingLookup `true` to simulate a keychain autofill attempt, otherwise `false`.
void setPendingKeychainLookupForTesting( bool isPendingLookup );

/// @brief Store a new password for a specific host and user.
///
/// @param ptrHost BBS host name.
/// @param ptrUser BBS user name.
/// @param ptrPassword Password to store.
/// @return `true` if the password was stored, otherwise `false`.
bool trySetKeychainPassword( const char *ptrHost, const char *ptrUser,
                             const char *ptrPassword );

/// @brief Try to auto-fill the current hidden password prompt from keychain.
///
/// @param ptrPassword Destination buffer for the password.
/// @param passwordSize Size of `ptrPassword`.
/// @return `true` if a password was loaded for the active prompt.
bool tryGetKeychainPasswordForPrompt( char *ptrPassword, size_t passwordSize );

/// @brief Try to store a pending manual-login password after a confirmed login success.
///
/// @param ptrHost BBS host name.
/// @param ptrUser Parsed BBS user name from the successful login line.
/// @param ptrStoreFunction Backend callback used to save the password.
/// @return `true` if the password store callback succeeded, otherwise `false`.
bool tryStorePendingLoginPassword( const char *ptrHost, const char *ptrUser,
                                   KeychainPasswordStoreFunction ptrStoreFunction );

/// @brief Insert or update a password for a specific host and user.
///
/// @param ptrHost BBS host name.
/// @param ptrUser BBS user name.
/// @param ptrPassword Password to store.
/// @return `true` if the password was saved, otherwise `false`.
bool tryUpsertKeychainPassword( const char *ptrHost, const char *ptrUser,
                                const char *ptrPassword );

#endif // MACOS_KEYCHAIN_H_INCLUDED
