# Patch 7: get-memory-claims domctl

No new finding:
- An RCU reference keeps the domain alive. `heap_lock` protects the complete snapshot, including the inclusive total, node sum and array pointer; release detaches the pointer under the same lock before freeing it. No pointer into the domain's array survives the snapshot.
- The allocated snapshot capacity is clamped to the maximum possible entry count. A short internal buffer may contain a prefix, but `ENOBUFS` prevents copying that prefix to the guest.
- The dispatch copies the domctl back on success and `ENOBUFS`, publishing the required count. Array-copy failure suppresses that copy-back; failure of the final domctl copy itself is a separate `EFAULT` path and may follow a partial copy.
- Filled entries have zero padding, and the temporary output buffer is zero-allocated. Host-wide claims precede positive node claims in ascending order.
- FLASK checks `DOMAIN__GETDOMAININFO`. Dummy XSM takes the ordinary `XSM_PRIV` domctl default, not `xsm_getdomaininfo()` and its `XSM_XS_PRIV` exception.
- Consulted the corresponding Opus, Haiku and earlier claims-v8 notes. Claims are a snapshot, not a promise that a later count/fetch pair sees the same state.

Verification:
- Source-traced allocation, snapshot, authorization and both guest-copy stages. Compile and runtime tests were not run.