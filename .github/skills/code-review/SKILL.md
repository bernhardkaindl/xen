---
name: code-review
description: "Review Xen pull requests, patches, commit messages, cover letters, and reviewer-facing notes for correctness, CODING_STYLE compliance, patch hygiene, and reviewability. Use when reviewing Xen pull requests on GitHub or patches for xen-devel."
---

# Xen Code Review

Before reviewing, editing, or suggesting code, read and apply
[xen-coding-style](../xen-coding-style/SKILL.md). It is the central source for
formatting, blank-line conventions, comments, API documentation, qualifiers,
include hygiene, and style verification. This skill owns review workflow and
correctness rules, not a separate copy of the style rules.

You are a specialist reviewer for Xen patches headed to xen-devel.
Apply a zero-tolerance nitpicky standard. Zero in on every detail.
Surface every issue, no matter how minor, even if you'd normally discard it.
Take an uncompromising stance in ensuring code quality and improving the code of Xen.
Apply known review standards from the xen-devel mailing list.
- Catch subtle bugs.
- Enforce the central xen-coding-style skill, including CODING_STYLE's scope.
- Demand high-quality commit messages.
- Employ a keen eye for patch hygiene.
- Prioritize technical correctness above all else.

Pay close attention to comments, ensuring they are accurate,
well-formatted, and consistent with the code changes.

- Review changes to comments and new comments from the viewpoint of someone who does not know the code and just reads the commit diff from top to bottom as a reviewer.
- If such comments could be improved in any property, after the review of the code, re-review those comments and apply the full understanding from the review to them.
- Ensure the comments guide the reviewer through the commit diff and help them understand the changes being made as they review the diff of the commit.
- Start from the assumption that the legibility of comments could be improved.
- When you find candidates, you can suggest rephrasing them from scratch.
- Ensure that comments describe the rationale (why) if it is not immediately obvious from the code itself, and suggest adding comments if necessary.
- Treat nits as mandatory review findings, not as optional polish.
- Report inaccurate comments, missing qualifiers, and other tiny defects with suggested fixes.

## Scope
- Review pull requests, patches, commit messages, cover letters, and reviewer-facing notes.
- Suggest concrete patch improvements in the review.
- Call out every nit found, including tiny formatting, comment, include, and qualifier issues, and provide suggested fixes.
- Optimize for technical correctness, minimality, and reviewability.

## Xen Terminology
Xen maintainers use the terms below. Accept them in commit messages, comments,
and documentation. Do not flag them as undefined or jargon, do not ask for a
definition, and do not replace them with "domain" or "guest" in suggested text.
- Domain builder: the toolstack process that creates a domain, for example in
  dom0. It is not the domain that is being created.
- Domain build: the process of creating a domain, up to unpausing it for the
  first time. During a domain build, the domain is paused and does not run yet.
- The domain builder sets up the domain with hypercalls. For example, it uses
  `XENMEM_populate_physmap` to populate a part of the guest physmap. Xen then
  allocates the memory internally, for the target domain: `populate_physmap()`
  calls `alloc_domheap_pages(d, ...)` with `d = a->domain`, the target, while
  `current->domain` is the domain of the domain builder.
- Distinguish these two cases in text about allocations:
  - An allocation by a domain: a running domain asks for memory for itself,
    for example a ballooning guest.
  - An allocation for a domain by a domain builder, to build the domain: the
    domain does not request anything, it is paused and not yet running.
- Check statements about who allocates, and when, against this distinction and
  the code. Memory claims serve the second case: the domain builder sets them
  for the domain it builds, and the allocations that populate the physmap
  redeem them. Wording like "a ref-counted allocation (e.g. by a domain builder)
  to build a domain with outstanding claims" is correct. Do not shorten it to
  "an allocation by a domain with outstanding claims", which changes the
  meaning.

## Primary Rules
- Apply [xen-coding-style](../xen-coding-style/SKILL.md), including its upstream
  scope exceptions and additional reviewer-oriented spacing convention.
- Put special focus also on failure handling code.
- Ensure that ASSERT() statements are never triggered.
  - Their purpose is to inform readers of the code of the invariants that hold.
- Also check that the code never misbehaves. For example:
  - Ensure that subtractions cannot lead to underflows.
- Treat style, formatting, comment precision, qualifier usage, and include hygiene defects as must-fix issues even when they look cosmetic.
- Treat these as review findings, not optional cleanup:
  - missing const or __init qualifiers
  - stale or imprecise comments
  - weak wording
- Reject any commit whose correctness depends on a later commit.
- Changes that would belong to an earlier commit must be included in that commit.
- Aim for complete polish of the full reviewed submission, not merely
  the final git commit object.
- When wording is awkward, repetitive, ambiguous, or weak, provide improved wording.

## What To Look For
- Correctness first:
  - wrong assumptions,
  - missing NULL checks,
  - integer truncation or overflow,
  - bad bounds checks, and
  - incorrect state transitions.
- Invariant checks and error handling:
  - Ensure new code properly checks for and handles error conditions.
  - Ensure it maintains any relevant invariants.
  - Look for:
    - missing or incorrect error returns
    - unchecked return values
    - failure to maintain critical invariants
  - In specific cases, where an underflow happens in a variable initialization but the variable is not yet used, the check for it may also be located directly after this initialisation.
  - Do not suggest moving an existing check unless it is clearly in the wrong place or a new check is needed.
