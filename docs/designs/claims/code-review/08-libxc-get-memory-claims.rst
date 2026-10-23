Patch 8: Add the libxc get wrapper
==================================

Add a libxenctrl wrapper for ``XEN_DOMCTL_get_memory_claims`` so toolstack
code and tests can read a domain's host-wide claim and per-node claims.

If the capacity in ``*nr_claims`` is too small, fail with ``ENOBUFS`` without
writing any claims, and set ``*nr_claims`` to the number of claims needed.

.. literalinclude:: /../tools/libs/ctrl/xc_domain.c
	:language: C
	:start-at: Read a domain's host
	:end-at: return ret
        :append: }

A NULL array with zero capacity queries the count without a claims bounce
buffer. It succeeds with count zero when no claims remain, or returns
``ENOBUFS`` with the required count otherwise.

Review questions
----------------

Does shrinking the bounce size lose the original allocation size?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** Setting ``HYPERCALL_BOUNCE_SET_SIZE()`` to the returned
length, or zero on failure, could seem to prevent correct buffer freeing.

**Disposition.** This size controls the final ``memcpy()`` only.
``xencall_free_buffer()`` recovers the original page count from allocation
metadata. Without shrinking, a short successful result could overwrite
unused caller capacity with the zero-filled tail; a failed call could
copy zero or partial bounce contents over the array.

The bounce post-processing uses the current size for copying, then frees
the buffer separately:

.. literalinclude:: /../tools/libs/ctrl/xc_hcall_buf.c
	:language: C
	:start-at: if ( b->dir == XC_HYPERCALL_BUFFER_BOUNCE_OUT
	:end-at: xc__hypercall_buffer_free(xch, b);
	:dedent: 4

The underlying free operation recovers the allocation size from its header:

.. literalinclude:: /../tools/libs/call/buffer.c
	:language: C
	:start-at: void xencall_free_buffer(
	:end-at: xencall_free_buffer_pages(xcall, hdr, hdr->nr_pages);

Does every failure preserve the original output count?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** Preserving the claims array on failure could imply that
all outputs, including capacity, retain their input values.

**Disposition.** The array and domctl use separate bounce paths. The final
domctl copy can fault after partially updating the count. The wrapper
still suppresses entries on failure because its error branch selects zero
without using that count as a copy length.

.. literalinclude:: /../tools/libs/ctrl/xc_domain.c
	:language: C
	:start-at: *nr_claims = domctl.u.memory_claims.nr_claims;
	:end-at: HYPERCALL_BOUNCE_SET_SIZE(claims,
	:dedent: 4