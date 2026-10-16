# 01 - protect per-node claims in `get_free_buddy()`

**Result:** No actionable defect found. No code or commit-message change suggested.

## Reviewed scope

Reviewed the first patch on `claims-main-v8` against its parent, guided by the corresponding Opus review and the other models' availability concerns. Later implementation and test-branch changes are not prerequisites for this conclusion.

## Code review

- At this patch, no path installs `d->claims` or increments `node_claimed_pages[]`. The new check therefore rejects only when the entire node has fewer free pages than requested; the existing zone checks necessarily reject that node too.
- A rejected node reaches the existing `MEMF_exact_node` handling. The next-node paths either establish an in-range node or return before another array access.
- Claim credit is conditional on a target domain and a ref-counted allocation, consistent with the existing host-wide reservation boundary.
- The claim pointer's placement is not changed: the next patch adds the related counter, and reordering here would add churn without improving the accounting.

## Does the clamp preserve claims after memory offlining?

**The suspicion.** Clamping the difference between free and claimed pages could appear to guarantee that all outstanding reservations remain satisfiable even after free memory is offlined.

**Disposition.** The clamp prevents unsigned subtraction from wrapping. It does not recreate removed capacity. With a domain's node claim bounded by the total claims on that node, the subsequent addition cannot wrap: its result is at most the node's free pages when free pages cover total claims, and otherwise equals the domain's unsigned-int node claim. The unchanged zone and buddy checks still require actual free memory.

**Targeted mitigation.** No source change. Retain the existing offlining explanation and do not describe it as a guarantee against capacity loss; that limitation is shared with legacy claims.

## Suggested commit-message diff

None.
