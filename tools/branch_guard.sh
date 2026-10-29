#!/bin/sh
# usage: branch_guard.sh --install
#        branch_guard.sh <pre-push arguments>   (refs on stdin)
# Git pre-push hook: refuses to push a claude/* branch. Work happens on a
# branch named after what it delivers. --install writes the hook into the
# repository's hooks directory (shared by every worktree).

set -u

if [ "${1:-}" = "--install" ]; then
    hooks=$(git rev-parse --git-common-dir)/hooks
    mkdir -p "$hooks"
    cat >"$hooks/pre-push" <<'HOOK'
#!/bin/sh
exec "$(git rev-parse --show-toplevel)/tools/branch_guard.sh" "$@"
HOOK
    chmod +x "$hooks/pre-push"
    exit 0
fi

status=0
while read -r local_ref local_sha remote_ref remote_sha; do
    case "$local_ref $remote_ref" in
        *refs/heads/claude/*)
            echo "branch_guard: refusing to push $local_ref to $remote_ref: name the branch after what it delivers" >&2
            status=1
            ;;
    esac
done
exit $status
