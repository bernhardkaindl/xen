/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Wrapper around xen/common/sysctl.c for the allocator test framework.
 *
 * Includes the real sysctl.c directly in its translation unit.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_WRAPPER_H
#define TOOLS_TESTS_NATIVE_HARNESS_SYSCTL_WRAPPER_H
#ifdef TEST_WRAP_XEN_COMMON_SYSCTL_C
#include "sysctl-shim.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../../xen/common/sysctl.c"
#pragma GCC diagnostic pop
#endif
#endif
