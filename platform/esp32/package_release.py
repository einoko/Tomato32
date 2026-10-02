#!/usr/bin/env python3
"""Package ESP-IDF build outputs as a versioned Tomato32 browser-flasher bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shlex
import sys
import zipfile
from pathlib import Path


BOARD_ID = "waveshare-esp32-s3-touch-lcd-3.49"
BOARD_NAME = "Waveshare ESP32-S3-Touch-LCD-3.49"
EXPECTED_FULL_IMAGES = {
    0x0000: "bootloader.bin",
    0x8000: "partition-table.bin",
    0x10000: "app.bin",
    0x830000: "storage.bin",
}
EXPECTED_FLASH_SETTINGS = {"flash_mode": "dio", "flash_freq": "80m", "flash_size": "16MB"}


def parse_args_file(path: Path) -> tuple[dict[str, str], list[tuple[int, str]]]:
    tokens = shlex.split(path.read_text(encoding="utf-8"))
    settings: dict[str, str] = {}
    images: list[tuple[int, str]] = []
    setting_flags = {"--flash_mode": "flash_mode", "--flash_freq": "flash_freq", "--flash_size": "flash_size"}
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if token in setting_flags:
            if index + 1 >= len(tokens):
                raise ValueError(f"Missing value after {token} in {path}")
            settings[setting_flags[token]] = tokens[index + 1]
            index += 2
            continue
        if re.fullmatch(r"0x[0-9a-fA-F]+", token):
            if index + 1 >= len(tokens):
                raise ValueError(f"Missing image path after {token} in {path}")
            images.append((int(token, 16), tokens[index + 1]))
            index += 2
            continue
        raise ValueError(f"Unexpected token {token!r} in {path}")
    return settings, images


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_manifest_images(
    build_dir: Path, pairs: list[tuple[int, str]], name_by_address: dict[int, str], folder: str
) -> list[dict[str, str]]:
    images = []
    for address, build_relative_path in sorted(pairs):
        if address not in name_by_address:
            raise ValueError(f"Unexpected flash address {address:#x}")
        source_path = (build_dir / build_relative_path).resolve()
        if not source_path.is_file() or not source_path.is_relative_to(build_dir.resolve()):
            raise ValueError(f"Missing or out-of-build firmware image: {build_relative_path}")
        archive_path = f"images/{folder}/{name_by_address[address]}"
        images.append(
            {
                "path": archive_path,
                "address": f"0x{address:x}",
                "sha256": sha256(source_path),
                "_source": str(source_path),
            }
        )
    if {int(image["address"], 16) for image in images} != set(name_by_address):
        raise ValueError(f"Flash image list does not match expected addresses for {folder}")
    return images


def public_images(images: list[dict[str, str]]) -> list[dict[str, str]]:
    return [{key: image[key] for key in ("path", "address", "sha256")} for image in images]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True, help="Stable release tag, for example v1.2.3")
    parser.add_argument("--build-dir", type=Path, default=Path("platform/esp32/build"))
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    args = parser.parse_args()

    if not re.fullmatch(r"v\d+\.\d+\.\d+", args.version):
        parser.error("--version must be a stable vX.Y.Z tag")

    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()
    try:
        flasher = json.loads((build_dir / "flasher_args.json").read_text(encoding="utf-8"))
        settings = flasher["flash_settings"]
        if settings != EXPECTED_FLASH_SETTINGS:
            raise ValueError(f"Unexpected firmware flash settings: {settings!r}")
        full_pairs = [(int(address, 16), relative_path) for address, relative_path in flasher["flash_files"].items()]
        if {address for address, _ in full_pairs} != set(EXPECTED_FULL_IMAGES):
            raise ValueError("ESP-IDF full flash image addresses differ from the supported board layout")

        app_settings, app_pairs = parse_args_file(build_dir / "flash_app_args")
        if app_settings != EXPECTED_FLASH_SETTINGS:
            raise ValueError(f"Unexpected app-update flash settings: {app_settings!r}")
        if len(app_pairs) != 1 or app_pairs[0][0] != 0x10000:
            raise ValueError("ESP-IDF app-only flash args must contain only the app at 0x10000")

        full_images = parse_manifest_images(build_dir, full_pairs, EXPECTED_FULL_IMAGES, "full")
        app_images = parse_manifest_images(build_dir, app_pairs, {0x10000: "app.bin"}, "update")
        manifest = {
            "schemaVersion": 1,
            "version": args.version,
            "board": {"id": BOARD_ID, "name": BOARD_NAME},
            "chip": "ESP32-S3",
            "flash": {"mode": "dio", "frequency": "80m", "size": "16MB"},
            "fullInstall": {
                "description": "Writes bootloader, partition table, application, and SPIFFS image.",
                "images": public_images(full_images),
            },
            "appUpdate": {
                "description": "Writes only the application partition and preserves SPIFFS data.",
                "images": public_images(app_images),
            },
        }

        output_dir.mkdir(parents=True, exist_ok=True)
        archive_path = output_dir / f"tomato32-firmware-{args.version}.zip"
        manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")
        with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            archive.writestr("manifest.json", manifest_bytes)
            for image in full_images + app_images:
                source_path = Path(image["_source"])
                archive.write(source_path, image["path"])
        sidecar = output_dir / f"{archive_path.name}.sha256"
        sidecar.write_text(f"{sha256(archive_path)}  {archive_path.name}\n", encoding="utf-8")
        print(f"Created {archive_path} ({archive_path.stat().st_size} bytes)")
        print(f"Created {sidecar}")
        return 0
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
