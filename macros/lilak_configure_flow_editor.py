#!/usr/bin/env python3

import argparse
import json
import os
import re
import socket
import sys
import tempfile
import time
import urllib.parse
import webbrowser
from copy import deepcopy
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from lilak_parameter_editor import (  # type: ignore
    build_configure_report,
    class_kind_for_name,
    doc_url_for_class,
    find_matching_classes_for_branch,
    list_directory,
    lilak_root_path,
    load_class_parameter_payload,
    make_root_parameter_payload,
    normalize_searchrun_mfm_rows,
    parse_branch_parameter_file,
    parse_parameter_text,
    read_named_class_log,
    serialize_rows,
)
from lilak_web_format import build_lilak_web_page


GRAPH_COLUMN_X = {
    "run": 36,
    "parameter_container": 36,
    "input_file": 250,
    "branch": 510,
    "class": 830,
    "output_file": 1170,
}


RUN_TEMPLATE_PATH = lilak_root_path() / "meta" / "parameters" / "configure_LKRun.mac"


def header_path_for_class(class_name):
    class_name = (class_name or "").strip()
    if not class_name:
        return ""
    root = lilak_root_path()
    search_roots = [
        root / "source",
        root / "stark",
        root / "atomx",
        root / "ko2421",
    ]
    for base in search_roots:
        if not base.is_dir():
            continue
        for suffix in (".h", ".hh", ".hpp"):
            matches = list(base.rglob(f"{class_name}{suffix}"))
            if matches:
                return str(matches[0])
    return ""


def make_blank_run_payload():
    if RUN_TEMPLATE_PATH.is_file():
        content = RUN_TEMPLATE_PATH.read_text(encoding="utf-8")
        return {
            "name": "configure_flow.conf",
            "path": "",
            "content": content,
            "rows": parse_parameter_text(content),
            "saved_at": time.strftime("%H:%M:%S"),
        }
    return {
        "name": "configure_flow.conf",
        "path": "",
        "content": "",
        "rows": [],
        "saved_at": time.strftime("%H:%M:%S"),
    }


def detect_source_kind(initial_path: Path | None):
    if initial_path is None:
        return "blank"
    suffix = initial_path.suffix.lower()
    if suffix in {".mac", ".conf", ".par"}:
        return "parameter"
    if suffix == ".root":
        return "root"
    return "mfm"


def full_name_of_row(row):
    if row.get("kind") != "parameter":
        return ""
    group = (row.get("group") or "").strip().strip("/")
    name = (row.get("name") or "").strip()
    if not name:
        return ""
    return f"{group}/{name}" if group else name


def clone_rows(rows):
    return json.loads(json.dumps(rows))


def strip_non_parameter_rows(rows):
    return [deepcopy(row) for row in rows if row.get("kind") == "parameter"]


def deduce_class_parameter_bundle(class_name, all_rows):
    try:
        payload = load_class_parameter_payload(class_name)
        default_rows = [
            row for row in strip_non_parameter_rows(payload.get("rows", []))
            if (row.get("group") or "").strip() != "persistency"
        ]
    except FileNotFoundError:
        default_rows = []

    current_by_name = {}
    for row in all_rows:
        if row.get("kind") != "parameter":
            continue
        current_by_name[full_name_of_row(row)] = deepcopy(row)

    merged_rows = []
    used_keys = set()
    known_groups = set()
    for row in default_rows:
        merged = deepcopy(row)
        key = full_name_of_row(row)
        if key:
            used_keys.add(key)
        if row.get("group"):
            known_groups.add(row.get("group"))
        if key in current_by_name:
            current = current_by_name[key]
            merged["value"] = current.get("value", merged.get("value", ""))
            merged["enabled"] = current.get("enabled", merged.get("enabled", True))
            merged["unit"] = current.get("unit", merged.get("unit", ""))
            if current.get("comment"):
                merged["comment"] = current.get("comment")
        merged_rows.append(merged)

    extra_rows = []
    for row in all_rows:
        if row.get("kind") != "parameter":
            continue
        key = full_name_of_row(row)
        group = (row.get("group") or "").strip()
        if key in used_keys:
            continue
        if group in {"persistency", "lilak", "LKRun"}:
            continue
        if group == class_name:
            extra_rows.append(deepcopy(row))

    branch_file = lilak_root_path() / "meta" / "parameters" / f"branch_{class_name}.par"
    branch_entries = parse_branch_parameter_file(branch_file)
    uses = []
    saves = []
    for entry in branch_entries:
        item = {
                            "branch_name": entry["branch_name"],
                            "class_name": entry["class_name"],
                            "action": entry["action"],
                            "doc_url": doc_url_for_class(entry["class_name"]),
                            "header_path": header_path_for_class(entry["class_name"]),
                        }
        if entry["action"] in {"get", "keep"}:
            uses.append(item)
        if entry["action"] in {"register", "keep"}:
            saves.append(item)

    return {
        "class_name": class_name,
        "class_kind": class_kind_for_name(class_name),
        "doc_url": doc_url_for_class(class_name),
        "header_path": header_path_for_class(class_name),
        "parameter_rows": merged_rows,
        "extra_rows": extra_rows,
        "uses": uses,
        "saves": saves,
    }


def build_run_rows(all_rows):
    template_rows = []
    if RUN_TEMPLATE_PATH.is_file():
        template_rows = strip_non_parameter_rows(parse_parameter_text(RUN_TEMPLATE_PATH.read_text(encoding="utf-8")))
    run_template_rows = [row for row in template_rows if (row.get("group") or "").strip() == "LKRun"]
    control_template_rows = [
        row
        for row in template_rows
        if (row.get("group") or "").strip() == "lilak" and (row.get("name") or "").strip() != "add"
    ]
    current_by_name = {full_name_of_row(row): deepcopy(row) for row in all_rows if row.get("kind") == "parameter"}

    merged_rows = []
    used = set()
    for row in run_template_rows:
        key = full_name_of_row(row)
        merged = deepcopy(row)
        if key in current_by_name:
            current = current_by_name[key]
            merged["value"] = current.get("value", merged.get("value", ""))
            merged["enabled"] = current.get("enabled", merged.get("enabled", True))
            merged["unit"] = current.get("unit", merged.get("unit", ""))
            if current.get("comment"):
                merged["comment"] = current.get("comment")
        merged_rows.append(merged)
        used.add(key)

    for row in all_rows:
        if row.get("kind") != "parameter":
            continue
        if (row.get("group") or "").strip() != "LKRun":
            continue
        key = full_name_of_row(row)
        if key in used:
            continue
        merged_rows.append(deepcopy(row))

    control_by_key = {}
    control_order = []
    for row in control_template_rows:
        key = full_name_of_row(row)
        control_order.append(key)
        control_by_key[key] = deepcopy(row)
    for row in all_rows:
        if row.get("kind") != "parameter":
            continue
        if (row.get("group") or "").strip() != "lilak":
            continue
        if (row.get("name") or "").strip() == "add":
            continue
        key = full_name_of_row(row)
        current = deepcopy(row)
        if key not in control_by_key:
            control_order.append(key)
            control_by_key[key] = current
            continue
        previous = control_by_key[key]
        previous_is_comment = previous.get("unit") == "#" or previous.get("enabled") is False
        current_is_enabled = current.get("unit") != "#" and current.get("enabled") is not False
        if previous_is_comment and current_is_enabled:
            control_by_key[key] = current
    control_rows = [control_by_key[key] for key in control_order]
    return merged_rows, control_rows


def classify_branch_style(is_input, read_count, write_count):
    if is_input:
        return "branch_input"
    if write_count > 0 and read_count == 0:
        return "branch_output"
    if read_count > 0 and write_count > 0:
        return "branch_pass"
    if read_count > 0:
        return "branch_pass"
    return "branch_plain"


def root_input_branches(root_path: Path):
    branches = []
    if not root_path.is_file():
        return branches
    try:
        import ROOT  # type: ignore

        root_file = ROOT.TFile.Open(str(root_path))
        if not root_file or root_file.IsZombie():
            return branches
        keys = root_file.GetListOfKeys()
        if keys:
            for key in keys:
                if key.GetClassName() != "TTree":
                    continue
                tree = root_file.Get(key.GetName())
                if not tree:
                    continue
                for branch in tree.GetListOfBranches():
                    class_name = branch.GetClassName() or branch.GetTitle() or ""
                    branches.append(
                        {
                            "branch_name": branch.GetName(),
                            "class_name": class_name,
                            "doc_url": doc_url_for_class(class_name) if class_name else "",
                            "header_path": header_path_for_class(class_name) if class_name else "",
                        }
                    )
        root_file.Close()
    except Exception:
        return branches
    return branches


def append_parameter_row(rows, group, name, value, comment=""):
    rows.append(
        {
            "kind": "parameter",
            "enabled": True,
            "group": group,
            "name": name,
            "value": str(value),
            "unit": "",
            "comment": comment,
        }
    )


def set_parameter_row(rows, group, name, value, comment=""):
    for row in rows:
        if row.get("kind") != "parameter":
            continue
        if (row.get("group") or "").strip() == group and (row.get("name") or "").strip() == name:
            row["value"] = str(value)
            row["enabled"] = True
            if comment and not row.get("comment"):
                row["comment"] = comment
            return
    append_parameter_row(rows, group, name, value, comment)


def parameter_value(rows, group, name, default=""):
    for row in rows:
        if row.get("kind") != "parameter":
            continue
        if (row.get("group") or "").strip() != group:
            continue
        if (row.get("name") or "").strip() != name:
            continue
        if row.get("unit") == "#" or row.get("enabled") is False:
            continue
        return str(row.get("value") or "").strip().strip('"')
    return default


def run_id_from_mfm_path(path):
    match = re.search(r"(?:^|[/_])run[_-]?0*([0-9]+)", str(path), re.IGNORECASE)
    if match:
        return match.group(1)
    return "0"


def build_graph_model(content, path_hint="", source_kind="blank", source_path=""):
    rows = parse_parameter_text(content)
    if source_kind == "mfm" and source_path:
        set_parameter_row(rows, "LKRun", "RunID", run_id_from_mfm_path(source_path), "run number")
        set_parameter_row(rows, "LKRun", "SearchRun", "mfm", "search input files with LKRun/RunID")
        set_parameter_row(rows, "LKRun", "InputFile", source_path, "input mfm file")
        append_parameter_row(rows, "lilak", "add", "LKGETConversionTask", "auto-added for mfm input")
        append_parameter_row(rows, "LKGETConversionTask", "InputFileName", source_path, "input is provided by LKRun/SearchRun")
        rows[-1]["unit"] = "#"
    if source_kind == "root" and source_path:
        set_parameter_row(rows, "LKRun", "InputFile", source_path, "input root file")
    rows = normalize_searchrun_mfm_rows(rows)
    content = serialize_rows(rows)
    run_rows, control_rows = build_run_rows(rows)
    report = build_configure_report(content, path_hint)

    classes = report.get("classes", [])
    input_branches = report.get("input_branches", [])
    if source_kind == "root" and source_path:
        input_branches = root_input_branches(Path(source_path))
    configure_log = report.get("configure_log", {})
    input_file_path = configure_log.get("input_file") or ""
    if not input_file_path:
        for row in rows:
            if row.get("kind") != "parameter":
                continue
            if (row.get("group") or "").strip() == "LKRun" and (row.get("name") or "").strip() == "InputFile":
                input_file_path = str(row.get("value") or "").strip().strip('"')
                break
    if not input_file_path and source_kind in {"root", "mfm"}:
        input_file_path = source_path
    output_file_path = configure_log.get("output_file") or ""
    input_file_lower = str(input_file_path or "").lower()
    input_file_is_mfm = source_kind == "mfm" or input_file_lower.endswith(".mfm") or ".dat" in input_file_lower
    run_name = parameter_value(rows, "LKRun", "Name", "run")
    run_id = parameter_value(rows, "LKRun", "RunID", "0")
    persistency_by_branch = {}
    for row in rows:
        if row.get("kind") != "parameter":
            continue
        if (row.get("group") or "").strip() != "persistency":
            continue
        branch_name = (row.get("name") or "").strip()
        if not branch_name:
            continue
        persistency_by_branch[branch_name] = str(row.get("value", "")).strip().lower() not in {"0", "false", "no", "off"}

    nodes = []
    edges = []
    branch_map = {}
    class_node_data = {}

    nodes.append(
        {
            "id": "run",
            "kind": "run",
            "label": "Run",
            "subtitle": f"{run_name} / {run_id}",
            "x": GRAPH_COLUMN_X["run"],
            "y": 32,
        }
    )
    nodes.append(
        {
            "id": "parameter_container",
            "kind": "parameter_container",
            "label": "Parameter Container",
            "subtitle": f"{len(rows)} rows",
            "x": GRAPH_COLUMN_X["parameter_container"],
            "y": 180,
        }
    )

    if input_file_path or source_kind in {"root", "mfm"}:
        nodes.append(
            {
                "id": "input_file",
                "kind": "input_file",
                "label": "Input File",
                "subtitle": source_kind,
                "x": GRAPH_COLUMN_X["input_file"],
                "y": 36,
            }
        )

    nodes.append(
        {
            "id": "output_file",
            "kind": "output_file",
            "label": "Output File",
            "subtitle": output_file_path or "not configured yet",
            "x": GRAPH_COLUMN_X["output_file"],
            "y": 36,
        }
    )

    for item in classes:
        class_name = item.get("class_name", "")
        class_node_data[class_name] = deduce_class_parameter_bundle(class_name, rows)
        for use in item.get("uses", []) or []:
            key = f'{use.get("branch_name","")}::{use.get("class_name","")}'
            branch = branch_map.setdefault(
                key,
                {
                    "branch_name": use.get("branch_name", ""),
                    "branch_class_name": use.get("class_name", ""),
                    "doc_url": use.get("doc_url", ""),
                    "header_path": use.get("header_path", "") or header_path_for_class(use.get("class_name", "")),
                    "is_input": False,
                    "read_count": 0,
                    "write_count": 0,
                },
            )
            branch["read_count"] += 1
        for save in item.get("saves", []) or []:
            if save.get("action") == "keep":
                continue
            key = f'{save.get("branch_name","")}::{save.get("class_name","")}'
            branch = branch_map.setdefault(
                key,
                {
                    "branch_name": save.get("branch_name", ""),
                    "branch_class_name": save.get("class_name", ""),
                    "doc_url": save.get("doc_url", ""),
                    "header_path": save.get("header_path", "") or header_path_for_class(save.get("class_name", "")),
                    "is_input": False,
                    "read_count": 0,
                    "write_count": 0,
                },
            )
            branch["write_count"] += 1

    for entry in input_branches:
        key = f'{entry.get("branch_name","")}::{entry.get("class_name","")}'
        branch = branch_map.setdefault(
            key,
            {
                "branch_name": entry.get("branch_name", ""),
                "branch_class_name": entry.get("class_name", ""),
                "doc_url": entry.get("doc_url", ""),
                "header_path": entry.get("header_path", "") or header_path_for_class(entry.get("class_name", "")),
                "is_input": False,
                "read_count": 0,
                "write_count": 0,
            },
        )
        branch["is_input"] = True

    branch_keys = sorted(branch_map.keys(), key=lambda key: branch_map[key]["branch_name"].lower())
    for index, key in enumerate(branch_keys):
        branch = branch_map[key]
        node_id = f"branch_{index}"
        branch["node_id"] = node_id
        branch["style"] = classify_branch_style(branch["is_input"], branch["read_count"], branch["write_count"])
        persist_enabled = persistency_by_branch.get(branch["branch_name"], True)
        nodes.append(
            {
                "id": node_id,
                "kind": "branch",
                "label": branch["branch_name"],
                "subtitle": branch["branch_class_name"],
                "x": GRAPH_COLUMN_X["branch"],
                "y": 36 + index * 92,
                "style": branch["style"],
                "branch_name": branch["branch_name"],
                "branch_class_name": branch["branch_class_name"],
                "doc_url": branch["doc_url"],
                "header_path": branch.get("header_path", ""),
                "persist_disabled": not persist_enabled,
            }
        )
        if branch["is_input"] and any(node["id"] == "input_file" for node in nodes):
            edges.append({"from": "input_file", "to": node_id, "type": "input"})
        if branch["write_count"] > 0 and persist_enabled:
            edges.append({"from": node_id, "to": "output_file", "type": "output"})

    for index, item in enumerate(classes):
        class_name = item.get("class_name", "")
        node_id = f"class_{index}"
        class_data = class_node_data[class_name]
        nodes.append(
            {
                "id": node_id,
                "kind": "class",
                "class_kind": item.get("class_kind", "class"),
                "label": class_name,
                "subtitle": item.get("class_kind", "class"),
                "x": GRAPH_COLUMN_X["class"],
                "y": 36 + index * 112,
                "doc_url": item.get("doc_url", ""),
                "header_path": class_data.get("header_path", "") or header_path_for_class(class_name),
                "class_name": class_name,
            }
        )
        if input_file_is_mfm and class_name == "LKGETConversionTask" and any(node["id"] == "input_file" for node in nodes):
            edges.append({"from": "input_file", "to": node_id, "type": "source"})
        for use in item.get("uses", []) or []:
            key = f'{use.get("branch_name","")}::{use.get("class_name","")}'
            if key in branch_map:
                edges.append({"from": branch_map[key]["node_id"], "to": node_id, "type": "read"})
        for save in item.get("saves", []) or []:
            if save.get("action") == "keep":
                continue
            key = f'{save.get("branch_name","")}::{save.get("class_name","")}'
            if key in branch_map:
                edges.append({"from": node_id, "to": branch_map[key]["node_id"], "type": "write"})

    return {
        "rows": rows,
        "run_rows": run_rows,
        "control_rows": control_rows,
        "report": report,
        "nodes": nodes,
        "edges": edges,
        "class_data": class_node_data,
        "source_kind": source_kind,
        "source_path": source_path,
        "input_file_path": input_file_path,
        "output_file_path": output_file_path,
    }


