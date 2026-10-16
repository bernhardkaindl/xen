Code review
===========

The claims v8 series introduces per-node NUMA memory claims.
Xen's existing claims reserve memory host-wide; toolstacks
building many domains in parallel also need reservations
on the NUMA nodes chosen for each domain to avoid overcommitting any
single node while building and destroying many domains in parallel.
Claims reserve capacity, not placement:
the builder must still steer allocations to the intended nodes.

The following pages explain each patch's purpose, the reasons for its
implementation choices, and questions that may arise during review.

.. toctree::
   :maxdepth: 1

   01-protect-node-claims
   02-release-node-claims
   03-redeem-numa-claims
   04-set-claim-entries
   05-domctl-set-memory-claims
   06-libxc-set-memory-claims
   07-domctl-get-memory-claims
   08-libxc-get-memory-claims
   overview