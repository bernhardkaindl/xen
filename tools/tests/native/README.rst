.. SPDX-License-Identifier: CC-BY-4.0

Native Xen Test Harness
=======================

Native test environment for Xen, initially focused on the hypercall
API and exercising the page allocator APIs in native test programs.
Available APIs include:

- ``DOMCTL`` APIs via ``xc_domain.c``, e.g. ``xc_domain_claim_memory()``.
- ``XENMEM`` APIs via ``xc_domain.c``, e.g. ``xc_domain_claim_pages()``.
- ``SYSCTL`` APIs via ``xc_domain.c``, e.g. ``xc_availheap()``.

Each test executable builds the Xen code under test into the same
compilation unit as the test case source. This gives the tests full
control over the environment and allows selected functions to be
stubbed, mocked, or wrapped for the scenario under test.

For the buddy allocator in ``page_alloc.c``, the environment creates
a synthetic Xen heap. Each test case populates the free lists as needed
for the scenario under test, and the test cases can check the resulting
state of the heap after exercising the code under test.

As the code under test is built as a native executable, all advantages
of native execution are available. The test cases can wrap selected
functions, and the resulting test programs can be debugged directly
using ``printf``-style diagnostics. For not cross-compiled builds,
``AddressSanitizer`` is enabled as well. Cross-compiled builds use
static binaries that can be run on the host using Linux ``binfmt-misc``
and ``qemu-user-static`` support.

Harness Shape
-------------

The harness lives under ``tools/tests/native/harness``. Its main role
is to provide the Xen runtime environment required by the code under
test.

The harness uses real Xen headers and definitions wherever possible.
It does not attempt to replicate Xen internals or maintain local
copies when they can be avoided. This ensures that the tests always
use the real definitions and minimises the maintenance burden of
keeping them in sync.

In particular, it includes 16 Xen headers, including the core headers
that provide the fundamental data structures and definitions used by
the code under test. These include central Xen headers such as:

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

The remaining shims are intentionally limited to runtime state for
the test context and the minimal set of shims and stubs needed to
support the code under test, such as:

- Synthetic heap setup and management
- Guest-copy helpers backed by host memory copies
- PDX/frame-table translations matching the synthetic heap
- Shims for code not yet used by the tests

This keeps Xen definitions in Xen headers wherever possible and
limits the harness primarily to modelling the execution environment.

How Tests Work
--------------

Each test case starts from a clean allocator state and initialises a
synthetic frame table with pages on the free lists. After the
allocator operations under test, each scenario checks the resulting
state using assertion helpers.

As running the tests after a change takes only a few seconds, they
can be used as a quick regression check during development. The test
cases can also be extended easily to cover new scenarios, and code
coverage tools can be integrated straightforwardly.

Running The Tests
-----------------

The ``Makefile`` automatically discovers the C test programs in this
directory and builds one executable per source file.

The ``test`` target executes the built tests when they are runnable
on the build host. If ``CC`` and ``HOSTCC`` differ, execution is only
attempted when ``binfmt`` support for the target architecture is
available.

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
source file. The test binary is built and executed with ``CNGS`` as an
argument, causing it to run only that scenario instead of all scenarios
in the source file. This is useful when working on a specific test case.

Cross-tests
^^^^^^^^^^^

For broad coverage, the ``test`` target builds and runs the tests for
all active Xen target architectures when suitable cross-compilers are
installed. On Debian-based systems, the required cross-compilers can
be installed with:

.. code:: shell

    sudo apt install -y \
        gcc-{aarch64,riscv64}-linux-gnu \
        gcc-arm-linux-gnueabihf

The ``test`` target provides a convenient way to build and run all tests
for multiple Xen target architectures. This is useful for broader coverage
and for catching regressions that may only appear on specific architectures.

For target architectures that are not directly executable on the
host, Linux ``binfmt`` and ``qemu-user-static`` support are required
so the built test executables can run. On Debian-based systems, these
can be installed with:

.. code:: shell

    sudo apt install -y binfmt-support qemu-user-static

To obtain broad coverage across multiple Xen target architectures,
pass ``cross=y`` with the ``test`` target to build and run the tests
for all supported Xen target architectures with detected
cross-compilers:

.. code:: shell

    make -C tools/tests/native test cross=y

To build and run tests for a specific Xen target architecture:

.. code:: shell

    make -C tools/tests/native test TEST_ARCHS=x86_64
    make -C tools/tests/native test TEST_ARCHS=arm64
    make -C tools/tests/native test TEST_ARCHS=arm32
    make -C tools/tests/native test TEST_ARCHS=riscv64
