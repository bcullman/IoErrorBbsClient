/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "color.h"
#include "defs.h"
#define ifzero( x ) if ( ( x ) < 0 || clearall )
#define RGB( red, green, blue ) colorValueFromRgb( ( red ), ( green ), ( blue ) )

/// @brief Apply the bright theme palette to the current color settings.
///
/// @return This function does not return a value.
void brilliantColors( void )
{
   useBlackThemeBackgrounds = false;
   color.text = RGB( 0x00, 0xff, 0x00 );
   color.forum = RGB( 0xff, 0xff, 0x00 );
   color.number = RGB( 0x00, 0xff, 0xff );
   color.errorTextColor = RGB( 0xff, 0x00, 0x00 );
   color.ansiBlackTextColor = RGB( 0x00, 0xff, 0x00 );
   color.ansiBlueTextColor = RGB( 0x00, 0x00, 0xff );
   color.ansiMagentaTextColor = RGB( 0xff, 0x00, 0xff );
   color.postDate = RGB( 0xff, 0x00, 0xff );
   color.postName = RGB( 0x00, 0xff, 0xff );
   color.postText = RGB( 0x00, 0xff, 0x00 );
   color.postFriendDate = RGB( 0xff, 0x00, 0xff );
   color.postFriendName = RGB( 0xff, 0x00, 0x00 );
   color.postFriendText = RGB( 0x00, 0xff, 0x00 );
   color.anonymous = RGB( 0xff, 0xff, 0x00 );
   color.morePrompt = RGB( 0xff, 0xff, 0x00 );
   color.ansiWhiteTextColor = RGB( 0xff, 0xff, 0xff );
   color.reserved5 = RGB( 0xff, 0xff, 0xff );
   color.background = 0;
   color.inputText = RGB( 0x00, 0xff, 0x00 );
   color.inputHighlight = RGB( 0x00, 0xff, 0xff );
   color.expressText = RGB( 0x00, 0xff, 0x00 );
   color.expressName = RGB( 0x00, 0xff, 0x00 );
   color.expressFriendName = RGB( 0x00, 0xff, 0x00 );
   color.expressFriendText = RGB( 0x00, 0xff, 0x00 );
}

/// @brief Apply the Catppuccin Latte palette to the current color settings.
///
/// @return This function does not return a value.
void catppuccinLatteColors( void )
{
   useBlackThemeBackgrounds = false;
   color.text = RGB( 0x4c, 0x4f, 0x69 );
   color.forum = RGB( 0x1e, 0x66, 0xf5 );
   color.number = RGB( 0x20, 0x9f, 0xb5 );
   color.errorTextColor = RGB( 0xd2, 0x0f, 0x39 );
   color.ansiBlackTextColor = RGB( 0x9c, 0xa0, 0xb0 );
   color.ansiBlueTextColor = RGB( 0x1e, 0x66, 0xf5 );
   color.ansiMagentaTextColor = RGB( 0xea, 0x76, 0xcb );
   color.postDate = RGB( 0x20, 0x9f, 0xb5 );
   color.postName = RGB( 0x72, 0x87, 0xfd );
   color.postText = RGB( 0x4c, 0x4f, 0x69 );
   color.postFriendDate = RGB( 0x17, 0x92, 0x99 );
   color.postFriendName = RGB( 0x40, 0xa0, 0x2b );
   color.postFriendText = RGB( 0x4c, 0x4f, 0x69 );
   color.anonymous = RGB( 0xfe, 0x64, 0x0b );
   color.morePrompt = RGB( 0xdf, 0x8e, 0x1d );
   color.ansiWhiteTextColor = RGB( 0x5c, 0x5f, 0x77 );
   color.reserved5 = RGB( 0x4c, 0x4f, 0x69 );
   color.background = RGB( 0xef, 0xf1, 0xf5 );
   color.inputText = RGB( 0x4c, 0x4f, 0x69 );
   color.inputHighlight = RGB( 0x1e, 0x66, 0xf5 );
   color.expressText = RGB( 0x4c, 0x4f, 0x69 );
   color.expressName = RGB( 0x1e, 0x66, 0xf5 );
   color.expressFriendName = RGB( 0x40, 0xa0, 0x2b );
   color.expressFriendText = RGB( 0x4c, 0x4f, 0x69 );
}

