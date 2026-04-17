.. SPDX-License-Identifier: CC-BY-4.0

Page Allocator Test Environment
===============================

The files in ``tools/tests/alloc`` provide a small, self-contained test
environment for Xen's page allocator in ``xen/common/page_alloc.c``.

Instead of reimplementing allocator logic in a model, the tests compile
the real allocator code into ordinary host executables and exercise it
inside a controlled environment with a synthetic Xen heap and domain
state and direct visibility into the allocator's internal state.

This approach is intended for test scenarios that are difficult to cover
through system-level testing alone, such as:

* rebuilding buddies after offlining subpages,
* checking page-list and zone accounting after allocator operations,
* verifying NUMA-aware allocator behaviour in synthetic topologies, and
* reproducing edge cases with precise control over ``struct page_info``
    state.

What The Environment Provides
-----------------------------

The test environment is assembled from a few small components:

* ``harness.h`` defines the common test harness, assertion helpers, and a
    minimal set of Xen-compatible types and macros used by the allocator.

* ``mock-page-list.h`` provides a lightweight page-list implementation
    and the ``struct page_info`` layout needed by ``page_alloc.c``.

* ``page-alloc-shim.h`` supplies the Xen definitions, stubs, and mock state
    that the allocator expects when built inside the test environment.

* ``page-alloc-wrapper.h`` includes the real ``xen/common/page_alloc.c``
    directly, wraps selected allocator entry points, and adds logging that
    makes allocator state transitions easier to follow.

* ``libtest-page-alloc.h`` ties the pieces together and adds helpers for
    common setup tasks such as resetting allocator state, preparing NUMA
    nodes, creating synthetic buddies, and checking resulting heap state.

The result is a test binary that executes real allocator code while keeping
the surrounding test environment small, explicit, and easy to inspect.

How Tests Work
--------------

Each test case starts from a clean allocator state. The library code resets
the imported allocator globals, initialises the synthetic frame table,
prepares the free lists and accounting state, and creates a minimal domain
and NUMA configuration for the scenario.

Tests then construct the required allocator state directly by manipulating
``struct page_info`` entries in the synthetic frame table and by using the
same allocator helpers that Xen uses at runtime, such as
``free_heap_pages()``. This keeps test setup aligned with allocator
behaviour instead of relying on an idealised model.

After invoking the allocator operation under test, each test scenario checks
the resulting state through assertion helpers. Typical checks include:

* the content and order of free lists,
* the contents of the offlined-page and broken-page lists,
* per-zone and per-node accounting,
* buddy order and alignment invariants, and
* page-local state such as ``count_info`` and ``first_dirty``.

Because the wrapper logs important allocator actions, test failures are
usually accompanied by enough context to show which allocator transition
broke.

Running The Tests
-----------------

The ``Makefile`` automatically discovers all ``test-*.c`` files in this
directory and builds one executable per source file.

To build and run all allocator tests:

.. code:: shell

        make -C tools/tests/alloc clean all run

To build and run a single test binary:

.. code:: shell

        make -C tools/tests/alloc clean all run TARGETS=test-reserve-offline-page

The ``run`` target executes the built tests when they are runnable on the
build host. If ``CC`` and ``HOSTCC`` differ, it will only attempt execution
when ``binfmt`` support for the target architecture is available.

For broader coverage, the ``run-archs`` target builds and runs the tests for
multiple Xen target architectures when suitable cross-compilers are installed.
