/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "color.h"
#include "defs.h"
#include "filter_globals.h"
#include "utility.h"
typedef struct
{
   const char *ptrName;
   int colorValue;
} NamedColorSpec;

static int *const aryColorFields[COLOR_FIELD_COUNT] =
   {
      &color.text,
      &color.forum,
      &color.number,
      &color.errorTextColor,
      &color.ansiBlackTextColor,
      &color.ansiBlueTextColor,
      &color.ansiMagentaTextColor,
      &color.postDate,
      &color.postName,
      &color.postText,
      &color.postFriendDate,
      &color.postFriendName,
      &color.postFriendText,
      &color.anonymous,
      &color.morePrompt,
      &color.ansiWhiteTextColor,
      &color.reserved5,
      &color.background,
      &color.inputText,
      &color.inputHighlight,
      &color.expressText,
      &color.expressName,
      &color.expressFriendText,
      &color.expressFriendName };

static const char *const aryColorTomlKeys[COLOR_FIELD_COUNT] =
   {
      "text",
      "forum_prompt",
      "number_prompt",
      "error_text",
      "incoming_ansi_black",
      "incoming_ansi_blue",
      "incoming_ansi_magenta",
      "post_date",
      "post_name",
      "post_text",
      "post_friend_date",
      "post_friend_name",
      "post_friend_text",
      "anonymous_post",
      "more_prompt",
      "incoming_ansi_white",
      NULL,
      "background",
      "input_text",
      "input_highlight",
      "express_text",
      "express_name",
      "express_friend_text",
      "express_friend_name" };

static const NamedColorSpec aryNamedColors[] =
   {
      { "brightblack", 8 },
      { "brightred", 9 },
      { "brightgreen", 10 },
      { "brightyellow", 11 },
      { "brightblue", 12 },
      { "brightmagenta", 13 },
      { "brightpurple", 13 },
      { "brightcyan", 14 },
      { "brightwhite", 15 },
      { "black", 16 },
      { "red", 160 },
      { "green", 34 },
      { "yellow", 220 },
      { "blue", 26 },
      { "magenta", 91 },
      { "purple", 91 },
      { "cyan", 44 },
      { "white", 231 },
      { "default", COLOR_VALUE_DEFAULT } };

static bool isColorNameMatch( const char *ptrLeft, const char *ptrRight );
static int hexDigitValue( int inputChar );
static int transformIncomingAnsiColor( int inputChar );
static int transformPostHeaderColor( int inputChar, int isFriend );

/// @brief Translate a general incoming ANSI color digit to the active theme.
///
/// @param inputChar Incoming ANSI color digit.
///
/// @return The transformed theme color value.
int ansiTransform( int inputChar )
{
   int transformedColor;

   transformedColor = transformIncomingAnsiColor( inputChar );

   return transformedColor;
}

/// @brief Recolor an express message line using the active theme.
///
/// @param ptrText Express message line to rewrite in place.
/// @param size Size of the destination buffer.
///
/// @return This function does not return a value.
void ansiTransformExpress( char *ptrText, size_t size )
{
   char aryTempText[580];
   char aryMessageColor[ANSI_SEQUENCE_BUFFER_SIZE];
   char aryNameColor[ANSI_SEQUENCE_BUFFER_SIZE];
   char aryResetColor[ANSI_SEQUENCE_BUFFER_SIZE];
   char *ptrExpressSender, *ptrExpressMarker;

   // Insert color only when ANSI is being used
   if ( !flagsConfiguration.shouldUseAnsi )
   {
      return;
   }

   // Verify this is an X message and set up pointers
   ptrExpressSender = findSubstring( ptrText, ") to " );
   ptrExpressMarker = findSubstring( ptrText, ") from " );
   if ( !ptrExpressSender && !ptrExpressMarker )
   {
      return;
   }
   if ( ( ptrExpressMarker && ptrExpressMarker < ptrExpressSender ) || !ptrExpressSender )
   {
      ptrExpressSender = ptrExpressMarker + 2;
   }

   ptrExpressMarker = findSubstring( ptrText, " at " );
   if ( !ptrExpressMarker )
   {
      return;
   }

   ptrExpressSender += 4;
   *( ptrExpressSender++ ) = 0;
   *( ptrExpressMarker++ ) = 0;

   if ( slistFind( friendList, ptrExpressSender, fStrCompareVoid ) != -1 )
   {
      formatAnsiForegroundSequence( aryMessageColor, sizeof( aryMessageColor ),
                                    color.expressFriendText );
      formatAnsiForegroundSequence( aryNameColor, sizeof( aryNameColor ),
                                    color.expressFriendName );
   }
   else
   {
      formatAnsiForegroundSequence( aryMessageColor, sizeof( aryMessageColor ),
                                    color.expressText );
      formatAnsiForegroundSequence( aryNameColor, sizeof( aryNameColor ),
                                    color.expressName );
   }
   formatAnsiForegroundSequence( aryResetColor, sizeof( aryResetColor ),
                                 color.text );
   snprintf( aryTempText, sizeof( aryTempText ), "%s%s %s%s%s %s%s",
             aryMessageColor, ptrText, aryNameColor, ptrExpressSender,
             aryMessageColor, ptrExpressMarker, aryResetColor );
   lastColor = color.text;
   snprintf( ptrText, size, "%s", aryTempText );
}

