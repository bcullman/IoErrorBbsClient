/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "client.h"
#include "client_globals.h"
#include "color.h"
#include "config_globals.h"
#include "defs.h"
#include "edit.h"
#include "filter.h"
#include "filter_globals.h"
#include "getline_input.h"
#include "network_globals.h"
#include "sysio.h"
#include "telnet.h"
#include "utility.h"
static void continueAfterExternalEdit( FILE **ptrMessageFile );
static bool copyNamedFileIntoMessage( FILE *ptrMessageFile, const char *ptrInputPath );
static void flushEditorNetworkOutput( void );
static bool loadNamedFileIntoMessage( FILE **ptrMessageFile, char *ptrInputPath,
                                      int commandChar );
static void printEditorCommandPrompt( void );
static void resetEditorReplayState( void );
static const char *resolveEditorCommand( void );
static void sendEditorCommand( int inputChar );
static void showEditorCommandPrompt( void );

/// @brief Reopen the temp file and revalidate it after returning from an external editor.
///
/// @param ptrMessageFile Address of the current draft file handle.
///
/// @return This helper does not return a value.
static void continueAfterExternalEdit( FILE **ptrMessageFile )
{
   if ( flagsConfiguration.shouldUseAnsi )
   {
      printAnsiDisplayStateValue( lastColor, color.background );
   }
   printf( "[Editing complete]\r\n" );
   if ( !( tempFile = freopen( aryTempFileName, "r+", tempFile ) ) )
   {
      fatalPerror( "aryEditor return: freopen(aryTempFileName, \"r+\")", "Edit file error" );
   }
   *ptrMessageFile = tempFile;
   if ( checkFile( *ptrMessageFile ) )
   {
      fflush( stdout );
      mySleep( 1 );
   }
}

/// @brief Copy the contents of a named file into the current draft.
///
/// @param ptrMessageFile Draft file to append to.
/// @param ptrInputPath Source file path to copy.
///
/// @return `true` on success, otherwise `false`.
static bool copyNamedFileIntoMessage( FILE *ptrMessageFile, const char *ptrInputPath )
{
   FILE *ptrCopyFile;
   int inputChar;

   ptrCopyFile = fopen( ptrInputPath, "r" );
   if ( ptrCopyFile == NULL )
   {
      printf( "\r\n[Error:  named file does not exist]\r\n\n" );
      return false;
   }

   while ( ( inputChar = getc( ptrCopyFile ) ) >= 0 )
   {
      if ( putc( inputChar, ptrMessageFile ) < 0 )
      {
         tempFileError();
         fclose( ptrCopyFile );
         return false;
      }
   }
   if ( feof( ptrCopyFile ) && fflush( ptrMessageFile ) < 0 )
   {
      tempFileError();
      fclose( ptrCopyFile );
      return false;
   }
   fclose( ptrCopyFile );
   return true;
}

/// @brief Flush editor commands that were sent to the BBS.
///
/// @return This helper does not return a value.
static void flushEditorNetworkOutput( void )
{
   if ( netflush() < 0 )
   {
      fatalPerror( "send", "Network error" );
   }
}

/// @brief Load a named file into the current draft from the editor prompt.
///
/// @param ptrMessageFile Address of the current draft file handle.
/// @param ptrInputPath Buffer that receives the file path from the user.
/// @param commandChar Editor command that triggered the load.
///
/// @return `true` if the load should continue, otherwise `false`.
static bool loadNamedFileIntoMessage( FILE **ptrMessageFile, char *ptrInputPath,
                                      int commandChar )
{
   if ( !isupper( commandChar ) )
   {
      return true;
   }

   fseek( *ptrMessageFile, 0L, SEEK_END );
   if ( ftell( *ptrMessageFile ) )
   {
      printf( "\r\nThere is text in your edit file.  Do you wish to erase it? (Y/N) -> " );
      if ( yesNo() )
      {
         if ( !( tempFile = freopen( aryTempFileName, "w+", tempFile ) ) )
         {
            fatalPerror( "load file into aryEditor: reopen temp file for truncate", "Edit file error" );
         }
         *ptrMessageFile = tempFile;
      }
      else
      {
         return false;
      }
   }
   printf( "\r\nFilename -> " );
   getString( 67, ptrInputPath, -999 );
   if ( !*ptrInputPath )
   {
      return false;
   }

   return copyNamedFileIntoMessage( *ptrMessageFile, ptrInputPath );
}

