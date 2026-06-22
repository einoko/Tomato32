# Tomato32

![Photo of Tomato32 running on the ESP32-S3-Touch-LCD-3.49](docs/running_on_device.jpeg)

Tomato32 is a [Pomodoro](https://en.wikipedia.org/wiki/Pomodoro_Technique) timer for the [Waveshare ESP32-S3-Touch-LCD-3.49](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm) development board.

## Video

https://github.com/user-attachments/assets/c5f4a6f5-b433-481a-8e97-07911eff9add

## Features

- Touchscreen interface
- Three preset timer profiles, each with customizable durations
  - You can adjust the durations for focus time, short break, long break, and the number of focus sessions before a long break.
- Various settings to make it fit your workflow.
- Custom background support (see [Custom Backgrounds](#custom-backgrounds))
- Battery life of ~14-24 hours with always-on display, depending on screen brightness used (using the 18650 battery).
  - Battery life can be extended if screen dimming is enabled.

## Requirements

### Simulator (macOS)

- CMake
- SDL2

```sh
brew install cmake sdl2
```

### ESP32 Hardware Target

- [Waveshare ESP32-S3-Touch-LCD-3.49](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm) development board
- Python 3
- Docker (used by the ESP32 build/flash script)

## Quick Start (Simulator)

> [!NOTE]
> The simulator has only been tested on macOS. If you get it working on Linux or Windows, a PR adding setup instructions would be welcome.

```sh
cmake -B build
cmake --build build
./build/platform/simulator/pomodoro_sim
```

## ESP32 Build and Flash

### Build and flash

```sh
cd platform/esp32
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
./build.sh clean
./build.sh flash /dev/cu.usbmodem101
```

> [!NOTE]
> Replace `/dev/cu.usbmodem101` with the actual serial port of your ESP32 board. Run `ls /dev/cu.*` before and after plugging in the board to identify the correct port.

### Serial monitor

After flashing, the board presents a USB CDC serial port (in addition to the config drive). Use that port with the monitor command:

```sh
./build.sh monitor /dev/cu.usbmodem101
```

> [!NOTE]
> The CDC serial port is assigned by the OS and may differ from the port used for flashing. Run `ls /dev/cu.*` while the board is connected to find the right one.

### Wi-Fi credentials via config drive

When you plug in the board, a small drive called **TOMATO32** appears on your computer. Open `TOMATO32_CONFIG.txt` on that drive and fill in your credentials:

```
WIFI_SSID=your_network
WIFI_PASS=your_password
TZ=UTC0
```

See [TZ format examples](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html) (`JST-9`, `CET-1CEST,M3.5.0/2,M10.5.0/3`, etc.).

Safely eject the drive, then power-cycle the device. The new settings take effect on the next boot.

> [!NOTE]
> **Why do I need Wi-Fi for a Pomodoro timer?**
>
> Wi-Fi is only used to sync the date and time via NTP and update the RTC. This is only needed for the daily "Focused today" statistic. Providing Wi-Fi credentials is completely optional. Without them, the timer will still work, but the "Focused today" stat may not reset at the correct local midnight.

## Custom Backgrounds

To use a custom background image on the timer, go to **System Settings → UI → Custom Backdrop** and set it to **On**.

Place your image files (640×172 PNG) in the `app/` directory, then rebuild and flash.

### Custom background file naming

Tomato32 uses the most specific matching file available. Name your files accordingly:

| Specificity    | File name example              | When it's used           |
| -------------- | ------------------------------ | ------------------------ |
| Preset + theme | `custom_background_b_dark.png` | Preset B, dark mode only |
| Preset only    | `custom_background_b.png`      | Preset B, both themes    |
| Theme only     | `custom_background_dark.png`   | Any preset, dark mode    |
| _(fallback)_   |                                | Default solid color      |

Valid preset suffixes are `_a`, `_b`, and `_c`. Valid theme suffixes are `_dark` and `_light`.

> [!IMPORTANT]
> Background images are embedded into the firmware at build time. Whenever you add or remove PNG files in `app/`, run a clean build so CMake picks up the changes:
>
> ```sh
> cd platform/esp32
> ./build.sh clean
> ./build.sh flash /dev/cu.usbmodem101
> ```

## Third-Party Licenses

This project vendors LVGL under `lvgl/`.

- Project-level notices: `THIRD_PARTY_NOTICES.md`
- LVGL license: `lvgl/LICENCE.txt`
- LVGL third-party attributions: `lvgl/COPYRIGHTS.md`

### Typeface

The typeface used in the UI is [Inter](https://rsms.me/inter/), licensed under the SIL Open Font License 1.1.

### Sound effect

The sound effect used for timer start/end is sourced from [freesound.org](https://freesound.org/people/JetRye/sounds/140128) and licensed under Creative Commons 0.

### Custom background images

#### Dark mode

- Photo by [Federico Respini](https://unsplash.com/@federicorespini) on [Unsplash](https://unsplash.com/photos/brown-field-near-tree-during-daytime-sYffw0LNr7s).

#### Light mode

- Photo by [Alex Machado](https://unsplash.com/@alexmachado) on [Unsplash](https://unsplash.com/photos/cloudy-sky-80sv993lUKI).

## Screenshots

![Screenshot of the main timer screen](docs/tomato32_001.png)

![Screenshot of the main timer screen (custom backdrop)](docs/tomato32_002.png)

![Screenshot of the settings](docs/tomato32_003.png)

![Screenshot of the adjustment settings](docs/tomato32_004.png)

![Screenshot of the sound settings](docs/tomato32_005.png)
