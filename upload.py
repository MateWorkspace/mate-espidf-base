#!/usr/bin/env python3
"""Upload the built firmware binary to the mate-things backend for OTA testing.

Reads upload.config.json (url, username, password, filename, node_class_name),
logs in, resolves the node class, then either creates a new firmware row
(POST /v1/firmwares) or - if a firmware with that name already exists -
replaces its binary in place (PUT /v1/firmwares/{id}/binary), so re-running
this after every rebuild just updates the same firmware entry instead of
erroring on a duplicate name.

firmware_name is derived automatically (not read from config) as
`<project_name>_<project_version>`, matching what the device sends as
`firmware_name` at MQTT registration: project_name comes from the root
CMakeLists.txt's project(...) call and project_version comes from
version.txt - the backend keys firmware lookups by that name.

Usage:
    python3 upload.py [path/to/config.json]

Prints the created/updated firmware's id, size, checksum, and - since
BE_MINIO_PRESIGN_DURATION is short-lived (5m by default) - a freshly issued
presigned download URL, ready to paste into an OTA dispatch request
(POST /v1/nodes/by-device/{device_id}/ota) immediately after running this.
"""

import json
import re
import sys
from pathlib import Path

import requests


def load_config(path: Path) -> dict:
    with path.open() as f:
        return json.load(f)


_SCHEMA_ROW_RE = re.compile(
    r"X\(\s*\w+\s*,\s*(DOMAIN_MODELS_PRELOADED_\w+_KEY)\s*,\s*DOMAIN_MODELS_PRELOADED_VALUE_TYPE_(\w+)\s*\)"
)
_KEY_DEFINE_RE = re.compile(
    r'#define\s+(DOMAIN_MODELS_PRELOADED_\w+_KEY)\s+"([^"]+)"'
)
_TYPE_MAP = {"STRING": "string", "UINT32": "uint32", "BOOL": "bool"}


def parse_preloaded_schema(repo_root: Path) -> list[dict]:
    header_path = repo_root / "main" / "include" / "domain" / "models" / "preloaded.h"
    text = header_path.read_text()

    key_defines = dict(_KEY_DEFINE_RE.findall(text))

    schema = []
    for key_macro, type_suffix in _SCHEMA_ROW_RE.findall(text):
        if key_macro not in key_defines:
            raise SystemExit(
                f"preloaded.h schema row references undefined key macro '{key_macro}' - "
                "check that DOMAIN_MODELS_PRELOADED_SCHEMA(X) and the _KEY defines are in sync"
            )
        if type_suffix not in _TYPE_MAP:
            raise SystemExit(f"preloaded.h schema row has unrecognized value type suffix '{type_suffix}'")

        schema.append({"key": key_defines[key_macro], "value_type": _TYPE_MAP[type_suffix]})

    if not schema:
        raise SystemExit(
            "no config schema entries found in preloaded.h - "
            "check DOMAIN_MODELS_PRELOADED_SCHEMA(X) is present and well-formed"
        )

    return schema


_PROJECT_NAME_RE = re.compile(r"project\(\s*([^)\s]+)\s*\)")


def parse_project_name(repo_root: Path) -> str:
    text = (repo_root / "CMakeLists.txt").read_text()
    match = _PROJECT_NAME_RE.search(text)
    if not match:
        raise SystemExit("could not find project(...) in CMakeLists.txt")
    return match.group(1)


def parse_project_version(repo_root: Path) -> str:
    return (repo_root / "version.txt").read_text().strip()


def build_firmware_name(repo_root: Path) -> str:
    return f"{parse_project_name(repo_root)}_{parse_project_version(repo_root)}"


def login(base_url: str, username: str, password: str) -> str:
    resp = requests.post(
        f"{base_url}/v1/auth/login",
        json={"username": username, "password": password},
        timeout=10,
    )
    resp.raise_for_status()
    return resp.json()["access_token"]


