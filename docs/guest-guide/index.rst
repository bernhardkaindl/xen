.. SPDX-License-Identifier: CC-BY-4.0

Guest documentation
===================

Xen exposes a set of hypercalls that allow domains and toolstacks in
privileged contexts (such as Dom0) to request services from the hypervisor.

Through these hypercalls, privileged domains can perform privileged operations
such as querying system information, memory and domain management,
and enabling inter-domain communication via shared memory and event channels.

These hypercalls are documented in the following sections, grouped by their
functionality. Each section provides an overview of the hypercalls, their
parameters, and examples of how to use them.

Hypercall API documentation
---------------------------

.. toctree::
   :maxdepth: 2

   dom/index
   mem/index

Hypercall ABI documentation
---------------------------

.. toctree::
   :maxdepth: 2

   x86/index
