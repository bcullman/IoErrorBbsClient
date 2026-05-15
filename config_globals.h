/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONFIG_GLOBALS_H_INCLUDED
#define CONFIG_GLOBALS_H_INCLUDED

#include "defs.h"

extern char aryConfigFileName[PATH_MAX]; // config.toml filename
extern char aryTempFileName[PATH_MAX];   // bbstmp filename (usually ~/.bbstmp)
extern bool isConfigFileReadOnly;        // set if config.toml is read-only
extern FILE *ptrConfigFile;              // file descriptor of config.toml
extern FILE *tempFile;                   // file pointer to above

extern char aryBbsHost[64];         // name of bbs host (bbs.iscabbs.com)
extern char aryCommandLineHost[64]; // name of bbs host from command line
extern unsigned short bbsPort;      // port to connect to
extern unsigned short cmdLinePort;  // port to connect to from command line

extern char aryAwayMessageLines[6][80]; // Away from keyboard message
extern char aryKeyMap[128];             // key remapping array
extern char aryShell[PATH_MAX];         // Shell command launched by the client
extern int awayKey;                     // Hotkey for isAway from keyboard
extern int browserKey;                  // Hotkey to launch web browser
extern int capture;                     // Capture status
extern int captureKey;                  // Toggle text capture key (" captureKey)
extern int commandKey;                  // Hotkey for local command sequences
extern int quitKey;                     // hotkey to quit (commandKey quitKey)
extern int shellKey;                    // hotkey for shelling out (" shellKey)
extern int suspKey;                     // hotkey for suspending (" suspKey)

#endif // CONFIG_GLOBALS_H_INCLUDED
