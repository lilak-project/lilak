#!/bin/bash
# elog_service_setup.sh -- interactive elog registration helper for LKRun
# NOTE: elog does not provide this file. Keep it in the system directory and run it.
set -euo pipefail

# Single stable entry point = the launcher. Everything is routed by project.
ELOG_LAUNCHER="${ELOG_LAUNCHER:-http://localhost:8010}"
ELOG_CONFIG_DIR="${ELOG_CONFIG_DIR:-meta}"

command -v python3 >/dev/null || { echo "python3 is required."; exit 1; }
mkdir -p "$ELOG_CONFIG_DIR"

# Write the interactive client to a temp file so prompts read the terminal.
# A stdin heredoc would be consumed and break input().
TMP="$(mktemp -t elog_setup.XXXXXX).py"
trap 'rm -f "$TMP"' EXIT

cat >"$TMP" <<'PY'
import getpass
import json
import os
import pathlib
import sys
import urllib.error
import urllib.request

LAUNCHER = sys.argv[1].rstrip("/")
CONFIG_DIR = pathlib.Path(sys.argv[2])
CONFIG_DIR.mkdir(parents=True, exist_ok=True)
CONFIG_FILE = CONFIG_DIR / "elog_config.json"
ENV_FILE = CONFIG_DIR / "elog_env.sh"

G = "\033[32m"
Y = "\033[33m"
C = "\033[36m"
B = "\033[1m"
R = "\033[0m"
RED = "\033[31m"
DIM = "\033[2m"

def hr():
    print(DIM + "-" * 52 + R)

def ask(prompt, default=None):
    suffix = f" [{default}]" if default else ""
    value = input(f"{prompt}{suffix}: ").strip()
    return value or (default or "")

def yes(prompt, default="N"):
    return ask(prompt + " (y/N)", default).lower().startswith("y")

def api(method, url, token=None, body=None):
    data = json.dumps(body).encode("utf-8") if body is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    try:
        with urllib.request.urlopen(req, timeout=20) as response:
            raw = response.read()
            return response.status, (json.loads(raw) if raw else None)
    except urllib.error.HTTPError as exc:
        raw = exc.read()
        try:
            return exc.code, json.loads(raw)
        except Exception:
            return exc.code, {"detail": raw.decode("utf-8", "replace")}
    except Exception as exc:
        return 0, {"detail": str(exc)}

def shell_quote(value):
    return "'" + value.replace("'", "'\"'\"'") + "'"

print(f"{B}  elog service / system registration{R}")
hr()

# 1) choose a project (experiment)
status, projects = api("GET", f"{LAUNCHER}/api/projects")
if status != 200 or not isinstance(projects, list) or not projects:
    print(f"{RED}No projects, or cannot reach launcher at {LAUNCHER}{R}")
    sys.exit(1)

print(f"{B}Projects:{R}")
for i, project in enumerate(projects):
    flag = f"{G}running{R}" if project.get("running") else f"{DIM}stopped{R}"
    print(f"  {B}{i + 1}{R}) {project['name']}  [{flag}]")

while True:
    try:
        project_name = projects[int(ask("Choose project number")) - 1]["name"]
        break
    except (ValueError, IndexError):
        print(f"{Y}invalid choice{R}")

base = f"{LAUNCHER}/p/{project_name}"
print(f"{C}-> {project_name}   elog_url = {base}{R}")
hr()

# 2) log in to that project
token = None
for _ in range(3):
    username = ask("elog username")
    password = getpass.getpass("elog password: ")
    status, result = api(
        "POST",
        f"{base}/api/auth/login",
        body={"username": username, "password": password},
    )
    if status == 200 and result and result.get("access_token"):
        token = result["access_token"]
        print(f"{G}OK logged in as {username}{R}")
        break
    print(f"{RED}login failed ({status}){R}")

if not token:
    sys.exit(1)
hr()

