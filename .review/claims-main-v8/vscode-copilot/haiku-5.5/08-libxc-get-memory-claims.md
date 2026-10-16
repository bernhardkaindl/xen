# 08 - libxc: add xc_domain_get_memory_claims()

**Result:** no open findings.
- The output bounce copies back only returned entries on success and
  nothing on failure.
- Success and ENOBUFS return the count. Errors before the copy-back,
  including EFAULT from copying the entries, preserve the input count.
  EFAULT from the final domctl copy-back promises no count.
- NULL with count zero is the documented count query. A NULL/nonzero
  buffer is caller misuse that Xen rejects when entries need copying;
  no extra client-side validation is required.
- A zero-count guest copy does not access the guest buffer.
- The copy-back comment already excludes ENOBUFS.
- The changelog's "reads them" refers to the preceding claims entry.
