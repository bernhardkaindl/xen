.. SPDX-License-Identifier: CC-BY-4.0

domain_install_claims
#####################

.. mermaid:: domain-install-claims.mmd
   :caption: domain_install_claims: availability checks and installation

The caller retains ``page_alloc_lock`` throughout this function and releases
it after the call returns, as shown in :doc:`domain-set-claims`.