/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Queue manipulation helpers.
 */
#include "defs.h"

/// @brief Free a queue object unconditionally.
///
/// @param ptrQueue Queue to destroy.
///
/// @return Always returns `0`.
int deleteQueue( queue *ptrQueue )
{
   free( ptrQueue );
   return 0;
}

/// @brief Check whether a string is already present in the queue.
///
/// @param ptrObject String to search for.
/// @param ptrQueue Queue to inspect.
///
/// @return `1` when the string is queued, otherwise `0`.
int isQueued( const char *ptrObject, queue *ptrQueue )
{
   int objectIndex;     // Object counter
   char *ptrQueueEntry; // Pointer inside queue

   // Move to head of queue.
   for ( ptrQueueEntry = ptrQueue->start + ( ptrQueue->objsize * ptrQueue->head ), objectIndex = 0; objectIndex < ptrQueue->itemCount; objectIndex++ )
   {
      // Do the comparison.
      if ( !strcmp( ptrQueueEntry, ptrObject ) )
      {
         return 1;
      }
      ptrQueueEntry += ptrQueue->objsize;
      if ( ptrQueueEntry >= (char *)( ptrQueue->start + ( ptrQueue->objsize * ptrQueue->size ) ) )
      {
         ptrQueueEntry = ptrQueue->start;
      }
   }
   return 0;
}

/// @brief Allocate a new queue.
///
/// @param size Size of each queued object.
/// @param itemCount Maximum number of objects the queue can hold.
///
/// @return A new queue on success, or `NULL` on allocation failure.
queue *newQueue( int size, int itemCount )
{
   queue *ptrQueue;

   if ( !( ptrQueue = (queue *)calloc( 1, sizeof( queue ) + (size_t)size * (size_t)itemCount ) ) )
   {
      return (queue *)NULL;
   }

   ptrQueue->start = (char *)( ptrQueue + 1 );
   ptrQueue->size = itemCount;
   ptrQueue->objsize = size;
   ptrQueue->head = 0;
   ptrQueue->tail = 0;
   ptrQueue->itemCount = 0;

   return ptrQueue;
}

/// @brief Remove the next object from the queue.
///
/// @param ptrObject Destination buffer that receives the removed object.
/// @param ptrQueue Queue to pop from.
///
/// @return `1` on success, or `0` when the queue is empty.
int popQueue( char *ptrObject, queue *ptrQueue )
{
   const char *ptrQueueRead; // Pointer into the queue

   if ( ptrQueue->itemCount <= 0 )
   {
      return ptrQueue->itemCount = 0; // Queue is empty
   }

   ptrQueue->itemCount--; // Removing an object...

   // Find the object within the queue.
   ptrQueueRead = ptrQueue->start + ( ptrQueue->objsize * ptrQueue->head );

   // Copy the object.
   memcpy( ptrObject, ptrQueueRead, (size_t)ptrQueue->objsize );

   if ( ++ptrQueue->head >= ptrQueue->size )
   {
      ptrQueue->head = 0;
   }

   return 1;
}

/// @brief Push a new object onto the queue.
///
/// @param ptrObject Object to queue.
/// @param ptrQueue Queue to modify.
///
/// @return `1` on success, or `0` when the queue is full.
int pushQueue( const char *ptrObject, queue *ptrQueue )
{
   char *ptrQueueWrite; // Pointer into the queue

   if ( ptrQueue->itemCount >= ptrQueue->size )
   { // Is the queue full?
      return 0;
   }

   ptrQueue->itemCount++;

   // Find the target address within the queue to insert object.
   ptrQueueWrite = ptrQueue->start + ptrQueue->objsize * ptrQueue->tail;

   // Clear the destination slot so the queued string is always terminated.
   memset( ptrQueueWrite, 0, (size_t)ptrQueue->objsize );

   // Copy the string into its queue position without reading past the source.
   if ( ptrQueue->objsize > 0 )
   {
      snprintf( ptrQueueWrite, (size_t)ptrQueue->objsize, "%s", ptrObject );
   }

   // Wrap around after passing the end of the queue.
   if ( ++ptrQueue->tail >= ptrQueue->size )
   {
      ptrQueue->tail = 0;
   }

   return 1;
}

/// @brief Free a queue only when it is empty.
///
/// @param ptrQueue Queue to destroy.
///
/// @return `1` when the queue was freed, or `0` when it still contained items.
int safeDeleteQueue( queue *ptrQueue )
{
   if ( ptrQueue->itemCount )
   {
      return 0;
   }
   free( ptrQueue );
   return 1;
}