/// @brief Print the ANSI-colored editor command prompt legend.
///
/// @return This helper does not return a value.
static void printEditorCommandPrompt( void )
{
   char aryAnsiSequence[ANSI_SEQUENCE_BUFFER_SIZE];
   static const char *aryCommandLabels[] =
      {
         "Abort",
         "Continue",
         "Edit",
         "Print",
         "Save",
         "Xpress" };
   size_t itemIndex;

   if ( !flagsConfiguration.shouldUseAnsi )
   {
      printf( "<A>bort <C>ontinue <E>dit <P>rint <S>ave <X>press -> " );
      return;
   }

   for ( itemIndex = 0;
         itemIndex < sizeof( aryCommandLabels ) / sizeof( aryCommandLabels[0] );
         itemIndex++ )
   {
      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.forum );
      printf( "%s", aryAnsiSequence );
      printf( "%c", aryCommandLabels[itemIndex][0] );

      formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                    color.number );
      printf( "%s", aryAnsiSequence );
      printf( "%s", aryCommandLabels[itemIndex] + 1 );

      if ( itemIndex + 1 <
           sizeof( aryCommandLabels ) / sizeof( aryCommandLabels[0] ) )
      {
         printf( "  " );
      }
   }

   printf( " -> " );
   formatAnsiForegroundSequence( aryAnsiSequence, sizeof( aryAnsiSequence ),
                                 color.text );
   printf( "%s", aryAnsiSequence );
}

