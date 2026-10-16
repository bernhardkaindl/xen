.. SPDX-License-Identifier: CC-BY-4.0

Redeeming
#########

.. contents:: Table of Contents
    :backlinks: entry
    :local:

After :c:func:`get_free_buddy()` finds suitable pages,
:c:func:`alloc_heap_pages()` updates the free-page counters and redeems claims
in the same :c:var:`heap_lock` critical region. For ref-counted domain requests,
the redemption amount is the smaller of the allocation size and the target
domain's outstanding total. It is redeemed in the following order:

1. Redeem claims from :c:expr:`domain.claims[alloc_node]` on the allocation
   node, up to the remaining redemption amount and the size of that claim.
2. Redeem any remaining amount from the
   host-wide claims (:c:member:`domain.outstanding_pages` -
   :c:member:`domain.node_claims`), up to the size of the host-wide claim.
3. Redeem any remaining amount from other per-node claims rather than retaining a
   reservation for pages already allocated to the domain.

The allocation node is the node of the returned pages, not necessarily the
requested node. Only the target domain's claims are redeemed.

Relationship to the domain's max_pages limit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When setting claims, :c:func:`domain_set_claims()` checks that
``domain_tot_pages(d) + request->total_pages`` fits within ``d->max_pages``.
The request specifies additional outstanding pages, not the existing claims.
This is an installation check, not an immutable lifetime invariant: the
toolstack can later lower ``max_pages``. Page assignment separately checks
the current limit.

At memory allocation time, memory might be allocated on a node on which
the domain has not sufficient claims. To ensure that the allocation does
not unfairly exceed :c:member:`domain.max_pages` due to an allocation on
a node without sufficient claims, if they exist, host-wide claims of the
domain or its per-node claims must be redeemed.

A limit check at heap admission would need to observe the domain's page
counts and limit under :c:member:`domain.page_alloc_lock`, taken before
:c:var:`heap_lock`. It would also need to account for allocated pages awaiting
assignment, including scrub continuations; the extra lock alone would not
cover them. The current implementation instead relies on builder serialization
of claim installation with complete allocation operations.

Redemption precedes page assignment. If assignment fails, freeing the pages
does not restore the redeemed claim. For the existing claims model, it was
agreed among Xen maintainers that such a failure aborts the domain build,
releasing all allocated pages and the remaining claims as part of it.
