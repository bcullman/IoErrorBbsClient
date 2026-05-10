/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config_file.h"
#include "browser.h"
#include "client.h"
#include <cmocka.h>
#include "color.h"
#include "config_menu.h"
#include "defs.h"
#include "edit.h"
#include "filter.h"
#include "getline_input.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include "telnet.h"
#include "utility.h"
typedef struct
{
   queue *ptrQueue;
} QueueFixture;

static int setupQueue( void **state )
{
   QueueFixture *ptrFixture;

   ptrFixture = calloc( 1, sizeof( QueueFixture ) );
   if ( ptrFixture == NULL )
   {
      fail_msg( "setupQueue: calloc failed for QueueFixture" );
      return -1;
   }

   ptrFixture->ptrQueue = newQueue( 16, 2 );
   if ( ptrFixture->ptrQueue == NULL )
   {
      free( ptrFixture );
      fail_msg( "setupQueue: newQueue returned NULL" );
      return -1;
   }

   *state = ptrFixture;
   return 0;
}

static int teardownQueue( void **state )
{
   QueueFixture *ptrFixture;

   ptrFixture = *state;
   if ( ptrFixture != NULL )
   {
      if ( ptrFixture->ptrQueue != NULL )
      {
         (void)deleteQueue( ptrFixture->ptrQueue );
      }
      free( ptrFixture );
   }
   return 0;
}

static void pushQueue_WhenQueueHasCapacity_ReturnsSuccess( void **state )
{
   // Arrange
   QueueFixture *ptrFixture;
   int pushResult;

   ptrFixture = *state;

   // Act
   pushResult = pushQueue( "one", ptrFixture->ptrQueue );

   // Assert
   if ( pushResult != 1 )
   {
      fail_msg( "pushQueue should return 1 when capacity is available; got %d", pushResult );
   }
}

static void popQueue_WhenQueueIsEmpty_ReturnsFailure( void **state )
{
   // Arrange
   QueueFixture *ptrFixture;
   char aryOut[16];
   int popResult;

   ptrFixture = *state;

   memset( aryOut, 0, sizeof( aryOut ) );

   // Act
   popResult = popQueue( aryOut, ptrFixture->ptrQueue );

   // Assert
   if ( popResult != 0 )
   {
      fail_msg( "popQueue should return 0 when queue is empty; got %d", popResult );
   }
}

static void pushQueue_WhenQueueIsFull_ReturnsFailure( void **state )
{
   // Arrange
   QueueFixture *ptrFixture;
   int pushResult;

   ptrFixture = *state;

   pushQueue( "one", ptrFixture->ptrQueue );
   pushQueue( "two", ptrFixture->ptrQueue );

   // Act
   pushResult = pushQueue( "three", ptrFixture->ptrQueue );

   // Assert
   if ( pushResult != 0 )
   {
      fail_msg( "pushQueue should return 0 when queue is full; got %d", pushResult );
   }
}

static void popQueue_WhenQueueHasItems_ReturnsFifoOrder( void **state )
{
   // Arrange
   QueueFixture *ptrFixture;
   char aryOut[16];
   int popResult;

   ptrFixture = *state;

   memset( aryOut, 0, sizeof( aryOut ) );
   pushQueue( "one", ptrFixture->ptrQueue );
   pushQueue( "two", ptrFixture->ptrQueue );

   // Act
   popResult = popQueue( aryOut, ptrFixture->ptrQueue );

   // Assert
   if ( popResult != 1 )
   {
      fail_msg( "popQueue should return 1 when queue has items; got %d", popResult );
   }
   if ( strcmp( aryOut, "one" ) != 0 )
   {
      fail_msg( "popQueue should return FIFO order; expected 'one', got '%s'", aryOut );
   }
}

