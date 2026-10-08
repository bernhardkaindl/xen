# 00 - Cover letter: xen/mm: introduce per-node NUMA memory claims

**Result:** wording changes applied to the testing paragraph and the changelog. After the re-check against the final series, a bullet about the pause requirement of patch 5 was added.

## Code review

- The interface list matches patches 5 to 8.

- "Patches 1-4 have no functional effect until patch 5 adds the domctl.":
  - matches the message of patch 4 ("nothing calls domain_set_claim_entries() yet").
  - Patches 1 to 3 only act on per-node claims:
    - which nothing can install before patch 5.

- The patch 4 bullets match the code:
  - smaller functions,
  - `xvzalloc_array()` only when `node_pages` is nonzero,
  - and EINVAL if the domain already has outstanding claims.
