.. SPDX-License-Identifier: CC-BY-4.0

Unit tests for the page allocator
=================================

The tests in ``tools/tests/alloc`` contain unit tests for the Xen page
allocator in ``xen/common/page_alloc.c``` and built as standalone
executables.

They are not intended to be run in a Xen environment, but rather to test
the allocator logic in isolation and can be run on any compatible host
system at build time and do not use any installed libraries or require
any special setup or dependencies beyond the standard C library.

The tests use a shim as a substitute for Xen hypervisor code that would
conflict with running the page allocator as a host executable, and they
use helper functions to initialize and assert the status of the data
structures of the allocator such as the page lists and zones.

The tests can be run with the "make run" target, which will execute all
the test executables and report their results unless you override the
TARGETS variable to run a specific test:

.. code:: shell

    make -C tools/tests/alloc clean all run \
            TARGETS=test-reserve_offline_page-uma

To add a new test, simply create a new C file with a name starting with
``test-``, implement the test logic, and it will be automatically included
in the build and run targets by default.
