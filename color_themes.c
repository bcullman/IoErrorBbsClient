/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client_globals.h"
#include "color.h"
#include "defs.h"

#define RGB_CONST( red, green, blue ) \
   ( COLOR_VALUE_RGB_FLAG | ( ( red ) << 16 ) | ( ( green ) << 8 ) | ( blue ) )

typedef struct
{
   Color colors;
   bool useBlackThemeBackgrounds;
} ColorThemePalette;

static void applyDefaultColorValuesToActiveColor( const ColorThemePalette *ptrPalette,
                                                  int clearall );
static void applyThemeAndDerive256( const ColorThemePalette *ptrPalette );
static void applyThemePaletteToActiveColor( const ColorThemePalette *ptrPalette );
static bool shouldApplyDefaultColor( int colorValue, int clearall );

static const ColorThemePalette BRILLIANT_THEME =
   { .colors = { .text = RGB_CONST( 0x00, 0xff, 0x00 ),
                 .forum = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .number = RGB_CONST( 0x00, 0xff, 0xff ),
                 .errorTextColor = RGB_CONST( 0xff, 0x00, 0x00 ),
                 .ansiBlackTextColor = RGB_CONST( 0x00, 0xff, 0x00 ),
                 .ansiBlueTextColor = RGB_CONST( 0x00, 0x00, 0xff ),
                 .ansiMagentaTextColor = RGB_CONST( 0xff, 0x00, 0xff ),
                 .postDate = RGB_CONST( 0xff, 0x00, 0xff ),
                 .postName = RGB_CONST( 0x00, 0xff, 0xff ),
                 .postText = RGB_CONST( 0x00, 0xff, 0x00 ),
                 .postFriendDate = RGB_CONST( 0xff, 0x00, 0xff ),
                 .postFriendName = RGB_CONST( 0xff, 0x00, 0x00 ),
                 .postFriendText = RGB_CONST( 0x00, 0xff, 0x00 ),
                 .anonymous = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .morePrompt = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .ansiWhiteTextColor = RGB_CONST( 0xff, 0xff, 0xff ),
                 .reserved5 = RGB_CONST( 0xff, 0xff, 0xff ),
                 .background = 0,
                 .inputText = RGB_CONST( 0x00, 0xff, 0x00 ),
                 .inputHighlight = RGB_CONST( 0x00, 0xff, 0xff ),
                 .expressText = RGB_CONST( 0x00, 0xff, 0x00 ),
                 .expressName = RGB_CONST( 0x00, 0xff, 0x00 ),
                 .expressFriendName = RGB_CONST( 0x00, 0xff, 0x00 ),
                 .expressFriendText = RGB_CONST( 0x00, 0xff, 0x00 ) },
     .useBlackThemeBackgrounds = false };

static const ColorThemePalette CATPPUCCIN_LATTE_THEME =
   { .colors = { .text = RGB_CONST( 0x4c, 0x4f, 0x69 ),
                 .forum = RGB_CONST( 0x1e, 0x66, 0xf5 ),
                 .number = RGB_CONST( 0x20, 0x9f, 0xb5 ),
                 .errorTextColor = RGB_CONST( 0xd2, 0x0f, 0x39 ),
                 .ansiBlackTextColor = RGB_CONST( 0x9c, 0xa0, 0xb0 ),
                 .ansiBlueTextColor = RGB_CONST( 0x1e, 0x66, 0xf5 ),
                 .ansiMagentaTextColor = RGB_CONST( 0xea, 0x76, 0xcb ),
                 .postDate = RGB_CONST( 0x20, 0x9f, 0xb5 ),
                 .postName = RGB_CONST( 0x72, 0x87, 0xfd ),
                 .postText = RGB_CONST( 0x4c, 0x4f, 0x69 ),
                 .postFriendDate = RGB_CONST( 0x17, 0x92, 0x99 ),
                 .postFriendName = RGB_CONST( 0x40, 0xa0, 0x2b ),
                 .postFriendText = RGB_CONST( 0x4c, 0x4f, 0x69 ),
                 .anonymous = RGB_CONST( 0xfe, 0x64, 0x0b ),
                 .morePrompt = RGB_CONST( 0xdf, 0x8e, 0x1d ),
                 .ansiWhiteTextColor = RGB_CONST( 0x5c, 0x5f, 0x77 ),
                 .reserved5 = RGB_CONST( 0x4c, 0x4f, 0x69 ),
                 .background = RGB_CONST( 0xef, 0xf1, 0xf5 ),
                 .inputText = RGB_CONST( 0x4c, 0x4f, 0x69 ),
                 .inputHighlight = RGB_CONST( 0x1e, 0x66, 0xf5 ),
                 .expressText = RGB_CONST( 0x4c, 0x4f, 0x69 ),
                 .expressName = RGB_CONST( 0x1e, 0x66, 0xf5 ),
                 .expressFriendName = RGB_CONST( 0x40, 0xa0, 0x2b ),
                 .expressFriendText = RGB_CONST( 0x4c, 0x4f, 0x69 ) },
     .useBlackThemeBackgrounds = false };

