#!/bin/sh
# Usage: run_e2e.sh [<case>...]
# RANDO_CASES, RANDO_CLAIMS, RANDO_STAGES and RANDO_GEN override the cases
# directory, claims.txt, stages.txt and the generator (build/rando_gen).
# The generator is run as `<generator> <case_dir> <out_dir>`.
# Exit 0 when no case is DIVERGENT, 1 when one is, 2 on a usage, claims-file or
# stage-table error.
set -eu
export LC_ALL=C

here="$(cd "$(dirname "$0")" && pwd)"
. "$here/cases.sh"
cases_dir="${RANDO_CASES:-$here/cases}"
claims="${RANDO_CLAIMS:-$here/claims.txt}"
stages="${RANDO_STAGES:-$here/stages.txt}"
gen="${RANDO_GEN:-$here/build/rando_gen}"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() {
	echo "run_e2e.sh: $1" >&2
	exit 2
}

for f in "$claims" "$stages"; do
	[ -f "$f" ] || fail "no such file: $f"
done

awk -v out="$tmp/stages" '
function bad(msg) { printf "%s:%d: %s\n", FILENAME, FNR, msg; err = 1; exit }
BEGIN { printf "" > out }
/^[ \t]*(#|$)/ { next }
$1 == "stage" {
	if (NF != 2) bad("malformed line")
	if ($2 in seen) bad("duplicate stage: " $2)
	seen[$2] = 1
	print $2 > out
	next
}
$1 == "check" && NF >= 3 {
	if ($3 == "section" && NF != 4 || $3 == "keys" && NF < 4 || $3 == "pool" && NF != 3) bad("malformed line")
	if ($3 != "section" && $3 != "keys" && $3 != "pool") bad("unknown check kind: " $3)
	check_stage[FNR] = $2
	next
}
{ bad("malformed line") }
END {
	if (err) exit 2
	for (n in check_stage)
		if (!(check_stage[n] in seen)) { printf "%s:%d: check names unknown stage: %s\n", FILENAME, n, check_stage[n]; exit 2 }
}' "$stages" >"$tmp/err" || fail "$(cat "$tmp/err")"

awk -v err="$tmp/err" '
function bad(msg) { printf "%s:%d: %s\n", FILENAME, FNR, msg > err; exit 2 }
/^[ \t]*(#|$)/ { next }
NF != 2 { bad("malformed line") }
$1 in seen { bad("duplicate case: " $1) }
{ seen[$1] = 1; print FNR, $1, $2 }' "$claims" >"$tmp/claims" || fail "$(cat "$tmp/err")"

while read -r line name stage; do
	where="$claims:$line"
	trace="$cases_dir/$name/trace.txt"
	[ -f "$cases_dir/$name/config.json" ] || fail "$where: no such case: $name"
	if [ "$stage" != all ]; then
		grep -qxF "$stage" "$tmp/stages" || fail "$where: unknown stage: $stage"
		awk -v s="$stage" '$1 == 0 && $3 == s { found = 1; exit } END { exit !found }' "$trace" ||
			fail "$where: $stage makes no draw in attempt 0 of $name"
	fi
	msg="$(awk 'NR == FNR { rank[$0] = NR; next }
		$1 != 0 { exit }
		!($3 in rank) { print "golden stage " $3 " not in table"; exit }
		rank[$3] < last { print "golden stage " $3 " out of table order"; exit }
		{ last = rank[$3] }' "$tmp/stages" "$trace")"
	[ -z "$msg" ] || fail "$name: $msg"
done <"$tmp/claims"

names="$(discover_cases "$cases_dir" run_e2e.sh "$@")"
if [ -z "$names" ]; then
	echo "e2e: no cases"
	exit 0
fi
set -f
set -- $names
set +f

# first_diff <golden> <c>: prints the first differing line number, then the
# golden and C lines ("<end>" past a file's end); prints nothing when the
# files hold the same lines.
first_diff() {
	G="$1" C="$2" awk 'BEGIN {
		for (n = 1; ; n++) {
			a = (getline g < ENVIRON["G"]) > 0
			b = (getline c < ENVIRON["C"]) > 0
			if (!a && !b) exit
			if (a != b || g != c) {
				print n; print (a ? g : "<end>"); print (b ? c : "<end>")
				exit
			}
		}
	}'
}

# check_trace <golden> <c> <draws>: compares the first <draws> lines.
check_trace() {
	head -n "$3" "$1" >"$tmp/g"
	if [ -f "$2" ]; then head -n "$3" "$2" >"$tmp/c"; else : >"$tmp/c"; fi
	cmp -s "$tmp/g" "$tmp/c" && return 0
	first_diff "$tmp/g" "$tmp/c" >"$tmp/diff"
	if [ "$(sed -n 3p "$tmp/diff")" = "<end>" ]; then
		echo "  c trace ends after $(awk 'END { print NR }' "$tmp/c") draws, claim needs $3"
	elif [ -s "$tmp/diff" ]; then
		echo "  first differing draw:"
		echo "    golden: $(sed -n 2p "$tmp/diff")"
		echo "    c: $(sed -n 3p "$tmp/diff")"
	else
		echo "  first differing draw: line endings differ"
	fi
	return 1
}

# check_file <golden_dir> <out_dir> <file>: byte-for-byte comparison.
check_file() {
	if [ ! -f "$2/$3" ]; then
		echo "  c wrote no $3"
		return 1
	fi
	cmp -s "$1/$3" "$2/$3" && return 0
	first_diff "$1/$3" "$2/$3" >"$tmp/diff"
	if [ -s "$tmp/diff" ]; then
		echo "  $3: first differing line $(sed -n 1p "$tmp/diff"):"
		echo "    golden: $(sed -n 2p "$tmp/diff")"
		echo "    c: $(sed -n 3p "$tmp/diff")"
	else
		echo "  $3: line endings differ"
	fi
	return 1
}

# run_claim <case> <stage>: prints the detail lines and returns 1 when DIVERGENT.
run_claim() {
	golden="$cases_dir/$1"
	if [ ! -f "$gen" ] || [ ! -x "$gen" ]; then
		echo "  no C generator: $gen"
		return 1
	fi
	out="$tmp/out"
	rm -rf "$out"
	mkdir "$out"
	status=0
	"$gen" "$golden" "$out" >&2 || status=$?
	if [ "$2" = all ]; then
		if [ "$status" -ne 0 ]; then
			echo "  C generator exited $status"
			return 1
		fi
		check_file "$golden" "$out" attempts || return 1
		check_trace "$golden/trace.txt" "$out/trace.txt" "$(awk 'END { print NR }' "$golden/trace.txt")" || return 1
		check_file "$golden" "$out" spoiler.json || return 1
		return 0
	fi
	draws="$(awk -v s="$2" '$1 == 0 && $3 == s { k = NR } END { print k }' "$golden/trace.txt")"
	check_trace "$golden/trace.txt" "$out/trace.txt" "$draws"
}

pass=0
divergent=0
pending=0
for name in "$@"; do
	stage=
	while read -r line claimed claimed_stage; do
		if [ "$claimed" = "$name" ]; then stage="$claimed_stage"; fi
	done <"$tmp/claims"
	if [ -z "$stage" ]; then
		echo "PENDING $name"
		pending=$((pending + 1))
	elif run_claim "$name" "$stage" >"$tmp/detail"; then
		echo "PASS $name ($stage)"
		pass=$((pass + 1))
	else
		echo "DIVERGENT $name"
		cat "$tmp/detail"
		divergent=$((divergent + 1))
	fi
done
echo "e2e: $pass pass, $divergent divergent, $pending pending"
[ "$divergent" -eq 0 ]