def inspect_input_file(path_value, source_kind):
    info = {
        "path": path_value,
        "kind": source_kind,
        "keys": [],
        "trees": [],
        "parameter_container": [],
        "run_header": [],
        "drawing_placeholder": "Drawing inspection placeholder is reserved here.",
    }
    if not path_value:
        return info
    path = Path(path_value)
    if source_kind != "root" or not path.is_file():
        return info
    try:
        import ROOT  # type: ignore

        root_file = ROOT.TFile.Open(str(path))
        if not root_file or root_file.IsZombie():
            return info
        keys = root_file.GetListOfKeys()
        if keys:
            for key in keys:
                key_name = key.GetName()
                class_name = key.GetClassName()
                info["keys"].append({"name": key_name, "class_name": class_name})
                if "ParameterContainer" in class_name or "ParameterContainer" in key_name:
                    info["parameter_container"].append(f"{key_name} ({class_name})")
                if "RunHeader" in class_name or "RunHeader" in key_name:
                    info["run_header"].append(f"{key_name} ({class_name})")
                if class_name == "TTree":
                    tree = root_file.Get(key_name)
                    branches = []
                    for branch in tree.GetListOfBranches():
                        branches.append(branch.GetName())
                        if "RunHeader" in branch.GetName():
                            info["run_header"].append(f"{key_name}/{branch.GetName()}")
                        if "ParameterContainer" in branch.GetName():
                            info["parameter_container"].append(f"{key_name}/{branch.GetName()}")
                    info["trees"].append({"name": key_name, "branches": branches})
        root_file.Close()
    except Exception as error:
        info["error"] = str(error)
    return info


def resolve_initial_payload(initial_path):
    if initial_path is None:
        payload = make_blank_run_payload()
        return build_graph_model(payload["content"], "", "blank", "")

    source_kind = detect_source_kind(initial_path)
    if source_kind == "parameter":
        content = initial_path.read_text(encoding="utf-8")
        return build_graph_model(content, str(initial_path), source_kind, str(initial_path))

    generated = make_root_parameter_payload(initial_path)
    configure_path_hint = str((Path.cwd() / generated.get("name", "configure_flow.mac")).resolve())
    return build_graph_model(generated["content"], configure_path_hint, source_kind, str(initial_path))


def save_flow_configuration(payload):
    path_value = (payload.get("path") or "").strip()
    if not path_value:
        target = Path.cwd() / "configure_flow.conf"
    else:
        target = Path(os.path.expanduser(os.path.expandvars(path_value)))
        if not target.is_absolute():
            target = (Path.cwd() / target).resolve()
    target.parent.mkdir(parents=True, exist_ok=True)

    all_rows = payload.get("all_rows", [])
    run_rows = payload.get("run_rows", [])
    control_rows = payload.get("control_rows", [])
    persistency_rows = payload.get("persistency_rows", [])
    class_nodes = payload.get("class_nodes", [])
    class_groups = set()
    for node in class_nodes:
        class_name = (node.get("class_name") or "").strip()
        if class_name:
            class_groups.add(class_name)
        for row in node.get("parameter_rows", []):
            if row.get("kind") == "parameter" and row.get("group"):
                class_groups.add(row.get("group"))

    preserved_rows = []
    for row in all_rows:
        if row.get("kind") != "parameter":
            preserved_rows.append(deepcopy(row))
            continue
        group = (row.get("group") or "").strip()
        if group == "LKRun":
            continue
        if group == "lilak":
            continue
        if group == "persistency":
            continue
        if group in class_groups:
            continue
        preserved_rows.append(deepcopy(row))

    merged_rows = []
    merged_rows.extend(deepcopy(control_rows))
    class_names = [node.get("class_name", "").strip() for node in class_nodes if node.get("class_name")]
    for class_name in class_names:
        merged_rows.append(
            {
                "kind": "parameter",
                "enabled": True,
                "group": "lilak",
                "name": "add",
                "value": class_name,
                "unit": "",
                "comment": "add task or detector class",
            }
        )
    if merged_rows and (run_rows or class_nodes or persistency_rows or preserved_rows):
        merged_rows.append({"kind": "comment", "enabled": False, "comment": ""})
    merged_rows.extend(deepcopy(run_rows))
    if class_nodes:
        merged_rows.append({"kind": "comment", "enabled": False, "comment": ""})
    for node in class_nodes:
        rows = node.get("parameter_rows", [])
        for row in rows:
            merged_rows.append(deepcopy(row))
        if rows:
            merged_rows.append({"kind": "comment", "enabled": False, "comment": ""})
    merged_rows.extend(deepcopy(persistency_rows))
    merged_rows.extend(preserved_rows)

    merged_rows = normalize_searchrun_mfm_rows(merged_rows)
    content = serialize_configuration_rows(group_rows_for_save(merged_rows))
    target.write_text(content, encoding="utf-8")
    return {"path": str(target), "saved_at": time.strftime("%Y-%m-%d %H:%M:%S"), "content": content}


def group_rows_for_save(rows):
    grouped = []
    current_group = None
    for row in rows:
        if row.get("kind") != "parameter":
            continue
        group = (row.get("group") or "").strip()
        grouped.append(deepcopy(row))
        current_group = group
    return grouped


def serialize_configuration_rows(rows):
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

        value = str(row.get("value", "")).strip()
        unit = str(row.get("unit", "")).strip()
        comment = str(row.get("comment", "")).strip()
        is_commented_parameter = unit == "#"
        prefix = "#" if is_commented_parameter else ("" if row.get("enabled", True) else "#")
        group_key = group
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


