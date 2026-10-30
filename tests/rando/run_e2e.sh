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

# Writes $tmp/claims, one "<case>\t<stage>\t<draws>\t<due file or ->" line per
# claim, and $tmp/due.<rank>, the "<stage>\t<kind>\t<arg>" checks a claim at the
# stage of that rank owes, in table order.
CASES="$cases_dir" TMP="$tmp" awk '
function bad(msg) { printf "%s:%d: %s\n", FILENAME, FNR, msg; err = 1; exit }
function die(msg) { print msg; exit 2 }
function end_stages(   n) {
	stages_done = 1
	for (n = 1; n <= ncheck; n++)
		if (!(check_stage[n] in seen)) {
			printf "%s:%d: check names unknown stage: %s\n", ARGV[1], check_line[n], check_stage[n]
			return 1
		}
	return 0
}
FILENAME == ARGV[1] {
	if (/^[ \t]*(#|$)/) next
	if ($1 == "stage") {
		if (NF != 2) bad("malformed line")
		if ($2 in seen) bad("duplicate stage: " $2)
		seen[$2] = ++nstages
		next
	}
	if ($1 == "check" && NF >= 3) {
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
	bad("malformed line")
}
!stages_done && end_stages() { err = 1; exit }
/^[ \t]*(#|$)/ { next }
NF != 2 { bad("malformed line") }
$1 in claimed { bad("duplicate case: " $1) }
{ claimed[$1] = 1; nclaims++; cl_line[nclaims] = FNR; cl_name[nclaims] = $1; cl_stage[nclaims] = $2 }
END {
	if (err || !stages_done && end_stages()) exit 2
	out = ENVIRON["TMP"] "/claims"
	printf "" > out
	for (i = 1; i <= nclaims; i++) {
		name = cl_name[i]; s = cl_stage[i]; where = ARGV[2] ":" cl_line[i]
		f = ENVIRON["CASES"] "/" name "/config.json"
		if ((getline line < f) < 0) die(where ": no such case: " name)
		close(f)
		t = ENVIRON["CASES"] "/" name "/trace.txt"
		n = 0; k = 0; last = 0; msg = ""; stop = 0
		while ((r = (getline < t)) > 0) {
			n++
			if ($1 == 0 && $3 == s) k = n
			if (stop) continue
			if ($1 != 0) stop = 1
			else if (!($3 in seen)) { msg = "golden stage " $3 " not in table"; stop = 1 }
			else if (seen[$3] < last) { msg = "golden stage " $3 " out of table order"; stop = 1 }
			else last = seen[$3]
		}
		close(t)
		if (s != "all") {
			if (!(s in seen)) die(where ": unknown stage: " s)
			if (!k) die(where ": " s " makes no draw in attempt 0 of " name)
		} else if (r < 0) die(name ": cannot read " t)
		if (msg != "") die(name ": " msg)
		due = "-"
		if (s != "all") {
			rank = seen[s]
			if (!(rank in written)) {
				written[rank] = ""
				for (rr = 1; rr <= rank; rr++)
					for (c = 1; c <= ncheck; c++)
						if (seen[check_stage[c]] == rr) written[rank] = written[rank] check_text[c] "\n"
			}
			if (written[rank] != "") {
				due = ENVIRON["TMP"] "/due." rank
				printf "%s", written[rank] > due
				close(due)
			}
		}
		print name "\t" s "\t" (s == "all" ? n : k) "\t" due > out
	}
}' "$stages" "$claims" >"$tmp/err" || fail "$(cat "$tmp/err")"

names="$(discover_cases "$cases_dir" run_e2e.sh "$@")"
if [ -z "$names" ]; then
	echo "e2e: no cases"
	exit 0
fi
set -f
set -- $names
set +f

# Compares one claim's C output (O) with its golden (G), prints the detail lines
# of the first failure and exits 1, or exits 0. MODE is "all" or "stage"; a
# stage claim compares the first DRAWS trace lines, then runs the checks of DUE.
CLAIM_AWK='
function slurp(path, L,   s, r, n) {
	split("", L)
	r = (getline s < path)
	if (r < 0) return -1
	close(path)
	nl_end = 1
	if (r == 0) return 0
	n = split(s, L, "\n")
	if (substr(s, length(s)) == "\n") { delete L[n]; n-- }
	else nl_end = 0
	return n
}
function same(A, na, B, nb,   k) {
	if (na != nb) return 0
	for (k = 1; k <= na; k++) if (A[k] "" != B[k] "") return 0
	return 1
}
function first_diff(A, na, B, nb,   k) {
	fd_n = 0
	fd_g = fd_c = ""
	for (k = 1; ; k++) {
		if (k > na && k > nb) return 0
		if (k > na || k > nb || A[k] != B[k]) {
			fd_n = k
			fd_g = k > na ? "<end>" : A[k]
			fd_c = k > nb ? "<end>" : B[k]
			return 1
		}
	}
}
function fail(msg) { printf "%s", msg; exit 1 }
function check_trace(draws,   ng, ge, nc, ce, pg, pc, eq) {
	draws += 0
	ng = slurp(ENVIRON["G"] "/trace.txt", GT); ge = nl_end
	if (ng < 0) ng = 0
	if (mode == "all") draws = ng
	nc = slurp(ENVIRON["O"] "/trace.txt", CT); ce = nl_end
	if (nc < 0) { nc = 0; ce = 1 }
	pg = ng < draws ? ng : draws + 0
	pc = nc < draws ? nc : draws + 0
	eq = same(GT, pg, CT, pc) && (pg == 0 || (pg < ng || ge) == (pc < nc || ce))
	if (eq) return
	if (first_diff(GT, pg, CT, pc) && fd_c == "<end>")
		fail("  c trace ends after " pc " draws, claim needs " draws "\n")
	if (fd_n)
		fail("  first differing draw:\n    golden: " fd_g "\n    c: " fd_c "\n")
	fail("  first differing draw: line endings differ\n")
}
function check_file(file,   ng, ge, nc, ce) {
	nc = slurp(ENVIRON["O"] "/" file, CF); ce = nl_end
	if (nc < 0) fail("  c wrote no " file "\n")
	ng = slurp(ENVIRON["G"] "/" file, GF); ge = nl_end
	if (ng >= 0 && same(GF, ng, CF, nc) && (ng == 0 || ge == ce)) return
	if (ng < 0) ng = 0
	if (first_diff(GF, ng, CF, nc))
		fail("  " file ": first differing line " fd_n ":\n    golden: " fd_g "\n    c: " fd_c "\n")
	fail("  " file ": line endings differ\n")
}
function block(L, n, name, B,   k, on, m) {
	m = 0
	for (k = 1; k <= n; k++) {
		if (index(L[k], "    \"" name "\": ") == 1) on = 1
		if (!on) continue
		if (L[k] ~ /^    [}\]]/) { B[++m] = L[k]; sub(/,$/, "", B[m]); return m }
		B[++m] = L[k]
	}
	return m
}
function entries(L, n, S, K, V,   k, sec, line, i, m) {
	m = 0
	sec = ""
	for (k = 1; k <= n; k++) {
		if (L[k] ~ /^    "/) { sec = substr(L[k], 6); sub(/": .*$/, "", sec); continue }
		if (sec == "playthrough" || L[k] !~ /^        "/) continue
		line = substr(L[k], 10)
		i = index(line, "\": ")
		if (!i) continue
		m++
		S[m] = sec
		K[m] = substr(line, 1, i - 1)
		V[m] = substr(line, i + 3)
		sub(/,$/, "", V[m])
	}
	return m
}
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
}
function matching(n, S, K, V, re, M,   k, m, j, x) {
	m = 0
	for (k = 1; k <= n; k++) {
		if (K[k] !~ re) continue
		x = "[" S[k] "] \"" K[k] "\": " V[k]
		for (j = m++; j > 0 && M[j] > x; j--) M[j + 1] = M[j]
		M[j + 1] = x
	}
	return m
}
function load_entries() {
	if (entries_done++) return
	gn = entries(GS, ngs, GES, GEK, GEV)
	cn = entries(CS, ncs, CES, CEK, CEV)
}
function check_section(name,   nb, nc) {
	nb = block(GS, ngs, name, GB)
	nc = block(CS, ncs, name, CB)
	if (same(GB, nb, CB, nc)) return 1
	if (!nc) { out = "    c lacks section " name "\n"; return 0 }
	first_diff(GB, nb, CB, nc)
	out = "    golden: " fd_g "\n    c: " fd_c "\n"
	return 0
}
function check_keys(glob,   re, mg, mc, k, p, have) {
	load_entries()
	re = glob2re(glob)
	split("", MG); split("", MC)
	mg = matching(gn, GES, GEK, GEV, re, MG)
	mc = matching(cn, CES, CEK, CEV, re, MC)
	if (same(MG, mg, MC, mc)) return 1
	for (k = 1; k <= mc; k++) { p = MC[k]; sub(/": .*$/, "\"", p); have[p] = 1 }
	for (k = 1; k <= mg; k++) {
		p = MG[k]; sub(/": .*$/, "\"", p)
		if (!(p in have)) { out = "    c lacks " p "\n"; return 0 }
	}
	first_diff(MG, mg, MC, mc)
	out = "    golden: " fd_g "\n    c: " fd_c "\n"
	return 0
}
function in_pool(sec, key,   i) {
	if (sec ~ /^(Bosses|Equipped|Shops|Special|meta)$/) return 0
	for (i = 1; i <= npre; i++) if (key ~ PRE[i]) return 0
	return 1
}
function check_pool(   np, k, v, g, c, all, x, first, at, pre, bad, y) {
	np = slurp(ENVIRON["O"] "/pool.txt", PL)
	if (np < 0) { out = "    c wrote no pool.txt\n"; return 0 }
	load_entries()
	npre = 0
	for (k = 1; k <= ndue; k++) if (DK[k] == "keys") { v = DA[k]; sub(/\t.*/, "", v); PRE[++npre] = glob2re(v) }
	for (k = 1; k <= gn; k++) at[GES[k] SUBSEP GEK[k]] = k
	for (k = 1; k <= cn; k++) {
		if (CEV[k] == "\"Nothing\"" || !in_pool(CES[k], CEK[k])) continue
		x = CES[k] SUBSEP CEK[k]
		if (x in at && GEV[at[x]] == CEV[k]) { pre[at[x]] = 1; continue }
		y = "[" CES[k] "] \"" CEK[k] "\""
		if (bad == "" || y < bad) {
			bad = y
			out = "    golden: " (x in at ? y ": " GEV[at[x]] : "<none>") "\n    c: " y ": " CEV[k] "\n"
		}
	}
	if (bad != "") return 0
	for (k = 1; k <= gn; k++) {
		if (k in pre || !in_pool(GES[k], GEK[k])) continue
		v = GEV[k]
		sub(/^"/, "", v)
		sub(/"$/, "", v)
		g[v]++; all[v] = 1
	}
	for (k = 1; k <= np; k++) if (PL[k] != "") { c[PL[k]]++; all[PL[k]] = 1 }
	first = ""
	for (x in all) if (g[x] != c[x] && (first == "" || x < first)) first = x
	if (first == "") return 1
	out = sprintf("    golden: %s x%d\n    c: %s x%d\n", first, g[first], first, c[first])
	return 0
}
BEGIN {
	RS = "\001"
	mode = ENVIRON["MODE"]
	if (mode == "all") {
		check_file("attempts")
		check_trace()
		check_file("spoiler.json")
		exit 0
	}
	check_trace(ENVIRON["DRAWS"])
	if (ENVIRON["DUE"] == "-") exit 0
	ndue = slurp(ENVIRON["DUE"], DL)
	ngs = slurp(ENVIRON["G"] "/spoiler.json", GS)
	ncs = slurp(ENVIRON["O"] "/spoiler.json", CS)
	for (d = 1; d <= ndue; d++) {
		i = index(DL[d], "\t"); DS[d] = substr(DL[d], 1, i - 1); rest = substr(DL[d], i + 1)
		i = index(rest, "\t"); DK[d] = substr(rest, 1, i - 1); DA[d] = substr(rest, i + 1)
	}
	for (d = 1; d <= ndue; d++) {
		if (DK[d] == "section") ok = check_section(DA[d])
		else if (DK[d] == "keys") ok = check_keys(DA[d])
		else ok = check_pool()
		if (!ok) fail("  " DS[d] ": " DK[d] (DA[d] != "" ? " " DA[d] : "") ":\n" out)
	}
}'

# run_claim <case> <stage> <draws> <due>: prints the detail lines and returns 1
# when DIVERGENT.
nout=0
run_claim() {
	golden="$cases_dir/$1"
	if [ ! -f "$gen" ] || [ ! -x "$gen" ]; then
		echo "  no C generator: $gen"
		return 1
	fi
	nout=$((nout + 1))
	out="$tmp/out$nout"
	mkdir "$out"
	status=0
	"$gen" "$golden" "$out" >&2 || status=$?
	mode=stage
	if [ "$2" = all ]; then
		if [ "$status" -ne 0 ]; then
			echo "  C generator exited $status"
			return 1
		fi
		mode=all
	fi
	G="$golden" O="$out" MODE="$mode" DRAWS="$3" DUE="$4" awk "$CLAIM_AWK"
}

tab="$(printf '\t')"
pass=0
divergent=0
pending=0
for name in "$@"; do
	stage=
	while IFS="$tab" read -r claimed claimed_stage claimed_draws claimed_due; do
		if [ "$claimed" = "$name" ]; then
			stage="$claimed_stage"
			draws="$claimed_draws"
			due="$claimed_due"
		fi
	done <"$tmp/claims"
	if [ -z "$stage" ]; then
		echo "PENDING $name"
		pending=$((pending + 1))
	elif run_claim "$name" "$stage" "$draws" "$due" >"$tmp/detail"; then
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
