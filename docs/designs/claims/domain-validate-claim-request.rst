.. SPDX-License-Identifier: CC-BY-4.0

domain_validate_claim_request
#############################

.. mermaid:: domain-validate-claim-request.mmd
   :caption: domain_validate_claim_request: entry checks and claim totals

The caller, :doc:`domain-set-claims`, holds ``page_alloc_lock`` throughout
validation. This function computes ``request->total_pages`` and
``request->node_pages`` without installing claims or taking ``heap_lock``.
On success, the caller allocates any per-node array before invoking
:doc:`domain-install-claims`.