#!/bin/sh
# Usage: measure.sh
# Run by `make measure`, which first builds the host objects under MEASURE_DIR
# (default build/measure): host-O2/ and host-Os/, without RANDO_FAULTS. CC names
# the host compiler. The ARM builds are compiled here, each toolchain only when
# found.
# Prints the report on stdout and writes it to $MEASURE_DIR/report.txt.
# Exit 0 whatever the figures; 1 when a measurement cannot be taken.
set -eu
export LC_ALL=C

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
rando="$root/rando"
cc="${CC:-gcc}"
dir="${MEASURE_DIR:-$here/build/measure}"
case "$dir" in
/*) ;;
*) dir="$PWD/$dir" ;;
esac
jobs="$(nproc 2>/dev/null || echo 4)"

code_budget=131072
state_budget=32768
probes="RandoGen RandoOptions RandoPrng RandoPools RandoWorld RandoItems RandoConfig RandoFillState"

mkdir -p "$dir"
report="$dir/report.txt"
error=
finish() {
	cat "$report" >&3
	if [ -n "$error" ]; then
		echo "measure: $error" >&2
		exit 1
	fi
}
exec 3>&1 >"$report"
trap finish EXIT

die() {
	error="$1"
	exit 1
}

lib_srcs="$(cd "$rando/src" && find . -name '*.c' | sed 's|^\./||' | sort)"
[ -n "$lib_srcs" ] || die "no library source in $rando/src"

# figure <name> <value> <unit> [<budget>]: one report line, "over" past the budget.
figure() {
	if [ "$#" -ge 4 ]; then
		if [ "$2" -gt "$4" ]; then
			echo "$1 $2 $3 budget $4 over"
		else
			echo "$1 $2 $3 budget $4"
		fi
	else
		echo "$1 $2 $3"
	fi
}

# compile_lib <build> <compiler> <flag>...: the library and the size probe into
# $dir/<build>/, from scratch; dies with the compiler's messages on a failure.
compile_lib() {
	b="$dir/$1"
	c="$2"
	shift 2
	rm -rf "$b"
	mkdir -p "$b"
	n=0
	for s in $lib_srcs; do
		o="$b/obj/${s%.c}.o"
		mkdir -p "${o%/*}"
		"$c" "$@" -I"$rando/include" -c -o "$o" "$rando/src/$s" 2>>"$b/errors" &
		n=$((n + 1))
		if [ "$n" -ge "$jobs" ]; then
			wait
			n=0
		fi
	done
	"$c" "$@" -fno-common -I"$rando/include" -c -o "$b/size_probe.o" "$here/measure/size_probe.c" 2>>"$b/errors" &
	wait
	for s in $lib_srcs; do
		[ -f "$b/obj/${s%.c}.o" ] || die "$1 build failed: $(cat "$b/errors")"
	done
	[ -f "$b/size_probe.o" ] || die "$1 size probe failed: $(cat "$b/errors")"
}

# lib_objs <build>: sets objs to the build's library objects.
lib_objs() {
	objs=
	for s in $lib_srcs; do
		o="$dir/$1/obj/${s%.c}.o"
		[ -f "$o" ] || die "missing object: $o (run make measure)"
		objs="$objs $o"
	done
}

# state_sizes <build> <nm>
state_sizes() {
	p="$dir/$1/size_probe.o"
	[ -f "$p" ] || die "missing size probe: $p (run make measure)"
	"$2" -S "$p" >"$dir/$1/probe.nm" || die "$2 cannot read $p"
	parts=0
	for t in $probes; do
		hex="$(awk -v s="rando_probe_$t" '$4 == s { print $2 }' "$dir/$1/probe.nm")"
		[ -n "$hex" ] || die "size probe: no symbol rando_probe_$t in $1"
		v="$(printf '%d' "0x$hex")"
		eval "size_$t=$v"
		case "$t" in
		RandoGen) figure "$1.sizeof.RandoGen" "$v" bytes "$state_budget" ;;
		RandoFillState) echo "$1.sizeof.RandoFillState $v bytes stack-resident (local of run_attempt)" ;;
		*) figure "$1.sizeof.$t" "$v" bytes ;;
		esac
		case "$t" in
		RandoOptions | RandoPrng | RandoPools | RandoWorld) parts=$((parts + v)) ;;
		esac
		if [ "$t" = RandoWorld ]; then
			echo "$1.sizeof.RandoGen-rest $((size_RandoGen - parts)) bytes (counters, trace hook, padding)"
		fi
	done
}

# lib_size <build> <size>: the library objects' sections summed by class.
lib_size() {
	lib_objs "$1"
	: >"$dir/$1/sections"
	for o in $objs; do
		"$2" -A "$o" >>"$dir/$1/sections" || die "$2 cannot read $o"
	done
	awk -v b="$1" -v code_budget="$code_budget" -v rootdir="$dir/$1/obj/" '
	function line(name, v, budget) {
		if (budget == "") printf "%s.lib.%s %d bytes\n", b, name, v
		else printf "%s.lib.%s %d bytes budget %d%s\n", b, name, v, budget, (v > budget ? " over" : "")
	}
	/^[^ \t].* :$/ { obj = $1; sub(rootdir, "", obj); next }
	NF != 3 || $1 == "section" || $1 == "Total" { next }
	$1 ~ /^\.(debug|comment|note|ARM\.attributes|llvm_addrsig|group|rel|symtab|strtab|shstrtab)/ { next }
	$1 ~ /^\.text/ { code += $2; next }
	$1 ~ /^\.(rodata|data\.rel\.ro)/ { ro += $2; next }
	$1 ~ /^\.(data|tdata)/ { rw += $2; if ($2) flags = flags "FLAG: mutable static state in the library: " obj " " $1 "\n"; next }
	$1 ~ /^\.(bss|tbss)/ || $1 == "COMMON" { bss += $2; if ($2) flags = flags "FLAG: mutable static state in the library: " obj " " $1 "\n"; next }
	$2 > 0 { other += $2; if (!($1 in seen)) { seen[$1] = 1; names = names (names == "" ? "" : ", ") $1 } }
	END {
		line("code", code)
		line("rodata", ro)
		line("code+rodata", code + ro, code_budget)
		line("writable", rw, 0)
		line("bss", bss, 0)
		if (other) printf "%s.lib.other %d bytes not counted (%s)\n", b, other, names
		printf "%s", flags
	}' "$dir/$1/sections"
}

# frames <build>: the ten largest frames and every frame that is not static.
frames() {
	lib_objs "$1"
	: >"$dir/$1/su"
	for o in $objs; do
		su="${o%.o}.su"
		[ -f "$su" ] || die "missing stack usage: $su"
		cat "$su" >>"$dir/$1/su"
	done
	awk -F '\t' '{
		n = split($1, p, ":")
		f = p[1]; sub(/.*\//, "", f)
		print $2 "\t" p[n] "\t" f "\t" $3
	}' "$dir/$1/su" | sort -t "$(printf '\t')" -k1,1nr -k2,2 >"$dir/$1/frames"
	head -n 10 "$dir/$1/frames" | awk -F '\t' -v b="$1" '{ printf "%s.frame.%s %d bytes %s %s\n", b, $2, $1, $4, $3 }'
	awk -F '\t' -v b="$1" '
	$4 != "static" {
		n++
		printf "%s.non-static.%s %d bytes %s %s\n", b, $2, $1, $4, $3
		if ($4 ~ /dynamic/ && $4 !~ /bounded/) printf "FLAG: unbounded stack frame: %s (%s)\n", $2, $3
	}
	END { if (!n) printf "%s.non-static none\n", b }' "$dir/$1/frames"
}

dkp=
for d in "${DEVKITPRO:-}" /opt/devkitpro "${HOME:-}/devkitpro"; do
	if [ -n "$d" ] && [ -x "$d/devkitARM/bin/arm-none-eabi-gcc" ] &&
		"$d/devkitARM/bin/arm-none-eabi-gcc" --version >/dev/null 2>&1; then
		dkp="$d"
		break
	fi
done

clang=
clang_nm=
clang_size=
if command -v clang >/dev/null 2>&1; then
	clang="$(command -v clang)"
	clang_dir="$(dirname "$(readlink -f "$clang")")"
	for t in nm size; do
		p="$(command -v "llvm-$t" 2>/dev/null || true)"
		[ -n "$p" ] || [ ! -x "$clang_dir/llvm-$t" ] || p="$clang_dir/llvm-$t"
		eval "clang_$t=\$p"
	done
	if [ -z "$clang_nm" ] || [ -z "$clang_size" ]; then
		clang=
	fi
fi

builds="host-O2 host-Os"
arm_flags="-march=armv5te -mtune=arm946e-s -marm -O2"
if [ -n "$dkp" ]; then
	dkp_bin="$dkp/devkitARM/bin"
	compile_lib arm-dkp "$dkp_bin/arm-none-eabi-gcc" $arm_flags -ffunction-sections -fdata-sections -std=c99 -fstack-usage
	builds="$builds arm-dkp"
fi
if [ -n "$clang" ]; then
	compile_lib arm-clang "$clang" --target=arm-none-eabi $arm_flags -ffreestanding -std=c99 -fstack-usage -I"$here/measure/shim"
	builds="$builds arm-clang"
fi

nm_of() {
	case "$1" in
	host-*) echo nm ;;
	arm-dkp) echo "$dkp_bin/arm-none-eabi-nm" ;;
	arm-clang) echo "$clang_nm" ;;
	esac
}
size_of() {
	case "$1" in
	host-*) echo size ;;
	arm-dkp) echo "$dkp_bin/arm-none-eabi-size" ;;
	arm-clang) echo "$clang_size" ;;
	esac
}

echo "== 1. Environment =="
echo "host.cc $("$cc" --version | head -n 1)"
cpu="$(awk -F ': *' '/^model name/ { print $2; exit }' /proc/cpuinfo 2>/dev/null || true)"
echo "host.cpu ${cpu:-$(uname -m)}"
commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
if [ "$commit" != unknown ] && ! git -C "$root" diff-index --quiet HEAD -- 2>/dev/null; then
	commit="$commit dirty"
fi
echo "commit $commit"
if [ -n "$dkp" ]; then
	echo "arm-dkp.cc $("$dkp_bin/arm-none-eabi-gcc" --version | head -n 1) ($dkp)"
else
	echo "arm-dkp skipped: devkitARM not found"
fi
if [ -n "$clang" ]; then
	echo "arm-clang.cc $("$clang" --version | head -n 1)"
	echo "arm-clang.note asserts compiled out by the shim: its code size is a lower bound"
else
	echo "arm-clang skipped: clang not found"
fi
echo "flags no RANDO_FAULTS, -O2, no stack protector, no fortification"

echo
echo "== 2. State sizes =="
state_sizes host-O2 nm
[ "$size_RandoItems" -eq 716 ] ||
	die "size probe check failed: host RandoItems is $size_RandoItems bytes, expected 716"
for b in $builds; do
	case "$b" in
	host-*) ;;
	*) state_sizes "$b" "$(nm_of "$b")" ;;
	esac
done

echo
echo "== 3. Library size =="
for b in $builds; do
	lib_size "$b" "$(size_of "$b")"
done

echo
echo "== 4. Largest stack frames =="
for b in $builds; do
	frames "$b"
done
