.. SPDX-License-Identifier: CC-BY-4.0

Development
###########

.. note::

   This section provides historical context on the development
   of NUMA-aware claims, including previous implementations and
   feedback received, to give a better understanding of the
   design decisions made in the current implementation.

Version history
---------------

The initial `implementation of single-node claims <v1_>`_ by Alejandro Vallejo
used the legacy claims hypercall :c:macro:`XENMEM_claim_pages` and passed a
NUMA node in the existing NUMA node bits of
:c:expr:`xen_memory_reservation.mem_flags`. This added the field
``d->claim_node`` to ``struct`` :c:struct:`domain`, which defined the target
node for the domain's claims.

.. epigraph::

   Roger Pau Monné reviewed it and proposed an `initial multi-node claim-sets
   specification <v1m_>`_ that inspired this design:

    The interface here seems to be focused on domains only being allowed to
    allocate from a single node, or otherwise you must first allocate memory
    from a node before moving to the next one (which defeats the purpose of
    claims?).

    I think we want to instead convert ``d->outstanding_pages``
    into a per-node array, so that a domain can have outstanding
    claims for multiple NUMA nodes?

    The hypercall interface becomes a bit awkward then, as the toolstack has
    to perform a different hypercall for each memory claim from a different
    node (and rollback in case of failure). Ideally we would need to introduce
    a new hypercall that allows making claims from multiple nodes in a single
    locked region, as to ensure success or failure in an atomic way.

    -- Roger Pau Monné

This led to the `v2 <v2_>`_ and `v3 <v3_>`_ series. Their new hypercall API
anticipated multi-node claims, but the implementation still tracked one claim
target per domain using ``d->claim_node``. The array-shaped API was preparation
for multi-node accounting, not evidence that multiple targets could already
be installed together.

.. sidebar:: Feedback and suggestions for multi-node claim sets

   The initial implementations of single-node claims received
   feedback from the community, with multiple suggestions to
   extend the API to support `multi-node claim sets <v1m_>`_.
   This feedback highlighted the need for a more flexible
   design that could accommodate claims on multiple NUMA nodes.

Between v3 and v4, `Roger Pau Monné and Andrew Cooper developed and merged
several critical fixes <fix1_>`_ for Xen's overall claims implementation.
These fixes also allowed Roger to improve the implementation for redeeming
claims during domain memory allocation. With a further suggestion by
Bernhard Kaindl, this enabled a fully working implementation that protected
claimed memory against parallel allocations by other domain builders.

.. glossary:

v4 series
 With the `v4 series <v4_>`_, we submitted the combined work that completed the
 fixes for protecting claimed memory on NUMA nodes. The review process indicated
 that supporting multiple claim targets would require a `redesign <v4-03_>`_ of
 claim installation and management, which led to this design document.

v5 series
 The `v5 series <v5_>`_ implemented the `Claim Sets Design Version 1 <d1_>`_
 with support for multiple claim records per domain, and with a terminology
 of "consuming claims" for the process of redeeming claims during domain memory
 allocation and "retiring claims" for releasing claims for the low-level
 action of reducing the number of claimed pages for redeeming claims for an
 allocation and when destroying a domain. Recalling claims after offlining
 was a design concern discussed at that time; the current v8 implementation
 does not recall claims when offlining reduces the available capacity.

v6 series
 The `v6 series <v6_>`_ implemented the `Claim Sets Design Version 2 <d2_>`_.
 The only difference between the two versions is that with design version 2,
 the initial term `"consuming claims"` was changed to `"redeeming claims"`
 and the term `"retiring claims"` was changed to `"deducting claims"`.

v7 series
 The `v7 series <v7_>`_ renamed the functions handling redeeming claims
 but retained the overall semantics of claim redemption and deduction.

v8 series
----------------
 The v8 series implemented dynamic allocation of the
 :c:member:`domain.claims` array, refactored claims installation,
 and split setting and getting claims into separate operations.

 Claim installation and retrieval use small, dedicated functions:
 :c:func:`domain_set_claims()` installs claims and
 :c:func:`domain_get_claims()` reads them for the new set/get domctls, retaining
 :c:func:`domain_set_outstanding_pages()` for the legacy
 :c:macro:`XENMEM_claim_pages` interface and claim cleanup.

