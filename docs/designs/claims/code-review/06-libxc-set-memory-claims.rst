Patch 6: Add the libxc set wrapper
==================================

Expose claim request installation and claims release to C toolstacks.

The wrapper prepares an input-only bounce buffer, initializes a set domctl,
performs the call and releases temporary storage.

.. literalinclude:: /../tools/libs/ctrl/xc_domain.c
	:language: C
	:start-at: Set a domain's host
	:end-at: return ret
        :append: }

Passing a ``NULL`` claims pointer and zero count releases claims without an
array buffer. For installation, the caller supplies the claim request.

Review questions
----------------

Should the wrapper add a bespoke multiplication-overflow check?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**The suspicion.** ``sizeof(*claims) * nr_claims`` can wrap on 32-bit
tools when given a fabricated enormous count.

**Disposition.** Like other libxc array wrappers, this function expects a
valid array containing the stated number of entries. An array large enough
to overflow the byte count cannot fit in a 32-bit address space. Xen also
rejects requests exceeding its entry limit before copying entries. The
wrapper follows the existing libxc caller-buffer convention.
