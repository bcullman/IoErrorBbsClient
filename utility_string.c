/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file handles string search and message header parsing helpers.
 */
#include "client_globals.h"
#include "defs.h"
#include "utility.h"
static bool tryParseNameFromHeader( const char *header, char *ptrNameBuffer, size_t nameBufferSize );

/// @brief Duplicate a NUL-terminated string with heap storage.
///
/// @param ptrSource Source text to copy.
///
/// @return A newly allocated copy of `ptrSource`, or `NULL` on allocation failure.
char *duplicateString( const char *ptrSource )
{
   size_t length;
   char *ptrCopy;

   length = strlen( ptrSource ) + 2;
   ptrCopy = (char *)calloc( 1, length );
   if ( ptrCopy )
   {
      snprintf( ptrCopy, length, "%s", ptrSource );
   }
   return ptrCopy;
}

/// @brief Extract the sender name from a header and move it to the recent-name history.
///
/// @param header Header text to parse.
///
/// @return A pointer to the stored history entry, or `NULL` when no sender name
/// could be extracted.
char *extractName( const char *header )
{
   int charIndex;
   int existingIndex = -1;
   const char *ptrExtractedName = extractNameNoHistory( header );

   if ( !ptrExtractedName )
   {
      return NULL;
   }
   for ( charIndex = 0; charIndex < MAX_USER_NAME_HISTORY_COUNT; charIndex++ )
   {
      if ( !strcmp( aryLastName[charIndex], ptrExtractedName ) )
      {
         existingIndex = charIndex;
      }
   }
   if ( existingIndex != 0 )
   {
      for ( charIndex = ( existingIndex > 0 ) ? existingIndex - 1 : MAX_USER_NAME_HISTORY_COUNT - 2; charIndex >= 0; --charIndex )
      {
         snprintf( aryLastName[charIndex + 1], sizeof( aryLastName[charIndex + 1] ), "%s", aryLastName[charIndex] );
      }
      snprintf( aryLastName[0], sizeof( aryLastName[0] ), "%s", ptrExtractedName );
   }
   return (char *)aryLastName[0];
}

/// @brief Extract the sender name from a header without updating name history.
///
/// @param header Header text to parse.
///
/// @return A pointer to a static buffer containing the extracted name, or `NULL`
/// when no sender name could be found.
char *extractNameNoHistory( const char *header )
{
   static char aryExtractedName[sizeof( aryLastName[0] )];

   if ( !tryParseNameFromHeader( header, aryExtractedName, sizeof( aryExtractedName ) ) )
   {
      return NULL;
   }
   return aryExtractedName;
}

/// @brief Extract the numeric message identifier from a `(#...)` header field.
///
/// @param header Header text to parse.
///
/// @return The parsed message number, or `0` when no number is present.
int extractNumber( const char *header )
{
   char *ptrMessageNumber;
   int number = 0;

   ptrMessageNumber = findSubstring( header, "(#" );
   if ( !ptrMessageNumber )
   {
      return 0;
   }

   for ( ptrMessageNumber += 2; *ptrMessageNumber != ')'; ptrMessageNumber++ )
   {
      number += number * 10 + ( *ptrMessageNumber - '0' );
   }

   return number;
}

/// @brief Find the first occurrence of a character in a string.
///
/// @param ptrString String to search.
/// @param targetChar Character to locate.
///
/// @return A pointer to the first matching character, or `NULL` when not found.
char *findChar( const char *ptrString, int targetChar )
{
   const char *ptrSearch;

   ptrSearch = ptrString;
   while ( *ptrSearch && targetChar != *ptrSearch )
   {
      ptrSearch++;
   }
   if ( *ptrSearch )
   {
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#endif
      char *ptrResult = (char *)ptrSearch;
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
      return ptrResult;
   }
   else
   {
      return ( (char *)NULL );
   }
}

/// @brief Find the first occurrence of a substring within a string.
///
/// @param ptrString String to search.
/// @param ptrSubstring Substring to locate.
///
/// @return A pointer to the first matching substring, or `NULL` when not found.
char *findSubstring( const char *ptrString, const char *ptrSubstring )
{
   const char *ptrSearch;

   for ( ptrSearch = ptrString; *ptrSearch; ptrSearch++ )
   {
      if ( *ptrSearch == *ptrSubstring && !strncmp( ptrSearch, ptrSubstring, strlen( ptrSubstring ) ) )
      {
         break;
      }
   }
   if ( !*ptrSearch )
   {
      return ( (char *)NULL );
   }
   else
   {
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#endif
      char *ptrResult = (char *)ptrSearch;
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
      return ptrResult;
   }
}

/// @brief Parse a sender name out of a formatted message header.
///
/// @param header Header text to inspect.
/// @param ptrNameBuffer Destination buffer for the parsed name.
/// @param nameBufferSize Size of `ptrNameBuffer`.
///
/// @return `true` when a name was parsed successfully, otherwise `false`.
static bool tryParseNameFromHeader( const char *header, char *ptrNameBuffer, size_t nameBufferSize )
{
   char *ptrHeaderName;
   size_t nameLength;
   bool isAfterSpace;

   ptrHeaderName = findSubstring( header, " from " );
   if ( !ptrHeaderName )
   {
      return false;
   }
   ptrHeaderName += 6;
   if ( *ptrHeaderName == '\033' )
   {
      ptrHeaderName += 5;
   }
   isAfterSpace = true;
   nameLength = 0;
   {
      size_t itemIndex;
      size_t sourceLength = strlen( ptrHeaderName );

      for ( itemIndex = 0; itemIndex < sourceLength &&
                           nameLength + 1 < nameBufferSize;
            itemIndex++ )
      {
         char inputChar = ptrHeaderName[itemIndex];

         if ( inputChar == '\033' )
         {
            break;
         }
         if ( isAfterSpace && !isupper( (unsigned char)inputChar ) )
         {
            break;
         }
         ptrNameBuffer[nameLength++] = inputChar;
         if ( inputChar == ' ' )
         {
            isAfterSpace = true;
         }
         else
         {
            isAfterSpace = false;
         }
      }
   }
   while ( nameLength > 0 &&
           ( ptrNameBuffer[nameLength - 1] == ' ' ||
             ptrNameBuffer[nameLength - 1] == '\r' ) )
   {
      nameLength--;
   }
   ptrNameBuffer[nameLength] = '\0';
   return nameLength > 0;
}
