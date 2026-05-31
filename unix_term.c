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
#include "network_globals.h"
#include "pane_ui.h"
#include "sysio.h"
#include "telnet.h"
#include "unix.h"
#include "utility.h"
static bool shouldUpdateTitleBar( void );
static bool terminalSupportsTitleBarUpdates( void );

static int isTerminalStateSaved = 0;
#ifdef HAVE_TERMIOS_H
static struct termios saveterm;
#else
#ifdef HAVE_TERMIO_H
static struct termio saveterm;
#else
static struct sgttyb saveterm;
static struct tchars savetchars;
static struct ltchars savedLocalTermChars;
static int savelocalmode;
#endif
#endif

/// @brief Flush pending terminal input after invalid or dangerous key sequences.
///
/// @param invalid Count of invalid bytes seen so far, used to scale the delay.
///
/// @return This function does not return a value.
void flushInput( unsigned int invalid )
{
#if defined( FIONREAD ) || defined( TCFLSH )
   int pendingInputBytes;
#endif

   if ( invalid / 2 )
   {
      mySleep( invalid / 2 < 3 ? invalid / 2 : 3 );
   }
#ifdef FIONREAD
   while ( isPtyInputAvailable() || ( !ioctl( 0, FIONREAD, &pendingInputBytes ) && pendingInputBytes > 0 ) )
   {
      (void)ptyget();
   }
#else
#ifdef TCFLSH
   pendingInputBytes = 0;
   ioctl( 0, TCFLSH, &pendingInputBytes );
#endif
   while ( isPtyInputAvailable() )
   {
      (void)ptyget();
   }
#endif
}

/// @brief Read the current terminal height and clamp it to supported limits.
///
/// @return The number of terminal rows now stored in `rows`.
int getWindowSize( void )
{
#ifdef TIOCGWINSZ
   struct winsize ws;

   if ( ioctl( 0, TIOCGWINSZ, (char *)&ws ) < 0 )
   {
      return ( rows = WINDOW_ROWS_DEFAULT );
   }
   else if ( ( rows = ws.ws_row ) < WINDOW_ROWS_MIN || rows > WINDOW_ROWS_MAX )
   {
      return ( rows = WINDOW_ROWS_DEFAULT );
   }
   else
   {
      return ( rows );
   }
#else
   return ( rows = WINDOW_ROWS_DEFAULT );
#endif
}

/// @brief Sleep for the requested number of seconds.
///
/// @param sec Number of seconds to sleep.
///
/// @return This function does not return a value.
void mySleep( unsigned int sec )
{
   sleep( sec );
}

/// @brief Clear any custom terminal title.
///
/// @return This function does not return a value.
void noTitleBar( void )
{
   if ( !shouldUpdateTitleBar() )
   {
      return;
   }

   printf( "\033]0;\007" );
   fflush( stdout );
}

/// @brief Restore the terminal state saved before client mode was enabled.
///
/// @return This function does not return a value.
void resetTerm( void )
{
   paneUiLeave();
   if ( flagsConfiguration.shouldUseAnsi )
   {
      printAnsiResetValue();
      fflush( stdout );
   }
   if ( !isTerminalStateSaved )
   {
      return;
   }
#ifdef HAVE_TERMIOS_H
   tcsetattr( 0, TCSANOW, &saveterm );
#else
#ifdef HAVE_TERMIO_H
   ioctl( 0, TCSETA, &saveterm );
#else
   ioctl( 0, TIOCSETN, (char *)&saveterm );
   ioctl( 0, TIOCSETC, (char *)&savetchars );
   ioctl( 0, TIOCSLTC, (char *)&savedLocalTermChars );
   ioctl( 0, TIOCLSET, (char *)&savelocalmode );
#endif
#endif
}

