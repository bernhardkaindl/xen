# claims-main-v8: review summary (haiku-5.5)

Scope: patches 1 to 8. Compile tests were not rerun.
The per-patch notes describe the current state.
They are separate review metadata, not part of the eight-patch submission.
References use patch numbers rather than commit hashes.

Current state:
- Patch 1 carries Suggested-by for Roger and Signed-off-by for the author.
- Patch 3 keeps the redemption-count explanation with a capitalised opening.
- Patch 4 uses a subtraction-based availability comparison.
- Patch 5 documents the builder's serialization responsibility.
- Patch 7 documents both uses of claim_request's nr_entries, without changing
  patch 4's comment on the sums.
- NULL-buffer, offline-node and EFAULT/count questions are resolved in
  the per-patch notes; they are not open posting blockers.

The arithmetic validation and its limitations are recorded in
`../opus-5.5/00-cover-letter.md`. The accepted builder-allocation and
memory-offlining scope constraints remain unchanged.