# 3) choose type
print(f"{B}Register as:{R}")
print(f"  {B}1{R}) service  {DIM}- elog requests data FROM your program{R}")
print(f"  {B}2{R}) system   {DIM}- your program PUSHES logs to elog (runs, DAQ, ...){R}")
while True:
    choice = ask("Choose 1 or 2", "2").lower()
    if choice in ("1", "service", "s"):
        is_system = False
        break
    if choice in ("2", "system", "sy"):
        is_system = True
        break
    print(f"{Y}please enter 1 or 2{R}")
is_main = yes("Is this the MAIN system?") if is_system else False
name = ask("Name", "lilak" if is_system else "MyService")
description = ask("Description", "LILAK/LKRun run control and elog push integration" if is_system else "")
url_label = "Command URL" if is_system else "Request URL"
service_url = ask(f"{url_label} (optional, where elog reaches your program)", "")
hr()

# 4) preview what THIS service/system sends and which format receives it
LOG_TYPE = {"init_of_run": 11, "start_of_run": 12, "end_of_run": 13, "monitoring_run": 14}

def _fields(fmt):
    return ", ".join(
        field.get("label", field.get("key", ""))
        for field in (fmt.get("fields") or [])
    )

status, formats = api("GET", f"{base}/api/formats", token=token)
if status == 200 and isinstance(formats, list):
    if is_system:
        runs = [
            fmt for fmt in formats
            if fmt.get("task_type") in LOG_TYPE
            and not (fmt.get("system_id") or fmt.get("subsystem_id"))
        ]
        print(f"{B}'{name}' will PUSH run logs to elog:{R}")
        for fmt in sorted(runs, key=lambda item: LOG_TYPE[item["task_type"]]):
            print(f"  log_type {B}{LOG_TYPE[fmt['task_type']]}{R} -> {C}{fmt['name']}{R}: {_fields(fmt)}")
        if not runs:
            print(f"{Y}  (no run-log formats found){R}")
    else:
        target = next((fmt for fmt in formats if fmt.get("name") == name + " log"), None)
        print(f"{B}elog will REQUEST data from '{name}', received into:{R}")
        if target:
            print(f"  {C}{target['name']}{R}: {_fields(target)}")
        else:
            print(f"{DIM}  a '{name} log' format will be created from the fields your")
            print(f"  program declares (handshake log_fields) on first connect.{R}")
else:
    print(f"{Y}could not load formats ({status}){R}")
hr()

if not yes("Proceed with registration?"):
    print("aborted.")
    sys.exit(0)

# 5) register via the API
payload = {
    "name": name,
    "description": description,
    "is_system": is_system,
    "is_main_system": is_main,
    "elog_url": base,
}
if service_url:
    if is_system:
        payload["command_url"] = service_url
    else:
        payload["request_url"] = service_url

status, result = api("POST", f"{base}/api/services", token=token, body=payload)
if status not in (200, 201):
    detail = result.get("detail") if isinstance(result, dict) else result
    print(f"{RED}registration failed ({status}): {detail}{R}")
    sys.exit(1)

print(f"{G}{B}OK registered '{name}' on {project_name}{R}")

# 6) systems get a push token -> save config with the stable proxy URL
push_token = result.get("token") if isinstance(result, dict) else None
if push_token:
    CONFIG_FILE.write_text(
        json.dumps({"elog_url": base, "elog_token": push_token}, indent=2) + "\n"
    )
    ENV_FILE.write_text(
        "export ELOG_URL=" + shell_quote(base) + "\n"
        "export ELOG_TOKEN=" + shell_quote(push_token) + "\n"
    )
    print(f"{G}OK saved {CONFIG_FILE} + {ENV_FILE}{R}")
    print(f"{DIM}  push logs to {base}/api/logs  (Authorization: Bearer <token>){R}")
else:
    print(f"{Y}Plain service: no token. elog will request data from {service_url or url_label}.{R}")

print(f"{C}elog_url is the launcher proxy URL -- project port changes will not break it.{R}")
PY

python3 "$TMP" "$ELOG_LAUNCHER" "$ELOG_CONFIG_DIR"
