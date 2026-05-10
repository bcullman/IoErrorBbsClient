/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "color.h"
#include "defs.h"
#define ifzero( x ) if ( ( x ) < 0 || clearall )

/// @brief Apply the bright theme palette to the current color settings.
///
/// @return This function does not return a value.
void brilliantColors( void )
{
   color.text = 10;
   color.forum = 11;
   color.number = 14;
   color.errorTextColor = 9;
   color.ansiBlackTextColor = 10;
   color.ansiBlueTextColor = 12;
   color.ansiMagentaTextColor = 13;
   color.postDate = 13;
   color.postName = 14;
   color.postText = 10;
   color.postFriendDate = 13;
   color.postFriendName = 9;
   color.postFriendText = 10;
   color.anonymous = 11;
   color.morePrompt = 11;
   color.ansiWhiteTextColor = 15;
   color.reserved5 = 15;
   color.background = 0;
   color.inputText = 10;
   color.inputHighlight = 14;
   color.expressText = 10;
   color.expressName = 10;
   color.expressFriendName = 10;
   color.expressFriendText = 10;
}

/// @brief Apply the Catppuccin Latte palette to the current color settings.
///
/// @return This function does not return a value.
void catppuccinLatteColors( void )
{
   color.text = 240;
   color.forum = 27;
   color.number = 37;
   color.errorTextColor = 161;
   color.ansiBlackTextColor = 246;
   color.ansiBlueTextColor = 27;
   color.ansiMagentaTextColor = 99;
   color.postDate = 37;
   color.postName = 69;
   color.postText = 240;
   color.postFriendDate = 30;
   color.postFriendName = 70;
   color.postFriendText = 240;
   color.anonymous = 172;
   color.morePrompt = 172;
   color.ansiWhiteTextColor = 60;
   color.reserved5 = 240;
   color.background = 255;
   color.inputText = 240;
   color.inputHighlight = 27;
   color.expressText = 240;
   color.expressName = 27;
   color.expressFriendName = 70;
   color.expressFriendText = 240;
}

/// @brief Apply the Catppuccin Macchiato palette to the current colors.
///
/// @return This function does not return a value.
void catppuccinMacchiatoColors( void )
{
   color.text = 189;
   color.forum = 111;
   color.number = 116;
   color.errorTextColor = 210;
   color.ansiBlackTextColor = 103;
   color.ansiBlueTextColor = 111;
   color.ansiMagentaTextColor = 183;
   color.postDate = 116;
   color.postName = 147;
   color.postText = 189;
   color.postFriendDate = 116;
   color.postFriendName = 150;
   color.postFriendText = 189;
   color.anonymous = 223;
   color.morePrompt = 223;
   color.ansiWhiteTextColor = 189;
   color.reserved5 = 189;
   color.background = 0;
   color.inputText = 189;
   color.inputHighlight = 111;
   color.expressText = 189;
   color.expressName = 147;
   color.expressFriendName = 150;
   color.expressFriendText = 189;
}

/// @brief Apply the colorblind-friendly theme palette to the current colors.
///
/// @return This function does not return a value.
void colorblindColors( void )
{
   color.text = 231;
   color.forum = 75;
   color.number = 214;
   color.errorTextColor = 166;
   color.ansiBlackTextColor = 146;
   color.ansiBlueTextColor = 75;
   color.ansiMagentaTextColor = 175;
   color.postDate = 75;
   color.postName = 214;
   color.postText = 231;
   color.postFriendDate = 25;
   color.postFriendName = 175;
   color.postFriendText = 231;
   color.anonymous = 221;
   color.morePrompt = 221;
   color.ansiWhiteTextColor = 221;
   color.reserved5 = 231;
   color.background = 0;
   color.inputText = 231;
   color.inputHighlight = 214;
   color.expressText = 231;
   color.expressName = 214;
   color.expressFriendName = 175;
   color.expressFriendText = 231;
}

/// @brief Fill unset color fields with the built-in default theme values.
///
/// @param clearall When non-zero, reinitialize every color field, including the
/// background color. When zero, only unset fields are filled in.
///
/// @return This function does not return a value.
void defaultColors( int clearall )
{
   ifzero( color.text ) color.text = 2;
   ifzero( color.forum ) color.forum = 3;
   ifzero( color.number ) color.number = 6;
   ifzero( color.errorTextColor ) color.errorTextColor = 1;
   ifzero( color.ansiBlackTextColor ) color.ansiBlackTextColor = 2;
   ifzero( color.ansiBlueTextColor ) color.ansiBlueTextColor = 4;
   ifzero( color.ansiMagentaTextColor ) color.ansiMagentaTextColor = 5;
   ifzero( color.postDate ) color.postDate = 5;
   ifzero( color.postName ) color.postName = 6;
   ifzero( color.postText ) color.postText = 2;
   ifzero( color.postFriendDate ) color.postFriendDate = 5;
   ifzero( color.postFriendName ) color.postFriendName = 1;
   ifzero( color.postFriendText ) color.postFriendText = 2;
   ifzero( color.anonymous ) color.anonymous = 3;
   ifzero( color.morePrompt ) color.morePrompt = 3;
   ifzero( color.ansiWhiteTextColor ) color.ansiWhiteTextColor = 7;
   color.reserved5 = 7;
   if ( clearall )
   {
      color.background = 0;
   }
   ifzero( color.inputText ) color.inputText = 2;
   ifzero( color.inputHighlight ) color.inputHighlight = 6;
   ifzero( color.expressText ) color.expressText = 2;
   ifzero( color.expressName ) color.expressName = 2;
   ifzero( color.expressFriendName ) color.expressFriendName = 2;
   ifzero( color.expressFriendText ) color.expressFriendText = 2;
}

