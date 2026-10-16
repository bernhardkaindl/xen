# Cover letter: initial review

Scope:
- Initial review of the `claims-main-v8` cover letter before reading the source patches; implementation claims remain subject to the per-patch review.

No initial wording change required:
- The letter explains atomic multi-node installation, separates the new interfaces from the retained host-wide interface, and states builder-side serialization explicitly.
- The production-use statement applies to the protection design, not necessarily this revision of all eight patches.
- Tests and per-node unclaimed-memory reporting are explicitly deferred, without presenting free-memory reporting as unclaimed-memory reporting.

Verification boundary:
- Compile tests will not be run in this cycle, as requested. Prior review results are supporting context, not new test results.