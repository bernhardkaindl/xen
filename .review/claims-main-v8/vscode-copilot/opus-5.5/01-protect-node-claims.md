# 01 - xen/mm: protect per-node claims in get_free_buddy()

**Result:** the code is correct.

## Code review

- `get_free_buddy()` is the only consumer:
  - `alloc_heap_pages()` calls it with `heap_lock` held,
  - which protects `node_claimed_pages[]` and `d->claims`, as the comments say.

- `node` is below `MAX_NUMNODES` whenever the loop body runs:
  - the requested node is checked before the loop,
  - and the node selection at `next_node:` returns NULL instead of continuing with `node >= MAX_NUMNODES`.
  - So the new `node_avail_pages[node]`, `node_claimed_pages[node]` and `d->claims[node]` accesses are in bounds:
    - like the existing `avail[node]`.

- "No functional change" holds:
  - `d->claims` is NULL and `node_claimed_pages[]` is zero,
  - so `available == node_avail_pages[node]`.
  - `node_avail_pages[node]` changes in lockstep with `avail[node][zone]` in `alloc_heap_pages()`, `reserve_offlined_page()` and `free_heap_pages()`:
    - so it is the sum over the zones of the node.
  - When it is below the request, every zone would fail the existing `avail[node][zone]` check.

- The clamp prevents the wrap of the unsigned subtraction:
  - after `XEN_SYSCTL_page_offline_op` reduced free memory below the claims.

- `goto next_node` reaches the `MEMF_exact_node` check:
  - so an exact-node allocation fails like for a node without enough free memory.

- `MEMF_no_refcount` and anonymous (`d == NULL`) allocations get no credit:
  - which matches the host-wide check in `alloc_heap_pages()`.

- Style:
  - the label is indented by one blank,
  - the ternary continuation is aligned with the start of the expression,
  - the new comments are single sentences.
  - The `sched.h` member aligns its name with the neighbouring members:
    - `*` in column 20, name in column 21.

- Re-traced the `for ( ; ; )` loop:
  - every `continue` and the fall-through reach the top with `node < MAX_NUMNODES`.
  - An offline requested node has `node_avail_pages[node] == 0` and no claim:
    - so it is skipped like before.

## Decisions

- "may use its node claim":
  - "its claim on this node" would be more precise,
  - but the comment would exceed 80 columns.
  - With `d->claims[node]` on the next line, the meaning is clear.

- The commit message does not name `node_claimed_pages[]` and `d->claims`:
  - Their comments in the diff explain them,
  - and "nothing installs per-node claims yet" says why they stay empty.

- The availability formula max(0, F - C) + c was accepted by earlier reviews.

- `struct domain`:
  - the new 8-byte pointer `claims` sits between the 4-byte `outstanding_pages` and `max_pages`.
  - Offsets on 64-bit, from the 8-byte aligned `tot_pages`:

  | Config (x86)                      | Before  | After (patch)     |
  |-----------------------------------|---------|-------------------|
  | default (no MEM_PAGING/SHARING)   | 1 hole  | 1 hole, size +8   |
  | exactly one of MEM_PAGING/SHARING | no hole | 2 holes, size +16 |
  | both                              | 1 hole  | 1 hole, size +8   |

  - Arm64, RISC-V and PPC have neither option, and behave like the default.
  - Moving `claims` above `outstanding_pages` would grow the size by 8 in all configurations without a hole:
    - but would put the per-node detail before the total,
    - and MEM_PAGING and MEM_SHARING are UNSUPPORTED.
  - Patch 2 fills the hole with `node_claims`, so the placement stays.
