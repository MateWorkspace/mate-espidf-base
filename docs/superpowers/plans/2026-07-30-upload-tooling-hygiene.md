# Upload Tooling Hygiene Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop committing real credentials in `upload.config.json`, and stop hand-maintaining `firmware_name` in it — derive it from `CMakeLists.txt` + `version.txt` instead.

**Architecture:** Two independent, small changes to the same script/repo: (1) git-hygiene around `upload.config.json` (untrack, gitignore, add a `.example` template), (2) two small parser functions in `upload.py` that replace the manually-typed `firmware_name` config field.

**Tech Stack:** Python 3.10+ (stdlib `re`/`pathlib` only, matching `upload.py`'s existing style), git.

## Global Constraints

- No new dependencies — `upload.py` uses only `json`, `re`, `sys`, `pathlib.Path`, and `requests` today; stay within that.
- Match `upload.py`'s existing code style: module-level compiled regexes prefixed `_`, functions typed with `Path`/`str`/`list[dict]` etc., `raise SystemExit(...)` for user-facing fatal errors (never a bare exception).
- `node_class_name` validation requires no code change — `get_node_class_id` (upload.py:84-98) already covers it.

---

### Task 1: Untrack `upload.config.json`, add `.gitignore` entry and `.example`

**Files:**
- Modify: `.gitignore`
- Create: `upload.config.json.example`
- Modify (git index only, not file content): `upload.config.json` — remove from tracking

**Interfaces:** none (pure repo/config hygiene, no code).

- [ ] **Step 1: Add the gitignore entry**

Append to `.gitignore` (top-level, alongside the existing entries):
```
upload.config.json
```

- [ ] **Step 2: Create the example template**

Create `upload.config.json.example`:
```json
{
  "url": "http://<backend-host>:8080/api",
  "username": "admin",
  "password": "<password>",
  "filename": "build/mate-espidf-base.bin",
  "node_class_name": "base_node"
}
```
(No `firmware_name` field — Task 2 removes the need for it.)

- [ ] **Step 3: Untrack the real config file**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
git rm --cached upload.config.json
```
This removes it from git's index only — the file stays on disk unchanged (verify with `ls upload.config.json` and `cat upload.config.json` afterward — both should still show the real local file).

- [ ] **Step 4: Verify**

```bash
git status
```
Expected: `upload.config.json` no longer appears as tracked (not listed as modified — it's now untracked and ignored, so plain `git status` won't show it at all); `.gitignore` shows as modified; `upload.config.json.example` shows as a new untracked file ready to be added.

- [ ] **Step 5: Commit**

```bash
git add .gitignore upload.config.json.example
git commit -m "chore: stop tracking upload.config.json, add .example template"
```

---

### Task 2: Automate `firmware_name` from `CMakeLists.txt` + `version.txt`

**Files:**
- Modify: `upload.py`
- Modify: `upload.config.json.example` (remove any lingering `firmware_name` reference — should already be absent per Task 1, just confirm)

**Interfaces:**
- Produces: `parse_project_name(repo_root: Path) -> str`, `parse_project_version(repo_root: Path) -> str`, `build_firmware_name(repo_root: Path) -> str`. `main()` calls `build_firmware_name` instead of reading `config["firmware_name"]`.

- [ ] **Step 1: Add the parser functions**

Add to `upload.py`, right after `parse_preloaded_schema` (before `login`):
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

- [ ] **Step 2: Verify the parsers standalone**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
python3 -c "
from pathlib import Path
import sys
sys.path.insert(0, '.')
from upload import parse_project_name, parse_project_version, build_firmware_name
print(parse_project_name(Path('.')))
print(parse_project_version(Path('.')))
print(build_firmware_name(Path('.')))
"
```
Expected output (three lines):
```
mate-espidf-base
v1.0.0-dev.1
mate-espidf-base_v1.0.0-dev.1
```
This must match today's manually-typed `firmware_name` value in the old `upload.config.json` (`"mate-espidf-base_v1.0.0-dev.1"`) exactly — that's the proof the automation is correct.

- [ ] **Step 3: Wire it into `main()`**

In `main()`, remove the line that reads `firmware_name = config["firmware_name"]` and replace it with a call to the new function, placed right after `config_schema` is computed (same spot, same style):
```python
    config_schema = parse_preloaded_schema(Path(__file__).parent)
    print(f"Parsed {len(config_schema)} config parameter(s) from preloaded.h")

    firmware_name = build_firmware_name(Path(__file__).parent)
    print(f"Resolved firmware_name: {firmware_name}")
```
Remove the old `firmware_name = config["firmware_name"]` line further down (it's currently right before `find_existing_firmware_id` is called) — the rest of `main()` (the `find_existing_firmware_id`/`create_firmware`/`replace_firmware_binary` calls using `firmware_name`) stays unchanged since the variable name is the same.

- [ ] **Step 4: Confirm `upload.config.json.example` has no `firmware_name` field**

```bash
grep -n firmware_name upload.config.json.example
```
Expected: no output (grep exits 1, meaning no match). If it does contain the field (shouldn't, since Task 1 already wrote it without one), remove it.

- [ ] **Step 5: Run a real upload against the running backend**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
idf.py build
python3 upload.py
```
Expected: script prints `Resolved firmware_name: mate-espidf-base_v1.0.0-dev.1`, then proceeds through login/node-class resolution/create-or-replace exactly as before. If the shared backend's MinIO is still offline (a known pre-existing infra issue from the dynamic-config sub-project, unrelated to this change), the request may fail at the storage layer — if so, confirm everything up to and including the `config_schema`/`firmware_name` construction succeeded (visible in the printed output before the failure), note the MinIO block explicitly, and don't treat it as a code defect in this task.

- [ ] **Step 6: Commit**

```bash
git add upload.py upload.config.json.example
git commit -m "feat: derive firmware_name from CMakeLists.txt + version.txt instead of hand-typing it"
```

---
