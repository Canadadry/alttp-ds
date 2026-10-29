#!/bin/sh
# Usage: gen_golden.sh [--fresh]
# Never called by `make test` or `make e2e`: regenerating goldens is a deliberate act (CLAUDE.md).
set -eu
export LC_ALL=C

VT_URL=https://github.com/sporchia/alttp_vt_randomizer.git
VT_PIN=219fcafd029dab597b8db400efafd8f56f8b4edb
PHP_EXPECTED=8.3

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
vt_dir="${VT_DIR:-$oracle_dir/vt}"

usage() {
	echo "usage: gen_golden.sh [--fresh]" >&2
	exit 2
}

fresh=0
while [ "$#" -gt 0 ]; do
	case "$1" in
	--fresh) fresh=1 ;;
	*) usage ;;
	esac
	shift
done

if [ "$fresh" -eq 1 ]; then rm -rf "$vt_dir"; fi
if [ ! -d "$vt_dir/.git" ]; then
	mkdir -p "$vt_dir"
	git -C "$vt_dir" init -q
	git -C "$vt_dir" remote add origin "$VT_URL"
fi
if ! git -C "$vt_dir" cat-file -e "$VT_PIN^{commit}" 2>/dev/null; then
	git -C "$vt_dir" fetch -q --depth 1 origin "$VT_PIN"
fi
git -C "$vt_dir" -c advice.detachedHead=false checkout -q --force "$VT_PIN"
git -C "$vt_dir" reset -q --hard "$VT_PIN"
# -fd without -x keeps VT's git-ignored vendor/ between runs; --fresh starts from nothing.
git -C "$vt_dir" clean -q -fd
git -C "$vt_dir" apply "$oracle_dir/vt.patch"

# ext-bcmath: this container's PHP lacks it, and only libraries with non-bcmath fallbacks reference it.
(cd "$vt_dir" && COMPOSER_ALLOW_SUPERUSER=1 composer install -q --no-dev --no-interaction \
	--ignore-platform-req=ext-bcmath)

php_version="$(php -r 'echo PHP_VERSION;')"
echo "gen_golden: php $php_version, $(COMPOSER_ALLOW_SUPERUSER=1 composer --version 2>/dev/null | head -n 1), vt $(git -C "$vt_dir" rev-parse HEAD)"
case "$php_version" in
"$PHP_EXPECTED".*) ;;
*) echo "gen_golden: warning: php $php_version, the goldens were made with $PHP_EXPECTED" >&2 ;;
esac

php "$oracle_dir/prng_check.php" "$vt_dir"
