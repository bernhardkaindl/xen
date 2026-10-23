Patch 7: Expose a claims snapshot
=================================

Report a domain's nonzero host-wide and per-node claims.

The handler takes a private snapshot under :c:var:`heap_lock` and copies it
to the guest after unlocking. Bypass the domctl lock while retaining
domain lifetime protection. Insufficient capacity returns ``ENOBUFS`` and
the required count without copying entries.

.. literalinclude:: /../xen/common/domctl.c
	:language: C
	:start-at: get_memory_claims
	:end-at: return ret
        :append: }
        :emphasize-lines: 14-15,19-22

The caller can query the required count with zero capacity and then
allocate an array. Claims may change between calls, so the next call can
still return ``ENOBUFS`` with a different required count.

Under FLASK, reuse the existing ``getdomaininfo`` permission:
``XEN_DOMCTL_getdomaininfo`` already exposes the aggregate claim count in
``outstanding_pages``; this operation reports the claims by target.

Review questions
----------------

Can lockless get race with release and read freed storage?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** Release or destruction can free ``d->claims`` while
get copies results to a guest without holding the domctl lock.

**Disposition.** The reader copies values into separately owned storage
under ``heap_lock``. Writers detach under that same lock and free only
afterwards; no claims-array pointer escapes into guest copying. RCU keeps
the domain alive. The result may become stale, but this path does not use
a dangling array pointer.

.. literalinclude:: /../xen/common/page_alloc.c
	:language: C
	:start-at: int domain_get_claims(
	:end-at: return count <= capacity
        :append: }
        :emphasize-lines: 7-8,36

Does FLASK permission reuse import xenstore's dummy-XSM exception?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** Reusing ``getdomaininfo`` could appear to give the new
domctl the same xenstore exception as the existing domain-info operation.

**Disposition.** Only FLASK reuses its existing ``getdomaininfo``
permission. Dummy XSM follows ``xsm_domctl(XSM_PRIV, ...)``; the separate
domain-info ``XSM_XS_PRIV`` dispatch is not reused.

The get operation reaches the same privileged authorization check as set,
then returns without acquiring the domctl lock:

.. literalinclude:: /../xen/common/domctl.c
	:language: C
	:start-at: XEN_DOMCTL_get_memory_claims
	:end-at: goto domctl_out_unlock_rcuonly;
	:dedent: 4

Does a guest-copy failure guarantee unchanged outputs?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** Copying entries only after a successful snapshot could
be read as preserving all guest buffers and the original count on error.

**Disposition.** Raw guest copies can fault after partial output. The
final domctl copy itself can replace success or ``ENOBUFS`` with ``EFAULT``
after a partial count copy. The later libxc wrapper's suppression of
claims-array copy-back is a separate guarantee.

.. literalinclude:: /../xen/common/domctl.c
	:language: C
	:start-at: domctl_out_unlock_rcuonly:
	:end-at: ret = -EFAULT;