/// @brief Translate a post ANSI color digit to the active post theme color.
///
/// @param inputChar Incoming ANSI color digit.
/// @param isFriend Non-zero when the post belongs to a friend.
///
/// @return The transformed theme color value.
int ansiTransformPost( int inputChar, int isFriend )
{
   int transformedColor;

   transformedColor = transformIncomingAnsiColor( inputChar );
   switch ( inputChar )
   {
      case '3':
         transformedColor = color.morePrompt;
         break;
      case '2':
         if ( isFriend )
         {
            transformedColor = color.postFriendText;
         }
         else
         {
            transformedColor = color.postText;
         }
         break;
      case '1':
         transformedColor = color.errorTextColor;
         break;
      default:
         break;
   }
   return transformedColor;
}

/// @brief Recolor a rendered post header using the active theme.
///
/// @param ptrText Header buffer to rewrite in place.
/// @param bufferSize Size of the header buffer.
/// @param isFriend Non-zero when the post belongs to a friend.
///
/// @return This function does not return a value.
void ansiTransformPostHeader( char *ptrText, size_t bufferSize, int isFriend )
{
   char aryTransformedHeader[320];
   char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];
   char *ptrScan;
   size_t writeOffset;

   writeOffset = 0;

   // Rewrite simple ESC[3xm foreground sequences into full palette-aware ANSI.
   for ( ptrScan = ptrText; *ptrScan != '\0' && writeOffset < sizeof( aryTransformedHeader ) - 1; )
   {
      if ( ptrScan[0] == '\033' && ptrScan[1] == '[' && ptrScan[2] == '3' &&
           ptrScan[3] != '\0' && ptrScan[4] == 'm' )
      {
         lastColor = transformPostHeaderColor( ptrScan[3], isFriend );
         formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                       lastColor );
         writeOffset += (size_t)snprintf( aryTransformedHeader + writeOffset,
                                          sizeof( aryTransformedHeader ) - writeOffset,
                                          "%s", aryAnsiSequence );
         ptrScan += 5;
         continue;
      }

      aryTransformedHeader[writeOffset++] = *ptrScan++;
   }

   aryTransformedHeader[writeOffset] = '\0';
   snprintf( ptrText, bufferSize, "%s", aryTransformedHeader );
}

/// @brief Return one color field from the internal color-field order.
///
/// @param colorIndex Field index in the internal color array.
///
/// @return Configured color value at the requested index.
int colorFieldValue( int colorIndex )
{
   assert( colorIndex >= 0 );
   assert( colorIndex < COLOR_FIELD_COUNT );

   return *aryColorFields[colorIndex];
}

/// @brief Return the canonical TOML key name for one persisted color field.
///
/// @param colorIndex Field index in the internal color array.
///
/// @return TOML key name, or `NULL` when that internal field is not persisted.
const char *colorFieldTomlKeyName( int colorIndex )
{
   assert( colorIndex >= 0 );
   assert( colorIndex < COLOR_FIELD_COUNT );

   return aryColorTomlKeys[colorIndex];
}

/// @brief Look up the canonical name for a color value.
///
/// @param colorValue Color value to resolve.
///
/// @return Matching color name, or `NULL` if the value is unknown.
const char *colorNameFromValue( int colorValue )
{
   size_t itemIndex;

   if ( colorValueIsRgb( colorValue ) )
   {
      return NULL;
   }

   for ( itemIndex = 0; itemIndex < sizeof( aryNamedColors ) / sizeof( aryNamedColors[0] ); itemIndex++ )
   {
      if ( aryNamedColors[itemIndex].colorValue == colorValue )
      {
         return aryNamedColors[itemIndex].ptrName;
      }
   }

   return NULL;
}

