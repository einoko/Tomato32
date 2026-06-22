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

### How to boot?

Tomato32 has two boot modes you can enter:

| Mode            | How to enter                                                                                                                                                   |
| --------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Normal**      | Hold the **PWR** button until you see the Tomato32 splash screen.                                                                                              |
| **Config mode** | Hold the **PWR** button for one second. While still holding **PWR**, immediately press and hold the **BOOT** button until the Tomato32 splash screen displays. |

### Serial monitor

The serial monitor is available in **config mode only**.

```sh
./build.sh monitor /dev/cu.usbmodem101
```

### Wi-Fi credentials via config drive

Enter **config mode** (see instructions in [How to boot?](#how-to-boot)). Connect the device to your computer with a USB-C cable, a drive will appear on your computer. Open `⁠TOMATO32_CONFIG.conf` on that drive and fill in your credentials:

```
WIFI_SSID=your_network
WIFI_PASS=your_password
TZ=UTC0
```

See [TZ format examples](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html) (`JST-9`, `CET-1CEST,M3.5.0/2,M10.5.0/3`, etc.).

Eject the drive, then power-cycle the device. The new settings take effect on the next boot.

> [!NOTE]
> **Why do I need Wi-Fi for a Pomodoro timer?**
>
> Wi-Fi is only used to sync the date and time via NTP and update the RTC. This is only needed for the daily "Focused today" statistic. Providing Wi-Fi credentials is completely optional. Without them, the timer will still work, but the "Focused today" stat may not reset at the correct local midnight.

## Settings

Press the **BOOT** button to open Settings. Press it again to return to the timer.

### Timer profiles

Tomato32 has three presets (A, B, C). Pick one on the left, tap **Edit durations** to configure it, then **Use profile** to go back to the timer.

| Setting       | Description                                      |
| ------------- | ------------------------------------------------ |
| Focus session | Focus phase length (minutes).                    |
| Short break   | Short break length (minutes).                    |
| Long break    | Long break length (minutes).                     |
| Rounds        | Focus sessions before a long break is triggered. |

### Advance to next

| Option | Description                                                |
| ------ | ---------------------------------------------------------- |
| Auto   | Next phase starts automatically when the current one ends. |
| Manual | You have to manually start the next phase.                 |

### System settings

#### UI

| Setting         | Options      | Description                                                                                    |
| --------------- | ------------ | ---------------------------------------------------------------------------------------------- |
| Theme           | Light / Dark | Colour scheme.                                                                                 |
| Visual pulse    | On / Off     | Pulsing background color when the timer ends.                                                  |
| Custom backdrop | On / Off     | Shows a custom background on the timer screen (see [Custom Backgrounds](#custom-backgrounds)). |

#### Sound

| Setting | Options  | Description                     |
| ------- | -------- | ------------------------------- |
| Sound   | On / Off | Chime at the end of each phase. |
| Volume  | 0–100 %  | Chime volume.                   |

#### Screen

| Setting              | Options  | Description                                                                                               |
| -------------------- | -------- | --------------------------------------------------------------------------------------------------------- |
| Default brightness   | 0–100 %  | Screen brightness during normal use.                                                                      |
| Smart dim brightness | 0–100 %  | Brightness when smart dim is enabled.                                                                     |
| Visual pulse opacity | 0–100 %  | Opacity of the visual pulse color.                                                                        |
| Smart dim            | On / Off | Dims to the smart dim level after 60 seconds of inactivity while the timer runs. Tap anywhere to restore. |
| Smart sleep          | On / Off | Turns the screen off after 60 seconds of inactivity instead of dimming. Tap to wake up.                   |

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

The sound effect used for timer end is sourced from [freesound.org](https://freesound.org/people/JetRye/sounds/140128) and licensed under Creative Commons 0.

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
