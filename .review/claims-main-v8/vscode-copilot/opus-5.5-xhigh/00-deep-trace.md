# NUMA memory claims: deeper allocator and ownership traces

## Purpose, scope and evidence

This document records four source traces from the extra-high-effort
pre-review. It is a companion to the review notes, not a proposed change to
the public interface. Each chapter introduces the feature, gives a route
through the code, and distinguishes the accounting conclusion from the
assumptions needed to use it.

The submission being reviewed is the first eight commits after master
`63d0e8cc842975e786455f8a2e6502e85685f258`. At the time of writing, on
`claims-main-v8`, they are:

| Patch | Commit | Purpose |
|-------|--------|---------|
| 1 | `617bf3e7b1` | Protect per-node claims |
| 2 | `085a4fe8db` | Release per-node claims |
| 3 | `c03ee3a28b` | Redeem per-node claims |
| 4 | `32066393a1` | Validate and install claim sets |
| 5 | `e886e92c47` | Set domctl |
| 6 | `2635c3a980` | Set libxc wrapper |
| 7 | `d858f129bf` | Get domctl |
| 8 | `1cec7c51e7` | Get libxc wrapper |

The previous review ended at `2e85b2c8f9`; its relevant allocator and
ownership implementations are unchanged by the subsequent comment fixes
and rebase. The exceptional-path implementation files discussed here are
also unchanged between master and the eight-patch tip. The allocator and
claim-accounting changes are the subject of the series.

The scope is an **initially paused domain build**, with claim installation
serialized against allocations for that domain. A domain builder is the
management process creating the domain; the domain being built is not
running and requesting those allocations itself.

These are source-review conclusions. No compilation, native tests or Xen
runtime tests were run for this document. Numerical examples below are
accounting illustrations, not observed runtime executions.

### How to retrace the review

Links point to repository files rather than revision-sensitive line
numbers. Search for the named functions and branches within those files.
To inspect the exact reviewed version, use, for example:

```sh
git show 1cec7c51e7:xen/common/memory.c
git show 1cec7c51e7:xen/common/page_alloc.c
git diff 63d0e8cc84 1cec7c51e7 -- xen/common/memory.c
```

The last command produces no diff: it distinguishes the existing deferred
scrubbing and exchange paths from the new allocator accounting. The same
comparison is empty for the domain lifecycle, grant-transfer and x86
sharing implementation files cited below.

## Accounting vocabulary

A **claim** reserves a quantity of free physical memory for later
allocation. It does not immediately give the domain ownership of particular
pages, create guest mappings, or guarantee a contiguous high-order extent.
Redeeming a claim means reducing that reservation when memory is allocated.
Freeing memory does not automatically recreate a claim.

The relevant counters are:

| Name | Meaning in this document |
|------|--------------------------|
| `T = d->tot_pages` | Internal page charge, including extra pages |
| `E = d->extra_pages` | Extra pages excluded from the ordinary reservation |
| `A = domain_tot_pages(d)` | Ordinary reservation count: `T - E` |
| `O = d->outstanding_pages` | Remaining host-wide plus per-node claims |
| `M = d->max_pages` | Maximum ordinary reservation count |

The definition of `A` is in
[sched.h](../../../../xen/include/xen/sched.h), and the count changes are in
[page_alloc.c](../../../../xen/common/page_alloc.c). In particular, do not
confuse the internal `d->tot_pages` with the reservation count returned by
`domain_tot_pages()`.

For an ordinary allocation of `q` pages, heap allocation redeems
`r = min(O, q)`. Successful assignment subsequently adds `q` to `A`.
If claims cover the allocation, `A + O` is unchanged across those two
steps. If they do not, claims become zero and assignment's maximum-page
check bounds `A`. The conclusion is **preservation of the domain limit**,
not that the sum can never grow.