static const ColorThemePalette CATPPUCCIN_MACCHIATO_THEME =
   { .colors = { .text = RGB_CONST( 0xca, 0xd3, 0xf5 ),
                 .forum = RGB_CONST( 0x8a, 0xad, 0xf4 ),
                 .number = RGB_CONST( 0x7d, 0xc4, 0xe4 ),
                 .errorTextColor = RGB_CONST( 0xed, 0x87, 0x96 ),
                 .ansiBlackTextColor = RGB_CONST( 0x80, 0x84, 0x9d ),
                 .ansiBlueTextColor = RGB_CONST( 0x8a, 0xad, 0xf4 ),
                 .ansiMagentaTextColor = RGB_CONST( 0xc6, 0xa0, 0xf6 ),
                 .postDate = RGB_CONST( 0x7d, 0xc4, 0xe4 ),
                 .postName = RGB_CONST( 0xb7, 0xbd, 0xf8 ),
                 .postText = RGB_CONST( 0xca, 0xd3, 0xf5 ),
                 .postFriendDate = RGB_CONST( 0x8b, 0xd5, 0xca ),
                 .postFriendName = RGB_CONST( 0xa6, 0xda, 0x95 ),
                 .postFriendText = RGB_CONST( 0xca, 0xd3, 0xf5 ),
                 .anonymous = RGB_CONST( 0xf5, 0xa9, 0x7f ),
                 .morePrompt = RGB_CONST( 0xee, 0xd4, 0x9f ),
                 .ansiWhiteTextColor = RGB_CONST( 0xca, 0xd3, 0xf5 ),
                 .reserved5 = RGB_CONST( 0xca, 0xd3, 0xf5 ),
                 .background = RGB_CONST( 0x24, 0x27, 0x3a ),
                 .inputText = RGB_CONST( 0xca, 0xd3, 0xf5 ),
                 .inputHighlight = RGB_CONST( 0x8a, 0xad, 0xf4 ),
                 .expressText = RGB_CONST( 0xca, 0xd3, 0xf5 ),
                 .expressName = RGB_CONST( 0xb7, 0xbd, 0xf8 ),
                 .expressFriendName = RGB_CONST( 0xa6, 0xda, 0x95 ),
                 .expressFriendText = RGB_CONST( 0xca, 0xd3, 0xf5 ) },
     .useBlackThemeBackgrounds = true };

static const ColorThemePalette COLORBLIND_THEME =
   { .colors = { .text = RGB_CONST( 0xff, 0xff, 0xff ),
                 .forum = RGB_CONST( 0x5f, 0xaf, 0xff ),
                 .number = RGB_CONST( 0xff, 0xaf, 0x00 ),
                 .errorTextColor = RGB_CONST( 0xd7, 0x5f, 0x00 ),
                 .ansiBlackTextColor = RGB_CONST( 0xaf, 0xaf, 0xd7 ),
                 .ansiBlueTextColor = RGB_CONST( 0x5f, 0xaf, 0xff ),
                 .ansiMagentaTextColor = RGB_CONST( 0xd7, 0x87, 0xaf ),
                 .postDate = RGB_CONST( 0x5f, 0xaf, 0xff ),
                 .postName = RGB_CONST( 0xff, 0xaf, 0x00 ),
                 .postText = RGB_CONST( 0xff, 0xff, 0xff ),
                 .postFriendDate = RGB_CONST( 0x00, 0x5f, 0xaf ),
                 .postFriendName = RGB_CONST( 0xd7, 0x87, 0xaf ),
                 .postFriendText = RGB_CONST( 0xff, 0xff, 0xff ),
                 .anonymous = RGB_CONST( 0xff, 0xd7, 0x5f ),
                 .morePrompt = RGB_CONST( 0xff, 0xd7, 0x5f ),
                 .ansiWhiteTextColor = RGB_CONST( 0xff, 0xd7, 0x5f ),
                 .reserved5 = RGB_CONST( 0xff, 0xff, 0xff ),
                 .background = 0,
                 .inputText = RGB_CONST( 0xff, 0xff, 0xff ),
                 .inputHighlight = RGB_CONST( 0xff, 0xaf, 0x00 ),
                 .expressText = RGB_CONST( 0xff, 0xff, 0xff ),
                 .expressName = RGB_CONST( 0xff, 0xaf, 0x00 ),
                 .expressFriendName = RGB_CONST( 0xd7, 0x87, 0xaf ),
                 .expressFriendText = RGB_CONST( 0xff, 0xff, 0xff ) },
     .useBlackThemeBackgrounds = false };

