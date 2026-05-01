#!/usr/bin/env python3

import argparse
import json
import mimetypes
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
from pathlib import Path, PurePosixPath
from typing import Optional


FORMAT_SUFFIXES = [".mac", ".conf", ".par", ".txt", ".log"]
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


def make_root_parameter_payload(path: Path):
    template_path = lilak_root_path() / "meta" / "parameters" / "configure_LKRun.mac"
    if not template_path.is_file():
        raise FileNotFoundError(str(template_path))

    content = template_path.read_text(encoding="utf-8")
    rows = parse_parameter_text(content)
    updated = False
    for row in rows:
        if row.get("kind") != "parameter":
            continue
        if (row.get("group") or "").strip() != "LKRun":
            continue
        if (row.get("name") or "").strip() != "InputFile":
            continue
        row["value"] = str(path)
        row["unit"] = ""
        row["enabled"] = True
        updated = True
        break

    if not updated:
        rows.append(
            {
                "kind": "parameter",
                "group": "LKRun",
                "name": "InputFile",
                "value": str(path),
                "unit": "",
                "comment": "input file generated from lilak par [root file]",
                "enabled": True,
            }
        )

    generated_content = serialize_rows(rows)
    generated_path = path.with_name(f"run_{path.stem}.mac")
    generated_name = generated_path.name
    return {
        "name": generated_name,
        "path": str(generated_path),
        "content": generated_content,
        "rows": parse_parameter_text(generated_content),
        "saved_at": time.strftime("%H:%M:%S"),
        "source_path": str(path),
    }


def create_run_parameter_file(root_path: Path, output_path: Optional[Path] = None) -> Path:
    payload = make_root_parameter_payload(root_path)
    if output_path is not None:
        target = output_path.resolve()
    else:
        target = (Path.cwd() / Path(payload["name"])).resolve()
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(payload["content"], encoding="utf-8")
    return target


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
            is_enabled_comment = stripped.startswith("##") and (len(stripped) == 2 or stripped[2].isspace())
            is_line_comment = (len(stripped) == 1 or stripped[1].isspace()) or is_enabled_comment
            if is_line_comment:
                inner = stripped[2:].lstrip() if is_enabled_comment else stripped[1:].lstrip()
                rows.append(
                    {
                        "kind": "comment",
                        "enabled": is_enabled_comment,
                        "group": "",
                        "name": "",
                        "value": "",
                        "unit": "",
                        "comment": inner,
                        "line_no": line_no,
                    }
                )
                continue

            inner = stripped[1:]
            if should_try_parameter(inner):
                parsed = parse_parameter_candidate(inner, current_group)
                if parsed["kind"] == "group":
                    group_stack.append(parsed["full_name"] + "/")
                    continue
                parsed["enabled"] = True
                parsed["unit"] = "#"
                parsed["line_no"] = line_no
                rows.append(parsed)
                continue

            rows.append(
                {
                    "kind": "comment",
                    "enabled": False,
                    "group": "",
                    "name": "",
                    "value": "",
                    "unit": "",
                    "comment": inner.lstrip(),
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
        is_commented_parameter = unit == "#"
        prefix = "#" if is_commented_parameter else ("" if row.get("enabled", True) else "#")
        group_key = f"{unit}{group}" if group else unit
        if lines and previous_kind == "parameter" and previous_parameter_group != group_key and lines[-1] != "":
            lines.append("")
        line = f"{prefix}{'' if is_commented_parameter else unit}{full_name}"
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


def build_root_runtime_env(repo_root: Path):
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
    return env


def nptool_library_load_args(repo_root: Path):
    build_options_file = repo_root / "meta" / "build_options.cmake"
    if not build_options_file.is_file():
        return []

    text = build_options_file.read_text(encoding="utf-8")
    if "set(BUILD_NPTOOL ON" not in text:
        return []

    nplib_dir = os.environ.get("NPLib_DIR", "").strip()
    if not nplib_dir:
        env_guess = build_root_runtime_env(repo_root)
        nplib_dir = env_guess.get("NPLib_DIR", "").strip()
    if not nplib_dir:
        return []

    root_args = []
    in_nplib_list = False
    for raw_line in text.splitlines():
        trimmed = raw_line.lstrip()
        if not in_nplib_list:
            if trimmed.startswith("set(NPTOOL_NPLIB_LIST"):
                in_nplib_list = True
            continue
        if trimmed.startswith("CACHE INTERNAL ") or trimmed == ")":
            break
        lib_name = trimmed.split()[0] if trimmed.split() else ""
        if lib_name and lib_name != "${NPTOOL_NPLIB_LIST}":
            root_args.extend(["-e", f'gSystem->Load("{nplib_dir}/lib/lib{lib_name}.dylib");'])
    return root_args


def make_temp_run_file(content: str, path_hint: str = ""):
    target_path = (path_hint or "").strip()
    if target_path:
        target = Path(os.path.expanduser(os.path.expandvars(target_path)))
        if not target.is_absolute():
            target = (Path.cwd() / target).resolve()
        target.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            mode="w",
            suffix=target.suffix or ".par",
            prefix=".lilak_web_configure_",
            dir=str(target.parent),
            delete=False,
            encoding="utf-8",
        ) as handle:
            handle.write(content)
            temp_path = Path(handle.name)
        return temp_path, target.parent

    with tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".par",
        prefix="lilak_web_run_",
        delete=False,
        encoding="utf-8",
    ) as handle:
        handle.write(content)
        temp_path = Path(handle.name)
    return temp_path, lilak_root_path()


def run_root_macro_with_parameter(content: str, path_hint: str = "", set_print_plane: int = 1, set_allow_run: int = 1):
    lilak_root = lilak_root_path()
    env = build_root_runtime_env(lilak_root)
    temp_path, run_cwd = make_temp_run_file(content, path_hint)
    run_path = str(temp_path)

    macro_arg = f'{(lilak_root / "macros" / "run_lilak.C").as_posix()}("{run_path}",{set_print_plane},{set_allow_run})'

    root_args = ["root", "-l", "-b", "-q", "-n"]
    root_args.extend(nptool_library_load_args(lilak_root))
    root_args.extend(["-e", f'gSystem->Load("{lilak_root}/build/libLILAK.dylib");'])
    root_args.append(macro_arg)
    root_args.extend(["-e", 'gSystem->Exit(0);'])

    try:
        result = subprocess.run(
            root_args,
            cwd=run_cwd,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )
    finally:
        try:
            temp_path.unlink()
        except OSError:
            pass

    return {
        "command": "",
        "stdout": result.stdout,
        "stderr": result.stderr,
        "returncode": result.returncode,
        "path": run_path,
        "log_info": read_last_lk_output_log_info(),
    }


def normalize_with_lkparametercontainer(content: str):
    repo_root = Path(__file__).resolve().parent.parent
    env = build_root_runtime_env(repo_root)

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


def lilak_root_path() -> Path:
    configured = os.environ.get("LILAK_PATH", "").strip()
    if configured:
        return Path(configured).expanduser().resolve()
    return Path(__file__).resolve().parent.parent


def read_par_class_entries():
    class_list_path = lilak_root_path() / "meta" / "LKClassList.par.log"
    if not class_list_path.is_file():
        raise FileNotFoundError(str(class_list_path))
    entries = []
    for raw_line in class_list_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip().strip("/")
        if line:
            entries.append(line)
    return sorted(dict.fromkeys(entries))


def class_parameter_row_count(class_name: str) -> int:
    parameter_path = lilak_root_path() / "meta" / "parameters" / f"configure_{class_name}.mac"
    if not parameter_path.is_file():
        return 0
    try:
        rows = parse_parameter_text(parameter_path.read_text(encoding="utf-8"))
    except OSError:
        return 0
    return sum(1 for row in rows if row.get("kind") == "parameter")


def normalize_class_tree_path(path_value: str) -> str:
    value = (path_value or "").strip()
    if not value or value == ".":
        return ""
    parts = []
    for part in PurePosixPath(value).parts:
        if part in ("", "."):
            continue
        if part == "..":
            if parts:
                parts.pop()
            continue
        parts.append(part)
    return "/".join(parts)


def list_class_directory(path_value: str):
    current_dir = normalize_class_tree_path(path_value)
    prefix = f"{current_dir}/" if current_dir else ""
    directories = {}
    files = []
    for entry in read_par_class_entries():
        if prefix:
            if not entry.startswith(prefix):
                continue
            remainder = entry[len(prefix):]
        else:
            remainder = entry
        if not remainder:
            continue
        parts = remainder.split("/")
        if len(parts) == 1:
            class_name = parts[0]
            files.append(
                {
                    "name": class_name,
                    "path": entry,
                    "is_dir": False,
                    "parameter_count": class_parameter_row_count(class_name),
                }
            )
            continue
        child_dir = parts[0]
        directories.setdefault(
            child_dir,
            {
                "name": child_dir,
                "path": f"{prefix}{child_dir}".rstrip("/"),
                "is_dir": True,
            },
        )
    entries = sorted(directories.values(), key=lambda item: item["name"].lower())
    entries.extend(sorted(files, key=lambda item: item["name"].lower()))
    parent_dir = ""
    if current_dir:
        parent_dir = current_dir.rsplit("/", 1)[0] if "/" in current_dir else ""
    return {
        "current_dir": current_dir,
        "parent_dir": parent_dir,
        "entries": entries,
    }


