---
name: but-series-review
description: "Review every commit of a GitButler (but) stacked patch series for upstream correctness, and record the results as structured per-branch markdown review files with YAML frontmatter. Use when asked to review a but stack or series, review fixup commits, or update .github/but-branch-reviews files."
---

# Review a GitButler patch series

Load the `gitbutler` skill first. It defines the `but` command rules. Never use
git write commands; read-only `git show` / `git log` are fine.

## 1. Map the series

```bash
but status -fv            # branches (bottom = oldest) with change IDs and files
```

Record, for each branch from bottom to top: full branch name, change IDs,
subjects, and the `(sha …)` of each commit. Use the SHAs only for read-only
`git show`, and use change IDs for `but` mutations.

## 2. Review each commit, oldest first

Start each commit from the viewpoint of a Xen maintainer (for example
Jan Beulich) who sees only this commit on xen-devel: before the checklist
below, list what he would plausibly ask or flag, and test each point against
the code. Typical angles:

- Why this value, limit or type? Is it a magic number, or defined in the
  public header?
- Does an error code mean the same thing for every input that can reach it?
- Is the interface extensible: undefined or reserved values, padding, and
  what an older Xen returns for a value a newer toolstack may define?
- Is a new special case needed, or does a simpler rule give the same result?
- What breaks for the toolstack, for other architectures, or for
  `CONFIG_` combinations the author did not test?

Put each such angle into the review file, even when the finding is accepted:
state the question, the answer from the code, and the verdict. An accepted
finding still needs this rationale, so the next reviewer does not raise it
again.

Add the suggestion how the finding could be fixed to the review file as well.
Show the diff context around the change for clarity, for files and commit messages.

When putting maintainer questions, split them into separate questions and
answers for each aspect of the code being reviewed.

For every commit, run `git --no-pager show --format=fuller <sha>` and check:

- **Correctness:** lock order and coverage, ownership and lifetime of
  allocated objects, accounting invariants, and error paths that leave state
  unchanged. Also check integer overflow and wrap, and edge inputs (zero,
  maximum, duplicates).
- **Series hygiene:** each commit builds on its own, later commits do not
  silently revert earlier ones, and fixups do what their message says.
  Upstream reviews each commit on its own, without the later ones, so each
  commit must be correct by itself, down to nits. Never accept a flaw
  because a later commit rewrites the code; fix it in the commit that
  introduces it.
- **Interfaces:** public header documentation, XSM/FLASK hooks for new ops,
  and toolstack wrappers.
- **Upstream etiquette:** subject prefix, message body, a single
  `Signed-off-by` before the `---` notes separator, typos, and coding style
  (`CODING_STYLE`: lines shorter than 80 columns, so at most 79). Check with
  `git diff -U0 <c>~1 <c> | awk '/^\+[^+]/ && length($0) > 80'`, which
  prints added lines of 80 or more columns (the `+` counts as one).

To confirm a suspected race, trace every reader and writer and the lock each
one holds before reporting it. Drop findings that the locking disproves.

Build the touched objects, for example
`make -C xen common/page_alloc.o common/domctl.o`. Also run
`git diff --check`. If a changed file is compiled only under a disabled
config (such as FLASK), say so in the review file.

## 3. Fix what is clearly wrong

Apply fixes in the working tree. Don't make commits, the Author will absorb fixes himself.

### Fix an earlier commit that a later commit rewrites

Use this section only when a fixup to an earlier commit is necessary because a later commit rewrites it. Wait until the Author confirms that such a fix is needed before proceeding.

An edit to lines that a later commit rewrites cannot be amended directly:
the later commit would conflict. Lift the later commit out instead:

```bash
sha=$(but show <later> | awk '/^Commit:/{print $2}')
git log -1 --format=%B $sha > /tmp/later.msg   # keep its message
git show $sha > /tmp/later.diff                  # keep its diff for reference
but uncommit <later>        # its changes become uncommitted
but discard <file-ids>      # the tree now matches the earlier commit
# edit the files: fix the earlier commit
but amend -t <earlier> <file-ids>
# edit the files again: re-create the later change on top of the fix
but commit --below <review-commit> -m "$(cat /tmp/later.msg)" <file-ids>
```

Design the later commit so that it does not revert the fix. Build and test
both commits on their own. Reword the later commit if its message
described the old structure.

## 4. Write one review file per branch

Path: `.github/but-branch-reviews/<branch-name>.md`, where the branch name
keeps its `/` separators. Commit each file into the branch it reviews.

```markdown
---
type: but-branch-review
series: <series>
branch: <full branch name>
reviews:
  - date: <ISO 8601 timestamp, `date -Iseconds`>
    model: <model>
    harness: <agent name>
    effort: <reasoning effort>
    verdict: approve | approve-after-<condition> | changes-requested
findings:
  - id: <PREFIX-N>
    severity: blocker | high | medium | low | nit | info
    status: open | fixed | accepted | accepted-risk | out-of-scope | not-applicable
    fixed_by: [<change-id or review-fixup>]   # when fixed
    title: <one line>
---

# Review: <branch>

## <ID> (<severity>, <status>): <title>
<trigger, consequence, and fix; cite files and functions>

## Verified
<suspected issues that were checked and ruled out>
```
The previous verification steps have been completed and the issues have been
checked and ruled out. They are documented in the commit messages and the review files.

When updating an existing file, keep the author's responses, append a new
`reviews` entry, and update the status of each finding instead of deleting it.

To compact review history after a series is finished, install PyYAML and run
the cleanup script on that series' review directory.
It removes keeps the latest review for each model, and moves fixed finding titles
into `## Fixes applied:`, sorted by severity with `nit` last,
while preserving the other review text.

```bash
python .github/skills/but-series-review/cleanup_reviews.py <review-directory>
python .github/skills/but-series-review/cleanup_reviews.py <review-directory> --apply
```

The default is a diff preview. `--absorb` can be used by itself to absorb
already-dirty review files, or together with `--apply` to absorb the cleanup
edits as well. It only targets dirty Markdown files in the selected directory
and previews each absorption with `but absorb --dry-run` first.

## 5. Commit

```bash
but diff
but commit -b <branch> -m "Review: <branch-leaf>" <review-file-id>
```

- Don't change the commit messages of the reviewed code commits.
- Only add new commits for code fixes.
- To submit changes to the commit messages of the reviewed code commits,
  if you have changes to them, describe the proposed changes/updates as review
  comments to the review files and add an empty fixup commit that does not
  change the reviewed code commits but provides the updated commit messages
  with a delimiter so a squash will show the delimiter between the old and new commit messages.


Commit review files after the code fixups, so the fixups sit directly above
the reviewed commits.