static const ColorThemePalette DEFAULT_256_THEME =
   { .colors = { .text = 2,
                 .forum = 3,
                 .number = 6,
                 .errorTextColor = 1,
                 .ansiBlackTextColor = 2,
                 .ansiBlueTextColor = 4,
                 .ansiMagentaTextColor = 5,
                 .postDate = 5,
                 .postName = 6,
                 .postText = 2,
                 .postFriendDate = 5,
                 .postFriendName = 1,
                 .postFriendText = 2,
                 .anonymous = 3,
                 .morePrompt = 3,
                 .ansiWhiteTextColor = 7,
                 .reserved5 = 7,
                 .background = 0,
                 .inputText = 2,
                 .inputHighlight = 6,
                 .expressText = 2,
                 .expressName = 2,
                 .expressFriendText = 2,
                 .expressFriendName = 2 },
     .useBlackThemeBackgrounds = false };

static const ColorThemePalette DEFAULT_TRUECOLOR_THEME =
   { .colors = { .text = RGB_CONST( 0x00, 0x80, 0x00 ),
                 .forum = RGB_CONST( 0x80, 0x80, 0x00 ),
                 .number = RGB_CONST( 0x00, 0x80, 0x80 ),
                 .errorTextColor = RGB_CONST( 0x80, 0x00, 0x00 ),
                 .ansiBlackTextColor = RGB_CONST( 0x00, 0x80, 0x00 ),
                 .ansiBlueTextColor = RGB_CONST( 0x00, 0x00, 0x80 ),
                 .ansiMagentaTextColor = RGB_CONST( 0x80, 0x00, 0x80 ),
                 .postDate = RGB_CONST( 0x80, 0x00, 0x80 ),
                 .postName = RGB_CONST( 0x00, 0x80, 0x80 ),
                 .postText = RGB_CONST( 0x00, 0x80, 0x00 ),
                 .postFriendDate = RGB_CONST( 0x80, 0x00, 0x80 ),
                 .postFriendName = RGB_CONST( 0x80, 0x00, 0x00 ),
                 .postFriendText = RGB_CONST( 0x00, 0x80, 0x00 ),
                 .anonymous = RGB_CONST( 0x80, 0x80, 0x00 ),
                 .morePrompt = RGB_CONST( 0x80, 0x80, 0x00 ),
                 .ansiWhiteTextColor = RGB_CONST( 0xc0, 0xc0, 0xc0 ),
                 .reserved5 = RGB_CONST( 0xc0, 0xc0, 0xc0 ),
                 .background = 0,
                 .inputText = RGB_CONST( 0x00, 0x80, 0x00 ),
                 .inputHighlight = RGB_CONST( 0x00, 0x80, 0x80 ),
                 .expressText = RGB_CONST( 0x00, 0x80, 0x00 ),
                 .expressName = RGB_CONST( 0x00, 0x80, 0x00 ),
                 .expressFriendName = RGB_CONST( 0x00, 0x80, 0x00 ),
                 .expressFriendText = RGB_CONST( 0x00, 0x80, 0x00 ) },
     .useBlackThemeBackgrounds = false };