/// @brief Handle the command prompt shown while composing a local message.
///
/// @param ptrMessageFile Draft file being edited.
/// @param previousChar Last character seen by the main editor loop.
/// @param commandChar Command character that opened the prompt.
///
/// @return `1` to continue editing, `0` to return to the caller, or `-1` to stop posting.
int prompt( FILE *ptrMessageFile, int *previousChar, int commandChar )
{
   int itemIndex;
   int inputChar = commandChar;
   int lineLength;
   unsigned int invalid = 0;
   long size;
   int lines;
   char aryCurrentLine[80];

   itemIndex = 0;
   while ( true )
   {
      if ( *previousChar != -1 )
      {
         if ( itemIndex != 1 )
         {
            sendEditorCommand( 'c' );
            flagsConfiguration.shouldCheckExpress = 1;
            (void)inKey();
            flagsConfiguration.shouldCheckExpress = 0;
            showEditorCommandPrompt();
            fflush( stdout );
         }
         itemIndex = 0;
         // Make 'x' work at this prompt for isXland function
         if ( !( isXland && xlandQueue->itemCount ) )
         {
            while ( true )
            {
               inputChar = readFoldedKey();
               if ( findChar( " \nacepsqtx?/", inputChar ) )
               {
                  break;
               }
               handleInvalidInput( &invalid );
            }
            invalid = 0;
         }
         else
         {
            inputChar = 'x';
         }
      }
      switch ( inputChar )
      {
         case ' ':
         case '\n':
            if ( !itemIndex++ )
            {
               continue;
            }
            // Flush repeated keystrokes before returning to edit mode.
            flushInput( (unsigned)itemIndex );
            printf( "\r\n" );
            continue;

         case 'a':
            printf( "Abort: are you sure? " );
            if ( yesNo() )
            {
               sendEditorCommand( 'a' );
               flagsConfiguration.isPosting = 0;
               resetEditorReplayState();
               return ( -1 );
            }
            continue;

         case 'c':
            printf( "Continue...\r\n" );
            if ( flagsConfiguration.shouldUseAnsi )
            {
               continuedPostHelper();
            }
            break;

         case 'p':
            if ( *previousChar == -1 )
            {
               *previousChar = '\n';
            }
            else
            {
               printf( "Print formatted\r\n\n%s", arySavedHeader );
            }
            fseek( ptrMessageFile, 0L, SEEK_END );
            size = ftell( ptrMessageFile );
            rewind( ptrMessageFile );
            lines = 2;
            lineLength = 0;
            itemIndex = 0;
            while ( ( inputChar = getc( ptrMessageFile ) ) > 0 )
            {
               itemIndex++;
               if ( inputChar == TAB )
               {
                  do
                  {
                     putchar( ' ' );
                  } while ( ++lineLength & 7 );
               }
               else
               {
                  if ( inputChar == '\n' )
                  {
                     putchar( '\r' );
                  }
                  putchar( inputChar );
                  lineLength++;
               }
               if ( inputChar == '\n' )
               {
                  lineLength = 0;
                  if ( ++lines == rows &&
                       more( &lines, size > 0 ? (int)( itemIndex * 100 / size ) : 0 ) < 0 )
                  {
                     break;
                  }
               }
            }
            fseek( ptrMessageFile, 0L, SEEK_END );
            break;

         case 's':
            printf( "Save message\r\n" );
            if ( checkFile( ptrMessageFile ) )
            {
               continue;
            }
            sendBlock();
            rewind( ptrMessageFile );
            while ( ( inputChar = getc( ptrMessageFile ) ) > 0 )
            {
               sendTrackedCharWithoutReplay( inputChar );
            }
            sendTrackedCharWithoutReplay( CTRL_D );
            sendTrackedCharWithoutReplay( 's' );
            flagsConfiguration.isLastSave = 1;
            flagsConfiguration.isPosting = 0;
            resetEditorReplayState();
            flushEditorNetworkOutput();
            return ( -1 );

         case 'q':
         case 't':
         case 'x':
         case '?':
         case '/':
            sendEditorCommand( inputChar );
            looper();
            netPutChar( 'c' );
            continue;

         case 'e':
            {
               const char *ptrEditorCommand;

               printf( "Edit\r\n" );
               ptrEditorCommand = resolveEditorCommand();
               if ( ptrEditorCommand == NULL )
               {
                  printf( "[Error:  No editor available]\r\n" );
               }
               else
               {
                  if ( !loadNamedFileIntoMessage( &ptrMessageFile,
                                                  aryCurrentLine,
                                                  commandChar ) )
                  {
                     continue;
                  }
                  // We have to close and reopen the tempFile due to locking
                  fclose( tempFile );
                  run( ptrEditorCommand, aryTempFileName );
                  if ( !( tempFile = fopen( aryTempFileName, "a+" ) ) )
                  {
                     fatalPerror( "openTmpFile: fopen", "Local error" );
                  }
                  continueAfterExternalEdit( &ptrMessageFile );
               }
               continue;
            }
      }
      return ( 0 );
   }
}

/// @brief Stop replaying buffered pre-editor input after the editor exits.
///
/// @return This helper does not return a value.
static void resetEditorReplayState( void )
{
   targetByte = 0;
   bytePosition = byte;
}

/// @brief Resolve the configured external editor command for the current session.
///
/// @return Editor command string to execute, or `NULL` if none is available.
static const char *resolveEditorCommand( void )
{
   if ( !*aryEditor )
   {
      return NULL;
   }
   if ( strcmp( aryEditor, DEFAULT_EDITOR_CONFIG_VALUE ) == 0 )
   {
      return *aryMyEditor ? aryMyEditor : NULL;
   }
   return aryEditor;
}

/// @brief Send a one-character editor command back to the server.
///
/// @param inputChar Command character to send.
///
/// @return This helper does not return a value.
static void sendEditorCommand( int inputChar )
{
   sendBlock();
   sendTrackedCharWithoutReplay( CTRL_D );
   sendTrackedCharWithoutReplay( inputChar );
}

/// @brief Show the editor command prompt using the current color mode.
///
/// @return This helper does not return a value.
static void showEditorCommandPrompt( void )
{
   if ( flagsConfiguration.shouldUseAnsi )
   {
      printEditorCommandPrompt();
   }
   else
   {
      printf( "<A>bort <C>ontinue <E>dit <P>rint <S>ave <X>press -> " );
   }
}
