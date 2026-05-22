/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * slist.c - Functions which maintain a sorted (non-linked) list of
 * arbitrary data.
 */
#include "defs.h"
#include <stdarg.h>
#include "utility.h"
/// @brief Append an item to a sorted list.
///
/// @param list List to modify.
/// @param item Item pointer to add.
/// @param deferSort Non-zero to skip immediate resorting.
///
/// @return `1` on success, or `0` when the backing array could not be grown.
int slistAddItem( slist *list, void *item, int deferSort )
{
   void **ptrItems;

   list->nitems++;
   if ( !( ptrItems = (void *)realloc( list->items, list->nitems * sizeof( void * ) ) ) )
   {
      return 0;
   }
   list->items = ptrItems;
   list->items[list->nitems - 1] = item;
   if ( !deferSort )
   {
      slistSort( list );
   }
   return 1;
}

/// @brief Create a new sorted list.
///
/// @param nitems Number of initial items supplied through the variadic arguments.
/// @param sortfn Comparison function used to keep the list sorted.
///
/// @return A new list on success, or `NULL` on allocation failure.
slist *slistCreate( int nitems, int ( *sortfn )( const void *, const void * ), ... )
{
   slist *ptrList;

   assert( nitems >= 0 );
   assert( sortfn );

   if ( !( ptrList = (slist *)calloc( 1, sizeof( slist ) ) ) )
   {
      return NULL;
   }
   ptrList->nitems = (unsigned int)nitems;
   ptrList->sortfn = sortfn;
   if ( nitems > 0 )
   {
      int itemIndex;
      va_list argList;

      if ( !( ptrList->items = (void *)calloc( 1, (size_t)nitems * sizeof( void * ) ) ) )
      {
         free( ptrList );
         return NULL;
      }
      va_start( argList, sortfn );
      for ( itemIndex = 0; itemIndex < nitems; itemIndex++ )
      {
         ptrList->items[itemIndex] = va_arg( argList, void * );
      }
      va_end( argList );
   }
   else
   {
      ptrList->items = NULL;
   }
   return ptrList;
}

/// @brief Destroy a sorted list container without freeing its items.
///
/// @param list List to destroy.
///
/// @return This function does not return a value.
void slistDestroy( slist *list )
{
   free( list->items );
   list->items = NULL;
   free( list );
}

/// @brief Free every item stored in a sorted list.
///
/// @param list List whose items should be freed.
///
/// @return This function does not return a value.
void slistDestroyItems( slist *list )
{
   unsigned int itemIndex;

   for ( itemIndex = 0; itemIndex < list->nitems; itemIndex++ )
   {
      free( list->items[itemIndex] );
      list->items[itemIndex] = NULL;
   }
}

/// @brief Locate an item in a sorted list with a binary search.
///
/// @param list List to search.
/// @param toFind Search key.
/// @param findfn Comparison callback.
///
/// @return The matching item index, or `-1` when no item matched.
int slistFind( slist *list, void *toFind, int ( *findfn )( const void *, const void * ) )
{
   int upperBound;
   int lowerBound;

   assert( list );
   assert( findfn );

   if ( !toFind )
   { // Fail if nothing to find
      return -1;
   }
   if ( list->nitems == 0 )
   {
      return -1;
   }
   upperBound = (int)list->nitems - 1;
   lowerBound = 0;
   while ( upperBound >= lowerBound )
   {
      int midIndex;
      int compareResult;

      midIndex = ( upperBound + lowerBound ) / 2;
      compareResult = findfn( toFind, list->items[midIndex] );
      if ( compareResult == 0 )
      {
         return midIndex;
      }
      if ( compareResult < 0 )
      {
         upperBound = midIndex - 1;
      }
      else
      {
         lowerBound = midIndex + 1;
      }
   }
   return -1;
}

/// @brief Build the shallow intersection of two sorted lists.
///
/// @param list1 Left input list.
/// @param list2 Right input list.
///
/// @return A new list containing the shared items, or `NULL` on error.
slist *slistIntersection( const slist *list1, const slist *list2 )
{
   int leftIndex;        // Count of items processed
   int rightIndex;       // Count of items processed
   slist *ptrResultList; // The list being created

   if ( !list1 || !list2 || list1->sortfn != list2->sortfn )
   {
      return NULL;
   }
   assert( list1 );
   assert( list2 );
   assert( list1->sortfn == list2->sortfn );

   rightIndex = 0;

   ptrResultList = slistCreate( 0, list1->sortfn );
   if ( !ptrResultList )
   {
      return NULL;
   }

   for ( leftIndex = 0; leftIndex < (int)list1->nitems; leftIndex++ )
   {
      // Run through list2 until a matching item is found, or an item
      // greater than the current item in list1 is found.
      // First item in n2 not less than current item n1.
      while ( ptrResultList->sortfn( list1->items[leftIndex], list2->items[rightIndex] ) < 0 )
      {
         rightIndex++;
         // Stop when the end of list2 is reached.
         if ( rightIndex > (int)list2->nitems )
         {
            break;
         }
      }

      // Stop when no later item can match.
      if ( rightIndex > (int)list2->nitems )
      {
         break;
      }

      // Items are equal when neither comparison reports a smaller value.
      if ( !( ptrResultList->sortfn( list2->items[rightIndex], list1->items[leftIndex] ) < 0 ) )
      {
         if ( !slistAddItem( ptrResultList, list1->items[leftIndex], 1 ) )
         {
            slistDestroy( ptrResultList );
            return NULL;
         }
      }
   }
   slistSort( ptrResultList );
   return ptrResultList;
}

/// @brief Remove one item from the sorted list without freeing the item itself.
///
/// @param list List to modify.
/// @param item Zero-based item index to remove.
///
/// @return `1` on success, or `0` if shrinking the backing array failed.
int slistRemoveItem( slist *list, int item )
{
   void **ptrItems;

   assert( list );
   assert( item >= 0 );
   assert( (unsigned int)item < list->nitems );

   printf( "slistRemoveItem(list, %d): nitems=%u\r\n", item, list->nitems );
   list->items[item] = NULL;
   if ( (unsigned int)item < --list->nitems )
   {
      unsigned int itemIndex;

      for ( itemIndex = (unsigned int)item; itemIndex < list->nitems; itemIndex++ )
      {
         list->items[itemIndex] = list->items[itemIndex + 1];
      }
   }
   ptrItems = (void *)realloc( list->items, list->nitems * sizeof( void * ) );
   if ( !ptrItems && list->nitems )
   { // request failed
      return 0;
   }

   list->items = ptrItems;
   return 1;
}

/// @brief Sort the list in place with its configured comparison function.
///
/// @param list List to sort.
///
/// @return This function does not return a value.
void slistSort( slist *list )
{
   assert( list );

   qsort( list->items, list->nitems, sizeof( void * ), list->sortfn );
}
