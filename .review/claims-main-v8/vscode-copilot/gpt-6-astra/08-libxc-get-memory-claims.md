# Patch 8: libxc get-memory-claims wrapper

No new finding for valid caller-provided arrays:
- The output-only bounce is sized from the input capacity. On success, the post-copy size is reduced to the returned entry count; on failure it is zero. The caller's claims array is therefore not overwritten on `ENOBUFS` or other errors.
- A NULL array with zero capacity is the documented count query. The returned count is valid on success and `ENOBUFS`; an `EFAULT` during final domctl copy-back does not promise a valid count.
- A count/fetch sequence is not atomic across calls. The fetch still handles its own capacity correctly if claims have changed.
- The accepted 32-bit size-wrap concern requires a capacity that cannot describe a valid array in that process. Xen's capacity clamp alone would not prove an undersized bounce buffer safe for arbitrary invalid capacities. Do not use page rounding as that proof.
- The getter completes the changelog's set/get interface description. Consulted the corresponding Opus, Haiku and earlier claims-v8 notes.

Suggested review-only message addition:
- "The output bounce copies no entries on any error. Count output is meaningful on success and ENOBUFS; EFAULT during the final domctl copy can follow a partial copy and does not promise a valid count."
- This is reviewer guidance, not a proposal to duplicate the complete hypercall error contract in `xenctrl.h`.

Verification:
- Source review against the real bounce-pre/post and domctl helper behavior. No compile or runtime tests were run; shimmed test results are not treated as evidence for real bounce copying.