/// @brief Apply the Everforest dark medium palette to the current color settings.
///
/// @return This function does not return a value.
void everforestDarkColors( void )
{
   color.text = 187;
   color.forum = 144;
   color.number = 109;
   color.errorTextColor = 174;
   color.ansiBlackTextColor = 245;
   color.ansiBlueTextColor = 109;
   color.ansiMagentaTextColor = 175;
   color.postDate = 109;
   color.postName = 108;
   color.postText = 187;
   color.postFriendDate = 109;
   color.postFriendName = 144;
   color.postFriendText = 187;
   color.anonymous = 180;
   color.morePrompt = 180;
   color.ansiWhiteTextColor = 187;
   color.reserved5 = 187;
   color.background = 0;
   color.inputText = 187;
   color.inputHighlight = 109;
   color.expressText = 187;
   color.expressName = 108;
   color.expressFriendName = 144;
   color.expressFriendText = 187;
}

/// @brief Apply the Everforest light medium palette to the current color settings.
///
/// @return This function does not return a value.
void everforestLightColors( void )
{
   color.text = 242;
   color.forum = 106;
   color.number = 68;
   color.errorTextColor = 203;
   color.ansiBlackTextColor = 246;
   color.ansiBlueTextColor = 68;
   color.ansiMagentaTextColor = 169;
   color.postDate = 68;
   color.postName = 72;
   color.postText = 242;
   color.postFriendDate = 68;
   color.postFriendName = 107;
   color.postFriendText = 242;
   color.anonymous = 178;
   color.morePrompt = 178;
   color.ansiWhiteTextColor = 242;
   color.reserved5 = 242;
   color.background = 230;
   color.inputText = 242;
   color.inputHighlight = 68;
   color.expressText = 242;
   color.expressName = 72;
   color.expressFriendName = 107;
   color.expressFriendText = 242;
}

/// @brief Apply the Gruvbox dark hard palette to the current color settings.
///
/// This preset intentionally leans on Gruvbox's softer aqua, purple, yellow,
/// and olive-green tones instead of the more aggressive red/orange accents.
///
/// @return This function does not return a value.
void gruvboxDarkColors( void )
{
   color.text = 223;
   color.forum = 142;
   color.number = 109;
   color.errorTextColor = 214;
   color.ansiBlackTextColor = 245;
   color.ansiBlueTextColor = 109;
   color.ansiMagentaTextColor = 175;
   color.postDate = 109;
   color.postName = 142;
   color.postText = 223;
   color.postFriendDate = 175;
   color.postFriendName = 108;
   color.postFriendText = 223;
   color.anonymous = 214;
   color.morePrompt = 214;
   color.ansiWhiteTextColor = 223;
   color.reserved5 = 223;
   color.background = 0;
   color.inputText = 223;
   color.inputHighlight = 109;
   color.expressText = 223;
   color.expressName = 142;
   color.expressFriendName = 108;
   color.expressFriendText = 223;
}

/// @brief Apply the Gruvbox light hard palette to the current color settings.
///
/// This preset keeps the same quieter accent bias as the dark palette while
/// switching to Gruvbox's light background and softer neutral text.
///
/// @return This function does not return a value.
void gruvboxLightColors( void )
{
   color.text = 239;
   color.forum = 100;
   color.number = 66;
   color.errorTextColor = 172;
   color.ansiBlackTextColor = 246;
   color.ansiBlueTextColor = 66;
   color.ansiMagentaTextColor = 132;
   color.postDate = 66;
   color.postName = 100;
   color.postText = 239;
   color.postFriendDate = 132;
   color.postFriendName = 107;
   color.postFriendText = 239;
   color.anonymous = 172;
   color.morePrompt = 172;
   color.ansiWhiteTextColor = 239;
   color.reserved5 = 239;
   color.background = 230;
   color.inputText = 239;
   color.inputHighlight = 66;
   color.expressText = 239;
   color.expressName = 100;
   color.expressFriendName = 107;
   color.expressFriendText = 239;
}

/// @brief Apply the hot dog theme palette to the current color settings.
///
/// @return This function does not return a value.
void hotDogColors( void )
{
   color.text = 220;
   color.forum = 196;
   color.number = 220;
   color.errorTextColor = 231;
   color.ansiBlackTextColor = 130;
   color.ansiBlueTextColor = 214;
   color.ansiMagentaTextColor = 130;
   color.postDate = 226;
   color.postName = 226;
   color.postText = 214;
   color.postFriendDate = 226;
   color.postFriendName = 226;
   color.postFriendText = 214;
   color.anonymous = 226;
   color.morePrompt = 220;
   color.ansiWhiteTextColor = 220;
   color.reserved5 = 130;
   color.background = 0;
   color.inputText = 220;
   color.inputHighlight = 231;
   color.expressText = 214;
   color.expressName = 226;
   color.expressFriendName = 226;
   color.expressFriendText = 214;
}
