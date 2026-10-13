---
name: xen-coding-style
description: "Apply Xen coding style and reviewer-oriented spacing when writing, editing, reviewing, or suggesting code, including small fixes, tests, comments, and API documentation. Use before any Xen code change or proposed code snippet, not only during review."
---

# Xen Coding Style

This is the central agent reference for Xen code style, whether implementing
new code, applying a small fix, or suggesting a patch. Read it before the first
code edit or suggestion. Review workflow, correctness, terminology, and commit
policy remain in [code-review](../code-review/SKILL.md).

## Sources and Scope

- Read [CODING_STYLE](../../../CODING_STYLE) and follow its applicable rules.
  It remains the authoritative upstream document; this skill does not replace
  it. Respect its exceptions for imported code and other subsystem styles.
- In Xen-style code, an explicit CODING_STYLE rule takes precedence over an
  inconsistent nearby example. Where it leaves a choice open, use local
  conventions and preserve deliberate author or earlier-review layouts.
- The blank-line guidance below is an additional workspace convention,
  requested by the author for Xen maintainer-oriented review preparation.
- Apply these rules to code being written or changed, including suggested
  snippets. Do not reformat unrelated code or override imported-file style.

## Formatting and Structure

- Use four spaces per block indentation level, not tabs. Distinguish block
  indentation from continuation alignment; in Xen-style declarations and
  calls, continued parameters and arguments commonly start one column beyond
  the opening parenthesis. Count columns instead of estimating visually.
- Keep lines below 80 columns, with the documented exception for searchable
  user-visible strings. Split long expressions at sensible boundaries.
- Use spaces around operators and inside control-flow parentheses, e.g.
  `if ( condition )`. Follow CODING_STYLE for braces, including its do/while
  and initializer exceptions. Omit braces for a single-statement body.
- Prefer flat control flow with early return, continue, or break when this
  simplifies code. Do not introduce a refactor solely to enforce this taste.
- Follow CODING_STYLE for identifiers, filenames, types, macros, include
  guards, and optional editor modelines. Prefer basic C types; use fixed-width
  types when the quantity itself has a fixed width.
- Use const for objects accessed without modification where appropriate, and
  apply lifetime qualifiers such as __init when justified by all call sites.
  Check qualifiers and include hygiene even in a tiny change; do not guess.
- Keep expressions consistent with their intended width, e.g. `1UL << order`
  for an unsigned-long quantity. Handle any affected call sites in the same
  logical change, not in a later patch.

## Blank Lines and Topic Blocks

- Keep connected syntax together: no empty line between a condition and its
  body, between a loop header and its body, within a tight nested loop chain,
  or between the closing brace and its else/else-if or do/while continuation.
- Do not insert empty lines immediately after an opening brace or before a
  closing brace merely to surround a block. Keep a comment with the code it
  introduces; place the topic separator before that comment.
- Treat a long multi-line statement as a topic block. In particular, separate
  large ENSURE, ASSERT, or call statements from adjacent independent setup,
  mutation, control flow, and other long checks with one empty line.
- Keep a short operation and its directly attached check together when they
  form one small logical unit, e.g. an allocation followed by its success
  check. Do not insert a blank line between every simple assignment or EQ.
- Keep a contiguous group of declarations, related assignments, or short
  checks together. Use one separator when the purpose changes, not multiple
  empty lines and not automatic blank lines based solely on line count.
- Inspect both missing and excessive spacing after larger if statements.
  When adding code to an existing topic block, include necessary separators;
  they are part of the change, not unrelated whitespace churn.

```c
    prepare_heap();

    if ( claimed )
        install_claim();

    page = allocate_page();
    ENSURE(page, "Allocation failed");

    ENSURE(total == expected_total &&
           node_total == expected_node_total,
           "Accounting differs");

    release_claims();
```

## Comments and API Documentation

- Use C comments only. Start multi-word comments with a capital letter.
  A single sentence may end with a full stop; multiple sentences each need
  one. Use separate opening/closing markers and leading stars in multi-line
  comments, as shown in CODING_STYLE.
- Read comments from the perspective of a reviewer reading the diff from
  top to bottom. Verify accuracy, qualifiers, and rationale against the code;
  update comments in the same patch when behavior changes. Improve awkward,
  repeated, ambiguous, or misleading wording, not merely its formatting.
- Explain why when not apparent; do not narrate operations the code shows.
  Before proposing a new comment, ask whether it supplies needed information.
  Do not add boilerplate or churn comments between successive review passes.
- Keep comments above implementation functions short: normally one sentence
  or two to three lines. Name side effects not apparent from the function name,
  e.g. "Validate a claim request and compute request->total and ->node_pages."
  Longer rationale is justified for a non-obvious invariant or test model;
  preserve author-requested test scenario descriptions rather than treating
  this brevity guidance as an absolute length limit.
- Do not list parameters, return values, allocation/free paths, or ownership
  already apparent from the implementation. Preserve these exceptions:
  a pointer handed back through an argument for the caller to free merits a
  short ownership explanation; a return value described within a useful
  one-line summary costs no extra space and should be kept.
- Do not restate which locks an implementation function takes or requires;
  code and lock assertions express that. Preserve data-protection rules on
  fields or variables, such as "protected by heap_lock".
- Header prototype comments are API documentation, not implementation
  narration. Preserve concise purpose and usage descriptions, including
  non-obvious parameter meanings, input/output conventions, special values,
  ownership, and important return behavior. Do not remove useful documentation
  merely because the function name or underlying hypercall overlaps with it.
- Keep the detailed hypercall contract in the canonical public header; avoid
  copying full contracts or error lists into wrapper headers. A short wrapper
  usage summary in xenctrl.h is useful documentation, not forbidden duplication.
- Do not document routine contracts readers can infer from analogous APIs,
  such as output-buffer contents after failure, without a concrete need.
- If a vague function name hides its purpose, suggest a precise name rather
  than compensating with a long comment. Preserve established public APIs
  unless a rename is explicitly within scope.

## Small Changes and Verification

- Re-read current file context before editing, especially after user changes.
  Preserve unrelated edits and deliberate layouts; do not reapply a reverted
  suggestion without addressing the author's objection.
- Change indentation only for a demonstrated violation. Inspect actual source
  with whitespace visible and count columns; rendered diffs can be misleading.
  If evidence is ambiguous, leave it unchanged and explain the uncertainty.
- Use the smallest change resolving the issue. For larger taste-based
  restructuring, explain the rationale rather than silently broadening scope.
  Review nits need a concrete fix or explanation; no automatic commits or
  history edits are authorized by this skill.
- After a code change, run the narrowest relevant behavioral check, test, or
  build allowed by the task, then inspect affected whitespace and the diff.
  Compilation alone does not validate layout. For comment-only changes, check
  wording against the implementation and validate links and formatting.
- Check `git diff --check`, added-line lengths, continuation columns, braces,
  topic separators, comment accuracy, includes, and qualifiers. Preserve the
  intended scope and repair introduced defects before finishing.