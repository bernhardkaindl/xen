Patch 4: Install a claim set
============================

Introduce functions validating and installing new claim requests.

1. Validation: validate the request's entries and the domain's limit.
2. Admission: check sufficient host and node capacity for the request.
3. Installation: install all claim counters atomically.

Validation rejects requests exceeding :c:member:`domain.max_pages` and
other invalid requests while holding :c:member:`domain.page_alloc_lock`.

.. literalinclude:: /../xen/common/page_alloc.c
	:language: C
	:start-at: node_pages = 0
	:end-at: EINVAL
	:dedent: 4
        :emphasize-lines: 2-3,10,12

Admission checks capacity under the global :c:var:`heap_lock`:

.. literalinclude:: /../xen/common/page_alloc.c
	:language: C
	:start-at: claims can be satisfied
	:end-at: }
        :emphasize-lines: 4-8,10-13,23-26

Installation applies the changes to the domain and the host, before both
locks are released, like the existing locking does.

.. literalinclude:: /../xen/common/page_alloc.c
	:language: C
	:start-at: static int domain_install_claims
	:end-at: claims = new_claims;
        :emphasize-lines: 9,16,20-26

The added functions are not yet exposed; no other changes.

Review questions
----------------

Does the page-allocation lock cover every allocation in flight?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** Is holding :c:member:`domain.page_alloc_lock` through
validation and installation alone sufficient to include every allocated
page in :c:func:`domain_tot_pages()`?

**Disposition.** No, by the design of the allocator, allocation starts
without holding :c:member:`domain.page_alloc_lock` and only acquires it
when reaching assignment. It is the responsibility of the domain builder
to account for this window by claiming pages appropriately before allocation.

See :ref:`claiming-and-page-assignment` for details on the interaction
between claiming and page assignment.