/// @brief Put the terminal into the mode expected by the interactive client.
///
/// @return This function does not return a value.
void setTerm( void )
{
#ifdef HAVE_TERMIOS_H
   struct termios tmpterm;
#else
#ifdef HAVE_TERMIO_H
   struct termio tmpterm;

#else
   struct sgttyb tmpterm;
   struct tchars temporaryTermChars;
   struct ltchars tmpltchars;
   int tmplocalmode;

#endif
#endif

   getWindowSize();

   if ( flagsConfiguration.shouldUseAnsi )
   {
      printAnsiDisplayStateValue( lastColor, color.background );
      fflush( stdout );
   }

   titleBar();
#ifdef HAVE_TERMIOS_H
   if ( !isTerminalStateSaved )
   {
      tcgetattr( 0, &saveterm );
   }
   tmpterm = saveterm;
   tmpterm.c_iflag &= ~( INLCR | IGNCR | ICRNL );
   tmpterm.c_iflag |= IXOFF | IXON | IXANY;
   tmpterm.c_oflag &= ~( ONLCR | OCRNL );
   tmpterm.c_lflag &= ~( ISIG | ICANON | ECHO );
   tmpterm.c_cc[VMIN] = 1;
   tmpterm.c_cc[VTIME] = 0;
   tcsetattr( 0, TCSANOW, &tmpterm );
#else
#ifdef HAVE_TERMIO_H
   if ( !isTerminalStateSaved )
   {
      ioctl( 0, TCGETA, &saveterm );
   }
   tmpterm = saveterm;
   tmpterm.c_iflag &= ~( INLCR | IGNCR | ICRNL );
   tmpterm.c_iflag |= IXOFF | IXON | IXANY;
   tmpterm.c_oflag &= ~( ONLCR | OCRNL );
   tmpterm.c_lflag &= ~( ISIG | ICANON | ECHO );
   tmpterm.c_cc[VMIN] = 1;
   tmpterm.c_cc[VTIME] = 0;
   ioctl( 0, TCSETA, &tmpterm );
#else
   if ( !isTerminalStateSaved )
   {
      ioctl( 0, TIOCGETP, (char *)&saveterm );
   }
   tmpterm = saveterm;
   tmpterm.sg_flags &= ~( ECHO | CRMOD );
   tmpterm.sg_flags |= CBREAK | TANDEM;
   ioctl( 0, TIOCSETN, (char *)&tmpterm );
   if ( !isTerminalStateSaved )
   {
      ioctl( 0, TIOCGETC, (char *)&savetchars );
   }
   temporaryTermChars = savetchars;
   temporaryTermChars.t_intrc = -1;
   temporaryTermChars.t_quitc = -1;
   temporaryTermChars.t_eofc = -1;
   temporaryTermChars.t_brkc = -1;
   ioctl( 0, TIOCSETC, (char *)&temporaryTermChars );
   if ( !isTerminalStateSaved )
   {
      ioctl( 0, TIOCGLTC, (char *)&savedLocalTermChars );
   }
   tmpltchars = savedLocalTermChars;
   tmpltchars.t_suspc = -1;
   tmpltchars.t_dsuspc = -1;
   tmpltchars.t_rprntc = -1;
   ioctl( 0, TIOCSLTC, (char *)&tmpltchars );
   if ( !isTerminalStateSaved )
   {
      ioctl( 0, TIOCLGET, (char *)&savelocalmode );
   }
   tmplocalmode = savelocalmode;
   tmplocalmode &= ~( LPRTERA | LCRTERA | LCRTKIL | LCTLECH | LPENDIN | LDECCTQ );
   tmplocalmode |= LCRTBS;
   ioctl( 0, TIOCLSET, (char *)&tmplocalmode );
#endif
#endif
   isTerminalStateSaved = 1;
   paneUiEnterIfEligible();
}

/// @brief Decide whether title-bar updates should be emitted right now.
///
/// @return `true` when title-bar updates are enabled and supported.
static bool shouldUpdateTitleBar( void )
{
   if ( !flagsConfiguration.shouldEnableTitleBar ||
        flagsConfiguration.isScreenReaderModeEnabled )
   {
      return false;
   }

   return terminalSupportsTitleBarUpdates();
}

/// @brief Suspend the client process and restore the terminal around the stop.
///
/// @return This function does not return a value.
void suspend( void )
{
   noTitleBar();
   resetTerm();
   kill( 0, SIGSTOP );
   setTerm();
   titleBar();
   printf( "\r\n[Continue]\r\n" );
   if ( oldRows != getWindowSize() && oldRows != -1 )
   {
      sendNaws();
   }
}

/// @brief Detect whether the current terminal supports title-bar updates.
///
/// @return `true` when the terminal is one of the supported title-capable terminals.
static bool terminalSupportsTitleBarUpdates( void )
{
   const char *ptrTerm;
   const char *ptrTermProgram;

   ptrTerm = getenv( "TERM" );
   ptrTermProgram = getenv( "TERM_PROGRAM" );

   if ( ptrTermProgram != NULL &&
        ( strcmp( ptrTermProgram, "Apple_Terminal" ) == 0 ||
          strcmp( ptrTermProgram, "iTerm.app" ) == 0 ) )
   {
      return true;
   }
   if ( ptrTerm == NULL )
   {
      return false;
   }
   if ( strncmp( ptrTerm, "xterm", 5 ) == 0 ||
        strncmp( ptrTerm, "screen", 6 ) == 0 ||
        strncmp( ptrTerm, "tmux", 4 ) == 0 )
   {
      return true;
   }

   return false;
}

/// @brief Update the terminal title with the current connection state.
///
/// @return This function does not return a value.
void titleBar( void )
{
   char aryTitle[80];

   if ( !shouldUpdateTitleBar() )
   {
      return;
   }

   snprintf( aryTitle, sizeof( aryTitle ), "%s:%d - BBS Client %s (%s)",
             aryCommandLineHost, cmdLinePort,
             BUILD_VERSION, "Unix" );
   printf( "\033]0;%s\007", aryTitle );
   fflush( stdout );
}