/// @brief Apply the Catppuccin Macchiato palette to the current colors.
///
/// @return This function does not return a value.
void catppuccinMacchiatoColors( void )
{
   useBlackThemeBackgrounds = true;
   color.text = RGB( 0xca, 0xd3, 0xf5 );
   color.forum = RGB( 0x8a, 0xad, 0xf4 );
   color.number = RGB( 0x7d, 0xc4, 0xe4 );
   color.errorTextColor = RGB( 0xed, 0x87, 0x96 );
   color.ansiBlackTextColor = RGB( 0x80, 0x84, 0x9d );
   color.ansiBlueTextColor = RGB( 0x8a, 0xad, 0xf4 );
   color.ansiMagentaTextColor = RGB( 0xc6, 0xa0, 0xf6 );
   color.postDate = RGB( 0x7d, 0xc4, 0xe4 );
   color.postName = RGB( 0xb7, 0xbd, 0xf8 );
   color.postText = RGB( 0xca, 0xd3, 0xf5 );
   color.postFriendDate = RGB( 0x8b, 0xd5, 0xca );
   color.postFriendName = RGB( 0xa6, 0xda, 0x95 );
   color.postFriendText = RGB( 0xca, 0xd3, 0xf5 );
   color.anonymous = RGB( 0xf5, 0xa9, 0x7f );
   color.morePrompt = RGB( 0xee, 0xd4, 0x9f );
   color.ansiWhiteTextColor = RGB( 0xca, 0xd3, 0xf5 );
   color.reserved5 = RGB( 0xca, 0xd3, 0xf5 );
   color.background = RGB( 0x24, 0x27, 0x3a );
   color.inputText = RGB( 0xca, 0xd3, 0xf5 );
   color.inputHighlight = RGB( 0x8a, 0xad, 0xf4 );
   color.expressText = RGB( 0xca, 0xd3, 0xf5 );
   color.expressName = RGB( 0xb7, 0xbd, 0xf8 );
   color.expressFriendName = RGB( 0xa6, 0xda, 0x95 );
   color.expressFriendText = RGB( 0xca, 0xd3, 0xf5 );
}

/// @brief Apply the colorblind-friendly theme palette to the current colors.
///
/// @return This function does not return a value.
void colorblindColors( void )
{
   useBlackThemeBackgrounds = false;
   color.text = RGB( 0xff, 0xff, 0xff );
   color.forum = RGB( 0x5f, 0xaf, 0xff );
   color.number = RGB( 0xff, 0xaf, 0x00 );
   color.errorTextColor = RGB( 0xd7, 0x5f, 0x00 );
   color.ansiBlackTextColor = RGB( 0xaf, 0xaf, 0xd7 );
   color.ansiBlueTextColor = RGB( 0x5f, 0xaf, 0xff );
   color.ansiMagentaTextColor = RGB( 0xd7, 0x87, 0xaf );
   color.postDate = RGB( 0x5f, 0xaf, 0xff );
   color.postName = RGB( 0xff, 0xaf, 0x00 );
   color.postText = RGB( 0xff, 0xff, 0xff );
   color.postFriendDate = RGB( 0x00, 0x5f, 0xaf );
   color.postFriendName = RGB( 0xd7, 0x87, 0xaf );
   color.postFriendText = RGB( 0xff, 0xff, 0xff );
   color.anonymous = RGB( 0xff, 0xd7, 0x5f );
   color.morePrompt = RGB( 0xff, 0xd7, 0x5f );
   color.ansiWhiteTextColor = RGB( 0xff, 0xd7, 0x5f );
   color.reserved5 = RGB( 0xff, 0xff, 0xff );
   color.background = 0;
   color.inputText = RGB( 0xff, 0xff, 0xff );
   color.inputHighlight = RGB( 0xff, 0xaf, 0x00 );
   color.expressText = RGB( 0xff, 0xff, 0xff );
   color.expressName = RGB( 0xff, 0xaf, 0x00 );
   color.expressFriendName = RGB( 0xd7, 0x87, 0xaf );
   color.expressFriendText = RGB( 0xff, 0xff, 0xff );
}