class ConfigureFlowHandler(BaseHTTPRequestHandler):
    server_version = "LILAKConfigureFlow/1.0"

    def _send_json(self, payload, status=HTTPStatus.OK):
        data = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _send_html(self, html):
        data = html.encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _read_json(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0:
            return {}
        return json.loads(self.rfile.read(length).decode("utf-8"))

    def log_message(self, format, *args):
        return

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/":
            self._send_html(HTML_PAGE)
            return
        if parsed.path == "/api/bootstrap":
            model = resolve_initial_payload(self.server.initial_path)
            model["input_info"] = inspect_input_file(model.get("input_file_path", ""), model.get("source_kind", "blank"))
            model["cwd"] = str(Path.cwd())
            model["common_dir"] = str(lilak_root_path() / "common")
            model["save_path"] = str(self.server.initial_path) if self.server.initial_path and detect_source_kind(self.server.initial_path) == "parameter" else ""
            self._send_json(model)
            return
        self._send_json({"error": "Not found"}, status=HTTPStatus.NOT_FOUND)

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        payload = self._read_json()
        try:
            if parsed.path == "/api/listdir":
                self._send_json(list_directory(payload.get("path", "")))
                return
            if parsed.path == "/api/class_candidates":
                entries = find_matching_classes_for_branch(payload.get("branch_name", ""), payload.get("branch_class_name", ""))
                task_entries = []
                for entry in entries.get("entries", []):
                    class_name = entry.get("name", "")
                    if class_kind_for_name(class_name) != "task":
                        continue
                    uses = []
                    for branch in parse_branch_parameter_file(lilak_root_path() / "meta" / "parameters" / f"branch_{class_name}.par"):
                        if branch.get("action") in {"get", "keep"}:
                            uses.append(
                                {
                                    "branch_name": branch.get("branch_name", ""),
                                    "class_name": branch.get("class_name", ""),
                                    "action": branch.get("action", ""),
                                    "header_path": header_path_for_class(branch.get("class_name", "")),
                                }
                            )
                    entry["uses"] = uses
                    entry["detail"] = ", ".join(
                        f'{use["branch_name"]} ({use["class_name"]})'
                        for use in uses
                        if use.get("branch_name") or use.get("class_name")
                    )
                    entry["kind_label"] = "task"
                    task_entries.append(entry)
                self._send_json({"entries": task_entries})
                return
            if parsed.path == "/api/class_list":
                requested_kind = (payload.get("kind") or "").strip().lower()
                if requested_kind == "task":
                    entries = [{"name": name, "class_kind": "task", "doc_url": doc_url_for_class(name), "header_path": header_path_for_class(name)} for name in read_named_class_log("LKClassList.task.log")]
                elif requested_kind == "detector":
                    entries = []
                    for name in read_named_class_log("LKClassList.detector.log"):
                        kind = class_kind_for_name(name)
                        if kind not in {"detector", "detector_plane"}:
                            continue
                        entries.append({"name": name, "class_kind": kind, "doc_url": doc_url_for_class(name), "header_path": header_path_for_class(name)})
                else:
                    entries = []
                self._send_json({"entries": entries})
                return
            if parsed.path == "/api/class_bundle":
                class_name = (payload.get("class_name") or "").strip()
                if not class_name:
                    raise RuntimeError("class_name is required.")
                self._send_json(deduce_class_parameter_bundle(class_name, payload.get("all_rows", [])))
                return
            if parsed.path == "/api/save":
                self._send_json(save_flow_configuration(payload))
                return
            if parsed.path == "/api/load":
                path_value = (payload.get("path") or "").strip()
                if not path_value:
                    raise RuntimeError("path is required.")
                target = Path(os.path.expanduser(os.path.expandvars(path_value)))
                if not target.is_absolute():
                    target = (Path.cwd() / target).resolve()
                model = resolve_initial_payload(target)
                model["input_info"] = inspect_input_file(model.get("input_file_path", ""), model.get("source_kind", "blank"))
                model["cwd"] = str(Path.cwd())
                model["common_dir"] = str(lilak_root_path() / "common")
                model["save_path"] = str(target) if detect_source_kind(target) == "parameter" else ""
                self._send_json(model)
                return
            if parsed.path == "/api/input_info":
                self._send_json(inspect_input_file(payload.get("path", ""), payload.get("kind", "blank")))
                return
        except Exception as error:
            self._send_json({"error": str(error)}, status=HTTPStatus.BAD_REQUEST)


HTML_PAGE = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>LILAK Configure Flow</title>
  <style>
    :root {
      --bg: #edf4ff;
      --panel: #f8fbff;
      --panel-border: #b7cae7;
      --grid: #d5e0f2;
      --text: #25344b;
      --muted: #60718f;
      --run: #7aa5db;
      --parc: #6f8bb0;
      --input: #6fb6dd;
      --output: #d7a46d;
      --branch-in: #f1c94b;
      --branch-out: #b9bec9;
      --branch-pass: #b9bec9;
      --task: #7fc59a;
      --detector: #c695df;
      --plane: #c695df;
      --selected: #a12626;
      --line: #6c86a8;
      --line-write: #c57532;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      color: var(--text);
      background: var(--bg);
    }
    .shell { display: flex; flex-direction: column; min-height: 100vh; }
    .topbar {
      display: flex; align-items: center; gap: 10px;
      padding: 10px 14px; border-bottom: 1px solid var(--panel-border);
      background: #f3f8ff;
    }
    .title { font-size: 18px; font-weight: 600; margin-right: 10px; }
    .pill {
      border: 1px solid #7f96b4; background: #ffffff; color: #22324d;
      border-radius: 8px; padding: 6px 11px; font-size: 13px; cursor: pointer;
    }
    .pill.primary { border-color: #476b9c; }
    .status { margin-left: auto; font-size: 12px; color: var(--muted); white-space: pre-wrap; text-align: right; max-width: 45vw; }
    .workspace { display: grid; grid-template-columns: minmax(760px, 1fr) 360px; gap: 12px; padding: 12px; flex: 1; min-height: 0; }
    .canvas-panel, .inspector-panel {
      background: var(--panel); border: 1px solid var(--panel-border); border-radius: 14px; min-height: 0;
      display: flex; flex-direction: column; overflow: hidden;
    }
    .panel-title {
      font-size: 14px; font-weight: 600; color: #1f2f49; padding: 10px 12px; border-bottom: 1px solid #d6e3f5;
      display: flex; align-items: center; gap: 8px;
    }
    .canvas-area { position: relative; flex: 1; min-height: 760px; background:
      linear-gradient(to right, transparent 39px, var(--grid) 40px, transparent 41px),
      linear-gradient(to bottom, transparent 39px, var(--grid) 40px, transparent 41px);
      background-size: 40px 40px;
      overflow: hidden;
    }
    svg.edges { position: absolute; inset: 0; width: 100%; height: 100%; pointer-events: none; }
    .node {
      position: absolute; width: 220px; border-radius: 14px; border: 1px solid rgba(0,0,0,0.12);
      background: rgba(255,255,255,0.96); box-shadow: 0 10px 24px rgba(66,96,146,0.14);
      overflow: hidden; cursor: pointer;
    }
    .node.selected { outline: 3px solid var(--selected); z-index: 20; }
    .node-header {
      padding: 8px 10px; color: white; font-size: 14px; font-weight: 600;
      display: flex; justify-content: space-between; align-items: center; gap: 8px;
    }
    .node-sub {
      padding: 8px 10px; font-size: 12px; color: var(--muted); min-height: 46px;
      display: flex; flex-direction: column; justify-content: center; gap: 4px;
    }
    .node-footer { padding: 0 10px 10px 10px; }
    .node button {
      border: 1px solid #748eaf; background: #fff; border-radius: 8px; padding: 4px 8px; font-size: 11px; cursor: pointer;
    }
    .node.run .node-header { background: var(--run); }
    .node.parameter_container .node-header { background: var(--parc); }
    .node.input_file .node-header { background: var(--input); }
    .node.output_file .node-header { background: var(--output); }
    .node.task .node-header { background: var(--task); }
    .node.detector .node-header, .node.class .node-header { background: var(--detector); }
    .node.branch_input .node-header { background: var(--branch-in); color: #2d2b16; }
    .node.branch_output .node-header { background: var(--branch-out); color: #27374f; }
    .node.branch_pass .node-header { background: var(--branch-pass); color: #27374f; }
    .node.branch_plain .node-header { background: #a5b0c5; }
    .node.branch_input_output .node-header { background: var(--branch-in); color: #2d2b16; }
    .inspector-content { flex: 1; overflow: auto; padding: 12px; }
    .section { border: 1px solid #d8e4f5; border-radius: 12px; padding: 10px; background: #fff; margin-bottom: 12px; }
    .section h3 { margin: 0 0 10px 0; font-size: 13px; color: #203149; }
    .meta { font-size: 12px; color: var(--muted); line-height: 1.5; }
    .param-row { display: grid; grid-template-columns: 18px 1fr 1fr; gap: 8px; align-items: center; margin-bottom: 7px; }
    .param-row .name { font-size: 12px; color: #2a3b56; }
    .param-row input[type="text"] {
      width: 100%; border: 1px solid #c1d2eb; border-radius: 8px; padding: 6px 8px; font-size: 12px;
      background: #fbfdff;
    }
    .small-button {
      border: 1px solid #7d96b6; background: #fff; border-radius: 8px; padding: 4px 8px; font-size: 11px; cursor: pointer;
    }
    .raw-box {
      white-space: pre-wrap; font-family: ui-monospace, SFMono-Regular, monospace; font-size: 11px;
      line-height: 1.45; background: #f7faff; border: 1px solid #d4e1f2; border-radius: 10px; padding: 10px; max-height: 340px; overflow: auto;
    }
    .navigator {
      position: fixed; inset: 0; background: rgba(32,44,68,0.25); display: none; align-items: flex-start; justify-content: center; padding-top: 48px;
    }
    .navigator.open { display: flex; }
    .navigator-card {
      width: min(920px, calc(100vw - 32px)); background: #eef5ff; border: 1px solid #b8cae6; border-radius: 16px; padding: 12px;
      box-shadow: 0 18px 40px rgba(42,65,103,0.26);
    }
    .navigator-row { display: grid; grid-template-columns: auto auto auto 1fr auto; gap: 8px; align-items: center; margin-bottom: 8px; }
    .navigator-row.file { grid-template-columns: auto 1fr auto; }
    .navigator-row input {
      border: 1px solid #bfd0ea; border-radius: 8px; padding: 7px 9px; font-size: 13px; background: #fff;
    }
    .nav-list {
      border: 1px solid #c5d5ec; background: #fff; border-radius: 12px; min-height: 360px; max-height: 55vh; overflow: auto; padding: 6px;
    }
    .nav-entry {
      display: flex; justify-content: space-between; align-items: center;
      padding: 8px 10px; border-radius: 8px; cursor: pointer; font-size: 16px;
    }
    .nav-entry:hover { background: #eef5ff; }
    .nav-entry.dir { color: #2f6fb0; }
    .nav-entry.conf { color: #7e4aa5; }
    .nav-entry.file { color: #7f8898; }
    .branch-actions { display: flex; gap: 8px; flex-wrap: wrap; }
    .modal {
      position: fixed; inset: 0; background: rgba(22,34,54,0.34); display: none; align-items: flex-start; justify-content: center; padding-top: 70px;
    }
    .modal.open { display: flex; }
    .modal-card {
      width: min(680px, calc(100vw - 36px)); background: #f5f9ff; border: 1px solid #bfd0ea; border-radius: 16px; padding: 14px;
    }
    .candidate-list { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 12px; }
    .candidate-list button { border: 1px solid #7590af; background: #fff; border-radius: 8px; padding: 7px 10px; cursor: pointer; }
    a { color: #33649d; text-decoration: none; }
    @media (max-width: 1180px) {
      .workspace { grid-template-columns: 1fr; }
      .canvas-area { min-height: 720px; }
    }
  </style>
</head>
<body>
<div class="shell">
  <div class="topbar">
    <div class="title">LILAK Configure Flow</div>
    <button class="pill primary" id="saveButton">Save</button>
    <div class="status" id="statusText"></div>
  </div>
  <div class="workspace">
    <div class="canvas-panel">
      <div class="panel-title">Flow Canvas</div>
      <div class="canvas-area" id="canvasArea">
        <svg class="edges" id="edgeLayer"></svg>
      </div>
    </div>
    <div class="inspector-panel">
      <div class="panel-title">Inspector</div>
      <div class="inspector-content" id="inspectorContent"></div>
    </div>
  </div>
</div>

<div class="navigator" id="navigator">
  <div class="navigator-card">
    <div class="navigator-row">
      <button class="small-button" id="navHome">HOME</button>
      <button class="small-button" id="navCommon">COMMON</button>
      <button class="small-button" id="navUp">Up</button>
      <input type="text" id="navPath" />
      <button class="small-button" id="navGo">Go</button>
    </div>
    <div class="navigator-row file">
      <div>File</div>
      <input type="text" id="navFile" />
      <button class="small-button" id="navSave">Save</button>
    </div>
    <div class="nav-list" id="navList"></div>
  </div>
</div>

<div class="modal" id="taskModal">
  <div class="modal-card">
    <div style="display:flex; justify-content:space-between; align-items:center; gap:8px;">
      <div style="font-weight:600;">Add Task From Branch</div>
      <button class="small-button" id="taskModalClose">Close</button>
    </div>
    <div class="meta" id="taskModalMeta"></div>
    <div class="candidate-list" id="taskCandidateList"></div>
  </div>
</div>

<script>
const state = {
  sourceKind: "blank",
  sourcePath: "",
  savePath: "",
  configurationName: "configure_flow",
  parameterTabs: [],
  activeParameterTabId: "",
  cwd: "",
  commonDir: "",
  rows: [],
  runRows: [],
  controlRows: [],
  nodes: [],
  edges: [],
  classData: {},
  inputInfo: {},
  selectedId: "run",
  navOpen: false,
  pendingSavePath: "",
  taskBranch: null,
};

async function fetchJson(url, payload = null) {
  const options = payload ? {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  } : {};
  const response = await fetch(url, options);
  const data = await response.json();
  if (!response.ok) {
    throw new Error(data.error || "Request failed");
  }
  return data;
}

function setStatus(message) {
  document.getElementById("statusText").textContent = message || "";
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>\"']/g, (ch) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;" }[ch]));
}

function nodeById(id) {
  return state.nodes.find((node) => node.id === id) || null;
}

function classRowsForNode(node) {
  const info = state.classData[node.class_name] || {};
  return info.parameter_rows || [];
}

function inputInfo() {
  return state.inputInfo || {};
}

function renderCanvas() {
  const canvas = document.getElementById("canvasArea");
  const edgeLayer = document.getElementById("edgeLayer");
  canvas.querySelectorAll(".node").forEach((el) => el.remove());
  edgeLayer.innerHTML = `
    <defs>
      <marker id="arrowRead" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L8,3 L0,6 z" fill="#5d7fa8"></path>
      </marker>
      <marker id="arrowWrite" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L8,3 L0,6 z" fill="#c57532"></path>
      </marker>
    </defs>
  `;

  const nodeRects = {};
  for (const node of state.nodes) {
    const el = document.createElement("div");
    const styleKind = node.kind === "class" ? (node.class_kind === "task" ? "task" : "detector") : (node.style || node.kind);
    el.className = `node ${styleKind}`;
    if (state.selectedId === node.id) {
      el.classList.add("selected");
    }
    el.style.left = `${node.x}px`;
    el.style.top = `${node.y}px`;
    const branchAction = node.kind === "branch"
      ? `<button data-action="add-task" data-node-id="${escapeHtml(node.id)}">add task</button>`
      : "";
    const subtitle = node.kind === "branch"
      ? `<a href="${escapeHtml(node.doc_url || "#")}" target="_blank" rel="noopener">${escapeHtml(node.subtitle || "")}</a>`
      : escapeHtml(node.subtitle || "");
    el.innerHTML = `
      <div class="node-header">
        <div>${escapeHtml(node.label)}</div>
      </div>
      <div class="node-sub">${subtitle}</div>
      <div class="node-footer">${branchAction}</div>
    `;
    el.addEventListener("click", (event) => {
      const actionTarget = event.target.closest("button[data-action='add-task']");
      if (actionTarget) {
        openTaskCandidates(actionTarget.dataset.nodeId);
        event.stopPropagation();
        return;
      }
      state.selectedId = node.id;
      render();
    });
    canvas.appendChild(el);
    nodeRects[node.id] = { x: node.x, y: node.y, width: 220, height: el.offsetHeight || 96 };
  }

  edgeLayer.setAttribute("viewBox", `0 0 ${canvas.clientWidth} ${canvas.clientHeight}`);
  for (const edge of state.edges) {
    const from = nodeRects[edge.from];
    const to = nodeRects[edge.to];
    if (!from || !to) continue;
    const x1 = from.x + from.width;
    const y1 = from.y + from.height / 2;
    const x2 = to.x;
    const y2 = to.y + to.height / 2;
    const line = document.createElementNS("http://www.w3.org/2000/svg", "path");
    const midX = (x1 + x2) / 2;
    line.setAttribute("d", `M ${x1} ${y1} C ${midX} ${y1}, ${midX} ${y2}, ${x2} ${y2}`);
    line.setAttribute("fill", "none");
    line.setAttribute("stroke-width", "2.2");
    if (edge.type === "write" || edge.type === "output") {
      line.setAttribute("stroke", "#c57532");
      line.setAttribute("marker-end", "url(#arrowWrite)");
    } else {
      line.setAttribute("stroke", "#6c86a8");
      line.setAttribute("marker-end", "url(#arrowRead)");
    }
    edgeLayer.appendChild(line);
  }
}

function parameterRowsHtml(rows, ownerId, extra = false) {
  return (rows || []).map((row, index) => {
    const name = `${row.group ? `${row.group}/` : ""}${row.name}`;
    return `
      <div class="param-row">
        <input type="checkbox" ${row.enabled === false ? "" : "checked"} data-kind="toggle" data-owner="${escapeHtml(ownerId)}" data-index="${index}" data-extra="${extra ? "1" : "0"}">
        <div class="name">${escapeHtml(name)}</div>
        <input type="text" value="${escapeHtml(row.value || "")}" data-kind="value" data-owner="${escapeHtml(ownerId)}" data-index="${index}" data-extra="${extra ? "1" : "0"}">
      </div>
    `;
  }).join("");
}

function renderInspector() {
  const panel = document.getElementById("inspectorContent");
  const node = nodeById(state.selectedId);
  if (!node) {
    panel.innerHTML = `<div class="meta">Select an item.</div>`;
    return;
  }
  if (node.kind === "run") {
    panel.innerHTML = `
      <div class="section">
        <h3>Run Parameters</h3>
        ${parameterRowsHtml(state.runRows, "run")}
        <button class="small-button" data-action="add-param" data-owner="run">Add Parameter</button>
      </div>
    `;
  } else if (node.kind === "parameter_container") {
    panel.innerHTML = `
      <div class="section">
        <h3>All Parameters</h3>
        <div class="raw-box">${escapeHtml(serializePreview())}</div>
      </div>
    `;
  } else if (node.kind === "input_file") {
    const info = inputInfo();
    panel.innerHTML = `
      <div class="section">
        <h3>Input File</h3>
        <div class="meta">
          path: ${escapeHtml(info.path || node.label)}<br>
          kind: ${escapeHtml(info.kind || state.sourceKind)}
        </div>
      </div>
      <div class="section">
        <h3>Parameter Container</h3>
        <div class="raw-box">${escapeHtml((info.parameter_container || []).join("\n") || "No parameter container information found.")}</div>
      </div>
      <div class="section">
        <h3>Run Header</h3>
        <div class="raw-box">${escapeHtml((info.run_header || []).join("\n") || "No run header information found.")}</div>
      </div>
      <div class="section">
        <h3>ROOT Keys / Trees</h3>
        <div class="raw-box">${escapeHtml(formatInputInfo(info))}</div>
      </div>
      <div class="section">
        <h3>Drawings</h3>
        <div class="meta">${escapeHtml(info.drawing_placeholder || "Reserved.")}</div>
      </div>
    `;
  } else if (node.kind === "output_file") {
    const outputRows = state.runRows.filter((row) => ["OutputPath", "FileName", "Tag", "AutoTerminate", "RunID"].includes(row.name));
    panel.innerHTML = `
      <div class="section">
        <h3>Output File Settings</h3>
        ${parameterRowsHtml(outputRows, "output")}
      </div>
    `;
  } else if (node.kind === "branch") {
    panel.innerHTML = `
      <div class="section">
        <h3>Branch</h3>
        <div class="meta">
          name: ${escapeHtml(node.branch_name || "")}<br>
          class: ${escapeHtml(node.branch_class_name || "")}
        </div>
        <div class="branch-actions" style="margin-top:10px;">
          <button class="small-button" data-action="add-task" data-node-id="${escapeHtml(node.id)}">Add Task</button>
        </div>
      </div>
    `;
  } else if (node.kind === "class") {
    const info = state.classData[node.class_name] || { parameter_rows: [], extra_rows: [] };
    panel.innerHTML = `
      <div class="section">
        <h3>${escapeHtml(node.label)} (${escapeHtml(node.class_kind)})</h3>
        <div class="meta"><a href="${escapeHtml(node.doc_url || "#")}" target="_blank" rel="noopener">reference</a></div>
      </div>
      <div class="section">
        <h3>Defined Parameters</h3>
        ${parameterRowsHtml(info.parameter_rows || [], node.class_name)}
      </div>
      <div class="section">
        <h3>Undefined / Extra Parameters</h3>
        ${parameterRowsHtml(info.extra_rows || [], node.class_name, true)}
        <button class="small-button" data-action="add-param" data-owner="${escapeHtml(node.class_name)}" data-extra="1">Add Parameter</button>
      </div>
    `;
  }

  panel.querySelectorAll("input[data-kind='value']").forEach((input) => {
    input.addEventListener("input", (event) => {
      updateParamValue(event.target);
    });
  });
  panel.querySelectorAll("input[data-kind='toggle']").forEach((input) => {
    input.addEventListener("change", (event) => {
      updateParamEnabled(event.target);
    });
  });
  panel.querySelectorAll("button[data-action='add-task']").forEach((button) => {
    button.addEventListener("click", () => openTaskCandidates(button.dataset.nodeId));
  });
  panel.querySelectorAll("button[data-action='add-param']").forEach((button) => {
    button.addEventListener("click", () => addParameterRow(button.dataset.owner, button.dataset.extra === "1"));
  });
}

function formatInputInfo(info) {
  const lines = [];
  for (const key of (info.keys || [])) {
    lines.push(`${key.name} (${key.class_name})`);
  }
  for (const tree of (info.trees || [])) {
    lines.push(``);
    lines.push(`[${tree.name}]`);
    for (const branch of (tree.branches || [])) {
      lines.push(`  ${branch}`);
    }
  }
  if (info.error) {
    lines.push(``);
    lines.push(`error: ${info.error}`);
  }
  return lines.join("\n");
}

function serializePreview() {
  const payload = buildSavePayload();
  const lines = [];
  for (const row of payload.run_rows) lines.push(renderRowText(row));
  if (payload.control_rows.length || payload.class_nodes.length) lines.push("");
  for (const row of payload.control_rows) lines.push(renderRowText(row));
  for (const node of payload.class_nodes) {
    lines.push(`lilak/add ${node.class_name}`);
  }
  if (payload.class_nodes.length) lines.push("");
  for (const node of payload.class_nodes) {
    for (const row of node.parameter_rows) lines.push(renderRowText(row));
    lines.push("");
  }
  return lines.join("\n");
}

function renderRowText(row) {
  if (row.kind === "comment") {
    return `# ${row.comment || ""}`.trimEnd();
  }
  const prefix = row.enabled === false ? "#" : "";
  const unit = row.unit === "#" ? "" : (row.unit || "");
  const full = `${row.group ? `${row.group}/` : ""}${row.name}`;
  let text = `${prefix}${unit}${full}`;
  if ((row.value || "").trim()) text += `  ${row.value}`;
  if ((row.comment || "").trim()) text += `  # ${row.comment}`;
  return text;
}

function updateParamValue(input) {
  const owner = input.dataset.owner;
  const index = Number(input.dataset.index);
  const isExtra = input.dataset.extra === "1";
  if (owner === "sheet") {
    const row = state.parSheetRows[index];
    if (!row) return;
    row.value = input.value;
    if (row._branchNodeId) {
      const node = nodeById(row._branchNodeId);
      if (node) {
        const enabled = String(input.value || "").trim().toLowerCase() !== "false";
        node.persist_disabled = !enabled;
        state.edges = state.edges.filter((edge) => !(edge.from === node.id && edge.to === "output_file" && edge.type === "output"));
        if (enabled) state.edges.push({ from: node.id, to: "output_file", type: "output" });
        renderCanvas();
      }
    }
    renderStatusPreview();
    return;
  }
  if (owner.startsWith("branch:")) {
    const node = nodeById(owner.slice("branch:".length));
    if (!node) return;
    const enabled = String(input.value || "").trim().toLowerCase() !== "false";
    node.persist_disabled = !enabled;
    state.edges = state.edges.filter((edge) => !(edge.from === node.id && edge.to === "output_file" && edge.type === "output"));
    if (enabled) state.edges.push({ from: node.id, to: "output_file", type: "output" });
    renderCanvas();
    renderStatusPreview();
    return;
  }
  if (owner === "run") {
    state.runRows[index].value = input.value;
    return;
  }
  if (owner === "output") {
    const outputRows = state.runRows.filter((row) => ["OutputPath", "FileName", "Tag", "AutoTerminate", "RunID"].includes(row.name));
    const target = outputRows[index];
    const full = `${target.group}/${target.name}`;
    const real = state.runRows.find((row) => `${row.group}/${row.name}` === full);
    if (real) real.value = input.value;
    return;
  }
  const info = state.classData[owner];
  if (!info) return;
  const rows = isExtra ? info.extra_rows : info.parameter_rows;
  rows[index].value = input.value;
}

function updateParamEnabled(input) {
  const owner = input.dataset.owner;
  const index = Number(input.dataset.index);
  const isExtra = input.dataset.extra === "1";
  if (owner === "run") {
    state.runRows[index].enabled = input.checked;
    return;
  }
  if (owner === "output") {
    const outputRows = state.runRows.filter((row) => ["OutputPath", "FileName", "Tag", "AutoTerminate", "RunID"].includes(row.name));
    const target = outputRows[index];
    const full = `${target.group}/${target.name}`;
    const real = state.runRows.find((row) => `${row.group}/${row.name}` === full);
    if (real) real.enabled = input.checked;
    return;
  }
  const info = state.classData[owner];
  if (!info) return;
  const rows = isExtra ? info.extra_rows : info.parameter_rows;
  rows[index].enabled = input.checked;
}

function addParameterRow(owner, isExtra) {
  if (owner === "run") {
    state.runRows.push({ kind: "parameter", enabled: true, group: "LKRun", name: "NewParameter", value: "", unit: "", comment: "" });
  } else {
    const info = state.classData[owner];
    if (!info) return;
    const target = isExtra ? info.extra_rows : info.parameter_rows;
    target.push({ kind: "parameter", enabled: true, group: owner, name: "NewParameter", value: "", unit: "", comment: "" });
  }
  renderInspector();
}

async function openTaskCandidates(nodeId) {
  const node = nodeById(nodeId);
  if (!node) return;
  state.taskBranch = node;
  const payload = await fetchJson("/api/class_candidates", {
    branch_name: node.branch_name || "",
    branch_class_name: node.branch_class_name || "",
  });
  const modal = document.getElementById("taskModal");
  document.getElementById("taskModalMeta").textContent = `${node.branch_name} (${node.branch_class_name})`;
  const list = document.getElementById("taskCandidateList");
  list.innerHTML = "";
  for (const entry of (payload.entries || [])) {
    const button = document.createElement("button");
    button.textContent = entry.name;
    button.addEventListener("click", () => addTaskFromClass(entry.name));
    list.appendChild(button);
  }
  if (!payload.entries || payload.entries.length === 0) {
    list.innerHTML = `<div class="meta">No matching tasks found.</div>`;
  }
  modal.classList.add("open");
}

async function addTaskFromClass(className) {
  const branch = state.taskBranch;
  if (!branch) return;
  const bundle = await fetchJson("/api/class_bundle", {
    class_name: className,
    all_rows: state.rows,
  });
  const nextIndex = state.nodes.filter((node) => node.kind === "class").length;
  const nodeId = `class_${Date.now()}`;
  state.classData[className] = bundle;
  state.nodes.push({
    id: nodeId,
    kind: "class",
    class_kind: bundle.class_kind || "task",
    label: className,
    subtitle: bundle.class_kind || "task",
    x: 830,
    y: 36 + nextIndex * 112,
    doc_url: bundle.doc_url || "",
    header_path: bundle.header_path || "",
    class_name: className,
  });
  const ensureBranch = (entry) => {
    const existing = state.nodes.find((node) => node.kind === "branch" && node.branch_name === entry.branch_name && node.branch_class_name === entry.class_name);
    if (existing) return existing.id;
    const branchNodes = state.nodes.filter((node) => node.kind === "branch");
    const node = {
      id: `branch_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`,
      kind: "branch",
      label: entry.branch_name,
      subtitle: entry.class_name,
      x: 510,
      y: 36 + branchNodes.length * 92,
      style: "branch_pass",
      branch_name: entry.branch_name,
      branch_class_name: entry.class_name,
      doc_url: entry.doc_url || "",
      header_path: entry.header_path || "",
    };
    state.nodes.push(node);
    return node.id;
  };
  for (const use of (bundle.uses || [])) {
    const bid = ensureBranch(use);
    state.edges.push({ from: bid, to: nodeId, type: "read" });
  }
  for (const save of (bundle.saves || [])) {
    if (save.action === "keep") continue;
    const bid = ensureBranch(save);
    state.edges.push({ from: nodeId, to: bid, type: "write" });
    if (!state.edges.some((edge) => edge.from === bid && edge.to === "output_file")) {
      state.edges.push({ from: bid, to: "output_file", type: "output" });
    }
  }
  document.getElementById("taskModal").classList.remove("open");
  state.selectedId = nodeId;
  render();
}

function buildSavePayload() {
  const classNodes = state.nodes
    .filter((node) => node.kind === "class")
    .map((node) => {
      const info = state.classData[node.class_name] || {};
      return {
        class_name: node.class_name,
        parameter_rows: [...(info.parameter_rows || []), ...(info.extra_rows || [])],
      };
    });
  return {
    path: state.savePath,
    all_rows: state.rows,
    run_rows: state.runRows,
    control_rows: state.controlRows,
    class_nodes: classNodes,
  };
}

async function saveConfiguration() {
  if (!state.savePath) {
    openNavigator();
    return;
  }
  const result = await fetchJson("/api/save", buildSavePayload());
  state.savePath = result.path;
  setStatus(`Saved ${result.path}`);
}

function openNavigator() {
  document.getElementById("navigator").classList.add("open");
  document.getElementById("navPath").value = state.savePath ? state.savePath.split("/").slice(0, -1).join("/") : state.cwd;
  document.getElementById("navFile").value = state.savePath ? state.savePath.split("/").pop() : "configure_flow.conf";
  loadNavigator(document.getElementById("navPath").value);
  setTimeout(() => {
    const input = document.getElementById("navFile");
    input.focus();
    input.select();
  }, 0);
}

function closeNavigator() {
  document.getElementById("navigator").classList.remove("open");
}

async function loadNavigator(path) {
  const payload = await fetchJson("/api/listdir", { path });
  document.getElementById("navPath").value = payload.current_dir;
  const list = document.getElementById("navList");
  list.innerHTML = "";
  for (const entry of (payload.entries || [])) {
    const div = document.createElement("div");
    const isConf = !entry.is_dir && entry.name.endsWith(".conf");
    div.className = `nav-entry ${entry.is_dir ? "dir" : (isConf ? "conf" : "file")}`;
    div.innerHTML = `<span>${escapeHtml(entry.name)}</span><span>${entry.is_dir ? "dir" : "file"}</span>`;
    div.addEventListener("click", () => {
      if (entry.is_dir) {
        loadNavigator(entry.path);
      } else {
        document.getElementById("navFile").value = entry.name;
      }
    });
    div.addEventListener("dblclick", () => {
      if (entry.is_dir) {
        loadNavigator(entry.path);
        return;
      }
      document.getElementById("navFile").value = entry.name;
      if (isConf || entry.name.endsWith(".mac") || entry.name.endsWith(".par")) {
        confirmNavigatorSave();
      }
    });
    list.appendChild(div);
  }
}

async function confirmNavigatorSave() {
  const dir = document.getElementById("navPath").value.trim() || state.cwd;
  const file = document.getElementById("navFile").value.trim() || "configure_flow.conf";
  state.savePath = `${dir.replace(/\/+$/, "")}/${file}`;
  closeNavigator();
  await saveConfiguration();
}

function render() {
  renderCanvas();
  renderInspector();
}

async function bootstrap() {
  const payload = await fetchJson("/api/bootstrap");
  state.sourceKind = payload.source_kind || "blank";
  state.sourcePath = payload.source_path || "";
  state.savePath = payload.save_path || "";
  state.cwd = payload.cwd || "";
  state.commonDir = payload.common_dir || "";
  state.rows = payload.rows || [];
  state.runRows = payload.run_rows || [];
  state.controlRows = payload.control_rows || [];
  state.nodes = payload.nodes || [];
  state.edges = payload.edges || [];
  state.classData = payload.class_data || {};
  state.inputInfo = payload.input_info || {};
  state.selectedId = state.nodes.length ? state.nodes[0].id : "";
  setStatus(state.sourcePath ? `Loaded ${state.sourcePath}` : "Opened blank configure flow");
  render();
}

document.getElementById("saveButton").addEventListener("click", saveConfiguration);
document.getElementById("navGo").addEventListener("click", () => loadNavigator(document.getElementById("navPath").value));
document.getElementById("navUp").addEventListener("click", () => {
  const current = document.getElementById("navPath").value.trim();
  const next = current.replace(/\/+$/, "").split("/").slice(0, -1).join("/") || "/";
  loadNavigator(next);
});
document.getElementById("navHome").addEventListener("click", () => loadNavigator(state.cwd || "/"));
document.getElementById("navCommon").addEventListener("click", () => loadNavigator(state.commonDir || state.cwd || "/"));
document.getElementById("navSave").addEventListener("click", confirmNavigatorSave);
document.getElementById("taskModalClose").addEventListener("click", () => document.getElementById("taskModal").classList.remove("open"));
document.addEventListener("keydown", (event) => {
  if (event.key === "Escape") {
    document.getElementById("taskModal").classList.remove("open");
    closeNavigator();
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "s") {
    event.preventDefault();
    saveConfiguration();
  }
  if (document.getElementById("navigator").classList.contains("open") && event.key === "Enter") {
    event.preventDefault();
    confirmNavigatorSave();
  }
});

bootstrap().catch((error) => {
  setStatus(error.message || String(error));
});
</script>
</body>
</html>
"""


CONFIGURE_EXTRA_STYLE = r"""
    .node { width: 220px; }
    .node.run .node-header { background: #7aa5db; }
    .node.parameter_container .node-header { background: #6f8bb0; }
    .node.input_file .node-header { background: #6fb6dd; }
    .node.output_file .node-header { background: #4a5563; color: #fff; }
    .node.class.task .node-header, .node.task .node-header { background: #7fc59a; }
    .node.class.detector .node-header, .node.class.detector_plane .node-header, .node.detector .node-header, .node.detector_plane .node-header { background: #c695df; }
    .node.branch_input .node-header { background: #f1c94b; color: #2d2b16; }
    .node.branch_output .node-header { background: #b9bec9; color: #27374f; }
    .node.branch_pass .node-header { background: #b9bec9; color: #27374f; }
    .node.branch_plain .node-header { background: #a5b0c5; }
    .node.branch_input_output .node-header { background: #f1c94b; color: #2d2b16; }
    .node.class .node-body, .node.run .node-body, .node.parameter_container .node-body, .node.input_file .node-body, .node.output_file .node-body, .node.branch .node-body { color: var(--muted); }
    .node .node-body { min-height: 64px; }
    .flow-meta { font-size: 12px; color: var(--muted); line-height: 1.45; overflow-wrap: anywhere; word-break: break-word; }
    .flow-actions { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 10px; }
    .flow-actions button {
      border: 1px solid rgba(33,66,90,0.24);
      background: rgba(247,251,255,0.98);
      color: var(--text);
      border-radius: 8px;
      padding: 4px 10px;
      cursor: pointer;
      box-shadow: var(--shadow);
      font-size: 11px;
    }
    .summary-actions {
      display: flex;
      gap: 5px;
      flex-wrap: wrap;
      margin-top: 6px;
    }
    .summary-actions button {
      border: 1px solid rgba(33,66,90,0.24);
      background: rgba(247,251,255,0.98);
      color: var(--text);
      border-radius: 7px;
      padding: 3px 8px;
      cursor: pointer;
      font-size: 11px;
    }
    #taskCandidateList .navigator-entry .name { color: #111; }
    .raw-box {
      white-space: pre-wrap;
      font-family: var(--mono);
      font-size: 11px;
      line-height: 1.45;
      background: #f7faff;
      border: 1px solid #d4e1f2;
      border-radius: 10px;
      padding: 10px;
      max-height: 340px;
      overflow: auto;
    }
    .help { color: var(--muted); font-size: 13px; }
    .configure-tab-actions {
      display: inline-flex;
      gap: 4px;
      align-items: center;
    }
"""


CONFIGURE_BODY_HTML = r"""
  <div class="app">
    <div class="topbar">
      <div class="headline">
        <h1>LILAK Configuration</h1>
      </div>
      <div class="toolbar">
        <button id="shortcutsBtn">Shortcuts</button>
      </div>
    </div>
    <div class="tabs-wrap">
      <div class="tabs">
        <span class="subtoolbar-title">Parameter Files</span>
        <button id="loadParameterBtn" title="Load parameter file">↓</button>
        <span id="parameterFileTabs"></span>
      </div>
      <div class="subtoolbar block">
        <div class="tabs">
          <span class="subtoolbar-title">Edit</span>
          <button id="addDetectorBtn">Add Detector</button>
          <button id="addTaskBtn">Add Task</button>
          <button id="addForBranchBtn">Add for branch</button>
        </div>
      </div>
    </div>
    <div class="workspace" id="workspace">
      <section class="panel summary-panel">
        <div class="panel-header">
          <button id="summaryToggleBtn" class="summary-toggle" title="Toggle summary" aria-label="Toggle summary">☰</button>
          <h2>Flow Summary</h2>
        </div>
        <div class="panel-body">
          <div class="summary-stack">
            <div>
              <h2 style="margin-bottom:8px;">Messages</h2>
              <div class="message-log" id="messageLog">Ready.</div>
            </div>
            <div id="flowSummary" class="group-summary-scroll"></div>
          </div>
        </div>
      </section>
      <section class="panel canvas-panel">
        <div class="panel-header" style="display:flex; align-items:center; justify-content:space-between; gap:10px;">
          <h2>Configuration Canvas</h2>
          <div class="toolbar">
            <button id="arrangeBtn">Arrange</button>
          </div>
        </div>
        <div class="panel-body">
          <div class="canvas-shell" id="canvas">
            <svg id="wireLayerBack" class="wire-layer-back"></svg>
            <div class="canvas-nodes" id="nodeLayer"></div>
            <svg id="wireLayerFront" class="wire-layer-front"></svg>
          </div>
        </div>
      </section>
      <section class="panel inspector-panel">
        <div class="panel-header"><h2>Inspector</h2></div>
        <div class="panel-body">
          <div class="stack">
            <div>
              <h2 style="margin-bottom:8px;">Parameter Preview</h2>
              <div class="status" id="statusLog">Waiting for save.</div>
            </div>
          </div>
        </div>
      </section>
    </div>
  </div>

  <div class="modal" id="navigatorModal">
    <div class="modal-card navigator-card">
      <div style="display:flex; justify-content:space-between; align-items:center; gap:12px; margin-bottom:12px;">
        <h3 id="navigatorTitle">Choose File</h3>
        <button id="navClose">Close</button>
      </div>
      <div class="navigator-controls">
        <div class="navigator-row path-row">
          <button id="navCommon">COMMON</button>
          <button id="navHome">HOME</button>
          <button id="navUp">Up</button>
          <input type="text" id="navPath" />
          <button id="navGo">Go</button>
        </div>
        <div class="navigator-row file-row">
          <div>File</div>
          <input type="text" id="navFile" />
          <button id="navConfirm">Open</button>
        </div>
      </div>
      <div class="navigator-list" id="navList"></div>
    </div>
  </div>

  <div class="modal" id="taskModal">
    <div class="modal-card">
      <div style="display:flex; justify-content:space-between; align-items:center; gap:8px;">
        <div style="font-weight:600;">Add Task From Branch</div>
        <button id="taskModalClose">Close</button>
      </div>
      <div class="flow-meta" id="taskModalMeta"></div>
      <div class="candidate-list" id="taskCandidateList"></div>
    </div>
  </div>

  <div class="modal" id="parModal">
    <div class="modal-card">
      <div style="display:flex; justify-content:space-between; align-items:center; gap:8px;">
        <div style="font-weight:600;" id="parModalTitle">Parameters</div>
        <div style="display:flex; gap:6px;">
          <button id="parModalSave">Save</button>
          <button id="parModalClose">Close</button>
        </div>
      </div>
      <div class="flow-meta" id="parModalMeta"></div>
      <div id="parModalContent"></div>
    </div>
  </div>

  <div class="modal" id="shortcutsModal">
    <div class="modal-card">
      <div style="display:flex; justify-content:space-between; align-items:center; gap:8px;">
        <div style="font-weight:600;">Shortcuts</div>
        <button id="shortcutsClose">Close</button>
      </div>
      <div class="parameter-sheet-wrap">
        <table class="parameter-sheet shortcut-sheet">
          <thead>
            <tr><th>Shortcut</th><th>Description</th></tr>
          </thead>
          <tbody>
            <tr><td class="readonly">Ctrl/Cmd+H</td><td class="readonly">Toggle Flow Summary panel.</td></tr>
            <tr><td class="readonly">?</td><td class="readonly">Open or close this Shortcuts window.</td></tr>
            <tr><td class="readonly">Cmd/Ctrl+S</td><td class="readonly">Save current configuration.</td></tr>
            <tr><td class="readonly">Cmd/Ctrl+S</td><td class="readonly">Save open parameter popup locally when it is open.</td></tr>
            <tr><td class="readonly">Cmd/Ctrl+P</td><td class="readonly">Open parameter popup for selected item.</td></tr>
            <tr><td class="readonly">Cmd/Ctrl+L</td><td class="readonly">Load a parameter file.</td></tr>
            <tr><td class="readonly">Cmd/Ctrl+A</td><td class="readonly">Arrange items on the canvas.</td></tr>
            <tr><td class="readonly">Enter</td><td class="readonly">Confirm file navigator action when navigator is open.</td></tr>
            <tr><td class="readonly">Esc</td><td class="readonly">Close open popup windows.</td></tr>
            <tr><td class="readonly">Tab / Shift+Tab</td><td class="readonly">Cycle selected item on the canvas.</td></tr>
            <tr><td class="readonly">Delete / Backspace</td><td class="readonly">Delete selected branch or task item.</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>
"""


CONFIGURE_SCRIPT_JS = r"""
const state = {
  sourceKind: "blank",
  sourcePath: "",
  savePath: "",
  configurationName: "configure_flow",
  parameterTabs: [],
  activeParameterTabId: "",
  cwd: "",
  commonDir: "",
  rows: [],
  runRows: [],
  controlRows: [],
  nodes: [],
  edges: [],
  classData: {},
  inputInfo: {},
  selectedId: "run",
  taskBranch: null,
  parSheetRows: [],
  navMode: "save",
  drag: null,
  messages: [{ text: "Ready.", error: false }],
  classPickerKind: "",
};

function basename(value) {
  if (!value) return "Untitled";
  return String(value).replace(/\\/g, "/").split("/").filter(Boolean).pop() || "Untitled";
}

function stripExtension(value) {
  const base = basename(value);
  return base.replace(/\.[^.]+$/, "") || "configure_flow";
}

function normalizedConfigName(value) {
  const clean = String(value || "").trim().replace(/[\/\\]/g, "_");
  return clean || "configure_flow";
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>\"']/g, (ch) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;" }[ch]));
}

async function fetchJson(url, payload = null) {
  const options = payload ? {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  } : {};
  const response = await fetch(url, options);
  const data = await response.json();
  if (!response.ok) {
    throw new Error(data.error || "Request failed");
  }
  return data;
}

function logMessage(message, error=false) {
  state.messages = [{ text: String(message || ""), error: !!error }];
  const box = document.getElementById("messageLog");
  if (box) {
    box.textContent = state.messages[0].text || "Ready.";
    box.classList.toggle("error", !!state.messages[0].error);
  }
}

function nodeById(id) {
  return state.nodes.find((node) => node.id === id) || null;
}

function connectedNodeIds(nodeId) {
  if (!nodeId) return new Set();
  const ids = new Set([nodeId]);
  for (const edge of state.edges) {
    if (edge.from === nodeId) ids.add(edge.to);
    if (edge.to === nodeId) ids.add(edge.from);
  }
  return ids;
}

function isEdgeSelected(edge) {
  if (!state.selectedId) return false;
  const ids = connectedNodeIds(state.selectedId);
  return ids.has(edge.from) || ids.has(edge.to);
}

function visibleNodesSorted() {
  return [...state.nodes].sort((a, b) => (a.y - b.y) || (a.x - b.x) || a.label.localeCompare(b.label));
}

function summaryNodesSorted() {
  const rank = (node) => {
    if (node.kind === "run") return 0;
    if (node.kind === "parameter_container") return 1;
    if (node.kind === "input_file") return 2;
    if (node.kind === "output_file") return 3;
    if (node.kind === "class") return 4;
    if (node.kind === "branch") return 5;
    return 6;
  };
  const indexById = new Map(state.nodes.map((node, index) => [node.id, index]));
  return [...state.nodes].sort((a, b) => {
    const rankDelta = rank(a) - rank(b);
    if (rankDelta) return rankDelta;
    return (indexById.get(a.id) || 0) - (indexById.get(b.id) || 0);
  });
}

function ensureParameterTabs() {
  if (!Array.isArray(state.parameterTabs)) state.parameterTabs = [];
  if (typeof state.activeParameterTabId !== "string") state.activeParameterTabId = "";
}

function captureParameterTab() {
  return {
    sourceKind: state.sourceKind,
    sourcePath: state.sourcePath,
    savePath: state.savePath,
    configurationName: state.configurationName,
    rows: JSON.parse(JSON.stringify(state.rows || [])),
    runRows: JSON.parse(JSON.stringify(state.runRows || [])),
    controlRows: JSON.parse(JSON.stringify(state.controlRows || [])),
    nodes: JSON.parse(JSON.stringify(state.nodes || [])),
    edges: JSON.parse(JSON.stringify(state.edges || [])),
    classData: JSON.parse(JSON.stringify(state.classData || {})),
    inputInfo: JSON.parse(JSON.stringify(state.inputInfo || {})),
    selectedId: state.selectedId || "",
  };
}

function storeActiveParameterTab() {
  ensureParameterTabs();
  const tab = state.parameterTabs.find(item => item.id === state.activeParameterTabId);
  if (!tab) return;
  Object.assign(tab, captureParameterTab());
  tab.label = currentParameterTabLabel();
}

function applyParameterTab(tab) {
  state.sourceKind = tab.sourceKind || "blank";
  state.sourcePath = tab.sourcePath || "";
  state.savePath = tab.savePath || "";
  state.configurationName = tab.configurationName || "configure_flow";
  state.rows = JSON.parse(JSON.stringify(tab.rows || []));
  state.runRows = JSON.parse(JSON.stringify(tab.runRows || []));
  state.controlRows = JSON.parse(JSON.stringify(tab.controlRows || []));
  state.nodes = JSON.parse(JSON.stringify(tab.nodes || []));
  state.edges = JSON.parse(JSON.stringify(tab.edges || []));
  state.classData = JSON.parse(JSON.stringify(tab.classData || {}));
  state.inputInfo = JSON.parse(JSON.stringify(tab.inputInfo || {}));
  state.selectedId = tab.selectedId || (state.nodes.length ? state.nodes[0].id : "");
}

function activateParameterTab(tabId) {
  ensureParameterTabs();
  if (tabId === state.activeParameterTabId) return;
  storeActiveParameterTab();
  const tab = state.parameterTabs.find(item => item.id === tabId);
  if (!tab) return;
  state.activeParameterTabId = tab.id;
  applyParameterTab(tab);
  render();
}

function makeParameterTabFromCurrent(label = "") {
  return {
    id: `par_tab_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`,
    label: label || currentParameterTabLabel(),
    ...captureParameterTab(),
  };
}

function closeParameterTab(tabId) {
  ensureParameterTabs();
  const index = state.parameterTabs.findIndex(item => item.id === tabId);
  if (index < 0) return;
  const closingActive = tabId === state.activeParameterTabId;
  state.parameterTabs.splice(index, 1);
  if (state.parameterTabs.length === 0) {
    state.sourceKind = "blank";
    state.sourcePath = "";
    state.savePath = "";
    state.configurationName = "configure_flow";
    state.rows = [];
    state.runRows = [];
    state.controlRows = [];
    state.nodes = [
      { id: "run", kind: "run", label: "Run", subtitle: "run / 0", x: 36, y: 36 },
      { id: "parameter_container", kind: "parameter_container", label: "Parameter Container", subtitle: "0 rows", x: 36, y: 192 },
      { id: "output_file", kind: "output_file", label: "Output File", subtitle: "not configured yet", x: 36, y: 504 },
    ];
    state.edges = [];
    state.classData = {};
    state.inputInfo = {};
    state.selectedId = "run";
    const tab = makeParameterTabFromCurrent("configure_flow.mac");
    state.parameterTabs.push(tab);
    state.activeParameterTabId = tab.id;
  } else if (closingActive) {
    const next = state.parameterTabs[Math.min(index, state.parameterTabs.length - 1)];
    state.activeParameterTabId = next.id;
    applyParameterTab(next);
  }
  render();
}

function renderParameterTabs() {
  const host = document.getElementById("parameterFileTabs");
  if (!host) return;
  ensureParameterTabs();
  storeActiveParameterTab();
  if (state.parameterTabs.length === 0) {
    const tab = makeParameterTabFromCurrent();
    state.parameterTabs.push(tab);
    state.activeParameterTabId = tab.id;
  }
  host.innerHTML = state.parameterTabs.map(tab => `
    <span class="tab-wrap">
      <button class="tab ${tab.id === state.activeParameterTabId ? "active" : ""}" data-tab-id="${escapeHtml(tab.id)}">${escapeHtml(tab.label || "configure_flow.mac")}</button>
      <button class="tab-close" data-close-tab-id="${escapeHtml(tab.id)}" title="Close parameter file">x</button>
    </span>
  `).join("");
  host.querySelectorAll("button[data-tab-id]").forEach(button => {
    button.addEventListener("click", () => activateParameterTab(button.dataset.tabId));
  });
  host.querySelectorAll("button[data-close-tab-id]").forEach(button => {
    button.addEventListener("click", event => {
      event.stopPropagation();
      closeParameterTab(button.dataset.closeTabId);
    });
  });
}

function renderSummary() {
  const host = document.getElementById("flowSummary");
  if (!host) return;
  const items = summaryNodesSorted();
  host.innerHTML = `
    <div class="summary-card">
      <div style="font-size:12px; color:var(--muted); margin-bottom:10px;">Configuration</div>
      <div class="field" style="margin-bottom:10px;">
        <input type="text" id="configurationNameInput" value="${escapeHtml(state.configurationName)}">
      </div>
      <button class="small-button" id="configurationSaveBtn">Save</button>
    </div>
    <div class="summary-card">
      <div style="font-size:12px; color:var(--muted); margin-bottom:10px;">Items</div>
      <div class="summary-list" id="summaryList"></div>
    </div>
  `;
  const nameInput = document.getElementById("configurationNameInput");
  if (nameInput) {
    nameInput.addEventListener("input", (event) => {
      state.configurationName = normalizedConfigName(event.target.value);
      renderParameterTabs();
    });
  }
  const saveBtn = document.getElementById("configurationSaveBtn");
  if (saveBtn) saveBtn.addEventListener("click", saveConfiguration);
  const list = document.getElementById("summaryList");
  items.forEach((node) => {
    const deletable = node.kind === "branch" || node.kind === "class";
    const canReference = !!node.doc_url;
    const canPath = node.kind === "branch" || node.kind === "class";
    const canAddTask = node.kind === "branch";
    const item = document.createElement("div");
    item.className = `summary-item ${node.id === state.selectedId ? "active" : ""}`;
    item.innerHTML = `
      <div class="summary-item-row">
        <div>
          <div style="font-weight:600;">${escapeHtml(node.label)}</div>
          <div style="font-size:11px; color:var(--muted);">${escapeHtml(node.subtitle || node.kind)}</div>
          <div class="summary-actions">
            ${canReference ? `<button data-action="open-ref" data-url="${escapeHtml(node.doc_url || "")}">Ref.</button>` : ``}
            ${canPath ? `<button data-action="show-path" data-node-id="${escapeHtml(node.id)}">Path</button>` : ``}
            ${canAddTask ? `<button data-action="add-task" data-node-id="${escapeHtml(node.id)}">+Task</button>` : ``}
            <button data-action="open-par" data-node-id="${escapeHtml(node.id)}">Par</button>
          </div>
        </div>
        ${deletable ? `<button class="summary-delete" data-action="delete-item" data-node-id="${escapeHtml(node.id)}">x</button>` : ``}
      </div>
    `;
    item.addEventListener("click", () => {
      state.selectedId = node.id;
      render();
    });
    item.querySelectorAll("button[data-action='open-ref']").forEach((button) => {
      button.addEventListener("click", (event) => {
        event.stopPropagation();
        const url = button.dataset.url || "";
        if (url) window.open(url, "_blank", "noopener");
      });
    });
    item.querySelectorAll("button[data-action='add-task']").forEach((button) => {
      button.addEventListener("click", (event) => {
        event.stopPropagation();
        openTaskCandidates(button.dataset.nodeId);
      });
    });
    item.querySelectorAll("button[data-action='show-path']").forEach((button) => {
      button.addEventListener("click", (event) => {
        event.stopPropagation();
        showNodeHeaderPath(button.dataset.nodeId);
      });
    });
    item.querySelectorAll("button[data-action='open-par']").forEach((button) => {
      button.addEventListener("click", (event) => {
        event.stopPropagation();
        openParameterSheet(button.dataset.nodeId);
      });
    });
    const deleteButton = item.querySelector("button[data-action='delete-item']");
    if (deleteButton) {
      deleteButton.addEventListener("click", (event) => {
        event.stopPropagation();
        deleteNodeById(deleteButton.dataset.nodeId);
      });
    }
    list.appendChild(item);
  });
}

function deleteNodeById(nodeId) {
  const node = nodeById(nodeId);
  if (!node) return;
  if (!(node.kind === "branch" || node.kind === "class")) return;
  state.nodes = state.nodes.filter((item) => item.id !== nodeId);
  state.edges = state.edges.filter((edge) => edge.from !== nodeId && edge.to !== nodeId);
  if (node.kind === "class" && node.class_name) {
    delete state.classData[node.class_name];
  }
  if (state.selectedId === nodeId) {
    state.selectedId = state.nodes.length ? state.nodes[0].id : "";
  }
  render();
}

function deleteSelection() {
  if (!state.selectedId) return;
  deleteNodeById(state.selectedId);
}

function inputInfoSummaryLines() {
  const info = state.inputInfo || {};
  const lines = [];
  if (info.path) lines.push(basename(info.path));
  for (const tree of (info.trees || [])) lines.push(tree.name);
  for (const name of (info.parameter_container || [])) lines.push(name);
  for (const name of (info.run_header || [])) lines.push(name);
  return lines.slice(0, 4);
}

function peerLabelForPort(node, portKey) {
  const edge = state.edges.find((item) => {
    const fromSpec = edgePortSpec(nodeById(item.from), item, "from");
    const toSpec = edgePortSpec(nodeById(item.to), item, "to");
    return (item.from === node.id && fromSpec.key === portKey) || (item.to === node.id && toSpec.key === portKey);
  });
  if (!edge) return ".";
  if (edge.from === node.id) {
    const peer = nodeById(edge.to);
    return peer ? peer.label : ".";
  }
  const peer = nodeById(edge.from);
  return peer ? peer.label : ".";
}

function isBranchPersistDisabled(node) {
  return !!(node && node.kind === "branch" && node.persist_disabled);
}

function toggleBranchPersistency(nodeId) {
  const node = nodeById(nodeId);
  if (!node || node.kind !== "branch") return;
  node.persist_disabled = !node.persist_disabled;
  state.edges = state.edges.filter((edge) => !(edge.from === node.id && edge.to === "output_file" && edge.type === "output"));
  if (!node.persist_disabled) {
    state.edges.push({ from: node.id, to: "output_file", type: "output" });
  }
  renderStatusPreview();
  render();
}

function renderPortCell(node, side, kind, label, portKey) {
  if (kind === "none") {
    return `<div class="port-cell"></div>`;
  }
  const portClass = kind === "input" ? "input" : "output";
  const disabled = node.kind === "branch" && portKey === "persist-link" && isBranchPersistDisabled(node);
  const port = `<div class="port connected ${portClass}${disabled ? " disabled" : ""}" data-side="${escapeHtml(side)}" data-port-key="${escapeHtml(portKey)}"></div>`;
  const disable = node.kind === "branch" && portKey === "persist-link"
    ? `<div class="port-disable ${disabled ? "disabled" : "enabled"}" title="${disabled ? "Enable output persistency" : "Disable output persistency"}" data-action="toggle-persist" data-node-id="${escapeHtml(node.id)}"></div>`
    : "";
  if (side === "left") {
    return `<div class="port-cell">${port}${disable}</div>`;
  }
  return `<div class="port-cell">${disable}${port}</div>`;
}

function renderPortRow(node, leftKind, leftLabel, leftKey, rightKind, rightLabel, rightKey) {
  return `
    <div class="port-row">
      ${renderPortCell(node, "left", leftKind, leftLabel, leftKey)}
      ${renderPortCell(node, "right", rightKind, rightLabel, rightKey)}
    </div>
  `;
}

function runParamValue(name, fallback = "") {
  const row = (state.runRows || []).find((item) => item.kind === "parameter" && item.group === "LKRun" && item.name === name);
  if (!row || row.enabled === false) return fallback;
  const value = String(row.value ?? "").trim();
  return value || fallback;
}

function outputFileDisplayName(node) {
  const fileName = runParamValue("FileName", "");
  if (fileName) return fileName;
  const configured = String(node.subtitle || "").trim();
  if (configured && configured !== "not configured yet") return basename(configured);
  const runName = runParamValue("Name", "run");
  const runId = runParamValue("RunID", "0");
  const tag = runParamValue("Tag", "");
  const idText = String(runId).padStart(4, "0");
  return tag ? `${runName}_${idText}.${tag}.root` : `${runName}_${idText}.root`;
}

function nodeBodyHtml(node) {
  if (node.kind === "run") {
    const runName = runParamValue("Name", "run");
    const runId = runParamValue("RunID", "0");
    const tag = runParamValue("Tag", "tag");
    return `
      <div class="flow-meta">Name: ${escapeHtml(runName)}<br>RunID: ${escapeHtml(runId)}<br>Tag: ${escapeHtml(tag)}</div>
    `;
  }
  if (node.kind === "parameter_container") return `
    <div class="flow-meta">Whole parameter container preview.</div>
  `;
  if (node.kind === "input_file") {
    const lines = inputInfoSummaryLines();
    return `<div class="flow-meta">${lines.map((line) => escapeHtml(line)).join("<br>") || escapeHtml(basename(node.label))}</div>`;
  }
  if (node.kind === "output_file") {
    const outputName = outputFileDisplayName(node);
    return `
      <div class="flow-meta">${escapeHtml(outputName)}</div>
    `;
  }
  if (node.kind === "branch") {
    return `
      <div class="flow-meta">${escapeHtml(node.branch_class_name || node.subtitle || "")}</div>
      <div class="flow-actions">
        <button data-action="open-ref" data-url="${escapeHtml(node.doc_url || "")}">Reference</button>
        <button data-action="show-path" data-node-id="${escapeHtml(node.id)}">Path</button>
        <button data-action="add-task" data-node-id="${escapeHtml(node.id)}">+Task</button>
      </div>
    `;
  }
  if (node.kind === "class") {
    return `
      <div class="flow-meta">${escapeHtml(node.class_name || node.label)}</div>
      <div class="flow-actions">
        <button data-action="open-ref" data-url="${escapeHtml(node.doc_url || "")}">Reference</button>
        <button data-action="show-path" data-node-id="${escapeHtml(node.id)}">Path</button>
      </div>
    `;
  }
  return `<div class="flow-meta">${escapeHtml(node.subtitle || "")}</div>`;
}

function applyNodeZOrder() {
  const order = visibleNodesSorted();
  const selectedIds = connectedNodeIds(state.selectedId);
  let z = 10;
  order.filter((node) => !selectedIds.has(node.id)).forEach((node) => {
    const el = document.querySelector(`.node[data-node-id="${CSS.escape(node.id)}"]`);
    if (el) el.style.zIndex = String(z++);
  });
  order.filter((node) => selectedIds.has(node.id) && node.id !== state.selectedId).forEach((node) => {
    const el = document.querySelector(`.node[data-node-id="${CSS.escape(node.id)}"]`);
    if (el) el.style.zIndex = String(z++);
  });
  const selected = nodeById(state.selectedId);
  if (selected) {
    const el = document.querySelector(`.node[data-node-id="${CSS.escape(selected.id)}"]`);
    if (el) el.style.zIndex = "999";
  }
}

function portCenter(nodeId, portKey, fallbackSide) {
  const nodeLayerRect = document.getElementById("nodeLayer").getBoundingClientRect();
  const node = document.querySelector(`.node[data-node-id="${CSS.escape(nodeId)}"]`);
  if (!node) return null;
  const port = node.querySelector(`.port[data-port-key="${CSS.escape(portKey)}"]`);
  if (port) {
    const rect = port.getBoundingClientRect();
    return {
      x: rect.left + rect.width / 2 - nodeLayerRect.left,
      y: rect.top + rect.height / 2 - nodeLayerRect.top,
    };
  }
  const rect = node.getBoundingClientRect();
  return {
    x: (fallbackSide === "right" ? rect.right : rect.left) - nodeLayerRect.left,
    y: rect.top + rect.height / 2 - nodeLayerRect.top,
  };
}

function edgePortSpec(node, edge, relation) {
  if (!node) return { key: relation === "from" ? "output" : "input", side: relation === "from" ? "right" : "left" };
  if (node.kind === "input_file") {
    return { key: "file-output", side: "right" };
  }
  if (node.kind === "output_file") {
    return { key: "file-input", side: "right" };
  }
  if (node.kind === "branch") {
    if (edge.type === "input" && relation === "to") return { key: "file-link", side: "left" };
    if (edge.type === "output" && relation === "from") return { key: "persist-link", side: "left" };
    if (edge.type === "read" && relation === "from") return { key: "task-link", side: "right" };
    if (edge.type === "write" && relation === "to") return { key: "task-link", side: "right" };
  }
  if (node.kind === "class") {
    if (edge.type === "source" && relation === "to") return { key: "read-input", side: "left" };
    if (edge.type === "read" && relation === "to") return { key: "read-input", side: "left" };
    if (edge.type === "write" && relation === "from") return { key: "write-output", side: "left" };
  }
  return { key: relation === "from" ? "output" : "input", side: relation === "from" ? "right" : "left" };
}

function edgeStrokeColor(edge) {
  if (edge.to === "output_file" || edge.type === "output") return "#111111";
  if (edge.type === "read") return "#9bd7a5";
  if (edge.type === "write") return "#1f7a3c";
  return "#6c86a8";
}

function showNodeHeaderPath(nodeId) {
  const node = nodeById(nodeId);
  if (!node) return;
  const path = node.header_path || "";
  if (path) logMessage(path);
  else logMessage(`Header path not found for ${node.class_name || node.branch_class_name || node.label}.`, true);
}

function renderCanvas() {
  const nodeLayer = document.getElementById("nodeLayer");
  const edgeLayer = document.getElementById("wireLayerBack");
  const edgeLayerFront = document.getElementById("wireLayerFront");
  nodeLayer.innerHTML = "";
  edgeLayer.innerHTML = `
    <defs>
      <marker id="flowArrowRead" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L8,3 L0,6 z" fill="#6c86a8"></path>
      </marker>
      <marker id="flowArrowWrite" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto" markerUnits="strokeWidth">
        <path d="M0,0 L8,3 L0,6 z" fill="#c57532"></path>
      </marker>
    </defs>
  `;
  edgeLayerFront.innerHTML = edgeLayer.innerHTML;
  const rects = {};
  state.nodes.forEach((node) => {
    const incoming = state.edges.filter((edge) => edge.to === node.id);
    const outgoing = state.edges.filter((edge) => edge.from === node.id);
    const kindClass = node.kind === "class" ? `${node.kind} ${node.class_kind || "class"}` : `${node.kind} ${node.style || ""}`;
    const el = document.createElement("div");
    el.className = `node ${kindClass}`.trim();
    if (node.id === state.selectedId) el.classList.add("selected");
    el.dataset.nodeId = node.id;
    el.style.left = `${node.x}px`;
    el.style.top = `${node.y}px`;
    let portBlocks = "";
    if (node.kind === "input_file") {
      portBlocks += renderPortRow(node, "none", "-", "unused-left-1", "output", "Branches", "file-output");
      portBlocks += renderPortRow(node, "none", "-", "unused-left-2", "none", "-", "unused-right-2");
    } else if (node.kind === "run" || node.kind === "parameter_container") {
      portBlocks = "";
    } else if (node.kind === "output_file") {
      portBlocks += renderPortRow(node, "none", "-", "unused-left-1", "input", "Saved branches", "file-input");
      portBlocks += renderPortRow(node, "none", "-", "unused-left-2", "none", "-", "unused-right-2");
    } else if (node.kind === "branch") {
      portBlocks += renderPortRow(node, "input", "File", "file-link", "input", "Task", "task-link");
      portBlocks += renderPortRow(node, "output", "Persist", "persist-link", "none", "-", "unused-right-branch-2");
    } else if (node.kind === "class") {
      portBlocks += renderPortRow(node, "input", "Read branches", "read-input", "none", "-", "unused-right-task-1");
      portBlocks += renderPortRow(node, "output", "Write / Keep branches", "write-output", "none", "-", "unused-right-task-2");
    } else {
      portBlocks += renderPortRow(node, "input", "Input", "input", "output", "Output", "output");
    }
    el.innerHTML = `
      <div class="node-header">
        <div class="node-title">${escapeHtml(node.label)}</div>
        ${window.lilakWebFormat.nodeEditButton(node.id, escapeHtml, {action: "open-par", title: "Parameters"})}
      </div>
      <div class="node-body">
        ${nodeBodyHtml(node)}
        ${portBlocks}
      </div>
    `;
    el.addEventListener("click", (event) => {
      const persistTarget = event.target.closest("[data-action='toggle-persist']");
      const parTarget = event.target.closest("button[data-action='open-par']");
      const actionTarget = event.target.closest("button[data-action='add-task']");
      const refTarget = event.target.closest("button[data-action='open-ref']");
      const pathTarget = event.target.closest("button[data-action='show-path']");
      if (persistTarget) {
        toggleBranchPersistency(persistTarget.dataset.nodeId);
        event.stopPropagation();
        return;
      }
      if (parTarget) {
        openParameterSheet(parTarget.dataset.nodeId);
        event.stopPropagation();
        return;
      }
      if (actionTarget) {
        openTaskCandidates(actionTarget.dataset.nodeId);
        event.stopPropagation();
        return;
      }
      if (refTarget) {
        const url = refTarget.dataset.url || "";
        if (url) window.open(url, "_blank", "noopener");
        event.stopPropagation();
        return;
      }
      if (pathTarget) {
        showNodeHeaderPath(pathTarget.dataset.nodeId);
        event.stopPropagation();
        return;
      }
      state.selectedId = node.id;
      render();
    });
    const header = el.querySelector(".node-header");
    header.addEventListener("mousedown", (event) => {
      if (event.button !== 0) return;
      if (event.target.closest("button")) return;
      const canvasRect = document.getElementById("canvas").getBoundingClientRect();
      state.selectedId = node.id;
      state.drag = {
        nodeId: node.id,
        offsetX: event.clientX - canvasRect.left - node.x,
        offsetY: event.clientY - canvasRect.top - node.y,
      };
      applyNodeZOrder();
      event.preventDefault();
    });
    nodeLayer.appendChild(el);
    rects[node.id] = { x: node.x, y: node.y, width: 220, height: el.offsetHeight || 110 };
  });
  const layerWidth = nodeLayer.offsetWidth || nodeLayer.clientWidth || 1520;
  const layerHeight = nodeLayer.offsetHeight || nodeLayer.clientHeight || 1520;
  for (const layer of [edgeLayer, edgeLayerFront]) {
    layer.setAttribute("width", String(layerWidth));
    layer.setAttribute("height", String(layerHeight));
    layer.setAttribute("viewBox", `0 0 ${layerWidth} ${layerHeight}`);
    layer.style.width = `${layerWidth}px`;
    layer.style.height = `${layerHeight}px`;
  }
  state.edges.forEach((edge) => {
    const from = rects[edge.from];
    const to = rects[edge.to];
    if (!from || !to) return;
    const fromSpec = edgePortSpec(nodeById(edge.from), edge, "from");
    const toSpec = edgePortSpec(nodeById(edge.to), edge, "to");
    const fromPort = portCenter(edge.from, fromSpec.key, fromSpec.side);
    const toPort = portCenter(edge.to, toSpec.key, toSpec.side);
    const x1 = fromPort ? fromPort.x : (from.x + from.width);
    const y1 = fromPort ? fromPort.y : (from.y + from.height / 2);
    const x2 = toPort ? toPort.x : to.x;
    const y2 = toPort ? toPort.y : (to.y + to.height / 2);
    const midX = (x1 + x2) / 2;
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("d", `M ${x1} ${y1} C ${midX} ${y1}, ${midX} ${y2}, ${x2} ${y2}`);
    path.setAttribute("fill", "none");
    path.setAttribute("stroke-width", "2.2");
    path.setAttribute("vector-effect", "non-scaling-stroke");
    if (edge.type === "source") path.setAttribute("stroke-dasharray", "7 5");
    path.setAttribute("stroke", edgeStrokeColor(edge));
    if (isEdgeSelected(edge)) edgeLayerFront.appendChild(path);
    else edgeLayer.appendChild(path);
  });
  applyNodeZOrder();
}

function parameterRowsHtml(rows, ownerId, extra = false) {
  return (rows || []).map((row, index) => {
    const name = `${row.group ? `${row.group}/` : ""}${row.name}`;
    return `
      <div class="param-row">
        <input type="checkbox" ${row.enabled === false ? "" : "checked"} data-kind="toggle" data-owner="${escapeHtml(ownerId)}" data-index="${index}" data-extra="${extra ? "1" : "0"}">
        <div class="name">${escapeHtml(name)}</div>
        <input type="text" value="${escapeHtml(row.value || "")}" data-kind="value" data-owner="${escapeHtml(ownerId)}" data-index="${index}" data-extra="${extra ? "1" : "0"}">
      </div>
    `;
  }).join("");
}

function parameterValueRowsHtml(rows, ownerId, extra = false) {
  return window.lilakWebFormat.parameterValueSheetHtml(rows || [], { ownerId, extra, escapeHtml });
}

function cloneParameterRow(row, target) {
  const cloned = JSON.parse(JSON.stringify(row || {}));
  cloned._target = target;
  return cloned;
}

function parameterKey(row) {
  const group = row.group || "";
  const name = row.name || "";
  if (group === "lilak" && name === "add") return `${group}/${name}/${row.value || ""}`;
  return `${group}/${name}`;
}

function branchPersistencyRows(node) {
  return [{
    kind: "parameter",
    enabled: true,
    group: "persistency",
    name: node.branch_name || node.label || "branch",
    value: node.persist_disabled ? "false" : "true",
    unit: "",
    comment: "save branch to output file",
    _branchNodeId: node.id,
  }];
}

function rowsForParameterSheet(node) {
  if (!node) return { title: "Parameters", meta: "", rows: [] };
  if (node.kind === "run") {
    return { title: "Run Parameters", meta: "LKRun", rows: state.runRows.map((row, index) => cloneParameterRow(row, { type: "run", index })) };
  }
  if (node.kind === "parameter_container") {
    const rows = [];
    const seenKeys = new Set();
    const addRow = (row, target) => {
      if (!row || row.kind !== "parameter") return;
      if ((row.group || "").trim() === "persistency" && target.type !== "branch") return;
      const key = parameterKey(row);
      if (seenKeys.has(key)) return;
      seenKeys.add(key);
      rows.push(cloneParameterRow(row, target));
    };
    state.runRows.forEach((row, index) => addRow(row, { type: "run", index }));
    state.controlRows.forEach((row, index) => addRow(row, { type: "control", index }));
    for (const className of Object.keys(state.classData || {})) {
      const info = state.classData[className] || {};
      (info.parameter_rows || []).forEach((row, index) => addRow(row, { type: "class", className, set: "parameter_rows", index }));
      (info.extra_rows || []).forEach((row, index) => addRow(row, { type: "class", className, set: "extra_rows", index }));
    }
    state.nodes.filter((item) => item.kind === "branch").forEach((item) => {
      branchPersistencyRows(item).forEach((row) => addRow(row, { type: "branch", nodeId: item.id }));
    });
    return { title: "Parameter Container", meta: "editable values from current configuration", rows };
  }
  if (node.kind === "output_file") {
    const rows = [];
    state.runRows.forEach((row, index) => {
      if (["OutputPath", "FileName", "Tag", "AutoTerminate", "RunID"].includes(row.name)) {
        rows.push(cloneParameterRow(row, { type: "run", index }));
      }
    });
    return { title: "Output File Parameters", meta: "LKRun output settings", rows };
  }
  if (node.kind === "branch") {
    return { title: "Branch Parameters", meta: `${node.branch_name || ""} (${node.branch_class_name || ""})`, rows: branchPersistencyRows(node).map((row) => cloneParameterRow(row, { type: "branch", nodeId: node.id })) };
  }
  if (node.kind === "class") {
    const info = state.classData[node.class_name] || { parameter_rows: [], extra_rows: [] };
    const rows = [
      ...(info.parameter_rows || []).map((row, index) => cloneParameterRow(row, { type: "class", className: node.class_name, set: "parameter_rows", index })),
      ...(info.extra_rows || []).map((row, index) => cloneParameterRow(row, { type: "class", className: node.class_name, set: "extra_rows", index })),
    ];
    return { title: `${node.label} Parameters`, meta: node.class_name || "", rows };
  }
  return { title: "Parameters", meta: node.label || "", rows: [] };
}

function openParameterSheet(nodeId) {
  const node = nodeById(nodeId);
  const sheet = rowsForParameterSheet(node);
  state.parModal = { nodeId, rows: sheet.rows || [] };
  document.getElementById("parModalTitle").textContent = sheet.title;
  document.getElementById("parModalMeta").textContent = sheet.meta;
  const content = document.getElementById("parModalContent");
  const html = parameterValueRowsHtml(state.parModal.rows, "modal") || `<div class="help">No parameters.</div>`;
  content.innerHTML = `<div class="summary-card">${html}</div>`;
  content.querySelectorAll("input[data-kind='value']").forEach((input) => input.addEventListener("input", (event) => updateParamValue(event.target)));
  content.querySelectorAll("input[data-kind='unit']").forEach((input) => input.addEventListener("input", (event) => updateParamUnit(event.target)));
  content.querySelectorAll("input[data-kind='toggle']").forEach((input) => input.addEventListener("change", (event) => updateParamEnabled(event.target)));
  document.getElementById("parModal").classList.add("open");
}

function formatInputInfo(info) {
  const lines = [];
  for (const key of (info.keys || [])) lines.push(`${key.name} (${key.class_name})`);
  for (const tree of (info.trees || [])) {
    lines.push("");
    lines.push(`[${tree.name}]`);
    for (const branch of (tree.branches || [])) lines.push(`  ${branch}`);
  }
  if (info.error) {
    lines.push("");
    lines.push(`error: ${info.error}`);
  }
  return lines.join("\n");
}

function serializePreview() {
  const payload = buildSavePayload();
  const lines = [];
  for (const row of payload.run_rows) lines.push(renderRowText(row));
  if (payload.control_rows.length || payload.persistency_rows.length || payload.class_nodes.length) lines.push("");
  for (const row of payload.control_rows) lines.push(renderRowText(row));
  for (const node of payload.class_nodes) lines.push(`lilak/add ${node.class_name}`);
  if (payload.class_nodes.length) lines.push("");
  for (const node of payload.class_nodes) {
    for (const row of node.parameter_rows) lines.push(renderRowText(row));
    lines.push("");
  }
  for (const row of payload.persistency_rows) lines.push(renderRowText(row));
  return lines.join("\n");
}

function renderRowText(row) {
  if (row.kind === "comment") return `# ${row.comment || ""}`.trimEnd();
  const prefix = row.enabled === false || row.unit === "#" ? "#" : "";
  const unit = row.unit === "#" ? "" : (row.unit || "");
  const full = `${row.group ? `${row.group}/` : ""}${row.name}`;
  let text = `${prefix}${unit}${full}`;
  if ((row.value || "").trim()) text += `  ${row.value}`;
  if ((row.comment || "").trim()) text += `  # ${row.comment}`;
  return text;
}

function updateParamValue(input) {
  const index = Number(input.dataset.index);
  if (!state.parModal || !state.parModal.rows[index]) return;
  state.parModal.rows[index].value = input.value;
}

function updateParamUnit(input) {
  const index = Number(input.dataset.index);
  if (!state.parModal || !state.parModal.rows[index]) return;
  const row = state.parModal.rows[index];
  row.unit = input.value;
  const applyCommentState = (row) => {
    if (!row) return;
    row.enabled = input.value === "#" ? false : true;
    const tr = input.closest("tr");
    if (!tr) return;
    const valueInput = tr.querySelector("input[data-kind='value']");
    const commented = input.value === "#";
    tr.classList.toggle("commented-parameter-row", commented);
    if (valueInput) valueInput.disabled = commented;
  };
  applyCommentState(row);
}

function updateParamEnabled(input) {
  const index = Number(input.dataset.index);
  if (!state.parModal || !state.parModal.rows[index]) return;
  state.parModal.rows[index].enabled = input.checked;
}

function syncBranchPersistEdge(node) {
  if (!node || node.kind !== "branch") return;
  state.edges = state.edges.filter((edge) => !(edge.from === node.id && edge.to === "output_file" && edge.type === "output"));
  if (!node.persist_disabled) {
    state.edges.push({ from: node.id, to: "output_file", type: "output" });
  }
}

function saveOpenParameterSheet() {
  if (!state.parModal || !Array.isArray(state.parModal.rows)) return;
  for (const row of state.parModal.rows) {
    const target = row._target || {};
    const saved = JSON.parse(JSON.stringify(row));
    delete saved._target;
    if (target.type === "run" && state.runRows[target.index]) {
      state.runRows[target.index] = saved;
    } else if (target.type === "control" && state.controlRows[target.index]) {
      state.controlRows[target.index] = saved;
    } else if (target.type === "class") {
      const info = state.classData[target.className];
      const rows = info ? info[target.set] : null;
      if (rows && rows[target.index]) rows[target.index] = saved;
    } else if (target.type === "branch") {
      const node = nodeById(target.nodeId);
      if (node) {
        node.persist_disabled = new Set(["false", "0", "no", "off"]).has(String(saved.value || "").trim().toLowerCase());
        syncBranchPersistEdge(node);
      }
    }
  }
  renderStatusPreview();
  render();
  logMessage("Parameter changes saved to current configuration.");
}

function addParameterRow(owner, isExtra) {
  if (owner === "run") {
    state.runRows.push({ kind: "parameter", enabled: true, group: "LKRun", name: "NewParameter", value: "", unit: "", comment: "" });
  } else {
    const info = state.classData[owner];
    if (!info) return;
    const target = isExtra ? info.extra_rows : info.parameter_rows;
    target.push({ kind: "parameter", enabled: true, group: owner, name: "NewParameter", value: "", unit: "", comment: "" });
  }
  renderInspector();
  renderStatusPreview();
}

function renderInspector() {
  renderStatusPreview();
}

function renderStatusPreview() {
  const box = document.getElementById("statusLog");
  if (box) box.textContent = serializePreview() || "Waiting for save.";
}

async function openClassPicker(kind) {
  state.classPickerKind = kind;
  const payload = await fetchJson("/api/class_list", { kind });
  document.getElementById("taskModalMeta").textContent = kind === "detector" ? "Add Detector / Detector Plane" : "Add Task";
  const list = document.getElementById("taskCandidateList");
  list.className = "navigator-list";
  window.lilakWebFormat.renderNavigatorEntries({
    listId: "taskCandidateList",
    entries: (payload.entries || []).map((entry) => ({
      ...entry,
      class_name: entry.name,
      name: entry.class_kind ? `${entry.name} (${entry.class_kind})` : entry.name,
      kind_label: entry.kind_label || entry.class_kind || "class",
    })),
    escapeHtml,
    parameterPredicate: () => false,
    handlers: {
      clickFile: (entry) => addStandaloneClass(entry.class_name || entry.name),
      openFile: (entry) => addStandaloneClass(entry.class_name || entry.name),
    },
  });
  if (!payload.entries || payload.entries.length === 0) {
    list.innerHTML = `<div class="flow-meta">No classes found.</div>`;
  }
  document.getElementById("taskModal").classList.add("open");
}

async function openTaskCandidates(nodeId) {
  const node = nodeById(nodeId);
  if (!node) return;
  state.taskBranch = node;
  const payload = await fetchJson("/api/class_candidates", {
    branch_name: node.branch_name || "",
    branch_class_name: node.branch_class_name || "",
  });
  document.getElementById("taskModalMeta").textContent = `${node.branch_name} (${node.branch_class_name})`;
  const list = document.getElementById("taskCandidateList");
  list.className = "navigator-list";
  window.lilakWebFormat.renderNavigatorEntries({
    listId: "taskCandidateList",
    entries: (payload.entries || []).map((entry) => ({
      ...entry,
      detail: entry.detail || "",
      kind_label: "task",
    })),
    escapeHtml,
    parameterPredicate: () => false,
    handlers: {
      clickFile: (entry) => addTaskFromClass(entry.name),
      openFile: (entry) => addTaskFromClass(entry.name),
    },
  });
  if (!payload.entries || payload.entries.length === 0) {
    list.innerHTML = `<div class="flow-meta">No matching tasks found.</div>`;
  }
  document.getElementById("taskModal").classList.add("open");
}

async function addStandaloneClass(className) {
  const bundle = await fetchJson("/api/class_bundle", {
    class_name: className,
    all_rows: state.rows,
  });
  const nextIndex = state.nodes.filter((node) => node.kind === "class").length;
  const nodeId = `class_${Date.now()}`;
  state.classData[className] = bundle;
  state.nodes.push({
    id: nodeId,
    kind: "class",
    class_kind: bundle.class_kind || "task",
    label: className,
    subtitle: bundle.class_kind || "class",
    x: 830,
    y: 36 + nextIndex * 112,
    doc_url: bundle.doc_url || "",
    header_path: bundle.header_path || "",
    class_name: className,
  });
  document.getElementById("taskModal").classList.remove("open");
  state.selectedId = nodeId;
  render();
}

async function addTaskFromClass(className) {
  const branch = state.taskBranch;
  if (!branch) return;
  const bundle = await fetchJson("/api/class_bundle", {
    class_name: className,
    all_rows: state.rows,
  });
  const nextIndex = state.nodes.filter((node) => node.kind === "class").length;
  const nodeId = `class_${Date.now()}`;
  state.classData[className] = bundle;
  state.nodes.push({
    id: nodeId,
    kind: "class",
    class_kind: bundle.class_kind || "task",
    label: className,
    subtitle: bundle.class_kind || "task",
    x: 830,
    y: 36 + nextIndex * 112,
    doc_url: bundle.doc_url || "",
    class_name: className,
  });
  const ensureBranch = (entry) => {
    const existing = state.nodes.find((node) => node.kind === "branch" && node.branch_name === entry.branch_name && node.branch_class_name === entry.class_name);
    if (existing) return existing.id;
    const branchNodes = state.nodes.filter((node) => node.kind === "branch");
    const branchNode = {
      id: `branch_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`,
      kind: "branch",
      label: entry.branch_name,
      subtitle: entry.class_name,
      x: 510,
      y: 36 + branchNodes.length * 92,
      style: "branch_pass",
      branch_name: entry.branch_name,
      branch_class_name: entry.class_name,
      doc_url: entry.doc_url || "",
    };
    state.nodes.push(branchNode);
    return branchNode.id;
  };
  for (const use of (bundle.uses || [])) {
    const bid = ensureBranch(use);
    state.edges.push({ from: bid, to: nodeId, type: "read" });
  }
  for (const save of (bundle.saves || [])) {
    if (save.action === "keep") continue;
    const bid = ensureBranch(save);
    state.edges.push({ from: nodeId, to: bid, type: "write" });
    if (!state.edges.some((edge) => edge.from === bid && edge.to === "output_file")) {
      state.edges.push({ from: bid, to: "output_file", type: "output" });
    }
  }
  document.getElementById("taskModal").classList.remove("open");
  state.selectedId = nodeId;
  render();
}

function buildSavePayload() {
  const seenClassParameterKeys = new Set();
  const classNodes = state.nodes.filter((node) => node.kind === "class").map((node) => {
    const info = state.classData[node.class_name] || {};
    const rows = [...(info.parameter_rows || []), ...(info.extra_rows || [])].filter((row) => {
      if (!row || row.kind !== "parameter") return true;
      if ((row.group || "").trim() === "persistency") return false;
      const key = `${row.group || ""}/${row.name || ""}`;
      if (seenClassParameterKeys.has(key)) return false;
      seenClassParameterKeys.add(key);
      return true;
    });
    return {
      class_name: node.class_name,
      parameter_rows: rows,
    };
  });
  const persistencyRows = state.nodes
    .filter((node) => node.kind === "branch" && node.branch_name)
    .map((node) => ({
      kind: "parameter",
      enabled: true,
      group: "persistency",
      name: node.branch_name,
      value: node.persist_disabled ? "false" : "true",
      unit: "",
      comment: "save branch to output file",
    }));
  return {
    path: state.savePath,
    all_rows: state.rows,
    run_rows: state.runRows,
    control_rows: state.controlRows,
    persistency_rows: persistencyRows,
    class_nodes: classNodes,
  };
}

async function saveConfiguration(forcePath = false) {
  const configFileName = `${normalizedConfigName(state.configurationName)}.mac`;
  if (state.savePath) {
    const dir = state.savePath.split("/").slice(0, -1).join("/");
    state.savePath = `${dir}/${configFileName}`;
  }
  if (!forcePath) {
    openNavigator("save");
    return;
  }
  if (!state.savePath) {
    openNavigator("save");
    return;
  }
  const result = await fetchJson("/api/save", buildSavePayload());
  state.savePath = result.path;
  renderParameterTabs();
  logMessage(`Saved ${result.path}`);
}

function isLoadableParameterFile(name) {
  const lower = (name || "").toLowerCase();
  return lower.endsWith(".mac") || lower.endsWith(".conf") || lower.endsWith(".par");
}

function baseName(path) {
  return String(path || "").split("/").pop();
}

function currentParameterTabLabel() {
  const path = state.savePath || state.sourcePath || "";
  if (isLoadableParameterFile(path)) return baseName(path);
  return `${normalizedConfigName(state.configurationName)}.mac`;
}

function navigatorDefaultFileName(mode) {
  if (mode === "load") {
    if (state.sourcePath) return baseName(state.sourcePath);
    if (state.savePath) return baseName(state.savePath);
    return "configure_flow.mac";
  }
  return state.savePath ? baseName(state.savePath) : `${normalizedConfigName(state.configurationName)}.mac`;
}

function openNavigator(mode) {
  state.navMode = mode || "save";
  document.getElementById("navigatorModal").classList.add("open");
  document.getElementById("navigatorTitle").textContent = state.navMode === "load" ? "Load Parameter File" : "Save Parameter File";
  document.getElementById("navConfirm").textContent = state.navMode === "load" ? "Open" : "Save";
  document.getElementById("navPath").value = state.savePath ? state.savePath.split("/").slice(0, -1).join("/") : state.cwd;
  document.getElementById("navFile").value = state.savePath ? state.savePath.split("/").pop() : `${normalizedConfigName(state.configurationName)}.mac`;
  loadNavigator(document.getElementById("navPath").value);
  setTimeout(() => {
    const input = document.getElementById("navFile");
    input.focus();
    input.select();
  }, 0);
}

function closeNavigator() {
  document.getElementById("navigatorModal").classList.remove("open");
}

function confirmNavigatorOpenFromEntry(entryName) {
  document.getElementById("navFile").value = entryName;
  if (state.navMode === "load" && isLoadableParameterFile(entryName)) {
    confirmNavigatorAction();
  }
}

async function loadNavigator(path) {
  const payload = await fetchJson("/api/listdir", { path });
  document.getElementById("navPath").value = payload.current_dir;
  window.lilakWebFormat.renderNavigatorEntries({
    listId: "navList",
    entries: payload.entries || [],
    escapeHtml,
    parameterPredicate: isLoadableParameterFile,
    handlers: {
      clickDir: entry => loadNavigator(entry.path),
      clickFile: entry => { document.getElementById("navFile").value = entry.name; },
      openDir: entry => loadNavigator(entry.path),
      openFile: entry => confirmNavigatorOpenFromEntry(entry.name),
    },
  });
}

async function confirmNavigatorAction() {
  const rawPath = document.getElementById("navPath").value.trim() || state.cwd;
  const file = document.getElementById("navFile").value.trim() || `${normalizedConfigName(state.configurationName)}.mac`;
  const fullPath = `${rawPath.replace(/\/+$/, "")}/${file}`;
  closeNavigator();
  if (state.navMode === "load") {
    await loadConfiguration(fullPath);
    return;
  }
  state.savePath = fullPath;
  await saveConfiguration(true);
}

async function loadConfiguration(path) {
  const payload = await fetchJson("/api/load", { path });
  storeActiveParameterTab();
  applyBootstrapPayload(payload, false);
  const tabLabel = baseName(payload.save_path || payload.source_path || path || "configure_flow.mac");
  const tab = makeParameterTabFromCurrent(tabLabel);
  state.parameterTabs.push(tab);
  state.activeParameterTabId = tab.id;
  arrangeNodes();
  render();
  logMessage(`Loaded ${path}`);
}

function applyBootstrapPayload(payload, renderNow = true) {
  state.cwd = payload.cwd || "";
  state.commonDir = payload.common_dir || "";
  state.sourceKind = payload.source_kind || "blank";
  state.sourcePath = payload.source_path || "";
  state.savePath = payload.save_path || "";
  state.rows = payload.rows || [];
  state.runRows = payload.run_rows || [];
  state.controlRows = payload.control_rows || [];
  state.nodes = payload.nodes || [];
  state.edges = payload.edges || [];
  state.classData = payload.class_data || {};
  state.inputInfo = payload.input_info || {};
  state.configurationName = stripExtension(payload.save_path || payload.source_path || "configure_flow");
  state.selectedId = state.nodes.length ? state.nodes[0].id : "";
  if (renderNow) {
    arrangeNodes();
    render();
  }
}

function layoutColumn(nodes, startX, startY, yFromMap = null) {
  let baseX = startX;
  let cursorY = startY;
  let countInBlock = 0;
  nodes.forEach((node, index) => {
    const refY = yFromMap && yFromMap[node.id] != null ? yFromMap[node.id] : null;
    node.x = baseX + (countInBlock * 18);
    node.y = refY != null ? refY : cursorY;
    cursorY += 156;
    countInBlock += 1;
    if (countInBlock >= 4) {
      countInBlock = 0;
      baseX = startX;
    }
  });
}

function classPriorityName(node) {
  return node.class_name || node.label || "";
}

function flowRootPriority(node) {
  const name = classPriorityName(node);
  if (name === "LKGETConversionTask") return 0;
  if (name === "LKGETConverter") return 1;
  return 10;
}

function sortedByFlowPriority(nodes, previousY = {}) {
  return [...nodes].sort((a, b) => {
    const ay = previousY[a.id];
    const by = previousY[b.id];
    if (ay != null && by != null && ay !== by) return ay - by;
    if (ay != null && by == null) return -1;
    if (ay == null && by != null) return 1;
    const rootDiff = flowRootPriority(a) - flowRootPriority(b);
    if (rootDiff !== 0) return rootDiff;
    return (a.label || "").localeCompare(b.label || "");
  });
}

function buildFlowOrder(classNodes, branchNodes) {
  const nodeMap = new Map([...classNodes, ...branchNodes].map((node) => [node.id, node]));
  const forward = new Map();
  const backward = new Map();
  const addForward = (from, to) => {
    if (!nodeMap.has(from) || !nodeMap.has(to)) return;
    if (!forward.has(from)) forward.set(from, []);
    if (!backward.has(to)) backward.set(to, []);
    forward.get(from).push(to);
    backward.get(to).push(from);
  };
  for (const edge of state.edges) {
    if (edge.type === "source") addForward(edge.from, edge.to);
    if (edge.type === "write") addForward(edge.from, edge.to);
    if (edge.type === "read") addForward(edge.from, edge.to);
  }

  let roots = classNodes.filter((node) => flowRootPriority(node) < 10);
  if (roots.length === 0) {
    roots = classNodes.filter((node) => !(backward.get(node.id) || []).some((id) => nodeMap.get(id)?.kind === "branch"));
  }
  roots = sortedByFlowPriority(roots);

  const levelById = new Map();
  const queue = roots.map((node) => node.id);
  roots.forEach((node) => levelById.set(node.id, 0));
  while (queue.length) {
    const id = queue.shift();
    const level = levelById.get(id) || 0;
    for (const next of forward.get(id) || []) {
      const nextLevel = level + 1;
      if (!levelById.has(next) || nextLevel < levelById.get(next)) {
        levelById.set(next, nextLevel);
        queue.push(next);
      }
    }
  }

  const levels = [];
  const addToLevel = (node, level) => {
    if (!levels[level]) levels[level] = [];
    levels[level].push(node);
  };
  for (const node of [...classNodes, ...branchNodes]) {
    if (levelById.has(node.id)) addToLevel(node, levelById.get(node.id));
  }

  let fallbackLevel = levels.length;
  for (const node of sortedByFlowPriority([...classNodes, ...branchNodes].filter((node) => !levelById.has(node.id)))) {
    addToLevel(node, fallbackLevel);
    fallbackLevel += 1;
  }

  const orderById = {};
  let order = 0;
  const previousOrder = {};
  for (const levelNodes of levels) {
    if (!levelNodes || levelNodes.length === 0) continue;
    for (const node of levelNodes) {
      const refs = (backward.get(node.id) || []).map((id) => orderById[id]).filter((value) => value != null);
      if (refs.length) previousOrder[node.id] = refs.reduce((sum, value) => sum + value, 0) / refs.length;
    }
    const ordered = sortedByFlowPriority(levelNodes, previousOrder);
    for (const node of ordered) {
      orderById[node.id] = order++;
    }
  }
  return orderById;
}

function arrangeNodes() {
  const runNodes = state.nodes.filter((node) => node.kind === "run");
  const parNodes = state.nodes.filter((node) => node.kind === "parameter_container");
  const inputNodes = state.nodes.filter((node) => node.kind === "input_file");
  const branchNodes = state.nodes.filter((node) => node.kind === "branch");
  const classNodes = state.nodes.filter((node) => node.kind === "class");
  const outputNodes = state.nodes.filter((node) => node.kind === "output_file");
  const topY = 36;
  const flowOrder = buildFlowOrder(classNodes, branchNodes);
  const byFlowOrder = (a, b) => {
    const ay = flowOrder[a.id] ?? 999999;
    const by = flowOrder[b.id] ?? 999999;
    if (ay !== by) return ay - by;
    return (a.label || "").localeCompare(b.label || "");
  };
  layoutColumn(inputNodes, 36, topY);
  layoutColumn(runNodes, 36, topY + 156);
  layoutColumn(parNodes, 36, topY + 312);
  const orderedBranches = [...branchNodes].sort(byFlowOrder);
  const orderedClasses = [...classNodes].sort(byFlowOrder);
  layoutColumn(orderedBranches, 320, topY);
  const classYMap = {};
  for (const node of orderedClasses) {
    const firstInputEdge = state.edges.find((edge) => edge.type === "read" && edge.to === node.id);
    const inputBranch = firstInputEdge ? nodeById(firstInputEdge.from) : null;
    if (inputBranch && inputBranch.kind === "branch") {
      classYMap[node.id] = inputBranch.y;
    }
  }
  layoutColumn(orderedClasses, 600, topY, classYMap);
  layoutColumn(outputNodes, 36, topY + 468);
  render();
}

function toggleSummaryPanel() {
  window.lilakWebFormat.toggleCollapsiblePanel({
    workspace: "workspace",
    button: "summaryToggleBtn",
    afterToggle: renderCanvas,
  });
}

function toggleShortcutsModal() {
  window.lilakWebFormat.toggleShortcutsModal();
}

function render() {
  renderParameterTabs();
  renderSummary();
  renderCanvas();
  renderInspector();
  renderStatusPreview();
}

function cycleSelection(step) {
  const items = visibleNodesSorted();
  if (!items.length) return;
  const index = Math.max(0, items.findIndex((node) => node.id === state.selectedId));
  const next = (index + step + items.length) % items.length;
  state.selectedId = items[next].id;
  render();
}

async function bootstrap() {
  const payload = await fetchJson("/api/bootstrap");
  applyBootstrapPayload(payload);
  logMessage(state.sourcePath ? `Loaded ${state.sourcePath}` : "Opened blank configure flow");
}

function openParameterLoad() {
  openNavigator("load");
}

function openAddMenu(kind) {
  if (kind === "detector") {
    openClassPicker("detector");
    return;
  }
  if (kind === "task") {
    openClassPicker("task");
    return;
  }
  if (kind === "branch") {
    const node = nodeById(state.selectedId);
    if (!node || node.kind !== "branch") {
      logMessage("Select a branch first.", true);
      return;
    }
    openTaskCandidates(node.id);
    return;
  }
  logMessage("Add is reserved here.");
}

document.getElementById("loadParameterBtn").addEventListener("click", openParameterLoad);
document.getElementById("addDetectorBtn").addEventListener("click", () => openAddMenu("detector"));
document.getElementById("addTaskBtn").addEventListener("click", () => openAddMenu("task"));
document.getElementById("addForBranchBtn").addEventListener("click", () => openAddMenu("branch"));
document.getElementById("arrangeBtn").addEventListener("click", arrangeNodes);
document.getElementById("summaryToggleBtn").addEventListener("click", toggleSummaryPanel);
document.getElementById("navGo").addEventListener("click", () => loadNavigator(document.getElementById("navPath").value));
document.getElementById("navUp").addEventListener("click", () => {
  const current = document.getElementById("navPath").value.trim();
  const next = current.replace(/\/+$/, "").split("/").slice(0, -1).join("/") || "/";
  loadNavigator(next);
});
document.getElementById("navHome").addEventListener("click", () => loadNavigator(state.cwd || "/"));
document.getElementById("navCommon").addEventListener("click", () => loadNavigator(state.commonDir || state.cwd || "/"));
document.getElementById("navConfirm").addEventListener("click", confirmNavigatorAction);
document.getElementById("navClose").addEventListener("click", closeNavigator);
document.getElementById("taskModalClose").addEventListener("click", () => document.getElementById("taskModal").classList.remove("open"));
document.getElementById("parModalSave").addEventListener("click", saveOpenParameterSheet);
document.getElementById("parModalClose").addEventListener("click", () => document.getElementById("parModal").classList.remove("open"));
document.getElementById("shortcutsBtn").addEventListener("click", toggleShortcutsModal);
document.getElementById("shortcutsClose").addEventListener("click", () => document.getElementById("shortcutsModal").classList.remove("open"));

document.addEventListener("mousemove", (event) => {
  if (!state.drag) return;
  const node = nodeById(state.drag.nodeId);
  if (!node) return;
  const canvas = document.getElementById("canvas");
  const rect = canvas.getBoundingClientRect();
  const width = 220;
  const height = 120;
  const nextX = Math.max(0, Math.min((canvas.clientWidth || 1400) - width, event.clientX - rect.left - state.drag.offsetX));
  const nextY = Math.max(0, Math.min((canvas.clientHeight || 1520) - height, event.clientY - rect.top - state.drag.offsetY));
  node.x = nextX;
  node.y = nextY;
  renderCanvas();
  renderSummary();
});

document.addEventListener("mouseup", () => {
  state.drag = null;
});

let resizeRenderPending = false;
window.addEventListener("resize", () => {
  if (resizeRenderPending) return;
  resizeRenderPending = true;
  requestAnimationFrame(() => {
    resizeRenderPending = false;
    renderCanvas();
  });
});

document.addEventListener("keydown", (event) => {
  const tag = document.activeElement ? document.activeElement.tagName : "";
  const isEditing = tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT";
  if (event.key === "Escape") {
    document.getElementById("taskModal").classList.remove("open");
    document.getElementById("parModal").classList.remove("open");
    document.getElementById("shortcutsModal").classList.remove("open");
    closeNavigator();
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "h") {
    event.preventDefault();
    toggleSummaryPanel();
  }
  if (!isEditing && event.key === "?") {
    event.preventDefault();
    toggleShortcutsModal();
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "p") {
    event.preventDefault();
    if (state.selectedId) openParameterSheet(state.selectedId);
    return;
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "s") {
    event.preventDefault();
    if (document.getElementById("parModal").classList.contains("open")) {
      saveOpenParameterSheet();
      return;
    }
    saveConfiguration();
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "l") {
    event.preventDefault();
    openNavigator("load");
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "a") {
    event.preventDefault();
    arrangeNodes();
  }
  if (document.getElementById("navigatorModal").classList.contains("open") && event.key === "Enter") {
    event.preventDefault();
    confirmNavigatorAction();
  }
  if (!isEditing && event.key === "Tab") {
    event.preventDefault();
    cycleSelection(event.shiftKey ? -1 : 1);
  }
  if (!isEditing && (event.key === "Backspace" || event.key === "Delete")) {
    event.preventDefault();
    deleteSelection();
  }
});

bootstrap().catch((error) => {
  logMessage(error.message || String(error), true);
});
"""

HTML_PAGE = build_lilak_web_page("LILAK Configuration", CONFIGURE_BODY_HTML, CONFIGURE_SCRIPT_JS, CONFIGURE_EXTRA_STYLE)


def pick_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def main():
    parser = argparse.ArgumentParser(description="LILAK configure flow editor")
    parser.add_argument("path", nargs="?", help="parameter/root/mfm file")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()

    initial_path = None
    if args.path:
        candidate = Path(os.path.expanduser(os.path.expandvars(args.path)))
        if not candidate.is_absolute():
            candidate = (Path.cwd() / candidate).resolve()
        initial_path = candidate

    host = args.host
    port = args.port or pick_port()
    server = ThreadingHTTPServer((host, port), ConfigureFlowHandler)
    server.initial_path = initial_path
    url = f"http://{host}:{port}/"
    print(f"Configure flow editor: {url}")
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
