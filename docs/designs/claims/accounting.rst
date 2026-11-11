.. SPDX-License-Identifier: CC-BY-4.0

Accounting
##########

.. contents:: Table of Contents
   :local:

.. note::
   Claims accounting state is only updated while holding the :c:var:`heap_lock`.
   See :ref:`designs/claims/accounting:Locking of the claims state` for details
   on the locks used to protect the claims accounting state.

This section formalises the internal state and invariants that Xen must
maintain to ensure correctness.


For readers following the design in order, the preceding sections are:

1. :doc:`/designs/claims/design` introduces the overall model and goals.
2. :doc:`/designs/claims/installation` explains how claim sets are installed.
3. :doc:`/designs/claims/protection` describes how claimed memory is protected
   during allocation.
4. :doc:`/designs/claims/redeeming` explains how claims are redeemed when
   allocations succeed.

Overview
^^^^^^^^

.. csv-table:: Claims accounting state
   :header: "Level", "Claims", "Free pages or constraint"
   :widths: 20 40 40

   "Host", "``outstanding_claims``", "``total_avail_pages``"
   "Per-node", "``node_claimed_pages[node]``", "``node_avail_pages[node]``"
   "Domain subtotal", "``d->node_claims``", "Sum of ``d->claims[]`` entries"
   "Domain total", "``d->outstanding_pages``", "Host-wide plus node claims"
   "Domain limit", "Allocated pages plus claims", "Checked at installation"


Total claims and available memory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

 These variables tracking the total claims and available memory in the system
 are aggregates of the actual per-node and per-domain values.

 They are only maintained for efficient checks in the allocator hot paths, to
 quickly determine if an allocation can be satisfied from unclaimed memory or
 if further checks are needed to determine if the claims of the domain can be
 used to free up memory for the allocation. This also ensures that the sum of
 claims do not exceed available memory at installation time.

 The number of unclaimed pages across all nodes in the system is derived as
 :c:var:`total_avail_pages` minus :c:var:`outstanding_claims`.

 .. note::
   By :term:`memory offlining`, e.g., using :term:`XEN_SYSCTL_page_offline_op`,
   or as an effect of background memory scrubbing by Xen or firmware,
   free memory counters are reduced and could fall below outstanding claims.
   To guard against underflow, subtractions added to get the actual available
   memory are carefully checked and guarded against underflow.

 The number of actually available unclaimed pages is then used to:

 - Permit allocation requests if they can be satisfied from unclaimed pages.
 - Validate that claims fit within currently available memory when installed.

 Documentation of the relevant variables follows:

 .. c:var:: unsigned long total_avail_pages

    Free pages accounted by the common heap allocator across all NUMA nodes,
    including capacity reserved by claims. This is not total installed RAM.
    It sums ``node_avail_pages[node]`` for valid node indices; ``MAX_NUMNODES``
    is the array length, not an index into the array.

    These counters aggregate heap zones, not just the zones usable by a given
    request. With ``CONFIG_SEPARATE_XENHEAP``, they include the Xen-only heap,
    which domain-heap allocations cannot use. Claim admission does not reserve
    zone-specific capacity; a later allocation still needs suitable pages in
    its permitted zones.

 .. c:var:: unsigned long outstanding_claims

   The total sum of all claims across all domains.
   :c:var:`outstanding_claims` =
   SUM(:c:var:`domain.outstanding_pages`)

Per-node claims and available memory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

 .. note::
   The per-node available pages and claimed pages are used to validate that
   node-specific claims do not exceed the available memory on each NUMA node.
   This ensures that memory allocation and claim installation respect the
   NUMA node boundaries and available resources.

   Like the global available and outstanding claims, the per-node available
   and claimed pages are carefully checked and guarded against underflow
   caused by :term:`memory offlining`.

 .. c:var:: unsigned long node_avail_pages[MAX_NUMNODES]

   Free heap pages for each NUMA node, including pages reserved by claims.
   Claims reserve counts within this free capacity; Xen does not tag
   individual free pages as claimed. Claims are not an additional page
   category and can exceed the remaining free capacity after offlining.
   This is used for validating that node claims do not exceed the available
   memory on the respective NUMA node.

 .. c:var:: unsigned long node_claimed_pages[MAX_NUMNODES]

   The total claims across all domains for each NUMA node, indexed by node
   ID. This is maintained for efficient checks in the allocator hot paths.

