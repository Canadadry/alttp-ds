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

awk -v out="$tmp/stages" -v checks="$tmp/checks" '
function bad(msg) { printf "%s:%d: %s\n", FILENAME, FNR, msg; err = 1; exit }
BEGIN { printf "" > out }
/^[ \t]*(#|$)/ { next }
$1 == "stage" {
	if (NF != 2) bad("malformed line")
	if ($2 in seen) bad("duplicate stage: " $2)
	seen[$2] = ++nstages
	print $2 > out
	next
}
$1 == "check" && NF >= 3 {
	if ($3 == "section" && NF != 4 || $3 == "keys" && NF < 4 || $3 == "pool" && NF != 3) bad("malformed line")
	if ($3 != "section" && $3 != "keys" && $3 != "pool") bad("unknown check kind: " $3)
	arg = $0
	sub(/^[ \t]*check[ \t]+[^ \t]+[ \t]+[^ \t]+[ \t]*/, "", arg)
	sub(/[ \t]+$/, "", arg)
	ncheck++
	check_line[ncheck] = FNR
	check_stage[ncheck] = $2
	check_text[ncheck] = $2 "\t" $3 "\t" arg
	next
}
{ bad("malformed line") }
END {
	if (err) exit 2
	printf "" > checks
	for (n = 1; n <= ncheck; n++) {
		if (!(check_stage[n] in seen)) { printf "%s:%d: check names unknown stage: %s\n", FILENAME, check_line[n], check_stage[n]; exit 2 }
		print seen[check_stage[n]] "\t" n "\t" check_text[n] > checks
	}
}' "$stages" >"$tmp/err" || fail "$(cat "$tmp/err")"
sort -n -k1,1 -k2,2 "$tmp/checks" >"$tmp/checks.sorted"

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

tab="$(printf '\t')"

# entries <spoiler>: prints "<section>\t<key>\t<value>" for every entry line of
# every top-level section but playthrough, key unquoted, trailing comma dropped.
# A missing file prints nothing.
entries() {
	[ -f "$1" ] || return 0
	awk '
	/^    "/ { sec = substr($0, 6); sub(/": .*$/, "", sec); next }
	sec != "playthrough" && /^        "/ {
		line = substr($0, 10)
		i = index(line, "\": ")
		if (!i) next
		val = substr(line, i + 3)
		sub(/,$/, "", val)
		print sec "\t" substr(line, 1, i - 1) "\t" val
	}' "$1"
}

# section_block <spoiler> <name>: the lines of top-level section <name>, the
# closing line without its trailing comma.
section_block() {
	[ -f "$1" ] || return 0
	NAME="$2" awk '
	index($0, "    \"" ENVIRON["NAME"] "\": ") == 1 { on = 1 }
	on && /^    [}\]]/ { sub(/,$/, ""); print; exit }
	on { print }' "$1"
}

GLOB2RE='
function glob2re(g,   i, c, re, inb) {
	re = "^"
	for (i = 1; i <= length(g); i++) {
		c = substr(g, i, 1)
		if (inb) {
			if (c == "]") inb = 0
			re = re (c == "\\" ? "\\\\" : c)
		} else if (c == "*") re = re ".*"
		else if (c == "?") re = re "."
		else if (c == "[") {
			inb = 1
			re = re "["
			if (substr(g, i + 1, 1) == "!") { re = re "^"; i++ }
		} else if (index(".^$+(){}|\\/", c)) re = re "\\" c
		else re = re c
	}
	return re "$"
}'

# matching <entries> <glob>: the entries whose key matches <glob>, as sorted
# "[<section>] "<key>": <value>" lines.
matching() {
	GLOB="$2" awk -F "$tab" "$GLOB2RE"'
	BEGIN { re = glob2re(ENVIRON["GLOB"]) }
	$2 ~ re { print "[" $1 "] \"" $2 "\": " $3 }' "$1" | sort
}

check_section() {
	section_block "$golden/spoiler.json" "$1" >"$tmp/g"
	section_block "$out/spoiler.json" "$1" >"$tmp/c"
	cmp -s "$tmp/g" "$tmp/c" && return 0
	if [ ! -s "$tmp/c" ]; then
		echo "    c lacks section $1"
		return 1
	fi
	first_diff "$tmp/g" "$tmp/c" >"$tmp/diff"
	echo "    golden: $(sed -n 2p "$tmp/diff")"
	echo "    c: $(sed -n 3p "$tmp/diff")"
	return 1
}

check_keys() {
	matching "$tmp/ge" "$1" >"$tmp/g"
	matching "$tmp/ce" "$1" >"$tmp/c"
	cmp -s "$tmp/g" "$tmp/c" && return 0
	missing="$(awk 'FILENAME == ARGV[1] { sub(/": .*$/, "\""); have[$0] = 1; next }
		{ sub(/": .*$/, "\"") } !($0 in have) { print; exit }' "$tmp/c" "$tmp/g")"
	if [ -n "$missing" ]; then
		echo "    c lacks $missing"
		return 1
	fi
	first_diff "$tmp/g" "$tmp/c" >"$tmp/diff"
	echo "    golden: $(sed -n 2p "$tmp/diff")"
	echo "    c: $(sed -n 3p "$tmp/diff")"
	return 1
}

# check_pool <due checks>: the golden's placed items, outside the sections that
# hold no location and the entries of due keys checks, against the C pool.txt.
check_pool() {
	if [ ! -f "$out/pool.txt" ]; then
		echo "    c wrote no pool.txt"
		return 1
	fi
	awk -F "$tab" "$GLOB2RE"'
	NR == FNR { if ($4 == "keys") re[++n] = glob2re($5); next }
	$1 ~ /^(Bosses|Equipped|Shops|Special|meta)$/ { next }
	{
		for (i = 1; i <= n; i++) if ($2 ~ re[i]) next
		v = $3
		sub(/^"/, "", v)
		sub(/"$/, "", v)
		print v
	}' "$1" "$tmp/ge" | sort >"$tmp/g"
	grep -v '^$' "$out/pool.txt" | sort >"$tmp/c"
	cmp -s "$tmp/g" "$tmp/c" && return 0
	awk 'FILENAME == ARGV[1] { g[$0]++; all[$0] = 1; next } { c[$0]++; all[$0] = 1 }
	END {
		for (x in all) if (g[x] != c[x] && (first == "" || x < first)) first = x
		printf "    golden: %s x%d\n    c: %s x%d\n", first, g[first], first, c[first]
	}' "$tmp/g" "$tmp/c"
	return 1
}

# run_checks <golden_dir> <out_dir> <stage>: runs, in table order, the checks
# owed by <stage> and the stages before it.
run_checks() {
	rank="$(awk -v s="$3" '$0 == s { print NR; exit }' "$tmp/stages")"
	awk -F "$tab" -v r="$rank" '$1 <= r' "$tmp/checks.sorted" >"$tmp/due"
	[ -s "$tmp/due" ] || return 0
	entries "$golden/spoiler.json" >"$tmp/ge"
	entries "$out/spoiler.json" >"$tmp/ce"
	while IFS="$tab" read -r _rank _n cstage kind arg; do
		case "$kind" in
		section) check_section "$arg" >"$tmp/check" ;;
		keys) check_keys "$arg" >"$tmp/check" ;;
		pool) check_pool "$tmp/due" >"$tmp/check" ;;
		esac && continue
		echo "  $cstage: $kind${arg:+ $arg}:"
		cat "$tmp/check"
		return 1
	done <"$tmp/due"
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
	check_trace "$golden/trace.txt" "$out/trace.txt" "$draws" || return 1
	run_checks "$golden" "$out" "$2"
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
