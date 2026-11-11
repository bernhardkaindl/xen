# 03: xen/mm: redeem per-node claims for allocations

## Review report

### New Functions

**domain_redeem_node_claim()**
- Redeems up to `@pages` from a specific node's claim
- Returns pages actually redeemed
- Validates invariants with `ASSERT`s
- Correctly updates all counters

**domain_redeem_host_claim()**
- Redeems up to `@pages` from host-wide claim
- Returns pages actually redeemed
- Correctly calculates host claim portion
- Updates counters appropriately

**domain_redeem_other_node_claims()**
- Redeems exactly `@pages` from nodes other than @node
- Uses assertion to ensure all pages are redeemed
- Iterates through online nodes

### alloc_heap_pages() Changes

**Redemption sequence:**
1. Redeem from domain's claim on allocation node first
2. Redeem remainder from host-wide claim
3. Redeem remainder from other nodes' claims

This order is correct and ensures claims are consumed in the right priority.

**Comment is clear:**
```c
/*
 * To keep the sum of assigned pages and claims within the domain's
 * limit, redeem the remainder from host-wide claims of the domain,
 * and if needed, from other per-node claim pools of the domain.
 */
```

## Summary

**Status: ✅ APPROVED**

All changes in this commit are technically correct and well-documented. No issues found.