static const ColorThemePalette DRACULA_THEME =
   { .colors = { .text = RGB_CONST( 0xe3, 0xe2, 0xe9 ),
                 .forum = RGB_CONST( 0x73, 0x59, 0xf8 ),
                 .number = RGB_CONST( 0x5c, 0xf5, 0xdb ),
                 .errorTextColor = RGB_CONST( 0xf8, 0x73, 0x59 ),
                 .ansiBlackTextColor = RGB_CONST( 0x73, 0x59, 0xf8 ),
                 .ansiBlueTextColor = RGB_CONST( 0x5c, 0xf5, 0xdb ),
                 .ansiMagentaTextColor = RGB_CONST( 0xf8, 0x59, 0xa8 ),
                 .postDate = RGB_CONST( 0x5c, 0xf5, 0xdb ),
                 .postName = RGB_CONST( 0xf8, 0x59, 0xa8 ),
                 .postText = RGB_CONST( 0xe3, 0xe2, 0xe9 ),
                 .postFriendDate = RGB_CONST( 0x73, 0x59, 0xf8 ),
                 .postFriendName = RGB_CONST( 0x66, 0xf8, 0x59 ),
                 .postFriendText = RGB_CONST( 0xe3, 0xe2, 0xe9 ),
                 .anonymous = RGB_CONST( 0xf8, 0xf8, 0x59 ),
                 .morePrompt = RGB_CONST( 0xf8, 0xf8, 0x59 ),
                 .ansiWhiteTextColor = RGB_CONST( 0xe3, 0xe2, 0xe9 ),
                 .reserved5 = RGB_CONST( 0xe3, 0xe2, 0xe9 ),
                 .background = 0,
                 .inputText = RGB_CONST( 0xe3, 0xe2, 0xe9 ),
                 .inputHighlight = RGB_CONST( 0x5c, 0xf5, 0xdb ),
                 .expressText = RGB_CONST( 0xe3, 0xe2, 0xe9 ),
                 .expressName = RGB_CONST( 0xf8, 0x59, 0xa8 ),
                 .expressFriendName = RGB_CONST( 0x66, 0xf8, 0x59 ),
                 .expressFriendText = RGB_CONST( 0xe3, 0xe2, 0xe9 ) },
     .useBlackThemeBackgrounds = true };

static const ColorThemePalette EVERFOREST_DARK_THEME =
   { .colors = { .text = RGB_CONST( 0xd3, 0xc6, 0xaa ),
                 .forum = RGB_CONST( 0x7f, 0xbb, 0xb3 ),
                 .number = RGB_CONST( 0x83, 0xc0, 0x92 ),
                 .errorTextColor = RGB_CONST( 0xe6, 0x7e, 0x80 ),
                 .ansiBlackTextColor = RGB_CONST( 0x85, 0x92, 0x89 ),
                 .ansiBlueTextColor = RGB_CONST( 0x7f, 0xbb, 0xb3 ),
                 .ansiMagentaTextColor = RGB_CONST( 0xd6, 0x99, 0xb6 ),
                 .postDate = RGB_CONST( 0x83, 0xc0, 0x92 ),
                 .postName = RGB_CONST( 0xa7, 0xc0, 0x80 ),
                 .postText = RGB_CONST( 0xd3, 0xc6, 0xaa ),
                 .postFriendDate = RGB_CONST( 0x7f, 0xbb, 0xb3 ),
                 .postFriendName = RGB_CONST( 0xdb, 0xbc, 0x7f ),
                 .postFriendText = RGB_CONST( 0xd3, 0xc6, 0xaa ),
                 .anonymous = RGB_CONST( 0xe6, 0x98, 0x75 ),
                 .morePrompt = RGB_CONST( 0xdb, 0xbc, 0x7f ),
                 .ansiWhiteTextColor = RGB_CONST( 0xe5, 0xdd, 0xc9 ),
                 .reserved5 = RGB_CONST( 0xd3, 0xc6, 0xaa ),
                 .background = RGB_CONST( 0x2f, 0x38, 0x3e ),
                 .inputText = RGB_CONST( 0xd3, 0xc6, 0xaa ),
                 .inputHighlight = RGB_CONST( 0x7f, 0xbb, 0xb3 ),
                 .expressText = RGB_CONST( 0xd3, 0xc6, 0xaa ),
                 .expressName = RGB_CONST( 0xa7, 0xc0, 0x80 ),
                 .expressFriendName = RGB_CONST( 0xdb, 0xbc, 0x7f ),
                 .expressFriendText = RGB_CONST( 0xd3, 0xc6, 0xaa ) },
     .useBlackThemeBackgrounds = true };

