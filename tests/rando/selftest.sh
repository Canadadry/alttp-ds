#!/bin/sh
# Usage: selftest.sh
# Exit 0 when every check passes, 1 on the first failure.
set -eu
export LC_ALL=C

here="$(cd "$(dirname "$0")" && pwd)"
. "$here/cases.sh"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() {
	echo "selftest: $1" >&2
	exit 1
}

expect() {
	[ "$2" = "$3" ] || fail "$1: expected [$3], got [$2]"
}

nl='
'

mkdir -p "$tmp/cases/b-1" "$tmp/cases/B-2" "$tmp/cases/a-0" "$tmp/cases/no-config"
for c in b-1 B-2 a-0; do : >"$tmp/cases/$c/config.json"; done

expect "sorted listing" "$(discover_cases "$tmp/cases" prog)" "B-2${nl}a-0${nl}b-1"

expect "named cases" "$(discover_cases "$tmp/cases" prog b-1 a-0)" "b-1${nl}a-0"

status=0
out="$(discover_cases "$tmp/cases" prog a-0 no-config 2>"$tmp/err")" || status=$?
expect "unknown case status" "$status" 2
expect "unknown case stdout" "$out" ""
expect "unknown case stderr" "$(cat "$tmp/err")" "prog: no such case: no-config"

mkdir "$tmp/empty"
status=0
out="$(discover_cases "$tmp/empty" prog)" || status=$?
expect "empty status" "$status" 0
expect "empty stdout" "$out" ""

status=0
out="$(discover_cases "$tmp/missing" prog)" || status=$?
expect "missing dir status" "$status" 0
expect "missing dir stdout" "$out" ""

echo "selftest: discovery passed"