`MEMF_no_refcount` needs particular care. In the paths below it suppresses
ordinary reservation accounting and claim credit/redemption, but its
caller may pre-account a replacement or mark an auxiliary allocation as
extra. It does not mean that a page has no reference count or that every
use of the flag has the same accounting outcome.

Protection of other domains' claimed free memory also assumes the free-memory
invariants held before allocation. Memory offlining is a separate, already
recorded limitation, not resolved by these traces.

## 1. Memory exchange: replace pages without enlarging the reservation

### Introduction

Memory exchange replaces physical pages already belonging to a domain
with an equal number of different physical pages. For example, the new
pages may need to be contiguous or satisfy an address constraint. The
domain is changing the physical backing of its memory, not asking to
increase its reservation.

Xen therefore needs temporary ownership bookkeeping: it removes the
inputs from the domain's page list, obtains replacement pages, and assigns
the replacements. Counting the replacements as entirely new guest memory
would charge the same reservation twice.

### Source route and findings

Start with `memory_exchange()` in
[memory.c](../../../../xen/common/memory.c), then follow `steal_page()` in
[x86/mm.c](../../../../xen/arch/x86/mm.c) and the heap/assignment functions
in [page_alloc.c](../../../../xen/common/page_alloc.c).

1. The exchange request checks that input and output contain equal total
   page counts, allowing their extent orders to differ.
2. Input pages are taken with
   `steal_page(d, page, MEMF_no_refcount)`. The x86 implementation removes
   ownership and the page-list entry but does not decrement `T` with this
   flag. The reservation count remains charged during the exchange.
3. Outputs use
   `alloc_domheap_pages(d, order, MEMF_no_owner | memflags)`.
   `alloc_domheap_pages()` makes `MEMF_no_owner` imply
   `MEMF_no_refcount` before calling the heap allocator.
4. The global admission check and `get_free_buddy()` give these output
   allocations **no credit for the domain's claims**. The redemption block
   in `alloc_heap_pages()` also excludes them. Replacement memory must
   come from capacity not reserved by claims, even if this domain has
   outstanding claims.
5. Once the outputs have been obtained, the anonymous input pages are
   freed and the outputs assigned with `MEMF_no_refcount`. Assignment
   attaches ownership without adding the already charged reservation
   again. For a successful exchange of a live domain, `A` and `O` are
   unchanged.

For a failed chunk before replacement commits, inspect the `fail:` path:
it reassigns stolen inputs with `MEMF_no_refcount` and frees anonymous
outputs. Neither operation replenishes claims. Dying-domain cleanup has a
separate count adjustment; the successful-live-domain statement must not
be applied to that teardown case.

This is a trace of the x86 implementation, whose `steal_page()` also
rejects unsupported paging modes. It is not a claim that every
architecture or guest mode supports exchange.

### Why this is not a concern within the review scope

Exchange does not represent a new ordinary reservation allocation, so
not redeeming claims is appropriate. The new per-node admission rule
retains the existing no-credit treatment and protects other domains'
claimed free memory. An exchange can fail for lack of unclaimed
replacement capacity despite claims; those claims are not a promise of
temporary workspace for exchanging already owned pages.

## 2. Auxiliary pages: owned by a domain but outside its reservation

### Introduction

Not every page allocated for a domain is part of the guest memory being
populated by the builder. Xen also allocates supporting structures, such
as VM-trace buffers and device-emulation request pages. Some such pages
are domain-owned for lifetime management but deliberately excluded from
the ordinary reservation count.

This is different from exchange: an auxiliary allocation can add owned
pages, but it adds them to the extra-page category.

### Source route and findings

Concrete callers are `vmtrace_alloc_buffer()` in
[domain.c](../../../../xen/common/domain.c) and
`ioreq_server_alloc_mfn()` in
[ioreq.c](../../../../xen/common/ioreq.c). Follow their
`alloc_domheap_pages(..., MEMF_no_refcount)` calls into
[page_alloc.c](../../../../xen/common/page_alloc.c).

