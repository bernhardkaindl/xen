# Claims design and guest-guide documentation

## The introducing document must describe the available API

**The suspicion.** This patch describes `XEN_DOMCTL_claim_memory`, `memory_claim_t`, claim replacement and new OCaml bindings, although the preceding core provides separate set/get domctls.

**Disposition.** It also describes `ERANGE`, fixed-array storage and offlining recall that do not match the implementation.
Those contracts depend on the following v8 documentation update for correction, so this patch is not independently correct.

**Targeted mitigation.** Fold the v8 contract corrections into this introducing patch.
Keep genuinely independent follow-up changes separate;
do not roll back the correct combined documentation.