/// @brief Fill unset color fields with the built-in default theme values.
///
/// @param clearall When non-zero, reinitialize every color field, including the
/// background color. When zero, only unset fields are filled in.
///
/// @return This function does not return a value.
void defaultColors( int clearall )
{
   useBlackThemeBackgrounds = false;
   ifzero( color.text ) color.text = RGB( 0x5f, 0xff, 0x87 );
   ifzero( color.forum ) color.forum = RGB( 0xff, 0xd7, 0x5f );
   ifzero( color.number ) color.number = RGB( 0x5f, 0xd7, 0xff );
   ifzero( color.errorTextColor ) color.errorTextColor = RGB( 0xff, 0x5f, 0x5f );
   ifzero( color.ansiBlackTextColor ) color.ansiBlackTextColor = RGB( 0x5f, 0xff, 0x87 );
   ifzero( color.ansiBlueTextColor ) color.ansiBlueTextColor = RGB( 0x5f, 0x87, 0xff );
   ifzero( color.ansiMagentaTextColor ) color.ansiMagentaTextColor = RGB( 0xd7, 0x87, 0xff );
   ifzero( color.postDate ) color.postDate = RGB( 0xd7, 0x87, 0xff );
   ifzero( color.postName ) color.postName = RGB( 0x5f, 0xd7, 0xff );
   ifzero( color.postText ) color.postText = RGB( 0x5f, 0xff, 0x87 );
   ifzero( color.postFriendDate ) color.postFriendDate = RGB( 0xd7, 0x87, 0xff );
   ifzero( color.postFriendName ) color.postFriendName = RGB( 0xff, 0x5f, 0x5f );
   ifzero( color.postFriendText ) color.postFriendText = RGB( 0x5f, 0xff, 0x87 );
   ifzero( color.anonymous ) color.anonymous = RGB( 0xff, 0xd7, 0x5f );
   ifzero( color.morePrompt ) color.morePrompt = RGB( 0xff, 0xd7, 0x5f );
   ifzero( color.ansiWhiteTextColor ) color.ansiWhiteTextColor = RGB( 0xe4, 0xe4, 0xe4 );
   color.reserved5 = RGB( 0xe4, 0xe4, 0xe4 );
   if ( clearall )
   {
      color.background = 0;
   }
   ifzero( color.inputText ) color.inputText = RGB( 0x5f, 0xff, 0x87 );
   ifzero( color.inputHighlight ) color.inputHighlight = RGB( 0x5f, 0xd7, 0xff );
   ifzero( color.expressText ) color.expressText = RGB( 0x5f, 0xff, 0x87 );
   ifzero( color.expressName ) color.expressName = RGB( 0x5f, 0xff, 0x87 );
   ifzero( color.expressFriendName ) color.expressFriendName = RGB( 0x5f, 0xff, 0x87 );
   ifzero( color.expressFriendText ) color.expressFriendText = RGB( 0x5f, 0xff, 0x87 );
}

