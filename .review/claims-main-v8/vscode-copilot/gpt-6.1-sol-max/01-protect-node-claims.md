# Protect per-node claims in `get_free_buddy()`

## Memory offlining can invalidate reservations

**The suspicion.** The node-availability clamp can be mistaken for preserving reservations after free pages are offlined.

**Disposition.** The clamp prevents unsigned subtraction from wrapping;
it does not replace removed capacity.
Actual zone and buddy checks still require free pages.
The separate `check_low_mem_virq()` subtraction is not protected by this clamp.

**Targeted mitigation.** Keep the offlining limitation explicit.
Any claim recall or low-memory-notification arithmetic change needs a separately scoped change.
