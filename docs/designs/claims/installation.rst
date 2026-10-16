.. SPDX-License-Identifier: CC-BY-4.0

Installation
############

.. toctree::
   :maxdepth: 1

   domain-set-claims
   domain-validate-claim-request
   domain-install-claims

*************
Claim request
*************

A claim request is an array of :c:type:`xen_domctl_memory_claim_t` entries
passed to :c:macro:`XEN_DOMCTL_set_memory_claims`.

.. c:type:: xen_domctl_memory_claim_t

   The ``typedef`` for :c:struct:`xen_domctl_memory_claim`, used to pass
   a claim set entry to the hypervisor.

.. c:struct:: xen_domctl_memory_claim

   Underlying structure for passing claim sets to the hypervisor.

   This structure represents an individual claim entry in a claim set.
   It specifies the number of pages claimed and the target of the claim,
   which can be a specific NUMA node or
   :c:macro:`XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST` for a host-wide claim.

   The structure includes a padding field which must be zero. Members are:

   .. c:member:: uint64_aligned_t pages

      Number of pages for this claim entry.

   .. c:member:: uint32_t target

      The target of the claim entry: an online Xen host NUMA node ID or the
      host-wide target constant. Guest vNUMA node IDs are not translated here.

      :c:macro:`XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST` selects a host-wide
      claim; all other values with bit 31 set are reserved.

   .. c:member:: uint32_t pad

      Reserved padding. It must be zero.

.. c:type:: uint64_aligned_t

   64-bit unsigned integer type with alignment requirements suitable for
   representing page counts in the claim structure.

*******************
Claims installation
*******************

Claim installation is invoked via
:c:macro:`XEN_DOMCTL_set_memory_claims`, and
:c:func:`domain_set_claims()` implements the claim set installation logic.

A non-empty request can only be installed when the domain has no outstanding
claims and only before its creation phase has finished. The request is
validated and installed atomically; it does not replace existing claims.
The domain builder must serialize claim installation with allocations
for the domain, including completion of any allocation continuation.
Passing an empty request releases outstanding claims.

The companion :c:macro:`XEN_DOMCTL_get_memory_claims` domctl reads the current
claims. Its input capacity is the maximum number of entries the caller can
receive; on ``-ENOBUFS``, the returned count gives the required capacity.

Entries request outstanding pages, not a desired final allocation size.
Pages already allocated to the domain are not subtracted from the request;
instead, ``domain_tot_pages(d)`` plus requested claims must fit within
``d->max_pages``. This count excludes ``d->extra_pages``, such as auxiliary
pages owned by the domain but not charged to its normal memory limit.
Partially populated domains can therefore install claims
before creation finishes, provided the builder completes and serializes its
allocation operations.

Each entry must have a nonzero page count, zero padding, and a unique target.
Targets are online node IDs or
:c:macro:`XEN_DOMCTL_MEMORY_CLAIM_TARGET_HOST`; other values with bit 31 set
are reserved. Non-empty requests are rejected while LLC coloring is enabled,
for dying domains, or after creation finishes. Zero-count release bypasses
these installation-only restrictions, but still requires authorization and
zero request padding.

The outer ``xen_domctl_memory_claims`` structure carries the array handle,
``nr_claims``, and its own zero-required ``pad`` field. Set accepts at most
``MAX_NUMNODES + 1`` entries, including the optional host-wide entry; larger
requests return ``-E2BIG``. This is a build-time node limit, not the current
number of online nodes. Get clamps larger capacities to the same bound.

Get returns the nonzero host-wide claim first, followed by nonzero node claims
in ascending node order. On ``-ENOBUFS`` it writes no entries. The returned
count and entries form a snapshot, not a reservation against subsequent
changes; callers must handle another ``-ENOBUFS`` when retrying a count query.
An ``-EFAULT`` during guest copying can leave partial output, so it does not
promise a valid returned count. The libxc get wrapper suppresses claims-array
copy-back on failure and copies only returned entries on success.

A zero-capacity get needs no claims-array buffer. With outstanding claims it
returns ``-ENOBUFS`` and the required count; without claims it succeeds with
count zero. Neither result requires writing an array entry.

See :doc:`accounting` for details on the claims accounting state.

The series adds the libxenctrl C wrappers
``xc_domain_set_memory_claims()`` and ``xc_domain_get_memory_claims()``.
It does not automatically install per-node claims in existing builders:
libxenguest's x86 memory initialization still uses the legacy host-wide call.
A toolstack must choose its node reservations, call the new interface, and
steer subsequent allocations separately.

*************************
Legacy claim installation
*************************

Legacy claims are set via the :c:macro:`XENMEM_claim_pages` hypercall command.
They retain their existing interface and semantics for compatibility.

.. note:: The legacy path remains available for compatibility.
   Use :c:macro:`XEN_DOMCTL_set_memory_claims` and
   :c:macro:`XEN_DOMCTL_get_memory_claims` for new code.