/// @brief Apply the Dracula Pro dark palette to the current color settings.
///
/// @return This function does not return a value.
void draculaProColors( void )
{
   useBlackThemeBackgrounds = true;
   color.text = RGB( 0xe3, 0xe2, 0xe9 );
   color.forum = RGB( 0x73, 0x59, 0xf8 );
   color.number = RGB( 0x5c, 0xf5, 0xdb );
   color.errorTextColor = RGB( 0xf8, 0x73, 0x59 );
   color.ansiBlackTextColor = RGB( 0x73, 0x59, 0xf8 );
   color.ansiBlueTextColor = RGB( 0x5c, 0xf5, 0xdb );
   color.ansiMagentaTextColor = RGB( 0xf8, 0x59, 0xa8 );
   color.postDate = RGB( 0x5c, 0xf5, 0xdb );
   color.postName = RGB( 0xf8, 0x59, 0xa8 );
   color.postText = RGB( 0xe3, 0xe2, 0xe9 );
   color.postFriendDate = RGB( 0x73, 0x59, 0xf8 );
   color.postFriendName = RGB( 0x66, 0xf8, 0x59 );
   color.postFriendText = RGB( 0xe3, 0xe2, 0xe9 );
   color.anonymous = RGB( 0xf8, 0xf8, 0x59 );
   color.morePrompt = RGB( 0xf8, 0xf8, 0x59 );
   color.ansiWhiteTextColor = RGB( 0xe3, 0xe2, 0xe9 );
   color.reserved5 = RGB( 0xe3, 0xe2, 0xe9 );
   color.background = 0;
   color.inputText = RGB( 0xe3, 0xe2, 0xe9 );
   color.inputHighlight = RGB( 0x5c, 0xf5, 0xdb );
   color.expressText = RGB( 0xe3, 0xe2, 0xe9 );
   color.expressName = RGB( 0xf8, 0x59, 0xa8 );
   color.expressFriendName = RGB( 0x66, 0xf8, 0x59 );
   color.expressFriendText = RGB( 0xe3, 0xe2, 0xe9 );
}

/// @brief Apply the Everforest dark medium palette to the current color settings.
///
/// @return This function does not return a value.
void everforestDarkColors( void )
{
   useBlackThemeBackgrounds = true;
   color.text = RGB( 0xd3, 0xc6, 0xaa );
   color.forum = RGB( 0x7f, 0xbb, 0xb3 );
   color.number = RGB( 0x83, 0xc0, 0x92 );
   color.errorTextColor = RGB( 0xe6, 0x7e, 0x80 );
   color.ansiBlackTextColor = RGB( 0x85, 0x92, 0x89 );
   color.ansiBlueTextColor = RGB( 0x7f, 0xbb, 0xb3 );
   color.ansiMagentaTextColor = RGB( 0xd6, 0x99, 0xb6 );
   color.postDate = RGB( 0x83, 0xc0, 0x92 );
   color.postName = RGB( 0xa7, 0xc0, 0x80 );
   color.postText = RGB( 0xd3, 0xc6, 0xaa );
   color.postFriendDate = RGB( 0x7f, 0xbb, 0xb3 );
   color.postFriendName = RGB( 0xdb, 0xbc, 0x7f );
   color.postFriendText = RGB( 0xd3, 0xc6, 0xaa );
   color.anonymous = RGB( 0xe6, 0x98, 0x75 );
   color.morePrompt = RGB( 0xdb, 0xbc, 0x7f );
   color.ansiWhiteTextColor = RGB( 0xe5, 0xdd, 0xc9 );
   color.reserved5 = RGB( 0xd3, 0xc6, 0xaa );
   color.background = RGB( 0x2f, 0x38, 0x3e );
   color.inputText = RGB( 0xd3, 0xc6, 0xaa );
   color.inputHighlight = RGB( 0x7f, 0xbb, 0xb3 );
   color.expressText = RGB( 0xd3, 0xc6, 0xaa );
   color.expressName = RGB( 0xa7, 0xc0, 0x80 );
   color.expressFriendName = RGB( 0xdb, 0xbc, 0x7f );
   color.expressFriendText = RGB( 0xd3, 0xc6, 0xaa );
}