def load_class_parameter_payload(class_path: str):
    normalized = normalize_class_tree_path(class_path)
    if not normalized:
        raise FileNotFoundError("Class path not selected")
    class_name = normalized.rsplit("/", 1)[-1]
    parameter_path = lilak_root_path() / "meta" / "parameters" / f"configure_{class_name}.mac"
    if not parameter_path.is_file():
        raise FileNotFoundError(str(parameter_path))
    payload = make_file_payload(parameter_path)
    payload["class_name"] = class_name
    payload["class_path"] = normalized
    payload["source_path"] = str(parameter_path)
    return payload


def parse_branch_parameter_file(path: Path):
    entries = []
    if not path.is_file():
        return entries
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 3:
            continue
        action, class_name, branch_name = parts[0], parts[1], parts[2]
        entries.append(
            {
                "action": action,
                "class_name": class_name,
                "branch_name": branch_name,
            }
        )
    return entries


def local_doc_url_for_class(class_name: str):
    doc_file = lilak_root_path() / "doc" / f"class{class_name}.html"
    if doc_file.is_file():
        return f"/doc/{doc_file.name}"
    return ""


def doc_url_for_class(class_name: str):
    local_url = local_doc_url_for_class(class_name)
    if local_url:
        return local_url
    return f"https://lilak-project.github.io/lilak_doxygen/class{class_name}.html"


def terminal_doc_url_for_class(class_name: str):
    return f"https://lilak-project.github.io/lilak_doxygen/class{class_name}.html"


def read_named_class_log(file_name: str):
    path = lilak_root_path() / "meta" / file_name
    if not path.is_file():
        return set()
    values = set()
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip().strip("/")
        if not line:
            continue
        values.add(line.rsplit("/", 1)[-1])
    return values


def class_kind_for_name(class_name: str):
    if class_name in read_named_class_log("LKClassList.task.log"):
        return "task"
    if class_name in read_named_class_log("LKClassList.detector.log"):
        return "detector"
    return "class"


def build_configure_report(content: str, path_hint: str = ""):
    rows = parse_parameter_text(content)
    class_names = []
    seen = set()
    for row in rows:
        if row.get("kind") != "parameter":
            continue
        if (row.get("unit") or "").strip() == "#":
            continue
        if row.get("enabled") is False:
            continue
        if (row.get("group") or "").strip() != "lilak":
            continue
        if (row.get("name") or "").strip() != "add":
            continue
        class_name = (row.get("value") or "").strip()
        if not class_name or class_name in seen:
            continue
        seen.add(class_name)
        class_names.append(class_name)

    configure_run = run_root_macro_with_parameter(content, path_hint, set_print_plane=0, set_allow_run=0)
    log_info = configure_run.get("log_info", {})
    repeated_log = {}
    resolved_log_path = Path(log_info.get("resolved_path", "")) if log_info.get("resolved_path") else None
    if resolved_log_path and resolved_log_path.is_file():
        repeated_log = parse_lk_log_multimap(resolved_log_path)

    input_branches = parse_branch_lines(repeated_log.get("branch/input", []))
    classes = []
    for class_name in class_names:
        branch_path = lilak_root_path() / "meta" / "parameters" / f"branch_{class_name}.par"
        branch_entries = parse_branch_parameter_file(branch_path)
        uses = []
        saves = []
        for entry in branch_entries:
            item = {
                "branch_name": entry["branch_name"],
                "class_name": entry["class_name"],
                "action": entry["action"],
                "doc_url": doc_url_for_class(entry["class_name"]),
            }
            if entry["action"] in {"get", "keep"}:
                uses.append(item)
            if entry["action"] in {"register", "keep"}:
                saves.append(item)
        classes.append(
            {
                "class_name": class_name,
                "doc_url": doc_url_for_class(class_name),
                "class_kind": class_kind_for_name(class_name),
                "branch_file": str(branch_path),
                "branch_file_exists": branch_path.is_file(),
                "uses": uses,
                "saves": saves,
            }
        )

    return {
        "classes": classes,
        "input_branches": input_branches,
        "count": len(classes),
        "configure_log": {
            "returncode": configure_run.get("returncode", 0),
            "stdout": configure_run.get("stdout", ""),
            "stderr": configure_run.get("stderr", ""),
            "summary": log_info.get("summary", ""),
            "kind": log_info.get("kind", ""),
            "path": log_info.get("resolved_path", ""),
            "input_file": log_info.get("parsed", {}).get("InputFile", ""),
            "output_file": log_info.get("parsed", {}).get("OutputFile", ""),
        },
    }


