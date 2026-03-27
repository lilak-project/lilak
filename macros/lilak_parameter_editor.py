#!/usr/bin/env python3

import argparse
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.parse
import webbrowser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


FORMAT_SUFFIXES = [".mac", ".conf", ".par", ".txt"]
FIND_LIMIT = 50


def build_search_paths():
    paths = [Path.cwd()]
    lilak_path = os.environ.get("LILAK_PATH", "")
    if lilak_path:
        lilak_root = Path(lilak_path)
        paths.append(lilak_root / "common")
        paths.append(lilak_root / "common" / "draw_style")
    return paths


def resolve_input_path(file_name: str) -> Path:
    path = Path(os.path.expanduser(os.path.expandvars(file_name)))
    if path.is_absolute() or file_name.startswith("."):
        candidate = path.resolve()
        if candidate.is_file():
            return candidate
        raise FileNotFoundError(file_name)

    has_suffix = path.suffix in FORMAT_SUFFIXES
    for base in build_search_paths():
        trial = (base / path).resolve()
        if has_suffix:
            if trial.is_file():
                return trial
            continue
        for suffix in FORMAT_SUFFIXES:
            suffixed = Path(f"{trial}{suffix}")
            if suffixed.is_file():
                return suffixed

    raise FileNotFoundError(file_name)


def split_value_and_comment(text: str):
    in_quotes = False
    for idx, char in enumerate(text):
        if char == '"':
            in_quotes = not in_quotes
        elif char == "#" and not in_quotes:
            return text[:idx].rstrip(), text[idx + 1 :].strip()
    return text.strip(), ""


def split_name_and_flags(raw_name: str):
    flags = {
        "rewrite": False,
        "temporary": False,
        "multiple": False,
        "conditional": False,
        "include": False,
    }
    prefix = ""
    name = raw_name
    while name:
        if name[0] == "!":
            flags["rewrite"] = True
            prefix += "!"
            name = name[1:]
            continue
        if name[0] == "*":
            flags["temporary"] = True
            prefix += "*"
            name = name[1:]
            continue
        if name[0] == "&":
            flags["multiple"] = True
            prefix += "&"
            name = name[1:]
            continue
        if name[0] == "@":
            flags["conditional"] = True
            prefix += "@"
            name = name[1:]
            continue
        if name[0] == "<":
            flags["include"] = True
            prefix += "<"
            name = name[1:]
            continue
        break
    return name, flags, prefix


def split_group_and_name(full_name: str):
    if "/" in full_name:
        group, name = full_name.rsplit("/", 1)
        return group, name
    return "", full_name


def update_group_context(indent, previous_indent, indent_stack, group_stack):
    current_group = ""
    if indent == 0:
        previous_indent = 0
        indent_stack.clear()
        group_stack.clear()
    elif indent == previous_indent:
        current_group = group_stack[-1] if group_stack else ""
        if len(group_stack) == len(indent_stack) + 1:
            group_stack.pop()
            current_group = group_stack[-1] if group_stack else ""
    elif indent < previous_indent:
        while indent_stack and indent_stack[-1] != indent:
            indent_stack.pop()
            if group_stack:
                group_stack.pop()
        previous_indent = indent_stack[-1] if indent_stack else 0
        current_group = group_stack[-1] if group_stack else ""
    else:
        if len(group_stack) == len(indent_stack) + 1:
            indent_stack.append(indent)
            current_group = group_stack[-1]
        else:
            current_group = group_stack[-1] if group_stack else ""
    return current_group, indent


def should_try_parameter(inner: str):
    if not inner:
        return False
    parts = inner.split(None, 1)
    token = parts[0]
    if token.startswith("#"):
        return False
    if token.endswith("/"):
        return True
    if "/" in token:
        return True
    if token[:1] in "!*&@<":
        return True
    if len(parts) == 1:
        return False

    value_head = parts[1].lstrip()[:1]
    if value_head in ['"', "'", "/", "{", "[", "(", "-", "+"]:
        return True
    if value_head.isdigit():
        return True

    value_token = parts[1].split(None, 1)[0].lower()
    if value_token in {"true", "false", "yes", "no", "on", "off"}:
        return True
    if value_token.startswith("k"):
        return True

    return False


def parse_parameter_candidate(inner: str, current_group: str):
    parts = inner.split(None, 1)
    raw_name = parts[0]
    remainder = parts[1] if len(parts) > 1 else ""
    parsed_name, flags, prefix = split_name_and_flags(raw_name)
    value, comment = split_value_and_comment(remainder)
    full_name = f"{current_group}{parsed_name}"

    if parsed_name.endswith("/") and not value:
        return {
            "kind": "group",
            "full_name": full_name.rstrip("/"),
            "comment": comment,
        }

    group, name = split_group_and_name(full_name)
    return {
        "kind": "parameter",
        "enabled": True,
        "group": group,
        "name": name,
        "value": value,
        "unit": prefix,
        "comment": comment,
        "flags": flags,
    }


def parse_parameter_text(content: str):
    rows = []
    previous_indent = 0
    indent_stack = []
    group_stack = []

    for line_no, raw_line in enumerate(content.splitlines(), start=1):
        if not raw_line.strip():
            continue

        stripped = raw_line.lstrip(" ")
        indent = len(raw_line) - len(stripped)
        current_group, previous_indent = update_group_context(
            indent, previous_indent, indent_stack, group_stack
        )

        if stripped.startswith("#"):
            comment_enabled = stripped.startswith("##")
            inner = stripped[2:].lstrip() if comment_enabled else stripped[1:].lstrip()
            if should_try_parameter(inner):
                parsed = parse_parameter_candidate(inner, current_group)
                if parsed["kind"] == "group":
                    group_stack.append(parsed["full_name"] + "/")
                    continue
                parsed["enabled"] = False
                parsed["line_no"] = line_no
                rows.append(parsed)
                continue

            rows.append(
                {
                    "kind": "comment",
                    "enabled": comment_enabled,
                    "group": "",
                    "name": "",
                    "value": "",
                    "unit": "",
                    "comment": inner,
                    "line_no": line_no,
                }
            )
            continue

        parsed = parse_parameter_candidate(stripped, current_group)
        if parsed["kind"] == "group":
            group_stack.append(parsed["full_name"] + "/")
            continue

        parsed["line_no"] = line_no
        rows.append(parsed)

    return rows


def serialize_rows(rows):
    lines = []
    previous_parameter_group = None
    previous_kind = None
    for row in rows:
        kind = row.get("kind", "parameter")
        if kind == "comment":
            comment = row.get("comment", "").strip()
            prefix = "##" if row.get("enabled", False) else "#"
            lines.append(f"{prefix} {comment}".rstrip())
            previous_kind = "comment"
            continue

        group = row.get("group", "").strip().strip("/")
        name = row.get("name", "").strip()
        full_name = f"{group}/{name}" if group and name else name
        if not full_name:
            continue

        value = row.get("value", "").strip()
        unit = row.get("unit", "").strip()
        comment = row.get("comment", "").strip()
        prefix = "" if row.get("enabled", True) else "#"
        group_key = f"{unit}{group}" if group else unit
        if lines and previous_kind == "parameter" and previous_parameter_group != group_key and lines[-1] != "":
            lines.append("")
        line = f"{prefix}{unit}{full_name}"
        if value:
            line += f"  {value}"
        if comment:
            line += f"  # {comment}"
        lines.append(line)
        previous_parameter_group = group_key
        previous_kind = "parameter"

    return "\n".join(lines) + ("\n" if lines else "")


def make_file_payload(path: Path):
    content = path.read_text(encoding="utf-8")
    return {
        "name": path.name,
        "path": str(path),
        "content": content,
        "rows": parse_parameter_text(content),
        "saved_at": time.strftime("%H:%M:%S"),
    }


def make_empty_payload():
    return {
        "name": "untitled_1.par",
        "path": "",
        "content": "",
        "rows": [],
        "saved_at": time.strftime("%H:%M:%S"),
    }


def resolve_browser_path(path_value: str, allow_missing=False):
    path_value = (path_value or "").strip()
    if not path_value:
        candidate = Path.cwd()
    else:
        candidate = Path(os.path.expanduser(os.path.expandvars(path_value)))
        if not candidate.is_absolute():
            candidate = (Path.cwd() / candidate).resolve()
        else:
            candidate = candidate.resolve()

    if candidate.exists():
        return candidate
    if allow_missing:
        return candidate
    raise FileNotFoundError(path_value)


def list_directory(path_value: str):
    target = resolve_browser_path(path_value)
    current_dir = target if target.is_dir() else target.parent
    entries = []
    for child in sorted(current_dir.iterdir(), key=lambda item: (not item.is_dir(), item.name.lower())):
        if child.name.startswith("."):
            continue
        if child.is_dir() or child.suffix.lower() in FORMAT_SUFFIXES:
            entries.append(
                {
                    "name": child.name,
                    "path": str(child.resolve()),
                    "is_dir": child.is_dir(),
                }
            )
    return {
        "current_dir": str(current_dir.resolve()),
        "parent_dir": str(current_dir.parent.resolve()) if current_dir.parent != current_dir else str(current_dir.resolve()),
        "entries": entries,
    }


