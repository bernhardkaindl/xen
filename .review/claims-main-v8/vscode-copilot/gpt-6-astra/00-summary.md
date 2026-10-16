# claims-main-v8 review summary

Result:
- No new actionable source defect established in the eight-patch series for the documented initially paused, serialized domain-build workflow.
- No source, cover-letter, commit-message or existing review files were changed. Only this review directory was added, unstaged. No commits, history changes, staging or branch switches were made.

Review scope:
- Reviewed the cover letter first and wrote its initial assessment before opening the source patches. Reviewed the eight patches individually in chronological order against their respective parents.
- Consulted the corresponding Opus 5.5, Haiku 5.5 and older claims-v8 reports during each patch review. Reconciled the completed review with the Opus extra-high and GPT 6.1 Sol medium/high/extra-high summaries and deeper concern documents afterwards.

Reviewer-facing recommendations:
- [Patch 4](04-set-claim-entries.md): clarify below the message separator that builder serialization covers the complete allocation, including pending scrub continuations.
- [Patch 5](05-domctl-set-memory-claims.md): distinguish the initially paused build from an asynchronous debugger stop; a positive controller pause count is not a general quiescence proof.
- [Patch 8](08-libxc-get-memory-claims.md): distinguish array-copy failure from failure during final domctl copy-back, which does not guarantee a valid returned count.
- [Reviewer concerns and targeted mitigations](09-reviewer-trains-of-thought.md): concise evidence and conclusions for arithmetic, redemption, node lifetime, locking, authorization, snapshot ownership, copy faults, capacity wrapping and scrub continuations. These are not an internal deliberation transcript.

Verification performed:
- Each of the eight parent-to-patch diffs passed `git diff-tree --check`.
- New review notes passed whitespace checks and the editor reported no diagnostics before the final documentation update; final checks are recorded by the review session.
- Source-traced the real allocator, assignment, hot-add rollback, domctl authorization, GDBSX pause, libxc bounce and deferred-scrub paths. Verified the generated libxc version-map rule exports the new wrappers without an explicit symbol list.
- No compile, native or live-Xen tests were run in this cycle, as requested for compile tests. Previously reported test results are not counted as executions by this review.

Evidence reconciliation:
- The latest existing extra-high review reports an improved native harness that executes real libxc bounce pre/post code with host-backed allocation, injected copy faults and an authorization-decision callback. Older reports describing only alias buffers and no-op bounce-post are historical, not a description of that updated harness.
- The latest report also says the reserved-special-target test now uses `HOST | 1U`. The older duplicate-HOST test-coverage finding is therefore not carried forward as an open issue here.
- Those native reports do not establish production xencall allocation, real guest-address translation, concurrent interleavings or FLASK policy enforcement. This cycle did not independently inspect or rerun the separate test submission.

Remaining scope limits:
- Memory offlining can reduce capacity below claims; the clamp prevents arithmetic underflow, not loss of the reservation guarantee.
- The builder must serialize claim installation with allocations for the target domain, including pages awaiting assignment and scrub continuations.
- Claims reserve page counts, not particular contiguous extents, every address zone or a placement policy. Arbitrary ownership-transfer and sharing workflows are not certified by this ordinary domain-build review.
- Live-Xen concurrency, architecture/configuration runtime behavior, guest-copy faults and actual XSM policy enforcement remain outside this source-only cycle. This review does not replace the previously performed compilation checks.