#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
	cat <<'EOF'
Usage: ./update-lvgl.sh [release-tag]

Install or replace the vendored lvgl/ source tree with an LVGL release.
Example: ./update-lvgl.sh v9.6.0

When lvgl/ exists, a release tag is required and local changes stop the update.
When lvgl/ is absent, an omitted tag selects the latest stable release.
EOF
}

fail() {
	printf 'Error: %s\n' "$1" >&2
	exit 1
}

if [[ $# -eq 1 && ("$1" == "-h" || "$1" == "--help") ]]; then
	usage
	exit 0
fi

if [[ $# -gt 1 ]]; then
	usage >&2
	exit 2
fi

LVGL_PRESENT=0
if [[ -e "$ROOT_DIR/lvgl" || -L "$ROOT_DIR/lvgl" ]]; then
	if [[ ! -d "$ROOT_DIR/lvgl" ]]; then
		fail "lvgl exists but is not a directory."
	fi
	LVGL_PRESENT=1
fi

for tool in git curl tar; do
	command -v "$tool" >/dev/null 2>&1 || fail "$tool is required to update LVGL."
done

git -C "$ROOT_DIR" rev-parse --show-toplevel >/dev/null 2>&1 ||
	fail "the updater must run inside the Tomato32 Git checkout."

TAG=""
if [[ $# -eq 1 ]]; then
	TAG="$1"
elif [[ "$LVGL_PRESENT" -eq 1 ]]; then
	fail "a release tag is required when lvgl/ already exists."
else
	printf 'No release tag supplied and lvgl/ is absent; resolving the latest stable LVGL release.\n'
	LATEST_RELEASE_URL="$(curl --fail --location --silent --show-error \
		--output /dev/null --write-out '%{url_effective}' \
		https://github.com/lvgl/lvgl/releases/latest)" ||
		fail "could not resolve the latest stable LVGL release."
	case "$LATEST_RELEASE_URL" in
	https://github.com/lvgl/lvgl/releases/tag/*)
		TAG="${LATEST_RELEASE_URL##*/}"
		;;
	*)
		fail "GitHub returned an unexpected latest-release URL: $LATEST_RELEASE_URL"
		;;
	esac
	printf 'Resolved latest stable LVGL release: %s\n' "$TAG"
fi

if [[ ! "$TAG" =~ ^v[0-9]+[.][0-9]+[.][0-9]+$ ]]; then
	fail "release tag must use the vMAJOR.MINOR.PATCH format (for example, v9.6.0)."
fi

if [[ "$LVGL_PRESENT" -eq 1 && -n "$(git -C "$ROOT_DIR" status --porcelain --untracked-files=all -- lvgl)" ]]; then
	fail "lvgl/ has tracked or non-ignored untracked changes; commit or revert them before updating."
fi

WORK_DIR="$(mktemp -d "$ROOT_DIR/.lvgl-update.XXXXXX")"
BACKUP_DIR="$WORK_DIR/previous-lvgl"
ORIGINAL_MOVED=0
NEW_TREE_INSTALLED=0
UPDATE_COMPLETE=0
KEEP_WORK_DIR=0

cleanup() {
	local result=$?
	trap - EXIT

	if [[ "$UPDATE_COMPLETE" -ne 1 && "$ORIGINAL_MOVED" -eq 1 ]]; then
		if [[ -e "$ROOT_DIR/lvgl" || -L "$ROOT_DIR/lvgl" ]]; then
			if ! rm -rf "$ROOT_DIR/lvgl"; then
				printf 'Error: update failed and the new LVGL tree could not be removed. Prior tree remains at %s\n' "$BACKUP_DIR" >&2
				KEEP_WORK_DIR=1
				result=1
			else
				NEW_TREE_INSTALLED=0
			fi
		fi
		if [[ "$KEEP_WORK_DIR" -ne 1 ]]; then
			if mv "$BACKUP_DIR" "$ROOT_DIR/lvgl"; then
				ORIGINAL_MOVED=0
			else
				printf 'Error: update failed and the prior LVGL tree could not be restored. Backup remains at %s\n' "$BACKUP_DIR" >&2
				KEEP_WORK_DIR=1
				result=1
			fi
		fi
	fi
	if [[ "$UPDATE_COMPLETE" -ne 1 && "$ORIGINAL_MOVED" -ne 1 && "$NEW_TREE_INSTALLED" -eq 1 ]]; then
		if ! rm -rf "$ROOT_DIR/lvgl"; then
			printf 'Error: installation failed and the new LVGL tree could not be removed. It remains at %s/lvgl\n' "$ROOT_DIR" >&2
			KEEP_WORK_DIR=1
			result=1
		fi
	fi

	if [[ "$KEEP_WORK_DIR" -ne 1 ]]; then
		rm -rf "$WORK_DIR" || {
			printf 'Warning: could not remove temporary updater files at %s\n' "$WORK_DIR" >&2
			[[ "$result" -ne 0 ]] || result=1
		}
	fi

	exit "$result"
}
trap cleanup EXIT

ARCHIVE="$WORK_DIR/lvgl.tar.gz"
EXTRACT_DIR="$WORK_DIR/extracted"
mkdir "$EXTRACT_DIR"
ARCHIVE_URL="https://github.com/lvgl/lvgl/archive/refs/tags/${TAG}.tar.gz"

printf 'Downloading LVGL %s from %s\n' "$TAG" "$ARCHIVE_URL"
curl --fail --location --silent --show-error --retry 2 \
	--output "$ARCHIVE" "$ARCHIVE_URL"
tar -xzf "$ARCHIVE" -C "$EXTRACT_DIR"

shopt -s nullglob
ARCHIVE_ROOTS=("$EXTRACT_DIR"/*)
if [[ ${#ARCHIVE_ROOTS[@]} -ne 1 || ! -d "${ARCHIVE_ROOTS[0]}" ]]; then
	fail "the downloaded archive did not contain one LVGL source directory."
fi

NEW_LVGL="$WORK_DIR/new-lvgl"
mv "${ARCHIVE_ROOTS[0]}" "$NEW_LVGL"

for required_file in \
	CMakeLists.txt \
	LICENCE.txt \
	COPYRIGHTS.md \
	include/lvgl/lv_version.h; do
	if [[ ! -f "$NEW_LVGL/$required_file" ]]; then
		fail "the downloaded archive is missing $required_file."
	fi
done

read_version() {
	local header="$1"
	local major minor patch
	major="$(awk '$1 == "#define" && $2 == "LVGL_VERSION_MAJOR" { print $3; exit }' "$header")"
	minor="$(awk '$1 == "#define" && $2 == "LVGL_VERSION_MINOR" { print $3; exit }' "$header")"
	patch="$(awk '$1 == "#define" && $2 == "LVGL_VERSION_PATCH" { print $3; exit }' "$header")"
	if [[ ! "$major" =~ ^[0-9]+$ || ! "$minor" =~ ^[0-9]+$ || ! "$patch" =~ ^[0-9]+$ ]]; then
		return 1
	fi
	printf 'v%s.%s.%s' "$major" "$minor" "$patch"
}

ARCHIVE_VERSION="$(read_version "$NEW_LVGL/include/lvgl/lv_version.h")" ||
	fail "could not read the LVGL version from the downloaded source."
if [[ "$ARCHIVE_VERSION" != "$TAG" ]]; then
	fail "tag $TAG contains LVGL version $ARCHIVE_VERSION; refusing to install a mismatched archive."
fi

# Keep Git-ignored local metadata (such as Finder's .DS_Store) across the
# directory replacement. Tracked and non-ignored local changes were rejected
# above.
while IFS= read -r -d '' ignored_file; do
	relative_path="${ignored_file#lvgl/}"
	new_path="$NEW_LVGL/$relative_path"
	if [[ -e "$new_path" || -L "$new_path" ]]; then
		fail "ignored local file lvgl/$relative_path collides with the downloaded LVGL tree."
	fi
	mkdir -p "$(dirname "$new_path")"
	cp -pP "$ROOT_DIR/$ignored_file" "$new_path"
done < <(git -C "$ROOT_DIR" ls-files --others --ignored --exclude-standard -z -- lvgl)

printf 'Replacing lvgl/ with the validated LVGL %s source tree\n' "$TAG"
if [[ "$LVGL_PRESENT" -eq 1 ]]; then
	mv "$ROOT_DIR/lvgl" "$BACKUP_DIR"
	ORIGINAL_MOVED=1
fi
mv "$NEW_LVGL" "$ROOT_DIR/lvgl"
NEW_TREE_INSTALLED=1

INSTALLED_VERSION="$(read_version "$ROOT_DIR/lvgl/include/lvgl/lv_version.h")" ||
	fail "could not read the installed LVGL version; restoring the prior tree."
if [[ "$INSTALLED_VERSION" != "$TAG" ]]; then
	fail "installed LVGL version is $INSTALLED_VERSION, expected $TAG; restoring the prior tree."
fi

UPDATE_COMPLETE=1
ORIGINAL_MOVED=0
NEW_TREE_INSTALLED=0
if [[ "$LVGL_PRESENT" -eq 1 ]]; then
	printf 'Updated lvgl/ to LVGL %s.\n' "$INSTALLED_VERSION"
else
	printf 'Installed LVGL %s into lvgl/.\n' "$INSTALLED_VERSION"
fi