/// @brief Apply the Everforest light medium palette to the current color settings.
///
/// @return This function does not return a value.
void everforestLightColors( void )
{
   useBlackThemeBackgrounds = false;
   color.text = RGB( 0x5c, 0x6a, 0x72 );
   color.forum = RGB( 0x35, 0x8f, 0xa2 );
   color.number = RGB( 0x3a, 0x94, 0x84 );
   color.errorTextColor = RGB( 0xf8, 0x55, 0x52 );
   color.ansiBlackTextColor = RGB( 0xa6, 0xb0, 0x9f );
   color.ansiBlueTextColor = RGB( 0x35, 0x8f, 0xa2 );
   color.ansiMagentaTextColor = RGB( 0xdf, 0x69, 0xba );
   color.postDate = RGB( 0x3a, 0x94, 0x84 );
   color.postName = RGB( 0x8d, 0xb8, 0x61 );
   color.postText = RGB( 0x5c, 0x6a, 0x72 );
   color.postFriendDate = RGB( 0x35, 0x8f, 0xa2 );
   color.postFriendName = RGB( 0xda, 0xa5, 0x20 );
   color.postFriendText = RGB( 0x5c, 0x6a, 0x72 );
   color.anonymous = RGB( 0xf5, 0x7d, 0x26 );
   color.morePrompt = RGB( 0xbf, 0x98, 0x3d );
   color.ansiWhiteTextColor = RGB( 0x4f, 0x5b, 0x58 );
   color.reserved5 = RGB( 0x5c, 0x6a, 0x72 );
   color.background = RGB( 0xfd, 0xf6, 0xe3 );
   color.inputText = RGB( 0x5c, 0x6a, 0x72 );
   color.inputHighlight = RGB( 0x35, 0x8f, 0xa2 );
   color.expressText = RGB( 0x5c, 0x6a, 0x72 );
   color.expressName = RGB( 0x8d, 0xb8, 0x61 );
   color.expressFriendName = RGB( 0xda, 0xa5, 0x20 );
   color.expressFriendText = RGB( 0x5c, 0x6a, 0x72 );
}

/// @brief Apply the Gruvbox dark hard palette to the current color settings.
///
/// This preset intentionally leans on Gruvbox's softer aqua, purple, yellow,
/// and olive-green tones instead of the more aggressive red/orange accents.
///
/// @return This function does not return a value.
void gruvboxDarkColors( void )
{
   useBlackThemeBackgrounds = true;
   color.text = RGB( 0xeb, 0xdb, 0xb2 );
   color.forum = RGB( 0x83, 0xa5, 0x98 );
   color.number = RGB( 0x8e, 0xc0, 0x7c );
   color.errorTextColor = RGB( 0xfe, 0x80, 0x19 );
   color.ansiBlackTextColor = RGB( 0x92, 0x83, 0x74 );
   color.ansiBlueTextColor = RGB( 0x83, 0xa5, 0x98 );
   color.ansiMagentaTextColor = RGB( 0xd3, 0x86, 0x9b );
   color.postDate = RGB( 0x83, 0xa5, 0x98 );
   color.postName = RGB( 0xb8, 0xbb, 0x26 );
   color.postText = RGB( 0xeb, 0xdb, 0xb2 );
   color.postFriendDate = RGB( 0xd3, 0x86, 0x9b );
   color.postFriendName = RGB( 0x8e, 0xc0, 0x7c );
   color.postFriendText = RGB( 0xeb, 0xdb, 0xb2 );
   color.anonymous = RGB( 0xfe, 0x80, 0x19 );
   color.morePrompt = RGB( 0xfa, 0xbd, 0x2f );
   color.ansiWhiteTextColor = RGB( 0xfb, 0xf1, 0xc7 );
   color.reserved5 = RGB( 0xeb, 0xdb, 0xb2 );
   color.background = RGB( 0x1d, 0x20, 0x21 );
   color.inputText = RGB( 0xeb, 0xdb, 0xb2 );
   color.inputHighlight = RGB( 0x83, 0xa5, 0x98 );
   color.expressText = RGB( 0xeb, 0xdb, 0xb2 );
   color.expressName = RGB( 0xb8, 0xbb, 0x26 );
   color.expressFriendName = RGB( 0x8e, 0xc0, 0x7c );
   color.expressFriendText = RGB( 0xeb, 0xdb, 0xb2 );
}

