# Cover letter: initial review 2026-10-14

**Result:** no cover-letter edit required on the initial read, before reviewing the code patches.

- Scope and interface:
  - Introduces per-node claims with independent set and get domctls and libxc wrappers.
  - Explains the atomic multi-node installation requirement and retains the legacy host-wide interface.

- Reviewer context:
  - Explicitly requires builder-side serialization with allocations for the domain.
  - Separates the submitted claims feature from the forthcoming tests and per-node memory-information interface.
  - Explains that free-memory reporting still includes claimed pages and installation is the atomic availability check.
