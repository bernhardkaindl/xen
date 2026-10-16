# 08 - add `xc_domain_get_memory_claims()`

**Result:** No actionable defect found. No source or commit-message change suggested.

## Code review

- Reviewed the wrapper against the real bounce implementation, domctl helper, and xencall allocation/free code at this patch's revision.
- The initial bounce capacity is the caller's input count. Output-only direction avoids reading an uninitialized caller array.
- `claims == NULL` with a zero capacity skips the claims bounce and queries the count. The result is success for zero outstanding entries or `ENOBUFS` with the required count otherwise.
- The domctl is bounced in both directions even on failure. Xen copies its updated count back on success or `ENOBUFS`; earlier errors normally leave the input capacity unchanged. Final-copy `EFAULT` is deliberately not treated as a valid-count result.
- Failure sets the claims copy-back length to zero, independently of any partially copied or invalid returned count. Success copies only the returned number of entries and preserves unused caller capacity.
- `do_domctl()` requests zero EFAULT retries. The wrapper therefore does not silently repeat a potentially completed operation or copy.
- The public documentation describes capacity, count queries, and the unchanged array on `ENOBUFS`; it does not promise valid output on every error.
- Existing reviews' later native-bounce coverage supersedes historical alias-buffer descriptions. No native or live-Xen result from another cycle is claimed as an execution by this review.

## Does shrinking the bounce size lose the information needed to free it?

**The suspicion.** `HYPERCALL_BOUNCE_SET_SIZE()` changes the size before bounce-post frees the buffer, including setting it to zero on failure.

**Disposition.** The size controls only the final `memcpy()`. `xencall_free_buffer()` recovers the original page count from its allocation header, independently of the current copy length. Without the size adjustment, a successful short result would overwrite unused caller capacity with the zero-filled bounce tail, and a failed call could overwrite the caller's array with zero or partial bounce contents.

**Targeted mitigation.** No edit. Keep the existing success-only copy-back comment and the current size adjustment. Do not replace the metadata argument with a blanket claim that page rounding makes arbitrary oversized capacities safe.

## Does an EFAULT guarantee that the output count stayed unchanged?

**The suspicion.** A failure result suppresses entries copy-back, so it could seem safe to promise that all outputs retain their input values.

**Disposition.** The entries array is preserved by this wrapper on failure, but the domctl bounce is separate. A fault during the final domctl copy can leave a partial count or other partial output before libxc receives the error. The wrapper still suppresses the claims-array copy, because its error branch selects zero without using that count as a length.

**Targeted mitigation.** Keep patch 7's proposed review-only distinction between raw guest-copy behavior and libxc suppression. No broader output-count guarantee or duplicate error paragraph is needed here.

## Suggested commit-message diff

None.
