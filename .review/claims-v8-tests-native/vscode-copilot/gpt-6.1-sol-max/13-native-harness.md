# Native harness

## Production inclusion does not eliminate model limitations

**The suspicion.** Including production translation units can be mistaken for exercising production lifecycle, synchronization, XSM policy and real guest-copy faults.

**Disposition.** The harness includes production handlers and libxc bounce code, but locks and RCU are no-ops.
XSM decisions and guest faults are injected.
Allocator contents and NUMA geometry are synthetic.
The lifecycle helper models claim-relevant transitions rather than executing production `domain.c`.

**Targeted mitigation.** Keep these boundaries explicit.
Production lifecycle integration, concurrent synchronization, real XSM policy and protected-page fault containment require distinct coverage.

## Make the message's coverage claim precise

**The suspicion.** The message names nonexistent `xc_domain_claim_memory()` and overstates what the modeled environment exercises.

**Disposition.** The setter is `xc_domain_set_memory_claims()`.
Production handler and bounce-code inclusion does not replace the synthetic surrounding infrastructure.

**Targeted mitigation.** Use the following proposed message diff.

```diff
--- a/commit-message
+++ b/commit-message
@@ -6 +6 @@
-- domctl calls e.g. xc_domain_claim_memory()
+- domctl calls e.g. xc_domain_set_memory_claims()
@@ -12,0 +13,5 @@
+The tests include production handlers and libxc bounce code, but replace
+locking, XSM decisions, guest access and lifecycle infrastructure with
+native models. They do not establish concurrency, real policy or
+production domain-lifecycle coverage.
+
```