/// @brief Return the canonical TOML name for one configured color output mode.
///
/// @param outputMode Runtime color output mode.
///
/// @return Canonical TOML string for the mode.
const char *colorOutputModeName( ColorOutputMode outputMode )
{
   switch ( outputMode )
   {
      case COLOR_OUTPUT_MODE_TRUECOLOR:
         return "truecolor";

      case COLOR_OUTPUT_MODE_256:
         return "256";

      case COLOR_OUTPUT_MODE_AUTO:
      default:
         return "auto";
   }
}

/// @brief Parse a TOML hex color string into the internal RGB encoding.
///
/// @param ptrColorText Hex string in `#RRGGBB` form.
///
/// @return Encoded RGB color value, or `-1` if the string is invalid.
int colorValueFromHexString( const char *ptrColorText )
{
   int blue;
   int green;
   int red;

   if ( ptrColorText == NULL || strlen( ptrColorText ) != 7 || ptrColorText[0] != '#' )
   {
      return -1;
   }

   red = ( hexDigitValue( ptrColorText[1] ) << 4 ) | hexDigitValue( ptrColorText[2] );
   green = ( hexDigitValue( ptrColorText[3] ) << 4 ) | hexDigitValue( ptrColorText[4] );
   blue = ( hexDigitValue( ptrColorText[5] ) << 4 ) | hexDigitValue( ptrColorText[6] );
   if ( red < 0 || green < 0 || blue < 0 )
   {
      return -1;
   }

   return colorValueFromRgb( red, green, blue );
}

/// @brief Convert a legacy digit color code into its numeric value.
///
/// @param inputChar Legacy color digit or raw value.
///
/// @return Parsed color value.
int colorValueFromLegacyDigit( int inputChar )
{
   if ( inputChar >= '0' && inputChar <= '9' )
   {
      return inputChar - '0';
   }

   return inputChar;
}

/// @brief Resolve a configured color name to its color value.
///
/// @param ptrColorName Color name to look up.
///
/// @return Matching color value, or `-1` if the name is unknown.
int colorValueFromName( const char *ptrColorName )
{
   size_t itemIndex;

   if ( ptrColorName == NULL || *ptrColorName == '\0' )
   {
      return -1;
   }

   for ( itemIndex = 0; itemIndex < sizeof( aryNamedColors ) / sizeof( aryNamedColors[0] ); itemIndex++ )
   {
      if ( isColorNameMatch( ptrColorName, aryNamedColors[itemIndex].ptrName ) )
      {
         return aryNamedColors[itemIndex].colorValue;
      }
   }

   return -1;
}

/// @brief Parse a TOML color output mode string.
///
/// @param ptrModeName TOML string value to parse.
/// @param ptrOutMode Destination for the parsed mode.
///
/// @return `true` on success, otherwise `false`.
bool tryFindColorOutputMode( const char *ptrModeName,
                             ColorOutputMode *ptrOutMode )
{
   if ( ptrModeName == NULL || ptrOutMode == NULL )
   {
      return false;
   }
   if ( isColorNameMatch( ptrModeName, "auto" ) )
   {
      *ptrOutMode = COLOR_OUTPUT_MODE_AUTO;
      return true;
   }
   if ( isColorNameMatch( ptrModeName, "truecolor" ) )
   {
      *ptrOutMode = COLOR_OUTPUT_MODE_TRUECOLOR;
      return true;
   }
   if ( strcmp( ptrModeName, "256" ) == 0 )
   {
      *ptrOutMode = COLOR_OUTPUT_MODE_256;
      return true;
   }

   return false;
}

/// @brief Convert a color value back to its legacy digit form.
///
/// @param colorValue Color value to encode.
///
/// @return Legacy digit character value.
int colorValueToLegacyDigit( int colorValue )
{
   return colorValue + '0';
}

/// @brief Format a themed ANSI foreground sequence for an incoming color digit.
///
/// @param ptrBuffer Destination buffer for the ANSI sequence.
/// @param bufferSize Size of the destination buffer.
/// @param inputChar Incoming ANSI color digit.
/// @param isPostContext Non-zero when post color mapping rules apply.
/// @param isFriend Non-zero when the post belongs to a friend.
///
/// @return Number of characters written by `snprintf`.
int formatTransformedAnsiForegroundSequence( char *ptrBuffer, size_t bufferSize,
                                             int inputChar, int isPostContext,
                                             int isFriend )
{
   int transformedColor;

   if ( isPostContext )
   {
      transformedColor = ansiTransformPost( inputChar, isFriend );
   }
   else
   {
      transformedColor = ansiTransform( inputChar );
   }

   lastColor = transformedColor;
   return formatAnsiForegroundSequence( ptrBuffer, bufferSize, transformedColor );
}

