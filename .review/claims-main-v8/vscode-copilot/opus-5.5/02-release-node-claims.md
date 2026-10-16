# 02 - xen/mm: release per-node claims in domain_set_outstanding_pages()

## Code review

- The release loop stops once `d->node_claims` is 0:
  - before it reads `d->claims[node]`,
  - so it never dereferences a NULL `d->claims`.
  - `ASSERT(!d->node_claims)` after the loop checks the cached sum against the entries.
  - The ASSERT()s before the subtractions state that per-node claims are part of `d->outstanding_pages` and `node_claimed_pages[node]`.

- A node goes offline only in the error path of x86 `memory_add()`:
  - before any of its memory reached the heap,
  - so it cannot hold a claim (to confirm with the claim checks of patch 4).
  - Hence, `for_each_online_node` visits every node that can hold a claim.

- `d->outstanding_pages` includes the per-node claims:
  - so the existing `outstanding_claims -= d->outstanding_pages` releases them as well.
  - The `d->claims[]` entries need not be cleared because the array is freed.
  - The function detaches `d->claims` under both locks and frees it after dropping them:
    - because `xvfree()` can take `heap_lock`.
  - `xvfree(NULL)` is fine on the paths that do not detach.

- `domain_kill()` calls `domain_set_outstanding_pages(d, 0)`:
  - so a dying domain loses its array before `free_domain_struct()`.
  - Installing claims on a dying domain must be refused by the later installer:
    - check at patch 5: the domctl runs under `domctl_lock` like `domain_kill()`.

- "No functional change" holds:
  - without per-node claims, the loop exits at once,
  - and `d->claims` stays NULL.

- In this patch, `alloc_heap_pages()` still reduces `d->outstanding_pages` without reducing per-node claims:
  - which would break the ASSERT()s once claims exist.
  - Nothing installs them before patch 5, so every commit is correct:
    - as the cover letter says ("no functional effect until patch 5").

- `struct domain`:
  - `node_claims` fills the 4-byte hole that patch 1 added before `claims` (see 01),
  - so the default layout has no hole now.

- Style:
  - `for_each_online_node ( node )` like `for_each_vcpu ( d, v )`,
  - `node` declared in the block that uses it.

- Builds for x86_64 (debug, FLASK).

## Decisions

- The loop ends once `d->node_claims` reaches 0:
  - so `ASSERT(!d->node_claims)` misses entries left on later nodes if the cached sum equals the sum of the earlier entries.
  - Visiting every node would need a separate NULL check of `d->claims`,
  - and the old helpers had the same limit.
  - The author decided that there is no need to check all entries.

## Re-review

- No new findings:
  - The `domain_kill()` call site (`DOMDYING_alive` case) and `XENMEM_claim_pages` with 0 pages release through the same path,
  - and `domain_kill()` is called only from `XEN_DOMCTL_destroydomain`.
