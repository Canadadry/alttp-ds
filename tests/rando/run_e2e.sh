#!/bin/sh
# Usage: run_e2e.sh [<case>...]
# Exit 0 when no case is DIVERGENT, 2 when a named case does not exist.
set -eu
export LC_ALL=C

cases_dir="$(cd "$(dirname "$0")" && pwd)/cases"

if [ "$#" -eq 0 ]; then
	if [ -d "$cases_dir" ]; then
		for dir in "$cases_dir"/*/; do
			if [ -f "$dir/config.json" ]; then set -- "$@" "$(basename "$dir")"; fi
		done
	fi
	if [ "$#" -eq 0 ]; then
		echo "e2e: no cases"
		exit 0
	fi
else
	for name in "$@"; do
		if [ ! -f "$cases_dir/$name/config.json" ]; then
			echo "run_e2e.sh: no such case: $name" >&2
			exit 2
		fi
	done
fi

for name in "$@"; do
	echo "PENDING $name"
done