/// @brief Compare two color names case-insensitively.
///
/// @param ptrLeft Left-hand color name.
/// @param ptrRight Right-hand color name.
///
/// @return `true` if the names match, otherwise `false`.
static bool isColorNameMatch( const char *ptrLeft, const char *ptrRight )
{
   while ( *ptrLeft && *ptrRight )
   {
      if ( tolower( (unsigned char)*ptrLeft ) != tolower( (unsigned char)*ptrRight ) )
      {
         return false;
      }
      ptrLeft++;
      ptrRight++;
   }

   return *ptrLeft == '\0' && *ptrRight == '\0';
}

/// @brief Convert one hexadecimal digit to its integer value.
///
/// @param inputChar Hexadecimal digit character.
///
/// @return Digit value, or `-1` for invalid input.
static int hexDigitValue( int inputChar )
{
   if ( inputChar >= '0' && inputChar <= '9' )
   {
      return inputChar - '0';
   }
   if ( inputChar >= 'a' && inputChar <= 'f' )
   {
      return inputChar - 'a' + 10;
   }
   if ( inputChar >= 'A' && inputChar <= 'F' )
   {
      return inputChar - 'A' + 10;
   }

   return -1;
}

/// @brief Set one color field in the internal color-field order.
///
/// @param colorIndex Field index in the internal color array.
/// @param colorValue New color value.
///
/// @return This function does not return a value.
void setColorFieldValue( int colorIndex, int colorValue )
{
   assert( colorIndex >= 0 );
   assert( colorIndex < COLOR_FIELD_COUNT );

   *aryColorFields[colorIndex] = colorValue;
}

/// @brief Resolve one TOML color key to its internal field index.
///
/// @param ptrKeyName TOML key name to look up.
/// @param ptrOutColorIndex Destination for the matching field index.
///
/// @return `true` on success, otherwise `false`.
bool tryFindColorFieldIndexByTomlKeyName( const char *ptrKeyName,
                                          int *ptrOutColorIndex )
{
   int itemIndex;

   if ( ptrKeyName == NULL || ptrOutColorIndex == NULL )
   {
      return false;
   }

   for ( itemIndex = 0; itemIndex < COLOR_FIELD_COUNT; itemIndex++ )
   {
      if ( aryColorTomlKeys[itemIndex] != NULL &&
           strcmp( ptrKeyName, aryColorTomlKeys[itemIndex] ) == 0 )
      {
         *ptrOutColorIndex = itemIndex;
         return true;
      }
   }

   return false;
}

/// @brief Translate a general incoming ANSI color digit to the configured palette.
///
/// @param inputChar Incoming ANSI color digit.
///
/// @return The transformed color value.
static int transformIncomingAnsiColor( int inputChar )
{
   switch ( inputChar )
   {
      case '0':
         return color.ansiBlackTextColor;
      case '1':
         return color.errorTextColor;
      case '2':
         return color.text;
      case '3':
         return color.forum;
      case '4':
         return color.ansiBlueTextColor;
      case '5':
         return color.ansiMagentaTextColor;
      case '6':
         return color.number;
      case '7':
         return color.ansiWhiteTextColor;
      default:
         return colorValueFromLegacyDigit( inputChar );
   }
}

/// @brief Translate a post header color digit to the configured post header color.
///
/// @param inputChar Incoming ANSI color digit.
/// @param isFriend Non-zero when the post belongs to a friend.
///
/// @return The transformed post header color value.
static int transformPostHeaderColor( int inputChar, int isFriend )
{
   switch ( inputChar )
   {
      case '6':
         if ( isFriend )
         {
            return color.postFriendName;
         }
         return color.postName;
      case '5':
         if ( isFriend )
         {
            return color.postFriendDate;
         }
         return color.postDate;
      case '3':
         return color.anonymous;
      case '2':
         if ( isFriend )
         {
            return color.postFriendText;
         }
         return color.postText;
      default:
         return transformIncomingAnsiColor( inputChar );
   }
}
