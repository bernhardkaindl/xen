# Reviewer concerns and rationale index

This document indexes concise evidence-based dispositions and clarification candidates, not an internal reasoning transcript. Each concern's evidence and targeted mitigation are recorded in the owning patch note so a maintainer can read the review in patch order.

| Patch note | Questions addressed |
| --- | --- |
| [01 - node protection](01-protect-node-claims.md) | Is the preparatory check neutral? What does the offlining clamp actually guarantee? |
| [02 - release](02-release-node-claims.md) | Can an offline node strand a claim? Why detach before freeing? |
| [03 - redemption](03-redeem-numa-claims.md) | Can fallback exhaust the wrong node or underflow its remainder? What happens after failed assignment? |
| [04 - installation](04-set-claim-entries.md) | What really bounds the per-node addition? Which checks belong to the helper? Does the page lock cover allocations awaiting assignment? |
| [05 - set domctl](05-domctl-set-memory-claims.md) | Is the old count-field blocker fixed? Does creation gating serialize builders? Is release still authorized? |
| [06 - set wrapper](06-libxc-set-memory-claims.md) | Are preallocated pages allowed? What is the creation-state restriction? Should the accepted size-multiplication concern be reopened? |
| [07 - get domctl](07-domctl-get-memory-claims.md) | Can release race with the snapshot? Which guest copies may be partial? Does the FLASK permission imply the dummy-XSM exception? |
| [08 - get wrapper](08-libxc-get-memory-claims.md) | Does shrinking the copy length affect freeing? Are entries and count equally trustworthy after EFAULT? |
| [09 - independent cleanup](09-release-leftover-claims.md) | Can release fail? Does one-time cleanup prohibit later legacy claims? |
| [00 - cover letter](00-cover-letter.md) | How can the toolstack find unclaimed per-node memory, and why is cleanup separate? |
| [11 - test follow-up](11-claims-tests.md) | Does native unpause execute creation cleanup? Why was the old EBUSY expectation stale? |