1. Heap admission grants no host-wide or per-node claim credit, and heap
   allocation does not redeem claims.
2. With a non-NULL domain and without `MEMF_no_owner`,
   `alloc_domheap_pages()` marks the returned pages `PGC_extra`.
3. In `assign_pages()`, that marker increments `E` by `q` and clears the
   local `MEMF_no_refcount` flag so the internal total `T` is also
   incremented by `q`.
4. Consequently `A = (T + q) - (E + q) = T - E`: the ordinary
   reservation and outstanding claims are unchanged.
5. If the internal total would overflow, assignment undoes its extra-page
   increment before returning failure. Normal freeing of owned extra
   pages decrements both counts, again leaving `A` unchanged.

### Why this is not a concern within the review scope

The absence of redemption is not missing accounting: these pages are
intentionally outside the reservation used to validate the claim request.
They still consume physical heap memory and can fail to allocate. Giving
them no claim credit prevents them from spending memory reserved for
ordinary population, including this domain's own remaining claims.

This also establishes why `MEMF_no_refcount` must not be explained as
being used only for page exchange. The same flag participates in a
different, explicit accounting scheme for extra pages.

## 3. Deferred scrubbing: one allocation across several executions

### Introduction

Before previously used physical memory can safely become guest memory,
Xen may need to erase its old contents. This is **scrubbing**: it prevents
one domain from observing data left by another.

Clearing a large extent can take long enough that Xen should yield the
CPU instead of finishing all the work in one hypercall execution. During
initial domain creation, Xen can allocate the extent first, scrub part of
it, save the unfinished allocation, and resume later.

The central distinction is between:

- **Heap allocation:** the pages leave the free pool and claims are
  redeemed.
- **Domain assignment:** the pages gain domain ownership and enter its
  reservation count.
- **Guest mapping:** the pages are inserted into the guest's physical
  address map after scrubbing and assignment.

For this path those are deliberately separate steps.

### Source route and findings

Start with the `MEMF_keep_scrub` contract in
[mm.h](../../../../xen/include/xen/mm.h). Then read `populate_physmap()`,
`stash_allocation()`, `get_stashed_allocation()` and the
`args.preempted` handling in `do_memory_op()` in
[memory.c](../../../../xen/common/memory.c). Connect them to
`alloc_domheap_pages()`, `alloc_heap_pages()` and `assign_pages()` in
[page_alloc.c](../../../../xen/common/page_alloc.c).

1. Before `creation_finished`, ordinary heap-backed population adds
   `MEMF_no_scrub | MEMF_keep_scrub`. The first flag defers allocator
   scrubbing; the second retains the per-page `PGC_need_scrub` information
   and prevents automatic domain assignment.
2. These flags do **not** imply `MEMF_no_refcount`. An ordinary allocation
   still passes through heap admission and redeems its claims once, using
   the allocation-node, host-wide, then other-node order.
3. `alloc_domheap_pages()` returns the unassigned extent. Its pages are
   already out of the free pool, but `A` has not yet increased.
   `MEMF_keep_scrub` prevents pages awaiting erasure from being exposed
   through normal domain ownership and `get_page()`.
4. `populate_physmap()` checks each page's scrub bit and scrubs those that
   need it. After batches of 256 scrubbed pages, if more of the extent
   remains and `hypercall_preempt_check()` requests a yield, it saves the
   extent and the next index with `stash_allocation()` and sets
   `a->preempted`.
5. `do_memory_op()` creates a continuation using the number of completed
   extents. The partly scrubbed extent is not yet completed, so the
   continuation returns to that extent.
6. On resumption, `get_stashed_allocation()` checks its order and, when
   requested, its exact node. It hands the same extent and saved scrub
   index back to the caller and clears the domain's stash.
   **There is no second heap allocation and no second redemption.**
7. When scrubbing finishes, `assign_page()` adds the extent to the
   domain's reservation. Only then does `guest_physmap_add_page()` map it.

