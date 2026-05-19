.. SPDX-License-Identifier: CC-BY-4.0

Native Xen Test Harness
=======================

Native test environment for Xen, initially focussed on the hypercall API and
exercising the page allocator APIs in native test programs. Available APIs:

- The ``DOMCTL`` APIs via ``xc_domain.c``, e.g. ``xc_domain_claim_memory()``.
- The ``XENMEM`` APIs via ``xc_domain.c``, e.g. ``xc_domain_claim_pages()``.
- The ``SYSCTL`` APIs of ``sysctl.c``, e.g. via ``do_sysctl()``.

Each test executable builds the Xen code under test into the same
compilation unit as the test case source.  This gives the test full
control over the environment and allows selected functions to be
stubbed, mocked, or wrapped for the scenario being exercised.

For the buddy allocator in ``page_alloc.c``, the environment creates
a synthetic Xen heap. For the specific scenarios, the test case
dynamically populates the free lists as needed.

As the code under test is built as a native executable, all amenities
of native execution are available: It simply runs as a native program,
meaning test cases can be compiled and run on the spot without requiring
a running Xen hypervisor.

For the native build, ``AddressSanitizer`` is enabled, test cases can
wrap selected functions, and the resulting test programs can be debugged
directly using ``printf``-style diagnostics.

Harness Shape
-------------

The harness lives under ``tools/tests/native/harness``.  Its main role
is to provide the Xen runtime environment that the code under test needs.

The harness uses real Xen headers and definitions wherever possible,
it does not attempt to replicate Xen internals or maintain local copies
if they can be avoided.  This is to ensure the tests are always using the
real definitions and to minimize the maintenance burden of keeping them
in sync.

In particular, include 16 headers from Xen, including the core headers
which provide the fundamental data structures and definitions used by
the code under test.  This includes the central Xen headers such as:

- ``public/xen.h``
- ``xen/compiler.h``
- ``xen/config.h``
- ``xen/domain.h``
- ``xen/kernel.h``
- ``xen/macros.h``
- ``xen/mm.h``
- ``xen/nospec.h``
- ``xen/numa.h``
- ``xen/page-size.h``
- ``xen/sched.h``
- ``xen/typesafe.h``
- ``xenctrl.h``

The remaining shims are intentionally limited to runtime state for the
test context and the minimal set of shims and stubs needed to support
the code under test, such as:

- Synthetic heap setup and management
- Guest-copy helpers backed by host memory copies
- PDX/frame-table translations matching the synthetic heap
- Shims for code not used by the tests yet.

This keeps definitions by Xen in Xen headers as far as possible and
leaves the harness to model mostly only the execution environment.

How Tests Work
--------------

Each test case starts from a clean allocator state and initializes a
synthetic frame table with pages on the free lists. After the allocator
operations under test, each test scenario checks the resulting state
through assertion helpers.

As running the tests after a change literally takes just seconds, it
can be used as a quick check for regressions during development. The
test cases can be easily extended to cover new scenarios as needed,
and integrating code coverage tools is straightforward.

Running The Tests
-----------------

The ``Makefile`` automatically discovers the C test programs in this
directory and builds one executable per source file.

The ``test`` target executes the built tests when they are runnable on the
build host. If ``CC`` and ``HOSTCC`` differ, it will only attempt execution
when ``binfmt`` support for the target architecture is available.

Example to clean, build, and run the tests for the default target
architecture:

.. code:: shell

    make -C tools/tests/native clean test

To build and run a single test program:

.. code:: shell

    make -C tools/tests/native test TARGETS=host-claims

To build and run a given test program with specific arguments:

.. code:: shell

    make -C tools/tests/native test TARGETS=host-claims RUN_ARGS=CNGS

In this example, ``CNGS`` is a test case defined in the ``host-claims.c``
source file.  The test binary is built and executed with ``CNGS`` as an
argument, causing it to run only that scenario instead of all scenarios in
the source file.  This is useful when working on a specific case.

Cross-tests
^^^^^^^^^^^

For broad coverage, the ``test`` target builds and runs the tests for all
active Xen target architectures when suitable cross-compilers are installed.
On a Debian-based system, the needed cross-compilers can be installed with:

.. code:: shell

    sudo apt install -y gcc-{{aarch,riscv}64-linux-gnu,arm-linux-gnueabihf}

The ``test`` target is a convenient way to build and run all tests for
multiple Xen target architectures, which is useful for broader coverage
and catching regressions that may only manifest on specific architectures.

For target architectures not directly executable on the host, Linux ``binfmt``
and ``qemu-user-static`` support are needed so the built test executables can
run. On a Debian-based system, these can be installed with:

.. code:: shell

    sudo apt install -y binfmt-support qemu-user-static

To get broad coverage across multiple Xen target architectures, pass
``cross=y`` with the ``test`` target to build and run the tests for
all supported Xen target architectures with detected cross-compilers:

.. code:: shell

    make -C tools/tests/native test cross=y

To build and run tests for a specific Xen target architecture:

.. code:: shell

    make -C tools/tests/native test TEST_ARCHS=x86_64
    make -C tools/tests/native test TEST_ARCHS=arm64
    make -C tools/tests/native test TEST_ARCHS=arm32
    make -C tools/tests/native test TEST_ARCHS=riscv64
