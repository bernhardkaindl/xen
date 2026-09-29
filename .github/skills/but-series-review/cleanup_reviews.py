#!/usr/bin/env python3
"""Deduplicate review rounds and summarize fixed findings in review files."""

from __future__ import annotations

import argparse
import difflib
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

import yaml


FRONTMATTER_DELIMITER = "---"
FIXES_HEADING = "## Fixes applied:"
SEVERITY_ORDER = {"blocker": 0, "high": 1, "medium": 2, "low": 3, "info": 4, "nit": 5}


def latest_reviews(reviews: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Keep the latest review for each model, retaining order by last entry."""
    latest_by_model: dict[str, int] = {}
    for index, review in enumerate(reviews):
        latest_by_model[str(review.get("model", ""))] = index
    return [
        review
        for index, review in enumerate(reviews)
        if latest_by_model[str(review.get("model", ""))] == index
    ]


def fixes_sort_key(fix: str) -> tuple[int, str]:
    match = re.match(r"\(([^)]+)\):", fix)
    severity = match.group(1) if match else "unknown"
    return SEVERITY_ORDER.get(severity, 4), fix


def cleanup_text(text: str) -> str:
    lines = text.splitlines(keepends=True)
    if not lines or lines[0].strip() != FRONTMATTER_DELIMITER:
        raise ValueError("missing YAML frontmatter")

    closing = next(
        (index for index in range(1, len(lines)) if lines[index].strip() == FRONTMATTER_DELIMITER),
        None,
    )
    if closing is None:
        raise ValueError("unterminated YAML frontmatter")

    frontmatter = lines[1:closing]
    yaml.safe_load("".join(frontmatter))
    sections: dict[str, tuple[int, int]] = {}
    section_starts = [
        (index, match.group(1))
        for index, line in enumerate(frontmatter)
        if (match := re.match(r"^([A-Za-z_][\w-]*):(?:\s|$)", line))
    ]
    for position, (start, name) in enumerate(section_starts):
        end = section_starts[position + 1][0] if position + 1 < len(section_starts) else len(frontmatter)
        sections[name] = (start, end)

    changed = False
    fixed: list[dict[str, Any]] = []

    def item_blocks(section: list[str]) -> list[list[str]]:
        starts = [index for index, line in enumerate(section) if line.startswith("  - ")]
        if not starts:
            return []
        return [section[start : starts[pos + 1] if pos + 1 < len(starts) else len(section)]
                for pos, start in enumerate(starts)]

    for name in ("findings", "reviews"):
        if name not in sections:
            continue
        start, end = sections[name]
        original_section = frontmatter[start:end]
        blocks = item_blocks(original_section)
        parsed_section = yaml.safe_load("".join(original_section)) or {}
        parsed_items = parsed_section.get(name, []) if isinstance(parsed_section, dict) else []
        if not isinstance(parsed_items, list) or len(parsed_items) != len(blocks):
            continue

        if name == "reviews":
            latest_index = {
                str(review.get("model", "")): index
                for index, review in enumerate(parsed_items)
            }
            kept_blocks = []
            for index, block in enumerate(blocks):
                if latest_index[str(parsed_items[index].get("model", ""))] == index:
                    kept_blocks.append(block)
            if len(kept_blocks) != len(blocks):
                frontmatter[start:end] = original_section[:1] + [line for block in kept_blocks for line in block]
                changed = True
        else:
            kept_blocks = []
            for block, finding in zip(blocks, parsed_items):
                if isinstance(finding, dict) and finding.get("status") == "fixed":
                    fixed.append(finding)
                else:
                    kept_blocks.append(block)
            if fixed:
                if kept_blocks:
                    frontmatter[start:end] = original_section[:1] + [line for block in kept_blocks for line in block]
                else:
                    del frontmatter[start:end]
                changed = True

    current_sections = [
        (index, match.group(1))
        for index, line in enumerate(frontmatter)
        if (match := re.match(r"^([A-Za-z_][\w-]*):(?:\s|$)", line))
    ]
    for position, (start, name) in enumerate(current_sections):
        if name == "commits":
            end = current_sections[position + 1][0] if position + 1 < len(current_sections) else len(frontmatter)
            del frontmatter[start:end]
            changed = True
            break

    body = "".join(lines[closing + 1 :])
    if fixed or FIXES_HEADING in body:
        original_body = body
        existing = []
        other_lines = []
        if FIXES_HEADING in body:
            prefix, section = body.split(FIXES_HEADING, 1)
            section_lines = section.lstrip("\n").splitlines(keepends=True)
            existing = [line[2:].rstrip("\n") for line in section_lines if line.startswith("- ")]
            other_lines = [line for line in section_lines if not line.startswith("- ")]
            body = prefix.rstrip("\n")
        summaries = [f"- ({item.get('severity', 'unknown')}): {item.get('title', '')}" for item in fixed]
        merged = list(existing)
        for summary in summaries:
            if summary[2:] not in existing and summary[2:] not in merged:
                merged.append(summary[2:])
        merged.sort(key=fixes_sort_key)
        body = body.rstrip("\n") + "\n\n" + FIXES_HEADING + "\n"
        body += "".join(f"- {item}\n" for item in merged)
        body += "".join(other_lines)
        changed = changed or body != original_body

    if not changed:
        return text

    return FRONTMATTER_DELIMITER + "\n" + "".join(frontmatter) + FRONTMATTER_DELIMITER + "\n" + body


def absorb_files(paths: list[Path], root: Path) -> None:
    for path in paths:
        relative = path.relative_to(root).as_posix()
        while True:
            diff_result = subprocess.run(
                ["but", "diff", "--json"], check=True, capture_output=True, text=True
            )
            changes = json.loads(diff_result.stdout).get("changes", [])
            ids = [change["id"] for change in changes if change.get("path") == relative]
            if not ids:
                break
            source = ids[0]
            before = {change["id"] for change in changes if change.get("path") == relative}
            subprocess.run(["but", "absorb", "--dry-run", source], check=True)
            subprocess.run(["but", "absorb", source], check=True)
            after_result = subprocess.run(
                ["but", "diff", "--json"], check=True, capture_output=True, text=True
            )
            after_changes = json.loads(after_result.stdout).get("changes", [])
            after = {change["id"] for change in after_changes if change.get("path") == relative}
            if after == before:
                raise RuntimeError(f"but absorb made no progress for {relative}")


def pending_review_files(directory: Path, repo_root: Path) -> list[Path]:
    result = subprocess.run(
        ["but", "diff", "--json"], check=True, capture_output=True, text=True
    )
    changes = json.loads(result.stdout).get("changes", [])
    directory_relative = directory.relative_to(repo_root).as_posix().rstrip("/") + "/"
    paths = {
        repo_root / change["path"]
        for change in changes
        if change.get("path", "").startswith(directory_relative)
        and change.get("path", "").endswith(".md")
    }
    return sorted(paths)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="directory containing one series' review files")
    parser.add_argument("--apply", action="store_true", help="write changes (default: show a diff)")
    parser.add_argument(
        "--absorb",
        action="store_true",
        help="use but absorb on dirty review files in the directory (previews each absorption first)",
    )
    args = parser.parse_args()

    root = args.directory.resolve()
    if not root.is_dir():
        parser.error(f"not a directory: {root}")

    changed_paths: list[Path] = []
    for path in sorted(root.glob("*.md")):
        original = path.read_text(encoding="utf-8")
        try:
            updated = cleanup_text(original)
        except (ValueError, yaml.YAMLError) as error:
            print(f"{path}: {error}", file=sys.stderr)
            return 1
        if updated == original:
            continue
        changed_paths.append(path)
        if args.apply:
            path.write_text(updated, encoding="utf-8")
        else:
            sys.stdout.writelines(
                difflib.unified_diff(
                    original.splitlines(keepends=True),
                    updated.splitlines(keepends=True),
                    fromfile=str(path),
                    tofile=str(path),
                )
            )

    repo_root = Path(__file__).resolve().parents[3]
    if args.absorb:
        absorb_files(pending_review_files(root, repo_root), repo_root)
    if args.apply:
        print(f"Updated {len(changed_paths)} review file(s).")
    elif not changed_paths:
        print("No changes needed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