The allocation/stash comment calls this single-threaded domain creation.
Its phrase "domain assigned page" is not evidence that ownership has
already been installed: the `MEMF_keep_scrub` contract and assignment
branch show that the extent is still unassigned.

### Accounting timeline

Suppose the allocation is `q` pages and redeems `r = min(O, q)`.

| Point in the operation | Free-pool change | Reservation | Claims |
|------------------------|------------------|-------------|--------|
| Before allocation | None | `A` | `O` |
| Allocated, not assigned | `-q` | `A` | `O - r` |
| Stashed or resumed | No additional change | `A` | `O - r` |
| Successfully assigned | No additional change | `A + q` | `O - r` |

The apparently missing pages during the middle rows are not lost: they
are the caller's or the stash's allocated extent, awaiting assignment.
They do not re-enter the free pool merely because execution is preempted.

### Why claim installation must not overlap the unfinished extent

Both claim setters validate against `A`, not against `A` plus any
unassigned extent. Holding `page_alloc_lock` stabilizes the counters while
validating, but does not make an unassigned extent part of those counters.
The new setter rejects existing outstanding claims, but that is not
sufficient once an allocation has exhausted them.

For example, assume sufficient unclaimed memory and a valid 1024-page
extent allocation:

1. Initially `M = 5120`, `A = 4096`, `O = 1024`.
2. Heap allocation redeems all 1024 claims, and scrubbing is preempted.
   The extent is stashed; now `A = 4096`, `O = 0`.
3. A second builder activity incorrectly installs a fresh 1024-page
   claim. Validation sees `A + 1024 = M`, and there is no old outstanding
   claim to reject.
4. The original population resumes and assigns its extent. Assignment
   allows `A + 1024 = M` because it does not add outstanding claims to
   that check. The resulting `A = 5120`, `O = 1024` exceeds the intended
   combined limit.

This illustrates a **violation of the documented serialization
requirement**, not a supported use of the interface. Serialization must
cover the unfinished allocation through its continuation and assignment,
not merely avoid simultaneous execution inside the claim setter. A
paused guest does not serialize two builder activities.

The same allocation/assignment split exists without preemption; deferred
scrubbing can make the interval persist across executions. The legacy
host-wide setter also bases its calculation on `domain_tot_pages()`, so
this is not a new per-node-specific window. See the existing
[SCE-1 discussion](../../../claims-v8/main/04-set-claim-entries.md).

### Abandonment, failure and teardown

- If a later request has a different extent order or an incompatible
  exact-node requirement, `get_stashed_allocation()` frees the old extent
  and returns no allocation. A newly allocated replacement follows the
  ordinary heap path; that is a new allocation, not duplicate redemption.
- `stash_allocation()` frees the passed extent if a stash already exists
  or the domain is dying. Failed assignment also frees the extent.
- Such freeing does not replenish claims. The allocator's existing
  comment explains the build-abort assumption for assignment failure.
- `domain_teardown()` calls `domain_pending_scrub_free()` in
  [domain.c](../../../../xen/common/domain.c), freeing any remaining stash.
- The first controller unpause checks for pending scrub work. If present,
  it crashes the domain, frees the stash and returns `-EBUSY`, rather than
  starting the guest with unfinished memory preparation.

### Why this is not a concern within the review scope

For a serialized build, the extent redeems once, remains reserved as
allocated physical memory while scrubbing, and is assigned once. The
continuation is not another request for physical pages. Existing cleanup
and first-unpause handling account for an abandoned stash.

The trace confirms correct redemption placement and explains the existing
caller-side serialization requirement. It does **not** establish safety
for installing claims over an unfinished population or justify removing
that requirement. Supporting that case would require a separate
accounting/concurrency change.

## 4. Grant transfers and sharing: ownership is not heap allocation

### Introduction

A grant transfer hands ownership of an already allocated page from one
domain to another. It differs from temporarily granting access to a page:
ownership and the reservation charge actually move.

