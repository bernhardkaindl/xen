# Install NUMA memory claims

## Installation must not overlap unfinished allocations

**The suspicion.** Holding `d->page_alloc_lock` during validation could be mistaken for accounting for every allocation already in flight for the domain.

**Disposition.** Pages removed from the heap but awaiting assignment are absent from `domain_tot_pages(d)`.
The lock preserves the counters read by validation but does not account for those pages, including scrub continuations.

**Targeted mitigation.** The domain builder must serialize claim installation with complete allocation operations.
Do not broaden the atomic-installation guarantee to include pending assignment.
