# Moving documents into the hypervisor guide

## The new index depends on the following patch

**The suspicion.** This patch adds `claims/index` to `docs/designs/index.rst` before adding the referenced document.

**Disposition.** The claims-document patch introduces the target later in the series.
This patch therefore has a dangling toctree reference on its own;
the combined tree has a valid target.

**Targeted mitigation.** Move this toctree addition into the patch introducing the claims document.
The proposed diffs below describe patch placement, not removing the valid link from the combined tree.

## Proposed series correction

```diff
--- a/docs/designs/index.rst
+++ b/docs/designs/index.rst
@@ -15,3 +15,2 @@ and for those interested in the internal workings of Xen.
    launch/index
    cache-coloring
-   claims/index
```

```diff
--- a/docs/designs/index.rst
+++ b/docs/designs/index.rst
@@ -15,2 +15,3 @@ and for those interested in the internal workings of Xen.
    launch/index
    cache-coloring
+   claims/index
```

## The move patch needs an author sign-off

**The suspicion.** This patch has no `Signed-off-by:` trailer.

**Disposition.** An upstream submission requires the author's certification;
a reviewer cannot provide it on the author's behalf.

**Targeted mitigation.** Confirm and add the appropriate sign-off when preparing this patch.

## Proposed message amendment

```diff
--- a/commit-message
+++ b/commit-message
@@ -1 +1,3 @@
 Sphinx docs: Move the unsorted documents to the hypervisor guide
+
+Signed-off-by: Bernhard Kaindl <bernhard.kaindl@citrix.com>
```
