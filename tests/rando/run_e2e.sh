#!/bin/sh
# Usage: run_e2e.sh [<case>...]
# Exit 0 when no case is DIVERGENT, 2 when a named case does not exist.
set -eu
export LC_ALL=C

here="$(cd "$(dirname "$0")" && pwd)"
. "$here/cases.sh"
cases_dir="$here/cases"

names="$(discover_cases "$cases_dir" run_e2e.sh "$@")"
if [ -z "$names" ]; then
	echo "e2e: no cases"
	exit 0
fi
set -f
set -- $names
set +f

for name in "$@"; do
	echo "PENDING $name"
done
