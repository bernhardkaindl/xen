Patch 5: Expose claim installation
==================================

Add the ``XEN_DOMCTL_set_memory_claims`` operation.

The handler authorizes the operation and copies input into private storage,
so validation and installation use the same entries, then invokes the set
or release path. Under FLASK, it reuses the existing ``setclaim`` permission.

.. literalinclude:: /../xen/common/domctl.c
	:language: C
	:start-at: set_memory_claims
	:end-at: return ret

The domctl lock serializes installation with domain destruction and
finishing domain creation.