static const ColorThemePalette EVERFOREST_LIGHT_THEME =
   { .colors = { .text = RGB_CONST( 0x5c, 0x6a, 0x72 ),
                 .forum = RGB_CONST( 0x35, 0x8f, 0xa2 ),
                 .number = RGB_CONST( 0x3a, 0x94, 0x84 ),
                 .errorTextColor = RGB_CONST( 0xf8, 0x55, 0x52 ),
                 .ansiBlackTextColor = RGB_CONST( 0xa6, 0xb0, 0x9f ),
                 .ansiBlueTextColor = RGB_CONST( 0x35, 0x8f, 0xa2 ),
                 .ansiMagentaTextColor = RGB_CONST( 0xdf, 0x69, 0xba ),
                 .postDate = RGB_CONST( 0x3a, 0x94, 0x84 ),
                 .postName = RGB_CONST( 0x8d, 0xb8, 0x61 ),
                 .postText = RGB_CONST( 0x5c, 0x6a, 0x72 ),
                 .postFriendDate = RGB_CONST( 0x35, 0x8f, 0xa2 ),
                 .postFriendName = RGB_CONST( 0xda, 0xa5, 0x20 ),
                 .postFriendText = RGB_CONST( 0x5c, 0x6a, 0x72 ),
                 .anonymous = RGB_CONST( 0xf5, 0x7d, 0x26 ),
                 .morePrompt = RGB_CONST( 0xbf, 0x98, 0x3d ),
                 .ansiWhiteTextColor = RGB_CONST( 0x4f, 0x5b, 0x58 ),
                 .reserved5 = RGB_CONST( 0x5c, 0x6a, 0x72 ),
                 .background = RGB_CONST( 0xfd, 0xf6, 0xe3 ),
                 .inputText = RGB_CONST( 0x5c, 0x6a, 0x72 ),
                 .inputHighlight = RGB_CONST( 0x35, 0x8f, 0xa2 ),
                 .expressText = RGB_CONST( 0x5c, 0x6a, 0x72 ),
                 .expressName = RGB_CONST( 0x8d, 0xb8, 0x61 ),
                 .expressFriendName = RGB_CONST( 0xda, 0xa5, 0x20 ),
                 .expressFriendText = RGB_CONST( 0x5c, 0x6a, 0x72 ) },
     .useBlackThemeBackgrounds = false };

static const ColorThemePalette GRUVBOX_DARK_THEME =
   { .colors = { .text = RGB_CONST( 0xeb, 0xdb, 0xb2 ),
                 .forum = RGB_CONST( 0x83, 0xa5, 0x98 ),
                 .number = RGB_CONST( 0x8e, 0xc0, 0x7c ),
                 .errorTextColor = RGB_CONST( 0xfe, 0x80, 0x19 ),
                 .ansiBlackTextColor = RGB_CONST( 0x92, 0x83, 0x74 ),
                 .ansiBlueTextColor = RGB_CONST( 0x83, 0xa5, 0x98 ),
                 .ansiMagentaTextColor = RGB_CONST( 0xd3, 0x86, 0x9b ),
                 .postDate = RGB_CONST( 0x83, 0xa5, 0x98 ),
                 .postName = RGB_CONST( 0xb8, 0xbb, 0x26 ),
                 .postText = RGB_CONST( 0xeb, 0xdb, 0xb2 ),
                 .postFriendDate = RGB_CONST( 0xd3, 0x86, 0x9b ),
                 .postFriendName = RGB_CONST( 0x8e, 0xc0, 0x7c ),
                 .postFriendText = RGB_CONST( 0xeb, 0xdb, 0xb2 ),
                 .anonymous = RGB_CONST( 0xfe, 0x80, 0x19 ),
                 .morePrompt = RGB_CONST( 0xfa, 0xbd, 0x2f ),
                 .ansiWhiteTextColor = RGB_CONST( 0xfb, 0xf1, 0xc7 ),
                 .reserved5 = RGB_CONST( 0xeb, 0xdb, 0xb2 ),
                 .background = RGB_CONST( 0x1d, 0x20, 0x21 ),
                 .inputText = RGB_CONST( 0xeb, 0xdb, 0xb2 ),
                 .inputHighlight = RGB_CONST( 0x83, 0xa5, 0x98 ),
                 .expressText = RGB_CONST( 0xeb, 0xdb, 0xb2 ),
                 .expressName = RGB_CONST( 0xb8, 0xbb, 0x26 ),
                 .expressFriendName = RGB_CONST( 0x8e, 0xc0, 0x7c ),
                 .expressFriendText = RGB_CONST( 0xeb, 0xdb, 0xb2 ) },
     .useBlackThemeBackgrounds = true };

