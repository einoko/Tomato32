#!/usr/bin/env bash
set -euo pipefail

# Docker-based ESP-IDF build script for the ESP32-S3 target.
# No local ESP-IDF installation required.
#
# On macOS, Docker runs inside a Linux VM and cannot access host /dev/ devices.
# Therefore: build happens inside Docker; flash/monitor happen on the host.
# Flashing requires esptool on the host: pip install esptool
#
# Usage:
#   ./build.sh                            # Build only
#   ./build.sh flash [/dev/cu.xxx]        # Build then flash from host
#   ./build.sh flash-app [/dev/cu.xxx]    # Build then flash app partition only (preserves SPIFFS)
#   ./build.sh monitor [/dev/cu.xxx]      # Build, flash, and monitor from host
#   ./build.sh monitor-app [/dev/cu.xxx]  # Build, app-only flash, and monitor from host
#   ./build.sh clean                      # Clean build artifacts

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
ESP32_DIR="${PROJECT_ROOT}/platform/esp32"

IMAGE="espressif/idf:v5.4"
CONTAINER_NAME="pomodoro_esp32_build"

DEVICE=""
DO_FLASH=false
DO_FLASH_APP_ONLY=false
DO_MONITOR=false
DO_CLEAN=false

# Parse arguments
for arg in "$@"; do
	if [[ "$arg" == /dev/* ]]; then
		DEVICE="$arg"
	elif [[ "$arg" == "flash" ]]; then
		DO_FLASH=true
	elif [[ "$arg" == "flash-app" || "$arg" == "app-flash" ]]; then
		DO_FLASH=true
		DO_FLASH_APP_ONLY=true
	elif [[ "$arg" == "monitor" || "$arg" == "flash-monitor" ]]; then
		DO_FLASH=true
		DO_MONITOR=true
	elif [[ "$arg" == "monitor-app" || "$arg" == "app-monitor" || "$arg" == "flash-app-monitor" ]]; then
		DO_FLASH=true
		DO_FLASH_APP_ONLY=true
		DO_MONITOR=true
	elif [[ "$arg" == "clean" ]]; then
		DO_CLEAN=true
	fi
done

# Auto-detect device if needed for flash/monitor
if $DO_FLASH && [ -z "${DEVICE}" ]; then
	# Skip Bluetooth, debug-console, and wlan-debug pseudo-devices
	DEVICE="$(ls /dev/cu.* 2>/dev/null |
		grep -viE 'bluetooth|debug-console|wlan-debug' |
		head -n1 || true)"
	if [ -z "${DEVICE}" ]; then
		DEVICE="$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -n1 || true)"
	fi
	if [ -z "${DEVICE}" ]; then
		echo "ERROR: No USB serial device found. Connect your ESP32 and try again."
		exit 1
	fi
fi

[ -n "${DEVICE}" ] && echo "Using device: ${DEVICE}"

echo "================================================"
echo "ESP32-S3 Docker Build"
echo "Project root: ${PROJECT_ROOT}"
echo "ESP32 dir:    ${ESP32_DIR}"
echo "Docker image: ${IMAGE}"
echo "================================================"

# Ensure docker is available
if ! command -v docker &>/dev/null; then
	echo "ERROR: docker is not installed."
	echo "Install Docker Desktop (macOS) or docker.io (Linux) and try again."
	exit 1
fi

# Pull the image if not present
if ! docker image inspect "${IMAGE}" &>/dev/null; then
	echo "Pulling ${IMAGE} ..."
	docker pull "${IMAGE}"
fi

# Use -t only if stdin is a terminal (avoids "input device is not a TTY" in CI)
TTY_FLAG=""
if [ -t 0 ]; then
	TTY_FLAG="-t"
fi

# Build (or clean) inside Docker — no device passthrough needed or possible on macOS
docker run --rm -i ${TTY_FLAG} \
	--name "${CONTAINER_NAME}" \
	-v "${PROJECT_ROOT}:${PROJECT_ROOT}" \
	-w "${ESP32_DIR}" \
	-e "TERM=${TERM:-xterm-256color}" \
	"${IMAGE}" \
	bash -c "
        set -e

        if ${DO_CLEAN}; then
            echo '=> Cleaning build...'
            find managed_components -name '.component_hash' -delete 2>/dev/null || true
            idf.py fullclean
            exit 0
        fi

        if [ ! -d build ] || [ ! -f build/config/sdkconfig.h ]; then
            echo '=> Setting target esp32s3...'
            idf.py set-target esp32s3
        else
            echo '=> Forcing fresh CMake configure...'
            rm -rf build/CMakeCache.txt build/CMakeFiles build/cmake_install.cmake
        fi

        echo '=> Building...'
        idf.py build
        echo '=> Build done.'
    "

$DO_CLEAN && exit 0

# Flash from host (Docker cannot access macOS serial devices)
if $DO_FLASH; then
	if $DO_FLASH_APP_ONLY; then
		echo "=> Flashing app partition only from host (preserving SPIFFS)..."
	else
		echo "=> Flashing from host..."
	fi
	ESPTOOL=""
	if command -v esptool.py &>/dev/null; then
		ESPTOOL="esptool.py"
	elif python3 -m esptool version &>/dev/null 2>&1; then
		ESPTOOL="python3 -m esptool"
	else
		echo "ERROR: esptool not found. Install it with:"
		echo "  pip install esptool"
		exit 1
	fi
	cd "${ESP32_DIR}/build"
	FLASH_ARGS_FILE="flash_args"
	if $DO_FLASH_APP_ONLY; then
		FLASH_ARGS_FILE="flash_app_args"
	fi
	if [ ! -f "${FLASH_ARGS_FILE}" ]; then
		echo "ERROR: ${FLASH_ARGS_FILE} not found in build output."
		echo "Run a build first, then retry."
		exit 1
	fi
	${ESPTOOL} --chip esp32s3 -p "${DEVICE}" -b 460800 \
		--before default_reset --after hard_reset \
		write-flash @"${FLASH_ARGS_FILE}"
	echo "=> Flash done."
fi

# Monitor from host
if $DO_MONITOR; then
	echo "=> Monitoring on ${DEVICE} (press Ctrl+] to exit)..."
	ELF="${ESP32_DIR}/build/pomodoro.elf"
	if python3 -m esp_idf_monitor --version &>/dev/null 2>&1; then
		python3 -m esp_idf_monitor --port "${DEVICE}" "${ELF}"
	else
		echo "NOTE: esp_idf_monitor not found; falling back to miniterm (no symbol decoding)."
		echo "  Install with: pip install esp-idf-monitor"
		python3 -m serial.tools.miniterm --raw "${DEVICE}" 115200
	fi
fi
