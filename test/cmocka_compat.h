/*
 * Copyright (C) 2026 Stilgar
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TEST_CMOCKA_COMPAT_H
#define TEST_CMOCKA_COMPAT_H

#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-attributes"
#endif

#include <cmocka.h>

#if defined( __clang__ )
#pragma clang diagnostic pop
#endif

#endif
