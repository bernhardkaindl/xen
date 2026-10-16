Patch 3: Redeem per-node claims
===============================

When allocating pages to a domain, the increase of
:c:func:`domain_tot_pages` must be compensated by a matching reduction in
claims to ensure :c:member:`domain.max_pages` is not exceeded. This is
called redeeming claims.

With per-node claims added, redeeming claims must consider the node
of the allocation, the host-wide claim, and, because per-node claims
do not steer allocations to claimed nodes, a domain's other claims.

Redeem the claims in this order:

1. Redeem the claim from the domain's claim on the node of the
   allocated pages, as far as possible.
2. Redeem any remainder from the domain's host-wide claim.
3. Redeem any final remainder from the domain's claims on other
   nodes, keeping allocated pages plus claims within
   :c:member:`domain.max_pages`.

.. literalinclude:: /../xen/common/page_alloc.c
   :language: C
   :start-at: unsigned long outstanding = min
   :end-at: domain_redeem_other_node_claims(d, node, outstanding);
   :dedent: 8
   :emphasize-lines: 6,8,10

Existing host-wide redemption behavior is preserved unchanged.

Review questions
----------------

Is there a window between redeeming claims and assigning pages to the domain?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** This code redeems claims before the allocated pages
are assigned to the domain. Isn't there a risk of redeeming claims for
pages that might fail to be assigned to the domain?

**Disposition.** 
Yes, this pre-existing comment before redeeming claims acknowledges it
and clarifies that Xen maintainers consider redeeming the allocation before
the pages are actually assigned to the domain as not a problem:

.. literalinclude:: /../xen/common/page_alloc.c
   :language: C
   :start-at: if ( d && d
   :end-at: }
   :emphasize-lines: 12-16

.. _claiming-and-page-assignment:

How does claiming interact with page assignment?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** As there is a window between redeeming claims and actually
assigning pages to the domain, there could be a risk of race conditions.

**Disposition.**
Yes, this is an inherent design property of Xen's page allocator, stemming
from deferring assignment of pages to a domain until after the allocation
itself, in the case of deferred scrubbing of the allocated pages, this
assignment happens only after deferred scrubbing has completed.

The domain builder is responsible for ensuring that it does not cause race
conditions to occur. Failure to do so does not compromise the integrity of
the domain's page allocation itself.

.. literalinclude:: /../xen/common/page_alloc.c
   :language: C
   :start-at: int assign_pages
   :end-at: nrspin_lock
   :emphasize-lines: 10

Inside the case for ref-counted page assignment:

.. literalinclude:: /../xen/common/page_alloc.c
   :language: C
   :start-at: unsigned int tot_pages = domain_tot_pages(d);
   :end-at: rc = -E2BIG;
   :emphasize-lines: 3,11


1. Race conditions related to the timing of claim redemption and page
   assignment. As allocation happens under a different lock, there is
   a window where claims have been redeemed but pages have not yet been
   assigned to the domain. Due to the individual locks, it is possible
   for other threads to observe the domain in an intermediate state where
   claims have been redeemed but pages have not yet been assigned.
   When assigning pages, :c:member:`domain.max_pages` is used to ensure that
   the domain does not exceed its allocated capacity. It is the responsibility
   of the domain builder to ensure that the assignment of pages respects
   the limit set by :c:member:`domain.max_pages`. Failure to do so only
   affects the domain builder's ability to assign pages correctly, but it
   does not compromise the integrity of the domain's page allocation itself.
2. Likewise, the window between allocation and assignment which does not
   carry a lock affects the checks for installing claims for the domain.
   Within the window, claims installation sees the reduction of free memory
   from the allocation, it does not see the actual assignment of pages to
   the claims installation exceed :c:member:`domain.max_pages` by the number
   of pages within the window between allocation and assignment. Building
   a domain is a privileged operation, and it is the responsibility of the
   domain builder install the claims before allocating pages to the domain.


The domain builders avoid race conditions using the following steps:

1. They set :c:member:`domain.max_pages` to a value that accommodates the
   total number of pages the domain needs.
2. They ensure that claims cover the pages they intend to allocate for the
   domain to populate its :term:`physmap`.

   .. literalinclude:: /../tools/libs/guest/xg_dom_x86.c
      :language: C
      :start-at: Try to claim pages
      :end-at: target_pages
      :dedent: 4
      :emphasize-lines: 1-4

3. They start to redeem claims only after completing these steps.

   .. literalinclude:: /../tools/libs/guest/xg_dom_x86.c
      :language: C
      :start-at: Allocate memory for HVM
      :end-at: memflags,
      :dedent: 4
      :emphasize-lines: 14