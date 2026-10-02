#!/usr/bin/env python3
"""Copy published firmware releases into the Pages site for same-origin downloads."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
import urllib.error
import urllib.request
from pathlib import Path


TAG_PATTERN = re.compile(r"v\d+\.\d+\.\d+\Z")
ASSET_PREFIX = "tomato32-firmware-"
MAX_ASSET_BYTES = 20 * 1024 * 1024
MAX_TOTAL_BYTES = 900 * 1024 * 1024


def request_json(url: str, token: str) -> tuple[list[dict] | dict, str | None]:
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "Tomato32-GitHub-Pages-Release-Sync",
        },
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        body = json.loads(response.read())
        return body, response.headers.get("Link")


def next_page(link_header: str | None) -> str | None:
    if not link_header:
        return None
    for part in link_header.split(","):
        if 'rel="next"' in part:
            start = part.find("<") + 1
            end = part.find(">", start)
            if start > 0 and end > start:
                return part[start:end]
    return None


def download(url: str) -> bytes:
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": "Tomato32-GitHub-Pages-Release-Sync",
        },
    )
    with urllib.request.urlopen(request, timeout=180) as response:
        return response.read()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, required=True, help="Built Vite output directory")
    args = parser.parse_args()

    repository = os.environ.get("GITHUB_REPOSITORY", "einoko/Tomato32")
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if not token:
        parser.error("GITHUB_TOKEN or GH_TOKEN is required")
    dist = args.dist.resolve()
    if not dist.is_dir():
        parser.error(f"Pages build output does not exist: {dist}")

    firmware_dir = dist / "firmware"
    shutil.rmtree(firmware_dir, ignore_errors=True)
    firmware_dir.mkdir(parents=True, exist_ok=True)

    try:
        api_url = f"https://api.github.com/repos/{repository}/releases?per_page=100&page=1"
        releases: list[dict] = []
        while api_url:
            page, link = request_json(api_url, token)
            if not isinstance(page, list):
                raise ValueError("GitHub returned an unexpected Releases response")
            releases.extend(page)
            api_url = next_page(link)

        entries = []
        total_bytes = 0
        for release in releases:
            tag = release.get("tag_name", "")
            if release.get("draft") or release.get("prerelease") or not TAG_PATTERN.fullmatch(tag):
                continue
            asset_name = f"{ASSET_PREFIX}{tag}.zip"
            asset = next((item for item in release.get("assets", []) if item.get("name") == asset_name), None)
            if asset is None:
                continue
            size = int(asset.get("size", 0))
            if size <= 0 or size > MAX_ASSET_BYTES:
                raise ValueError(f"Unexpected firmware bundle size for {asset_name}: {size} bytes")
            total_bytes += size
            if total_bytes > MAX_TOTAL_BYTES:
                raise ValueError("Published firmware bundles exceed the GitHub Pages size budget (900 MiB).")

            # Release asset downloads redirect to a separate host. Do not send
            # the Actions token outside api.github.com.
            content = download(asset["browser_download_url"])
            if len(content) != size:
                raise ValueError(f"Downloaded {asset_name} has an unexpected size")
            digest = hashlib.sha256(content).hexdigest()
            reported_digest = asset.get("digest")
            if reported_digest and reported_digest.lower() != f"sha256:{digest}".lower():
                raise ValueError(f"GitHub asset digest does not match for {asset_name}")
            (firmware_dir / asset_name).write_bytes(content)
            entries.append(
                {
                    "tag": tag,
                    "name": release.get("name") or tag,
                    "published_at": release.get("published_at"),
                    "html_url": release.get("html_url"),
                    "assetPath": f"firmware/{asset_name}",
                    "assetSha256": digest,
                    "assetSize": size,
                }
            )
            print(f"Copied {asset_name} ({size} bytes)")

        entries.sort(key=lambda entry: tuple(int(part) for part in entry["tag"][1:].split(".")), reverse=True)
        index = {"schemaVersion": 1, "releases": entries}
        (dist / "releases.json").write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
        print(f"Wrote releases.json with {len(entries)} published stable release(s)")
        return 0
    except (OSError, urllib.error.URLError, json.JSONDecodeError, ValueError, KeyError, TypeError) as error:
        print(f"ERROR: Could not prepare Pages firmware bundles: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
