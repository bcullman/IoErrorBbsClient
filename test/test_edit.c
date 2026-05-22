/*
 * Copyright (C) 2024-2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config_file.h"
#include "browser.h"
#include "client.h"
#include "test/cmocka_compat.h"
#include "color.h"
#include "config_menu.h"
#include "defs.h"
#include "edit.h"
#include "ext.h"
#include "filter.h"
#include "getline_input.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include "telnet.h"
#include "test_helpers.h"
#include "utility.h"
static int aryInputQueue[16];
static size_t inputCount;
static size_t inputIndex;
static int arySentChars[512];
static size_t sentCharCount;

static void resetState( void )
{
   byte = 0;
   bytePosition = 0;
   inputCount = 0;
   inputIndex = 0;
   sentCharCount = 0;
   targetByte = 0;
   flagsConfiguration.isLastSave = 0;
   flagsConfiguration.isPosting = 0;
}

static void setInputSequence( const int *aryKeys, size_t count )
{
   inputCount = copyIntArray( aryKeys, count, aryInputQueue,
                              sizeof( aryInputQueue ) / sizeof( aryInputQueue[0] ) );
   inputIndex = 0;
}

// edit.c dependencies outside checkFile() scope for these tests.
int colorize( const char *ptrText )
{
   (void)ptrText;
   return 1;
}

void continuedPostHelper( void )
{
   // Test stub: continued-post handling is not relevant in this test.
}

noreturn void fatalPerror( const char *error, const char *heading )
{
   (void)error;
   (void)heading;
   abort();
}

noreturn void fatalExit( const char *message, const char *heading )
{
   (void)message;
   (void)heading;
   abort();
}

char *findChar( const char *ptrString, int targetChar )
{
   return (char *)strchr( ptrString, targetChar );
}

void flushInput( unsigned int count )
{
   (void)count;
}

void handleInvalidInput( unsigned int *ptrInvalidCount )
{
   if ( ( *ptrInvalidCount )++ )
   {
      flushInput( *ptrInvalidCount );
   }
}

void getString( int length, char *result, int line )
{
   (void)length;
   (void)line;
   result[0] = '\0';
}

int inKey( void )
{
   if ( inputIndex < inputCount )
   {
      return aryInputQueue[inputIndex++];
   }
   return '\n';
}

int readFoldedKey( void )
{
   int inputChar;

   inputChar = inKey();
   if ( isalpha( inputChar ) )
   {
      inputChar = tolower( inputChar );
   }
   return inputChar;
}

void looper( void )
{
   // Test stub: main-loop behavior is not relevant in this test.
}

int more( int *line, int percentComplete )
{
   (void)line;
   (void)percentComplete;
   return 0;
}

void mySleep( unsigned int seconds )
{
   (void)seconds;
}

int netPutChar( int inputChar )
{
   return inputChar;
}

void printAnsiDisplayStateValue( int foregroundColor, int backgroundColor )
{
   (void)foregroundColor;
   (void)backgroundColor;
}

void sendTrackedChar( int inputChar )
{
   if ( sentCharCount < sizeof( arySentChars ) / sizeof( arySentChars[0] ) )
   {
      arySentChars[sentCharCount++] = inputChar;
   }
   netPutChar( inputChar );
   byte++;
}

/// @brief Send a test byte without replay tracking.
///
/// @param inputChar Character to send.
///
/// @return This stub does not return a value.
void sendTrackedCharWithoutReplay( int inputChar )
{
   sendTrackedChar( inputChar );
}

void run( const char *ptrCommand, const char *ptrArg )
{
   (void)ptrCommand;
   (void)ptrArg;
}

void sendBlock( void )
{
   if ( sentCharCount < sizeof( arySentChars ) / sizeof( arySentChars[0] ) )
   {
      arySentChars[sentCharCount++] = IAC;
   }
   if ( sentCharCount < sizeof( arySentChars ) / sizeof( arySentChars[0] ) )
   {
      arySentChars[sentCharCount++] = BLOCK;
   }
}

void tempFileError( void )
{
   // Test stub: temp-file error handling is not relevant in this test.
}

int yesNo( void )
{
   return 0;
}

static void checkFile_WhenMessageIsValid_ReturnsZero( void **state )
{
   // Arrange
   FILE *ptrMessageFile;
   int result;

   (void)state;

   resetState();

   ptrMessageFile = tmpfile();
   if ( ptrMessageFile == NULL )
   {
      fail_msg( "tmpfile failed in valid-message test setup" );
      return;
   }
   fprintf( ptrMessageFile, "Hello world.\nThis line is fine.\n" );
   fflush( ptrMessageFile );

   // Act
   result = checkFile( ptrMessageFile );

   // Assert
   if ( result != 0 )
   {
      fail_msg( "checkFile should return 0 for valid message content; got %d", result );
   }

   fclose( ptrMessageFile );
}

static void checkFile_WhenLineExceeds79Chars_ReturnsOne( void **state )
{
   // Arrange
   FILE *ptrMessageFile;
   int result;

   (void)state;

   resetState();

   ptrMessageFile = tmpfile();
   if ( ptrMessageFile == NULL )
   {
      fail_msg( "tmpfile failed in line-length test setup" );
      return;
   }
   if ( !tryWriteRepeatedChar( ptrMessageFile, 'A', 80 ) )
   {
      fclose( ptrMessageFile );
      fail_msg( "Arrange failed: unable to write long line content for line-length test" );
      return;
   }
   fputc( '\n', ptrMessageFile );
   fflush( ptrMessageFile );

   // Act
   result = checkFile( ptrMessageFile );

   // Assert
   if ( result != 1 )
   {
      fail_msg( "checkFile should return 1 when line exceeds 79 chars; got %d", result );
   }

   fclose( ptrMessageFile );
}

static void checkFile_WhenLongLineHasSpaces_WrapsAndReturnsZero( void **state )
{
   FILE *ptrMessageFile;
   char aryResult[256];
   int result;

   (void)state;

   resetState();

   ptrMessageFile = tmpfile();
   if ( ptrMessageFile == NULL )
   {
      fail_msg( "tmpfile failed in line-wrap test setup" );
      return;
   }
   if ( fputs( "This draft line is intentionally long enough to require wrapping when the file is saved automatically.\n",
               ptrMessageFile ) == EOF )
   {
      fclose( ptrMessageFile );
      fail_msg( "Arrange failed: unable to write wrapable long line fixture" );
      return;
   }
   fflush( ptrMessageFile );

   result = checkFile( ptrMessageFile );

   if ( result != 0 )
   {
      fclose( ptrMessageFile );
      fail_msg( "checkFile should auto-wrap long prose lines; got %d", result );
      return;
   }
   if ( !tryReadFileIntoBuffer( ptrMessageFile, aryResult, sizeof( aryResult ) ) )
   {
      fclose( ptrMessageFile );
      fail_msg( "Assert failed: unable to read wrapped message text back from temp file" );
      return;
   }
   if ( strchr( aryResult, '\n' ) == NULL || strcmp( aryResult, "This draft line is intentionally long enough to require wrapping when the file\nis saved automatically.\n" ) != 0 )
   {
      fclose( ptrMessageFile );
      fail_msg( "checkFile should wrap long prose lines at spaces; got '%s'", aryResult );
      return;
   }

   fclose( ptrMessageFile );
}

static void checkFile_WhenIllegalControlCharacterPresent_ReturnsOne( void **state )
{
   // Arrange
   FILE *ptrMessageFile;
   int result;

   (void)state;

   resetState();

   ptrMessageFile = tmpfile();
   if ( ptrMessageFile == NULL )
   {
      fail_msg( "tmpfile failed in illegal-control-char test setup" );
      return;
   }
   fputc( 'O', ptrMessageFile );
   fputc( 1, ptrMessageFile );
   fputc( 'K', ptrMessageFile );
   fputc( '\n', ptrMessageFile );
   fflush( ptrMessageFile );

   // Act
   result = checkFile( ptrMessageFile );

   // Assert
   if ( result != 1 )
   {
      fail_msg( "checkFile should return 1 for illegal control chars; got %d", result );
   }

   fclose( ptrMessageFile );
}

static void checkFile_WhenTabExpansionPushesPast79_ReturnsOne( void **state )
{
   // Arrange
   FILE *ptrMessageFile;
   int result;

   (void)state;

   resetState();

   ptrMessageFile = tmpfile();
   if ( ptrMessageFile == NULL )
   {
      fail_msg( "tmpfile failed in tab-expansion test setup" );
      return;
   }
   if ( !tryWriteRepeatedChar( ptrMessageFile, 'A', 73 ) )
   {
      fclose( ptrMessageFile );
      fail_msg( "Arrange failed: unable to write message content for tab-expansion test" );
      return;
   }
   fputc( '\t', ptrMessageFile );
   fputc( '\n', ptrMessageFile );
   fflush( ptrMessageFile );

   // Act
   result = checkFile( ptrMessageFile );

   // Assert
   if ( result != 1 )
   {
      fail_msg( "checkFile should return 1 when tab expansion exceeds limit; got %d", result );
   }

   fclose( ptrMessageFile );
}

static void checkFile_WhenTotalMessageSizeExceedsLimit_ReturnsOne( void **state )
{
   // Arrange
   FILE *ptrMessageFile;
   int result;
   int lineIndex;

   (void)state;

   resetState();

   ptrMessageFile = tmpfile();
   if ( ptrMessageFile == NULL )
   {
      fail_msg( "tmpfile failed in total-size test setup" );
      return;
   }
   for ( lineIndex = 0; lineIndex < 620; ++lineIndex )
   {
      if ( !tryWriteRepeatedChar( ptrMessageFile, 'A', 79 ) )
      {
         fclose( ptrMessageFile );
         fail_msg( "Arrange failed: unable to write message content for size-limit test" );
         return;
      }
      fputc( '\n', ptrMessageFile );
   }
   fflush( ptrMessageFile );

   // Act
   result = checkFile( ptrMessageFile );

   // Assert
   if ( result != 1 )
   {
      fail_msg( "checkFile should return 1 when total message size exceeds hard cap; got %d", result );
   }

   fclose( ptrMessageFile );
}

static void checkFile_WhenSupportedUtf8PunctuationPresent_NormalizesToAsciiAndReturnsZero( void **state )
{
   // Arrange
   FILE *ptrMessageFile;
   char aryResult[256];
   int result;

   (void)state;

   resetState();

   ptrMessageFile = tmpfile();
   if ( ptrMessageFile == NULL )
   {
      fail_msg( "tmpfile failed in typographic punctuation test setup" );
      return;
   }
   if ( fputs( "\xef\xbb\xbfIt\xe2\x80\x99s \xe2\x80\x9cquoted\xe2\x80\x9d text\xe2\x80\x94okay\xe2\x80\xa6"
               "\xc2\xa0Soft\xc2\xad hyphen \xe2\x88\x92 math \xe2\x80\x8bjoin "
               "\xc2\xabhi\xc2\xbb.\n",
               ptrMessageFile ) == EOF )
   {
      fclose( ptrMessageFile );
      fail_msg( "Arrange failed: unable to write UTF-8 punctuation fixture" );
      return;
   }
   fflush( ptrMessageFile );

   // Act
   result = checkFile( ptrMessageFile );

   // Assert
   if ( result != 0 )
   {
      fclose( ptrMessageFile );
      fail_msg( "checkFile should normalize supported UTF-8 punctuation and spacing; got %d", result );
      return;
   }
   if ( !tryReadFileIntoBuffer( ptrMessageFile, aryResult, sizeof( aryResult ) ) )
   {
      fclose( ptrMessageFile );
      fail_msg( "Assert failed: unable to read normalized message text back from temp file" );
      return;
   }
   if ( strcmp( aryResult, "It's \"quoted\" text-okay... Soft hyphen - math join \"hi\".\n" ) != 0 )
   {
      fclose( ptrMessageFile );
      fail_msg( "checkFile should rewrite supported UTF-8 punctuation to ASCII; got '%s'", aryResult );
      return;
   }

   fclose( ptrMessageFile );
}

static void prompt_WhenSaveSelected_SavesMessageAndReturnsMinusOne( void **state )
{
   // Arrange
   static const int aryExpectedSaveSuffix[] = {
      IAC, BLOCK,
      'B', 'r', 'e', 'a', 'k', 'i', 'n', 'g', ' ', 'N', 'e', 'w', 's', '\n',
      CTRL_D, 's' };
   FILE *ptrMessageFile;
   int result;
   int previousChar;
   int aryKeys[] = { '\n', 's' };

   (void)state;

   resetState();
   setInputSequence( aryKeys, sizeof( aryKeys ) / sizeof( aryKeys[0] ) );
   byte = 3;
   bytePosition = 8;
   flagsConfiguration.isPosting = 1;
   previousChar = 0;
   targetByte = 12;
   ptrMessageFile = tmpfile();
   if ( ptrMessageFile == NULL )
   {
      fail_msg( "tmpfile failed in prompt save test setup" );
      return;
   }
   fprintf( ptrMessageFile, "Breaking News\n" );
   fflush( ptrMessageFile );

   // Act
   result = prompt( ptrMessageFile, &previousChar, '\n' );

   // Assert
   if ( result != -1 )
   {
      fclose( ptrMessageFile );
      fail_msg( "prompt should return -1 after saving a valid message; got %d", result );
      return;
   }
   if ( !flagsConfiguration.isLastSave || flagsConfiguration.isPosting )
   {
      fclose( ptrMessageFile );
      fail_msg( "prompt should mark the post saved and clear posting state; got isLastSave=%u isPosting=%u",
                flagsConfiguration.isLastSave, flagsConfiguration.isPosting );
      return;
   }
   if ( flagsConfiguration.shouldCheckExpress )
   {
      fclose( ptrMessageFile );
      fail_msg( "prompt save path should clear shouldCheckExpress before returning" );
      return;
   }
   if ( targetByte != 0 || bytePosition != byte )
   {
      fclose( ptrMessageFile );
      fail_msg( "prompt save path should clear editor replay state; got targetByte=%ld bytePosition=%ld byte=%ld",
                targetByte, bytePosition, byte );
      return;
   }
   if ( sentCharCount < sizeof( aryExpectedSaveSuffix ) / sizeof( aryExpectedSaveSuffix[0] ) )
   {
      fclose( ptrMessageFile );
      fail_msg( "prompt save path should send the full save payload; sent count=%zu",
                sentCharCount );
      return;
   }
   if ( memcmp( arySentChars + sentCharCount -
                   ( sizeof( aryExpectedSaveSuffix ) / sizeof( aryExpectedSaveSuffix[0] ) ),
                aryExpectedSaveSuffix, sizeof( aryExpectedSaveSuffix ) ) != 0 )
   {
      fclose( ptrMessageFile );
      fail_msg( "prompt save path should send BLOCK, message text, CTRL_D, and 's' in order" );
      return;
   }

   fclose( ptrMessageFile );
}

static void prompt_WhenInvokedWithPrintCommand_LoadsExistingMessage( void **state )
{
   // Arrange
   FILE *ptrMessageFile;
   int result;
   int previousChar;

   (void)state;

   resetState();
   previousChar = -1;
   ptrMessageFile = tmpfile();
   if ( ptrMessageFile == NULL )
   {
      fail_msg( "tmpfile failed in prompt print test setup" );
      return;
   }
   fprintf( ptrMessageFile, "Existing draft line\n" );
   fflush( ptrMessageFile );

   // Act
   result = prompt( ptrMessageFile, &previousChar, 'p' );

   // Assert
   if ( result != 0 )
   {
      fclose( ptrMessageFile );
      fail_msg( "prompt should return 0 when reloading an existing draft; got %d", result );
      return;
   }
   if ( previousChar != '\n' )
   {
      fclose( ptrMessageFile );
      fail_msg( "prompt should change previousChar from -1 to newline after reloading an existing draft; got %d", previousChar );
      return;
   }
   if ( fseek( ptrMessageFile, 0L, SEEK_END ) != 0 )
   {
      fclose( ptrMessageFile );
      fail_msg( "Arrange failed: unable to seek to end of message file after print path" );
      return;
   }
   if ( ftell( ptrMessageFile ) <= 0 )
   {
      fclose( ptrMessageFile );
      fail_msg( "prompt print path should preserve existing draft contents" );
      return;
   }

   fclose( ptrMessageFile );
}

int main( void )
{
   const struct CMUnitTest aryTests[] = {
      cmocka_unit_test( checkFile_WhenMessageIsValid_ReturnsZero ),
      cmocka_unit_test( checkFile_WhenLineExceeds79Chars_ReturnsOne ),
      cmocka_unit_test( checkFile_WhenLongLineHasSpaces_WrapsAndReturnsZero ),
      cmocka_unit_test( checkFile_WhenIllegalControlCharacterPresent_ReturnsOne ),
      cmocka_unit_test( checkFile_WhenTabExpansionPushesPast79_ReturnsOne ),
      cmocka_unit_test( checkFile_WhenTotalMessageSizeExceedsLimit_ReturnsOne ),
      cmocka_unit_test( checkFile_WhenSupportedUtf8PunctuationPresent_NormalizesToAsciiAndReturnsZero ),
      cmocka_unit_test( prompt_WhenSaveSelected_SavesMessageAndReturnsMinusOne ),
      cmocka_unit_test( prompt_WhenInvokedWithPrintCommand_LoadsExistingMessage ),
   };

   return cmocka_run_group_tests( aryTests, NULL, NULL );
}
