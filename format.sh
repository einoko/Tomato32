#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="format"

usage() {
	cat <<'EOF'
Usage: ./format.sh [--check]

Formats project code under app/ and platform/ (plus root CMake files), while
excluding lvgl/ and build output directories.

Options:
  --check   Verify formatting only (no file changes)
  -h, --help  Show this help
EOF
}

if [[ $# -gt 1 ]]; then
	usage
	exit 1
fi

if [[ $# -eq 1 ]]; then
	case "$1" in
	--check)
		MODE="check"
		;;
	-h | --help)
		usage
		exit 0
		;;
	*)
		usage
		exit 1
		;;
	esac
fi

cd "$ROOT_DIR"

if ! command -v clang-format >/dev/null 2>&1; then
	echo "Error: clang-format is required but not installed." >&2
	echo "Install on macOS: brew install clang-format" >&2
	exit 1
fi

have_cmake_format="false"
if command -v cmake-format >/dev/null 2>&1; then
	have_cmake_format="true"
fi

have_shfmt="false"
if command -v shfmt >/dev/null 2>&1; then
	have_shfmt="true"
fi

clang_files=()
while IFS= read -r -d '' file; do
	clang_files+=("$file")
done < <(
	find app platform \
		-type d \( -name lvgl -o -name build -o -name .git \) -prune -o \
		-type f \( -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o -name '*.hpp' \) \
		-print0
)

if [[ -f lv_conf.h ]]; then
	clang_files+=("lv_conf.h")
fi

cmake_files=()
while IFS= read -r -d '' file; do
	cmake_files+=("$file")
done < <(
	find . \
		-type d \( -name .git -o -name lvgl -o -name build \) -prune -o \
		-type f \( -name 'CMakeLists.txt' -o -name '*.cmake' \) \
		-print0
)

sh_files=()
while IFS= read -r -d '' file; do
	sh_files+=("$file")
done < <(
	find app platform \
		-type d \( -name lvgl -o -name build -o -name .git \) -prune -o \
		-type f -name '*.sh' -print0
)

if [[ -f format.sh ]]; then
	sh_files+=("format.sh")
fi

echo "Mode: $MODE"
echo "C/C++ files: ${#clang_files[@]}"
echo "CMake files: ${#cmake_files[@]}"
echo "Shell files: ${#sh_files[@]}"

if [[ "$MODE" == "check" ]]; then
	if ((${#clang_files[@]} > 0)); then
		for file in "${clang_files[@]}"; do
			clang-format --dry-run --Werror "$file"
		done
	fi

	if [[ "$have_cmake_format" == "true" ]]; then
		if ((${#cmake_files[@]} > 0)); then
			for file in "${cmake_files[@]}"; do
				cmake-format --check "$file"
			done
		fi
	else
		echo "Warning: cmake-format not found; skipping CMake format checks." >&2
	fi

	if [[ "$have_shfmt" == "true" ]]; then
		if ((${#sh_files[@]} > 0)); then
			for file in "${sh_files[@]}"; do
				shfmt -d "$file"
			done
		fi
	else
		echo "Warning: shfmt not found; skipping shell format checks." >&2
	fi

	echo "Formatting checks passed."
	exit 0
fi

if ((${#clang_files[@]} > 0)); then
	for file in "${clang_files[@]}"; do
		clang-format -i "$file"
	done
fi

if [[ "$have_cmake_format" == "true" ]]; then
	if ((${#cmake_files[@]} > 0)); then
		for file in "${cmake_files[@]}"; do
			cmake-format -i "$file"
		done
	fi
else
	echo "Warning: cmake-format not found; skipping CMake formatting." >&2
fi

if [[ "$have_shfmt" == "true" ]]; then
	if ((${#sh_files[@]} > 0)); then
		for file in "${sh_files[@]}"; do
			shfmt -w "$file"
		done
	fi
else
	echo "Warning: shfmt not found; skipping shell formatting." >&2
fi

echo "Formatting complete."
