/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This file handles input validation and simple menu prompts.
 */
#include "client.h"
#include "config_globals.h"
#include "defs.h"
#include "network_globals.h"
#include "utility.h"
static int printYesNoResult( int inputChar );
static ColorEditorAction colorEditorActionFromArrowKey( int inputChar );
static int readValidatedInput( const char *allowedChars, bool shouldFoldInput );

/// @brief Track repeated invalid input and flush the terminal if it keeps happening.
///
/// @param ptrInvalidCount Counter of consecutive invalid inputs.
///
/// @return This function does not return a value.
void handleInvalidInput( unsigned int *ptrInvalidCount )
{
   if ( ( *ptrInvalidCount )++ )
   {
      flushInput( *ptrInvalidCount );
   }
}

/// @brief Forward validated local keystrokes directly to the BBS.
///
/// @return This function does not return a value.
void looper( void )
{
   unsigned int invalid = 0;

   while ( true )
   {
      register int inputChar;

      if ( ( inputChar = inKey() ) < 0 )
      {
         return;
      }
      // Skip input the BBS cannot use.
      if ( ( inputChar >= ASCII_PRINTABLE_MIN &&
             inputChar <= ASCII_PRINTABLE_MAX ) ||
           findChar( ALLOWED_INPUT_CONTROL_CHARS, inputChar ) )
      {
         invalid = 0;
         netPutChar( aryKeyMap[inputChar] );
         if ( fflush( netOutputFile ) < 0 )
         {
            fatalPerror( "send", "Network error" );
         }
         if ( byte )
         {
            lastInteractiveInputByte = byte;
            trackReplayableSentChar( inputChar );
         }
      }
      else
      {
         handleInvalidInput( &invalid );
      }
   }
}

/// @brief Show the pager prompt and read the next pagination choice.
///
/// @param line Current pager line counter, updated in place.
/// @param percentComplete Optional completion percentage, or a negative value
/// when no percentage should be shown.
///
/// @return `0` to continue paging or `-1` when the user quits the pager.
int more( int *line, int percentComplete )
{
   char aryPrompt[96];
   unsigned int invalid = 0;

   if ( percentComplete >= 0 )
   {
      snprintf( aryPrompt, sizeof( aryPrompt ),
                "Page break (%d%%): Space next page, Enter next line, Q quit",
                percentComplete );
   }
   else
   {
      snprintf( aryPrompt, sizeof( aryPrompt ),
                "%s",
                "Page break: Space next page, Enter next line, Q quit" );
   }
   printf( "%s", aryPrompt );
   while ( true )
   {
      register int inputChar;

      inputChar = readFoldedKey();
      if ( inputChar == ' ' || inputChar == 'y' )
      {
         *line = 1;
      }
      else if ( inputChar == '\n' )
      {
         --*line;
      }
      else if ( findChar( "nqs", inputChar ) )
      {
         *line = -1;
      }
      else
      {
         handleInvalidInput( &invalid );
         continue;
      }
      printf( "\r%*s\r", (int)strlen( aryPrompt ), "" );
      break;
   }
   return ( *line < 0 ? -1 : 0 );
}

/// @brief Print the normalized Yes or No response and return it as a boolean.
///
/// @param inputChar Response key that should already be one of the yes/no keys.
///
/// @return `1` for yes or `0` for no.
static int printYesNoResult( int inputChar )
{
   if ( inputChar == 'y' || inputChar == 'Y' )
   {
      stdPrintf( "Yes\r\n" );
      return 1;
   }

   stdPrintf( "No\r\n" );
   return 0;
}

/// @brief Map one editor escape-sequence suffix byte to a normalized action.
///
/// Supports arrow keys plus common application-keypad operator keys.
///
/// @param inputChar Final byte from a CSI or SS3 editor-input sequence.
///
/// @return Matching color-editor action, or cancel when not recognized.
static ColorEditorAction colorEditorActionFromArrowKey( int inputChar )
{
   switch ( inputChar )
   {
      case 'A':
         return COLOR_EDITOR_ACTION_PREVIOUS_FIELD;

      case 'B':
         return COLOR_EDITOR_ACTION_NEXT_FIELD;

      case 'C':
         return COLOR_EDITOR_ACTION_MOVE_RIGHT;

      case 'D':
         return COLOR_EDITOR_ACTION_MOVE_LEFT;

      case 'M':
         return COLOR_EDITOR_ACTION_SAVE;

      case 'k':
         return COLOR_EDITOR_ACTION_INCREASE_FAST;

      case 'm':
         return COLOR_EDITOR_ACTION_DECREASE_FAST;

      default:
         return COLOR_EDITOR_ACTION_CANCEL;
   }
}

/// @brief Read one key and fold alphabetic input to lowercase.
///
/// @return The normalized key value.
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

