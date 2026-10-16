# Patch 8: get-memory-claims domctl

No new implementation finding:
- RCU keeps the domain alive; `heap_lock` protects the complete snapshot and excludes pointer detachment. No domain-array pointer survives the snapshot.
- Capacity is clamped to the maximum possible entry count. A short private buffer may contain a prefix, but `ENOBUFS` suppresses entry copy-back while publishing the required count through the domctl.
- Compound literals zero entry padding, and the output allocation is zeroed. Positive host-wide claims precede positive per-node claims in node order.
- An entry-array copy fault suppresses domctl copy-back. A fault during the final domctl copy is different: that copy may have partially modified the guest buffer.
- FLASK reuses `DOMAIN__GETDOMAININFO`; dummy XSM takes `XSM_PRIV`, not the separate `XSM_XS_PRIV` path used by the older getdomaininfo operation.
- Consulted the corresponding Opus, Astra, Haiku and earlier GPT reviews. No authorization or snapshot-lifetime regression was established.

Commit-message precision:
- Replace the awkward possessive wording with the concrete existing disclosure: getdomaininfo already exposes the aggregate claim count. Correct the suggested-by surname.

Verification:
- Source-traced snapshot ownership, both guest-copy stages and authorization. No compile or runtime tests were run.