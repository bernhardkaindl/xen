.. SPDX-License-Identifier: CC-BY-4.0

*******************
Development History
*******************

.. note:: This section provides historical context on the development of
   NUMA-aware claims, including previous implementations and feedback received,
   to give a better understanding of the design decisions made in the current
   implementation.

The initial `implementation of single-node claims <v1_>`_ (by Alejandro Vallejo)
introduced node-exact claims, allowing :term:`domain builders` to claim memory
on one :term:`NUMA node`. It passed a NUMA node in the node bits of the
:c:expr:`xen_memory_reservation.mem_flags`
field of the pre-existing claims hypercall :ref:`XENMEM_claim_pages` and, by
adding the flag ``d->claim_node`` and updating it to the passed node, defined
the target of the claim as either the specified NUMA node or global memory.

.. sidebar:: Feedback and suggestions for multi-node claim sets

   The initial implementations of single-node claims received feedback from the
   community, with multiple suggestions to extend the API to support `multi-node
   claim sets <v1m_>`_. This feedback highlighted the need for a more flexible
   and extensible design that could accommodate claims on multiple NUMA nodes.

This design was relatively simple and allowed for a quick implementation of
single-node claims, but it had limitations in terms of flexibility and future
extensibility.

The `v2 series added a hypercall API for multi-node claims <v2_>`_, opening the
door to future multi-node claim sets and further work in that direction.

The `v3 series refactored and improved the implementation <v3_>`_, protecting
claimed memory against parallel allocations by other domain builders.

Between v3 and v4, `Roger Pau Monné and Andrew Cooper developed and merged
several critical fixes <fix1_>`_ for Xen's overall claims implementation.
These fixes also allowed Roger to improve the implementation for redeeming
claims during domain memory allocation. In turn, this enabled a
fully working implementation that protected claimed memory against parallel
allocations by other domain builders.

With the `v4 series <v4_>`_, we submitted the combined work that completed the
fixes for protecting claimed memory on NUMA nodes. The review process indicated
that supporting multiple claim sets would require a `redesign <v4-03_>`_ of
claim installation and management, which led to this design document.

Acknowledgements
----------------

The claim sets design builds on the single-node claims implementation
described above and the feedback it generated. The following people
should be acknowledged for their contributions:

- *Alejandro Vallejo* for initiating the single-node NUMA claims series.
- *Roger Pau Monné* for merging critical fixes and proposing the initial
  multi-node claim-sets specification that inspired this design.
- *Andrew Cooper* for integrating and validating the work internally,
  helping to stabilise and productise the single-node implementation.
- *Jan Beulich* for providing reviews that led to many improvements.
- *Bernhard Kaindl* for maintaining the single-node series, initiating
  the multi-node implementation and authoring this design document.
- *Marcus Granado* and *Edwin Török* for contributing design input,
  providing guidance, debugging and testing of single-node implementations.

.. _fix1:
   https://lists.xenproject.org/archives/html/xen-devel/2026-01/msg00164.html

.. _v1:
   https://patchew.org/Xen/20250314172502.53498-1-alejandro.vallejo@cloud.com/
.. _v1m:
   https://lists.xenproject.org/archives/html/xen-devel/2025-06/msg00484.html
.. _v2:
   https://lists.xen.org/archives/html/xen-devel/2025-08/msg01076.html
.. _v3:
   https://patchew.org/Xen/cover.1757261045.git.bernhard.kaindl@cloud.com/
.. _v4:
    https://lists.xenproject.org/archives/html/xen-devel/2026-02/msg01387.html
.. _v4-03: https://patchwork.kernel.org/project/xen-devel/
   patch/6927e45bf7c2ce56b8849c16a2024edb86034358.1772098423
   .git.bernhard.kaindl@citrix.com/
