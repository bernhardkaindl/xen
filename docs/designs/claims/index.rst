.. SPDX-License-Identifier: CC-BY-4.0

NUMA-aware Claim Sets
=====================

Design and implementation of NUMA-aware claim sets.

Status: Draft for review

This design first introduces the external behaviour of claim sets: how claims
are installed, how they protect allocations, and how they are retired.
It then covers the underlying accounting model and implementation details.

For readers following the design in order, the next sections cover the
following topics:

1. :doc:`/designs/claims/usecases` describes the use cases for claim sets.
2. :doc:`/designs/claims/history` provides the development's historical context
3. :doc:`/designs/claims/design` introduces the overall model and goals.
4. :doc:`/designs/claims/installation` explains how claim sets are installed.
5. :doc:`/designs/claims/protection` describes how claimed memory is
   protected during allocation.
6. :doc:`/designs/claims/retirement` explains how claims are retired as
   allocations succeed.
7. :doc:`/designs/claims/accounting` describes the accounting model that
   underpins those steps.

.. toctree:: :caption: Contents
   :maxdepth: 2

   usecases
   history
   design
   installation
   protection
   retirement
   accounting
   implementation
   edge-cases

.. contents::
    :backlinks: entry
    :local:
