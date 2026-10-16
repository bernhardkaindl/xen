# Patch 4: install NUMA memory claims

Installer review:
- Validation bounds each incremental sum by `max_pages` before narrowing. Duplicate, reserved, zero-sized and invalid-node requests fail before published state changes.
- Global admission uses guarded subtraction; successful admission bounds subsequent counter increments. Node admission uses a 64-bit expression with input bounded by the domain limit and existing claims bounded by physical page counts.
- Candidate allocation, pointer replacement and freeing preserve ownership. Fully redeemed arrays may remain attached safely until replacement or release.
- Hot-add rollback clears a newly online node only before `transfer_pages_to_heap()`. A positive claim cannot pass admission on that node before it has free heap pages, closing the online-node-loop concern from patches 2 and 3.
- Consulted the corresponding Opus, Astra, Haiku and earlier GPT summary sections. Preserved previously approved layouts and the supported serialized domain-build scope.

## Suggested commit-message diff

Verification:
- An arithmetic model passed 648 boundary cases for the widened assignment guard and reproduced unsigned-int wrap in the old expression. This does not execute Xen or establish reachability of the extreme counterexample.
- Editor diagnostics and whitespace checks passed. Compile and runtime tests were not run.