def normalize_with_lkparametercontainer(content: str):
    repo_root = Path(__file__).resolve().parent.parent
    env = os.environ.copy()
    runtime_paths = [str(repo_root / "build")]
    try:
        root_libdir = subprocess.run(
            ["root-config", "--libdir"],
            cwd=repo_root,
            capture_output=True,
            text=True,
            check=False,
        ).stdout.strip()
        if root_libdir:
            runtime_paths.append(root_libdir)
    except OSError:
        pass

    if not env.get("NPLib_DIR"):
        research_root = repo_root.parent
        matches = list(research_root.rglob("libNPCore.dylib"))
        if matches:
            env["NPLib_DIR"] = str(matches[0].parent.parent)
            runtime_paths.append(str(matches[0].parent))

    for key in ("DYLD_LIBRARY_PATH", "LD_LIBRARY_PATH"):
        current = [item for item in env.get(key, "").split(":") if item]
        merged = []
        for item in runtime_paths + current:
            if item and item not in merged:
                merged.append(item)
        env[key] = ":".join(merged)

    with tempfile.TemporaryDirectory(prefix="lilak_par_norm_") as temp_dir:
        temp_path = Path(temp_dir)
        input_path = temp_path / "input.par"
        output_path = temp_path / "output.par"
        input_path.write_text(content, encoding="utf-8")

        result = subprocess.run(
            [
                "root",
                "-l",
                "-b",
                "-q",
                "-e",
                f'gROOT->Macro("{(repo_root / "macros/rootlogon.C").as_posix()}");',
                "-e",
                f'LKParameterContainer container; container.AddFile("{input_path.as_posix()}"); container.SaveAs("{output_path.as_posix()}", "raw:lcm:parcm");',
                "-e",
                "gSystem->Exit(0);",
            ],
            cwd=repo_root,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            details = (result.stderr or result.stdout or "ROOT normalization failed").strip()
            raise RuntimeError(details)
        if not output_path.exists():
            details = (result.stderr or result.stdout or "Normalized output was not created").strip()
            raise RuntimeError(details)
        normalized = output_path.read_text(encoding="utf-8")
        cleaned_lines = []
        for line in normalized.splitlines():
            stripped = line.strip()
            if "created from LKParameterContainer::Print" in stripped:
                continue
            if stripped.startswith("## input_par_0 ") or stripped.startswith("# input_par_0 "):
                continue
            cleaned_lines.append(line)
        return "\n".join(cleaned_lines).strip() + "\n"


def find_matching_files(query: str):
    query = query.strip().lower()
    if not query:
        return []

    results = []
    seen = set()
    for base in build_search_paths():
        if not base.exists():
            continue
        for suffix in FORMAT_SUFFIXES:
            for path in base.rglob(f"*{query}*{suffix}"):
                resolved = path.resolve()
                if resolved in seen:
                    continue
                seen.add(resolved)
                results.append(str(resolved))
                if len(results) >= FIND_LIMIT:
                    return sorted(results)
    return sorted(results)


HTML_PAGE = """<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LILAK Parameter Editor</title>
  <style>
    :root {
      --bg: #f8f8f3;
      --panel: rgba(255, 255, 255, 0.95);
      --panel-strong: rgba(255, 255, 255, 0.98);
      --line: #dbdbd6;
      --line-strong: #c9c9c2;
      --text: #232323;
      --muted: #6f6f6f;
      --accent: #3d6f9a;
      --accent-2: #5b7390;
      --focus: #6ea0cf;
      --selected: #eaf4ff;
      --comment: #f2f6fb;
      --danger: #7b3020;
      --ok: #2f6b57;
      --shadow: 0 12px 28px rgba(90, 90, 90, 0.08);
      --mono: "SFMono-Regular", "Menlo", "Monaco", monospace;
      --sans: "Helvetica Neue", Arial, sans-serif;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      color: var(--text);
      font-family: var(--sans);
      background: #ffffff;
    }
    .app {
      padding: 8px 10px;
      min-height: 100vh;
      display: grid;
      grid-template-rows: auto auto 1fr auto;
      gap: 6px;
    }
    .panel {
      background: var(--panel);
      backdrop-filter: blur(8px);
      border: 1px solid rgba(120, 99, 67, 0.12);
      border-radius: 18px;
      box-shadow: var(--shadow);
      overflow: visible;
    }
    .menu-bar {
      display: grid;
      gap: 6px;
      padding: 8px 10px;
      background: #ffffff;
      border-bottom: 1px solid rgba(120, 99, 67, 0.1);
    }
    .brand {
      font-size: 12px;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      color: var(--accent);
      margin-right: 0;
    }
    .menu-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      flex-wrap: wrap;
    }
    .menu-switches {
      display: flex;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
      margin-left: 0;
    }
    .menu-switch-button {
      border: 1px solid transparent;
      background: #d8e6f4;
      color: #203448;
      border-radius: 999px;
      padding: 4px 10px;
      cursor: pointer;
      font: inherit;
      font-size: 13px;
      font-weight: 400;
    }
    .menu-switch-button.active {
      background: #7ea9d1;
      color: #ffffff;
      border-color: #6b97c1;
    }
    .menu-switch-button[data-menu-toggle="file"] {
      background: #f7efcf;
      color: #6a5830;
      border-color: #e1d19c;
    }
    .menu-switch-button[data-menu-toggle="file"].active {
      background: #ddc16f;
      color: #ffffff;
      border-color: #c6aa57;
    }
    .menu-switch-button[data-menu-toggle="view"] {
      background: #efe7f8;
      color: #5a4674;
      border-color: #d6c5e8;
    }
    .menu-switch-button[data-menu-toggle="view"].active {
      background: #a88bc9;
      color: #ffffff;
      border-color: #9070b4;
    }
    .menu-panels {
      display: block;
    }
    .toolbar-group {
      display: flex;
      align-items: center;
      gap: 8px;
      padding: 6px 8px;
      background: #edf3f8;
      border: 0;
      border-radius: 10px;
      flex-wrap: wrap;
      width: 100%;
      min-height: 38px;
    }
    .menu-label {
      display: inline-block;
      width: 92px;
      flex: 0 0 92px;
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.12em;
      color: #111111;
      margin: 0;
    }
    .toolbar-button {
      border: 1px solid transparent;
      background: #d8e6f4;
      color: #203448;
      border-radius: 999px;
      padding: 4px 8px;
      cursor: pointer;
      font: inherit;
      font-weight: 400;
      font-size: 13px;
    }
    .toolbar-button:hover {
      filter: brightness(0.96);
    }
    .toolbar-button.active {
      background: #7ea9d1;
      color: #ffffff;
      border-color: #6b97c1;
    }
    .toolbar-group[data-menu-panel="file"] .toolbar-button {
      background: #f7efcf;
      color: #6a5830;
      border-color: #e5d7a9;
    }
    .toolbar-group[data-menu-panel="file"] .toolbar-button.active {
      background: #ddc16f;
      color: #ffffff;
      border-color: #c6aa57;
    }
    .toolbar-group[data-menu-panel="view"] .toolbar-button {
      background: #efe7f8;
      color: #5a4674;
      border-color: #ddd0ed;
    }
    .toolbar-group[data-menu-panel="view"] .toolbar-button.active {
      background: #a88bc9;
      color: #ffffff;
      border-color: #9070b4;
    }
    .toolbar-button.recent,
    .quick-button.recent {
      border-color: #355f87;
      box-shadow: inset 0 0 0 1px #355f87;
    }
    .toolbar-row {
      display: flex;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
    }
    .toolbar-row input {
      width: 240px;
      max-width: 100%;
      border: 1px solid var(--line);
      border-radius: 12px;
      padding: 9px 10px;
      background: #ffffff;
      font: inherit;
      color: var(--text);
    }
    .tab-bar {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      padding: 8px 10px;
      overflow: visible;
      background: #edf3f8;
      border-bottom: 1px solid rgba(120, 99, 67, 0.1);
    }
    .workspace-sticky {
      position: sticky;
      top: 0;
      z-index: 20;
      background: #edf3f8;
      box-shadow: 0 1px 0 rgba(120, 99, 67, 0.08);
    }
    .tab-toolbar {
      display: grid;
      gap: 6px;
      padding: 6px 10px 8px;
      background: #edf3f8;
      border-bottom: 1px solid rgba(120, 99, 67, 0.1);
    }
    .tab-toolbar .toolbar-group {
      background: #e3edf8;
    }
    .status-group {
      min-height: 38px;
    }
    .status-bar {
      display: flex;
      align-items: center;
      gap: 8px;
      padding: 6px 8px;
      background: #edf3f8;
      border-top: 1px solid rgba(120, 99, 67, 0.08);
      min-height: 38px;
      border-radius: 10px;
    }
    .status-text {
      flex: 1 1 auto;
      color: #1f3f5d;
      font-size: 13px;
      font-weight: 600;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .status-text.warn {
      color: #7b3020;
    }
    .status-text.ok {
      color: #2f6b57;
    }
    .message-bar {
      margin: 0;
      padding: 0;
      background: transparent;
      color: #1f3f5d;
      font-size: 13px;
      font-weight: 600;
    }
    .message-bar.warn {
      color: #7b3020;
    }
    .message-bar.ok {
      color: #2f6b57;
    }
    .tab {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      border-radius: 999px;
      padding: 6px 10px;
      background: #ffffff;
      border: 1px solid rgba(120, 99, 67, 0.12);
      cursor: pointer;
      white-space: nowrap;
      font-size: 13px;
    }
    .tab.active {
      background: #7ea9d1;
      color: #ffffff;
      border-color: #6b97c1;
    }
    .tab.targeted {
      box-shadow: inset 0 0 0 2px #3d6f9a;
    }
    .tab-close {
      border: 0;
      background: transparent;
      padding: 0;
      cursor: pointer;
      color: inherit;
      font: inherit;
    }
    .workspace {
      display: grid;
      grid-template-rows: auto 1fr;
      min-height: 0;
    }
    .workspace-head {
      display: none;
    }
    .head-left {
      display: flex;
      align-items: center;
      gap: 12px;
      min-width: 0;
    }
    .title-inline {
      display: flex;
      align-items: center;
      gap: 10px;
      min-width: 0;
      flex-wrap: nowrap;
    }
    .title-inline h1 {
      margin: 0;
      font-size: 22px;
      font-weight: 700;
      white-space: nowrap;
    }
    .title-inline p {
      margin: 0;
      font-size: 13px;
      color: var(--muted);
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .head-actions {
      display: flex;
      align-items: center;
      gap: 8px;
      margin-left: auto;
    }
    .quick-button {
      border: 1px solid #c9c9c2;
      background: #d8e6f4;
      color: #203448;
      border-radius: 999px;
      padding: 4px 8px;
      cursor: pointer;
      font: inherit;
      font-weight: 400;
      font-size: 13px;
    }
    .status {
      align-self: center;
      font-size: 13px;
      color: var(--muted);
    }
    .status.ok { color: var(--ok); }
    .status.warn { color: var(--danger); }
    .table-wrap {
      overflow-x: auto;
      overflow-y: visible;
      min-height: 0;
      background: #edf3f8;
    }
    .group-view-label {
      color: #315a80;
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.12em;
      margin-right: 2px;
    }
    .group-view-toggle {
      width: 28px;
      height: 28px;
      border: 1px solid #6b97c1;
      background: #ffffff;
      color: #315a80;
      border-radius: 8px;
      font: inherit;
      cursor: pointer;
      font-weight: 700;
      padding: 0;
      margin: 0;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      min-width: 980px;
      table-layout: fixed;
      font-size: 14px;
    }
    thead th {
      position: sticky;
      top: 0;
      z-index: 5;
      background: #4f7faa;
      border-bottom: 1px solid #3d678c;
      color: #ffffff;
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }
    .group-view-table thead tr.group-view-header th {
      top: 29px;
      z-index: 4;
      background: #dfeaf5;
      border-bottom: 1px solid rgba(120, 99, 67, 0.1);
      color: #315a80;
      text-transform: none;
      letter-spacing: 0;
      font-size: 12px;
      padding: 6px 10px;
    }
    th, td {
      padding: 4px 10px;
      border-bottom: 1px solid rgba(120, 99, 67, 0.08);
      vertical-align: middle;
    }
    th:nth-child(1), td:nth-child(1) { width: 54px; text-align: right; }
    th:nth-child(2), td:nth-child(2) { width: 68px; text-align: center; }
    th:nth-child(3), td:nth-child(3) { width: 180px; }
    th:nth-child(4), td:nth-child(4) { width: 180px; }
    th:nth-child(5), td:nth-child(5) { width: 220px; }
    th:nth-child(6), td:nth-child(6) { width: 84px; }
    tr.selected {
      background: var(--selected);
    }
    tr.comment-row {
      background: var(--comment);
    }
    tr.group-header td {
      background: #e1edf8;
      color: var(--accent-2);
      font-size: 12px;
      text-transform: uppercase;
      letter-spacing: 0.12em;
      padding-top: 4px;
      padding-bottom: 4px;
      text-align: left;
    }
    .group-header-bar {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .group-toggle {
      width: 28px;
      height: 28px;
      border: 1px solid #6b97c1;
      background: #ffffff;
      color: #315a80;
      border-radius: 8px;
      font: inherit;
      cursor: pointer;
      font-weight: 700;
      padding: 0;
      margin: 0;
    }
    .group-enabled-toggle {
      width: 22px;
      height: 22px;
      margin: 0;
    }
    .group-title {
      color: #315a80;
    }
    input[type="text"] {
      width: 100%;
      border: 1px solid transparent;
      background: rgba(255,255,255,0.94);
      border-radius: 10px;
      padding: 4px 9px;
      font: inherit;
      color: var(--text);
    }
    textarea {
      width: 100%;
      border: 1px solid transparent;
      background: rgba(255,255,255,0.96);
      border-radius: 10px;
      padding: 5px 10px;
      font: inherit;
      color: var(--text);
      resize: vertical;
    }
    input[type="checkbox"] {
      width: 20px;
      height: 20px;
      accent-color: var(--accent);
      cursor: pointer;
    }
    textarea:focus,
    input[type="text"]:focus {
      outline: none;
      border-color: var(--focus);
      box-shadow: 0 0 0 3px rgba(216,131,75,0.14);
    }
    .icon {
      display: inline-block;
      min-width: 14px;
      margin-right: 4px;
      color: var(--accent-2);
      text-align: center;
    }
    .row-id {
      color: var(--muted);
      font-family: var(--mono);
      font-size: 12px;
    }
    .comment-cell {
      padding-left: 0;
    }
    .comment-editor {
      width: 100%;
    }
    .raw-wrap {
      padding: 8px;
      background: #e7eef5;
    }
    .raw-toolbar {
      display: flex;
      gap: 8px;
      align-items: center;
      margin-bottom: 8px;
    }
    .raw-toolbar [data-raw-mode="normalized"] {
      background: #d9eadf;
      color: #1f5a3a;
      border-color: #9fc4ad;
    }
    .raw-toolbar [data-raw-mode="normalized"].active {
      background: #5f9a76;
      color: #ffffff;
      border-color: #4d8462;
    }
    .raw-note {
      color: #557493;
      font-size: 12px;
      margin-left: auto;
    }
    .raw-editor {
      min-height: 62vh;
      font-family: var(--mono);
      font-size: 13px;
      line-height: 1.5;
      background: #ffffff;
    }
    .raw-editor[readonly] {
      color: #43586d;
    }
    .rich-value-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(96px, 1fr));
      gap: 6px;
      background: #e5e5df;
      padding: 6px;
      border-radius: 10px;
    }
    .rich-value-grid input {
      background: #ffffff;
    }
    .navigator-wrap {
      padding: 6px;
      display: grid;
      gap: 6px;
      background: #edf3f8;
      min-height: 64vh;
    }
    .navigator-panel {
      display: grid;
      gap: 0;
      background: #e5edf6;
      border-radius: 12px;
      padding: 0 10px;
    }
    .navigator-bar {
      display: grid;
      gap: 0;
      align-items: stretch;
    }
    .navigator-inputs {
      display: grid;
      gap: 0;
    }
    .navigator-field {
      display: grid;
      gap: 0;
    }
    .navigator-label {
      flex: 0 0 68px;
      font-size: 10px;
      font-weight: 700;
      letter-spacing: 0.08em;
      color: #35506b;
      text-transform: uppercase;
    }
    .navigator-row {
      display: flex;
      gap: 6px;
      align-items: center;
      margin: 0;
      min-height: 0;
    }
    .navigator-actions {
      display: flex;
      gap: 8px;
      align-items: center;
      flex-wrap: nowrap;
    }
    .navigator-bar input {
      width: 100%;
      background: #ffffff;
      border-radius: 8px;
    }
    .navigator-row input {
      order: 1;
      flex: 1 1 auto;
      min-width: 0;
    }
    .navigator-row .navigator-label {
      order: 0;
    }
    .navigator-row .navigator-actions {
      order: 2;
    }
    .navigator-actions .quick-button { white-space: nowrap; }
    .navigator-list {
      display: grid;
      align-content: start;
      gap: 6px;
      background: #e5edf6;
      border-radius: 12px;
      padding: 6px;
      min-height: 360px;
      max-height: 360px;
      overflow: auto;
    }
    .navigator-entry {
      display: flex;
      justify-content: space-between;
      gap: 10px;
      align-items: flex-start;
      padding: 4px 10px;
      border-radius: 10px;
      background: #ffffff;
      cursor: pointer;
    }
    .navigator-entry:hover {
      background: #f7fbff;
    }
    .navigator-entry .path {
      color: #557493;
    }
    .navigator-entry.dir .name {
      color: #2f628f;
      font-weight: 700;
    }
    .navigator-entry.file .name {
      color: #5f6670;
    }
    .sub-row td {
      background: #f7fbff;
    }
    .sub-row-label {
      color: var(--muted);
      font-size: 12px;
      font-family: var(--mono);
    }
    .mini-button {
      border: 1px solid #c5d8eb;
      background: #d8e6f4;
      color: #203448;
      border-radius: 8px;
      padding: 4px 8px;
      cursor: pointer;
      font: inherit;
      font-size: 12px;
      font-weight: 600;
    }
    .value-cell-tools {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .empty {
      padding: 28px 14px;
      text-align: center;
      color: var(--muted);
      font-size: 15px;
    }
    .footer {
      padding: 0 4px;
      color: var(--muted);
      font-size: 12px;
    }
    @media (max-width: 960px) {
      .app {
        padding: 6px;
      }
    }
  </style>
</head>
<body>
  <div class="app">
    <section class="panel">
      <div class="workspace">
        <div class="workspace-sticky">
          <div class="menu-bar">
            <div class="menu-head">
              <div class="menu-switches">
                <button class="menu-switch-button active" data-menu-toggle="file">FILE</button>
                <button class="menu-switch-button active" data-menu-toggle="view">VIEW</button>
                <button class="menu-switch-button active" data-menu-toggle="parameter">PARAMETER</button>
              </div>
              <span class="brand">LILAK Parameter Table</span>
            </div>

            <div class="menu-panels">
              <div class="toolbar-group" data-menu-panel="file">
                <span class="menu-label">FILE</span>
                <button class="toolbar-button" data-action="file-new"><span class="icon">+</span>new</button>
                <button class="toolbar-button" data-action="file-open"><span class="icon">&#8599;</span>open</button>
                <button class="toolbar-button" data-action="file-clone"><span class="icon">&#10697;</span>clone</button>
                <button class="toolbar-button" data-action="file-save"><span class="icon">&#10003;</span>save</button>
                <button class="toolbar-button" data-action="file-save-as"><span class="icon">&#8680;</span>save as</button>
                <button class="toolbar-button" data-action="file-close"><span class="icon">x</span>close</button>
                <button class="toolbar-button" data-action="file-restore"><span class="icon">&#8634;</span>restore</button>
              </div>

              <div class="toolbar-group" data-menu-panel="view">
                <span class="menu-label">VIEW</span>
                <button class="toolbar-button active" data-view-mode="default"><span class="icon">&#9635;</span>default</button>
                <button class="toolbar-button" data-view-mode="group"><span class="icon">&#8801;</span>group</button>
                <button class="toolbar-button" data-view-mode="rich"><span class="icon">&#8942;</span>rich</button>
                <button class="toolbar-button" data-view-mode="raw"><span class="icon">&#123;</span>raw</button>
              </div>

              <div class="toolbar-group" data-menu-panel="parameter">
                <span class="menu-label">PARAMETER</span>
                <button class="toolbar-button" data-action="file-find"><span class="icon">?</span>find</button>
                <button class="toolbar-button" data-action="row-add-parameter"><span class="icon">+</span>+par</button>
                <button class="toolbar-button" data-action="row-add-comment"><span class="icon">#</span>+comment</button>
                <button class="toolbar-button" data-action="row-remove"><span class="icon">-</span>remove</button>
                <button class="toolbar-button" data-action="row-up"><span class="icon">&#8593;</span>up</button>
                <button class="toolbar-button" data-action="row-down"><span class="icon">&#8595;</span>down</button>
                <button class="toolbar-button" data-action="row-top"><span class="icon">&#8679;</span>top</button>
                <button class="toolbar-button" data-action="row-end"><span class="icon">&#8681;</span>end</button>
                <button class="toolbar-button" data-action="row-copy"><span class="icon">&#10697;</span>copy</button>
                <button class="toolbar-button" data-action="row-paste"><span class="icon">&#10753;</span>paste</button>
              </div>
            </div>

            <div class="status-bar">
              <span class="menu-label">STATUS</span>
              <span class="status-text" id="status">Ready</span>
            </div>
          </div>

          <div class="tab-bar" id="tabBar"></div>
        </div>

        <div class="table-wrap" id="tableWrap"></div>
      </div>
    </section>

    <div class="footer">Columns: #, toggle, group, name, value, unit, comment</div>
  </div>

  <script>
    const state = {
      tabs: [],
      activeTabId: null,
      selectedRowId: null,
      selectedRowIds: [],
      clipboardRows: [],
      lastAction: "",
      viewMode: "default",
      activeMenu: "file",
      rawMode: "original",
      rawParseTimer: null,
      untitledIndex: 1
    };

    const statusEl = document.getElementById("status");
    const tableWrapEl = document.getElementById("tableWrap");
    const tabBarEl = document.getElementById("tabBar");
    const menuBarEl = document.querySelector(".menu-bar");

    function setStatus(message, tone = "") {
      statusEl.textContent = message;
      statusEl.className = "status-text" + (tone ? " " + tone : "");
    }

    function renderMenuPanels() {
      for (const button of document.querySelectorAll("[data-menu-toggle]")) {
        button.classList.toggle("active", button.dataset.menuToggle === state.activeMenu);
      }
      for (const panel of document.querySelectorAll("[data-menu-panel]")) {
        panel.style.display = panel.dataset.menuPanel === state.activeMenu ? "flex" : "none";
      }
    }

    function statusLabelForTab(tab) {
      if (!tab) {
        return "No file open";
      }
      if (tab.type === "navigator") {
        return tab.currentDir || tab.browserPath || "Navigator";
      }
      return tab.path || `${tab.title} (unsaved)`;
    }

    function nextRowId() {
      return "row_" + Math.random().toString(36).slice(2, 10);
    }

    function markRecentAction(action) {
      state.lastAction = action || "";
      for (const node of document.querySelectorAll("[data-action]")) {
        node.classList.toggle("recent", Boolean(action) && node.dataset.action === action);
      }
    }

    function cloneRow(row) {
      return JSON.parse(JSON.stringify({ ...row, id: nextRowId() }));
    }

    function normalizeRow(row) {
      return {
        id: row.id || nextRowId(),
        kind: row.kind || "parameter",
        enabled: row.enabled === true,
        group: row.group || "",
        name: row.name || "",
        value: row.value || "",
        unit: row.unit || "",
        comment: row.comment || ""
      };
    }

    function activeTab() {
      return state.tabs.find((tab) => tab.id === state.activeTabId) || null;
    }

    function selectedRowIdsForTab(tab) {
      if (!tab || tab.type !== "editor") {
        return [];
      }
      const valid = new Set(tab.rows.map((row) => row.id));
      return state.selectedRowIds.filter((rowId) => valid.has(rowId));
    }

    function setSelectedRows(rowIds, anchorId = null) {
      const unique = Array.from(new Set((rowIds || []).filter(Boolean)));
      state.selectedRowIds = unique;
      state.selectedRowId = anchorId ?? unique[unique.length - 1] ?? null;
    }

    function applyRangeSelection(tab, rowId, checked) {
      if (!tab) return false;
      const rowIds = tab.rows.map((row) => row.id);
      const anchorId = state.selectedRowId;
      if (!anchorId || !rowIds.includes(anchorId) || !rowIds.includes(rowId)) {
        return false;
      }
      const start = rowIds.indexOf(anchorId);
      const end = rowIds.indexOf(rowId);
      const [from, to] = start <= end ? [start, end] : [end, start];
      const rangeIds = rowIds.slice(from, to + 1);
      const next = new Set(state.selectedRowIds);
      for (const id of rangeIds) {
        if (checked) next.add(id);
        else next.delete(id);
      }
      setSelectedRows(Array.from(next), rowId);
      return true;
    }

    function getEditorTab(tabId) {
      return state.tabs.find((tab) => tab.id === tabId && tab.type === "editor") || null;
    }

    function currentEditorTab() {
      return getEditorTab(activeTab()?.id) || getEditorTab(activeTab()?.targetEditorId);
    }

    function actionNavigatorTab(action) {
      return state.tabs.find((tab) => tab.type === "navigator" && tab.action === action) || null;
    }

    function parameterGroupKey(row) {
      const group = (row.group || "").trim();
      const unit = (row.unit || "").trim();
      if (!group) {
        return unit || "(root)";
      }
      return `${unit}${group}`;
    }

    function serializeRowsForRaw(rows) {
      const lines = [];
      let previousParameterGroup = null;
      let previousKind = null;
      for (const row of rows) {
        if (row.kind === "comment") {
          lines.push((row.enabled ? "## " : "# ") + (row.comment || ""));
          previousKind = "comment";
          continue;
        }
        const group = (row.group || "").trim().replace(/^\\/|\\/$/g, "");
        const name = (row.name || "").trim();
        if (!name) {
          continue;
        }
        const fullName = group ? `${group}/${name}` : name;
        const groupKey = parameterGroupKey(row);
        if (lines.length && previousKind === "parameter" && previousParameterGroup !== groupKey && lines[lines.length - 1] !== "") {
          lines.push("");
        }
        let line = (row.enabled === false ? "#" : "") + (row.unit || "").trim() + fullName;
        if ((row.value || "").trim()) {
          line += "  " + row.value.trim();
        }
        if ((row.comment || "").trim()) {
          line += "  # " + row.comment.trim();
        }
        lines.push(line);
        previousParameterGroup = groupKey;
        previousKind = "parameter";
      }
      return lines.join("\\n");
    }

    function computeContent(tab) {
      return serializeRowsForRaw(tab.rows);
    }

    async function parseContentToRows(content) {
      const response = await fetch("/api/parse", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content })
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Failed to parse raw text");
      }
      return (payload.rows || []).map((row) => normalizeRow(row));
    }

    async function normalizeRawContent(content) {
      const response = await fetch("/api/normalize", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content })
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Failed to normalize raw text");
      }
      return payload.content || "";
    }

    async function syncRawDraft(tab, quiet = false) {
      if (!tab) return;
      const rows = await parseContentToRows(tab.rawDraft ?? tab.content ?? "");
      tab.rows = rows;
      tab.content = tab.rawDraft ?? tab.content ?? "";
      tab.normalizedSource = "";
      if (!quiet) {
        setStatus("Raw text parsed", "ok");
      }
    }

    function updateDirty(tab) {
      tab.content = computeContent(tab);
      tab.normalizedSource = "";
      tab.dirty = tab.content !== tab.originalContent;
    }

    function tabLabel(tab) {
      const name = tab.type === "navigator"
        ? `[${tab.title}]`
        : (tab.path ? tab.path.split("/").pop() : tab.title);
      return tab.dirty ? `${name}*` : name;
    }

    function setActiveTab(tabId) {
      state.activeTabId = tabId;
      const tab = activeTab();
      if (tab && tab.type === "editor") {
        const selectedIds = selectedRowIdsForTab(tab);
        setSelectedRows(selectedIds, selectedIds.includes(state.selectedRowId) ? state.selectedRowId : null);
      }
      render();
      setStatus(statusLabelForTab(tab));
    }

    function createTabFromPayload(payload) {
      const rows = (payload.rows || []).map((row) => normalizeRow(row));
      const content = rows.map((row) => {
        if (row.kind === "comment") return (row.enabled ? "## " : "# ") + (row.comment || "");
        const group = row.group ? `${row.group}/` : "";
        const base = `${group}${row.name}`;
        const value = row.value ? `  ${row.value}` : "";
        const comment = row.comment ? `  # ${row.comment}` : "";
        return (row.enabled === false ? "#" : "") + base + value + comment;
      }).join("\\n");

      const tab = {
        id: "tab_" + Math.random().toString(36).slice(2, 10),
        type: "editor",
        title: payload.name || `untitled_${state.untitledIndex}.par`,
        path: payload.path || "",
        rows,
        collapsedGroups: {},
        originalRows: JSON.parse(JSON.stringify(rows)),
        originalContent: payload.content || content,
        content: payload.content || content,
        rawDraft: payload.content || content,
        normalizedDraft: "",
        normalizedSource: "",
        dirty: false
      };
      state.tabs.push(tab);
      state.untitledIndex += 1;
      setActiveTab(tab.id);
      setStatus(`Opened ${tab.title}`, "ok");
    }

    async function createNavigatorTab(action, targetEditorId = null) {
      if (action === "open" || action === "saveas") {
        const existing = actionNavigatorTab(action);
        if (existing) {
          existing.targetEditorId = targetEditorId || existing.targetEditorId;
          const sourceEditor = getEditorTab(targetEditorId) || getEditorTab(activeTab()?.id);
          existing.browserPath = sourceEditor?.path || existing.browserPath || "";
          state.activeTabId = existing.id;
          await refreshNavigatorTab(existing, existing.browserPath || "");
          render();
          setStatus(`${action} navigator reused for ${sourceEditor?.path || sourceEditor?.title || "editor tab"}`, "ok");
          return;
        }
      }
      const sourceEditor = getEditorTab(targetEditorId) || getEditorTab(activeTab()?.id);
      const tab = {
        id: "tab_" + Math.random().toString(36).slice(2, 10),
        type: "navigator",
        title: action.toUpperCase(),
        action,
        targetEditorId: sourceEditor?.id || null,
        browserPath: sourceEditor?.path || "",
        fileName: sourceEditor?.path ? sourceEditor.path.split("/").pop() : sourceEditor?.title || "",
        currentDir: "",
        entries: []
      };
      state.tabs.push(tab);
      state.activeTabId = tab.id;
      await refreshNavigatorTab(tab, tab.browserPath || "");
      render();
      setStatus(`${action} navigator opened`, "ok");
    }

    async function refreshNavigatorTab(tab, pathValue) {
      const response = await fetch("/api/listdir", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path: pathValue || "" })
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Directory load failed");
      }
      tab.currentDir = payload.current_dir;
      tab.browserPath = payload.current_dir;
      tab.entries = payload.entries || [];
    }

    function renderTabs() {
      if (state.tabs.length === 0) {
        tabBarEl.innerHTML = '<div class="empty">No open tabs</div>';
        return;
      }

      tabBarEl.innerHTML = state.tabs.map((tab) => `
        <div class="tab ${tab.id === state.activeTabId ? "active" : ""} ${tab.type === "editor" && state.tabs.some((item) => item.type === "navigator" && item.action === "saveas" && item.targetEditorId === tab.id) ? "targeted" : ""}" data-tab-id="${tab.id}">
          <span>${tabLabel(tab)}</span>
          <button class="tab-close" data-close-tab="${tab.id}">x</button>
        </div>
      `).join("");

      for (const node of tabBarEl.querySelectorAll("[data-tab-id]")) {
        node.addEventListener("click", (event) => {
          if (event.target.dataset.closeTab) {
            closeTab(event.target.dataset.closeTab);
            event.stopPropagation();
            return;
          }
          setActiveTab(node.dataset.tabId);
        });
      }
    }

    function groupedRows(rows) {
      const order = [];
      const groups = new Map();
      for (const row of rows) {
        const key = row.kind === "comment" ? "__comment__" : parameterGroupKey(row);
        if (!groups.has(key)) {
          groups.set(key, []);
          order.push(key);
        }
        groups.get(key).push(row);
      }
      return { order, groups };
    }

    function groupEnabledState(rows) {
      const enabledCount = rows.filter((row) => row.enabled !== false).length;
      return {
        everyOn: rows.length > 0 && enabledCount === rows.length,
        someOn: enabledCount > 0 && enabledCount < rows.length
      };
    }

    function rawSourceContent(tab) {
      return tab.rawDraft ?? tab.content ?? "";
    }

    async function refreshNormalizedRaw(tab, force = false) {
      const source = rawSourceContent(tab);
      if (!force && tab.normalizedSource === source && typeof tab.normalizedDraft === "string") {
        return tab.normalizedDraft;
      }
      tab.normalizedDraft = await normalizeRawContent(source);
      tab.normalizedSource = source;
      return tab.normalizedDraft;
    }

    function attachRawModeButtons(tab) {
      for (const button of tableWrapEl.querySelectorAll("[data-raw-mode]")) {
        button.classList.toggle("active", button.dataset.rawMode === state.rawMode);
        button.addEventListener("click", async () => {
          if (button.dataset.rawMode === state.rawMode) {
            return;
          }
          state.rawMode = button.dataset.rawMode;
          renderTable();
          if (state.rawMode === "normalized") {
            try {
              await refreshNormalizedRaw(tab, true);
              const normalizedEditor = document.getElementById("rawEditor");
              if (normalizedEditor) {
                normalizedEditor.value = tab.normalizedDraft || "";
              }
              setStatus("Compile preview updated", "ok");
            } catch (error) {
              state.rawMode = "original";
              renderTable();
              setStatus(error.message, "warn");
            }
          } else {
            setStatus("Raw original view");
          }
        });
      }
    }

    function renderTable() {
      const tab = activeTab();
      tableWrapEl.className = "table-wrap";
      if (!tab) {
        tableWrapEl.innerHTML = '<div class="empty">Open a parameter file to start editing.</div>';
        return;
      }

      if (tab.type === "navigator") {
        renderNavigator(tab);
        return;
      }

      if (state.viewMode === "raw") {
        const isNormalized = state.rawMode === "normalized";
        const rawValue = isNormalized
          ? (tab.normalizedSource === rawSourceContent(tab) ? (tab.normalizedDraft || "") : "")
          : rawSourceContent(tab);
        tableWrapEl.innerHTML = `
          <div class="raw-wrap">
            <div class="raw-toolbar">
              <button class="toolbar-button ${!isNormalized ? "active" : ""}" data-raw-mode="original">original</button>
              <button class="toolbar-button ${isNormalized ? "active" : ""}" data-raw-mode="normalized">compile</button>
              <span class="raw-note">${isNormalized ? "LKParameterContainer compile preview" : "editable source text"}</span>
            </div>
            <textarea class="raw-editor" id="rawEditor" ${isNormalized ? "readonly" : ""}>${escapeHtml(rawValue)}</textarea>
          </div>
        `;
        attachRawModeButtons(tab);
        const rawEditor = document.getElementById("rawEditor");
        if (isNormalized) {
          if (rawValue) {
            setStatus("Compile preview", "ok");
          } else {
            refreshNormalizedRaw(tab, true)
              .then((normalized) => {
                const editor = document.getElementById("rawEditor");
                if (editor) {
                  editor.value = normalized;
                }
                setStatus("Compile preview updated", "ok");
              })
              .catch((error) => {
                state.rawMode = "original";
                renderTable();
                setStatus(error.message, "warn");
              });
          }
          return;
        }
        rawEditor.addEventListener("input", () => {
          tab.rawDraft = rawEditor.value;
          tab.content = rawEditor.value;
          tab.normalizedSource = "";
          tab.dirty = tab.content !== tab.originalContent;
          renderTabs();
          setStatus("Unsaved raw text changes");
          window.clearTimeout(state.rawParseTimer);
          state.rawParseTimer = window.setTimeout(() => {
            syncRawDraft(tab, true).catch((error) => setStatus(error.message, "warn"));
          }, 400);
        });
        return;
      }

      const rowsHtml = [];
      let groupViewHeaderHtml = "";
      if (state.viewMode === "group") {
        const grouped = groupedRows(tab.rows);
        const groupKeys = grouped.order.map((key) => key === "__comment__" ? "comments" : key);
        const allClosed = groupKeys.length > 0 && groupKeys.every((groupKey) => Boolean(tab.collapsedGroups?.[groupKey]));
        const selectedIds = selectedRowIdsForTab(tab);
        const everySelected = tab.rows.length > 0 && selectedIds.length === tab.rows.length;
        groupViewHeaderHtml = `
          <tr class="group-view-header">
            <th colspan="7">
              <div class="group-header-bar">
                <button class="group-toggle" data-action="groups-toggle-all">${allClosed ? "+" : "-"}</button>
                <input class="group-enabled-toggle" type="checkbox" id="toggleAllGroups" ${everySelected ? "checked" : ""}>
                <span class="group-title">All groups</span>
              </div>
            </th>
          </tr>
        `;
        let visualIndex = 1;
        for (const key of grouped.order) {
          const groupKey = key === "__comment__" ? "comments" : key;
          const collapsed = Boolean(tab.collapsedGroups?.[groupKey]);
          const groupRows = grouped.groups.get(key);
          const selectedIds = new Set(selectedRowIdsForTab(tab));
          const selectedCount = groupRows.filter((row) => selectedIds.has(row.id)).length;
          const allSelected = groupRows.length > 0 && selectedCount === groupRows.length;
          const someSelected = selectedCount > 0 && selectedCount < groupRows.length;
          rowsHtml.push(`
            <tr class="group-header">
              <td colspan="7">
                <div class="group-header-bar">
                  <button class="group-toggle" data-group-key="${escapeHtml(groupKey)}">${collapsed ? "+" : "-"}</button>
                  <input class="group-enabled-toggle" type="checkbox" data-group-select="${escapeHtml(groupKey)}" ${allSelected ? "checked" : ""}>
                  <span class="group-title">${escapeHtml(groupKey)}</span>
                </div>
              </td>
            </tr>
          `);
          if (!collapsed) {
            for (const row of groupRows) {
              rowsHtml.push(renderRow(row, visualIndex));
              visualIndex += 1;
            }
          }
        }
      } else {
        tab.rows.forEach((row, index) => rowsHtml.push(renderRow(row, index + 1)));
      }

      tableWrapEl.innerHTML = `
        <table class="${state.viewMode === "group" ? "group-view-table" : ""}">
          <thead>
            <tr>
              <th>#</th>
              <th><input type="checkbox" id="toggleAllRows"></th>
              <th>group</th>
              <th>name</th>
              <th>value</th>
              <th>unit</th>
              <th>comment</th>
            </tr>
            ${groupViewHeaderHtml}
          </thead>
          <tbody>
            ${rowsHtml.join("") || '<tr><td colspan="7" class="empty">No rows</td></tr>'}
          </tbody>
        </table>
      `;

      attachRowHandlers();
      const toggleAll = document.getElementById("toggleAllRows");
      const selectedIds = selectedRowIdsForTab(tab);
      const everySelected = tab.rows.length > 0 && selectedIds.length === tab.rows.length;
      const someSelected = selectedIds.length > 0 && selectedIds.length < tab.rows.length;
      toggleAll.checked = everySelected;
      toggleAll.indeterminate = someSelected;
      toggleAll.addEventListener("change", () => {
        setSelectedRows(toggleAll.checked ? tab.rows.map((row) => row.id) : []);
        renderTable();
        setStatus(toggleAll.checked ? `${tab.rows.length} row${tab.rows.length !== 1 ? "s" : ""} selected` : "Selection cleared");
      });
      for (const button of tableWrapEl.querySelectorAll(".group-toggle")) {
        button.addEventListener("click", (event) => {
          event.stopPropagation();
          tab.collapsedGroups = tab.collapsedGroups || {};
          tab.collapsedGroups[button.dataset.groupKey] = !tab.collapsedGroups[button.dataset.groupKey];
          renderTable();
          setStatus("Group view updated");
        });
      }
      const groupToggleAll = document.querySelector("[data-action='groups-toggle-all']");
      if (groupToggleAll) {
        groupToggleAll.addEventListener("click", () => {
          const grouped = groupedRows(tab.rows);
          const allClosed = grouped.order.length > 0 && grouped.order.every((key) => {
            const groupKey = key === "__comment__" ? "comments" : key;
            return Boolean(tab.collapsedGroups?.[groupKey]);
          });
          tab.collapsedGroups = tab.collapsedGroups || {};
          for (const key of grouped.order) {
            const groupKey = key === "__comment__" ? "comments" : key;
            tab.collapsedGroups[groupKey] = !allClosed;
          }
          renderTable();
          setStatus(allClosed ? "All groups opened" : "All groups closed");
        });
      }
      const toggleAllGroups = document.getElementById("toggleAllGroups");
      if (toggleAllGroups) {
        const someSelectedGroups = selectedIds.length > 0 && selectedIds.length < tab.rows.length;
        toggleAllGroups.indeterminate = someSelectedGroups;
        toggleAllGroups.addEventListener("change", () => {
          setSelectedRows(toggleAllGroups.checked ? tab.rows.map((row) => row.id) : []);
          renderTable();
          setStatus(toggleAllGroups.checked ? `${tab.rows.length} row${tab.rows.length !== 1 ? "s" : ""} selected` : "Selection cleared");
        });
      }
      for (const toggle of tableWrapEl.querySelectorAll("[data-group-select]")) {
        const groupKey = toggle.dataset.groupSelect;
        const grouped = groupedRows(tab.rows);
        const sourceRows = grouped.groups.get(groupKey === "comments" ? "__comment__" : groupKey) || [];
        const selectedSet = new Set(selectedRowIdsForTab(tab));
        const selectedCount = sourceRows.filter((row) => selectedSet.has(row.id)).length;
        toggle.indeterminate = selectedCount > 0 && selectedCount < sourceRows.length;
        toggle.addEventListener("click", (event) => event.stopPropagation());
        toggle.addEventListener("change", () => {
          const current = new Set(selectedRowIdsForTab(tab));
          for (const row of sourceRows) {
            if (toggle.checked) current.add(row.id);
            else current.delete(row.id);
          }
          setSelectedRows(Array.from(current));
          renderTable();
          setStatus(toggle.checked ? `${sourceRows.length} row${sourceRows.length !== 1 ? "s" : ""} selected in ${groupKey}` : `${groupKey} selection cleared`);
        });
      }
    }

    function renderNavigator(tab) {
      const confirmLabel = tab.action === "open" ? "Open"
        : tab.action === "new" ? "Create"
        : tab.action === "save" ? "Save"
        : "Save As";
      tableWrapEl.innerHTML = `
        <div class="navigator-wrap">
          <div class="navigator-panel">
            <div class="navigator-bar">
              <div class="navigator-inputs">
                <div class="navigator-field">
                  <div class="navigator-row">
                    <span class="navigator-label">Path</span>
                    <div class="navigator-actions">
                      <button class="quick-button" id="navUpBtn"><span class="icon">&#8593;</span>up</button>
                    </div>
                    <input type="text" id="navPathInput" value="${escapeHtml(tab.currentDir || tab.browserPath || "")}">
                    <div class="navigator-actions">
                      <button class="quick-button" id="navRefreshBtn"><span class="icon">&#8635;</span>navigate</button>
                    </div>
                  </div>
                </div>
                <div class="navigator-field">
                  <div class="navigator-row">
                    <span class="navigator-label">File Name</span>
                    <input type="text" id="navFileInput" value="${escapeHtml(tab.fileName || "")}" placeholder="file name">
                    <div class="navigator-actions">
                      <button class="quick-button" id="navConfirmBtn">${confirmLabel}</button>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
          <div class="navigator-list">
            ${(tab.entries || []).map((entry) => `
              <div class="navigator-entry ${entry.is_dir ? "dir" : "file"}" data-entry-path="${entry.path}" data-entry-dir="${entry.is_dir ? "1" : "0"}">
                <span class="name">${entry.is_dir ? "▸" : "•"} ${escapeHtml(entry.name)}</span>
                <span class="path">${escapeHtml(entry.path)}</span>
              </div>
            `).join("") || '<div class="empty">No entries</div>'}
          </div>
        </div>
      `;

      const pathInput = document.getElementById("navPathInput");
      const fileInput = document.getElementById("navFileInput");
      document.getElementById("navUpBtn").addEventListener("click", async () => {
        await refreshNavigatorTab(tab, `${tab.currentDir}/..`);
        tab.fileName = "";
        renderTable();
      });
      document.getElementById("navRefreshBtn").addEventListener("click", async () => {
        await refreshNavigatorTab(tab, pathInput.value || tab.currentDir);
        renderTable();
      });
      pathInput.addEventListener("keydown", async (event) => {
        if (event.key !== "Enter") {
          return;
        }
        event.preventDefault();
        await refreshNavigatorTab(tab, pathInput.value || tab.currentDir);
        renderTable();
      });
      fileInput.addEventListener("input", () => {
        tab.fileName = fileInput.value;
      });
      document.getElementById("navConfirmBtn").addEventListener("click", async () => {
        try {
          await confirmNavigator(tab, pathInput.value, fileInput.value);
        } catch (error) {
          setStatus(error.message, "warn");
        }
      });

      for (const entryEl of document.querySelectorAll(".navigator-entry")) {
        entryEl.addEventListener("click", async () => {
          const entryPath = entryEl.dataset.entryPath;
          if (entryEl.dataset.entryDir === "1") {
            await refreshNavigatorTab(tab, entryPath);
            tab.fileName = "";
            renderTable();
            return;
          }
          fileInput.value = entryEl.querySelector(".name").textContent.replace(/^[▸•]\\s*/, "");
          tab.fileName = fileInput.value;
        });
        entryEl.addEventListener("dblclick", async () => {
          try {
            await confirmNavigator(tab, entryEl.dataset.entryPath);
          } catch (error) {
            setStatus(error.message, "warn");
          }
        });
      }
    }

    async function confirmNavigator(tab, pathValue, fileNameValue = "") {
      const basePath = (pathValue || "").trim();
      const fileName = (fileNameValue || "").trim();
      const path = fileName ? `${basePath.replace(/\\/+$/, "")}/${fileName}` : basePath;
      if (!path) {
        throw new Error("Select path first");
      }
      if (tab.action === "open") {
        await openFileByPath(path);
        closeTab(tab.id);
        return;
      }
      if (tab.action === "new") {
        createTabFromPayload({
          name: path.split("/").pop() || `untitled_${state.untitledIndex}.par`,
          path,
          rows: [],
          content: ""
        });
        closeTab(tab.id);
        return;
      }
      const editorTab = getEditorTab(tab.targetEditorId);
      if (!editorTab) {
        throw new Error("No target editor tab");
      }
      editorTab.path = path;
      editorTab.title = path.split("/").pop() || editorTab.title;
      state.activeTabId = editorTab.id;
      await saveActiveTab();
      closeTab(tab.id);
      state.activeTabId = editorTab.id;
      render();
    }

    function renderRow(row, visualIndex) {
      const selected = state.selectedRowIds.includes(row.id) ? "selected" : "";
      const commentClass = row.kind === "comment" ? "comment-row" : "";
      const checked = state.selectedRowIds.includes(row.id) ? "checked" : "";
      if (row.kind === "comment") {
        return `
          <tr class="${selected} ${commentClass}" data-row-id="${row.id}">
            <td class="row-id">${visualIndex}</td>
            <td><input type="checkbox" data-field="selected" ${checked}></td>
            <td colspan="5" class="comment-cell">
              <input class="comment-editor" type="text" data-field="comment" value="${escapeHtml(row.comment || "")}">
            </td>
          </tr>
        `;
      }

      if (state.viewMode === "rich" && (row.value || "").includes(",")) {
        const parts = (row.value || "").split(",").map((item) => item.trim()).filter(Boolean);
        const parentRow = `
          <tr class="${selected} ${commentClass}" data-row-id="${row.id}">
            <td class="row-id">${visualIndex}</td>
            <td><input type="checkbox" data-field="selected" ${checked}></td>
            <td><input type="text" data-field="group" value="${escapeHtml(row.group || "")}"></td>
            <td><input type="text" data-field="name" value="${escapeHtml(row.name || "")}"></td>
            <td><div class="value-cell-tools"><span class="sub-row-label">${parts.length} values</span><button class="mini-button" data-action="add-part">+ value</button></div></td>
            <td><input type="text" data-field="unit" value="${escapeHtml(row.unit || "")}"></td>
            <td>
              <input type="text" data-field="comment" value="${escapeHtml(row.comment || "")}">
            </td>
          </tr>
        `;
        const subRows = parts.map((part, index) => `
          <tr class="sub-row" data-row-id="${row.id}" data-sub-index="${index}">
            <td></td>
            <td><button class="mini-button" data-action="remove-part" data-part-index="${index}">-</button></td>
            <td colspan="2" class="sub-row-label">${escapeHtml(row.group ? `${row.group}/` : "")}${escapeHtml(row.name || "")}[${index}]</td>
            <td><input type="text" data-field="value-part" data-part-index="${index}" value="${escapeHtml(part)}"></td>
            <td></td>
            <td></td>
          </tr>
        `).join("");
        return parentRow + subRows;
      }
      return `
        <tr class="${selected} ${commentClass}" data-row-id="${row.id}">
          <td class="row-id">${visualIndex}</td>
          <td><input type="checkbox" data-field="selected" ${checked}></td>
          <td><input type="text" data-field="group" value="${escapeHtml(row.group || "")}"></td>
          <td><input type="text" data-field="name" value="${escapeHtml(row.name || "")}"></td>
          <td><input type="text" data-field="value" value="${escapeHtml(row.value || "")}"></td>
          <td><input type="text" data-field="unit" value="${escapeHtml(row.unit || "")}"></td>
          <td><input type="text" data-field="comment" value="${escapeHtml(row.comment || "")}"></td>
        </tr>
      `;
    }

    function escapeHtml(value) {
      return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;");
    }

    function attachRowHandlers() {
      const tab = activeTab();
      if (!tab) {
        return;
      }

      for (const rowEl of tableWrapEl.querySelectorAll("tbody tr[data-row-id]")) {
        for (const input of rowEl.querySelectorAll("input, textarea")) {
          input.addEventListener("click", (event) => {
            event.stopPropagation();
            if (input.dataset.field === "selected") {
              const row = tab.rows.find((entry) => entry.id === rowEl.dataset.rowId);
              if (!row) return;
              const didApplyRange = event.shiftKey && applyRangeSelection(tab, rowEl.dataset.rowId, input.checked);
              if (!didApplyRange) {
                const next = input.checked
                  ? [...state.selectedRowIds, rowEl.dataset.rowId]
                  : state.selectedRowIds.filter((id) => id !== rowEl.dataset.rowId);
                setSelectedRows(next, rowEl.dataset.rowId);
              }
              renderTable();
              setStatus(`${selectedRowIdsForTab(tab).length} row${selectedRowIdsForTab(tab).length !== 1 ? "s" : ""} selected`);
              return;
            }
          });
          input.addEventListener("focus", () => {
            if (input.dataset.field !== "selected") return;
          });
          input.addEventListener("input", () => {
            const row = tab.rows.find((entry) => entry.id === rowEl.dataset.rowId);
            if (!row) return;
            if (input.dataset.field === "selected") {
              return;
            }
            if (input.dataset.field === "value-part") {
              const values = Array.from(tableWrapEl.querySelectorAll(`tr[data-row-id="${rowEl.dataset.rowId}"] [data-field="value-part"]`))
                .map((node) => node.value.trim())
                .filter(Boolean);
              row.value = values.join(", ");
            } else {
              row[input.dataset.field] = input.value;
            }
            updateDirty(tab);
            tab.rawDraft = tab.content;
            renderTabs();
            setStatus("Unsaved changes");
          });
          input.addEventListener("change", () => {
            const row = tab.rows.find((entry) => entry.id === rowEl.dataset.rowId);
            if (!row) return;
            if (input.dataset.field === "selected") {
              return;
            }
            updateDirty(tab);
            tab.rawDraft = tab.content;
            renderTabs();
            renderTable();
            setStatus("Unsaved changes");
          });
        }
        for (const button of rowEl.querySelectorAll("[data-action='add-part'], [data-action='remove-part']")) {
          button.addEventListener("click", (event) => {
            event.stopPropagation();
            const row = tab.rows.find((entry) => entry.id === rowEl.dataset.rowId);
            if (!row) return;
            const parts = (row.value || "").split(",").map((item) => item.trim()).filter(Boolean);
            if (button.dataset.action === "add-part") {
              parts.push(`value${parts.length}`);
            } else {
              parts.splice(Number(button.dataset.partIndex), 1);
            }
            row.value = parts.join(", ");
            updateDirty(tab);
            tab.rawDraft = tab.content;
            render();
            setStatus("Array values updated");
          });
        }
      }
    }

    function render() {
      renderTabs();
      renderTable();
    }

    function insertRow(kind) {
      const tab = currentEditorTab();
      if (!tab) return;
      const newRow = normalizeRow(
        kind === "comment"
          ? { kind: "comment", enabled: false, comment: "new comment" }
          : { kind: "parameter", enabled: true, group: "", name: "new/name", value: "", unit: "", comment: "" }
      );
      const index = tab.rows.findIndex((row) => row.id === state.selectedRowId);
      if (index >= 0) {
        tab.rows.splice(index + 1, 0, newRow);
      } else {
        tab.rows.unshift(newRow);
      }
      setSelectedRows([newRow.id], newRow.id);
      updateDirty(tab);
      render();
      setStatus(kind === "comment" ? "Comment row added" : "Parameter row added");
    }

    function removeSelectedRow() {
      const tab = currentEditorTab();
      const selectedIds = selectedRowIdsForTab(tab);
      if (!tab || selectedIds.length === 0) return;
      const firstIndex = tab.rows.findIndex((row) => row.id === selectedIds[0]);
      tab.rows = tab.rows.filter((row) => !selectedIds.includes(row.id));
      const nextId = tab.rows[firstIndex]?.id || tab.rows[firstIndex - 1]?.id || null;
      setSelectedRows(nextId ? [nextId] : [], nextId);
      updateDirty(tab);
      render();
      setStatus(`${selectedIds.length} row${selectedIds.length > 1 ? "s" : ""} removed`);
    }

    function moveSelected(delta) {
      const tab = currentEditorTab();
      const selectedIds = selectedRowIdsForTab(tab);
      if (!tab || selectedIds.length === 0) return;
      const selectedSet = new Set(selectedIds);
      if (delta < 0) {
        for (let index = 0; index < tab.rows.length; index += 1) {
          if (!selectedSet.has(tab.rows[index].id)) continue;
          if (index === 0 || selectedSet.has(tab.rows[index - 1].id)) continue;
          [tab.rows[index - 1], tab.rows[index]] = [tab.rows[index], tab.rows[index - 1]];
        }
      } else {
        for (let index = tab.rows.length - 1; index >= 0; index -= 1) {
          if (!selectedSet.has(tab.rows[index].id)) continue;
          if (index === tab.rows.length - 1 || selectedSet.has(tab.rows[index + 1].id)) continue;
          [tab.rows[index + 1], tab.rows[index]] = [tab.rows[index], tab.rows[index + 1]];
        }
      }
      updateDirty(tab);
      render();
      setStatus(`${selectedIds.length} row${selectedIds.length > 1 ? "s" : ""} moved ${delta < 0 ? "up" : "down"}`);
    }

    function moveSelectedToEdge(toTop) {
      const tab = currentEditorTab();
      const selectedIds = selectedRowIdsForTab(tab);
      if (!tab || selectedIds.length === 0) return;
      const selectedSet = new Set(selectedIds);
      const selectedRows = tab.rows.filter((row) => selectedSet.has(row.id));
      const otherRows = tab.rows.filter((row) => !selectedSet.has(row.id));
      tab.rows = toTop ? [...selectedRows, ...otherRows] : [...otherRows, ...selectedRows];
      updateDirty(tab);
      render();
      setStatus(`${selectedIds.length} row${selectedIds.length > 1 ? "s" : ""} moved to ${toTop ? "top" : "end"}`);
    }

    function copySelectedRow() {
      const tab = currentEditorTab();
      const selectedIds = selectedRowIdsForTab(tab);
      if (!tab || selectedIds.length === 0) return;
      state.clipboardRows = tab.rows
        .filter((entry) => selectedIds.includes(entry.id))
        .map((entry) => JSON.parse(JSON.stringify(entry)));
      setStatus(`${state.clipboardRows.length} parameter${state.clipboardRows.length > 1 ? "s" : ""} copied`, "ok");
    }

    function pasteRow() {
      const tab = currentEditorTab();
      if (!tab || !state.clipboardRows.length) return;
      const selectedIds = selectedRowIdsForTab(tab);
      const insertAfter = selectedIds.length
        ? Math.max(...selectedIds.map((id) => tab.rows.findIndex((entry) => entry.id === id)))
        : tab.rows.findIndex((entry) => entry.id === state.selectedRowId);
      const rows = state.clipboardRows.map((entry) => cloneRow(normalizeRow({ ...entry, id: "" })));
      if (insertAfter >= 0) tab.rows.splice(insertAfter + 1, 0, ...rows);
      else tab.rows.push(...rows);
      setSelectedRows(rows.map((row) => row.id), rows[rows.length - 1]?.id || null);
      updateDirty(tab);
      render();
      setStatus(`${rows.length} parameter${rows.length > 1 ? "s" : ""} pasted`, "ok");
    }

    async function openFileByPath(pathValue) {
      const path = pathValue.trim();
      if (!path) return;
      const response = await fetch("/api/read", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path })
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Failed to open file");
      }
      createTabFromPayload(payload);
    }

    async function saveActiveTab() {
      const tab = currentEditorTab();
      if (!tab) return;
      const path = (tab.path || "").trim();
      if (!path) {
        throw new Error("Use SaveAs first");
      }
      if (state.viewMode === "raw") {
        tab.content = tab.rawDraft ?? tab.content ?? "";
        tab.dirty = tab.content !== tab.originalContent;
      } else {
        updateDirty(tab);
      }
      const response = await fetch("/api/save", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(
          state.viewMode === "raw"
            ? { path, content: tab.rawDraft ?? tab.content ?? "" }
            : { path, rows: tab.rows }
        )
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Failed to save");
      }
      tab.path = payload.path;
      tab.title = payload.name;
      tab.rows = (payload.rows || tab.rows).map((row) => normalizeRow(row));
      tab.originalRows = JSON.parse(JSON.stringify(tab.rows));
      tab.originalContent = payload.content;
      tab.content = payload.content;
      tab.rawDraft = payload.content;
      tab.dirty = false;
      render();
      setStatus(`Saved at ${payload.saved_at}`, "ok");
    }

    async function restoreActiveTab() {
      const tab = currentEditorTab();
      if (!tab || !tab.path) {
        throw new Error("No file path to restore");
      }
      const response = await fetch("/api/read", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path: tab.path })
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Failed to restore");
      }
      tab.rows = payload.rows.map((row) => normalizeRow(row));
      tab.originalRows = JSON.parse(JSON.stringify(tab.rows));
      tab.originalContent = payload.content;
      tab.content = tab.originalContent;
      tab.rawDraft = payload.content;
      tab.dirty = false;
      setSelectedRows([]);
      render();
      setStatus("Restored from disk", "ok");
    }

    function closeTab(tabId) {
      const index = state.tabs.findIndex((tab) => tab.id === tabId);
      if (index < 0) return;
      state.tabs.splice(index, 1);
      if (state.activeTabId === tabId) {
        state.activeTabId = state.tabs[index]?.id || state.tabs[index - 1]?.id || null;
      }
      if (!activeTab()) {
        setSelectedRows([]);
      }
      render();
      setStatus("Tab closed");
    }

    function newTab() {
      createTabFromPayload({
        name: `untitled_${state.untitledIndex}.par`,
        path: "",
        rows: [],
        content: ""
      });
    }

    function escapeRegExp(value) {
      return Array.from(value || "").map((char) => "\\^$.*+?()[]{}|".includes(char) ? `\\${char}` : char).join("");
    }

    function nextCloneName(sourceName) {
      const fallback = sourceName || `untitled_${state.untitledIndex}.par`;
      const dotIndex = fallback.lastIndexOf(".");
      const hasExtension = dotIndex > 0;
      const stemWithClone = hasExtension ? fallback.slice(0, dotIndex) : fallback;
      const ext = hasExtension ? fallback.slice(dotIndex) : "";
      let stem = stemWithClone;
      const cloneMarkerIndex = stemWithClone.lastIndexOf("_clone_");
      if (cloneMarkerIndex >= 0) {
        const suffix = stemWithClone.slice(cloneMarkerIndex + 7);
        if (suffix && Array.from(suffix).every((char) => char >= "0" && char <= "9")) {
          stem = stemWithClone.slice(0, cloneMarkerIndex);
        }
      }
      let maxRevision = 0;
      for (const openTab of state.tabs) {
        if (openTab.type !== "editor") continue;
        const name = openTab.path ? openTab.path.split("/").pop() : openTab.title;
        const openName = name || "";
        if (!openName.endsWith(ext)) continue;
        const openStem = ext ? openName.slice(0, -ext.length) : openName;
        if (!openStem.startsWith(`${stem}_clone_`)) continue;
        const revisionText = openStem.slice(stem.length + 7);
        if (revisionText && Array.from(revisionText).every((char) => char >= "0" && char <= "9")) {
          maxRevision = Math.max(maxRevision, Number(revisionText) || 0);
        }
      }
      return `${stem}_clone_${maxRevision + 1}${ext}`;
    }

    function cloneActiveTab() {
      const tab = currentEditorTab();
      if (!tab) return;
      if (state.viewMode === "raw") {
        tab.content = tab.rawDraft ?? tab.content ?? "";
      } else {
        updateDirty(tab);
      }
      const sourceName = tab.path ? tab.path.split("/").pop() : tab.title;
      createTabFromPayload({
        name: nextCloneName(sourceName),
        path: "",
        rows: JSON.parse(JSON.stringify(tab.rows || [])),
        content: tab.rawDraft ?? tab.content ?? ""
      });
      const clonedTab = activeTab();
      if (clonedTab) {
        clonedTab.dirty = true;
        clonedTab.originalContent = "";
        clonedTab.originalRows = [];
      }
      render();
      setStatus(`Cloned ${sourceName || tab.title}`, "ok");
    }

    async function findFile() {
      const query = window.prompt("Find file");
      if (!query) return;
      const response = await fetch("/api/find", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ query })
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Find failed");
      }
      if (!payload.matches.length) {
        throw new Error("No matching files");
      }
      const listing = payload.matches.map((item, index) => `${index + 1}. ${item}`).join("\\n");
      const choice = window.prompt(`Select file number\\n${listing}`, "1");
      if (!choice) return;
      const selected = payload.matches[Number(choice) - 1];
      if (!selected) {
        throw new Error("Invalid selection");
      }
      await openFileByPath(selected);
    }

    function bindMenus() {
      for (const button of document.querySelectorAll("[data-menu-toggle]")) {
        button.addEventListener("click", () => {
          const menuName = button.dataset.menuToggle;
          if (!menuName) return;
          state.activeMenu = menuName;
          renderMenuPanels();
        });
      }

      for (const button of document.querySelectorAll("[data-view-mode]")) {
        button.addEventListener("click", async () => {
          const tab = activeTab();
          let rawParseWarning = "";
          if (state.viewMode === "raw" && button.dataset.viewMode !== "raw") {
            try {
              await syncRawDraft(tab, true);
            } catch (error) {
              rawParseWarning = error.message || "Failed to parse raw text";
            }
          }
          state.viewMode = button.dataset.viewMode;
          for (const node of document.querySelectorAll("[data-view-mode]")) {
            node.classList.toggle("active", node.dataset.viewMode === state.viewMode);
          }
          renderTable();
          if (rawParseWarning) {
            setStatus(`${rawParseWarning}; showing last parsed table`, "warn");
          } else {
            setStatus(`${state.viewMode} view`);
          }
        });
      }

      for (const item of document.querySelectorAll("[data-action]")) {
        item.addEventListener("click", async (event) => {
          event.stopPropagation();
          markRecentAction(item.dataset.action);
          try {
            switch (item.dataset.action) {
              case "file-new":
                newTab();
                break;
              case "file-clone":
                cloneActiveTab();
                break;
              case "file-open": {
                await createNavigatorTab("open");
                break;
              }
              case "file-save": {
                const tab = currentEditorTab();
                if (!tab) throw new Error("No editor tab selected");
                await saveActiveTab();
                break;
              }
              case "file-find":
                await findFile();
                break;
              case "file-save-as": {
                const tab = getEditorTab(activeTab()?.id) || getEditorTab(activeTab()?.targetEditorId);
                if (!tab) throw new Error("No editor tab selected");
                await createNavigatorTab("saveas", tab.id);
                break;
              }
              case "file-close":
                if (state.activeTabId) closeTab(state.activeTabId);
                break;
              case "file-restore":
                await restoreActiveTab();
                break;
              case "row-add-parameter":
                insertRow("parameter");
                break;
              case "row-add-comment":
                insertRow("comment");
                break;
              case "row-remove":
                removeSelectedRow();
                break;
              case "row-up":
                moveSelected(-1);
                break;
              case "row-down":
                moveSelected(1);
                break;
              case "row-top":
                moveSelectedToEdge(true);
                break;
              case "row-end":
                moveSelectedToEdge(false);
                break;
              case "row-copy":
                copySelectedRow();
                break;
              case "row-paste":
                pasteRow();
                break;
            }
          } catch (error) {
            setStatus(error.message, "warn");
          }
        });
      }
    }

    async function bootstrap() {
      bindMenus();
      renderMenuPanels();
      const response = await fetch("/api/bootstrap");
      const payload = await response.json();
      createTabFromPayload(payload);
      render();
    }

    window.addEventListener("keydown", (event) => {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "s") {
        event.preventDefault();
        saveActiveTab().catch((error) => setStatus(error.message, "warn"));
        return;
      }
      if ((event.metaKey || event.ctrlKey) && !event.shiftKey && event.key.toLowerCase() === "z") {
        event.preventDefault();
        restoreActiveTab().catch((error) => setStatus(error.message, "warn"));
      }
    });

    bootstrap().catch((error) => setStatus(error.message, "warn"));
  </script>
</body>
</html>
"""


class ParameterEditorServer(ThreadingHTTPServer):
    def __init__(self, server_address, initial_file: Path | None):
        super().__init__(server_address, RequestHandler)
        self.initial_file = initial_file


class RequestHandler(BaseHTTPRequestHandler):
    server: ParameterEditorServer

    def log_message(self, fmt, *args):
        return

    def _read_json(self):
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length)
        return json.loads(raw.decode("utf-8") if raw else "{}")

    def _send_json(self, payload, status=HTTPStatus.OK):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_html(self, payload):
        body = payload.encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/":
            self._send_html(HTML_PAGE)
            return
        if parsed.path == "/api/bootstrap":
            if self.server.initial_file is None:
                self._send_json(make_empty_payload())
            else:
                self._send_json(make_file_payload(self.server.initial_file))
            return
        self._send_json({"error": "Not found"}, status=HTTPStatus.NOT_FOUND)

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        payload = self._read_json()

        try:
            if parsed.path == "/api/read":
                path = resolve_input_path(payload.get("path", ""))
                self._send_json(make_file_payload(path))
                return

            if parsed.path == "/api/save":
                target = Path(os.path.expanduser(os.path.expandvars(payload.get("path", ""))))
                if not target.is_absolute():
                    target = (Path.cwd() / target).resolve()
                target.parent.mkdir(parents=True, exist_ok=True)
                if "content" in payload:
                    content = payload.get("content", "")
                else:
                    rows = payload.get("rows", [])
                    content = serialize_rows(rows)
                rows = parse_parameter_text(content)
                target.write_text(content, encoding="utf-8")
                self._send_json(
                    {
                        "name": target.name,
                        "path": str(target),
                        "content": content,
                        "rows": rows,
                        "saved_at": time.strftime("%H:%M:%S"),
                    }
                )
                return

            if parsed.path == "/api/parse":
                content = payload.get("content", "")
                self._send_json({"rows": parse_parameter_text(content)})
                return

            if parsed.path == "/api/normalize":
                content = payload.get("content", "")
                self._send_json({"content": normalize_with_lkparametercontainer(content)})
                return

            if parsed.path == "/api/find":
                matches = find_matching_files(payload.get("query", ""))
                self._send_json({"matches": matches})
                return

            if parsed.path == "/api/listdir":
                self._send_json(list_directory(payload.get("path", "")))
                return
        except FileNotFoundError:
            self._send_json({"error": "File not found"}, status=HTTPStatus.NOT_FOUND)
            return
        except OSError as error:
            self._send_json({"error": str(error)}, status=HTTPStatus.BAD_REQUEST)
            return
        except RuntimeError as error:
            self._send_json({"error": str(error)}, status=HTTPStatus.BAD_REQUEST)
            return

        self._send_json({"error": "Not found"}, status=HTTPStatus.NOT_FOUND)


def pick_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def main():
    parser = argparse.ArgumentParser(description="Open a LILAK parameter file in a local web editor.")
    parser.add_argument("file", nargs="?", help="Parameter file to edit")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()

    initial_file = None
    if args.file:
        try:
            initial_file = resolve_input_path(args.file)
        except FileNotFoundError:
            print(f"Cannot find parameter file: {args.file}", file=sys.stderr)
            return 1

    port = args.port or pick_port()
    server = ParameterEditorServer((args.host, port), initial_file)
    url = f"http://{args.host}:{port}/"

    if initial_file is None:
        print("Opening empty parameter editor")
    else:
        print(f"Opening {initial_file}")
    print(f"Parameter editor: {url}")
    print("Press Ctrl-C to stop the server.")

    if not args.no_browser:
        webbrowser.open(url)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\\nStopping parameter editor.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