This diagram illustrates the claims accounting state and the invariants:

Accounting diagram
^^^^^^^^^^^^^^^^^^

 .. mermaid:: invariants.mmd
   :caption: Diagram: Claims accounting state and invariants

Claims accounting state for each domain
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

 .. c:struct:: domain

   The main structure representing a domain in Xen. It includes the
   claims accounting state for the domain, including both host-wide
   and node-specific claims, as well as the maximum page limits for the
   domain and the lock protecting the domain's page allocation counts.

   The domain's page counts, including its claims and maximum page limit, are
   currently ``unsigned int``.

   .. c:member:: unsigned int outstanding_pages

      The domain's total claim, representing the number of pages claimed
      for the domain.

   .. c:member:: unsigned int node_claims

      The total of the domain's per-node claims, maintained for efficient
      checks in the allocator hot paths without summing the per-node array.

   .. c:member:: unsigned int *claims

      Per-node claims indexed by node ID. A nonzero per-node claim requires
      an array, but an all-zero array can remain attached after full redemption.
      Xen allocates the array when installing per-node claims and detaches and
      frees it on explicit release, destruction, or replacement by a subsequent
      installation after all outstanding claims have been redeemed.
      Access is protected by :c:var:`heap_lock`.

      The sum of the entries is cached in :c:member:`domain.node_claims`.
      This allows the allocator to derive the domain's host-wide claim as
      :c:member:`domain.outstanding_pages` minus
      :c:member:`domain.node_claims`.

   .. c:member:: unsigned int max_pages

      The maximum for ``domain_tot_pages(d)``, which excludes extra pages.
      Installation checks that count plus requested claims against this limit.
      The toolstack can change the
      limit with ``XEN_DOMCTL_max_mem``; it is not fixed at creation time.

   .. c:member:: rspinlock_t page_alloc_lock

      Lock for checking :c:func:`domain_tot_pages` on top of new claims
      against :c:member:`domain.max_pages` when installing these new claims.
      This recursive spinlock protects domain page counts and limits. Claim
      redemption itself takes only the heap lock and does not acquire it.
      It is taken before :c:var:`heap_lock` when installing claims to ensure a
      consistent locking order and must not be taken while holding
      :c:var:`heap_lock` to avoid deadlocks.

   .. c:member:: nodemask_t node_affinity

      A :c:type:`nodemask_t` representing the set of NUMA nodes the domain
      is affine to. This is used for efficient checks in the allocator hot
      paths to quickly get the set of nodes a domain is affine to for
      memory allocation decisions.

Claims accounting invariants
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   Host and node admission maintain the following capacity bounds while
   offlining has not removed the memory backing existing claims:

 - For all claims, including per-node and host-wide claims:
    :c:var:`outstanding_claims` :math:`\le` :c:var:`total_avail_pages`

 - For per-node claims:
    :c:expr:`node_claimed_pages[alloc_node]` :math:`\le`
    :c:expr:`node_avail_pages[alloc_node]`

   The separate domain-limit check at installation is
   ``domain_tot_pages(d) + requested_claims <= d->max_pages``. It is not
   an unconditional capacity invariant: page offlining does not affect this
   limit check, whereas a later reduction of ``max_pages`` can invalidate it.
   The allocation count also excludes pages removed from the heap but not yet
   assigned. Builder serialization must cover those allocations and their
   continuations. See :doc:`redeeming` for the redemption relationship.

   Offlining memory can invalidate the host and node free-versus-claimed
   relationships. Xen does not recall claims in that case. The per-node
   availability subtraction is clamped to zero; this is not a blanket
   underflow guarantee for every legacy host-wide accounting expression.