/// @brief Apply the Gruvbox light hard palette to the current color settings.
///
/// This preset keeps the same quieter accent bias as the dark palette while
/// switching to Gruvbox's light background and softer neutral text.
///
/// @return This function does not return a value.
void gruvboxLightColors( void )
{
   useBlackThemeBackgrounds = false;
   color.text = RGB( 0x3c, 0x38, 0x36 );
   color.forum = RGB( 0x45, 0x85, 0x88 );
   color.number = RGB( 0x68, 0x9d, 0x6a );
   color.errorTextColor = RGB( 0xd6, 0x5d, 0x0e );
   color.ansiBlackTextColor = RGB( 0xa8, 0x99, 0x84 );
   color.ansiBlueTextColor = RGB( 0x45, 0x85, 0x88 );
   color.ansiMagentaTextColor = RGB( 0xb1, 0x62, 0x86 );
   color.postDate = RGB( 0x45, 0x85, 0x88 );
   color.postName = RGB( 0x79, 0x74, 0x0e );
   color.postText = RGB( 0x3c, 0x38, 0x36 );
   color.postFriendDate = RGB( 0xb1, 0x62, 0x86 );
   color.postFriendName = RGB( 0x68, 0x9d, 0x6a );
   color.postFriendText = RGB( 0x3c, 0x38, 0x36 );
   color.anonymous = RGB( 0xaf, 0x3a, 0x03 );
   color.morePrompt = RGB( 0xd7, 0x99, 0x21 );
   color.ansiWhiteTextColor = RGB( 0x28, 0x28, 0x28 );
   color.reserved5 = RGB( 0x3c, 0x38, 0x36 );
   color.background = RGB( 0xf9, 0xf5, 0xd7 );
   color.inputText = RGB( 0x3c, 0x38, 0x36 );
   color.inputHighlight = RGB( 0x45, 0x85, 0x88 );
   color.expressText = RGB( 0x3c, 0x38, 0x36 );
   color.expressName = RGB( 0x79, 0x74, 0x0e );
   color.expressFriendName = RGB( 0x68, 0x9d, 0x6a );
   color.expressFriendText = RGB( 0x3c, 0x38, 0x36 );
}

/// @brief Apply the hot dog theme palette to the current color settings.
///
/// @return This function does not return a value.
void hotDogColors( void )
{
   useBlackThemeBackgrounds = false;
   color.text = RGB( 0xff, 0xd7, 0x00 );
   color.forum = RGB( 0xff, 0x00, 0x00 );
   color.number = RGB( 0xff, 0xd7, 0x00 );
   color.errorTextColor = RGB( 0xff, 0xff, 0xff );
   color.ansiBlackTextColor = RGB( 0xaf, 0x5f, 0x00 );
   color.ansiBlueTextColor = RGB( 0xff, 0xaf, 0x00 );
   color.ansiMagentaTextColor = RGB( 0xaf, 0x5f, 0x00 );
   color.postDate = RGB( 0xff, 0xff, 0x00 );
   color.postName = RGB( 0xff, 0xff, 0x00 );
   color.postText = RGB( 0xff, 0xaf, 0x00 );
   color.postFriendDate = RGB( 0xff, 0xff, 0x00 );
   color.postFriendName = RGB( 0xff, 0xff, 0x00 );
   color.postFriendText = RGB( 0xff, 0xaf, 0x00 );
   color.anonymous = RGB( 0xff, 0xff, 0x00 );
   color.morePrompt = RGB( 0xff, 0xd7, 0x00 );
   color.ansiWhiteTextColor = RGB( 0xff, 0xd7, 0x00 );
   color.reserved5 = RGB( 0xaf, 0x5f, 0x00 );
   color.background = 0;
   color.inputText = RGB( 0xff, 0xd7, 0x00 );
   color.inputHighlight = RGB( 0xff, 0xff, 0xff );
   color.expressText = RGB( 0xff, 0xaf, 0x00 );
   color.expressName = RGB( 0xff, 0xff, 0x00 );
   color.expressFriendName = RGB( 0xff, 0xff, 0x00 );
   color.expressFriendText = RGB( 0xff, 0xaf, 0x00 );
}