def format_terminal_configure_report(report: dict) -> str:
    use_color = sys.stdout.isatty() and not os.environ.get("NO_COLOR")

    def color(text: str, code: str) -> str:
        if not use_color:
            return text
        return f"\033[{code}m{text}\033[0m"

    def visible_len(text: str) -> int:
        length = 0
        inside_escape = False
        for char in text:
            if inside_escape:
                if char == "m":
                    inside_escape = False
                continue
            if char == "\033":
                inside_escape = True
                continue
            length += 1
        return length

    def pad_visible(text: str, width: int) -> str:
        return text + (" " * max(width - visible_len(text), 0))

    def fit(text: str, width: int) -> str:
        text = text or ""
        if width <= 0:
            return ""
        if len(text) <= width:
            return text.ljust(width)
        if width <= 3:
            return text[:width]
        return text[: width - 3] + "..."

    branch_class_align = 0
    branch_name_align = 0

    def branch_label(branch_name: str, class_name: str, prefix: str) -> str:
        prefix_text = color(f"[{prefix}]", "36" if prefix == "input" else "33")
        if class_name:
            class_cell = pad_visible(f"({class_name})", branch_class_align)
            branch_cell = pad_visible(branch_name, branch_name_align)
            return f"{prefix_text} {class_cell} {branch_cell}"
        return f"{prefix_text} {branch_name}"

    def class_label(class_name: str, class_kind: str) -> str:
        return f"{color(f'[{class_kind}]', '32')} {class_name}"

    def branch_index_label(index: int, repeated: bool = False) -> str:
        marker = f"*[{index}]" if repeated else f" [{index}]"
        return color(marker, "33")

    def task_index_label(index: int, repeated: bool = False) -> str:
        marker = f"*[{index}]" if repeated else f" [{index}]"
        return color(marker, "32")

    lines = []
    configure_log = report.get("configure_log", {}) or {}
    classes = report.get("classes", []) or []
    input_branches = report.get("input_branches", []) or []

    branch_items = []
    for item in input_branches:
        branch_items.append((item.get("branch_name", ""), item.get("class_name", "")))
    for item in classes:
        for use in item.get("uses", []) or []:
            branch_items.append((use.get("branch_name", ""), use.get("class_name", "")))
        for save in item.get("saves", []) or []:
            branch_items.append((save.get("branch_name", ""), save.get("class_name", "")))
    branch_class_align = max((len(f"({class_name})") for _, class_name in branch_items if class_name), default=0)
    branch_name_align = max((len(branch_name) for branch_name, _ in branch_items), default=0)

    lines.append(color("========================================", "90"))
    lines.append(color("LILAK Configure Report", "1;37"))
    lines.append(color("========================================", "90"))
    lines.append(f"Classes: {report.get('count', 0)}")
    if configure_log.get("summary"):
        summary = configure_log.get("summary")
        status_color = "31" if "failed" in summary.lower() else "32"
        lines.append(f"Status: {color(summary, status_color)}")
    if configure_log.get("path"):
        lines.append(f"Log: {configure_log.get('path')}")
    if configure_log.get("input_file"):
        lines.append(f"Input: {configure_log.get('input_file')}")
    if configure_log.get("output_file"):
        lines.append(f"Output: {configure_log.get('output_file')}")

    tail_lines = []
    if input_branches:
        tail_lines.append("")
        tail_lines.append(color("Input Branches", "1;36"))
        tail_lines.append(color("----------------------------------------", "90"))
        for item in input_branches:
            branch_name = item.get("branch_name", "")
            class_name = item.get("class_name", "")
            tail_lines.append(f"  {branch_label(branch_name, class_name, 'input')}")
            doc_url = terminal_doc_url_for_class(class_name)
            if class_name:
                tail_lines.append(f"    reference: {doc_url}")

    graph_lines = []
    if classes:
        class_ref_lines = []
        input_branch_map = {item.get("branch_name", ""): item for item in input_branches}
        left_width = 34
        middle_width = 34
        right_width = 34
        graph_lines.append(f"{fit('Input', left_width)}    {fit('Branch', middle_width)}    {fit('Task', right_width)}")
        graph_lines.append(color(f"{'-' * left_width}    {'-' * middle_width}    {'-' * right_width}", "90"))

        doc_lines = []
        seen_doc_lines = set()
        branch_indices = {}
        branch_order = []
        task_indices = {}
        for item in classes:
            class_name = item.get("class_name", "")
            class_kind = item.get("class_kind", "class")
            doc_url = terminal_doc_url_for_class(class_name)
            class_text = class_label(class_name, class_kind)
            if doc_url and class_name not in seen_doc_lines:
                class_ref_lines.append(f"  {class_text}: {doc_url}")
                seen_doc_lines.add(class_name)
            if class_name not in task_indices:
                task_indices[class_name] = len(task_indices) + 1

            for use in item.get("uses", []) or []:
                branch_name = use.get("branch_name", "")
                branch_class = use.get("class_name", "")
                branch_key = (branch_name, branch_class)
                if branch_key not in branch_indices:
                    branch_indices[branch_key] = len(branch_indices) + 1
                    branch_order.append(branch_key)
            for save in item.get("saves", []) or []:
                branch_name = save.get("branch_name", "")
                branch_class = save.get("class_name", "")
                branch_key = (branch_name, branch_class)
                if branch_key not in branch_indices:
                    branch_indices[branch_key] = len(branch_indices) + 1
                    branch_order.append(branch_key)
        graph_rows = []
        for item in classes:
            class_name = item.get("class_name", "")
            class_kind = item.get("class_kind", "class")
            task_text = f"{task_index_label(task_indices.get(class_name, 0))} {class_label(class_name, class_kind)}"
            for use in item.get("uses", []) or []:
                branch_name = use.get("branch_name", "")
                branch_class = use.get("class_name", "")
                action = use.get("action", "get")
                input_item = input_branch_map.get(branch_name, {})
                input_text = ""
                if input_item:
                    input_text = branch_label(
                        input_item.get("branch_name", ""),
                        input_item.get("class_name", ""),
                        "input",
                    )
                branch_key = (branch_name, branch_class)
                branch_text = branch_label(branch_name, branch_class, 'branch')
                arrow = color("--read-->", "34") if action == "get" else color("..keep...", "32")
                graph_rows.append((input_text, branch_text, task_text, arrow, True, class_name, branch_key))

            for save in item.get("saves", []) or []:
                branch_name = save.get("branch_name", "")
                branch_class = save.get("class_name", "")
                action = save.get("action", "register")
                if action == "keep":
                    continue
                branch_key = (branch_name, branch_class)
                branch_text = branch_label(branch_name, branch_class, 'branch')
                arrow = color("<-write--", "33")
                graph_rows.append(("", branch_text, task_text, arrow, False, class_name, branch_key))

        if not graph_rows:
            graph_lines.append("No branch relations found")
        else:
            left_align = max((visible_len(item[0]) for item in graph_rows), default=0)
            middle_align = max(
                (
                    visible_len(f"{branch_index_label(branch_indices.get(item[6], 0))} {item[1]}")
                    for item in graph_rows
                ),
                default=0,
            )
            right_align = max((visible_len(item[2]) for item in graph_rows), default=0)
            seen_task_rows = set()
            seen_branch_rows = set()
            relation_align = max((visible_len(item[3]) for item in graph_rows), default=0)
            connector_align = 2
            for left_text, middle_text, right_text, arrow, left_to_right, task_key, branch_key in graph_rows:
                left_cell = pad_visible(left_text, left_align)
                branch_index = branch_indices.get(branch_key, 0)
                branch_text = f"{branch_index_label(branch_index, repeated=branch_key in seen_branch_rows)} {middle_text}"
                middle_cell = pad_visible(branch_text, middle_align)
                seen_branch_rows.add(branch_key)
                if task_key in seen_task_rows:
                    right_cell = task_index_label(task_indices.get(task_key, 0), repeated=True)
                else:
                    right_cell = right_text
                    seen_task_rows.add(task_key)
                right_cell = pad_visible(right_cell, right_align)
                relation_cell = pad_visible(arrow, relation_align)
                connector = "->" if left_text else ""
                connector_cell = pad_visible(connector, connector_align)
                graph_lines.append(f"{left_cell} {connector_cell} {middle_cell} {relation_cell} {right_cell}")

        if class_ref_lines:
            tail_lines.append("")
            tail_lines.append(color("Task/Detector References", "1;32"))
            tail_lines.append(color("----------------------------------------", "90"))
            tail_lines.extend(class_ref_lines)

        tail_lines.append("")
        tail_lines.append(color("Graph", "1;35"))
        tail_lines.append(color("----------------------------------------", "90"))
        tail_lines.extend(graph_lines)

    stdout = (configure_log.get("stdout", "") or "").strip()
    stderr = (configure_log.get("stderr", "") or "").strip()
    if stdout:
        lines.append(color("ROOT stdout", "1;34"))
        lines.append(color("----------------------------------------", "90"))
        lines.append(stdout)
    if stderr:
        lines.append("")
        lines.append(color("ROOT stderr", "1;31"))
        lines.append(color("----------------------------------------", "90"))
        lines.append(stderr)

    lines.extend(tail_lines)

    return "\n".join(lines).rstrip() + "\n"


def find_matching_classes_for_branch(branch_name: str, class_name: str):
    branch_name = (branch_name or "").strip()
    class_name = (class_name or "").strip()
    if not branch_name or not class_name:
        return {"entries": [], "current_dir": "", "parent_dir": ""}

    branch_to_class_path = {}
    for entry in read_par_class_entries():
        branch_to_class_path[entry.rsplit("/", 1)[-1]] = entry

    entries = []
    for branch_file in sorted((lilak_root_path() / "meta" / "parameters").glob("branch_*.par")):
        target_class = branch_file.stem.removeprefix("branch_")
        branch_entries = parse_branch_parameter_file(branch_file)
        matched = any(
            item["action"] in {"get", "keep"}
            and item["branch_name"] == branch_name
            and item["class_name"] == class_name
            for item in branch_entries
        )
        if not matched:
            continue
        class_path = branch_to_class_path.get(target_class, target_class)
        entries.append(
            {
                "name": target_class,
                "path": class_path,
                "is_dir": False,
                "parameter_count": class_parameter_row_count(target_class),
            }
        )

    return {
        "current_dir": f"{class_name}/{branch_name}",
        "parent_dir": "",
        "entries": entries,
    }


def run_lilak_with_content(content: str, path_hint: str = ""):
    return run_root_macro_with_parameter(content, path_hint, set_print_plane=1, set_allow_run=1)


def parse_lk_log_multimap(path: Path):
    parsed = {}
    if not path.is_file():
        return parsed
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.rstrip()
        if not line.strip():
            continue
        parts = line.split()
        if not parts:
            continue
        key = parts[0]
        value = line[len(key):].strip()
        parsed.setdefault(key, []).append(value)
    return parsed


def parse_branch_lines(values):
    entries = []
    for value in values or []:
        parts = value.split()
        if len(parts) < 2:
            continue
        class_name, branch_name = parts[0], parts[1]
        entries.append(
            {
                "class_name": class_name,
                "branch_name": branch_name,
                "doc_url": doc_url_for_class(class_name),
            }
        )
    return entries


def parse_lk_log_file(path: Path):
    parsed = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.rstrip()
        if not line.strip():
            continue
        parts = line.split()
        if not parts:
            continue
        key = parts[0]
        value = line[len(key):].strip()
        parsed[key] = value
    return parsed


def summarize_lk_log_info(log_info):
    link_target = log_info.get("link_target", "")
    if not link_target:
        return "Run failed: lk_last_output.log is empty"

    kind = log_info.get("kind", "unknown")
    parsed = log_info.get("parsed", {})
    initialized = parsed.get("Initialized", "")
    num_events = parsed.get("num_events", "")
    run_time = parsed.get("run_time", "")

    if kind == "run":
        details = []
        if num_events:
            details.append(f"{num_events} events")
        if run_time:
            details.append(run_time)
        suffix = f" ({', '.join(details)})" if details else ""
        return f"Run completed{suffix}"
    if kind == "init":
        if initialized == "1":
            return "Initialization finished, but run did not complete"
        return "Initialization failed"
    return "Run finished, but log type is unknown"


