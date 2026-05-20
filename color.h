/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef COLOR_H_INCLUDED
#define COLOR_H_INCLUDED

#include "defs.h"

int ansiTransform( int inputChar );
void ansiTransformExpress( char *ptrText, size_t size );
int ansiTransformPost( int inputChar, int isFriend );
void ansiTransformPostHeader( char *ptrText, size_t bufferSize, int isFriend );
void brilliantColors( void );
void catppuccinLatteColors( void );
void catppuccinMacchiatoColors( void );
int colorEditorChannelValueWithinRgbRange( int value );
void colorblindColors( void );
void colorConfig( void );
void copyColorTable( Color *ptrDestination, const Color *ptrSource );
void commitActiveColorEditorState( void );
int cycleColorEditorPaletteValue( int colorValue, int delta, bool shouldAllowDefaultValue );
int colorFieldValue( int colorIndex );
int colorFieldValueForColor( const Color *ptrColor, int colorIndex );
const char *colorFieldTomlKeyName( int colorIndex );
int colorize( const char *str );
const char *colorNameFromValue( int colorValue );
const char *colorOutputModeName( ColorOutputMode outputMode );
void colorOptions( void );
int colorValueFromHexString( const char *ptrColorText );
int colorValueFromLegacyDigit( int inputChar );
int colorValueFromName( const char *ptrColorName );
int colorValueToLegacyDigit( int colorValue );
void defaultColors( int clearall );
void draculaProColors( void );
void everforestDarkColors( void );
void everforestLightColors( void );
void expressColorConfig( void );
char expressColorMenu( void );
void expressFriendColorConfig( void );
void expressUserColorConfig( void );
int formatTransformedAnsiForegroundSequence( char *ptrBuffer, size_t bufferSize,
                                             int inputChar, int isPostContext,
                                             int isFriend );
void generalColorConfig( void );
char generalColorMenu( void );
void gruvboxDarkColors( void );
void gruvboxLightColors( void );
void hotDogColors( void );
void inputColorConfig( void );
void postColorConfig( void );
char postColorMenu( void );
void postFriendColorConfig( void );
void postUserColorConfig( void );
void printAnsiBackgroundColorValue( int colorValue );
void printAnsiDisplayStateValue( int foregroundColor, int backgroundColor );
void printAnsiForegroundColorValue( int colorValue );
void printAnsiResetValue( void );
void printThemedMnemonicText( const char *ptrText, int defaultColor );
void restoreActiveColorEditorState( const Color *ptrSnapshot,
                                    bool useBlackBackgroundsForTheme );
void rebuildConfiguredColorTables( bool has256Table, bool hasTruecolorTable,
                                   bool *ptrShouldRewriteConfig );
void refreshActiveColorTable( void );
void setColorFieldValue( int colorIndex, int colorValue );
void setColorFieldValueForColor( Color *ptrColor, int colorIndex, int colorValue );
void snapshotActiveColorEditorState( Color *ptrSnapshot,
                                     bool *ptrUseBlackBackgroundsForTheme );
void syncActiveColorTable( void );
void tidalReefColors( void );
bool tryFindColorOutputMode( const char *ptrModeName,
                             ColorOutputMode *ptrOutMode );
bool tryFindColorFieldIndexByTomlKeyName( const char *ptrKeyName,
                                          int *ptrOutColorIndex );
char userOrFriend( void );

#endif // COLOR_H_INCLUDED
