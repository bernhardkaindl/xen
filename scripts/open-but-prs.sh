#!/bin/sh
set -eu

repo=${REPO:-bernhardkaindl/xen}
remote=${REMOTE:-origin}
but_bin=${BUT_BIN:-but}
target=$($but_bin config target --json | node -e 'let s="";process.stdin.on("data",d=>s+=d).on("end",()=>process.stdout.write(JSON.parse(s).branch));')
target=${target#"$remote"/}

metadata=$(mktemp)
trap 'rm -f "$metadata"' EXIT
$but_bin status --json | node -e '
let s="";
process.stdin.on("data",d=>s+=d).on("end",()=>{
	const status=JSON.parse(s);
	for (const stack of status.stacks) {
		const branches=stack.branches.map(branch=>branch.name);
		if (branches.length) console.log(JSON.stringify({push:branches[branches.length-1],branches}));
	}
});
' >"$metadata"

while IFS= read -r stack; do
	push=$(printf '%s\n' "$stack" | node -e 'let s="";process.stdin.on("data",d=>s+=d).on("end",()=>process.stdout.write(JSON.parse(s).branches[0]));')
	$but_bin push "$push"
	branches=$(printf '%s\n' "$stack" | node -e 'let s="";process.stdin.on("data",d=>s+=d).on("end",()=>process.stdout.write(JSON.parse(s).branches.slice().reverse().join("\n")));')
	printf '%s\n' "$branches" |
	while IFS= read -r branch; do
		base=$target
		for candidate in $branches; do
			[ "$candidate" = "$branch" ] && continue
			[ "$candidate" = "$target" ] && continue
			git merge-base --is-ancestor "$candidate" "$branch" 2>/dev/null || continue
			if git merge-base --is-ancestor "$remote/$base" "$candidate" 2>/dev/null; then
				base=$candidate
			fi
		done
		if [ "$base" = "$target" ]; then
			base_ref=$remote/$base
		else
			base_ref=$base
		fi
		description=$(git log --reverse --format=%B "$base_ref..$branch")
		title=$(printf '%s\n' "$description" | sed -n '1p')
		[ -n "$title" ] || title=$branch
		pr=$(gh pr list --repo "$repo" --head "$branch" --state all --json number --jq '.[0].number')
		if [ -n "$pr" ]; then
			gh pr edit "$pr" --repo "$repo" --base "$base" --title "$title" --body "$description"
		else
			gh pr create --repo "$repo" --head "$branch" --base "$base" --title "$title" --body "$description"
		fi
	done
done <"$metadata"