static const ColorThemePalette GRUVBOX_LIGHT_THEME =
   { .colors = { .text = RGB_CONST( 0x3c, 0x38, 0x36 ),
                 .forum = RGB_CONST( 0x45, 0x85, 0x88 ),
                 .number = RGB_CONST( 0x68, 0x9d, 0x6a ),
                 .errorTextColor = RGB_CONST( 0xd6, 0x5d, 0x0e ),
                 .ansiBlackTextColor = RGB_CONST( 0xa8, 0x99, 0x84 ),
                 .ansiBlueTextColor = RGB_CONST( 0x45, 0x85, 0x88 ),
                 .ansiMagentaTextColor = RGB_CONST( 0xb1, 0x62, 0x86 ),
                 .postDate = RGB_CONST( 0x45, 0x85, 0x88 ),
                 .postName = RGB_CONST( 0x79, 0x74, 0x0e ),
                 .postText = RGB_CONST( 0x3c, 0x38, 0x36 ),
                 .postFriendDate = RGB_CONST( 0xb1, 0x62, 0x86 ),
                 .postFriendName = RGB_CONST( 0x68, 0x9d, 0x6a ),
                 .postFriendText = RGB_CONST( 0x3c, 0x38, 0x36 ),
                 .anonymous = RGB_CONST( 0xaf, 0x3a, 0x03 ),
                 .morePrompt = RGB_CONST( 0xd7, 0x99, 0x21 ),
                 .ansiWhiteTextColor = RGB_CONST( 0x28, 0x28, 0x28 ),
                 .reserved5 = RGB_CONST( 0x3c, 0x38, 0x36 ),
                 .background = RGB_CONST( 0xf9, 0xf5, 0xd7 ),
                 .inputText = RGB_CONST( 0x3c, 0x38, 0x36 ),
                 .inputHighlight = RGB_CONST( 0x45, 0x85, 0x88 ),
                 .expressText = RGB_CONST( 0x3c, 0x38, 0x36 ),
                 .expressName = RGB_CONST( 0x79, 0x74, 0x0e ),
                 .expressFriendName = RGB_CONST( 0x68, 0x9d, 0x6a ),
                 .expressFriendText = RGB_CONST( 0x3c, 0x38, 0x36 ) },
     .useBlackThemeBackgrounds = false };

static const ColorThemePalette HOT_DOG_THEME =
   { .colors = { .text = RGB_CONST( 0xff, 0xd7, 0x00 ),
                 .forum = RGB_CONST( 0xff, 0x00, 0x00 ),
                 .number = RGB_CONST( 0xff, 0xd7, 0x00 ),
                 .errorTextColor = RGB_CONST( 0xff, 0xff, 0xff ),
                 .ansiBlackTextColor = RGB_CONST( 0xaf, 0x5f, 0x00 ),
                 .ansiBlueTextColor = RGB_CONST( 0xff, 0xaf, 0x00 ),
                 .ansiMagentaTextColor = RGB_CONST( 0xaf, 0x5f, 0x00 ),
                 .postDate = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .postName = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .postText = RGB_CONST( 0xff, 0xaf, 0x00 ),
                 .postFriendDate = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .postFriendName = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .postFriendText = RGB_CONST( 0xff, 0xaf, 0x00 ),
                 .anonymous = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .morePrompt = RGB_CONST( 0xff, 0xd7, 0x00 ),
                 .ansiWhiteTextColor = RGB_CONST( 0xff, 0xd7, 0x00 ),
                 .reserved5 = RGB_CONST( 0xaf, 0x5f, 0x00 ),
                 .background = 0,
                 .inputText = RGB_CONST( 0xff, 0xd7, 0x00 ),
                 .inputHighlight = RGB_CONST( 0xff, 0xff, 0xff ),
                 .expressText = RGB_CONST( 0xff, 0xaf, 0x00 ),
                 .expressName = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .expressFriendName = RGB_CONST( 0xff, 0xff, 0x00 ),
                 .expressFriendText = RGB_CONST( 0xff, 0xaf, 0x00 ) },
     .useBlackThemeBackgrounds = false };