The v8 interface uses separate :c:macro:`XEN_DOMCTL_set_memory_claims` and
:c:macro:`XEN_DOMCTL_get_memory_claims` commands. Non-empty claims are accepted
only during domain creation and only when no claims are already outstanding;
an empty set releases claims. The per-node claims array is allocated on demand
and freed when claims are explicitly released. A fully redeemed all-zero
array can remain attached until release or a later installation replaces it.
An independent cleanup patch releases leftover build claims when controller
unpause first reduces the pause count to zero. It is not required by the
eight-patch NUMA-claims series and does not restrict later legacy claim requests.

Testing
-------

The basis of the `v4 series <v4_>`_ is included in the XenServer XS9 preview
release, and besides functional product testing, it has been tested to
meet the performance expectation of customers from improved NUMA placement.

With the `v6 series <v6_>`_, a comprehensive set of functional system tests
was added to the submission. Also, `a separate host-side integration test
suite <tv2_>`_ for validating the `v6 series <v6_>`_ was posted.

Further development
-------------------

The host-wide :c:var:`total_avail_pages` and :c:var:`outstanding_claims`
counters are ``unsigned long`` in the v8 baseline. The domain's claim counters
and per-node array entries remain ``unsigned int``, with validation bounding
requests before those stores. The `earlier widening proposal <u1_>`_ is
historical context, not an outstanding prerequisite for this series.

Acknowledgements
----------------

The claim sets design builds on the single-node claims implementation
described above and the feedback it generated. The following people
should be acknowledged for their contributions:

- *Edwin Török* for developing the `initial best-effort NUMA placement
  feature in the XAPI toolstack <xapi_>`_, which inspired the initial
  implementation of NUMA-aware claims, and his work in productizing and
  validating the integration of NUMA claims with the XAPI toolstack.

- *Alejandro Vallejo* for starting the development of the NUMA claims series.

- *Jan Beulich* for providing review suggestions that led to many improvements.

- *Roger Pau Monné* for reviewing the initial implementation, `proposing
  the initial multi-node claim-sets specification <v1m_>`_, developing and
  merging `critical fixes <fix1_>`_ upstream that enabled product-quality
  support for single-node claims which is the basis of the multi-node
  claim sets implementation.

- *Andrew Cooper* for integrating and validating the work internally,
  helping to stabilise and productise the single-node implementation.

- *Bernhard Kaindl* for collaborating on the single-node implementation,
  developing the claim sets hypercall since version 2, designing and
  implementing the multi-node claim sets design, the functional system-level
  test suite and the host-side integration test suite for validating the
  claim sets implementation.

- *Marcus Granado* for leading the development effort inside XenServer for
  productising the single-node claims implementation, for providing feedback
  and suggestions for improving the design and implementation. This included
  coordinating the work of multiple contributors and stakeholders, integrating
  the work into XenServer products and ensuring it meets customer requirements.

.. _xapi: https://xapi-project.github.io/new-docs/toolstack/features/NUMA
.. _fix1:
   https://lists.xenproject.org/archives/html/xen-devel/2026-01/msg00164.html
.. _v1:
   https://patchew.org/Xen/20250314172502.53498-1-alejandro.vallejo@cloud.com/
.. _v1m:
   https://lists.xenproject.org/archives/html/xen-devel/2025-06/msg00484.html
.. _v2: https://lists.xen.org/archives/html/xen-devel/2025-08/msg01076.html
.. _v3: https://patchew.org/Xen/cover.1757261045.git.bernhard.kaindl@cloud.com/
.. _v4:
    https://lists.xenproject.org/archives/html/xen-devel/2026-02/msg01387.html
.. _v4-03: https://patchwork.kernel.org/project/xen-devel/
   patch/6927e45bf7c2ce56b8849c16a2024edb86034358.1772098423
   .git.bernhard.kaindl@citrix.com/
.. _d1:
   https://bernhard-xen.readthedocs.io/en/claim-sets-v1-design/designs/claims
.. _d2:
   https://bernhard-xen.readthedocs.io/en/claim-sets-v2-design/designs/claims
.. _v5: https://patchwork.kernel.org/project/xen-devel/list/?series=1078053
.. _v6: https://patchwork.kernel.org/project/xen-devel/list/?series=1081139
.. _v7: https://lore.kernel.org/xen-devel/cover.1778272036.git.bernhard.kaindl@citrix.com/
.. _tv2: https://patchwork.kernel.org/project/xen-devel/list/?series=1083329
.. _u1: https://patchwork.kernel.org/project/xen-devel/list/?series=1084344
