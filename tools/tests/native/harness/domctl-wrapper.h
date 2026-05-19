/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Wrapper around xen/common/domctl.c for the allocator test framework.
 *
 * This file includes the real domctl.c directly in its translation unit.
 *
 * It allows to pass hypercalls from xc_domain to the do_domctl()
 * hypercall handler bin xen/common/domctl.c, which is the entry
 * point for domctl commands in the Xen hypervisor.
 *
 * Copyright (C) 2026 Cloud Software Group
 */
#ifndef TOOLS_TESTS_NATIVE_HARNESS_DOMCTL_WRAPPER_H
#define TOOLS_TESTS_NATIVE_HARNESS_DOMCTL_WRAPPER_H
#ifdef TEST_WRAP_XEN_COMMON_DOMCTL_C
#include "domctl-shim.h"
#pragma GCC diagnostic push
/* Header guards for xen/common/domctl.c inclusion. */
#define _ASM_HW_IRQ_H
#define __XEN_HYPERCALL_H__
#pragma GCC diagnostic ignored "-Wunused-parameter"
#ifdef __clang__
#pragma clang diagnostic ignored "-Wfor-loop-analysis"
#endif
#include "../../xen/common/domctl.c"
#pragma GCC diagnostic pop
#endif
#endif
