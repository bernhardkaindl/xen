# Follow-up review, 2026-10-17

Reviewed the revised `claims-main-v8` eight-patch series, cover letter and independent creation-cleanup patch against the previous review. No new functional blocker was established within the initially paused, builder-serialized domain-build contract. The earlier patch 4 field-name correction, patch 7 permission rationale, cleanup article correction, source topic separator and patch 5 serialization clarification are applied. The cover letter intentionally keeps cleanup independent; the earlier patch-9 suggestion is superseded.

## Deeper checks

Re-examined candidate-array ownership, fully redeemed array reuse, interaction with legacy host-wide claims, exact redemption bounds, and the transition from creation to first unpause. A retained array contains only zeros once the cached node sum and outstanding total are zero. A later legacy host-only claim leaves that array zero and uses only host redemption; a subsequent new installation replaces and frees the retained array through the existing ownership handover. Release and snapshot readers remain serialized by `heap_lock`.

The added lock comments in patch 4 describe protected data as well as implementation locking. They do not change behavior, and were left in the author's chosen layout rather than churned for brevity. This review does not treat comments as proof of locking; the actual acquisition spans validation and installation, and heap admission remains a separate critical section from page assignment.

First-unpause cleanup neither validates nor resumes a pending allocation continuation. The existing pending-scrub refusal precedes cleanup; builder-side completion and serialization remain required. Release cannot fail through its zero-pages branch. Existing memory-offlining and allocation-failure limitations remain unchanged, not newly certified safe.

## Verification scope

Compile tests, native tests and live-Xen tests were not run. All nine current source-patch diffs pass whitespace checks and added C/header lines are at most 79 columns. The public `nr_claims` member is consistently named from its introduction through every consumer patch. The remaining message suggestion matches the current message text and has valid unified-diff hunk counts.

A finite arithmetic model passed 881,847 admitted allocations with two domains and three NUMA nodes. Each domain's node claims, each node's initially unclaimed pages, and each domain's host-wide claim range from zero to two pages; request sizes range from one to four pages. Starting states must satisfy global and per-node reservation bounds. The model checks the current global admission rule and own-node credit, then redeems actual-node, host-wide and other-node claims in order. Every accepted case preserves exact redemption, other-domain reservations, the node-claim sum and global free-versus-claimed bounds. The three-page requests generalize the arithmetic beyond the allocator's power-of-two requests.

This model does not execute production C, model fragmentation, failed assignment, memory offlining or allocation continuations, or prove concurrent interleavings. Mixed legacy/new lifecycle behavior was source-traced rather than executed. No commits, staging or history edits were performed, and only review notes were edited in this follow-up.
