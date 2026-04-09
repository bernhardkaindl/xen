.. SPDX-License-Identifier: CC-BY-4.0

#########
Use Cases
#########

.. glossary::

 Parallel :term:`domain builds`

  When many domains need to be created and built, many :term:`domain builders`
  compete for the same pools of memory, which can lead to inefficient NUMA
  placement of :term:`guest physical memory` and thus suboptimal performance
  for the domains.

  NUMA-aware claims can help solve this problem and ensure that memory
  is available on the appropriate NUMA nodes.

 Domain builds

  The process of constructing and configuring :term:`domains` by
  :term:`domain builders`, which includes installing :term:`claims`,
  :term:`populating` memory, and setting up other resources before the
  :term:`domains` are started. When multiple :term:`domain builders` can
  run in parallel, this is referred to as parallel domain builds, which can
  benefit from NUMA-aware claims because the domain builders are competing for
  the same pools of memory on the NUMA nodes.

 Boot storms

  It is common for many domains to be booted at the same time, such as during
  system startup or when large numbers of domains need to be started.

 Parallel migrations

  Similar to :term:`boot storms`, except that the domains are being migrated
  instead of booted, which can happen when other hosts are being drained
  for maintenance (host evacuation) or when workloads are being rebalanced
  across hosts.