/// @brief Read one normalized action for the interactive color editor.
///
/// Arrow keys are accepted directly, and fallback keys provide the same actions
/// when arrow sequences are awkward in the terminal.
///
/// @return Normalized editor action.
ColorEditorAction readColorEditorAction( void )
{
   unsigned int invalid;

   invalid = 0;
   while ( true )
   {
      int inputChar;

      inputChar = inKey();
      if ( inputChar == '\033' )
      {
         int sequenceLeadChar;

         sequenceLeadChar = inKey();
         if ( sequenceLeadChar == '[' || sequenceLeadChar == 'O' )
         {
            int sequenceFinalChar;
            ColorEditorAction sequenceAction;

            sequenceFinalChar = inKey();
            sequenceAction = colorEditorActionFromArrowKey( sequenceFinalChar );
            if ( sequenceAction != COLOR_EDITOR_ACTION_CANCEL )
            {
               return sequenceAction;
            }
         }
      }

      if ( isalpha( inputChar ) )
      {
         inputChar = tolower( inputChar );
      }
      switch ( inputChar )
      {
         case 'a':
            return COLOR_EDITOR_ACTION_MOVE_LEFT;

         case 'c':
            return COLOR_EDITOR_ACTION_CANCEL;

         case 'd':
            return COLOR_EDITOR_ACTION_MOVE_RIGHT;

         case 'n':
            return COLOR_EDITOR_ACTION_NEXT_FIELD;

         case 'p':
            return COLOR_EDITOR_ACTION_PREVIOUS_FIELD;

         case 'q':
            return COLOR_EDITOR_ACTION_CANCEL;

         case 'r':
            return COLOR_EDITOR_ACTION_RESET_FIELD;

         case 's':
            return COLOR_EDITOR_ACTION_DECREASE_SMALL;

         case 'w':
            return COLOR_EDITOR_ACTION_INCREASE_SMALL;

         case '=':
         case '+':
            return COLOR_EDITOR_ACTION_INCREASE_FAST;

         case '_':
         case '-':
            return COLOR_EDITOR_ACTION_DECREASE_FAST;

         case '\n':
         case ' ':
            return COLOR_EDITOR_ACTION_SAVE;

         default:
            handleInvalidInput( &invalid );
            break;
      }
   }
}

/// @brief Read input until one of the allowed characters is entered.
///
/// @param allowedChars Set of accepted characters.
/// @param shouldFoldInput Non-zero to lowercase alphabetic input before validation.
///
/// @return The accepted key value.
static int readValidatedInput( const char *allowedChars, bool shouldFoldInput )
{
   unsigned int invalid = 0;

   while ( true )
   {
      int inputChar;

      if ( shouldFoldInput )
      {
         inputChar = readFoldedKey();
      }
      else
      {
         inputChar = inKey();
      }
      if ( findChar( allowedChars, inputChar ) )
      {
         return inputChar;
      }
      handleInvalidInput( &invalid );
   }
}

/// @brief Read one key that must match the supplied set exactly.
///
/// @param allowedChars Set of accepted characters.
///
/// @return The accepted key value.
int readValidatedKey( const char *allowedChars )
{
   return readValidatedInput( allowedChars, false );
}

/// @brief Read a menu key after folding alphabetic input to lowercase.
///
/// @param allowedCharsLowercase Lowercase accepted menu characters.
///
/// @return The accepted menu key.
int readValidatedMenuKey( const char *allowedCharsLowercase )
{
   return readValidatedInput( allowedCharsLowercase, true );
}

/// @brief Read and print a strict yes-or-no answer.
///
/// @return `1` for yes or `0` for no.
int yesNo( void )
{
   register int inputChar;

   inputChar = readValidatedKey( "nNyY" );
   return printYesNoResult( inputChar );
}

/// @brief Read a yes-or-no answer with a default on space or return.
///
/// @param defaultAnswer Default boolean value used for space or return.
///
/// @return `1` for yes or `0` for no.
int yesNoDefault( int defaultAnswer )
{
   register int inputChar;

   inputChar = readValidatedKey( "nNyY\n " );
   if ( inputChar == '\n' || inputChar == ' ' )
   {
      inputChar = ( defaultAnswer ? 'Y' : 'N' );
   }
   if ( inputChar == 'y' || inputChar == 'Y' ||
        inputChar == 'n' || inputChar == 'N' )
   {
      return printYesNoResult( inputChar );
   }
   else
   {
      char aryBuffer[160];
      stdPrintf( "\r\n" );
      snprintf( aryBuffer, sizeof( aryBuffer ), "yesNoDefault: 0x%x\r\n"
                                                "Please report this to IO ERROR\r\n",
                inputChar );
      fatalExit( aryBuffer, "Internal error" );
   }
   return 0;
}