def read_last_lk_output_log_info():
    link_path = lilak_root_path() / "data" / "lk_last_output.log"
    if not link_path.exists() and not link_path.is_symlink():
        return {
            "link_path": str(link_path),
            "link_target": "",
            "resolved_path": "",
            "kind": "missing",
            "summary": "Run failed: lk_last_output.log does not exist",
            "content": "",
            "parsed": {},
        }

    raw_target = os.readlink(link_path) if link_path.is_symlink() else ""
    if not raw_target:
        return {
            "link_path": str(link_path),
            "link_target": "",
            "resolved_path": "",
            "kind": "empty",
            "summary": "Run failed: lk_last_output.log is empty",
            "content": "",
            "parsed": {},
        }

    resolved_path = Path(raw_target)
    if not resolved_path.is_absolute():
        resolved_path = (link_path.parent / resolved_path).resolve()

    content = ""
    parsed = {}
    if resolved_path.is_file():
        content = resolved_path.read_text(encoding="utf-8")
        parsed = parse_lk_log_file(resolved_path)

    base_name = resolved_path.name
    kind = "unknown"
    if base_name.startswith("run_"):
        kind = "run"
    elif base_name.startswith("init_"):
        kind = "init"
    elif base_name.startswith("eor_"):
        kind = "eor"

    info = {
        "link_path": str(link_path),
        "link_target": raw_target,
        "resolved_path": str(resolved_path),
        "kind": kind,
        "content": content,
        "parsed": parsed,
    }
    info["summary"] = summarize_lk_log_info(info)
    return info


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
    .menu-switch-button[data-menu-toggle="all"] {
      background: #e6edf5;
      color: #36526c;
      border-color: #cfdae6;
    }
    .menu-switch-button[data-menu-toggle="all"].active {
      background: #7d96b1;
      color: #ffffff;
      border-color: #687f97;
    }
    .menu-switch-button[data-menu-toggle="run"] {
      background: #dcefe4;
      color: #27563f;
      border-color: #bddcc9;
    }
    .menu-switch-button[data-menu-toggle="run"].active {
      background: #63a07a;
      color: #ffffff;
      border-color: #4f8765;
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
    .toolbar-button:disabled,
    .toolbar-group[data-menu-panel="file"] .toolbar-button:disabled,
    .toolbar-group[data-menu-panel="view"] .toolbar-button:disabled,
    .toolbar-group[data-menu-panel="run"] .toolbar-button:disabled {
      background: #eeeeee;
      color: #9a9a9a;
      border-color: #d7d7d7;
      cursor: default;
      filter: none;
      box-shadow: none;
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
    .toolbar-group[data-menu-panel="run"] {
      background: #e8f3ec;
      align-items: flex-start;
    }
    .toolbar-group[data-menu-panel="run"] .toolbar-button {
      background: #dcefe4;
      color: #27563f;
      border-color: #c7e1d0;
    }
    .toolbar-group[data-menu-panel="run"] .toolbar-button.active {
      background: #63a07a;
      color: #ffffff;
      border-color: #4f8765;
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
    .status-text.error {
      color: #b00020;
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
    .message-bar.error {
      color: #b00020;
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
    tr.commented-parameter-row td {
      background: #eceff1;
    }
    tr.commented-parameter-row td input[type="text"] {
      background: #eceff1;
      color: #7a7f85;
    }
    tr.duplicate-parameter-row td {
      background: #fffbe8;
    }
    tr.duplicate-parameter-row td input[type="text"] {
      background: #ffffff;
    }
    tr.active-duplicate-parameter-row td {
      background: #ffeef4;
    }
    tr.active-duplicate-parameter-row td input[type="text"] {
      background: #ffffff;
    }
    tr.commented-duplicate-parameter-row td {
      background: #fffbe8;
    }
    tr.commented-duplicate-parameter-row td input[type="text"] {
      background: #ffffff;
      color: #7a7f85;
    }
    tr.selected.duplicate-parameter-row td {
      background: #fffbe8;
    }
    tr.selected.active-duplicate-parameter-row td {
      background: #ffeef4;
    }
    tr.selected.commented-duplicate-parameter-row td {
      background: #fffbe8;
    }
    tr.selected.commented-parameter-row td {
      background: #eceff1;
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
    .run-panel {
      display: grid;
      gap: 8px;
      flex: 1 1 480px;
      min-width: 280px;
    }
    .run-panel.expanded {
      width: 100%;
    }
    .run-output {
      width: 100%;
      min-height: 160px;
      border: 1px solid #c7e1d0;
      border-radius: 10px;
      background: #ffffff;
      color: #244334;
      font: 12px/1.5 var(--mono);
      padding: 10px;
      white-space: pre-wrap;
      overflow: auto;
    }
    .run-output.expanded {
      min-height: calc(100vh - 220px);
      height: calc(100vh - 220px);
    }
    .run-output a {
      color: var(--accent);
      text-decoration: none;
    }
    .run-output a:hover {
      text-decoration: underline;
    }
    .configure-report {
      position: relative;
      min-height: 0;
      overflow: hidden;
    }
    .configure-legend {
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      align-items: center;
      margin-bottom: 8px;
      font-size: 11px;
      color: var(--muted);
    }
    .configure-legend-item {
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }
    .configure-legend-line {
      width: 28px;
      height: 0;
      border-top: 2px solid #7f9db8;
      position: relative;
      display: inline-block;
    }
    .configure-legend-line.read {
      border-top-color: #4f82b4;
    }
    .configure-legend-line.write {
      border-top-color: #d07a2d;
    }
    .configure-legend-line.keep {
      border-top-color: #4f9b54;
      border-top-style: dashed;
    }
    .configure-legend-line.input-match {
      border-top-color: #8c98a6;
    }
    .configure-legend-line.arrow::after {
      content: "";
      position: absolute;
      right: -1px;
      top: -5px;
      width: 0;
      height: 0;
      border-top: 4px solid transparent;
      border-bottom: 4px solid transparent;
      border-left: 6px solid currentColor;
    }
    .configure-legend-line.read.arrow {
      color: #4f82b4;
    }
    .configure-legend-line.write.arrow {
      color: #d07a2d;
    }
    .configure-graph {
      position: relative;
      min-width: 0;
    }
    .configure-svg {
      position: absolute;
      inset: 0;
      overflow: visible;
      pointer-events: none;
    }
    .configure-edge {
      stroke-width: 2.1;
      fill: none;
      opacity: 0.9;
    }
    .configure-edge.read {
      stroke: #4f82b4;
    }
    .configure-edge.write {
      stroke: #d07a2d;
    }
    .configure-edge.keep {
      stroke: #4f9b54;
      stroke-width: 2.4;
      opacity: 0.95;
      stroke-dasharray: 6 4;
    }
    .configure-edge.input-match {
      stroke: #8c98a6;
      stroke-width: 1.8;
      opacity: 0.8;
    }
    .configure-node {
      position: absolute;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: rgba(255, 255, 255, 0.95);
      box-shadow: 0 2px 6px rgba(80, 80, 80, 0.06);
      padding: 4px 6px;
      font-size: 11px;
      line-height: 1.12;
      overflow: hidden;
      display: flex;
      flex-direction: column;
      justify-content: center;
    }
    .configure-node.branch {
      border-color: #cfdbe8;
      background: #f7fbff;
    }
    .configure-node.input {
      border-color: #d9dce2;
      background: #fafbfc;
    }
    .configure-node.class {
      border-color: #d7e4d4;
      background: #f7fcf6;
    }
    .configure-node-title {
      font-weight: 600;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      margin-bottom: 1px;
    }
    .configure-node-head {
      display: flex;
      align-items: center;
      gap: 4px;
    }
    .configure-node-head .configure-node-title {
      margin-bottom: 0;
      flex: 1 1 auto;
    }
    .configure-add-button {
      border: 1px solid #c5d8eb;
      background: #d8e6f4;
      color: #203448;
      border-radius: 6px;
      padding: 0 5px;
      height: 18px;
      font-size: 10px;
      line-height: 16px;
      cursor: pointer;
      flex: 0 0 auto;
    }
    .configure-node-subtitle {
      color: var(--muted);
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .configure-node-subtitle a {
      color: var(--accent);
      text-decoration: none;
    }
    .configure-node-subtitle a:hover,
    .configure-node-title a:hover {
      text-decoration: underline;
    }
    .configure-empty {
      color: var(--muted);
      font-style: italic;
    }
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
                <button class="menu-switch-button active" data-menu-toggle="all">ALL</button>
                <button class="menu-switch-button" data-menu-toggle="file">FILE</button>
                <button class="menu-switch-button" data-menu-toggle="view">VIEW</button>
                <button class="menu-switch-button" data-menu-toggle="parameter">PARAMETER</button>
                <button class="menu-switch-button" data-menu-toggle="run">LILAK</button>
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

              <div class="toolbar-group" data-menu-panel="run">
                <span class="menu-label">RUN</span>
                <div class="run-panel">
                  <div class="toolbar-row">
                    <button class="toolbar-button" data-action="run-task"><span class="icon">?</span>add</button>
                    <button class="toolbar-button" data-action="run-configure"><span class="icon">&#9881;</span>configure</button>
                    <button class="toolbar-button" data-action="run-current"><span class="icon">&#9654;</span>run</button>
                    <button class="toolbar-button" data-action="run-expand"><span class="icon">&#8645;</span>expand</button>
                    <button class="toolbar-button" data-action="run-status"><span class="icon">&#9432;</span>status</button>
                    <button class="toolbar-button" data-action="run-log"><span class="icon">&#8801;</span>log</button>
                    <button class="toolbar-button" data-action="run-info"><span class="icon">i</span>info</button>
                    <button class="toolbar-button" data-action="run-file"><span class="icon">&#128193;</span>file</button>
                  </div>
                  <div class="run-output" id="runOutput"></div>
                </div>
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
      activeMenu: "all",
      rawMode: "original",
      rawParseTimer: null,
      untitledIndex: 1,
      runOutput: "",
      runOutputIsHtml: false,
      runPanelView: "",
      runStatusOutput: "",
      runStatusMessage: "",
      hasRunResult: false,
      runPanelExpanded: false
    };

    const statusEl = document.getElementById("status");
    const tableWrapEl = document.getElementById("tableWrap");
    const tabBarEl = document.getElementById("tabBar");
    const menuBarEl = document.querySelector(".menu-bar");
    const runOutputHostEl = document.getElementById("runOutput");

    if (runOutputHostEl) {
      runOutputHostEl.addEventListener("click", async (event) => {
        const button = event.target.closest("[data-config-add]");
        if (!button) {
          return;
        }
        event.preventDefault();
        event.stopPropagation();
        const currentTab = currentEditorTab();
        try {
          await createNavigatorTab("matchclass", currentTab?.id || null, {
            branchName: button.dataset.branchName || "",
            className: button.dataset.branchClass || ""
          });
        } catch (error) {
          setStatus(error.message, "warn");
        }
      });
    }

    function setStatus(message, tone = "") {
      statusEl.textContent = message;
      statusEl.className = "status-text" + (tone ? " " + tone : "");
    }

    function renderMenuPanels() {
      for (const button of document.querySelectorAll("[data-menu-toggle]")) {
        button.classList.toggle("active", button.dataset.menuToggle === state.activeMenu);
      }
      for (const panel of document.querySelectorAll("[data-menu-panel]")) {
        const panelName = panel.dataset.menuPanel;
        const showAllPanel = state.activeMenu === "all" && ["file", "view", "parameter"].includes(panelName);
        panel.style.display = showAllPanel || panelName === state.activeMenu ? "flex" : "none";
      }
      const runPanelEl = document.querySelector(".run-panel");
      if (runPanelEl) {
        runPanelEl.classList.toggle("expanded", state.runPanelExpanded);
      }
      const runOutputEl = document.getElementById("runOutput");
      if (runOutputEl) {
        if (state.runOutputIsHtml) {
          runOutputEl.innerHTML = state.runOutput || "";
        } else {
          runOutputEl.textContent = state.runOutput || "";
        }
        runOutputEl.classList.toggle("expanded", state.runPanelExpanded);
      }
      const runConfigureButton = document.querySelector('[data-action="run-configure"]');
      if (runConfigureButton) {
        runConfigureButton.disabled = !currentEditorTab();
      }
      const runCurrentButton = document.querySelector('[data-action="run-current"]');
      if (runCurrentButton) {
        const tab = currentEditorTab();
        runCurrentButton.disabled = !canRunCurrentTab(tab);
      }
      const runExpandButton = document.querySelector('[data-action="run-expand"]');
      if (runExpandButton) {
        runExpandButton.textContent = state.runPanelExpanded ? "collapse" : "expand";
      }
      for (const button of document.querySelectorAll('[data-action="run-status"], [data-action="run-log"], [data-action="run-info"], [data-action="run-file"]')) {
        button.disabled = !state.hasRunResult;
      }
    }

    function currentRunContent(tab) {
      if (!tab) {
        return "";
      }
      if (state.viewMode === "raw") {
        return tab.rawDraft ?? tab.content ?? "";
      }
      return serializeRowsForRaw(rowsInCurrentViewOrder(tab));
    }

    function canRunCurrentTab(tab) {
      if (!tab) {
        return false;
      }
      const current = currentRunContent(tab);
      return current !== (tab.lastRunContent || "");
    }

    function isRunStatusFailure(summary, returncode = 0) {
      const text = String(summary || "").toLowerCase();
      return returncode !== 0 || text.includes("failed") || text.includes("did not complete");
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
        const isCommentedParameter = (row.unit || "").trim() === "#";
        let line = (isCommentedParameter ? "#" : (row.enabled === false ? "#" : "")) + (isCommentedParameter ? "" : (row.unit || "").trim()) + fullName;
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

    function parameterIdentity(row) {
      if (!row || row.kind !== "parameter") {
        return "";
      }
      const group = (row.group || "").trim();
      const name = (row.name || "").trim();
      if (!name) {
        return "";
      }
      return `${group}::${name}`;
    }

    function duplicateParameterIds(rows) {
      const counts = new Map();
      for (const row of rows || []) {
        const key = parameterIdentity(row);
        if (!key) continue;
        counts.set(key, (counts.get(key) || 0) + 1);
      }
      const duplicates = new Set();
      for (const row of rows || []) {
        const key = parameterIdentity(row);
        if (key && (counts.get(key) || 0) > 1) {
          duplicates.add(row.id);
        }
      }
      return duplicates;
    }

    function duplicateParameterKeys(rows) {
      const counts = new Map();
      for (const row of rows || []) {
        const key = parameterIdentity(row);
        if (!key) continue;
        counts.set(key, (counts.get(key) || 0) + 1);
      }
      return Array.from(counts.entries())
        .filter(([, count]) => count > 1)
        .map(([key]) => key);
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
        const isCommentedParameter = (row.unit || "").trim() === "#";
        const unit = isCommentedParameter ? "" : (row.unit || "").trim();
        return (isCommentedParameter ? "#" : (row.enabled === false ? "#" : "")) + unit + base + value + comment;
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
        dirty: false,
        lastRunContent: ""
      };
      state.tabs.push(tab);
      state.untitledIndex += 1;
      setActiveTab(tab.id);
      setStatus(`Opened ${tab.title}`, "ok");
    }

    async function createNavigatorTab(action, targetEditorId = null, options = {}) {
      if (action === "open" || action === "saveas" || action === "findclass" || action === "matchclass") {
        const existing = actionNavigatorTab(action);
        if (existing) {
          existing.targetEditorId = targetEditorId || existing.targetEditorId;
          const sourceEditor = getEditorTab(targetEditorId) || getEditorTab(activeTab()?.id);
          existing.browserPath = (action === "findclass" || action === "matchclass") ? (existing.browserPath || "") : (sourceEditor?.path || existing.browserPath || "");
          if (action === "matchclass") {
            existing.matchBranchName = options.branchName || "";
            existing.matchClassName = options.className || "";
          }
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
        browserPath: (action === "findclass" || action === "matchclass") ? "" : (sourceEditor?.path || ""),
        fileName: (action === "findclass" || action === "matchclass") ? "" : (sourceEditor?.path ? sourceEditor.path.split("/").pop() : sourceEditor?.title || ""),
        currentDir: "",
        entries: [],
        matchBranchName: options.branchName || "",
        matchClassName: options.className || ""
      };
      state.tabs.push(tab);
      state.activeTabId = tab.id;
      await refreshNavigatorTab(tab, tab.browserPath || "");
      render();
      setStatus(`${action} navigator opened`, "ok");
    }

    async function refreshNavigatorTab(tab, pathValue) {
      const endpoint = tab.action === "findclass"
        ? "/api/classdir"
        : tab.action === "matchclass"
          ? "/api/classmatch"
          : "/api/listdir";
      const requestBody = tab.action === "matchclass"
        ? { branch_name: tab.matchBranchName || "", class_name: tab.matchClassName || "" }
        : { path: pathValue || "" };
      const response = await fetch(endpoint, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(requestBody)
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
        node.addEventListener("dblclick", async (event) => {
          if (event.target.dataset.closeTab) {
            return;
          }
          const tab = getEditorTab(node.dataset.tabId);
          if (!tab) {
            return;
          }
          try {
            setActiveTab(node.dataset.tabId);
            await saveActiveTab();
          } catch (error) {
            setStatus(error.message, "warn");
          }
        });
      }
    }

    function groupedRows(rows) {
      const parameterOrder = [];
      const commentedParameterOrder = [];
      const commentOrder = [];
      const groups = new Map();
      for (const row of rows) {
        const key = row.kind === "comment" ? "__comment__" : parameterGroupKey(row);
        if (!groups.has(key)) {
          groups.set(key, []);
          if (key === "__comment__") commentOrder.push(key);
          else if ((row.unit || "").trim() === "#") commentedParameterOrder.push(key);
          else parameterOrder.push(key);
        }
        groups.get(key).push(row);
      }
      for (const key of parameterOrder) {
        const groupRows = groups.get(key) || [];
        const activeRows = groupRows.filter((row) => !((row.unit || "").trim() === "#"));
        const commentedRows = groupRows.filter((row) => (row.unit || "").trim() === "#");
        groups.set(key, [...activeRows, ...commentedRows]);
      }
      const lilakFirst = [];
      const activeRest = [];
      for (const key of parameterOrder) {
        if (key === "lilak") lilakFirst.push(key);
        else activeRest.push(key);
      }
      return { order: [...lilakFirst, ...activeRest, ...commentedParameterOrder, ...commentOrder], groups };
    }

    function rowsInCurrentViewOrder(tab) {
      if (!tab || state.viewMode !== "group") {
        return tab?.rows || [];
      }
      const grouped = groupedRows(tab.rows || []);
      const orderedRows = [];
      for (const key of grouped.order) {
        const groupRows = grouped.groups.get(key) || [];
        orderedRows.push(...groupRows);
      }
      return orderedRows;
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

      const duplicateIds = duplicateParameterIds(tab.rows || []);
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
              rowsHtml.push(renderRow(row, visualIndex, duplicateIds));
              visualIndex += 1;
            }
          }
        }
      } else {
        tab.rows.forEach((row, index) => rowsHtml.push(renderRow(row, index + 1, duplicateIds)));
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
      const isClassNavigator = tab.action === "findclass" || tab.action === "matchclass";
      const confirmLabel = tab.action === "open" ? "Open"
        : tab.action === "new" ? "Create"
        : tab.action === "save" ? "Save"
        : isClassNavigator ? "Add"
        : "Save As";
      const pathLabel = isClassNavigator ? "Class Path" : "Path";
      const fileLabel = isClassNavigator ? "Class" : "File Name";
      const pathValue = escapeHtml(tab.currentDir || tab.browserPath || "");
      tableWrapEl.innerHTML = `
        <div class="navigator-wrap">
          <div class="navigator-panel">
            <div class="navigator-bar">
              <div class="navigator-inputs">
                <div class="navigator-field">
                  <div class="navigator-row">
                    <span class="navigator-label">${pathLabel}</span>
                    <div class="navigator-actions">
                      <button class="quick-button" id="navUpBtn"><span class="icon">&#8593;</span>up</button>
                    </div>
                    <input type="text" id="navPathInput" value="${pathValue}">
                    <div class="navigator-actions">
                      <button class="quick-button" id="navRefreshBtn"><span class="icon">&#8635;</span>navigate</button>
                    </div>
                  </div>
                </div>
                <div class="navigator-field">
                  <div class="navigator-row">
                    <span class="navigator-label">${fileLabel}</span>
                    <input type="text" id="navFileInput" value="${escapeHtml(tab.fileName || "")}" placeholder="${isClassNavigator ? "class name" : "file name"}">
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
                <span class="name">${entry.is_dir ? "▸" : "•"} ${escapeHtml(entry.name)}${isClassNavigator && !entry.is_dir ? ` (${Number(entry.parameter_count || 0)})` : ""}</span>
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
	          fileInput.value = (entryPath || "").split("/").pop() || "";
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
      if (tab.action === "findclass" || tab.action === "matchclass") {
        await addClassParameters(path);
        closeTab(tab.id, { preserveStatus: true });
        return;
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

    async function addClassParameters(classPath) {
      const response = await fetch("/api/classparams", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path: classPath })
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Class parameter load failed");
      }

      const importedRows = (payload.rows || []).map((row) => cloneRow(normalizeRow({ ...row, id: "" })));
      const addRow = cloneRow(normalizeRow({
        kind: "parameter",
        id: "",
        enabled: true,
        group: "lilak",
        name: "add",
        value: payload.class_name || classPath.split("/").pop() || "",
        unit: "",
        comment: ""
      }));
      const addedRows = [addRow, ...importedRows];

      let tab = currentEditorTab();
      if (!tab) {
        const duplicateKeys = duplicateParameterKeys(addedRows);
        createTabFromPayload({
          name: `configure_${payload.class_name || "class"}.mac`,
          path: "",
          rows: addedRows,
          content: ""
        });
        const newTab = currentEditorTab();
        if (newTab) {
          setSelectedRows(addedRows.map((row) => row.id), addedRows[addedRows.length - 1]?.id || null);
        }
        if (duplicateKeys.length > 0) {
          setStatus(`Duplicate parameter: ${duplicateKeys.join(", ")}`, "error");
        } else {
          setStatus(`Created new parameter file from ${payload.class_name}`, "ok");
        }
        render();
        if (state.runPanelView === "configure") {
          await configureCurrentTab();
        }
        return;
      }

      const duplicateKeys = duplicateParameterKeys([...(tab.rows || []), ...addedRows]);
      tab.rows.push(...addedRows);
      updateDirty(tab);
      tab.rawDraft = tab.content;
      state.activeTabId = tab.id;
      setSelectedRows(addedRows.map((row) => row.id), addedRows[addedRows.length - 1]?.id || null);
      render();
      if (state.runPanelView === "configure") {
        await configureCurrentTab();
        if (duplicateKeys.length > 0) {
          setStatus(`Duplicate parameter: ${duplicateKeys.join(", ")}`, "error");
        }
        return;
      }
      if (duplicateKeys.length > 0) {
        setStatus(`Duplicate parameter: ${duplicateKeys.join(", ")}`, "error");
      } else {
        setStatus(`Added ${addedRows.length} row${addedRows.length !== 1 ? "s" : ""} from ${payload.class_name}`, "ok");
      }
    }

    function renderRow(row, visualIndex, duplicateIds = new Set()) {
      const selected = state.selectedRowIds.includes(row.id) ? "selected" : "";
      const commentClass = row.kind === "comment" ? "comment-row" : "";
      const isCommentedParameter = row.kind === "parameter" && (row.unit || "").trim() === "#";
      const isDuplicateParameter = row.kind === "parameter" && duplicateIds.has(row.id);
      const commentedParameterClass = isCommentedParameter ? "commented-parameter-row" : "";
      const duplicateParameterClass = isDuplicateParameter ? "duplicate-parameter-row" : "";
      const duplicateStateClass = isDuplicateParameter
        ? (isCommentedParameter ? "commented-duplicate-parameter-row" : "active-duplicate-parameter-row")
        : "";
      const checked = state.selectedRowIds.includes(row.id) ? "checked" : "";
      if (row.kind === "comment") {
        return `
          <tr class="${selected} ${commentClass} ${commentedParameterClass} ${duplicateParameterClass} ${duplicateStateClass}" data-row-id="${row.id}">
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
          <tr class="${selected} ${commentClass} ${commentedParameterClass} ${duplicateParameterClass} ${duplicateStateClass}" data-row-id="${row.id}">
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
        <tr class="${selected} ${commentClass} ${commentedParameterClass} ${duplicateParameterClass} ${duplicateStateClass}" data-row-id="${row.id}">
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
      renderMenuPanels();
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
        tab.rows = rowsInCurrentViewOrder(tab);
        updateDirty(tab);
      }
      const response = await fetch("/api/save", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(
          state.viewMode === "raw"
            ? { path, content: tab.rawDraft ?? tab.content ?? "" }
            : { path, rows: rowsInCurrentViewOrder(tab) }
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

    function closeTab(tabId, options = {}) {
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
      if (!options.preserveStatus) {
        setStatus("Tab closed");
      }
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
      const tab = currentEditorTab();
      await createNavigatorTab("findclass", tab?.id || null);
    }

    function renderConfigureReportHtml(payload) {
      const classes = payload.classes || [];
      const inputBranches = payload.input_branches || [];
      const configureLog = payload.configure_log || {};
      const inputFilePath = configureLog.input_file || "";
      const inputFileLabel = inputFilePath ? inputFilePath.split("/").pop() : "input";
      const inputLegendLabel = inputFilePath ? `input file: ${inputFileLabel}` : "input file: none";
      if (classes.length === 0) {
        return '<div class="configure-report"><div class="configure-empty">No classes found in lilak/add</div></div>';
      }
      const inputNodes = [];
      const branchNodes = [];
      const classNodes = [];
      const inputIndex = new Map();
      const branchIndex = new Map();
      const classIndex = new Map();
      const classEdges = [];
      const inputEdges = [];
      const ensureInputNode = (branchName, branchClassName, branchDocUrl) => {
        const key = `${branchName}::${branchClassName}`;
        if (!inputIndex.has(key)) {
          inputIndex.set(key, inputNodes.length);
          inputNodes.push({
            key,
            branch_name: branchName,
            branch_class_name: branchClassName,
            branch_doc_url: branchDocUrl || "#",
          });
        }
        return inputIndex.get(key);
      };
      const ensureBranchNode = (branchName, branchClassName, branchDocUrl) => {
        const key = `${branchName}::${branchClassName}`;
        if (!branchIndex.has(key)) {
          branchIndex.set(key, branchNodes.length);
          branchNodes.push({
            key,
            branch_name: branchName,
            branch_class_name: branchClassName,
            branch_doc_url: branchDocUrl || "#",
          });
        }
        return branchIndex.get(key);
      };
      const ensureClassNode = (className, classDocUrl, classKind) => {
        if (!classIndex.has(className)) {
          classIndex.set(className, classNodes.length);
          classNodes.push({
            class_name: className,
            class_doc_url: classDocUrl || "#",
            class_kind: classKind || "class",
          });
        }
        return classIndex.get(className);
      };
      for (const entry of classes) {
        const classNodeIndex = ensureClassNode(entry.class_name, entry.doc_url, entry.class_kind);
        for (const item of (entry.uses || [])) {
          const branchNodeIndex = ensureBranchNode(item.branch_name, item.class_name, item.doc_url);
          classEdges.push({
            branch_index: branchNodeIndex,
            class_index: classNodeIndex,
            direction: item.action === "keep" ? "keep" : "read",
          });
        }
        for (const item of (entry.saves || [])) {
          if (item.action === "keep") {
            continue;
          }
          const branchNodeIndex = ensureBranchNode(item.branch_name, item.class_name, item.doc_url);
          classEdges.push({
            branch_index: branchNodeIndex,
            class_index: classNodeIndex,
            direction: "write",
          });
        }
      }
      for (const item of inputBranches) {
        const inputNodeIndex = ensureInputNode(item.branch_name, item.class_name, item.doc_url);
        const branchNodeIndex = branchIndex.get(`${item.branch_name}::${item.class_name}`);
        if (branchNodeIndex !== undefined) {
          inputEdges.push({
            input_index: inputNodeIndex,
            branch_index: branchNodeIndex,
            direction: "input-match",
          });
        }
      }
      if (classEdges.length === 0 && inputEdges.length === 0) {
        return '<div class="configure-report"><div class="configure-empty">No branch relations found for classes in lilak/add</div></div>';
      }
      const inputNodeWidth = 180;
      const branchNodeWidth = 180;
      const classNodeWidth = 180;
      const nodeHeight = 38;
      const topPadding = 10;
      const sidePadding = 12;
      const columnGap = 120;
      const verticalGap = 14;
      const rowCount = Math.max(inputNodes.length, branchNodes.length, classNodes.length);
      const graphHeight = Math.max(
        72,
        topPadding * 2 + rowCount * nodeHeight + Math.max(0, rowCount - 1) * verticalGap
      );
      const graphWidth = sidePadding * 2 + inputNodeWidth + columnGap + branchNodeWidth + columnGap + classNodeWidth + 20;
      const inputPositions = inputNodes.map((_, index) => ({
        x: sidePadding,
        y: topPadding + index * (nodeHeight + verticalGap),
      }));
      const branchPositions = branchNodes.map((_, index) => ({
        x: sidePadding + inputNodeWidth + columnGap,
        y: topPadding + index * (nodeHeight + verticalGap),
      }));
      const classPositions = classNodes.map((_, index) => ({
        x: graphWidth - sidePadding - classNodeWidth,
        y: topPadding + index * (nodeHeight + verticalGap),
      }));
      const readMarkerId = `configureArrowRead_${Math.random().toString(36).slice(2, 10)}`;
      const writeMarkerId = `configureArrowWrite_${Math.random().toString(36).slice(2, 10)}`;
      const inputEdgeSlots = inputNodes.map(() => ({ match: 0 }));
      const branchInputEdgeSlots = branchNodes.map(() => ({ match: 0 }));
      for (const edge of inputEdges) {
        inputEdgeSlots[edge.input_index].match += 1;
        branchInputEdgeSlots[edge.branch_index].match += 1;
      }
      const branchEdgeSlots = branchNodes.map(() => ({ read: 0, write: 0, keep: 0 }));
      const classEdgeSlots = classNodes.map(() => ({ read: 0, write: 0, keep: 0 }));
      for (const edge of classEdges) {
        branchEdgeSlots[edge.branch_index][edge.direction] += 1;
        classEdgeSlots[edge.class_index][edge.direction] += 1;
      }
      const inputEdgeUsed = inputNodes.map(() => ({ match: 0 }));
      const branchInputEdgeUsed = branchNodes.map(() => ({ match: 0 }));
      const inputEdgeLayouts = inputEdges.map((edge) => {
        const iTotal = inputEdgeSlots[edge.input_index].match;
        const bTotal = branchInputEdgeSlots[edge.branch_index].match;
        const iIndex = inputEdgeUsed[edge.input_index].match++;
        const bIndex = branchInputEdgeUsed[edge.branch_index].match++;
        const spread = 7;
        return {
          inputOffset: (iIndex - (iTotal - 1) / 2) * spread,
          branchOffset: (bIndex - (bTotal - 1) / 2) * spread,
        };
      });
      const branchEdgeUsed = branchNodes.map(() => ({ read: 0, write: 0, keep: 0 }));
      const classEdgeUsed = classNodes.map(() => ({ read: 0, write: 0, keep: 0 }));
      const edgeLayouts = classEdges.map((edge) => {
        const edgeType = edge.direction === "keep" ? "keep" : edge.direction;
        const bTotal = branchEdgeSlots[edge.branch_index][edgeType];
        const cTotal = classEdgeSlots[edge.class_index][edgeType];
        const bIndex = branchEdgeUsed[edge.branch_index][edgeType]++;
        const cIndex = classEdgeUsed[edge.class_index][edgeType]++;
        const spread = 7;
        const branchOffset = (bIndex - (bTotal - 1) / 2) * spread;
        const classOffset = (cIndex - (cTotal - 1) / 2) * spread;
        return { branchOffset, classOffset };
      });
      return `
        <div class="configure-report">
          <div class="configure-legend">
            <span class="configure-legend-item"><span class="configure-legend-line input-match"></span><span>input: run log branch matched to parameter branch</span></span>
            <span class="configure-legend-item"><span>${escapeHtml(inputLegendLabel)}</span></span>
            <span class="configure-legend-item"><span class="configure-legend-line read arrow"></span><span>read: branch to class</span></span>
            <span class="configure-legend-item"><span class="configure-legend-line write arrow"></span><span>write: class to branch</span></span>
            <span class="configure-legend-item"><span class="configure-legend-line keep"></span><span>keep: branch shared/kept</span></span>
          </div>
          <div class="configure-graph" style="width:${graphWidth}px; height:${graphHeight}px;">
            <svg class="configure-svg" viewBox="0 0 ${graphWidth} ${graphHeight}" preserveAspectRatio="none">
              <defs>
                <marker id="${readMarkerId}" markerWidth="8" markerHeight="6" refX="6" refY="3" orient="auto" markerUnits="strokeWidth">
                  <path d="M0,0 L6,3 L0,6 z" fill="#4f82b4"></path>
                </marker>
                <marker id="${writeMarkerId}" markerWidth="8" markerHeight="6" refX="6" refY="3" orient="auto" markerUnits="strokeWidth">
                  <path d="M0,0 L6,3 L0,6 z" fill="#d07a2d"></path>
                </marker>
              </defs>
              ${inputEdges.map((edge, edgeIndex) => {
                const layout = inputEdgeLayouts[edgeIndex];
                const inputPos = inputPositions[edge.input_index];
                const branchPos = branchPositions[edge.branch_index];
                const edgeGap = 8;
                const inputX = inputPos.x + inputNodeWidth + edgeGap;
                const inputY = inputPos.y + nodeHeight / 2 + layout.inputOffset;
                const branchX = branchPos.x - edgeGap;
                const branchY = branchPos.y + nodeHeight / 2 + layout.branchOffset;
                return `<line class="configure-edge input-match" x1="${inputX}" y1="${inputY}" x2="${branchX}" y2="${branchY}"></line>`;
              }).join("")}
              ${classEdges.map((edge, edgeIndex) => {
                const layout = edgeLayouts[edgeIndex];
                const branchPos = branchPositions[edge.branch_index];
                const classPos = classPositions[edge.class_index];
                const edgeGap = 8;
                const branchX = branchPos.x + branchNodeWidth + edgeGap;
                const branchY = branchPos.y + nodeHeight / 2 + layout.branchOffset;
                const classX = classPos.x - edgeGap;
                const classY = classPos.y + nodeHeight / 2 + layout.classOffset;
                if (edge.direction === "read") {
                  return `<line class="configure-edge read" marker-end="url(#${readMarkerId})" x1="${branchX}" y1="${branchY}" x2="${classX}" y2="${classY}"></line>`;
                }
                if (edge.direction === "keep") {
                  return `<line class="configure-edge keep" x1="${branchX}" y1="${branchY}" x2="${classX}" y2="${classY}"></line>`;
                }
                return `<line class="configure-edge write" marker-end="url(#${writeMarkerId})" x1="${classX}" y1="${classY}" x2="${branchX}" y2="${branchY}"></line>`;
              }).join("")}
            </svg>
            ${inputNodes.map((node, index) => `
              <div class="configure-node input" style="left:${inputPositions[index].x}px; top:${inputPositions[index].y}px; width:${inputNodeWidth}px; height:${nodeHeight}px;">
                <div class="configure-node-title">${escapeHtml(node.branch_name || "")}</div>
                <div class="configure-node-subtitle">
                  <a href="${escapeHtml(node.branch_doc_url || "#")}" target="_blank" rel="noopener noreferrer">${escapeHtml(node.branch_class_name || "")}</a>
                </div>
              </div>
            `).join("")}
            ${branchNodes.map((node, index) => `
              <div class="configure-node branch" style="left:${branchPositions[index].x}px; top:${branchPositions[index].y}px; width:${branchNodeWidth}px; height:${nodeHeight}px;">
                <div class="configure-node-head">
                  <div class="configure-node-title">${escapeHtml(node.branch_name || "")}</div>
                  <button class="configure-add-button" data-config-add="1" data-branch-name="${escapeHtml(node.branch_name || "")}" data-branch-class="${escapeHtml(node.branch_class_name || "")}">add</button>
                </div>
                <div class="configure-node-subtitle">
                  <a href="${escapeHtml(node.branch_doc_url || "#")}" target="_blank" rel="noopener noreferrer">${escapeHtml(node.branch_class_name || "")}</a>
                </div>
              </div>
            `).join("")}
            ${classNodes.map((node, index) => `
              <div class="configure-node class" style="left:${classPositions[index].x}px; top:${classPositions[index].y}px; width:${classNodeWidth}px; height:${nodeHeight}px;">
                <div class="configure-node-title">
                  <a href="${escapeHtml(node.class_doc_url || "#")}" target="_blank" rel="noopener noreferrer">${escapeHtml(node.class_name || "")}</a>
                </div>
                <div class="configure-node-subtitle">${escapeHtml(node.class_kind || "class")}</div>
              </div>
            `).join("")}
          </div>
        </div>
      `;
    }

    async function configureCurrentTab() {
      const tab = currentEditorTab();
      if (!tab) {
        throw new Error("No editor tab selected");
      }
      const response = await fetch("/api/configure_report", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          content: currentRunContent(tab),
          path: tab.path || ""
        })
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Configure load failed");
      }
      state.runOutput = renderConfigureReportHtml(payload);
      state.runOutputIsHtml = true;
      state.runPanelView = "configure";
      renderMenuPanels();
      setStatus(`Loaded configure view for ${payload.count || 0} class${(payload.count || 0) !== 1 ? "es" : ""}`, "ok");
    }

    async function runCurrentTab() {
      const tab = currentEditorTab();
      if (!tab) {
        throw new Error("No editor tab selected");
      }
      setStatus("Running current parameter file...");

      const content = currentRunContent(tab);

      const response = await fetch("/api/run", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          path: tab.path || "",
          content
        })
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Run failed");
      }

      const blocks = [];
      if (payload.command) {
        blocks.push(`$ ${payload.command}`);
      }
      if (payload.stdout) {
        blocks.push(payload.stdout.trimEnd());
      }
      if (payload.stderr) {
        blocks.push(payload.stderr.trimEnd());
      }
      state.runOutput = blocks.filter(Boolean).join("\\n\\n");
      state.runOutputIsHtml = false;
      state.runPanelView = "run";
      state.runStatusOutput = state.runOutput;
      const summary = payload.log_info?.summary || "";
      state.hasRunResult = true;
      state.runStatusMessage = summary || (payload.returncode === 0 ? "Run finished" : `Run failed (${payload.returncode})`);
      tab.lastRunContent = content;
      renderMenuPanels();
      const failure = isRunStatusFailure(summary, payload.returncode);
      if (summary) {
        setStatus(summary, failure ? "error" : "ok");
      } else {
        setStatus(payload.returncode === 0 ? "Run finished" : `Run failed (${payload.returncode})`, failure ? "error" : "ok");
      }
    }

    function showRunStatus() {
      if (!state.hasRunResult) {
        throw new Error("No run result available");
      }
      state.runOutput = state.runStatusOutput || "";
      state.runOutputIsHtml = false;
      state.runPanelView = "status";
      renderMenuPanels();
      setStatus(state.runStatusMessage || "Status loaded", isRunStatusFailure(state.runStatusMessage, 0) ? "error" : "ok");
    }

    function toggleRunPanelExpanded() {
      state.runPanelExpanded = !state.runPanelExpanded;
      renderMenuPanels();
      setStatus(state.runPanelExpanded ? "Run panel expanded" : "Run panel collapsed", "ok");
    }

    async function fetchRunLogInfo() {
      const response = await fetch("/api/run_log_info", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({})
      });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || "Failed to load run log info");
      }
      return payload;
    }

    async function showRunLog() {
      const payload = await fetchRunLogInfo();
      state.runOutput = payload.content || "";
      state.runOutputIsHtml = false;
      state.runPanelView = "log";
      renderMenuPanels();
      setStatus(payload.summary || "Log loaded", "ok");
    }

    async function showRunInfo() {
      const payload = await fetchRunLogInfo();
      const parsed = payload.parsed || {};
      const lines = [
        `Status: ${payload.summary || ""}`,
        `Log: ${payload.resolved_path || payload.link_target || ""}`,
        parsed.RunName ? `RunName: ${parsed.RunName}` : "",
        parsed.RunID ? `RunID: ${parsed.RunID}` : "",
        parsed.Tag ? `Tag: ${parsed.Tag}` : "",
        parsed.Initialized ? `Initialized: ${parsed.Initialized}` : "",
        parsed.num_events ? `Events: ${parsed.num_events}` : "",
        parsed.run_time ? `RunTime: ${parsed.run_time}` : "",
      ].filter(Boolean);
      state.runOutput = lines.join("\\n");
      state.runOutputIsHtml = false;
      state.runPanelView = "info";
      renderMenuPanels();
      setStatus(payload.summary || "Info loaded", "ok");
    }

    async function showRunFiles() {
      const payload = await fetchRunLogInfo();
      const parsed = payload.parsed || {};
      const lines = [
        parsed.InputFile ? `InputFile: ${parsed.InputFile}` : "InputFile: ",
        parsed.OutputFile ? `OutputFile: ${parsed.OutputFile}` : "OutputFile: ",
      ];
      state.runOutput = lines.join("\\n");
      state.runOutputIsHtml = false;
      state.runPanelView = "file";
      renderMenuPanels();
      setStatus(payload.summary || "File info loaded", "ok");
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
              case "run-task":
                await findFile();
                break;
              case "run-configure":
                await configureCurrentTab();
                break;
              case "run-current":
                await runCurrentTab();
                break;
              case "run-expand":
                toggleRunPanelExpanded();
                break;
              case "run-status":
                showRunStatus();
                break;
              case "run-log":
                await showRunLog();
                break;
              case "run-info":
                await showRunInfo();
                break;
              case "run-file":
                await showRunFiles();
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

    function isEditableEventTarget(target) {
      if (!target) {
        return false;
      }
      const tagName = (target.tagName || "").toLowerCase();
      if (tagName === "input" || tagName === "textarea" || tagName === "select") {
        return true;
      }
      if (typeof target.isContentEditable === "boolean" && target.isContentEditable) {
        return true;
      }
      return false;
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
        return;
      }
      if (!event.metaKey && !event.ctrlKey && !event.altKey && (event.key === "Delete" || event.key === "Backspace")) {
        if (isEditableEventTarget(event.target)) {
          return;
        }
        const tab = currentEditorTab();
        const selectedIds = selectedRowIdsForTab(tab);
        if (!tab || selectedIds.length === 0) {
          return;
        }
        event.preventDefault();
        removeSelectedRow();
      }
    });

    bootstrap().catch((error) => setStatus(error.message, "warn"));
  </script>
