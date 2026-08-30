#!/usr/bin/env python
import datetime as dt
import requests
import json
import os

REPO_URL = "https://github.com/AJ60/Flipper_OLED_PCF8574_PN532"

base_url = f"{os.environ.get('INDEXER_URL', REPO_URL)}/builds/firmware/dev"
artifact_tgz = f"{base_url}/{os.environ.get('ARTIFACT_TAG', 'oled-dev-update')}.tgz"
artifact_sdk = f"{base_url}/{os.environ.get('ARTIFACT_TAG', 'oled-dev-sdk').replace('update', 'sdk')}.zip"


if __name__ == "__main__":
    with open(os.environ["GITHUB_EVENT_PATH"], "r") as f:
        event = json.load(f)

    release = "release"
    before = event["before"]
    after = event["after"]
    compare = event["compare"].rsplit("/", 1)[0]

    version_tag = os.environ.get("VERSION_TAG", after[:8])

    requests.post(
        os.environ["BUILD_WEBHOOK"],
        headers={"Accept": "application/json", "Content-Type": "application/json"},
        json={
            "content": None,
            "embeds": [
                {
                    "title": f"New Dev Build: `{version_tag}`!",
                    "description": "",
                    "url": f"{REPO_URL}/commits/main",
                    "color": 16751147,
                    "fields": [
                        {
                            "name": "Code Diff:",
                            "value": "\n".join(
                                [
                                    f"[From last build ({before[:8]} to {after[:8]})]({compare}/{before}...{after})",
                                ]
                            ),
                        },
                        {
                            "name": "Changelog:",
                            "value": "\n".join(
                                [
                                    f"[View CHANGELOG]({REPO_URL}/blob/{after}/CHANGELOG.md)",
                                ]
                            ),
                        },
                        {
                            "name": "Firmware Artifacts:",
                            "value": "\n".join(
                                [
                                    f"- [🐬 Download Firmware TGZ]({artifact_tgz})",
                                    f"- [📦 SDK (for development)]({artifact_sdk})",
                                ]
                            ),
                        },
                    ],
                    "timestamp": dt.datetime.utcnow().isoformat(),
                }
            ],
        },
    )
