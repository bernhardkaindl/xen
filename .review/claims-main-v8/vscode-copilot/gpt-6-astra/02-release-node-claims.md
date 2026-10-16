# Patch 2: release per-node claims

No new finding:
- The zero-count path removes per-node contributions before removing the domain's inclusive global total. A repeated release exits the node loop before dereferencing a NULL array.
- Detachment occurs under `heap_lock`; `xvfree()` occurs after both locks are dropped. Error and non-release paths leave `old_claims` NULL.
- Existing `domain_kill()` and zero-count `XENMEM_claim_pages` callers use this path. No new claim installer is reachable at this revision.
- Consulted the corresponding Opus and Haiku notes. Preserve the author's accepted early-exit layout: the final assertion checks exhaustion of the cached sum, not an independent rescan of every array entry.

Follow-through checks:
- The installer must reject dying domains and preserve the cached sum. Positive claims must not survive removal of a node from the online map; the known node-offline operation is hot-add rollback, to be checked against installation.

Verification:
- Source review and whitespace check only; compilation was not rerun.