/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CLIENT_GLOBALS_H_INCLUDED
#define CLIENT_GLOBALS_H_INCLUDED

#include "defs.h"

extern char aryAutoName[21]; // Automatic login name
extern char aryEditor[80];   // name of aryEditor to invoke
extern char aryEscape[2];
extern char aryLastName[MAX_USER_NAME_HISTORY_COUNT][21]; // last username received in post or X
extern char aryMyEditor[80];                              // name of aryUser's preferred aryEditor
extern int aryReplyRecipient[21];
extern char aryUser[80];         // username
extern int childPid;             // process id of child process
extern Color color;              // color transformations
extern Flags flagsConfiguration; // Miscellaneous flags
extern bool isAutoLoggedIn;      // Has autologin been done?
extern bool isAway;              // away from keyboard?
extern bool isLoginShell;        // whether this client is a login shell
extern bool isXland;             // X Land - auto-fill-in-recipient
#ifdef USE_POSIX_SIGSETJMP
extern sigjmp_buf jumpEnv; // Jump buffer for child-process return flow
#else
extern jmp_buf jumpEnv; // Jump buffer for child-process return flow
#endif

extern int lastColor;     // last color code received from BBS
extern int lastPtr;       // Current aryLastName index
extern int oldRows;       // previous value of rows
extern int rows;          // number of rows on aryUser's screen
extern int sendingXState; // automatically sending an X?
extern int version;       // Client version number
extern queue *xlandQueue; // X Land name queue

#endif // CLIENT_GLOBALS_H_INCLUDED
