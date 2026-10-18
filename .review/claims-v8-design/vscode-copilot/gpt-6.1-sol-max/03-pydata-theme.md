# PyData theme

## The version switcher assumes root hosting

**The suspicion.** The local version entry uses `url: "/"`, which assumes the documentation is served at the origin root.

**Disposition.** The local autobuild command fits that assumption.
Read the Docs supplies hosted URLs separately, but the local base does not support an arbitrary static-site subpath.

**Targeted mitigation.** Determine whether non-root static hosting is supported.
If it is, use the actual deployment base URL instead of assuming `/`;
do not guess a deployment URL.
