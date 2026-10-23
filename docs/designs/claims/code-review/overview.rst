Series overview
===============

Multi-node claims address the earlier review requests to accept or reject
all node claims in a single locked region.

- Patches 1-4 prepare protection, release, redemption and installation.
  They have no functional effect until per-node claims are installed.

  1. Protection of memory claims protects per-node claims from allocations
     without sufficient claims on the NUMA node.
  2. Release of memory claims extends the existing release path to remove
     the domain's per-node reservations as well as its host-wide claims.
  3. Redemption of memory claims
  4. Installation of memory claims

- Patches 5-8 add the set and get interfaces and their libxenctrl wrappers:

  5. ``XEN_DOMCTL_set_memory_claims`` and
  6. ``xc_domain_set_memory_claims()`` to atomically install a claim request:

     - Installation is all-or-nothing,
       not a sequence of separately accepted node requests.
  7. ``XEN_DOMCTL_get_memory_claims`` and
  8. ``xc_domain_get_memory_claims()`` report the domain's remaining
     host-wide and per-node claims for toolstacks and tests.
- The legacy host-wide ``XENMEM_claim_pages`` interface remains supported;
  consolidation and deprecation are deferred.

Constraints:

- Claims installation is a domain-build operation, permitted only before
  domain creation finishes.
- Neither the creation-state check nor atomic counter updates account for
  pages already allocated from the heap but still awaiting assignment.

  This is fine as domain builders serialize claims installation with
  completed allocation operations for the domain, including continuations.

Separate submissions in this context:

- Integration and system tests
- Reporting per-node unclaimed memory
- Reporting a domain's NUMA placement

