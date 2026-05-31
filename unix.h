/*
 * Copyright (C) 2024-2026 Stilgar
 * Copyright (C) 1995-2003 Michael Hampton
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This is where all the system-specific #include files go, and all the #ifdefs
 * for portability to different Unix systems belong here and in unix.c.
 */
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined( sun ) && defined( unix ) && !defined( FIONREAD ) && !defined( __svr4__ )
#define __svr4__
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#ifdef HAVE_TERMIO_H
#include <termio.h>
#else
#ifdef HAVE_SGTTY_H
#include <sgtty.h>
#endif
// If neither is present, punt
#endif
#endif

#ifdef _AIX
#include <sys/select.h>
#endif

#ifdef NeXT
#include <libc.h>
#endif

#if defined( AMIX ) || defined( __svr4__ )
#include <sys/filio.h>
#endif
