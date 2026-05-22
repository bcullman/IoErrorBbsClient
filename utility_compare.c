/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file handles comparison helpers used by lists and searches.
 */
#include "defs.h"
#include "utility.h"
static int compareFriendNames( const friend *ptrLeft, const friend *ptrRight );
static int compareStringPointers( const char *const *ptrLeft, const char *const *ptrRight );

/// @brief Compare two friend records by their stored names.
///
/// @param ptrLeft Left friend record.
/// @param ptrRight Right friend record.
///
/// @return A negative value, zero, or a positive value following `strcmp()` semantics.
static int compareFriendNames( const friend *ptrLeft, const friend *ptrRight )
{
   assert( ptrLeft->magic == 0x3231 );
   assert( ptrRight->magic == 0x3231 );

   return strcmp( ptrLeft->name, ptrRight->name );
}

/// @brief Compare two pointers to C strings.
///
/// @param ptrLeft Pointer to the left string pointer.
/// @param ptrRight Pointer to the right string pointer.
///
/// @return A negative value, zero, or a positive value following `strcmp()` semantics.
static int compareStringPointers( const char *const *ptrLeft, const char *const *ptrRight )
{
   return strcmp( *ptrLeft, *ptrRight );
}

/// @brief Compare two friend-pointer entries for sorting.
///
/// @param ptrLeft Pointer to the left friend pointer.
/// @param ptrRight Pointer to the right friend pointer.
///
/// @return A negative value, zero, or a positive value following `strcmp()` semantics.
int fSortCompare( const friend *const *ptrLeft, const friend *const *ptrRight )
{
   return compareFriendNames( *ptrLeft, *ptrRight );
}

/// @brief `void *` wrapper for friend sorting callbacks.
///
/// @param ptrLeft Pointer to the left friend pointer.
/// @param ptrRight Pointer to the right friend pointer.
///
/// @return A negative value, zero, or a positive value following `strcmp()` semantics.
int fSortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   return fSortCompare( (const friend *const *)ptrLeft, (const friend *const *)ptrRight );
}

/// @brief Compare a plain name string against a friend record name.
///
/// @param ptrName Name string to compare.
/// @param ptrFriend Friend record to compare against.
///
/// @return A negative value, zero, or a positive value following `strcmp()` semantics.
int fStrCompare( const char *ptrName, const friend *ptrFriend )
{
   return strcmp( ptrName, ptrFriend->name );
}

/// @brief `void *` wrapper for comparing a name string against a friend record.
///
/// @param ptrName Name string to compare.
/// @param ptrFriend Friend record to compare against.
///
/// @return A negative value, zero, or a positive value following `strcmp()` semantics.
int fStrCompareVoid( const void *ptrName, const void *ptrFriend )
{
   return fStrCompare( (const char *)ptrName, (const friend *)ptrFriend );
}

/// @brief Compare two string-pointer entries for sorting.
///
/// @param ptrLeft Pointer to the left string pointer.
/// @param ptrRight Pointer to the right string pointer.
///
/// @return A negative value, zero, or a positive value following `strcmp()` semantics.
int sortCompare( char **ptrLeft, char **ptrRight )
{
   return compareStringPointers( (const char *const *)ptrLeft,
                                 (const char *const *)ptrRight );
}

/// @brief `void *` wrapper for sorting string-pointer entries.
///
/// @param ptrLeft Pointer to the left string pointer.
/// @param ptrRight Pointer to the right string pointer.
///
/// @return A negative value, zero, or a positive value following `strcmp()` semantics.
int sortCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   return compareStringPointers( (const char *const *)ptrLeft,
                                 (const char *const *)ptrRight );
}

/// @brief Compare two plain C strings passed as `void *`.
///
/// @param ptrLeft Left string pointer.
/// @param ptrRight Right string pointer.
///
/// @return A negative value, zero, or a positive value following `strcmp()` semantics.
int strCompareVoid( const void *ptrLeft, const void *ptrRight )
{
   return strcmp( (const char *)ptrLeft, (const char *)ptrRight );
}
