# Upload tooling hygiene design

## Context

`upload.py` is the local dev script that logs into the backend, resolves a
node class, and creates/replaces a firmware row + binary. It reads its
settings from `upload.config.json`, which today is:

- **Tracked in git** with real-looking credentials committed in history
  (`url`, `username`, `password`, plus `node_class_name` and a manually
  typed `firmware_name`).
- Missing a `.example` template for other contributors to copy.
- Carrying a `firmware_name` field that must be kept in sync by hand with
  what the device actually sends at MQTT registration
  (`"%s_%s" % (PROJECT_NAME, PROJECT_VER)`, built in
  `app_internal_messaging_callbacks_impl_build_firmware_name`) — a value
  that can already be derived from `CMakeLists.txt` and `version.txt`.

`node_class_name` validation was also flagged as a target for this
sub-project, but `get_node_class_id` already validates it (raises with a
clear, actionable message before any upload happens if the class doesn't
exist on the backend) — no change needed there.

## 1. Untrack `upload.config.json`, add a `.example`

- `git rm --cached upload.config.json` — removes it from tracking going
  forward; the real local file on disk is untouched.
- Add `upload.config.json` to `.gitignore`.
- Add `upload.config.json.example` (tracked) with placeholder values:
  ```json
  {
    "url": "http://<backend-host>:8080/api",
    "username": "admin",
    "password": "<password>",
    "filename": "build/mate-espidf-base.bin",
    "node_class_name": "base_node"
  }
  ```
  (`node_class_name` keeps a real seeded value as a useful hint;
  `firmware_name` is omitted since it's being automated away — see below.)

Not in scope: rewriting git history to purge the previously committed real
values — a separate, more invasive concern if ever needed.

## 2. Automate `firmware_name`

Add to `upload.py`, near `parse_preloaded_schema`:

```python
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
```

This mirrors `app_internal_messaging_callbacks_impl_build_firmware_name`'s
`"%s_%s"` format exactly, so the uploaded firmware's name is guaranteed to
match what the device registers with — no manual field to drift.

`main()` changes: drop `config["firmware_name"]`; compute
`firmware_name = build_firmware_name(Path(__file__).parent)` unconditionally,
right where `config_schema` is already computed. Remove `firmware_name`
from `upload.config.json`/`.example`.

## 3. `node_class_name` validation

No change — already handled by `get_node_class_id`.

## Verification

1. Run the new parsers standalone against the real repo files and confirm
   `parse_project_name` → `mate-espidf-base`, `parse_project_version` →
   `v1.0.0-dev.1`, `build_firmware_name` → `mate-espidf-base_v1.0.0-dev.1`
   (matching today's manually-typed value, proving the automation is
   correct).
2. `git status` after `git rm --cached` shows `upload.config.json` as
   untracked-but-present locally, `.example` as a new tracked file, and
   `.gitignore` updated.
3. Run a real upload against the running backend and confirm the created/
   updated firmware's name matches what a real device sends at MQTT
   registration (cross-check against the known MinIO-offline infra issue
   from the dynamic-config sub-project — retry once storage is healthy if
   still blocked).