static void popQueue_WhenQueueWrapsAround_ReturnsExpectedOrder( void **state )
{
   // Arrange
   QueueFixture *ptrFixture;
   char aryOut[16];
   char aryFirstPop[16];
   int popSecondResult;
   int popThirdResult;

   ptrFixture = *state;

   pushQueue( "one", ptrFixture->ptrQueue );
   pushQueue( "two", ptrFixture->ptrQueue );

   memset( aryFirstPop, 0, sizeof( aryFirstPop ) );
   popQueue( aryFirstPop, ptrFixture->ptrQueue );

   pushQueue( "three", ptrFixture->ptrQueue );

   // Act
   memset( aryOut, 0, sizeof( aryOut ) );
   popSecondResult = popQueue( aryOut, ptrFixture->ptrQueue );

   // Assert
   if ( strcmp( aryFirstPop, "one" ) != 0 )
   {
      fail_msg( "wrap setup first dequeue should return 'one'; got '%s'", aryFirstPop );
   }
   if ( popSecondResult != 1 )
   {
      fail_msg( "wrap-around dequeue #1 should succeed; got %d", popSecondResult );
   }
   if ( strcmp( aryOut, "two" ) != 0 )
   {
      fail_msg( "wrap-around dequeue #1 should return 'two'; got '%s'", aryOut );
   }

   memset( aryOut, 0, sizeof( aryOut ) );
   popThirdResult = popQueue( aryOut, ptrFixture->ptrQueue );
   if ( popThirdResult != 1 )
   {
      fail_msg( "wrap-around dequeue #2 should succeed; got %d", popThirdResult );
   }
   if ( strcmp( aryOut, "three" ) != 0 )
   {
      fail_msg( "wrap-around dequeue #2 should return 'three'; got '%s'", aryOut );
   }
}

static void isQueued_WhenItemExists_ReturnsTrue( void **state )
{
   // Arrange
   QueueFixture *ptrFixture;
   int isQueuedResult;

   ptrFixture = *state;

   pushQueue( "one", ptrFixture->ptrQueue );
   pushQueue( "two", ptrFixture->ptrQueue );

   // Act
   isQueuedResult = isQueued( "two", ptrFixture->ptrQueue );

   // Assert
   if ( isQueuedResult != 1 )
   {
      fail_msg( "isQueued should return 1 when item is present; got %d", isQueuedResult );
   }
}

static void isQueued_WhenItemDoesNotExist_ReturnsFalse( void **state )
{
   // Arrange
   QueueFixture *ptrFixture;
   int isQueuedResult;

   ptrFixture = *state;

   pushQueue( "one", ptrFixture->ptrQueue );

   // Act
   isQueuedResult = isQueued( "missing", ptrFixture->ptrQueue );

   // Assert
   if ( isQueuedResult != 0 )
   {
      fail_msg( "isQueued should return 0 when item is absent; got %d", isQueuedResult );
   }
}

static void safeDeleteQueue_WhenQueueHasItems_ReturnsFailure( void **state )
{
   // Arrange
   QueueFixture *ptrFixture;
   int deleteResult;

   ptrFixture = *state;

   pushQueue( "one", ptrFixture->ptrQueue );

   // Act
   deleteResult = safeDeleteQueue( ptrFixture->ptrQueue );

   // Assert
   if ( deleteResult != 0 )
   {
      fail_msg( "safeDeleteQueue should return 0 when queue has items; got %d", deleteResult );
   }
}

static void safeDeleteQueue_WhenQueueIsEmpty_ReturnsSuccess( void **state )
{
   // Arrange
   QueueFixture *ptrFixture;
   int deleteResult;

   ptrFixture = *state;

   // Act
   deleteResult = safeDeleteQueue( ptrFixture->ptrQueue );

   // Assert
   if ( deleteResult != 1 )
   {
      fail_msg( "safeDeleteQueue should return 1 when queue is empty; got %d", deleteResult );
   }
   ptrFixture->ptrQueue = NULL;
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test_setup_teardown( pushQueue_WhenQueueHasCapacity_ReturnsSuccess,
                                       setupQueue,
                                       teardownQueue ),
      cmocka_unit_test_setup_teardown( popQueue_WhenQueueIsEmpty_ReturnsFailure,
                                       setupQueue,
                                       teardownQueue ),
      cmocka_unit_test_setup_teardown( pushQueue_WhenQueueIsFull_ReturnsFailure,
                                       setupQueue,
                                       teardownQueue ),
      cmocka_unit_test_setup_teardown( popQueue_WhenQueueHasItems_ReturnsFifoOrder,
                                       setupQueue,
                                       teardownQueue ),
      cmocka_unit_test_setup_teardown( popQueue_WhenQueueWrapsAround_ReturnsExpectedOrder,
                                       setupQueue,
                                       teardownQueue ),
      cmocka_unit_test_setup_teardown( isQueued_WhenItemExists_ReturnsTrue,
                                       setupQueue,
                                       teardownQueue ),
      cmocka_unit_test_setup_teardown( isQueued_WhenItemDoesNotExist_ReturnsFalse,
                                       setupQueue,
                                       teardownQueue ),
      cmocka_unit_test_setup_teardown( safeDeleteQueue_WhenQueueHasItems_ReturnsFailure,
                                       setupQueue,
                                       teardownQueue ),
      cmocka_unit_test_setup_teardown( safeDeleteQueue_WhenQueueIsEmpty_ReturnsSuccess,
                                       setupQueue,
                                       teardownQueue ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