static const ColorThemePalette TIDAL_REEF_THEME =
   { .colors = { .text = RGB_CONST( 0xea, 0xf6, 0xad ),
                 .forum = RGB_CONST( 0x1b, 0x77, 0x8c ),
                 .number = RGB_CONST( 0x6d, 0x93, 0xea ),
                 .errorTextColor = RGB_CONST( 0x6a, 0x2f, 0xee ),
                 .ansiBlackTextColor = RGB_CONST( 0x1b, 0x77, 0x8c ),
                 .ansiBlueTextColor = RGB_CONST( 0x6d, 0x93, 0xea ),
                 .ansiMagentaTextColor = RGB_CONST( 0x6a, 0x2f, 0xee ),
                 .postDate = RGB_CONST( 0x6d, 0x93, 0xea ),
                 .postName = RGB_CONST( 0xb6, 0xdb, 0x00 ),
                 .postText = RGB_CONST( 0xea, 0xf6, 0xad ),
                 .postFriendDate = RGB_CONST( 0x1b, 0x77, 0x8c ),
                 .postFriendName = RGB_CONST( 0x6a, 0x2f, 0xee ),
                 .postFriendText = RGB_CONST( 0xea, 0xf6, 0xad ),
                 .anonymous = RGB_CONST( 0xb6, 0xdb, 0x00 ),
                 .morePrompt = RGB_CONST( 0xb6, 0xdb, 0x00 ),
                 .ansiWhiteTextColor = RGB_CONST( 0xea, 0xf6, 0xad ),
                 .reserved5 = RGB_CONST( 0xea, 0xf6, 0xad ),
                 .background = 0,
                 .inputText = RGB_CONST( 0xea, 0xf6, 0xad ),
                 .inputHighlight = RGB_CONST( 0x6d, 0x93, 0xea ),
                 .expressText = RGB_CONST( 0xea, 0xf6, 0xad ),
                 .expressName = RGB_CONST( 0xb6, 0xdb, 0x00 ),
                 .expressFriendName = RGB_CONST( 0x6a, 0x2f, 0xee ),
                 .expressFriendText = RGB_CONST( 0xea, 0xf6, 0xad ) },
     .useBlackThemeBackgrounds = true };

/// @brief Apply the bright theme palette to the configured color tables.
///
/// @return This function does not return a value.
void brilliantColors( void )
{
   applyThemeAndDerive256( &BRILLIANT_THEME );
}

/// @brief Apply the Catppuccin Latte palette to the configured color tables.
///
/// @return This function does not return a value.
void catppuccinLatteColors( void )
{
   applyThemeAndDerive256( &CATPPUCCIN_LATTE_THEME );
}

/// @brief Apply the Catppuccin Macchiato palette to the configured color tables.
///
/// @return This function does not return a value.
void catppuccinMacchiatoColors( void )
{
   applyThemeAndDerive256( &CATPPUCCIN_MACCHIATO_THEME );
}

/// @brief Apply the colorblind-friendly theme palette to the configured color tables.
///
/// @return This function does not return a value.
void colorblindColors( void )
{
   applyThemeAndDerive256( &COLORBLIND_THEME );
}

/// @brief Fill unset color fields with the built-in default theme values.
///
/// @param clearall When non-zero, reinitialize every color field, including the
/// background color. When zero, only unset fields are filled in.
///
/// @return This function does not return a value.
void defaultColors( int clearall )
{
   color = color256;
   useBlackThemeBackgrounds = useBlackThemeBackgrounds256;
   applyDefaultColorValuesToActiveColor( &DEFAULT_256_THEME, clearall );
   copyColorTable( &color256, &color );
   useBlackThemeBackgrounds256 = useBlackThemeBackgrounds;

   color = colorTruecolor;
   useBlackThemeBackgrounds = useBlackThemeBackgroundsTruecolor;
   applyDefaultColorValuesToActiveColor( &DEFAULT_TRUECOLOR_THEME, clearall );
   copyColorTable( &colorTruecolor, &color );
   useBlackThemeBackgroundsTruecolor = useBlackThemeBackgrounds;

   refreshActiveColorTable();
}