</body>
</html>
"""


class ParameterEditorServer(ThreadingHTTPServer):
    def __init__(self, server_address, initial_file: Optional[Path]):
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

    def _send_file(self, path: Path):
        body = path.read_bytes()
        mime_type, _ = mimetypes.guess_type(str(path))
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", (mime_type or "application/octet-stream"))
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/":
            self._send_html(HTML_PAGE)
            return
        if parsed.path.startswith("/doc/"):
            relative_path = parsed.path[len("/doc/"):].lstrip("/")
            doc_root = lilak_root_path() / "doc"
            target = (doc_root / relative_path).resolve()
            if not str(target).startswith(str(doc_root.resolve())) or not target.is_file():
                self._send_json({"error": "Not found"}, status=HTTPStatus.NOT_FOUND)
                return
            self._send_file(target)
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

            if parsed.path == "/api/run":
                content = payload.get("content", "")
                path_hint = payload.get("path", "")
                self._send_json(run_lilak_with_content(content, path_hint))
                return

            if parsed.path == "/api/run_log_info":
                self._send_json(read_last_lk_output_log_info())
                return

            if parsed.path == "/api/find":
                matches = find_matching_files(payload.get("query", ""))
                self._send_json({"matches": matches})
                return

            if parsed.path == "/api/classdir":
                self._send_json(list_class_directory(payload.get("path", "")))
                return

            if parsed.path == "/api/classparams":
                self._send_json(load_class_parameter_payload(payload.get("path", "")))
                return

            if parsed.path == "/api/classmatch":
                self._send_json(
                    find_matching_classes_for_branch(
                        payload.get("branch_name", ""),
                        payload.get("class_name", ""),
                    )
                )
                return

            if parsed.path == "/api/configure_report":
                self._send_json(build_configure_report(payload.get("content", ""), payload.get("path", "")))
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
        except Exception as error:
            self._send_json({"error": str(error)}, status=HTTPStatus.INTERNAL_SERVER_ERROR)
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
    parser.add_argument("--configure-report", action="store_true", help="Print terminal configure report for the given parameter file and exit")
    parser.add_argument("--make-run", action="store_true", help="Create a run parameter file from an input ROOT file and exit")
    parser.add_argument("--output", help="Output path for --make-run")
    args = parser.parse_args()

    initial_file = None
    if args.file:
        try:
            if args.make_run:
                initial_file = Path(os.path.expanduser(os.path.expandvars(args.file))).resolve()
                if not initial_file.is_file():
                    raise FileNotFoundError(args.file)
            else:
                initial_file = resolve_input_path(args.file)
        except FileNotFoundError:
            print(f"Cannot find parameter file: {args.file}", file=sys.stderr)
            return 1

    if args.make_run:
        if initial_file is None:
            print("ROOT file is required for --make-run", file=sys.stderr)
            return 1
        output_path = None
        if args.output:
            output_path = Path(os.path.expanduser(os.path.expandvars(args.output)))
            if not output_path.is_absolute():
                output_path = (Path.cwd() / output_path).resolve()
        created = create_run_parameter_file(initial_file, output_path)
        print(created)
        return 0

    if args.configure_report:
        if initial_file is None:
            print("Parameter file is required for --configure-report", file=sys.stderr)
            return 1
        content = initial_file.read_text(encoding="utf-8")
        report = build_configure_report(content, str(initial_file))
        print(format_terminal_configure_report(report), end="")
        return 0

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
