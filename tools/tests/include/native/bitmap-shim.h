/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Bitmap implementations for native Xen tests.
 *
 * Copyright (C) 2026 Cloud Software Group
 */

#ifndef TOOLS_TESTS_NATIVE_HARNESS_BITMAP_SHIM_H
#define TOOLS_TESTS_NATIVE_HARNESS_BITMAP_SHIM_H

#define hweightl(value) __builtin_popcountl(value)
#undef LITTLE_ENDIAN /* bitmap.c relies on __LITTLE_ENDIAN */
#include <common/bitmap.c>
#undef __XEN_TOOLS__
#include <lib/find-next-bit.c>
#define __XEN_TOOLS__
#include <lib/generic-ffsl.c>
#include <lib/generic-flsl.c>

#endif
