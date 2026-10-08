# 03 - xen/mm: redeem per-node claims for allocations

**Result:** refactored. This patch now adds the three helpers, named after the redemption steps: `domain_redeem_node_claim()`, `domain_redeem_host_claim()` and `domain_redeem_other_node_claims()`. The step comments say "redeem", and the v8 changes name the helpers. Step 1 of the redemption order in the commit message was reworded. The code diff below omits the release loop in `domain_set_outstanding_pages()`, which belongs to patch 2.

## Code review

- `alloc_heap_pages()` redeems `r = min(d->outstanding_pages, request)` pages in the order of the message:
  - the claim on `node`, which is `page_to_nid(pg)` at this point,
  - then the host-wide claim,
  - then claims on other nodes.

- `domain_redeem_node_claim()` returns 0 while `d->node_claims` is 0:
  - so it never dereferences a NULL `d->claims`.
  - It clamps to `d->claims[node]`,
  - and its four ASSERT()s bound the five subtractions.

- `ASSERT(!pages)` in `domain_redeem_other_node_claims()` holds:
  - With `O` outstanding pages, `N` per-node claims and `H = O - N`:
    - step 1 redeems `a`,
    - step 2 redeems `min(r - a, H)` (step 1 reduces `O` and `N` alike, so `H` is unchanged).
  - A remainder is left only if step 2 used up `H`:
    - and then it is `r - a - H <= O - a - H = N - a`,
    - the per-node claims that are left.
  - They are on other nodes:
    - because a remainder after step 1 means that step 1 used up the claim on `node`.

- No underflow:
  - `domain_redeem_host_claim()` clamps to `H` after asserting `O >= N`,
  - and the existing `BUG_ON()` covers `outstanding_claims`.

- "No functional change" holds:
  - with `d->node_claims == 0`, step 1 returns 0,
  - step 2 redeems all of `r`, exactly the two removed lines,
  - and `domain_redeem_other_node_claims()` leaves its loop at once.

- The wording "a ref-counted allocation (e.g. by a domain builder) to build a domain with outstanding claims":
  - is the precise wording of the skill's terminology section,
  - kept.

- Builds for x86_64 (debug, FLASK), also at the series tip.

## Considered, not edited

- "/* outstanding is the number of claimed pages to redeem. */" sits between the declaration and the `BUG_ON()`:
  - Re-reviewed: it is accurate,
  - explains the variable that the three steps decrement,
  - and the layout was chosen in an earlier review, so it stays.

## Re-review

- No new findings:
  - Re-derived `ASSERT(!pages)` for the case where the allocation comes from a node without a claim and the host-wide claim is zero:
  - step 3 redeems all `min(O, request)` pages from the other nodes,
  - because `N == O` there.