Constants
^^^^^^^^^

 .. c:macro:: MAX_NUMNODES

   The maximum number of NUMA nodes supported by Xen. Used for validating
   node IDs in the :c:type:`xen_domctl_memory_claim_t` entries of claim sets.
   When Xen is built without NUMA support, it is 1.

   With NUMA enabled, ``xen/arch/Kconfig`` permits values from 2 to 64 and
   defaults to 64. A 64-node bitmap fits in one 64-bit word, but the claims
   array contains a separate ``unsigned int`` for every configured node.
   These build-time limits do not prescribe a platform's firmware NUMA layout.

 .. c:var:: nodemask_t node_online_map

   A bitmap representing which NUMA nodes are currently online in the system.
   This is used for validating that claims are only made for online nodes and
   for efficient checks in the allocator hot paths to quickly determine which
   nodes are online. Claims can only target online nodes when installed.
   Page offlining reduces free-page counts without necessarily removing the
   node from this map. The current runtime node-offline path rolls back
   unsuccessful hot-add before the new memory reaches the heap; arbitrary
   node hot-remove is not covered by this design.

Types
^^^^^

 .. c:type:: uint8_t nodeid_t

   Internal type for :term:`NUMA node` IDs. ``NUMA_NO_NODE`` is 255, leaving
   numeric IDs 0 through 254 representable; the supported build-time node
   count is separately limited by ``CONFIG_NR_NUMA_NODES``.

   Population hypercalls encode the requested node in the unsigned
   ``mem_flags`` argument using ``XENMEMF_node()``. Its eight-bit field stores
   the node ID plus one, with zero meaning no specified node; it is not a
   public ``nodeid_t`` argument. Claim entries instead carry a 32-bit target
   field and validate node targets against ``MAX_NUMNODES``.

 .. c:type:: nodemask_t

   A bitmap representing a set of NUMA nodes, used for status information
   like :c:var:`node_online_map` and the :c:member:`domain.node_affinity`,
   and to track which nodes are online and which nodes are in a domain's
   node affinity.

Memflags
^^^^^^^^

The internal ``memflags`` argument is an ``unsigned int`` bitmask, not a C
type. Its ``MEMF_*`` flags are distinct from the public ``XENMEMF_*`` flags
translated by the memory hypercall handler.

 .. c:macro:: MEMF_no_owner

    Flag for memory allocation requests to indicate that the allocation
    shall not be owned by a domain, and as part of that,
    :c:macro:`MEMF_no_refcount` is also set.

 .. c:macro:: MEMF_no_refcount

    Prevents credit from, and redemption of, the target domain's claims in
    the heap allocator. On the domain-heap allocation path, owned pages can
    still be assigned and tracked as extra pages excluded from
    ``domain_tot_pages()``. It does not disable page reference counting or
    imply ``MEMF_no_owner``. Assignment and transfer callers can also use the
    flag to avoid double-charging pages already accounted for elsewhere.

 .. c:macro:: MEMF_no_scrub

    Suppresses allocator-side scrubbing. The caller remains responsible for
    making the contents safe before exposing the pages to the guest; the flag
    does not authorize disclosure of data belonging to a previous owner.

 .. c:macro:: MEMF_keep_scrub

    Valid only with ``MEMF_no_scrub``. Retains per-page scrub markers and
    defers ordinary domain assignment so that the caller can scrub before
    assigning the pages. Creation-time population uses this for preemptible
    deferred scrubbing.

Locking of the claims state
^^^^^^^^^^^^^^^^^^^^^^^^^^^

When both locks are needed, take :c:member:`domain.page_alloc_lock` before
:c:var:`heap_lock`. Installation uses that order to preserve the validated
domain page count and limit until claim admission and publication complete.
Taking the page allocation lock while holding the heap lock would invert
this order.

 .. c:var:: spinlock_t heap_lock

    Protects common-heap free lists, free-page counters, and published claims
    state. Admission, redemption, and free-page counter changes use this lock
    to keep their accounting consistent. Scrubbing page contents, TLB flushing,
    and subsequent domain assignment do not all occur under this lock.

Helper functions
^^^^^^^^^^^^^^^^

 .. c:function:: unsigned int domain_tot_pages(const struct domain *d)

   :param d: The domain for which to calculate the total pages.
   :type d: const struct domain *
   :returns: ``d->tot_pages - d->extra_pages``, the pages counted towards
             the domain's normal page limit.

   This function is used for validating that an allocation and the domain's
   claims do not exceed :c:member:`domain.max_pages`.