Memory sharing lets guests refer to a common physical page. If a page
later becomes private again, Xen can sometimes return ownership of the
existing page rather than allocate a copy. A special sharing owner,
`dom_cow`, holds such pages during the shared state.

Neither ownership change necessarily obtains memory from the free heap.
That matters because heap allocation is where claims are redeemed.

### Source route: grant transfer

Read `gnttab_transfer()` in
[grant_table.c](../../../../xen/common/grant_table.c), followed by
`steal_page()` in [x86/mm.c](../../../../xen/arch/x86/mm.c) and
`assign_pages()` in [page_alloc.c](../../../../xen/common/page_alloc.c).

1. The donor page is stolen with flags zero, decrementing the donor's
   ordinary reservation.
2. The recipient is checked for liveness, reservation headroom and
   internal-total overflow. The headroom check uses `domain_tot_pages()`,
   not that count plus outstanding claims.
3. The recipient's internal total is incremented under its
   `page_alloc_lock` before transfer preparation drops that lock.
4. Assignment uses `MEMF_no_refcount` because the recipient's page charge
   was already reserved. It attaches ownership without counting twice.
   No recipient claim is redeemed.
5. If preparation or assignment fails, that pre-charge is undone. If
   address constraints require an intermediate replacement allocation,
   it uses `MEMF_no_owner`, receiving no claim credit.

Ordinary transfer moves an already allocated page; it does not reduce
free heap capacity. Its separate reservation check is therefore not
evidence that allocator redemption has run.

### Source route: sharing and return to private ownership

Read `page_make_sharable()`, `page_make_private()` and
`__mem_sharing_unshare_page()` in
[mem_sharing.c](../../../../xen/arch/x86/mm/mem_sharing.c).

- Successful, non-validation-only `page_make_sharable()` changes ownership
  to `dom_cow`, removes the domain's page-list entry and decrements its
  internal total. It does not restore claims.
- `page_make_private()` changes ownership back, increments the domain's
  total and adds the page-list entry without a heap allocation. It does
  not redeem claims.
- In the unshare path, returning the last remaining guest mapping to
  private ownership uses that existing-page route. When other mappings
  remain, allocating a private copy instead uses
  `alloc_domheap_page(d, 0)` and therefore the ordinary allocation and
  redemption path. **Not every unshare bypasses redemption.**

### Why this is not a new concern within the review scope

These existing ownership routes already bypass redemption of legacy
host-wide claims. The per-node series does not change them, and an
existing-page ownership change does not itself take another domain's
claimed free pages from the heap.

However, **unchanged behavior is not a universal combined-limit proof**.
A recipient can gain allocated pages without reducing its claims; the
grant-transfer check alone does not enforce `A + O <= M`.
The existing-page sharing route likewise does not run allocator
redemption. A paused target is not, by itself, proof that management-driven
sharing operations cannot occur.

The review conclusion is consequently limited to ordinary population in
the documented, serialized initial build. It does not certify combining
active claims with arbitrary ownership transfers or sharing workflows.
If that combination becomes a supported use, those reservation changes
need explicit claim-aware accounting or a restriction, in a separately
reviewed change. These traces are not a reason to silently expand this
series into a grant/sharing rewrite.

## Overall conclusion

The four traces explain why heap allocation, page ownership and the
domain's ordinary reservation cannot be treated as interchangeable events.
Exchange preserves an existing reservation, auxiliary pages use extra-page
accounting, and deferred scrubbing delays assignment of a single
already-claimed allocation. Ownership transfers expose the boundary of
the allocator-based guarantee rather than a new per-node implementation
defect.

No additional defect in the eight-patch implementation was established
under the stated build scope. The most important practical qualification
is that claim installation must not overlap an allocation still awaiting
assignment, including one paused in a scrub continuation. This document
records the reasoning and its limits; it is not a substitute for runtime
validation or a general audit of all memory-management paths.
