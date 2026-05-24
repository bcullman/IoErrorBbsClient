/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file contains small TOML tokenizing helpers for config loading.
 */
#include "config_toml.h"

static void stripInlineTomlComment( char *ptrText );

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

/// @brief Trim leading and trailing ASCII whitespace in place.
///
/// @param ptrText Mutable string buffer to normalize.
///
/// @return Pointer to the trimmed view inside `ptrText`.
char *trimTomlWhitespace( char *ptrText )
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
                               size_t valueSize )
{
   const char *ptrEquals;
   char aryKeyBuffer[CONFIG_TOML_MAX_NAME_LENGTH];
   char aryValueBuffer[CONFIG_TOML_MAX_VALUE_LENGTH];
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

   ptrTrimmedKey = trimTomlWhitespace( aryKeyBuffer );
   stripInlineTomlComment( aryValueBuffer );
   ptrTrimmedValue = trimTomlWhitespace( aryValueBuffer );
   if ( *ptrTrimmedKey == '\0' || *ptrTrimmedValue == '\0' ||
        strlen( ptrTrimmedKey ) >= keyNameSize || strlen( ptrTrimmedValue ) >= valueSize )
   {
      return false;
   }

   snprintf( aryKeyName, keyNameSize, "%s", ptrTrimmedKey );
   snprintf( aryValue, valueSize, "%s", ptrTrimmedValue );
   return true;
}

/// @brief Decode a TOML double-quoted string.
///
/// @param ptrValue Raw value text to decode.
/// @param aryOutput Destination buffer for decoded text.
/// @param outputSize Capacity of `aryOutput`.
///
/// @return `true` on success, otherwise `false`.
bool tryParseTomlQuotedString( const char *ptrValue,
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
/// @return `true` on success, otherwise `false`.
bool tryParseTomlSectionName( const char *ptrLine,
                              char *arySectionName,
                              size_t sectionNameSize )
{
   size_t sectionNameLength;

   sectionNameLength = strlen( ptrLine );
   if ( sectionNameLength < 3 || ptrLine[0] != '[' || ptrLine[sectionNameLength - 1] != ']' )
   {
      return false;
   }
   sectionNameLength -= 2;
   if ( sectionNameLength >= sectionNameSize )
   {
      return false;
   }

   memcpy( arySectionName, ptrLine + 1, sectionNameLength );
   arySectionName[sectionNameLength] = '\0';
   (void)trimTomlWhitespace( arySectionName );
   return true;
}

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
         char aryQuotedValue[CONFIG_TOML_MAX_VALUE_LENGTH];

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