def get_node_class_id(base_url: str, token: str, node_class_name: str) -> str:
    resp = requests.get(
        f"{base_url}/v1/node-classes/by-name/{node_class_name}",
        headers={"Authorization": f"Bearer {token}"},
        timeout=10,
    )
    if resp.status_code == 404:
        raise SystemExit(
            f"node class '{node_class_name}' does not exist on the backend - "
            "it should have been created by the backend's seeder "
            "(database/seeder/node_class.json). Check upload.config.json's "
            "node_class_name, or that the backend has been seeded."
        )
    resp.raise_for_status()
    return resp.json()["id"]


def find_existing_firmware_id(base_url: str, token: str, firmware_name: str) -> str | None:
    resp = requests.get(
        f"{base_url}/v1/firmwares/by-name/{firmware_name}",
        headers={"Authorization": f"Bearer {token}"},
        timeout=10,
    )
    if resp.status_code == 404:
        return None
    resp.raise_for_status()
    return resp.json()["id"]


def create_firmware(base_url: str, token: str, node_class_id: str, name: str, file_path: Path, config_schema: list[dict]) -> dict:
    with file_path.open("rb") as f:
        resp = requests.post(
            f"{base_url}/v1/firmwares",
            headers={"Authorization": f"Bearer {token}"},
            data={
                "node_class_id": node_class_id,
                "name": name,
                "config_schema": json.dumps(config_schema),
            },
            files={"file": (file_path.name, f, "application/octet-stream")},
            timeout=60,
        )
    resp.raise_for_status()
    return resp.json()


def replace_firmware_binary(base_url: str, token: str, firmware_id: str, file_path: Path, config_schema: list[dict]) -> dict:
    with file_path.open("rb") as f:
        resp = requests.put(
            f"{base_url}/v1/firmwares/{firmware_id}/binary",
            headers={"Authorization": f"Bearer {token}"},
            data={"config_schema": json.dumps(config_schema)},
            files={"file": (file_path.name, f, "application/octet-stream")},
            timeout=60,
        )
    resp.raise_for_status()
    return resp.json()


def get_presigned_download_url(base_url: str, token: str, firmware_id: str) -> str | None:
    resp = requests.get(
        f"{base_url}/v1/firmwares/{firmware_id}/binary",
        headers={"Authorization": f"Bearer {token}"},
        allow_redirects=False,
        timeout=10,
    )
    if resp.status_code != 302:
        return None
    return resp.headers.get("Location")


def main() -> None:
    config_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "upload.config.json"
    config = load_config(config_path)

    base_url = config["url"].rstrip("/")
    file_path = (Path(__file__).parent / config["filename"]).resolve()
    if not file_path.is_file():
        raise SystemExit(f"firmware binary not found at {file_path} - build it first (idf.py build)")

    config_schema = parse_preloaded_schema(Path(__file__).parent)
    print(f"Parsed {len(config_schema)} config parameter(s) from preloaded.h")

    firmware_name = build_firmware_name(Path(__file__).parent)
    print(f"Resolved firmware_name: {firmware_name}")

    print(f"Logging in to {base_url} as {config['username']}...")
    token = login(base_url, config["username"], config["password"])

    print(f"Resolving node class '{config['node_class_name']}'...")
    node_class_id = get_node_class_id(base_url, token, config["node_class_name"])

    existing_id = find_existing_firmware_id(base_url, token, firmware_name)

    if existing_id:
        print(f"Firmware '{firmware_name}' already exists (id={existing_id}) - replacing its binary...")
        result = replace_firmware_binary(base_url, token, existing_id, file_path, config_schema)
        firmware_id = existing_id
    else:
        print(f"Creating new firmware '{firmware_name}'...")
        result = create_firmware(base_url, token, node_class_id, firmware_name, file_path, config_schema)
        firmware_id = result["id"]

    print()
    print(f"firmware_id: {firmware_id}")
    print(f"size:        {result.get('size')}")
    print(f"checksum:    {result.get('checksum')}")
    print(f"binary_path: {result.get('binary_path')}")

    download_url = get_presigned_download_url(base_url, token, firmware_id)
    if download_url:
        print()
        print("Presigned download URL (short-lived - use it promptly for OTA dispatch):")
        print(download_url)


if __name__ == "__main__":
    main()