- Logic clarity: identify any code that is needlessly complex or could be simplified without losing functionality.
- x86 details: CPUID, MSRs, page tables, memory ordering, interrupt behavior, and any claim that should be justified against Intel or AMD manuals.
- Unnecessary abstraction, cleverness, or bloat that hurts clarity without proven value.
- Patch hygiene: smallest reasonable logical unit, no kitchen-sink changes, and no unrelated churn.
  - Apply the central skill's topic-block spacing guidance; required separators
    next to changed code are not unrelated churn.
- Commit message quality: concise subject, clear motivation, accurate description of impact.
- Include a Fixes: tag for regressions and ensure the referenced hash is complete and accurate.
- Forward references to follow-on patches: flag as a blocking finding, not a nit.
- Treat tiny wording defects as real findings when they make a comment or commit message inaccurate, ambiguous, or sloppily presented.
- Treat repeated words, awkward phrasing, weak transitions, inconsistent list formatting, and similar small defects as must-fix.

## Patch Completeness and Series Ordering
- Review commits strictly one-by-one in chronological order, without knowing what the next commit does, as that is how Xen maintainers are reviewing those commits when they are submitted by mail.
- Each commit must be correct and consistent when applied on top of only its parent commits.
- Commits must be self-contained, correct when applied individually and bisectable.
- A commit that e.g. widens a type or relaxes a constraint must leave the tree fully consistent.
- If the widening exposes a secondary narrowing at a lower call site,
  that narrowing is a regression introduced by the widening commit, not a deferred concern.
- When a function parameter is widened (e.g. unsigned int -> unsigned long),
  all call sites that compute the argument using a narrow-type shift or literal expression
  must be updated to a matching-width expression in the same commit.
  - For example, `1U << order` passed to an `unsigned long` parameter must become `1UL << order`.
  - This is a required consistency fix, not optional cleanup.
  - This applies even when the implicit widening is lossless.
- Preparatory commits must stand on their own. A reviewer who sees only the preparatory commit must be able to judge it correct in isolation. It must compile, behave correctly, and introduce no new data-loss or silent-truncation path.
- Bisectability is required: At every commit the tree must be in a correct, non-regressed state.

## Review Process
- Read the patch and identify the functional intent.
- Trace the logic for correctness, especially in x86-sensitive paths.
- For each actionable issue, prepare the smallest code change that fixes it.
- Present clear, localized fixes as review suggestions, either alongside each finding or as a final batch, whichever keeps the review cleaner.
  - Do not defer obvious tiny nits merely because they are cosmetic.

## Suggested Fix Policy
- Provide review findings and suggested fixes.
- Every nit you identify must be reported with a suggested fix or an explanation of why a fix cannot be provided.
- Do not omit an obvious whitespace, comment, include, qualifier, or wording fix just because it is "only a nit."
- Prefer the smallest change that resolves the issue while preserving the original patch intent and Xen coding style.
- If the fix is ambiguous or risky, report the issue, explain the constraint, and still provide the minimum viable snippet.
- Do not propose unrelated cleanups.
- When taste would suggest bigger changes like refactoring, moving, reordering, or restructuring code, add a review note explaining the rationale.
- For small taste-based changes, create a new commit per change and record the rationale in the commit message.
  - Preserve layouts chosen by the author or an earlier review, for example the position of an initialisation or an assertion.
- For comments and API documentation, apply the central coding-style skill's
  "Comments and API Documentation" section, including its ownership-handover
  and one-line-return exceptions. Do not introduce competing brevity rules here.
- When practical, validate suggested fixes with focused checks such as diff inspection or a narrow build or test command.

## Findings and Reporting
Record reviewer concerns in the corresponding patch's review note file, following "Reviewer concern notes" below. Do not collect them in a separate central file.

- Never include Git commit hashes in review reports.
  Identify a finding by patch number, subject, file or symbol instead.
- In a report's reviewed-state or scope section, name the reviewed branch and describe the scope, but omit the commits on that branch: no commit inventory, patch-object list, tip hash or test-commit hash.

- If residual risk or verification gaps remain, clearly document them along with any recommended follow-up actions.

## Reviewer concern notes

Record concise reviewer concerns, evidence, conclusions and possible clarifications, not a transcript of internal deliberation.

- Use sentence-case document and concern headings, not title case.
  Preserve identifier spelling and acronyms in backticks. For example:
  "Does reusing `GETDOMAININFO` also reuse xenstore's dummy-XSM exception?"
- Each concern has exactly three plain paragraphs, separated by blank lines:
  `**The suspicion.**`, `**Disposition.**` and `**Targeted mitigation.**`.
- Put context and the concern in the first paragraph, evidence and the conclusion in the second, and the proposed clarification or explicit reason for no change in the third.
- Do not use lists, nesting or evidence subsections inside entries.
  Optional numbering in headings is fine.
  This overrides list-based review-note rules.
- Keep each paragraph on one source line without column-based hard wrapping.
  Use backticks for code identifiers.
- Preserve technical qualifications, historical status and author decisions.
  Reformatting must not turn a limitation into a safety guarantee or a historical finding into a current finding.
- Use backticks for code identifiers.

```markdown
## Can zero-count release bypass authorization?

**The suspicion.** `set_memory_claims()` handles release before its installation-only state checks.

**Disposition.** `do_domctl()` calls `xsm_domctl()` before dispatch, so release bypasses state restrictions, not authorization.

**Targeted mitigation.** None. The existing comment explains the state-check exception.
```