/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This header exposes the small TOML parsing helpers used by config loading.
 */
#ifndef CONFIG_TOML_H_INCLUDED
#define CONFIG_TOML_H_INCLUDED

#include "defs.h"

#define CONFIG_TOML_MAX_NAME_LENGTH 64
#define CONFIG_TOML_MAX_VALUE_LENGTH 256

/// @brief Trim leading and trailing ASCII whitespace in place.
///
/// @param ptrText Mutable string buffer to normalize.
///
/// @return Pointer to the trimmed view inside `ptrText`.
char *trimTomlWhitespace( char *ptrText );

/// @brief Split a TOML assignment line into key and value text.
///
/// @param ptrLine Raw line content.
/// @param aryKeyName Destination for the decoded key name.
/// @param keyNameSize Capacity of `aryKeyName`.
/// @param aryValue Destination for the trimmed value text.
/// @param valueSize Capacity of `aryValue`.
///
/// @return `true` on success, otherwise `false`.
bool tryParseTomlKeyValueLine( const char *ptrLine,
                               char *aryKeyName,
                               size_t keyNameSize,
                               char *aryValue,
                               size_t valueSize );

/// @brief Decode a TOML double-quoted string.
///
/// @param ptrValue Raw value text to decode.
/// @param aryOutput Destination buffer for decoded text.
/// @param outputSize Capacity of `aryOutput`.
///
/// @return `true` on success, otherwise `false`.
bool tryParseTomlQuotedString( const char *ptrValue,
                               char *aryOutput,
                               size_t outputSize );

/// @brief Decode a TOML section header.
///
/// @param ptrLine Raw line content.
/// @param arySectionName Destination for the decoded section name.
/// @param sectionNameSize Capacity of `arySectionName`.
///
/// @return `true` on success, otherwise `false`.
bool tryParseTomlSectionName( const char *ptrLine,
                              char *arySectionName,
                              size_t sectionNameSize );

/// @brief Parse one TOML double-quoted string token from the start of a buffer.
///
/// @param ptrText Text that begins with a TOML string token.
/// @param ptrConsumedLength Receives the number of input characters consumed.
/// @param aryOutput Destination buffer for the decoded string.
/// @param outputSize Capacity of `aryOutput`.
///
/// @return `true` on success, otherwise `false`.
bool tryParseTomlStringToken( const char *ptrText,
                              size_t *ptrConsumedLength,
                              char *aryOutput,
                              size_t outputSize );

#endif
