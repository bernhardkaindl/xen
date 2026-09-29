---
name: xen-commit-messages
description: "Write or fix commit messages for Xen hypervisor and toolstack patches in upstream style: subsystem-prefixed subjects, why-first bodies, trailer order, and the --- notes separator. Use when writing, rewording, or reviewing Xen commit messages, adding missing bodies or Signed-off-by lines, or preparing a series for xen-devel."
---

# Xen commit messages

Derived from the last 100 non-merge commits on `upstream/master`. Refresh
with:

```bash
git log -100 --no-merges --format=%s upstream/master        # subjects
git log -100 --no-merges --format='%(trailers:only)' upstream/master
```

## Subject

`<prefix>: <imperative summary>`

- **Prefix:** the subsystem or path, not the file name. Find what earlier
  patches used with
  `git log --format=%s upstream/master -- <path> | cut -d: -f1 | sort | uniq -c | sort -rn`.

  | Area | Common prefixes |
  |---|---|
  | `xen/common/page_alloc.c` | `xen/mm`, `xen/page_alloc`, `mm` |
  | `xen/common/domctl.c` | `domctl`, `xen/domctl` |
  | `xen/xsm` | `xsm`, `flask`, `xen/xsm` |
  | `tools/libs/ctrl` | `libxc`, `tools/libs/ctrl`, `tools` |
  | `xen/arch/x86/...` | `x86`, `x86/mm`, `x86/HVM`, ... |
  | RISC-V / Arm | `xen/riscv`, `xen/arm`, `Arm` |

  Combine prefixes for cross-cutting patches (`domctl/XSM:`).
- Use the imperative mood ("add", "fix", "drop"), usually lowercase after the
  prefix. Capitalise identifiers, acronyms and proper nouns as spelled.
- No trailing period (0 of 100). Aim for about 50 characters and stay
  within 72.

## Body

- Explain **why** first: the problem, its trigger and its consequence. Then
  explain **what** the patch does, and how only where the diff alone does
  not show it.
- Wrap at 72 columns. Put code, commands and log output on indented lines.
- Name functions with `()`; name hypercalls and constants as spelled
  (`XEN_DOMCTL_set_memory_claims`).
- For refactoring, end the body with `No functional change.` or
  `No functional change intended.`. For preparatory patches, say what
  depends on them ("Not used yet; the next patch adds the caller.").
- State design assumptions that reviewers would otherwise question, for
  example concurrency or who may call the operation.
- Every commit needs a body, except trivial one-liners where the subject
  says everything.

## Trailers

Use this order, with no blank lines between trailers:

```
Fixes: <12-hex sha> ("<subject of the fixed commit>")
Reported-by: / Suggested-by: Name <email>
Co-developed-by: Name <email>     # immediately before that person's SoB
Signed-off-by: Author <email>     # required (DCO), exactly once per person
Assisted-by: <Tool>:<model>       # when AI assisted, e.g. Claude:claude-opus-5-5
Reviewed-by: / Acked-by:          # added by maintainers or on re-post only
```

- Do not invent `Reviewed-by` or `Acked-by`. Add only tags given on
  xen-devel for this exact version.
- Only the author may add their own `Signed-off-by`. When rewording on the
  author's behalf, use the commit's author identity (`git log -1 --format='%an <%ae>'`).

## Notes after `---`

Version history and reviewer notes go below a line with exactly three
dashes. `git am` drops everything after it:

```
Signed-off-by: Author <email>
---
Changes in v8:
- Allocate the claims array instead of embedding it in struct domain.
```

- Use exactly `---`; `--` is kept in the commit.
- Put only one `Signed-off-by` above the separator; do not repeat it below.
- Write notes such as "not used yet, for a focused review" there, or in
  the body if they explain the design.

## Checklist

1. The prefix matches earlier history for the touched paths.
2. The subject is imperative, has no period, and is at most 72 characters.
3. The body explains why, is wrapped at 72, and has no typos (run a spell
   check).
4. `No functional change.` is present where it applies.
5. Trailers are in order, with a single `Signed-off-by` from the author.
6. Notes, if any, are below `---`.

## Rewording with GitButler

```bash
but reword <change-id> -m "$(cat /tmp/msg.txt)"
```

Write the message to a file first to keep the formatting exact. Fixup
commits (`fixup: ...`) keep short messages; they are squashed before
posting.
