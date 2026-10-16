# Patch 7: libxc set-memory-claims wrapper

No wrapper implementation finding:
- The zeroed domctl supplies reserved padding; `do_domctl()` sets the interface version and does not retry `EFAULT`. The input-only bounce accepts the const array and is freed after the call.
- NULL with zero count skips the entry-array bounce. Non-NULL with zero count can still incur a local allocation failure before release reaches Xen.
- A huge count can wrap the byte-size product on 32-bit tools, but Xen bounds the unchanged count before reading entries. No accepted request receives a truncated buffer through this path.
- The generated version map exports all global symbols. No separate export-list addition is missing.
- Consulted the corresponding Opus, Astra, Haiku and earlier GPT notes. Their pause-count wording is obsolete; the current interface restricts installation to unfinished domain creation.

Commit-message correction:
- Correct the suggested-by surname without changing the attribution or committing anything.

Verification:
- Source-traced the bounce helpers, domctl helper and generated export rule. No compile or runtime tests were run.