# Patch 2: release per-node claims

Release and lifetime:
- Checked the patch independently and consulted the corresponding Opus, Astra and Haiku reports.
- The loop exits before dereferencing a NULL claims array. The inclusive global count is released exactly once. Detachment is serialized by `heap_lock`; freeing occurs after dropping both locks.
- Preserve the author's accepted early exit: the final assertion verifies exhaustion of the cached sum, not an independent sum of every array element.
- Added topic separators around the release loop without moving the assertion or changing control flow.

Follow-through:
- Installation must preserve the cached sum and exclude dying domains. Node-map rollback must not remove a node holding a positive claim; these checks belong with the later installer.

Verification:
- no compile tests requested or run.