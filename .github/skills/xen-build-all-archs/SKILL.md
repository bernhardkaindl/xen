---
name: xen-build-all-archs
description: "Build Xen for x86_64, arm32 and arm64 (hypervisor) and x86_64 tools, to validate changes across architectures. Use when asked to compile Xen, cross-compile for Arm, check that a change builds on all architectures, or audit architecture-specific code before submission."
---

# Build Xen for all architectures

Run from the repository root. `yes ''` accepts the Kconfig defaults for any
new options, so the build never stops at a prompt.

| Target | Command |
|---|---|
| x86_64 Xen | ``yes '' \| make -C xen -j`nproc` `` |
| x86_64 tools | ``yes '' \| make -C tools -j`nproc` `` |
| arm32 Xen | ``yes '' \| make XEN_TARGET_ARCH=arm32 CROSS_COMPILE=arm-linux-gnueabihf- -j`nproc` -C xen`` |
| arm64 Xen | ``yes '' \| make XEN_TARGET_ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j`nproc` -C xen`` |

Cross-compiling only works for the hypervisor (`-C xen`), not for tools.
Use only these commands, and do not change the cross-compiler prefixes.

## Faster checks

- For one object: `make -C xen common/page_alloc.o`.
- For one tools library: `make -C tools/libs/ctrl`. If linking fails because
  sibling libraries such as `libxencall.so` are missing, the objects still
  compiled; build `tools` fully to link.

## Config-dependent code

The default `xen/.config` may disable options such as `CONFIG_XSM` or
`CONFIG_XSM_FLASK`. Check with
`grep -E 'CONFIG_XSM|CONFIG_<option>' xen/.config`. Code behind a disabled
option is not compiled; state this when reporting build results.

## Architecture audit

When a change touches common code or widens types (for example page
counters), check the Arm side too:

- Inspect Arm-specific counterparts, such as `xen/arch/arm/mmu/p2m.c` and
  the width of `paging.p2m_total_pages`.
- Look for Arm-specific narrow types that would matter if 64-bit Arm
  guests grew past 16 TiB.
- Build arm32 and arm64 with the commands above.
