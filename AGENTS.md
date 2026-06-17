# AGENTS.md

## Project overview

Tomato32 — a Pomodoro timer built with LVGL, targeting two platforms:
- **Simulator**: macOS SDL2 desktop app for local development
- **ESP32**: Waveshare ESP32-S3-Touch-LCD-3.49 (built via Docker, no local ESP-IDF needed)

## Repository layout

- `app/` — shared application code (platform-agnostic, links against LVGL). All screens and Pomodoro logic live here.
- `app/fonts/` — pre-generated LVGL Inter font files (do not hand-edit).
- `platform/simulator/` — SDL2 desktop simulator entry point and drivers.
- `platform/esp32/` — ESP32-S3 target: IDF project, partition table, SPIFFS image, Docker build script.
- `platform/esp32/components/codec_board/` — vendored audio/LCD driver for the Waveshare board.
- `lvgl/` — vendored LVGL library. **Do not modify.**

## Dual-platform build

Code uses `#ifdef ESP_PLATFORM` to branch at compile time (see `app/CMakeLists.txt`, `lv_conf.h`). The simulator build excludes ESP-IDF; the ESP32 build excludes SDL2.

## Build & run commands

**Simulator (macOS):**
```sh
cmake -B build
cmake --build build
./build/platform/simulator/pomodoro_sim
```

**ESP32 (Docker-based):**
```sh
cd platform/esp32
cp .env.example .env   # first time; set WIFI_SSID / WIFI_PASS
./build.sh                           # build only
./build.sh flash /dev/cu.usbmodem101 # build + flash
./build.sh monitor /dev/cu.usbmodem101 # build + flash + monitor
./build.sh clean                     # full clean
```
Flashing requires `esptool` on the host (`pip install esptool`). Monitoring requires `esp-idf-monitor` or falls back to `miniterm`.

## Formatting

```sh
./format.sh          # auto-format (clang-format, cmake-format, shfmt)
./format.sh --check  # check only, exit on failure
```
Formats `app/`, `platform/`, root `CMakeLists.txt`, `lv_conf.h`, and `format.sh` itself. Excludes `lvgl/` and `build/`.

## ESP32 build-time code generation

On ESP32 builds, `app/gen_bg_images.py` converts `custom_background_dark.png` and `custom_background_light.png` into RGB565 C arrays. These generated `.c`/`.h` files go into the build directory, not source. The simulator build does not run this step.

## ESP32 persistence

State and theme are saved to SPIFFS (`/spiffs/.pomodoro_state`, `/spiffs/.pomodoro_theme`). Wi-Fi credentials are read from `platform/esp32/.env` at CMake configure time — this file is gitignored and must be created from `.env.example`.

## No automated tests

There is no test suite in this repo. Verify changes by running the simulator and visually checking behavior.