/// @brief Apply the Dracula dark palette to the configured color tables.
///
/// @return This function does not return a value.
void draculaProColors( void )
{
   applyThemeAndDerive256( &DRACULA_THEME );
}

/// @brief Apply the Everforest dark medium palette to the configured color tables.
///
/// @return This function does not return a value.
void everforestDarkColors( void )
{
   applyThemeAndDerive256( &EVERFOREST_DARK_THEME );
}

/// @brief Apply the Everforest light medium palette to the configured color tables.
///
/// @return This function does not return a value.
void everforestLightColors( void )
{
   applyThemeAndDerive256( &EVERFOREST_LIGHT_THEME );
}

/// @brief Apply the Gruvbox dark hard palette to the configured color tables.
///
/// @return This function does not return a value.
void gruvboxDarkColors( void )
{
   applyThemeAndDerive256( &GRUVBOX_DARK_THEME );
}

/// @brief Apply the Gruvbox light hard palette to the configured color tables.
///
/// @return This function does not return a value.
void gruvboxLightColors( void )
{
   applyThemeAndDerive256( &GRUVBOX_LIGHT_THEME );
}

/// @brief Apply the hot dog theme palette to the configured color tables.
///
/// @return This function does not return a value.
void hotDogColors( void )
{
   applyThemeAndDerive256( &HOT_DOG_THEME );
}

/// @brief Apply the Tidal Reef dark palette to the configured color tables.
///
/// @return This function does not return a value.
void tidalReefColors( void )
{
   applyThemeAndDerive256( &TIDAL_REEF_THEME );
}

/// @brief Fill missing live-palette fields from the supplied default palette.
///
/// @param ptrPalette Default color values to apply.
/// @param clearall When non-zero, reinitialize every configurable color field.
///
/// @return This helper does not return a value.
static void applyDefaultColorValuesToActiveColor( const ColorThemePalette *ptrPalette,
                                                  int clearall )
{
   int colorIndex;

   useBlackThemeBackgrounds = ptrPalette->useBlackThemeBackgrounds;
   for ( colorIndex = 0; colorIndex < COLOR_FIELD_COUNT; colorIndex++ )
   {
      if ( colorIndex == COLOR_FIELD_BACKGROUND && !clearall )
      {
         continue;
      }
      if ( colorIndex == COLOR_FIELD_RESERVED5 ||
           shouldApplyDefaultColor( colorFieldValue( colorIndex ), clearall ) )
      {
         setColorFieldValue(
            colorIndex,
            colorFieldValueForColor( &ptrPalette->colors, colorIndex ) );
      }
   }
}

/// @brief Apply one preset to the truecolor table and derive its 256-color table.
///
/// @param ptrPalette Theme palette to copy into the active `color` table.
///
/// @return This helper does not return a value.
static void applyThemeAndDerive256( const ColorThemePalette *ptrPalette )
{
   applyThemePaletteToActiveColor( ptrPalette );
   copyColorTable( &colorTruecolor, &color );
   useBlackThemeBackgroundsTruecolor = useBlackThemeBackgrounds;

   rebuildConfiguredColorTables( false, true, NULL );
   useBlackThemeBackgrounds256 = useBlackThemeBackgroundsTruecolor;
   refreshActiveColorTable();
}

/// @brief Copy one static theme palette into the live color table.
///
/// @param ptrPalette Theme palette to apply.
///
/// @return This helper does not return a value.
static void applyThemePaletteToActiveColor( const ColorThemePalette *ptrPalette )
{
   color = ptrPalette->colors;
   useBlackThemeBackgrounds = ptrPalette->useBlackThemeBackgrounds;
}

/// @brief Check whether one color field should be replaced by a default value.
///
/// @param colorValue Current configured value for the field.
/// @param clearall When non-zero, all fields should be reinitialized.
///
/// @return `true` when the default should be assigned, otherwise `false`.
static bool shouldApplyDefaultColor( int colorValue, int clearall )
{
   return colorValue < 0 || clearall;
}
