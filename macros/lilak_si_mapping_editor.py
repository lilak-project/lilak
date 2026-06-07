#!/usr/bin/env python3

import argparse
import re
import json
import os
import shutil
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

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from lilak_web_format import rebuild_legacy_page


DEFAULT_DETECTOR_DB = {
    "detectors": [
        {
            "number": index,
            "thickness_um": 1000,
            "bias_voltage_v": 0,
            "active_width_mm": 40.3,
            "active_height_mm": 75.0,
            "note": "",
        }
        for index in range(1, 101)
    ]
}

DEFAULT_GROUP_DB = {
    "groups": [
        {"name": "Ring-12 (dE)", "capacity": 12, "radius_mm": 109.2, "z_mm": 150.0, "phi_offset_deg": -15.0, "half_opening_deg": 10.45483508},
        {"name": "Ring-12 (E)", "capacity": 12, "radius_mm": 129.2, "z_mm": 170.0, "phi_offset_deg": -15.0, "half_opening_deg": 10.45483508},
        {"name": "Ring-16", "capacity": 16, "radius_mm": 150.0, "z_mm": 200.0, "phi_offset_deg": 0.0, "half_opening_deg": 8.0},
        {"name": "QQQ5 plane", "capacity": 4, "radius_mm": 95.0, "z_mm": 120.0, "phi_offset_deg": 45.0, "half_opening_deg": 20.0},
        {"name": "Free", "capacity": 0, "radius_mm": 0.0, "z_mm": 0.0, "phi_offset_deg": 0.0, "half_opening_deg": 10.0},
    ]
}

GROUP_PRESET_ALIASES = {
    "R12-I": "Ring-12 (dE)",
    "R12-O": "Ring-12 (E)",
    "R16": "Ring-16",
    "PQ": "QQQ5 plane",
    "U": "Free",
}

DEFAULT_BOARD_CONFIGS = {
    "board_types": [
        {
            "key": "X6",
            "detector_label": "X6",
            "merging_label": "X6 Merging",
            "zap_label": "X6/QQQ5 ZAP/AsAd",
            "junction_strip_count": 16,
            "ohmic_strip_count": 4,
            "merging_slots": [
                {"id": "A", "label": "A"},
                {"id": "B", "label": "B"},
                {"id": "C", "label": "C"},
                {"id": "D", "label": "D"},
            ],
            "zap_pins": [
                {"id": "PIN1", "label": "Pin 1 > AGET#3, AGET#0 (0-15)", "junction_aget": 3, "junction_first_channel": 0, "ohmic_aget": 0, "ohmic_first_channel": 0},
                {"id": "PIN2", "label": "Pin 2 > AGET#2, AGET#0 (16-31)", "junction_aget": 2, "junction_first_channel": 0, "ohmic_aget": 0, "ohmic_first_channel": 16},
                {"id": "PIN3", "label": "Pin 3 > AGET#1, AGET#0 (32-47)", "junction_aget": 1, "junction_first_channel": 0, "ohmic_aget": 0, "ohmic_first_channel": 32},
            ],
        },
        {
            "key": "BB10",
            "detector_label": "BB10",
            "merging_label": "BB10 Merging",
            "zap_label": "BB10 ZAP/AsAd",
            "junction_strip_count": 8,
            "ohmic_strip_count": 1,
            "merging_slots": [
                {"id": "A", "label": "A"},
                {"id": "B", "label": "B"},
                {"id": "C", "label": "C"},
                {"id": "D", "label": "D"},
            ],
            "zap_pins": [
                {"id": "PIN1_1", "label": "Pin 1-1 > AGET#3 (0-31), AGET#0 (0-3)", "junction_aget": 3, "junction_first_channel": 0, "ohmic_aget": 0, "ohmic_first_channel": 0},
                {"id": "PIN1_2", "label": "Pin 1-2 > AGET#3 (32-63), AGET#0 (4-7)", "junction_aget": 3, "junction_first_channel": 32, "ohmic_aget": 0, "ohmic_first_channel": 4},
                {"id": "PIN2_1", "label": "Pin 2-1 > AGET#2 (0-31), AGET#0 (8-11)", "junction_aget": 2, "junction_first_channel": 0, "ohmic_aget": 0, "ohmic_first_channel": 8},
                {"id": "PIN2_2", "label": "Pin 2-2 > AGET#2 (32-63), AGET#0 (12-15)", "junction_aget": 2, "junction_first_channel": 32, "ohmic_aget": 0, "ohmic_first_channel": 12},
                {"id": "PIN3_1", "label": "Pin 3-1 > AGET#1 (0-31), AGET#0 (16-19)", "junction_aget": 1, "junction_first_channel": 0, "ohmic_aget": 0, "ohmic_first_channel": 16},
                {"id": "PIN3_2", "label": "Pin 3-2 > AGET#1 (32-63), AGET#0 (20-23)", "junction_aget": 1, "junction_first_channel": 32, "ohmic_aget": 0, "ohmic_first_channel": 20},
            ],
        },
        {
            "key": "QQQ5",
            "detector_label": "QQQ5",
            "merging_label": "QQQ5 Merging",
            "zap_label": "X6/QQQ5 ZAP/AsAd",
            "junction_strip_count": 32,
            "ohmic_strip_count": 4,
            "merging_slots": [
                {"id": "A", "label": "A"},
                {"id": "B", "label": "B"},
            ],
            "zap_pins": [
                {"id": "PIN1", "label": "Pin 1 > AGET#3 (0-63), AGET#0 (0-7)", "junction_aget": 3, "junction_first_channel": 0, "ohmic_aget": 0, "ohmic_first_channel": 0},
                {"id": "PIN2", "label": "Pin 2 > AGET#2 (0-63), AGET#0 (16-23)", "junction_aget": 2, "junction_first_channel": 0, "ohmic_aget": 0, "ohmic_first_channel": 16},
                {"id": "PIN3", "label": "Pin 3 > AGET#1 (0-63), AGET#0 (33-40)", "junction_aget": 1, "junction_first_channel": 0, "ohmic_aget": 0, "ohmic_first_channel": 33},
            ],
        },
    ]
}


def lilak_root_path() -> Path:
    lilak_path = os.environ.get("LILAK_PATH", "")
    if lilak_path:
        return Path(lilak_path).resolve()
    return Path(__file__).resolve().parents[1]


def data_dir() -> Path:
    path = lilak_root_path() / "common" / "si_mapping"
    path.mkdir(parents=True, exist_ok=True)
    return path


def detector_db_path() -> Path:
    return data_dir() / "detector_db.par"


def group_db_path() -> Path:
    return data_dir() / "group_db.par"


def mapping_store_dir() -> Path:
    return data_dir()


def rings_db_path() -> Path:
    return data_dir() / "rings.par"


def board_config_path() -> Path:
    return data_dir() / "board_types.par"


def ensure_json_file(path: Path, default_payload):
    if not path.exists():
        path.write_text(json.dumps(default_payload, indent=2), encoding="utf-8")


def load_json(path: Path, default_payload):
    ensure_json_file(path, default_payload)
    return json.loads(path.read_text(encoding="utf-8"))


def save_json(path: Path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def legacy_detector_db_json_path() -> Path:
    return data_dir() / "detectors.json"


def legacy_rings_db_json_path() -> Path:
    return data_dir() / "rings.json"


def quote_parameter_value(value: str) -> str:
    return json.dumps(value)


def should_insert_list_spacing(prefix: str) -> bool:
    return prefix in {
        "board_types",
        "detector_db/detectors",
        "group_db/groups",
        "rings/groups",
    }


def flatten_parameter_payload(lines: list[str], prefix: str, value):
    if isinstance(value, dict):
        if not value:
            lines.append(f"{prefix} {json.dumps({})}")
            return
        items = list(value.items())
        for index, (key, item) in enumerate(items):
            if prefix == "" and index > 0 and lines and lines[-1] != "":
                lines.append("")
            flatten_parameter_payload(lines, f"{prefix}/{key}" if prefix else str(key), item)
        return
    if isinstance(value, list):
        if not value:
            lines.append(f"{prefix} {json.dumps([])}")
            return
        for index, item in enumerate(value):
            if index > 0 and should_insert_list_spacing(prefix) and lines and lines[-1] != "":
                lines.append("")
            flatten_parameter_payload(lines, f"{prefix}/{index}" if prefix else str(index), item)
        return
    lines.append(f"{prefix} {json.dumps(value)}")


def serialize_parameter_bundle(bundle_type: str, payload: dict) -> str:
    lines = [
        '# silicon mapping parameter file',
        f'si_mapping/type {json.dumps(bundle_type)}',
    ]
    flatten_parameter_payload(lines, "", payload)
    return "\n".join(lines) + "\n"


def assign_nested_value(container, segments: list[str], value):
    current = container
    for index, segment in enumerate(segments):
        last = index == len(segments) - 1
        is_list_index = segment.isdigit()
        next_is_list_index = (index + 1 < len(segments) and segments[index + 1].isdigit())

        if is_list_index:
            list_index = int(segment)
            while len(current) <= list_index:
                current.append(None)
            if last:
                current[list_index] = value
            else:
                if current[list_index] is None:
                    current[list_index] = [] if next_is_list_index else {}
                current = current[list_index]
            continue

        if last:
            current[segment] = value
        else:
            if segment not in current or current[segment] is None:
                current[segment] = [] if next_is_list_index else {}
            current = current[segment]


def parse_parameter_bundle(content: str) -> dict:
    bundle_type = ""
    payload = {}
    pending_lines = list(content.splitlines())
    embedded_key_pattern = re.compile(r"^(.*?)\s+([A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)+)\s+(.+)$")
    embedded_key_roots = {"experiment", "detector_db", "group_db", "board_types", "rings", "state"}
    line_index = 0
    while line_index < len(pending_lines):
        raw = pending_lines[line_index]
        line_index += 1
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        key, raw_value = parts
        try:
            value = json.loads(raw_value)
        except json.JSONDecodeError:
            value = raw_value.strip().strip('"')

        if isinstance(value, str):
            match = embedded_key_pattern.match(value)
            if match is not None:
                first_value = match.group(1).strip()
                embedded_key = match.group(2).strip()
                embedded_value = match.group(3).strip()
                key_root = embedded_key.split("/", 1)[0]
                if key_root in embedded_key_roots:
                    if first_value:
                        try:
                            value = json.loads(first_value)
                        except json.JSONDecodeError:
                            value = first_value.strip('"')
                    else:
                        value = ""
                    pending_lines.insert(line_index, f"{embedded_key} {embedded_value}")

        if key == "si_mapping/type":
            bundle_type = str(value)
            continue
        if key == "si_mapping/payload":
            try:
                payload = json.loads(value)
            except Exception as error:
                raise RuntimeError(f"Cannot parse legacy mapping payload: {error}") from error
            continue

        assign_nested_value(payload, [segment for segment in key.split("/") if segment], value)

    return {"type": bundle_type, "payload": payload}


def make_empty_state():
    timestamp = int(time.time() * 1000)
    return {
        "version": 1,
        "updated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "experiments": [
            {
                "id": f"exp-{timestamp}",
                "name": "Experiment 1",
                "groups": [
                    {
                        "id": f"group-{timestamp}",
                        "name": "Group 1",
                        "nodes": [],
                        "connections": [],
                    }
                ],
                "placeholders": [],
                "loose_nodes": [],
            }
        ],
    }


def resolve_state_path(file_name: str | None) -> Path | None:
    if not file_name:
        return None
    path = Path(os.path.expanduser(os.path.expandvars(file_name)))
    if not path.is_absolute():
        path = (Path.cwd() / path).resolve()
    return path


def resolve_initial_input(file_name: str | None) -> Path | None:
    path = resolve_state_path(file_name)
    if path is None or not path.exists():
        return None
    if path.is_dir():
        conf_files = sorted(item for item in path.iterdir() if item.is_file() and item.suffix.lower() == ".conf")
        if not conf_files:
            return None
        preferred = path / f"{safe_experiment_name(path.name)}.conf"
        if preferred.exists():
            return preferred
        experiment_like = [item for item in conf_files if item.name not in {"group_db.conf", "board_types.conf", "si_group_db.conf"}]
        return experiment_like[0] if experiment_like else conf_files[0]
    return path


def load_state(initial_path: Path | None):
    if initial_path is None or not initial_path.is_file():
        return make_empty_state()
    content = initial_path.read_text(encoding="utf-8")
    if initial_path.suffix.lower() == ".json":
        return json.loads(content)
    bundle = parse_parameter_bundle(content)
    payload = bundle.get("payload", {})
    return payload.get("state", make_empty_state())


def load_experiment_bundle_payload(source: Path):
    parsed_bundle = load_parameter_file(source, {"experiment"})
    bundle_dir = source.parent
    missing_files = []
    payload_data = dict(parsed_bundle["payload"])
    detector_bundle_path = bundle_dir / "detector_db.par"
    if not detector_bundle_path.exists() and (bundle_dir / "detectors.par").exists():
        detector_bundle_path = bundle_dir / "detectors.par"
    payload_data["detector_db"] = load_bundle_file_or_default(
        detector_bundle_path,
        {"detector_db"},
        "detector_db",
        load_detector_db_file,
        "detector_db.par",
        missing_files,
    )
    payload_data["group_db"] = load_bundle_file_or_default(
        bundle_dir / "group_db.par",
        {"group_db"},
        "group_db",
        lambda: load_parameter_file(group_db_path(), {"group_db"})["payload"].get("group_db", DEFAULT_GROUP_DB) if group_db_path().exists() else DEFAULT_GROUP_DB,
        "group_db.par",
        missing_files,
    )
    payload_data["rings_db"] = payload_data["group_db"]
    board_configs = load_bundle_file_or_default(
        bundle_dir / "board_types.par",
        {"board_types"},
        "board_types",
        load_board_configs_file,
        "board_types.par",
        missing_files,
    )
    if isinstance(board_configs, list):
        board_configs = {"board_types": board_configs}
    elif not (isinstance(board_configs, dict) and isinstance(board_configs.get("board_types"), list)):
        board_configs = load_board_configs_file()
    payload_data["board_configs"] = board_configs
    payload_data["missing_bundle_files"] = missing_files
    return payload_data


def list_directory(path_value: str):
    path_value = (path_value or "").strip()
    if not path_value:
        target = Path.cwd()
    else:
        target = Path(os.path.expanduser(os.path.expandvars(path_value)))
        if not target.is_absolute():
            target = (Path.cwd() / target).resolve()
        else:
            target = target.resolve()

    current_dir = target if target.is_dir() else target.parent
    entries = []
    for child in sorted(current_dir.iterdir(), key=lambda item: (not item.is_dir(), item.name.lower())):
        if child.name.startswith("."):
            continue
        if child.is_dir() or child.suffix.lower() in {".par", ".mac", ".conf"}:
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


def next_output_path(experiment_name: str, suffix: str, output_dir: Path):
    safe = "".join(ch if ch.isalnum() else "_" for ch in experiment_name).strip("_") or "mapping"
    return output_dir / f"{safe}_{suffix}"


def safe_experiment_name(name: str) -> str:
    return "".join(ch if ch.isalnum() else "_" for ch in str(name or "")).strip("_") or "mapping"


FPN_CHANNELS = (11, 22, 45, 56)


def signal_channel_to_aget_channel(signal_channel):
    """Convert logical signal-channel index to physical AGET channel, skipping FPN channels."""
    base_channel = int(signal_channel)
    channel = base_channel
    while True:
        shifted = base_channel + sum(1 for fpn_channel in FPN_CHANNELS if fpn_channel <= channel)
        if shifted == channel:
            return channel
        channel = shifted


class ExportError(RuntimeError):
    def __init__(self, message: str, node_ids=None, group_ids=None):
        super().__init__(message)
        self.node_ids = list(dict.fromkeys(node_ids or []))
        self.group_ids = list(dict.fromkeys(group_ids or []))


def save_experiment_bundle(experiment: dict, detector_db: dict, group_db: dict, output_dir: Path, board_configs=None):
    safe_name = safe_experiment_name(experiment.get("name", "mapping"))
    expected_dir_name = f"mapping_{safe_name}"
    export_dir = output_dir if output_dir.name == expected_dir_name else output_dir / expected_dir_name
    export_dir.mkdir(parents=True, exist_ok=True)
    experiment_file = export_dir / f"{safe_name}.conf"
    save_parameter_file(
        experiment_file,
        "experiment",
        {"experiment": experiment, "detector_db": detector_db, "group_db": group_db},
    )
    save_parameter_file(
        export_dir / "detector_db.par",
        "detector_db",
        {"detector_db": detector_db},
    )
    save_parameter_file(
        export_dir / "group_db.par",
        "group_db",
        {"group_db": group_db},
    )
    save_parameter_file(
        export_dir / "board_types.par",
        "board_types",
        {"board_types": (board_configs or load_board_configs_file()).get("board_types", DEFAULT_BOARD_CONFIGS.get("board_types", []))},
    )
    return {
        "export_dir": str(export_dir),
        "experiment_file": str(experiment_file),
        "detector_db_file": str(export_dir / "detector_db.par"),
        "group_db_file": str(export_dir / "group_db.par"),
        "board_types_file": str(export_dir / "board_types.par"),
    }


def get_group_lookup(group_db):
    lookup = {}
    for entry in group_db.get("groups", []):
        name = entry["name"]
        lookup[name] = entry
    for alias, canonical in GROUP_PRESET_ALIASES.items():
        if canonical in lookup:
            lookup[alias] = lookup[canonical]
    return lookup


def get_detector_lookup(detector_db):
    return {int(entry["number"]): entry for entry in detector_db.get("detectors", [])}


def find_group(experiment, group_id):
    for group in experiment.get("groups", []):
        if group.get("id") == group_id:
            return group
    return None


def find_node(group, node_id):
    for node in group.get("nodes", []):
        if node.get("id") == node_id:
            return node
    return None


def find_node_in_experiment(experiment, node_id):
    for group in experiment.get("groups", []):
        node = find_node(group, node_id)
        if node is not None:
            return node
    return None


def find_group_for_node(experiment, node_id):
    for group in experiment.get("groups", []):
        if find_node(group, node_id) is not None:
            return group
    return None


def current_experiment_nodes(experiment):
    nodes = []
    for group in experiment.get("groups", []):
        nodes.extend(group.get("nodes", []))
    return nodes


def port_owner(group, endpoint, experiment=None):
    node_id = endpoint.get("node_id")
    node = find_node(group, node_id)
    if node is not None:
        return node
    if experiment is not None:
        return find_node_in_experiment(experiment, node_id)
    return None


def build_connection_maps(group, experiment=None):
    outgoing = {}
    incoming = {}
    all_connections = list(group.get("connections", []))
    if experiment is not None:
        all_connections.extend(experiment.get("board_connections", []))
    for connection in all_connections:
        source = connection.get("from", {})
        target = connection.get("to", {})
        outgoing[(source.get("node_id"), source.get("port"))] = connection
        incoming[(target.get("node_id"), target.get("port"))] = connection
    return outgoing, incoming


def compute_phi(detector_data, ring_entry):
    phi_number = int(detector_data.get("phi_number", 0))
    if ring_entry is None:
        center = float(detector_data.get("phi_center_deg", 0))
        half_opening = float(detector_data.get("phi_half_opening_deg", 10))
        return center, center - half_opening, center + half_opening

    capacity = int(ring_entry.get("capacity", 0))
    if capacity > 0 and ring_entry.get("name") != "Free":
        pitch = 360.0 / capacity
        center = float(ring_entry.get("phi_offset_deg", 0)) + phi_number * pitch
    else:
        center = float(detector_data.get("phi_center_deg", 0))
    half_opening = float(detector_data.get("phi_half_opening_deg", ring_entry.get("half_opening_deg", 10)))
    return center, center - half_opening, center + half_opening


def ensure_experiment_board_connections(experiment):
    if "board_connections" in experiment and isinstance(experiment.get("board_connections"), list):
        return
    experiment["board_connections"] = []
    for group in experiment.get("groups", []):
        kept_connections = []
        for connection in group.get("connections", []):
            from_node = find_node(group, connection.get("from", {}).get("node_id"))
            to_node = find_node(group, connection.get("to", {}).get("node_id"))
            if from_node is not None and to_node is not None and from_node.get("kind") != "detector" and to_node.get("kind") != "detector":
                experiment["board_connections"].append(connection)
            else:
                kept_connections.append(connection)
        group["connections"] = kept_connections


def endpoint_chain(group, detector_node, experiment=None):
    outgoing, incoming = build_connection_maps(group, experiment)
    det_out = outgoing.get((detector_node["id"], "OUT"))
    if det_out is None:
        return None, "Detector is not connected to a merging board."

    merging_node = port_owner(group, det_out.get("to", {}), experiment)
    slot_name = det_out.get("to", {}).get("port")
    if merging_node is None or merging_node.get("kind") != "merging":
        return None, "Detector must connect to a merging board slot."

    merging_out = outgoing.get((merging_node["id"], "OUT"))
    if merging_out is None:
        return None, "Merging board is not connected to a ZAP pin."

    zap_node = port_owner(group, merging_out.get("to", {}), experiment)
    pin_name = merging_out.get("to", {}).get("port")
    if zap_node is None or zap_node.get("kind") != "zap":
        return None, "Merging board must connect to a ZAP board."

    zap_out = outgoing.get((zap_node["id"], "OUT"))
    if zap_out is None:
        return None, "ZAP board is not connected to a COBO board."

    direct_cobo = port_owner(group, zap_out.get("to", {}), experiment)
    asad_node = None
    if direct_cobo is not None and direct_cobo.get("kind") == "cobo":
        cobo_node = direct_cobo
    else:
        asad_node = direct_cobo
        if asad_node is None or asad_node.get("kind") != "asad":
            return None, "ZAP board must connect to a COBO board."
        asad_out = outgoing.get((asad_node["id"], "OUT"))
        if asad_out is None:
            return None, "ASAD board is not connected to a COBO board."
        cobo_node = port_owner(group, asad_out.get("to", {}), experiment)
        if cobo_node is None or cobo_node.get("kind") != "cobo":
            return None, "ASAD board must connect to a COBO board."

    return {
        "slot_name": slot_name,
        "pin_name": pin_name,
        "merging": merging_node,
        "zap": zap_node,
        "asad": asad_node,
        "cobo": cobo_node,
    }, None


def export_experiment(state, detector_db, group_db, experiment_id, output_dir: Path, board_configs=None):
    experiments = state.get("experiments", [])
    experiment = next((item for item in experiments if item.get("id") == experiment_id), None)
    if experiment is None:
        raise RuntimeError("Experiment not found.")
    ensure_experiment_board_connections(experiment)

    def node_and_group_ids_for(node):
        if node is None:
            return [], []
        group = find_group_for_node(experiment, node.get("id"))
        group_ids = [group.get("id")] if group is not None else []
        return [node.get("id")], group_ids

    def export_error_for(node, message):
        node_ids, group_ids = node_and_group_ids_for(node)
        raise ExportError(f"{node.get('label', 'Item')}: {message}", node_ids=node_ids, group_ids=group_ids)

    def export_chain_error_for(group, detector_node, message):
        outgoing, _incoming = build_connection_maps(group, experiment)
        marked_nodes = []
        det_out = outgoing.get((detector_node["id"], "OUT"))
        merging_node = port_owner(group, det_out.get("to", {}), experiment) if det_out is not None else None
        merging_out = outgoing.get((merging_node["id"], "OUT")) if merging_node is not None else None
        zap_node = port_owner(group, merging_out.get("to", {}), experiment) if merging_out is not None else None

        if "Detector is not connected to a merging board" in message or "Detector must connect to a merging board" in message:
            marked_nodes = [detector_node]
        elif "Merging board is not connected to a ZAP pin" in message or "Merging board must connect to a ZAP board" in message:
            marked_nodes = [item for item in [merging_node] if item is not None]
            marked_nodes.extend([node for node in current_experiment_nodes(experiment) if node.get("kind") == "zap" and node not in marked_nodes])
        elif "ZAP board is not connected to a COBO board" in message or "ZAP board must connect to a COBO board" in message or "ASAD board is not connected to a COBO board" in message or "ASAD board must connect to a COBO board" in message:
            marked_nodes = [item for item in [zap_node] if item is not None]
            marked_nodes.extend([node for node in current_experiment_nodes(experiment) if node.get("kind") == "cobo" and node not in marked_nodes])
        else:
            marked_nodes = [detector_node]

        node_ids = []
        group_ids = []
        for node in marked_nodes:
            ids, gids = node_and_group_ids_for(node)
            node_ids.extend(ids)
            group_ids.extend(gids)
        if not node_ids:
            node_ids, group_ids = node_and_group_ids_for(detector_node)
        raise ExportError(f"{detector_node.get('label', 'Detector')}: {message}", node_ids=node_ids, group_ids=group_ids)

    detector_lookup = get_detector_lookup(detector_db)
    ring_lookup = get_group_lookup(group_db)
    detector_rows = [
        "detector mapping",
        "det_type\tdet_idx\tdet_number\tdet_thickness\tdet_width\tdet_height\tring_number\tring_type\tring_radius\tring_z\tdEE\tphi_number\tphi\tphi1\tphi2",
    ]
    channel_rows = [
        "channel mapping",
        "ch_idx\tcobo\tasad\taget\tch\tdet_idx\tphi_number\tside\tstrip",
    ]
    board_rows = [
        "det_group\tdet_idx\tdet_type\tdet_number\tring_number\tring_type\tring_radius\tring_z\tdet_r\tdet_z\tphi_number\tmg\tslot\tzap\tpin\tasad\tcobo",
    ]

    channel_index = 0
    detector_index = 0
    phi_occupancy = {}
    used_channels = set()
    channel_owner = {}

    board_configs = board_configs or load_board_configs_file()
    export_specs = {}
    for config in board_configs.get("board_types", []):
        export_specs[config.get("key")] = {
            "slot_order": {slot.get("id"): index for index, slot in enumerate(config.get("merging_slots", []))},
            "pin_junction_aget": {pin.get("id"): int(pin.get("junction_aget", 0)) for pin in config.get("zap_pins", [])},
            "pin_junction_base": {pin.get("id"): int(pin.get("junction_first_channel", 0)) for pin in config.get("zap_pins", [])},
            "pin_ohmic_aget": {pin.get("id"): int(pin.get("ohmic_aget", 0)) for pin in config.get("zap_pins", [])},
            "pin_ohmic_base": {pin.get("id"): int(pin.get("ohmic_first_channel", 0)) for pin in config.get("zap_pins", [])},
            "junction_strip_count": int(config.get("junction_strip_count", 0)),
            "ohmic_strip_count": int(config.get("ohmic_strip_count", 0)),
        }

    for group_index, group in enumerate(experiment.get("groups", [])):
        nodes = group.get("nodes", [])
        detectors = [node for node in nodes if node.get("kind") == "detector"]
        for detector_node in detectors:
            data = detector_node.get("data", {})
            det_type = str(data.get("detector_type", "X6")).strip()
            spec = export_specs.get(det_type)
            if spec is None:
                raise ExportError(f"{det_type} export is not implemented yet. Supported types are X6, QQQ5, and BB10.")

            slot_order = spec["slot_order"]
            pin_junction_aget = spec["pin_junction_aget"]
            pin_junction_base = spec["pin_junction_base"]
            pin_ohmic_aget = spec["pin_ohmic_aget"]
            pin_ohmic_base = spec["pin_ohmic_base"]
            junction_strip_count = spec["junction_strip_count"]
            ohmic_strip_count = spec["ohmic_strip_count"]

            chain, error = endpoint_chain(group, detector_node, experiment)
            if error is not None:
                export_chain_error_for(group, detector_node, error)

            slot_name = chain["slot_name"]
            pin_name = chain["pin_name"]
            if slot_name not in slot_order:
                export_error_for(detector_node, f"invalid merging slot {slot_name}.")
            if pin_name not in pin_junction_aget:
                export_error_for(detector_node, f"invalid ZAP pin {pin_name}.")

            det_number = int(data.get("det_number", 0))
            if det_number < 0:
                export_error_for(detector_node, "detector number must be zero or positive.")
            if det_number == 0:
                det_db_entry = {
                    "thickness_um": 0,
                    "active_width_mm": 0,
                    "active_height_mm": 0,
                }
            else:
                det_db_entry = detector_lookup.get(det_number)
                if det_db_entry is None:
                    export_error_for(detector_node, f"detector number {det_number} is missing from the detector database.")

            ring_type = GROUP_PRESET_ALIASES.get(data.get("ring_type", "Free"), data.get("ring_type", "Free"))
            ring_type_export = ring_type.replace(" ", "_")
            ring_number = int(data.get("ring_number", 0))
            phi_number = int(data.get("phi_number", 0))
            ring_entry = ring_lookup.get(ring_type)
            ring_key = (group_index, ring_number, ring_type)
            phi_occupancy.setdefault(ring_key, set())
            if phi_number in phi_occupancy[ring_key]:
                export_error_for(detector_node, f"duplicated phi_number {phi_number} in ring {ring_type}/{ring_number}.")
            phi_occupancy[ring_key].add(phi_number)

            phi, phi1, phi2 = compute_phi(data, ring_entry)
            ring_radius = float(data.get("ring_radius_mm", ring_entry.get("radius_mm", 0) if ring_entry else 0))
            ring_z = float(data.get("ring_z_mm", ring_entry.get("z_mm", 0) if ring_entry else 0))

            detector_rows.append(
                "\t".join(
                    [
                        det_type,
                        str(detector_index),
                        str(det_number),
                        str(det_db_entry.get("thickness_um", 0)),
                        str(det_db_entry.get("active_width_mm", 0)),
                        str(det_db_entry.get("active_height_mm", 0)),
                        str(ring_number),
                        ring_type_export,
                        str(ring_radius),
                        str(ring_z),
                        data.get("dee", "E"),
                        str(phi_number),
                        f"{phi:.8f}",
                        f"{phi1:.8f}",
                        f"{phi2:.8f}",
                    ]
                )
            )

            board_rows.append(
                "\t".join(
                    [
                        str(group_index),
                        str(detector_index),
                        det_type,
                        str(det_number),
                        str(ring_number),
                        ring_type_export,
                        str(ring_radius),
                        str(ring_z),
                        str(data.get("det_r_mm", 0)),
                        str(data.get("det_z_mm", 0)),
                        str(phi_number),
                        str(chain["merging"].get("data", {}).get("board_number", 0)),
                        slot_name,
                        str(chain["zap"].get("data", {}).get("board_number", 0)),
                        pin_name,
                        str(
                            chain["asad"].get("data", {}).get("board_number", 0)
                            if chain["asad"] is not None
                            else chain["zap"].get("data", {}).get("asad_number", chain["zap"].get("data", {}).get("board_number", 0))
                        ),
                        str(chain["cobo"].get("data", {}).get("board_number", 0)),
                    ]
                )
            )

            slot_index = slot_order[slot_name]
            junction_aget = pin_junction_aget[pin_name]
            junction_base = pin_junction_base[pin_name]
            ohmic_base = pin_ohmic_base[pin_name]
            cobo_number = int(chain["cobo"].get("data", {}).get("board_number", 0))
            asad_number = int(
                chain["asad"].get("data", {}).get("board_number", 0)
                if chain["asad"] is not None
                else chain["zap"].get("data", {}).get("asad_number", chain["zap"].get("data", {}).get("board_number", 0))
            )

            for strip_index in range(junction_strip_count):
                channel = signal_channel_to_aget_channel(junction_base + slot_index * junction_strip_count + strip_index)
                key = (cobo_number, asad_number, junction_aget, channel)
                if key in used_channels:
                    other = channel_owner.get(key)
                    other_group = find_group_for_node(experiment, other.get("id")) if other is not None else None
                    this_group = find_group_for_node(experiment, detector_node.get("id"))
                    other_text = other.get("label", "Item") if other is not None else "Item"
                    if other_group is not None:
                        other_text = f"{other_text} in {other_group.get('name')}"
                    raise ExportError(
                        f"Duplicated junction channel CAAC={key}. Conflicts with {other_text}.",
                        node_ids=[item for item in [detector_node.get("id"), other.get("id") if other is not None else None] if item is not None],
                        group_ids=[item for item in [this_group.get("id") if this_group is not None else None, other_group.get("id") if other_group is not None else None] if item is not None],
                    )
                used_channels.add(key)
                channel_owner[key] = detector_node
                channel_rows.append(
                    "\t".join(
                        [
                            str(channel_index),
                            str(cobo_number),
                            str(asad_number),
                            str(junction_aget),
                            str(channel),
                            str(detector_index),
                            str(phi_number),
                            "1",
                            str(strip_index + 1),
                        ]
                    )
                )
                channel_index += 1

            for strip_index in range(ohmic_strip_count):
                channel = signal_channel_to_aget_channel(ohmic_base + slot_index * ohmic_strip_count + strip_index)
                ohmic_aget = pin_ohmic_aget[pin_name]
                key = (cobo_number, asad_number, ohmic_aget, channel)
                if key in used_channels:
                    other = channel_owner.get(key)
                    other_group = find_group_for_node(experiment, other.get("id")) if other is not None else None
                    this_group = find_group_for_node(experiment, detector_node.get("id"))
                    other_text = other.get("label", "Item") if other is not None else "Item"
                    if other_group is not None:
                        other_text = f"{other_text} in {other_group.get('name')}"
                    raise ExportError(
                        f"Duplicated ohmic channel CAAC={key}. Conflicts with {other_text}.",
                        node_ids=[item for item in [detector_node.get("id"), other.get("id") if other is not None else None] if item is not None],
                        group_ids=[item for item in [this_group.get("id") if this_group is not None else None, other_group.get("id") if other_group is not None else None] if item is not None],
                    )
                used_channels.add(key)
                channel_owner[key] = detector_node
                channel_rows.append(
                    "\t".join(
                        [
                            str(channel_index),
                            str(cobo_number),
                            str(asad_number),
                            str(ohmic_aget),
                            str(channel),
                            str(detector_index),
                            str(phi_number),
                            "0",
                            str(strip_index + 1),
                        ]
                    )
                )
                channel_index += 1

            detector_index += 1

    if detector_index == 0:
        raise ExportError("There is no detector to export.")

    bundle_result = save_experiment_bundle(experiment, detector_db, group_db, output_dir)
    export_dir = Path(bundle_result["export_dir"])
    detector_file = next_output_path(experiment.get("name", "mapping"), "detector_mapping.txt", export_dir)
    channel_file = next_output_path(experiment.get("name", "mapping"), "channel_mapping.txt", export_dir)
    board_file = next_output_path(experiment.get("name", "mapping"), "board_mapping.txt", export_dir)

    detector_file.write_text("\n".join(detector_rows) + "\n", encoding="utf-8")
    channel_file.write_text("\n".join(channel_rows) + "\n", encoding="utf-8")
    board_file.write_text("\n".join(board_rows) + "\n", encoding="utf-8")

    return {
        **bundle_result,
        "detector_file": str(detector_file),
        "channel_file": str(channel_file),
        "board_file": str(board_file),
        "detector_count": detector_index,
        "channel_count": channel_index,
    }


def save_parameter_file(path: Path, bundle_type: str, payload: dict):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(serialize_parameter_bundle(bundle_type, payload), encoding="utf-8")


def load_parameter_file(path: Path, expected_types=None):
    parsed = parse_parameter_bundle(path.read_text(encoding="utf-8"))
    if expected_types is not None and parsed["type"] not in expected_types:
        raise RuntimeError(f"Expected {expected_types}, found {parsed['type']}")
    return parsed


def load_detector_db_file() -> dict:
    path = detector_db_path()
    if path.exists():
        parsed = load_parameter_file(path, {"detector_db"})
        return parsed["payload"].get("detector_db", DEFAULT_DETECTOR_DB)

    legacy_par_path = data_dir() / "detectors.par"
    if legacy_par_path.exists():
        parsed = load_parameter_file(legacy_par_path, {"detector_db"})
        return parsed["payload"].get("detector_db", DEFAULT_DETECTOR_DB)

    legacy_path = legacy_detector_db_json_path()
    if legacy_path.exists():
        return json.loads(legacy_path.read_text(encoding="utf-8"))

    save_parameter_file(path, "detector_db", {"detector_db": DEFAULT_DETECTOR_DB})
    return DEFAULT_DETECTOR_DB


def save_detector_db_file(payload: dict):
    save_parameter_file(detector_db_path(), "detector_db", {"detector_db": payload})


def load_rings_db_file() -> dict:
    path = rings_db_path()
    if path.exists():
        parsed = load_parameter_file(path, {"rings", "group_db"})
        return parsed["payload"].get("rings", parsed["payload"].get("group_db", DEFAULT_GROUP_DB))

    legacy_path = legacy_rings_db_json_path()
    if legacy_path.exists():
        return json.loads(legacy_path.read_text(encoding="utf-8"))

    save_parameter_file(path, "rings", {"rings": DEFAULT_GROUP_DB})
    return DEFAULT_GROUP_DB


def load_board_configs_file() -> dict:
    path = board_config_path()
    if path.exists():
        parsed = load_parameter_file(path, {"board_types"})
        payload = parsed["payload"].get("board_types", DEFAULT_BOARD_CONFIGS)
        if isinstance(payload, list):
            return {"board_types": payload}
        if isinstance(payload, dict) and isinstance(payload.get("board_types"), list):
            return payload
        return DEFAULT_BOARD_CONFIGS
    save_parameter_file(path, "board_types", {"board_types": DEFAULT_BOARD_CONFIGS.get("board_types", [])})
    return DEFAULT_BOARD_CONFIGS


def save_board_configs_file(payload: dict):
    board_types = payload.get("board_types", []) if isinstance(payload, dict) else payload
    save_parameter_file(board_config_path(), "board_types", {"board_types": board_types})


def run_lk_silicon_mapping_check(mapping_dir: Path, action: str, query_cobo=0, query_asad=0, query_aget=3, query_chan=0, num_print=20) -> str:
    mapping_dir = Path(os.path.expanduser(os.path.expandvars(str(mapping_dir)))).resolve()
    if not mapping_dir.exists() or not mapping_dir.is_dir():
        raise RuntimeError(f"Mapping directory not found: {mapping_dir}")

    lilak_root = lilak_root_path()
    lilak_lib = (lilak_root / "build" / "libLILAK.dylib").resolve()
    if not lilak_lib.exists():
        raise RuntimeError(f"LILAK library not found: {lilak_lib}")

    action = str(action or "").strip().lower()
    if action not in {"detectors", "channels", "lookup"}:
        raise RuntimeError(f"Unknown check action: {action}")

    action_line = {
        "detectors": f"mapping->PrintDetectors({int(num_print)});",
        "channels": f"mapping->PrintChannels({int(num_print)});",
        "lookup": f"mapping->PrintChannelLookup({int(query_cobo)}, {int(query_asad)}, {int(query_aget)}, {int(query_chan)});",
    }[action]

    macro_text = "\n".join([
        "{",
        f'  gSystem->Load({json.dumps(str(lilak_lib))});',
        "  auto mapping = new LKSiliconMapping();",
        f'  if (!mapping->Load({json.dumps(str(mapping_dir))})) {{',
        '    std::cout << "Load failed" << std::endl;',
        "    return;",
        "  }",
        f"  {action_line}",
        "}",
        "",
    ])

    temp_path = None
    try:
        with tempfile.NamedTemporaryFile("w", suffix=".C", delete=False, encoding="utf-8") as handle:
            handle.write(macro_text)
            temp_path = Path(handle.name)
        result = subprocess.run(
            ["root", "-l", "-b", "-q", str(temp_path)],
            capture_output=True,
            text=True,
            cwd=str(lilak_root),
        )
        output = (result.stdout or "").strip()
        stderr = (result.stderr or "").strip()
        if result.returncode != 0:
            raise RuntimeError(stderr or output or f"ROOT check failed with exit code {result.returncode}")
        return output if output else (stderr or "No output.")
    finally:
        if temp_path is not None:
            try:
                temp_path.unlink(missing_ok=True)
            except Exception:
                pass


def load_bundle_file_or_default(path: Path, expected_types, payload_key: str, default_loader, missing_name: str, missing_list: list[str]):
    if path.exists():
        parsed = load_parameter_file(path, expected_types)
        return parsed["payload"].get(payload_key, default_loader())
    missing_list.append(missing_name)
    return default_loader()


def pick_file_dialog(mode: str, suggested_name: str = "", initial_dir: Path | None = None) -> Path | None:
    initial_directory = (initial_dir or Path.cwd()).resolve()
    suggested_path = (initial_directory / suggested_name).resolve() if suggested_name else initial_directory

    if sys.platform == "darwin":
        if mode == "open":
            script = [
                'set defaultFolder to ((POSIX file (item 1 of argv)) as alias)',
                'set chosenFile to choose file with prompt "Open LILAK parameter file" default location defaultFolder',
                'POSIX path of chosenFile',
            ]
            result = subprocess.run(
                ["osascript", *sum([["-e", line] for line in script], []), str(initial_directory)],
                capture_output=True,
                text=True,
            )
        else:
            script = [
                'set defaultFolder to ((POSIX file (item 1 of argv)) as alias)',
                'set defaultName to item 2 of argv',
                'set chosenFile to choose file name with prompt "Save LILAK parameter file" default name defaultName default location defaultFolder',
                'POSIX path of chosenFile',
            ]
            result = subprocess.run(
                ["osascript", *sum([["-e", line] for line in script], []), str(initial_directory), suggested_name or "untitled.conf"],
                capture_output=True,
                text=True,
            )
        selected = result.stdout.strip()
        if result.returncode == 0 and selected:
            return Path(selected).resolve()

    if sys.platform.startswith("linux") and (os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")):
        zenity = shutil.which("zenity")
        if zenity is not None:
            cmd = [zenity, "--file-selection", "--filename", str(suggested_path if mode == "save" else initial_directory) + ("" if mode == "save" else os.sep)]
            if mode == "save":
                cmd.append("--save")
                cmd.append("--confirm-overwrite")
            result = subprocess.run(cmd, capture_output=True, text=True)
            selected = result.stdout.strip()
            if result.returncode == 0 and selected:
                return Path(selected).resolve()

        kdialog = shutil.which("kdialog")
        if kdialog is not None:
            if mode == "open":
                cmd = [kdialog, "--getopenfilename", str(initial_directory), "*.conf *.par *.mac|LILAK parameter file"]
            else:
                cmd = [kdialog, "--getsavefilename", str(suggested_path), "*.conf *.par *.mac|LILAK parameter file"]
            result = subprocess.run(cmd, capture_output=True, text=True)
            selected = result.stdout.strip()
            if result.returncode == 0 and selected:
                return Path(selected).resolve()

    try:
        import tkinter as tk
        from tkinter import filedialog
    except Exception as error:
        raise RuntimeError(f"File dialog is not available: {error}") from error

    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    initial_directory_text = str(initial_directory)
    filetypes = [
        ("LILAK parameter file", "*.conf *.par *.mac"),
        ("All files", "*.*"),
    ]

    try:
        if mode == "open":
            selected = filedialog.askopenfilename(
                initialdir=initial_directory_text,
                filetypes=filetypes,
            )
        else:
            selected = filedialog.asksaveasfilename(
                initialdir=initial_directory_text,
                initialfile=suggested_name or "",
                defaultextension=".conf",
                filetypes=filetypes,
            )
    finally:
        root.destroy()

    if not selected:
        return None
    return Path(selected).resolve()


class SiMappingServer(ThreadingHTTPServer):
    def __init__(self, address, initial_path: Path | None):
        super().__init__(address, SiMappingHandler)
        self.initial_path = initial_path


class SiMappingHandler(BaseHTTPRequestHandler):
    server: SiMappingServer

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

    def _send_html(self, html):
        body = html.encode("utf-8")
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
            mapping_dir = mapping_store_dir()
            initial_path = self.server.initial_path
            state = make_empty_state()
            detector_db = load_detector_db_file()
            group_db = load_parameter_file(group_db_path(), {"group_db"})["payload"].get("group_db", DEFAULT_GROUP_DB) if group_db_path().exists() else DEFAULT_GROUP_DB
            rings_db = load_rings_db_file()
            board_configs = load_board_configs_file()
            missing_bundle_files = []
            if initial_path is not None and initial_path.is_file():
                try:
                    parsed_bundle = load_parameter_file(initial_path)
                    bundle_type = parsed_bundle.get("type")
                    if bundle_type == "experiment":
                        payload = load_experiment_bundle_payload(initial_path)
                        experiment = payload.get("experiment")
                        if experiment is not None:
                            state = {"experiments": [experiment]}
                        detector_db = payload.get("detector_db", detector_db)
                        group_db = payload.get("group_db", group_db)
                        rings_db = payload.get("rings_db", rings_db)
                        board_configs = payload.get("board_configs", board_configs)
                        missing_bundle_files = payload.get("missing_bundle_files", [])
                    elif bundle_type == "layout":
                        state = parsed_bundle["payload"].get("state", make_empty_state())
                        detector_db = parsed_bundle["payload"].get("detector_db", detector_db)
                        group_db = parsed_bundle["payload"].get("group_db", group_db)
                except Exception:
                    state = load_state(initial_path)
            self._send_json(
                {
                    "state": state,
                    "state_path": str(initial_path) if initial_path else "",
                    "detector_db": detector_db,
                    "group_db": group_db,
                    "rings_db": rings_db,
                    "board_configs": board_configs,
                    "missing_bundle_files": missing_bundle_files,
                    "cwd": str(Path.cwd()),
                    "lilak_mapping_dir": str(mapping_dir),
                }
            )
            return
        self._send_json({"error": "Not found"}, status=HTTPStatus.NOT_FOUND)

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        payload = self._read_json()

        try:
            if parsed.path == "/api/listdir":
                self._send_json(list_directory(payload.get("path", "")))
                return

            if parsed.path == "/api/save_state":
                state = payload.get("state", {})
                target = resolve_state_path(payload.get("path")) or (Path.cwd() / "si_mapping_layout.par")
                state["updated_at"] = time.strftime("%Y-%m-%d %H:%M:%S")
                save_parameter_file(target, "layout", {"state": state, "detector_db": payload.get("detector_db", DEFAULT_DETECTOR_DB), "group_db": payload.get("group_db", DEFAULT_GROUP_DB)})
                self._send_json({"path": str(target), "saved_at": state["updated_at"]})
                return

            if parsed.path == "/api/load_state":
                source = resolve_state_path(payload.get("path"))
                if source is None or not source.is_file():
                    raise RuntimeError("Layout file not found.")
                parsed_bundle = load_parameter_file(source, {"layout"})
                self._send_json(
                    {
                        "path": str(source),
                        "state": parsed_bundle["payload"].get("state", make_empty_state()),
                        "detector_db": parsed_bundle["payload"].get("detector_db", load_detector_db_file()),
                        "group_db": parsed_bundle["payload"].get("group_db", DEFAULT_GROUP_DB),
                    }
                )
                return

            if parsed.path == "/api/save_experiment":
                target = resolve_state_path(payload.get("path")) or (Path.cwd() / "si_mapping_experiment.par")
                save_parameter_file(target, "experiment", payload)
                self._send_json({"path": str(target), "saved_at": time.strftime("%Y-%m-%d %H:%M:%S")})
                return

            if parsed.path == "/api/save_experiment_bundle":
                output_dir = Path(os.path.expanduser(os.path.expandvars(payload.get("output_dir") or str(Path.cwd()))))
                if not output_dir.is_absolute():
                    output_dir = (Path.cwd() / output_dir).resolve()
                result = save_experiment_bundle(
                    payload.get("experiment", {}),
                    payload.get("detector_db", DEFAULT_DETECTOR_DB),
                    payload.get("group_db", DEFAULT_GROUP_DB),
                    output_dir,
                    payload.get("board_configs", DEFAULT_BOARD_CONFIGS),
                )
                self._send_json(result)
                return

            if parsed.path == "/api/load_experiment":
                source = resolve_state_path(payload.get("path"))
                if source is None or not source.is_file():
                    raise RuntimeError("Experiment file not found.")
                self._send_json(load_experiment_bundle_payload(source))
                return

            if parsed.path == "/api/save_group":
                target = resolve_state_path(payload.get("path")) or (Path.cwd() / "si_mapping_group.par")
                save_parameter_file(target, "group", payload)
                self._send_json({"path": str(target), "saved_at": time.strftime("%Y-%m-%d %H:%M:%S")})
                return

            if parsed.path == "/api/load_group":
                source = resolve_state_path(payload.get("path"))
                if source is None or not source.is_file():
                    raise RuntimeError("Group file not found.")
                parsed_bundle = load_parameter_file(source, {"group"})
                self._send_json(parsed_bundle["payload"])
                return

            if parsed.path == "/api/save_detector_db":
                save_detector_db_file(payload.get("detector_db", DEFAULT_DETECTOR_DB))
                self._send_json({"saved_at": time.strftime("%Y-%m-%d %H:%M:%S")})
                return

            if parsed.path == "/api/save_group_db":
                target = resolve_state_path(payload.get("path")) or group_db_path()
                save_parameter_file(target, "group_db", {"group_db": payload.get("group_db", DEFAULT_GROUP_DB)})
                self._send_json({"path": str(target), "saved_at": time.strftime("%Y-%m-%d %H:%M:%S")})
                return

            if parsed.path == "/api/load_group_db":
                source = resolve_state_path(payload.get("path")) or group_db_path()
                if source is None or not source.is_file():
                    raise RuntimeError("Group DB file not found.")
                parsed_bundle = load_parameter_file(source, {"group_db"})
                self._send_json({"path": str(source), "group_db": parsed_bundle["payload"].get("group_db", DEFAULT_GROUP_DB)})
                return

            if parsed.path == "/api/serialize_bundle":
                bundle_type = payload.get("bundle_type", "")
                bundle_payload = payload.get("payload", {})
                if not bundle_type:
                    raise RuntimeError("bundle_type is required.")
                content = serialize_parameter_bundle(bundle_type, bundle_payload)
                self._send_json({"content": content})
                return

            if parsed.path == "/api/parse_bundle":
                content = payload.get("content", "")
                expected_types = set(payload.get("expected_types", []))
                parsed_bundle = parse_parameter_bundle(content)
                if expected_types and parsed_bundle["type"] not in expected_types:
                    raise RuntimeError(f"Expected one of {sorted(expected_types)}, found {parsed_bundle['type']}.")
                self._send_json(parsed_bundle)
                return

            if parsed.path == "/api/pick_open_path":
                selected = pick_file_dialog("open", initial_dir=Path.cwd())
                self._send_json({"path": str(selected) if selected is not None else ""})
                return

            if parsed.path == "/api/pick_save_path":
                selected = pick_file_dialog(
                    "save",
                    suggested_name=payload.get("suggested_name", ""),
                    initial_dir=Path.cwd(),
                )
                self._send_json({"path": str(selected) if selected is not None else ""})
                return

            if parsed.path == "/api/export":
                output_dir = Path(os.path.expanduser(os.path.expandvars(payload.get("output_dir") or str(Path.cwd()))))
                if not output_dir.is_absolute():
                    output_dir = (Path.cwd() / output_dir).resolve()
                result = export_experiment(
                    payload.get("state", {}),
                    payload.get("detector_db", DEFAULT_DETECTOR_DB),
                    payload.get("group_db", DEFAULT_GROUP_DB),
                    payload.get("experiment_id", ""),
                    output_dir,
                    payload.get("board_configs", DEFAULT_BOARD_CONFIGS),
                )
                self._send_json(result)
                return

            if parsed.path == "/api/save_board_configs":
                target = resolve_state_path(payload.get("path")) or board_config_path()
                save_parameter_file(target, "board_types", {"board_types": payload.get("board_configs", DEFAULT_BOARD_CONFIGS).get("board_types", DEFAULT_BOARD_CONFIGS.get("board_types", []))})
                self._send_json({"path": str(target), "saved_at": time.strftime("%Y-%m-%d %H:%M:%S")})
                return

            if parsed.path == "/api/load_board_configs":
                source = resolve_state_path(payload.get("path")) or board_config_path()
                if source is None or not source.is_file():
                    raise RuntimeError("Board types file not found.")
                parsed_bundle = load_parameter_file(source, {"board_types"})
                board_types = parsed_bundle["payload"].get("board_types", DEFAULT_BOARD_CONFIGS)
                if isinstance(board_types, list):
                    board_configs = {"board_types": board_types}
                elif isinstance(board_types, dict) and isinstance(board_types.get("board_types"), list):
                    board_configs = board_types
                else:
                    board_configs = DEFAULT_BOARD_CONFIGS
                self._send_json({"path": str(source), "board_configs": board_configs})
                return

            if parsed.path == "/api/check_mapping":
                mapping_dir = Path(payload.get("mapping_dir") or "")
                output = run_lk_silicon_mapping_check(
                    mapping_dir,
                    payload.get("action", ""),
                    payload.get("query_cobo", 0),
                    payload.get("query_asad", 0),
                    payload.get("query_aget", 3),
                    payload.get("query_chan", 0),
                    payload.get("num_print", 20),
                )
                self._send_json({"output": output})
                return
        except ExportError as error:
            self._send_json({"error": str(error), "node_ids": error.node_ids, "group_ids": error.group_ids}, status=HTTPStatus.BAD_REQUEST)
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


HTML_PAGE = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LILAK Silicon Mapping Editor</title>
  <style>
    :root {
      --bg: #dfeaf7;
      --panel: #f7fbff;
      --line: #b6cbe0;
      --text: #2b2f36;
      --muted: #666f7b;
      --accent: #2e6f95;
      --accent-2: #5fa8d3;
      --ok: #3b82a6;
      --danger: #b24a5a;
      --shadow: 0 14px 30px rgba(20, 54, 84, 0.12);
      --radius: 16px;
      --mono: "SFMono-Regular", Menlo, Consolas, monospace;
      --sans: "Avenir Next", "Helvetica Neue", Helvetica, sans-serif;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: var(--sans);
      color: var(--text);
      background: var(--bg);
      min-height: 100vh;
    }
    button, select, input, textarea {
      font: inherit;
    }
    .app {
      display: grid;
      grid-template-rows: auto auto 1fr;
      min-height: 100vh;
    }
    .topbar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      padding: 18px 22px 10px;
    }
    .headline h1 {
      margin: 0;
      font-size: 26px;
      letter-spacing: 0.02em;
    }
    .headline p {
      margin: 4px 0 0;
      color: var(--muted);
      font-size: 14px;
    }
    .toolbar, .subtoolbar {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
    }
    .subtoolbar.block {
      display: grid;
      gap: 8px;
      align-items: start;
    }
    .subtoolbar-title {
      font-size: 12px;
      color: #2b2f36;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      margin-right: 4px;
      font-weight: 700;
    }
    .toolbar button, .subtoolbar button {
      border: 0;
      border-radius: 999px;
      padding: 3px 8px;
      box-shadow: var(--shadow);
      background: var(--panel);
      color: var(--text);
      cursor: pointer;
      transition: transform 0.15s ease, background 0.2s ease;
      font-size: 11px;
    }
    .toolbar button.primary, .subtoolbar button.primary {
      background: var(--accent);
      color: white;
    }
    .toolbar button:hover, .subtoolbar button:hover {
      transform: translateY(-1px);
    }
    .tabs-wrap {
      padding: 0 22px 16px;
      display: grid;
      gap: 10px;
    }
    .tabs {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
    }
    .tabs > button:not(.tab), #detectorBoardButtons > button {
      border: 1px solid var(--line);
      background: rgba(247,251,255,0.98);
      border-radius: 8px;
      padding: 3px 10px;
      min-height: 26px;
      color: var(--text);
      cursor: pointer;
      box-shadow: var(--shadow);
      font-size: 11px;
      transition: transform 0.15s ease, background 0.2s ease, border-color 0.2s ease;
    }
    .toolbar button.active-toggle {
      border-width: 3px;
      border-color: #8b0000;
      box-shadow: inset 0 0 0 1px rgba(139,0,0,0.22), 0 0 0 1px rgba(139,0,0,0.28), var(--shadow);
    }
    .tabs > button:not(.tab):hover, #detectorBoardButtons > button:hover {
      transform: translateY(-1px);
    }
    .tabs > button:not(.tab).tone-add,
    .tabs > button:not(.tab).tone-save,
    .tabs > button:not(.tab).tone-load,
    .tabs > button:not(.tab).tone-export {
      background: rgba(247,251,255,0.98);
      border-color: #294a62;
      color: #294a62;
      font-weight: 400;
    }
    .tabs > button:not(.tab).tone-delete {
      background: rgba(247,251,255,0.98);
      border-color: #294a62;
      color: #294a62;
      font-weight: 400;
    }
    .tab {
      border: 1px solid var(--line);
      background: #edf5fc;
      border-radius: 999px;
      padding: 4px 10px;
      cursor: pointer;
      color: #495662;
      font-size: 12px;
    }
    .tab.active {
      background: #edf5fc;
      color: #2b2f36;
      border-color: #2b2f36;
      border-width: 2px;
      font-weight: 600;
    }
    .tab.add-tab {
      background: #d8eef8;
      border-color: #2f6f95;
      color: #174b68;
      font-weight: 700;
      min-width: 30px;
    }
    .tabs > button.tab.add-tab {
      border: 1px solid #2f6f95;
      background: #d8eef8;
      color: #174b68;
      border-radius: 999px;
      padding: 4px 10px;
      min-width: 30px;
      box-shadow: none;
    }
    #placeholderTabs .tab {
      border: 1px solid var(--line);
      background: #edf5fc;
      border-radius: 999px;
      padding: 4px 10px;
      color: #495662;
      font-size: 12px;
      box-shadow: none;
    }
    #placeholderTabs .tab.active {
      background: #edf5fc;
      color: #2b2f36;
      border-color: #2b2f36;
      border-width: 2px;
      font-weight: 600;
    }
    .tab.error {
      border-color: #b10018;
      border-width: 3px;
      box-shadow: inset 0 0 0 1px rgba(177,0,24,0.18);
    }
    .tab-wrap {
      display: inline-flex;
      align-items: center;
      gap: 4px;
    }
    .tab-close {
      width: 22px;
      height: 22px;
      border: 1px solid rgba(33,66,90,0.24);
      border-radius: 999px;
      background: rgba(247,251,255,0.98);
      color: var(--muted);
      cursor: pointer;
      font-size: 14px;
      line-height: 1;
      padding: 0;
      display: inline-flex;
      align-items: center;
      justify-content: center;
    }
    .tab-close:hover {
      color: #7f2030;
      border-color: #7f2030;
    }
    #experimentTabs, #groupTabs, #placeholderTabs, #detectorBoardButtons {
      display: inline-flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
    }
    .toolbar-separator {
      width: 1px;
      height: 18px;
      background: rgba(33,66,90,0.18);
      margin: 0 2px;
      display: inline-block;
    }
    .tab-separator-mark {
      color: #294a62;
      font-weight: 700;
      opacity: 0.75;
      margin: 0 2px 0 -2px;
    }
    .workspace {
      display: grid;
      grid-template-columns: 300px 1fr;
      gap: 18px;
      padding: 0 22px 22px;
      min-height: 0;
      height: 1520px;
      align-items: stretch;
    }
    .panel {
      background: rgba(247,251,255,0.94);
      border: 1px solid var(--line);
      border-radius: var(--radius);
      box-shadow: var(--shadow);
      overflow: hidden;
      display: flex;
      flex-direction: column;
      min-height: 0;
    }
    .panel h2 {
      margin: 0;
      font-size: 15px;
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }
    .panel-header {
      padding: 14px 16px;
      border-bottom: 1px solid rgba(182,203,224,0.85);
      background: rgba(231,242,252,0.9);
    }
    .panel-body {
      padding: 14px 16px;
      overflow: auto;
      flex: 1 1 auto;
      min-height: 0;
    }
    .summary-panel .panel-body {
      overflow: hidden;
    }
    .canvas-panel .panel-body {
      overflow: hidden;
    }
    .inspector-panel {
      grid-column: 1 / -1;
      max-height: 360px;
      min-height: 260px;
    }
    .inspector-panel .panel-body {
      overflow: auto;
    }
    .stack {
      display: grid;
      gap: 12px;
    }
    .summary-stack {
      display: flex;
      flex-direction: column;
      gap: 12px;
      height: 100%;
      min-height: 0;
    }
    .group-summary-scroll {
      flex: 1 1 auto;
      min-height: 0;
      overflow: auto;
      padding-right: 2px;
    }
    .help {
      color: var(--muted);
      font-size: 13px;
      line-height: 1.5;
    }
    .group-card, .summary-card {
      border: 1px solid rgba(182,203,224,0.85);
      border-radius: 14px;
      padding: 12px;
      background: rgba(255,255,255,0.78);
    }
    .summary-list {
      display: grid;
      gap: 6px;
      margin-top: 10px;
    }
    .summary-item {
      border: 1px solid rgba(182,203,224,0.85);
      border-radius: 10px;
      padding: 8px 10px;
      background: rgba(255,255,255,0.9);
      cursor: pointer;
      font-size: 12px;
    }
    .summary-item.active {
      border-color: var(--accent);
      box-shadow: inset 0 0 0 1px rgba(46,111,149,0.18);
    }
    .summary-item-row {
      display: flex;
      align-items: start;
      justify-content: space-between;
      gap: 8px;
    }
    .summary-delete {
      border: 0;
      background: transparent;
      color: #7f2030;
      cursor: pointer;
      font-size: 12px;
      line-height: 1;
      padding: 0;
    }
    .message-log {
      font-family: var(--mono);
      font-size: 12px;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
      word-break: break-word;
      background: rgba(255,255,255,0.98);
      color: var(--text);
      padding: 12px;
      border-radius: 12px;
      border: 1px solid rgba(182,203,224,0.85);
      min-height: 96px;
    }
    .message-log.error {
      background: #fff0f1;
      color: #8f1d2c;
      border-color: #d58b95;
    }
    .canvas-shell {
      position: relative;
      border-radius: 18px;
      background:
        linear-gradient(0deg, rgba(247,251,255,0.9), rgba(247,251,255,0.9)),
        repeating-linear-gradient(0deg, rgba(99,140,173,0.09) 0px, rgba(99,140,173,0.09) 1px, transparent 1px, transparent 32px),
        repeating-linear-gradient(90deg, rgba(99,140,173,0.09) 0px, rgba(99,140,173,0.09) 1px, transparent 1px, transparent 32px);
      min-height: 1520px;
      overflow: hidden;
      position: relative;
    }
    .canvas-shell svg {
      position: absolute;
      inset: 0;
      width: 100%;
      height: 100%;
      pointer-events: none;
    }
    .wire-layer-back {
      z-index: 1;
    }
    .wire-layer-front {
      z-index: 4;
    }
    .canvas-nodes {
      position: relative;
      min-height: 1520px;
      z-index: 2;
    }
    .node {
      position: absolute;
      width: 190px;
      border-radius: 16px;
      border: 1px solid rgba(71,112,145,0.16);
      background: rgba(255,255,255,0.98);
      box-shadow: 0 12px 24px rgba(37,72,103,0.12);
      overflow: hidden;
      user-select: none;
    }
    .node.selected { outline: 3px solid rgba(46,111,149,0.32); }
    .node.error { outline: 3px solid rgba(177,0,24,0.54); border-color: #b10018; }
    .node-header {
      padding: 10px 12px;
      font-weight: 700;
      cursor: move;
      display: flex;
      justify-content: space-between;
      align-items: center;
      color: white;
      gap: 8px;
      min-height: 38px;
    }
    .node-title {
      min-width: 0;
      white-space: normal;
      overflow-wrap: anywhere;
      word-break: break-word;
      line-height: 1.15;
    }
    .node.detector .node-header { background: #2a9d8f; }
    .node.detector.unloaded .node-header { background: #8d99ae; }
    .node.merging .node-header { background: #f28f3b; }
    .node.zap .node-header { background: #5f0f40; }
    .node.asad .node-header { background: #6a4c93; }
    .node.cobo .node-header { background: #19647e; }
    .node.new_group .node-header { background: #354f52; color: #fff; }
    .node.new_group .node-body { color: var(--muted); }
    .node.placeholder {
      width: 360px;
      min-height: 360px;
      background: rgba(255,255,255,0.62);
      border: 2px dashed rgba(46,111,149,0.42);
      box-shadow: none;
      pointer-events: auto;
    }
    .node.placeholder .node-header { background: #577590; }
    .node.placeholder .node-body { padding: 0; }
    .node.detector.geom-compact {
      border-radius: 8px;
    }
    .node.detector.geom-compact .node-header {
      height: 100%;
      padding: 4px 6px;
      font-size: 11px;
      justify-content: stretch;
      text-align: center;
      line-height: 1.15;
      display: grid;
      grid-template-rows: 1fr auto;
      place-items: center;
      gap: 2px;
      overflow: hidden;
      color: #fff;
    }
    .node.detector.geom-compact .node-title {
      white-space: normal;
      overflow: visible;
      text-overflow: clip;
      display: grid;
      gap: 2px;
    }
    .geom-edit-button {
      border: 1px solid rgba(255,255,255,0.65);
      border-radius: 6px;
      background: rgba(255,255,255,0.16);
      color: #fff;
      font-size: 11px;
      line-height: 1.1;
      padding: 3px 8px;
      min-height: 18px;
      cursor: pointer;
      justify-self: center;
    }
    .node.detector.geom-compact .node-body { display: none; }
    .placeholder-svg {
      width: 100%;
      height: auto;
      display: block;
    }
    .placeholder-slot {
      fill: rgba(241,196,15,0.18);
      stroke: rgba(46,111,149,0.5);
      stroke-width: 1.4;
    }
    .placeholder-label {
      font-size: 12px;
      fill: #26394f;
      dominant-baseline: middle;
      text-anchor: middle;
      pointer-events: none;
    }
    .node-body { padding: 10px 12px 12px; font-size: 13px; }
    .node-inline-tools {
      display: grid;
      grid-template-columns: 1fr;
      gap: 2px;
      margin-top: 2px;
    }
    .node-inline-tools.single {
      grid-template-columns: 1fr;
    }
    .node-inline-tools.single input,
    .node-inline-tools input {
      width: 52%;
      min-width: 60px;
    }
    .node-inline-tools select,
    .node-inline-tools input,
    .node-inline-tools button {
      min-height: 20px;
      border-radius: 8px;
      border: 1px solid rgba(122,106,78,0.28);
      background: rgba(255,255,255,0.9);
      padding: 1px 5px;
      font-size: 11px;
    }
    .node-input-row {
      display: grid;
      grid-template-columns: auto 1fr;
      gap: 3px;
      align-items: center;
      margin-top: 4px;
      font-size: 12px;
      color: var(--muted);
    }
    .port-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin: 8px 0 0;
      gap: 10px;
      font-size: 12px;
    }
    .port-label { color: var(--muted); }
    .port {
      width: 14px;
      height: 14px;
      border-radius: 50%;
      border: 2px solid white;
      box-shadow: 0 0 0 1px rgba(47,38,25,0.28);
      cursor: crosshair;
      flex: 0 0 auto;
      background: #f4d35e;
    }
    .port.output {
      background: #b8b8b8;
    }
    .port.connected.input { background: var(--accent-2); }
    .port.connected.output { background: var(--accent); }
    .port.disabled {
      background: #111 !important;
      cursor: not-allowed;
    }
    .port.active-connect {
      box-shadow: 0 0 0 4px rgba(242,143,59,0.28);
      transform: scale(1.1);
    }
    .port-cell {
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .port-disable {
      width: 11px;
      height: 11px;
      border: 1px solid #111;
      background: transparent;
      cursor: pointer;
      user-select: none;
      line-height: 1;
      border-radius: 2px;
      flex: 0 0 auto;
    }
    .port-disable.disabled {
      background: #111;
    }
    .port-disable.enabled {
      background: transparent;
    }
    .port-disable:hover {
      box-shadow: 0 0 0 1px rgba(17,17,17,0.12);
    }
    .port-disable-label {
      font-size: 0;
      color: transparent;
      cursor: pointer;
    }
    .form-grid {
      display: grid;
      gap: 10px;
    }
    .field {
      display: grid;
      gap: 5px;
    }
    .field label {
      font-size: 12px;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: 0.04em;
    }
    .field input, .field select, .field textarea {
      width: 100%;
      border-radius: 10px;
      border: 1px solid rgba(122,106,78,0.28);
      background: rgba(255,255,255,0.88);
      padding: 9px 10px;
      min-height: 38px;
    }
    .form-grid.two {
      grid-template-columns: 1fr 1fr;
    }
    .form-grid.four {
      grid-template-columns: repeat(4, 1fr);
    }
    .status {
      font-family: var(--mono);
      font-size: 12px;
      white-space: pre-wrap;
      background: rgba(47,38,25,0.92);
      color: #fdf7ee;
      padding: 12px;
      border-radius: 14px;
      min-height: 180px;
    }
    .pill {
      display: inline-block;
      border-radius: 999px;
      padding: 3px 8px;
      font-size: 11px;
      background: rgba(79,119,45,0.12);
      color: var(--ok);
      margin-left: 6px;
    }
    .table {
      width: 100%;
      border-collapse: collapse;
      font-size: 12px;
    }
    .table th, .table td {
      border-bottom: 1px solid rgba(182,203,224,0.85);
      padding: 8px 6px;
      text-align: left;
    }
    .table input {
      width: 100%;
      min-width: 60px;
      border: 1px solid rgba(182,203,224,0.92);
      border-radius: 8px;
      padding: 6px 7px;
      background: rgba(255,255,255,0.92);
    }
    .modal {
      position: fixed;
      inset: 0;
      background: rgba(22,17,10,0.42);
      display: none;
      align-items: center;
      justify-content: center;
      padding: 24px;
      z-index: 100;
    }
    .modal.open { display: flex; }
    #navigatorModal.open {
      align-items: flex-start;
      justify-content: center;
      padding-top: 18px;
    }
    .modal-card {
      width: min(1100px, 100%);
      max-height: 90vh;
      overflow: auto;
      border-radius: 18px;
      background: var(--bg);
      box-shadow: var(--shadow);
      border: 1px solid var(--line);
      padding: 18px;
    }
    #placeholderCreateModal .modal-card {
      width: min(560px, 100%);
      padding: 14px;
    }
    .placeholder-create-grid {
      display: grid;
      gap: 8px;
    }
    .placeholder-create-row {
      display: grid;
      grid-template-columns: 110px 1fr auto;
      gap: 8px;
      align-items: center;
    }
    .placeholder-create-row.rectangular {
      grid-template-columns: 110px 1fr 1fr auto;
    }
    .placeholder-create-row input {
      min-width: 0;
    }
    .navigator-card {
      width: min(900px, 100%);
      max-height: 86vh;
      display: flex;
      flex-direction: column;
      background: var(--bg);
    }
    .navigator-controls {
      display: grid;
      gap: 8px;
      margin-bottom: 12px;
    }
    .navigator-row {
      display: grid;
      gap: 8px;
      align-items: center;
    }
    .navigator-row.path-row {
      grid-template-columns: auto max-content max-content max-content 1fr max-content;
    }
    .navigator-row.file-row {
      grid-template-columns: auto max-content 1fr max-content;
    }
    .navigator-row input {
      width: 100%;
      border: 1px solid rgba(122,106,78,0.28);
      border-radius: 8px;
      background: rgba(255,255,255,0.88);
      padding: 7px 9px;
      min-height: 32px;
    }
    .navigator-row button {
      width: auto;
      justify-self: start;
      white-space: nowrap;
    }
    .navigator-list {
      min-height: 0;
      overflow: auto;
      border: 1px solid rgba(211,198,170,0.75);
      border-radius: 12px;
      background: rgba(255,255,255,0.7);
      padding: 6px;
      display: grid;
      gap: 4px;
    }
    .navigator-entry {
      border: 1px solid rgba(211,198,170,0.55);
      border-radius: 10px;
      padding: 8px 10px;
      cursor: pointer;
      background: rgba(255,255,255,0.82);
    }
    .navigator-entry:hover {
      border-color: var(--accent);
    }
    .navigator-entry .name {
      font-size: 15px;
      font-weight: 500;
      color: #7a8089;
    }
    .navigator-entry.dir-entry .name {
      color: #0b4f8a;
      font-weight: 700;
    }
    .navigator-entry.conf-entry .name {
      color: #7a1f5c;
      font-weight: 700;
    }
    .modal-head {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
    }
    .modal-head h3 { margin: 0; }
    .tab-edit-input {
      min-width: 0;
      border-radius: 999px;
      border: 1px solid var(--line);
      background: rgba(255,250,240,0.98);
      padding: 3px 8px;
      color: var(--text);
      box-shadow: var(--shadow);
      font-size: 11px;
    }
    @media (max-width: 1280px) {
      .workspace { grid-template-columns: 280px 1fr; }
      .workspace > .panel:last-child { grid-column: 1 / -1; }
    }
    @media (max-width: 900px) {
      .workspace { grid-template-columns: 1fr; height: auto; }
      .workspace > .panel:nth-child(1) { order: 2; }
      .workspace > .panel:nth-child(2) { order: 1; }
      .workspace > .panel:nth-child(3) { order: 3; }
      .canvas-shell, .canvas-nodes { min-height: 900px; }
    }
  </style>
</head>
<body>
  <div class="app">
    <div class="topbar">
      <div class="headline">
        <h1>Silicon Mapping Editor</h1>
      </div>
      <div class="toolbar">
        <button onclick="openDetectorDb()">Detector DB</button>
        <button onclick="openGroupDb()">Group DB</button>
        <button onclick="openBoardTypesDb()">Board Types</button>
        <button onclick="openCheckMapping()">Check Mapping</button>
        <button onclick="window.lilakWebFormat.toggleShortcutsModal()">Shortcuts</button>
      </div>
    </div>
    <div class="tabs-wrap">
      <div class="tabs">
        <span class="subtoolbar-title">Experiments</span>
        <span id="experimentTabs"></span>
      </div>
      <div class="tabs">
        <span class="subtoolbar-title">User Groups</span>
        <span id="groupTabs"></span>
      </div>
      <div class="subtoolbar block">
        <div class="tabs">
          <span class="subtoolbar-title">Placeholder</span>
          <button class="tab add-tab" onclick="openPlaceholderCreateModal()">+</button>
          <span id="placeholderTabs"></span>
        </div>
        <div class="tabs">
          <span class="subtoolbar-title">Detectors / Boards</span>
          <span id="detectorBoardButtons"></span>
        </div>
      </div>
    </div>
    <div class="workspace">
      <section class="panel summary-panel">
        <div class="panel-header"><h2>Group Summary</h2></div>
        <div class="panel-body">
          <div class="summary-stack">
            <div>
              <h2 style="margin-bottom:8px;">Messages</h2>
              <div class="message-log" id="messageLog">Waiting for messages.</div>
            </div>
            <div id="groupSummary" class="group-summary-scroll"></div>
          </div>
        </div>
      </section>
      <section class="panel canvas-panel">
        <div class="panel-header" style="display:flex; align-items:center; justify-content:space-between; gap:10px;">
          <h2 id="canvasTitle">Board Canvas</h2>
          <div class="toolbar">
            <button onclick="arrangeCanvasItems()" id="arrangeBtn">Arrange</button>
            <button onclick="toggleBoardView()" id="boardViewBtn">Board View</button>
            <button onclick="toggleGeomView()" id="geomViewBtn">Upst. View</button>
            <button onclick="toggleShowAllZaps()" id="showAllZapsBtn">Show all ZAPs</button>
            <button onclick="toggleShowAllCobos()" id="showAllCobosBtn">Show all Cobos</button>
            <button onclick="toggleShowAllDetectors()" id="showAllDetectorsBtn">Show all Detectors</button>
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
              <h2 style="margin-bottom:8px;">Export Log</h2>
              <div class="status" id="statusLog">Waiting for export.</div>
            </div>
          </div>
        </div>
      </section>
    </div>
  </div>

  <div class="modal" id="detectorDbModal">
    <div class="modal-card">
      <div class="modal-head">
        <h3>Detector Database</h3>
        <div class="toolbar">
          <button onclick="saveDetectorDb()" class="primary">Save Detector DB</button>
          <button onclick="closeModal('detectorDbModal')">Close</button>
        </div>
      </div>
      <p class="help">Detector numbers are local now. Thickness is exported to the mapping file. Width and height use active area dimensions.</p>
      <div style="overflow:auto;">
        <table class="table">
          <thead>
            <tr><th>#</th><th>Thickness (um)</th><th>Bias (V)</th><th>Width (mm)</th><th>Height (mm)</th><th>Note</th></tr>
          </thead>
          <tbody id="detectorDbTable"></tbody>
        </table>
      </div>
    </div>
  </div>

  <div class="modal" id="groupDbModal">
    <div class="modal-card">
      <div class="modal-head">
        <h3>Group DB</h3>
        <div class="toolbar">
          <button onclick="addGroupPreset()">Add Group Preset</button>
          <button onclick="saveGroupDb()" class="primary">Save Group DB</button>
          <button onclick="loadGroupDb()">Load Group DB</button>
          <button onclick="closeModal('groupDbModal')">Close</button>
        </div>
      </div>
      <p class="help">Edit detector-group presets. The preset name is what detector boxes use as ring/group type. Save and load this DB as a separate parameter file.</p>
      <div style="overflow:auto;">
        <table class="table">
          <thead>
            <tr><th>Name</th><th>Capacity</th><th>Radius (mm)</th><th>Z (mm)</th><th>Phi Offset</th><th>Half Opening</th><th></th></tr>
          </thead>
          <tbody id="groupDbTable"></tbody>
        </table>
      </div>
    </div>
  </div>

  <div class="modal" id="boardTypesModal">
    <div class="modal-card">
      <div class="modal-head">
        <h3>Board Types</h3>
        <div class="toolbar">
          <button onclick="saveBoardTypesDb()" class="primary">Save Board Types</button>
          <button onclick="loadBoardTypesDb()">Load Board Types</button>
          <button onclick="closeModal('boardTypesModal')">Close</button>
        </div>
      </div>
      <p class="help">Edit pin-level export mapping here. For each pin, set where junction and ohmic channels start: AGET number and first channel.</p>
      <div id="boardTypesTable"></div>
    </div>
  </div>

  <div class="modal" id="checkMappingModal">
    <div class="modal-card">
      <div class="modal-head">
        <h3>Check Mapping</h3>
        <div class="toolbar">
          <button onclick="closeModal('checkMappingModal')">Close</button>
        </div>
      </div>
      <div class="form-grid two" style="margin-bottom:10px;">
        <div class="field">
          <label>Mapping Folder</label>
          <div style="display:flex; gap:6px;">
            <input id="checkMappingPath" type="text">
            <button onclick="browseCheckMappingPath()">Browse</button>
          </div>
        </div>
        <div class="field">
          <label>Print Count</label>
          <input id="checkMappingNumPrint" type="number" value="20">
        </div>
      </div>
      <div class="form-grid four" style="margin-bottom:10px;">
        <div class="field">
          <label>COBO</label>
          <input id="checkMappingCobo" type="number" value="0">
        </div>
        <div class="field">
          <label>ASAD</label>
          <input id="checkMappingAsad" type="number" value="0">
        </div>
        <div class="field">
          <label>AGET</label>
          <input id="checkMappingAget" type="number" value="3">
        </div>
        <div class="field">
          <label>CHAN</label>
          <input id="checkMappingChan" type="number" value="0">
        </div>
      </div>
      <div class="toolbar" style="margin-bottom:10px;">
        <button onclick="runCheckMapping('detectors')">Print Detectors</button>
        <button onclick="runCheckMapping('channels')">Print Channels</button>
        <button onclick="runCheckMapping('lookup')">Print Channel Lookup</button>
      </div>
      <pre id="checkMappingOutput" class="status" style="white-space:pre-wrap; max-height:420px; overflow:auto; margin:0;">Waiting for output.</pre>
    </div>
  </div>

  <div class="modal" id="nodeEditModal">
    <div class="modal-card">
      <div class="modal-head">
        <h3 id="nodeEditTitle">Edit Item</h3>
        <div class="toolbar">
          <button onclick="closeModal('nodeEditModal')">Close</button>
        </div>
      </div>
      <div id="nodeEditContent"></div>
    </div>
  </div>

  <div class="modal" id="placeholderCreateModal">
    <div class="modal-card">
      <div class="modal-head">
        <h3 id="placeholderCreateTitle">Create Placeholder</h3>
        <div class="toolbar">
          <button onclick="closePlaceholderCreateModal()">Close</button>
        </div>
      </div>
      <div id="placeholderCreateFields" class="placeholder-create-grid"></div>
    </div>
  </div>

  <div class="modal" id="placeholderSelectModal">
    <div class="modal-card">
      <div class="modal-head">
        <h3>Select placeholder</h3>
        <div class="toolbar">
          <button onclick="closeModal('placeholderSelectModal')">Close</button>
        </div>
      </div>
      <div id="placeholderSelectList" class="navigator-list"></div>
    </div>
  </div>

  <div class="modal" id="navigatorModal">
    <div class="modal-card navigator-card">
      <div class="modal-head">
        <h3 id="navigatorTitle">Navigator</h3>
        <div class="toolbar">
          <button onclick="closeNavigator('__cancel__')">Close</button>
        </div>
      </div>
      <div class="navigator-controls">
        <div class="navigator-row path-row">
          <span class="help">Path</span>
          <button onclick="navigatorGoCommon()">COMMON</button>
          <button onclick="navigatorGoHome()">HOME</button>
          <button onclick="navigatorGoUp()">Up</button>
          <input id="navigatorPathInput" type="text">
          <button onclick="navigatorRefresh()">Go</button>
        </div>
        <div class="navigator-row file-row">
          <span class="help">File</span>
          <span></span>
          <input id="navigatorFileInput" type="text">
          <button id="navigatorConfirmBtn" onclick="confirmNavigatorSelection()">Select</button>
        </div>
      </div>
      <div class="navigator-list" id="navigatorList"></div>
    </div>
  </div>

  <div class="modal" id="shortcutsModal">
    <div class="modal-card">
      <div class="modal-head">
        <h3>Shortcuts</h3>
        <div class="toolbar">
          <button onclick="closeModal('shortcutsModal')">Close</button>
        </div>
      </div>
      <table class="shortcut-sheet">
        <thead>
          <tr><th>Shortcut</th><th>Description</th></tr>
        </thead>
        <tbody>
          <tr><td>?</td><td>Open or close this Shortcuts window.</td></tr>
          <tr><td>Ctrl/Cmd+S</td><td>Save current experiment and export mapping when possible.</td></tr>
          <tr><td>Ctrl/Cmd+L</td><td>Load experiment configuration.</td></tr>
          <tr><td>Ctrl/Cmd+A</td><td>Arrange visible canvas items.</td></tr>
          <tr><td>Ctrl/Cmd+G</td><td>Add a new group.</td></tr>
          <tr><td>Ctrl/Cmd+E</td><td>Export mapping for current experiment.</td></tr>
          <tr><td>Ctrl/Cmd+B</td><td>Toggle Board View.</td></tr>
          <tr><td>Ctrl/Cmd+H</td><td>Select Placeholder Homeless tab.</td></tr>
          <tr><td>Ctrl/Cmd+X</td><td>Select User Groups No group tab.</td></tr>
          <tr><td>Upst. View</td><td>Use geometrical detector placement with placeholders.</td></tr>
          <tr><td>Ctrl/Cmd+0</td><td>Select User Groups All tab.</td></tr>
          <tr><td>Ctrl/Cmd+1..9</td><td>Select user group by tab index.</td></tr>
          <tr><td>Tab / Shift+Tab</td><td>Cycle selected canvas item.</td></tr>
          <tr><td>Backspace</td><td>Delete selected item.</td></tr>
          <tr><td>Esc</td><td>Close navigator, database, check, and shortcut popups.</td></tr>
        </tbody>
      </table>
    </div>
  </div>

  <script>
    const stateStore = {
      state: null,
      detectorDb: null,
      groupDb: null,
      statePath: "",
      selectedExperimentId: null,
      selectedGroupId: null,
      selectedNodeId: null,
      dragNodeId: null,
      dragOffsetX: 0,
      dragOffsetY: 0,
      dragConnection: null,
      pendingConnection: null,
      navigator: null,
      cwd: "",
      commonMappingDir: "",
      lastExperimentPath: "",
      lastGroupPath: "",
      lastGroupDbPath: "",
      lastExportDir: "",
      lastCheckMappingPath: "",
      boardConfigs: {},
      errorNodeIds: [],
      errorGroupIds: [],
      autosaveTimers: {},
      boardViewMode: false,
      geomViewMode: false,
      showAllZaps: false,
      showAllCobos: false,
      showAllDetectors: false,
      selectedPlaceholderId: null,
      pendingPlaceholderKind: "",
      pendingPlaceholderAttachSelection: false,
      focusGroupNameInput: false,
      managerPositions: {},
    };

    const STATIC_NODE_PORTS = {
      detector: [{ id: "OUT", direction: "output", label: "Signal" }],
      cobo: [
        { id: "IN0", direction: "input", label: "0" },
        { id: "IN1", direction: "input", label: "1" },
        { id: "IN2", direction: "input", label: "2" },
        { id: "IN3", direction: "input", label: "3" },
      ],
    };

    const DETECTOR_TYPES = ["X6", "BB10", "CSD", "QQQ5"];
    const GEOM_DL = 72;
    const PLACEHOLDER_HEADER_H = 42;
    const GROUP_PRESET_ALIASES = {
      "R12-I": "Ring-12 (dE)",
      "R12-O": "Ring-12 (E)",
      "R16": "Ring-16",
      "PQ": "QQQ5 plane",
      "U": "Free",
    };

    function boardConfigMap() {
      const items = stateStore.boardConfigs?.board_types || [];
      return Object.fromEntries(items.map(item => [item.key, item]));
    }

    function boardConfig(boardType) {
      return boardConfigMap()[boardType] || boardConfigMap().X6 || null;
    }

    function boardPinLabelFromData(pin, config=null) {
      const pinId = String(pin?.id || "PIN");
      const junctionAget = toInt(pin?.junction_aget, 0);
      const junctionFirst = toInt(pin?.junction_first_channel, 0);
      const ohmicAget = toInt(pin?.ohmic_aget, 0);
      const ohmicFirst = toInt(pin?.ohmic_first_channel, 0);
      const junctionCount = Math.max(0, toInt(config?.junction_strip_count, 0)) * 4;
      const ohmicCount = Math.max(0, toInt(config?.ohmic_strip_count, 0)) * 4;
      const junctionLast = junctionCount > 0 ? junctionFirst + junctionCount - 1 : junctionFirst;
      const ohmicLast = ohmicCount > 0 ? ohmicFirst + ohmicCount - 1 : ohmicFirst;
      return `${pinId.replace("PIN", "Pin ")} > AGET#${junctionAget} (${junctionFirst}-${junctionLast}), AGET#${ohmicAget} (${ohmicFirst}-${ohmicLast})`;
    }

    function normalizeGroupPresetName(name) {
      return GROUP_PRESET_ALIASES[name] || name || "Free";
    }

    function nodeBoardType(node) {
      if (!node)
        return "X6";
      if (node.kind === "detector")
        return node.data?.detector_type || "X6";
      if (node.data?.board_type)
        return node.data.board_type;
      const label = String(node.label || "");
      if (label.startsWith("QQQ5"))
        return "QQQ5";
      if (label.startsWith("BB10"))
        return "BB10";
      if (label.startsWith("X6"))
        return "X6";
      if (node.kind === "zap") {
        const group = currentGroup();
        const incoming = canvasConnections(group).find(item => item.to.node_id === node.id);
        const source = incoming ? nodeByIdFromExperiment(currentExperiment(), incoming.from.node_id) : null;
        if (source)
          return nodeBoardType(source);
      }
      return "X6";
    }

    function nodePorts(node) {
      if (!node)
        return [];
      if (node.kind === "detector")
        return STATIC_NODE_PORTS.detector;
      if (node.kind === "new_group")
        return [{ id: "IN", direction: "input", label: "Detectors" }];
      if (node.kind === "placeholder")
        return [];
      if (node.kind === "cobo")
        return STATIC_NODE_PORTS.cobo;
      const config = boardConfig(nodeBoardType(node));
      if (!config)
        return [];
      if (node.kind === "merging") {
        return [
          ...(config.merging_slots || []).map(item => ({ id: item.id, direction: "input", label: item.label })),
          { id: "OUT", direction: "output", label: "To ZAP" },
        ];
      }
      if (node.kind === "zap") {
        return [
          ...(config.zap_pins || []).map(item => ({ id: item.id, direction: "input", label: boardPinLabelFromData(item, config) })),
          { id: "OUT", direction: "output", label: "To COBO" },
        ];
      }
      return [];
    }

    function boardButtonDefinitions() {
      const configs = stateStore.boardConfigs?.board_types || [];
      const buttons = [];
      const seenZapLabels = new Set();
      configs.forEach(config => {
        buttons.push({ kind: "merging", boardType: config.key, label: config.merging_label || `${config.key} Merging` });
        const zapLabel = config.zap_label || `${config.key} ZAP/AsAd`;
        if (!seenZapLabels.has(zapLabel)) {
          buttons.push({ kind: "zap", boardType: config.key, label: zapLabel });
          seenZapLabels.add(zapLabel);
        }
      });
      return buttons;
    }

    function zapCompatibleTypes(boardType) {
      if (boardType === "X6" || boardType === "QQQ5")
        return ["X6", "QQQ5"];
      return [boardType];
    }

    function uid(prefix) {
      return `${prefix}-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`;
    }

    function currentExperiment() {
      return stateStore.state.experiments.find(item => item.id === stateStore.selectedExperimentId) || null;
    }

    function ensureExperimentBoardConnections(experiment) {
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      if (Array.isArray(experiment.board_connections))
        return;
      experiment.board_connections = [];
      experiment.groups.forEach(group => {
        if (!Array.isArray(group.connections))
          group.connections = [];
        const kept = [];
        group.connections.forEach(connection => {
          const fromNode = group.nodes.find(item => item.id === connection.from.node_id);
          const toNode = group.nodes.find(item => item.id === connection.to.node_id);
          if (fromNode && toNode && fromNode.kind !== "detector" && toNode.kind !== "detector")
            experiment.board_connections.push(connection);
          else
            kept.push(connection);
        });
        group.connections = kept;
      });
    }

    function ensureExperimentCollections(experiment) {
      if (!experiment)
        return;
      if (!Array.isArray(experiment.placeholders))
        experiment.placeholders = [];
      if (!Array.isArray(experiment.loose_nodes))
        experiment.loose_nodes = [];
    }

    function experimentPlaceholders(experiment=currentExperiment()) {
      if (!experiment)
        return [];
      ensureExperimentCollections(experiment);
      const placeholders = [...experiment.placeholders];
      for (const group of experiment.groups || []) {
        for (const node of group.nodes || []) {
          if (node.kind === "placeholder" && !placeholders.some(item => item.id === node.id))
            placeholders.push(node);
        }
      }
      return placeholders;
    }

    function experimentDetectorNodes(experiment=currentExperiment()) {
      if (!experiment)
        return [];
      ensureExperimentCollections(experiment);
      const nodes = [...experiment.loose_nodes.filter(node => node.kind === "detector")];
      for (const group of experiment.groups || [])
        nodes.push(...(group.nodes || []).filter(node => node.kind === "detector"));
      return nodes;
    }

    function managerPositionKey() {
      const experiment = currentExperiment();
      if (!experiment)
        return "";
      if (stateStore.selectedGroupId === "__homeless__")
        return `${experiment.id}:homeless`;
      if (stateStore.selectedGroupId === "__nogroup__")
        return `${experiment.id}:nogroup`;
      return `${experiment.id}:${stateStore.selectedGroupId || "manager"}`;
    }

    function managerPosition(fallbackX=360, fallbackY=56) {
      const key = managerPositionKey();
      return stateStore.managerPositions[key] || { x: fallbackX, y: fallbackY };
    }

    function storeManagerPosition(node) {
      if (!node || node.kind !== "new_group")
        return;
      const key = managerPositionKey();
      if (!key)
        return;
      stateStore.managerPositions[key] = { x: node.x, y: node.y };
    }

    function noGroupGroup() {
      const experiment = currentExperiment();
      if (!experiment)
        return null;
      ensureExperimentCollections(experiment);
      const selected = (experiment.loose_nodes || []).filter(node => node.kind === "detector" && !node.data?.placeholder_id && node.data?.new_group_selected);
      const managerPos = managerPosition();
      return {
        id: "__nogroup__",
        name: "No group",
        no_group: true,
        group_preset: "Free",
        nodes: [
          ...experiment.loose_nodes.filter(node => node.kind !== "detector" || !node.data?.placeholder_id),
          {
            id: "__new_group__",
            kind: "new_group",
            label: "Group manager",
            x: managerPos.x,
            y: managerPos.y,
            z_order: 1000,
            data: { selected_count: selected.length },
          },
        ],
        connections: selected.map(node => ({
          id: `new-group-${node.id}`,
          from: { node_id: node.id, port: "OUT" },
          to: { node_id: "__new_group__", port: "IN" },
        })),
      };
    }

    function homelessGroup() {
      const experiment = currentExperiment();
      if (!experiment)
        return null;
      ensureExperimentCollections(experiment);
      const selected = experimentDetectorNodes(experiment).filter(node => node.kind === "detector" && !node.data?.placeholder_id && node.data?.new_group_selected);
      const managerPos = managerPosition();
      return {
        id: "__homeless__",
        name: "Homeless",
        homeless_view: true,
        group_preset: "Free",
        nodes: [
          ...experimentDetectorNodes(experiment).filter(node => node.kind === "detector" && !node.data?.placeholder_id),
          {
            id: "__new_group__",
            kind: "new_group",
            label: "Placeholder manager",
            x: managerPos.x,
            y: managerPos.y,
            z_order: 1000,
            data: { selected_count: selected.length },
          },
        ],
        connections: selected.map(node => ({
          id: `new-group-${node.id}`,
          from: { node_id: node.id, port: "OUT" },
          to: { node_id: "__new_group__", port: "IN" },
        })),
      };
    }

    function placeholderViewGroup() {
      const experiment = currentExperiment();
      if (!experiment || !stateStore.selectedPlaceholderId)
        return null;
      ensureExperimentCollections(experiment);
      const placeholder = experimentPlaceholders(experiment).find(node => node.id === stateStore.selectedPlaceholderId);
      if (!placeholder)
        return null;
      const detectors = experimentDetectorNodes(experiment).filter(node =>
        stateStore.showAllDetectors || node.data?.placeholder_id === placeholder.id
      );
      return {
        id: `__placeholder__${placeholder.id}`,
        name: placeholder.label,
        placeholder_view: true,
        group_preset: "Free",
        nodes: [placeholder, ...detectors],
        connections: [],
      };
    }

    function boardViewGroup() {
      const experiment = currentExperiment();
      if (!experiment)
        return null;
      ensureExperimentBoardConnections(experiment);
      const nodes = [];
      experiment.groups.forEach(group => {
        (group.nodes || []).forEach(node => {
          if (node.kind !== "detector" && node.kind !== "placeholder")
            nodes.push(node);
        });
      });
      return {
        id: "__board_view__",
        name: "Board View",
        board_view: true,
        group_preset: "R12-I",
        nodes,
        connections: experiment.board_connections,
      };
    }

    function allViewGroup() {
      const experiment = currentExperiment();
      if (!experiment)
        return null;
      ensureExperimentBoardConnections(experiment);
      ensureExperimentCollections(experiment);
      const nodes = [];
      const connections = [];
      nodes.push(...experiment.loose_nodes);
      experiment.groups.forEach(group => {
        nodes.push(...(group.nodes || []).filter(node => node.kind !== "placeholder"));
        connections.push(...(group.connections || []));
      });
      connections.push(...(experiment.board_connections || []));
      return {
        id: "__all__",
        name: "All",
        all_view: true,
        group_preset: "Free",
        nodes,
        connections,
      };
    }

    function placeholderOwnerGroup(placeholderId=stateStore.selectedPlaceholderId) {
      const experiment = currentExperiment();
      if (!experiment || !placeholderId)
        return null;
      ensureExperimentCollections(experiment);
      if ((experiment.placeholders || []).some(node => node.id === placeholderId))
        return noGroupGroup();
      return (experiment.groups || []).find(group => (group.nodes || []).some(node => node.id === placeholderId)) || null;
    }

    function currentGroup() {
      const experiment = currentExperiment();
      if (!experiment) return null;
      if (stateStore.boardViewMode)
        return boardViewGroup();
      if (stateStore.selectedGroupId === "__all__")
        return allViewGroup();
      if (stateStore.selectedGroupId === "__nogroup__")
        return noGroupGroup();
      if (stateStore.selectedGroupId === "__homeless__")
        return homelessGroup();
      if (stateStore.selectedPlaceholderId)
        return placeholderViewGroup() || null;
      return experiment.groups.find(item => item.id === stateStore.selectedGroupId) || null;
    }

    function selectedDataGroup() {
      const experiment = currentExperiment();
      if (!experiment)
        return null;
      if (stateStore.selectedPlaceholderId)
        return noGroupGroup();
      if (stateStore.selectedGroupId === "__homeless__")
        return homelessGroup();
      if (stateStore.selectedGroupId === "__nogroup__")
        return noGroupGroup();
      return experiment.groups.find(item => item.id === stateStore.selectedGroupId) || experiment.groups[0] || null;
    }

    function currentVisibleNodes() {
      const experiment = currentExperiment();
      const group = currentGroup();
      if (!group)
        return [];
      if (group.board_view)
        return group.nodes || [];
      if (group.all_view)
        return stateStore.boardViewMode ? (group.nodes || []).filter(node => node.kind !== "placeholder") : (group.nodes || []);
      if (group.no_group || group.homeless_view || group.placeholder_view)
        return group.nodes || [];
      if (stateStore.geomViewMode)
        return (group.nodes || []).filter(node => node.kind === "detector" || node.kind === "placeholder");
      const nodes = (group.nodes || []).filter(node => node.kind !== "placeholder");
      const seen = new Set(nodes.map(node => node.id));
      for (const otherGroup of experiment?.groups || []) {
        if (otherGroup.id === group.id)
          continue;
        for (const node of otherGroup.nodes || []) {
          if (seen.has(node.id))
            continue;
          if (stateStore.showAllZaps && node.kind === "zap") {
            nodes.push(node);
            seen.add(node.id);
          } else if (stateStore.showAllCobos && node.kind === "cobo") {
            nodes.push(node);
            seen.add(node.id);
          }
        }
      }
      return nodes;
    }

    function currentNodeById(nodeId) {
      return currentVisibleNodes().find(item => item.id === nodeId) || nodeByIdFromExperiment(currentExperiment(), nodeId);
    }

    function currentSelection() {
      if (!stateStore.selectedNodeId)
        return null;
      return currentNodeById(stateStore.selectedNodeId);
    }

    function nextZOrder(group) {
      const values = group.nodes.map(node => Number(node.z_order) || 1);
      return (values.length === 0 ? 1 : Math.max(...values)) + 1;
    }

    function connectedNodeIds(nodeId) {
      const group = currentGroup();
      if (!group || !nodeId)
        return new Set();
      const ids = new Set([nodeId]);
      for (const connection of canvasConnections(group)) {
        if (connection.from.node_id === nodeId)
          ids.add(connection.to.node_id);
        if (connection.to.node_id === nodeId)
          ids.add(connection.from.node_id);
      }
      return ids;
    }

    function bringNodeToFront(nodeId) {
      const group = currentGroup();
      if (!group) return;
      const ids = connectedNodeIds(nodeId);
      if (ids.size === 0)
        return;
      let z = nextZOrder(group);
      const ordered = currentVisibleNodes()
        .filter(node => ids.has(node.id))
        .sort((a, b) => {
          if (a.id === nodeId) return 1;
          if (b.id === nodeId) return -1;
          return (Number(a.z_order) || 1) - (Number(b.z_order) || 1);
        });
      ordered.forEach(node => {
        node.z_order = z++;
      });
    }

    function toggleShowAllZaps() {
      stateStore.showAllZaps = !stateStore.showAllZaps;
      render();
    }

    function toggleShowAllCobos() {
      stateStore.showAllCobos = !stateStore.showAllCobos;
      render();
    }

    function toggleShowAllDetectors() {
      stateStore.showAllDetectors = !stateStore.showAllDetectors;
      render();
    }

    function promptPath(label, fallback) {
      const value = window.prompt(label, fallback || "");
      if (value === null) return null;
      const trimmed = value.trim();
      return trimmed || fallback || null;
    }

    function showBanner(message, kind="ok") {
      const log = document.getElementById("messageLog");
      if (!log) return;
      log.textContent = message;
      log.classList.toggle("error", kind === "error");
    }

    function clearExportErrors() {
      stateStore.errorNodeIds = [];
      stateStore.errorGroupIds = [];
    }

    function applyExportError(payload) {
      stateStore.errorNodeIds = Array.isArray(payload?.node_ids) ? payload.node_ids : [];
      stateStore.errorGroupIds = Array.isArray(payload?.group_ids) ? payload.group_ids : [];
      render(false);
    }

    function logStatus(message) {
      document.getElementById("statusLog").textContent = message;
    }

    function debounceAutosave(key, callback, delay=500) {
      const timers = stateStore.autosaveTimers || (stateStore.autosaveTimers = {});
      if (timers[key])
        clearTimeout(timers[key]);
      timers[key] = setTimeout(async () => {
        timers[key] = null;
        try {
          await callback();
        } catch (_error) {
        }
      }, delay);
    }

    function autosaveDetectorDbToCommon() {
      return fetchJson("/api/save_detector_db", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ detector_db: stateStore.detectorDb }),
      });
    }

    function autosaveGroupDbToCommon() {
      return fetchJson("/api/save_group_db", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ group_db: stateStore.groupDb }),
      });
    }

    function autosaveBoardTypesToCommon() {
      return fetchJson("/api/save_board_configs", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ board_configs: stateStore.boardConfigs }),
      });
    }

    async function fetchJson(url, options={}) {
      const response = await fetch(url, options);
      const payload = await response.json();
      if (!response.ok) {
        const error = new Error(payload.error || "Request failed");
        error.payload = payload;
        throw error;
      }
      return payload;
    }

    async function refreshNavigator(pathValue) {
      const payload = await fetchJson("/api/listdir", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path: pathValue || "" }),
      });
      const nav = stateStore.navigator;
      if (!nav) return;
      nav.currentDir = payload.current_dir;
      nav.entries = payload.entries || [];
      renderNavigator();
    }

    function renderNavigator() {
      const nav = stateStore.navigator;
      if (!nav) return;
      document.getElementById("navigatorTitle").textContent = nav.title;
      const modal = document.getElementById("navigatorModal");
      const pathInput = document.getElementById("navigatorPathInput");
      const fileInput = document.getElementById("navigatorFileInput");
      const confirmBtn = document.getElementById("navigatorConfirmBtn");
      pathInput.value = nav.currentDir || "";
      fileInput.value = nav.fileName || "";
      confirmBtn.textContent = nav.confirmLabel || "Select";
      fileInput.disabled = nav.mode === "directory";
      modal.onkeydown = event => {
        if (event.key === "Enter") {
          event.preventDefault();
          confirmNavigatorSelection();
        }
      };
      pathInput.onkeydown = event => {
        if (event.key === "Enter") {
          event.preventDefault();
          confirmNavigatorSelection();
        }
      };
      fileInput.onkeydown = event => {
        if (event.key === "Enter") {
          event.preventDefault();
          confirmNavigatorSelection();
        }
      };
      queueMicrotask(() => {
        if (nav.mode === "save") {
          fileInput.focus();
          fileInput.select();
        } else {
          pathInput.focus();
        }
      });
      window.lilakWebFormat.renderNavigatorEntries({
        listId: "navigatorList",
        entries: nav.entries,
        escapeHtml,
        parameterPredicate: name => String(name || "").toLowerCase().endsWith(".conf") || window.lilakWebFormat.isParameterFile(name),
        handlers: {
          clickDir: entry => refreshNavigator(entry.path),
          clickFile: entry => {
            if (nav.mode === "directory")
              return;
            fileInput.value = String(entry.path || "").split("/").pop() || "";
            nav.fileName = fileInput.value;
          },
          openDir: entry => refreshNavigator(entry.path),
          openFile: entry => {
            if (nav.mode === "directory") {
              confirmNavigatorSelection();
              return;
            }
            fileInput.value = String(entry.path || "").split("/").pop() || "";
            nav.fileName = fileInput.value;
            confirmNavigatorSelection();
          },
        },
      });
    }

    async function openNavigator(options) {
      stateStore.navigator = {
        mode: options.mode,
        title: options.title,
        confirmLabel: options.confirmLabel,
        fileName: options.fileName || "",
        currentDir: "",
        entries: [],
        resolve: null,
      };
      document.getElementById("navigatorModal").classList.add("open");
      const result = await new Promise(async resolve => {
        stateStore.navigator.resolve = resolve;
        await refreshNavigator(options.path || "");
      });
      return result;
    }

    function closeNavigator(result=null) {
      const nav = stateStore.navigator;
      document.getElementById("navigatorModal").classList.remove("open");
      stateStore.navigator = null;
      if (nav?.resolve)
        nav.resolve(result);
    }

    async function navigatorGoUp() {
      const nav = stateStore.navigator;
      if (!nav) return;
      await refreshNavigator(`${nav.currentDir || ""}/..`);
    }

    async function navigatorGoCommon() {
      await refreshNavigator(stateStore.commonMappingDir || stateStore.cwd || "");
    }

    async function navigatorGoHome() {
      await refreshNavigator(stateStore.cwd || "");
    }

    async function navigatorRefresh() {
      const pathValue = document.getElementById("navigatorPathInput").value;
      await refreshNavigator(pathValue);
    }

    function confirmNavigatorSelection() {
      const nav = stateStore.navigator;
      if (!nav) return;
      const basePath = (document.getElementById("navigatorPathInput").value || nav.currentDir || "").trim();
      if (nav.mode === "directory") {
        if (!basePath) return;
        closeNavigator(basePath);
        return;
      }
      const fileName = (document.getElementById("navigatorFileInput").value || "").trim();
      const path = fileName ? `${basePath.replace(/\/+$/, "")}/${fileName}` : basePath;
      if (!path) return;
      closeNavigator(path);
    }

    function directoryOfPath(path) {
      if (!path)
        return "";
      const normalized = String(path).replace(/\\/g, "/").replace(/\/+$/, "");
      const slashIndex = normalized.lastIndexOf("/");
      if (slashIndex <= 0)
        return normalized || "";
      return normalized.slice(0, slashIndex);
    }

    async function pickNativeOpenPath() {
      try {
        const result = await fetchJson("/api/pick_open_path", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({}),
        });
        return result.path || null;
      } catch (error) {
        return null;
      }
    }

    async function pickNativeSavePath(suggestedName) {
      try {
        const result = await fetchJson("/api/pick_save_path", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ suggested_name: suggestedName || "" }),
        });
        return result.path || null;
      } catch (error) {
        return null;
      }
    }

    async function pickOpenTextFile(types, fallbackPrompt, fallbackName) {
      if (window.showOpenFilePicker) {
        const [handle] = await window.showOpenFilePicker({
          multiple: false,
          types,
        });
        if (!handle)
          return null;
        const file = await handle.getFile();
        return { name: file.name, content: await file.text() };
      }
      const path = promptPath(fallbackPrompt, fallbackName);
      if (!path)
        return null;
      const payload = await fetchJson("/api/load_state", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path }),
      });
      return { path, payload };
    }

    async function pickOpenBundle(expectedTypes, types, fallbackPrompt, fallbackName, fallbackApi, initialPath="") {
      const navigatorPath = await openNavigator({
        mode: "open",
        title: "Open File",
        confirmLabel: "Open",
        fileName: "",
        path: initialPath || stateStore.cwd || stateStore.statePath || "",
      });
      if (navigatorPath === "__cancel__")
        return null;
      if (navigatorPath) {
        const parsed = await fetchJson(fallbackApi, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ path: navigatorPath }),
        });
        return { name: navigatorPath, payload: parsed };
      }
      const nativePath = await pickNativeOpenPath();
      if (nativePath) {
        const parsed = await fetchJson(fallbackApi, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ path: nativePath }),
        });
        return { name: nativePath, payload: parsed };
      }
      if (window.showOpenFilePicker) {
        const [handle] = await window.showOpenFilePicker({
          multiple: false,
          types,
        });
        if (!handle)
          return null;
        const file = await handle.getFile();
        const parsed = await fetchJson("/api/parse_bundle", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ content: await file.text(), expected_types: expectedTypes }),
        });
        return { name: file.name, parsed };
      }
      const path = promptPath(fallbackPrompt, fallbackName);
      if (!path)
        return null;
      const parsed = await fetchJson(fallbackApi, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path }),
      });
      return { name: path, payload: parsed };
    }

    async function saveBundleWithPicker(bundleType, payload, suggestedName, fallbackApi, initialPath="") {
      const navigatorPath = await openNavigator({
        mode: "save",
        title: `Save ${bundleType}`,
        confirmLabel: "Save",
        fileName: suggestedName || "",
        path: initialPath || stateStore.cwd || stateStore.statePath || "",
      });
      if (navigatorPath === "__cancel__")
        return null;
      if (navigatorPath) {
        return await fetchJson(fallbackApi, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ ...payload, path: navigatorPath }),
        });
      }
      const nativePath = await pickNativeSavePath(suggestedName);
      if (nativePath) {
        return await fetchJson(fallbackApi, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ ...payload, path: nativePath }),
        });
      }
      if (window.showSaveFilePicker) {
        const serialized = await fetchJson("/api/serialize_bundle", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ bundle_type: bundleType, payload }),
        });
        const handle = await window.showSaveFilePicker({
          suggestedName,
          types: [{
            description: "LILAK parameter file",
            accept: { "text/plain": [".conf", ".par", ".mac"] },
          }],
        });
        const writable = await handle.createWritable();
        await writable.write(serialized.content);
        await writable.close();
        return { path: handle.name || suggestedName };
      }
      const path = promptPath(`Save ${bundleType} to file`, suggestedName);
      if (!path)
        return null;
      return await fetchJson(fallbackApi, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ...payload, path }),
      });
    }

    function selectExperiment(id) {
      stateStore.selectedExperimentId = id;
      stateStore.boardViewMode = false;
      stateStore.geomViewMode = false;
      stateStore.selectedPlaceholderId = null;
      const experiment = currentExperiment();
      if (experiment && experiment.groups.length > 0) {
        if (!experiment.groups.some(group => group.id === stateStore.selectedGroupId)) {
          stateStore.selectedGroupId = experiment.groups[0].id;
        }
      } else {
        stateStore.selectedGroupId = null;
      }
      stateStore.selectedNodeId = null;
      render();
    }

    function selectGroup(id) {
      stateStore.boardViewMode = false;
      stateStore.geomViewMode = false;
      stateStore.selectedPlaceholderId = null;
      stateStore.selectedGroupId = id;
      stateStore.selectedNodeId = null;
      render();
    }

    function selectAllGroups() {
      stateStore.boardViewMode = false;
      stateStore.geomViewMode = false;
      stateStore.selectedPlaceholderId = null;
      stateStore.selectedGroupId = "__all__";
      stateStore.selectedNodeId = null;
      render();
    }

    function selectNoGroup() {
      stateStore.boardViewMode = false;
      stateStore.geomViewMode = false;
      stateStore.selectedPlaceholderId = null;
      stateStore.selectedGroupId = "__nogroup__";
      stateStore.selectedNodeId = null;
      render();
    }

    function selectHomelessView() {
      stateStore.boardViewMode = false;
      stateStore.geomViewMode = false;
      stateStore.selectedPlaceholderId = null;
      stateStore.selectedGroupId = "__homeless__";
      stateStore.selectedNodeId = null;
      render();
    }

    function selectPlaceholderView(placeholderId) {
      const placeholder = experimentPlaceholders().find(node => node.id === placeholderId);
      if (!placeholder)
        return;
      stateStore.boardViewMode = false;
      stateStore.geomViewMode = true;
      stateStore.selectedGroupId = "__placeholder__";
      stateStore.selectedPlaceholderId = placeholderId;
      stateStore.selectedNodeId = placeholderId;
      arrangeGeometryByPhi(false);
      render();
    }

    function selectBoardView() {
      stateStore.boardViewMode = true;
      stateStore.geomViewMode = false;
      stateStore.selectedPlaceholderId = null;
      stateStore.selectedNodeId = null;
      render();
    }

    function toggleBoardView() {
      if (stateStore.boardViewMode) {
        stateStore.boardViewMode = false;
        if (!stateStore.selectedGroupId) {
          const firstGroup = currentExperiment()?.groups?.[0];
          stateStore.selectedGroupId = firstGroup?.id || null;
        }
      } else {
        selectBoardView();
        return;
      }
      render();
    }

    function toggleGeomView() {
      stateStore.geomViewMode = !stateStore.geomViewMode;
      if (stateStore.geomViewMode) {
        stateStore.boardViewMode = false;
        stateStore.showAllZaps = false;
        stateStore.showAllCobos = false;
        arrangeGeometryByPhi(false);
      } else {
        stateStore.selectedPlaceholderId = null;
      }
      stateStore.selectedNodeId = null;
      render();
    }

    function commitTabRename(type, id, value) {
      const nextValue = (value || "").trim();
      if (type === "experiment") {
        const experiment = stateStore.state.experiments.find(item => item.id === id);
        if (!experiment) return;
        experiment.name = nextValue || "Experiment";
      } else {
        const group = currentExperiment()?.groups.find(item => item.id === id);
        if (!group) return;
        group.name = nextValue || "Group";
      }
      render(false);
    }

    function makeEditableTab(label, isActive, onSelect, onRename, id, type) {
      const button = document.createElement("button");
      button.className = `tab ${isActive ? "active" : ""}`;
      if (type === "group" && stateStore.errorGroupIds.includes(id))
        button.classList.add("error");
      button.textContent = label;
      button.onclick = () => {
        clearTimeout(button._clickTimer);
        button._clickTimer = setTimeout(() => onSelect(), 180);
      };
      button.ondblclick = event => {
        event.preventDefault();
        event.stopPropagation();
        clearTimeout(button._clickTimer);
        const buttonWidth = button.getBoundingClientRect().width;
        const buttonHeight = button.getBoundingClientRect().height;
        const input = document.createElement("input");
        input.className = "tab-edit-input";
        input.value = label;
        input.style.width = `${Math.ceil(buttonWidth)}px`;
        input.style.height = `${Math.ceil(buttonHeight)}px`;
        input.onkeydown = keyEvent => {
          if (keyEvent.key === "Enter")
            onRename(input.value);
          else if (keyEvent.key === "Escape")
            render(false);
        };
        input.onblur = () => onRename(input.value);
        button.replaceWith(input);
        input.focus();
        input.select();
      };
      return button;
    }

    function addExperiment() {
      const experiment = {
        id: uid("exp"),
        name: `Experiment ${stateStore.state.experiments.length + 1}`,
        groups: [{ id: uid("group"), name: "Group 1", nodes: [], connections: [], group_preset: "Free" }],
        placeholders: [],
        loose_nodes: [],
      };
      stateStore.state.experiments.push(experiment);
      selectExperiment(experiment.id);
      showBanner(`Added ${experiment.name}`);
    }

    function deleteExperiment(experimentId) {
      const index = stateStore.state.experiments.findIndex(item => item.id === experimentId);
      if (index < 0)
        return;
      const deletingSelected = stateStore.selectedExperimentId === experimentId;
      stateStore.state.experiments.splice(index, 1);
      if (stateStore.state.experiments.length === 0) {
        const fallback = {
          id: uid("exp"),
          name: "Experiment 1",
          groups: [{ id: uid("group"), name: "Group 1", nodes: [], connections: [], group_preset: "R12-I" }],
          placeholders: [],
          loose_nodes: [],
        };
        stateStore.state.experiments.push(fallback);
      }
      if (deletingSelected) {
        const nextExperiment = stateStore.state.experiments[Math.max(0, index - 1)] || stateStore.state.experiments[0];
        stateStore.selectedExperimentId = nextExperiment?.id || null;
        stateStore.selectedGroupId = nextExperiment?.groups?.[0]?.id || null;
        stateStore.selectedNodeId = null;
      }
      render();
    }

    function groupPresetEntry(presetName) {
      const normalized = normalizeGroupPresetName(presetName);
      return stateStore.groupDb.groups.find(item => item.name === normalized) || null;
    }

    function groupCapacity(group) {
      const entry = groupPresetEntry(group?.group_preset || group?.name || "Free");
      return Math.max(0, Number(entry?.capacity || 0));
    }

    function detectorPhiDegrees(phiNumber, capacity) {
      if (!capacity || capacity <= 0)
        return 0;
      const pitch = 360 / capacity;
      return phiNumber * pitch + pitch / 2;
    }

    function syncDetectorPhiByIndex(group) {
      if (!group) return;
      const capacity = groupCapacity(group);
      const detectors = group.nodes.filter(item => item.kind === "detector");
      detectors.forEach((node, index) => {
        node.data.ring_type = normalizeGroupPresetName(group.group_preset || node.data.ring_type || "Free");
        if (capacity > 0) {
          node.data.phi_number = index % capacity;
          node.data.phi_center_deg = detectorPhiDegrees(node.data.phi_number, capacity);
        }
      });
    }

    function ensureGroupPreset(group) {
      if (!group) return;
      if (!group.group_preset)
        group.group_preset = "Free";
      group.group_preset = normalizeGroupPresetName(group.group_preset);
      if (!Array.isArray(group.nodes))
        group.nodes = [];
      if (!Array.isArray(group.connections))
        group.connections = [];
    }

    function configureCurrentGroupFromPreset() {
      if (stateStore.boardViewMode)
        return;
      const group = currentGroup();
      if (!group) return;
      ensureGroupPreset(group);
      const preset = groupPresetEntry(group.group_preset);
      if (!preset) {
        showBanner(`Group DB preset ${group.group_preset} does not exist.`, "error");
        return;
      }
      syncDetectorPhiByIndex(group);
      render(false);
      showBanner(`${group.name} configured as ${preset.name} (${Math.max(0, Number(preset.capacity || 0))})`);
    }

    function addGroup() {
      if (stateStore.boardViewMode)
        stateStore.boardViewMode = false;
      const experiment = currentExperiment();
      if (!experiment) return;
      const group = { id: uid("group"), name: `Group ${experiment.groups.length + 1}`, nodes: [], connections: [], group_preset: "Free" };
      experiment.groups.push(group);
      stateStore.focusGroupNameInput = true;
      selectGroup(group.id);
      showBanner(`Added ${group.name}`);
    }

    function deleteGroup(groupId) {
      const experiment = currentExperiment();
      if (!experiment) return;
      const group = experiment.groups.find(item => item.id === groupId);
      const nodeIds = new Set((group?.nodes || []).map(node => node.id));
      const groupIndex = experiment.groups.findIndex(item => item.id === groupId);
      if (groupIndex < 0) return;
      ensureExperimentBoardConnections(experiment);
      experiment.board_connections = experiment.board_connections.filter(connection => !nodeIds.has(connection.from.node_id) && !nodeIds.has(connection.to.node_id));
      const deletingSelected = stateStore.selectedGroupId === groupId;
      experiment.groups.splice(groupIndex, 1);
      if (experiment.groups.length === 0) {
        stateStore.selectedGroupId = null;
        stateStore.selectedNodeId = null;
      } else if (deletingSelected) {
        const nextGroup = experiment.groups[Math.max(0, groupIndex - 1)] || experiment.groups[0];
        stateStore.selectedGroupId = nextGroup?.id || null;
        stateStore.selectedNodeId = null;
      }
      render();
    }

    function clearGroup() {
      if (stateStore.boardViewMode)
        return;
      const group = currentGroup();
      if (!group)
        return;
      if (group.all_view || stateStore.selectedPlaceholderId) {
        showBanner("Select a user group to clear items.", "error");
        return;
      }
      const experiment = currentExperiment();
      const nodeIds = new Set((group.nodes || []).map(node => node.id));
      group.nodes = [];
      group.connections = [];
      if (experiment) {
        ensureExperimentBoardConnections(experiment);
        experiment.board_connections = experiment.board_connections.filter(connection => !nodeIds.has(connection.from.node_id) && !nodeIds.has(connection.to.node_id));
      }
      stateStore.selectedNodeId = null;
      stateStore.pendingConnection = null;
      stateStore.dragConnection = null;
      render();
      showBanner(`${group.name} cleared`);
    }

    function defaultNodeData(kind, subtype="X6") {
      if (kind === "detector") {
        return {
          detector_type: subtype,
          det_number: 0,
          detector_loaded: false,
          ring_number: 1,
          ring_type: "Free",
          dee: "E",
          phi_number: 0,
          phi_center_deg: 0,
          phi_half_opening_deg: 10,
          det_r_mm: 0,
          det_z_mm: 0,
        };
      }
      if (kind === "zap")
        return { board_type: subtype, board_number: 0, asad_number: 0 };
      if (kind === "merging")
        return { board_type: subtype, board_number: 0 };
      return { board_number: 0 };
    }

    function addDetector(detectorType) {
      if (stateStore.boardViewMode) {
        showBanner("Board View does not add detectors.", "error");
        return;
      }
      addNode("detector", detectorType);
    }

    function addPlaceholder(kind, data) {
      const experiment = currentExperiment();
      if (!experiment) {
        showBanner("Add a group first.", "error");
        return;
      }
      ensureExperimentCollections(experiment);
      const count = experimentPlaceholders(experiment).filter(node => node.kind === "placeholder" && node.data?.placeholder_type === kind).length;
      const geometry = placeholderGeometryDefaults(kind, data);
      const node = {
        id: uid("placeholder"),
        kind: "placeholder",
        label: `${kind} ${count}`,
        x: 260 + count * 24,
        y: 80 + count * 24,
        z_order: 0,
        data: {
          placeholder_type: kind,
          dl: GEOM_DL,
          ...geometry,
          ...data,
        },
      };
      experiment.placeholders.push(node);
      stateStore.selectedNodeId = node.id;
      stateStore.selectedPlaceholderId = node.id;
      stateStore.geomViewMode = true;
      stateStore.boardViewMode = false;
      stateStore.selectedGroupId = "__placeholder__";
      render();
    }

    function addRingPlaceholder() {
      openPlaceholderCreateModal("Ring");
    }

    function addCircularPlaceholder() {
      openPlaceholderCreateModal("Circular");
    }

    function addRectangularPlaceholder() {
      openPlaceholderCreateModal("Rectangular");
    }

    function openPlaceholderCreateModal(kind="Ring") {
      stateStore.pendingPlaceholderKind = kind || "Ring";
      const title = document.getElementById("placeholderCreateTitle");
      const fields = document.getElementById("placeholderCreateFields");
      if (title)
        title.textContent = "Create Placeholder";
      fields.innerHTML = `
        <div class="placeholder-create-row">
          <label>Ring</label>
          <input id="placeholderRingSlotsInput" data-placeholder-kind="Ring" type="number" min="3" value="12">
          <button onclick="confirmPlaceholderCreate('Ring')" class="primary">Create</button>
        </div>
        <div class="placeholder-create-row">
          <label>Circular</label>
          <input id="placeholderCircularSlotsInput" data-placeholder-kind="Circular" type="number" min="3" value="12">
          <button onclick="confirmPlaceholderCreate('Circular')" class="primary">Create</button>
        </div>
        <div class="placeholder-create-row rectangular">
          <label>Rectangular</label>
          <input id="placeholderRectangularNxInput" data-placeholder-kind="Rectangular" type="number" min="1" value="4">
          <input id="placeholderRectangularNyInput" data-placeholder-kind="Rectangular" type="number" min="1" value="3">
          <button onclick="confirmPlaceholderCreate('Rectangular')" class="primary">Create</button>
        </div>`;
      document.getElementById("placeholderCreateModal").classList.add("open");
      queueMicrotask(() => {
        const inputs = Array.from(fields.querySelectorAll("input"));
        inputs.forEach(input => {
          input.onkeydown = event => {
            if (event.key === "Enter") {
              event.preventDefault();
              event.stopPropagation();
              confirmPlaceholderCreate(input.dataset.placeholderKind || stateStore.pendingPlaceholderKind || "Ring");
            } else if (event.key === "Tab") {
              event.preventDefault();
              event.stopPropagation();
              const index = inputs.indexOf(input);
              const nextIndex = event.shiftKey
                ? (index - 1 + inputs.length) % inputs.length
                : (index + 1) % inputs.length;
              inputs[nextIndex]?.focus();
              inputs[nextIndex]?.select();
            }
          };
        });
        const focusId = stateStore.pendingPlaceholderKind === "Circular"
          ? "placeholderCircularSlotsInput"
          : stateStore.pendingPlaceholderKind === "Rectangular"
            ? "placeholderRectangularNxInput"
            : "placeholderRingSlotsInput";
        (document.getElementById(focusId) || fields.querySelector("input"))?.focus();
      });
    }

    function closePlaceholderCreateModal() {
      stateStore.pendingPlaceholderAttachSelection = false;
      closeModal("placeholderCreateModal");
    }

    function confirmPlaceholderCreate(kind=null) {
      kind = kind || stateStore.pendingPlaceholderKind || "Ring";
      const attachSelection = !!stateStore.pendingPlaceholderAttachSelection;
      stateStore.pendingPlaceholderAttachSelection = false;
      if (kind === "Rectangular") {
        const nx = toInt(document.getElementById("placeholderRectangularNxInput")?.value, 0);
        const ny = toInt(document.getElementById("placeholderRectangularNyInput")?.value, 0);
        if (nx < 1 || ny < 1) {
          showBanner("Rectangular requires Nx >= 1 and Ny >= 1.", "error");
          return;
        }
        closePlaceholderCreateModal();
        addPlaceholder("Rectangular", { nx, ny, slots: nx * ny });
        if (attachSelection)
          attachSelectedHomelessDetectorsToLatestPlaceholder();
        return;
      }
      const n = toInt(document.getElementById(kind === "Circular" ? "placeholderCircularSlotsInput" : "placeholderRingSlotsInput")?.value, 0);
      if (n < 3) {
        showBanner(`${kind} requires N >= 3.`, "error");
        return;
      }
      closePlaceholderCreateModal();
      addPlaceholder(kind, { slots: n });
      if (attachSelection)
        attachSelectedHomelessDetectorsToLatestPlaceholder();
    }

    function nextBoardNumber(kind, subtype="") {
      const experiment = currentExperiment();
      if (!experiment)
        return 0;
      let maxNumber = -1;
      for (const group of experiment.groups || []) {
        for (const node of group.nodes || []) {
          if (node.kind !== kind)
            continue;
          if (kind !== "cobo" && subtype && nodeBoardType(node) !== subtype)
            continue;
          maxNumber = Math.max(maxNumber, toInt(node.data?.board_number, -1));
        }
      }
      return maxNumber + 1;
    }

    function nextDetectorLabelIndex() {
      const experiment = currentExperiment();
      if (!experiment)
        return 0;
      let maxIndex = -1;
      const nodeSets = [
        ...(experiment.groups || []).map(group => group.nodes || []),
        ...(experiment.loose_nodes ? [experiment.loose_nodes] : []),
      ];
      for (const nodes of nodeSets) {
        for (const node of nodes || []) {
          if (node.kind !== "detector")
            continue;
          const match = String(node.label || "").match(/^Detector\s+(\d+)/);
          if (match)
            maxIndex = Math.max(maxIndex, toInt(match[1], -1));
        }
      }
      return maxIndex + 1;
    }

    function autoNodeLabel(node) {
      if (!node)
        return "";
      if (node.kind === "detector") {
        const match = String(node.label || "").match(/^Detector\s+(\d+)/);
        const index = node.data?.det_index ?? (match ? toInt(match[1], 0) : nextDetectorLabelIndex());
        if (!node.data)
          node.data = {};
        node.data.det_index = index;
        return `Detector ${index}`;
      }
      if (node.kind === "merging")
        return `${boardConfig(nodeBoardType(node))?.merging_label || `${nodeBoardType(node)} Merging`} ${node.data?.board_number ?? 0}`;
      if (node.kind === "zap")
        return `${boardConfig(nodeBoardType(node))?.zap_label || `${nodeBoardType(node)} ZAP/AsAd`} ${node.data?.board_number ?? 0}`;
      if (node.kind === "cobo")
        return `COBO ${node.data?.board_number ?? 0}`;
      return node.label || "Item";
    }

    function syncNodeLabel(node) {
      if (!node)
        return;
      if (node.kind === "detector" || node.kind === "merging" || node.kind === "zap" || node.kind === "cobo")
        node.label = autoNodeLabel(node);
    }

    function syncExperimentNodeLabels(experiment) {
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      for (const node of experiment.loose_nodes || [])
        syncNodeLabel(node);
      for (const node of experiment.placeholders || [])
        syncNodeLabel(node);
      for (const group of experiment.groups || [])
        for (const node of group.nodes || [])
          syncNodeLabel(node);
    }

    function addNode(kind, subtype="X6", doRender=true) {
      if (stateStore.boardViewMode && kind === "detector") {
        showBanner("Board View does not add detectors.", "error");
        return;
      }
      const group = stateStore.boardViewMode || stateStore.selectedGroupId === "__all__" || stateStore.selectedPlaceholderId ? selectedDataGroup() : currentGroup();
      if (!group) {
        showBanner("Add a group first.", "error");
        return;
      }
      if (group.no_group && kind !== "detector") {
        showBanner("No group view can only add detectors.", "error");
        return;
      }
      ensureGroupPreset(group);
      const experiment = currentExperiment();
      ensureExperimentCollections(experiment);
      const targetNodes = group.no_group ? experiment.loose_nodes : group.nodes;
      const selectedPlaceholder = stateStore.selectedPlaceholderId
        ? experimentPlaceholders(experiment).find(item => item.id === stateStore.selectedPlaceholderId && item.kind === "placeholder")
        : null;
      const index = kind === "detector"
        ? nextDetectorLabelIndex(subtype)
        : group.nodes.filter(item => item.kind === kind).length;
      const config = boardConfig(subtype);
      const labels = {
        detector: "Detector",
        merging: config?.merging_label || `${subtype} Merging`,
        zap: config?.zap_label || `${subtype} ZAP/AsAd`,
        cobo: "COBO",
        placeholder: "Placeholder",
      };
      const boardNumber = kind === "detector" ? null : nextBoardNumber(kind, subtype);
      const detectorCount = targetNodes.filter(item => item.kind === "detector").length;
      const capacity = selectedPlaceholder
        ? placeholderSlotPositions(selectedPlaceholder).length
        : groupCapacity(group);
      const attachedCount = selectedPlaceholder
        ? experimentDetectorNodes(experiment).filter(item => item.kind === "detector" && item.data?.placeholder_id === selectedPlaceholder.id).length
        : detectorCount;
      if (kind === "detector" && capacity > 0 && attachedCount >= capacity) {
        showBanner(`Group capacity is ${capacity}. More detectors cannot be added.`, "error");
        return;
      }
      const boardCount = targetNodes.filter(item => item.kind !== "detector").length;
      const detectorColumn = detectorCount % 4;
      const detectorRow = detectorCount;
      const position = kind === "detector"
        ? { x: 24 + detectorColumn * 21, y: 56 + detectorRow * 42 }
        : { x: 360 + (boardCount % 4) * 21, y: 56 + boardCount * 42 };
      const node = {
        id: uid(kind),
        kind,
        label: kind === "detector" ? `${labels[kind]} ${index}` : `${labels[kind]} ${boardNumber}`,
        x: position.x,
        y: position.y,
        z_order: nextZOrder(group),
        data: {
          ...defaultNodeData(kind, subtype),
          ring_type: kind === "detector" ? normalizeGroupPresetName(group.group_preset || "Free") : undefined,
          det_index: kind === "detector" ? index : undefined,
          phi_number: kind === "detector" ? detectorCount : undefined,
          phi_center_deg: kind === "detector" ? detectorPhiDegrees(detectorCount, groupCapacity(group)) : undefined,
          board_number: kind === "detector" ? undefined : boardNumber,
        },
      };
      syncNodeLabel(node);
      const clamped = clampNodePosition(node.x, node.y);
      node.x = clamped.x;
      node.y = clamped.y;
      targetNodes.push(node);
      if (kind === "detector" && selectedPlaceholder) {
        const used = new Set(experimentDetectorNodes(experiment)
          .filter(item => item.kind === "detector" && item.id !== node.id && item.data?.placeholder_id === selectedPlaceholder.id)
          .map(item => toInt(item.data?.phi_number, -1)));
        const slots = placeholderSlotPositions(selectedPlaceholder);
        let slotIndex = 0;
        while (used.has(slotIndex) && slotIndex < slots.length)
          slotIndex++;
        placeDetectorOnPlaceholderSlot(node, selectedPlaceholder, slotIndex);
      }
      if (kind === "detector" && !selectedPlaceholder && !group.no_group)
        syncDetectorPhiByIndex(group);
      stateStore.selectedNodeId = node.id;
      if (doRender)
        render();
    }

    function deleteSelection() {
      const group = currentGroup();
      const node = currentSelection();
      if (!group || !node) return;
      deleteNodeById(node.id);
    }

    function deleteNodeById(nodeId) {
      const experiment = currentExperiment();
      if (!experiment) return;
      ensureExperimentCollections(experiment);
      experiment.groups.forEach(group => {
        group.nodes = (group.nodes || []).filter(item => item.id !== nodeId);
        group.connections = (group.connections || []).filter(connection => connection.from.node_id !== nodeId && connection.to.node_id !== nodeId);
      });
      experiment.loose_nodes = (experiment.loose_nodes || []).filter(item => item.id !== nodeId);
      experiment.placeholders = (experiment.placeholders || []).filter(item => item.id !== nodeId);
      ensureExperimentBoardConnections(experiment);
      experiment.board_connections = experiment.board_connections.filter(connection => connection.from.node_id !== nodeId && connection.to.node_id !== nodeId);
      if (stateStore.selectedNodeId === nodeId)
        stateStore.selectedNodeId = null;
      if (stateStore.selectedPlaceholderId === nodeId)
        stateStore.selectedPlaceholderId = null;
      render();
    }

    function deletePlaceholder(placeholderId) {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      experiment.placeholders = (experiment.placeholders || []).filter(node => node.id !== placeholderId);
      for (const group of experiment.groups || [])
        group.nodes = (group.nodes || []).filter(node => node.id !== placeholderId);
      experimentDetectorNodes(experiment).forEach(detector => {
        if (detector.data?.placeholder_id === placeholderId) {
          delete detector.data.placeholder_id;
          delete detector.data.placeholder_type;
          delete detector.data.geom_rotation_deg;
        }
      });
      if (stateStore.selectedPlaceholderId === placeholderId) {
        stateStore.selectedPlaceholderId = null;
        stateStore.selectedGroupId = "__all__";
      }
      if (stateStore.selectedNodeId === placeholderId)
        stateStore.selectedNodeId = null;
      render();
    }

    function selectedHomelessDetectors() {
      const experiment = currentExperiment();
      if (!experiment)
        return [];
      ensureExperimentCollections(experiment);
      return experimentDetectorNodes(experiment).filter(node => node.kind === "detector" && !node.data?.placeholder_id && node.data?.new_group_selected);
    }

    function selectedNoGroupDetectors() {
      const experiment = currentExperiment();
      if (!experiment)
        return [];
      ensureExperimentCollections(experiment);
      return (experiment.loose_nodes || []).filter(node => node.kind === "detector" && !node.data?.placeholder_id && node.data?.new_group_selected);
    }

    function toggleNoGroupDetectorSelection(nodeId) {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      const node = (experiment.loose_nodes || []).find(item => item.id === nodeId && item.kind === "detector");
      if (!node)
        return;
      if (node.data?.placeholder_id)
        return;
      if (!node.data)
        node.data = {};
      node.data.new_group_selected = !node.data.new_group_selected;
      render();
    }

    function toggleHomelessDetectorSelection(nodeId) {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      const node = experimentDetectorNodes(experiment).find(item => item.id === nodeId && item.kind === "detector");
      if (!node)
        return;
      if (node.data?.placeholder_id)
        return;
      if (!node.data)
        node.data = {};
      node.data.new_group_selected = !node.data.new_group_selected;
      render();
    }

    function attachSelectedHomelessDetectorsToPlaceholder(placeholder) {
      const experiment = currentExperiment();
      if (!experiment || !placeholder)
        return;
      ensureExperimentCollections(experiment);
      const selected = selectedHomelessDetectors();
      if (selected.length === 0)
        return;
      const slots = placeholderSlotPositions(placeholder);
      selected.forEach((node, index) => {
        if (!node.data)
          node.data = {};
        node.data.placeholder_id = placeholder.id;
        node.data.placeholder_type = placeholder.data?.placeholder_type || "";
        node.data.new_group_selected = false;
        placeDetectorOnPlaceholderSlot(node, placeholder, index % Math.max(1, slots.length));
      });
    }

    function attachSelectedHomelessDetectorsToLatestPlaceholder() {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      const placeholder = [...(experiment.placeholders || [])].slice(-1)[0];
      if (!placeholder)
        return;
      attachSelectedHomelessDetectorsToPlaceholder(placeholder);
      render();
    }

    function createPlaceholderFromHomelessSelection() {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      const selected = selectedHomelessDetectors();
      if (selected.length === 0) {
        showBanner("No detectors are connected to Placeholder manager.", "error");
        return;
      }
      stateStore.pendingPlaceholderAttachSelection = true;
      openPlaceholderCreateModal("Ring");
      return;
    }

    function clearHomelessSelection() {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      experimentDetectorNodes(experiment).forEach(node => {
        if (node.data)
          delete node.data.new_group_selected;
      });
      stateStore.pendingConnection = null;
      stateStore.dragConnection = null;
      render();
    }

    function connectAllHomelessDetectors() {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      experimentDetectorNodes(experiment).forEach(node => {
        if (node.kind !== "detector")
          return;
        if (!node.data)
          node.data = {};
        node.data.new_group_selected = true;
      });
      stateStore.pendingConnection = null;
      stateStore.dragConnection = null;
      render();
    }

    function openPlaceholderSelectModal() {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      const selected = selectedHomelessDetectors();
      if (selected.length === 0) {
        showBanner("No detectors are connected to Placeholder manager.", "error");
        return;
      }
      const list = document.getElementById("placeholderSelectList");
      list.innerHTML = experimentPlaceholders(experiment).map(placeholder => `
        <div class="navigator-entry" data-placeholder-id="${escapeHtml(placeholder.id)}">
          <span class="name">${escapeHtml(placeholder.label)}</span>
          <span>${(experimentDetectorNodes(experiment).filter(node => node.data?.placeholder_id === placeholder.id)).length} det</span>
        </div>
      `).join("") || '<div class="help">No placeholders yet</div>';
      list.querySelectorAll(".navigator-entry").forEach(entry => {
        entry.addEventListener("click", () => moveSelectedHomelessDetectorsToPlaceholder(entry.dataset.placeholderId));
      });
      document.getElementById("placeholderSelectModal").classList.add("open");
    }

    function moveSelectedHomelessDetectorsToPlaceholder(placeholderId) {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      const placeholder = experimentPlaceholders(experiment).find(item => item.id === placeholderId);
      if (!placeholder)
        return;
      attachSelectedHomelessDetectorsToPlaceholder(placeholder);
      closeModal("placeholderSelectModal");
      stateStore.selectedPlaceholderId = placeholder.id;
      stateStore.selectedGroupId = "__placeholder__";
      stateStore.selectedNodeId = placeholder.id;
      stateStore.geomViewMode = true;
      stateStore.boardViewMode = false;
      render();
      showBanner(`Moved detectors to ${placeholder.label}`);
    }

    function deleteSelectedDetector() {
      const node = currentSelection();
      if (!node || node.kind !== "detector")
        return;
      deleteNodeById(node.id);
    }

    function portCompatible(start, target) {
      if (!start || !target) return false;
      if (start.nodeId === target.nodeId) return false;
      const startNode = currentNodeById(start.nodeId);
      const targetNode = currentNodeById(target.nodeId);
      if (isPortDisabled(startNode, start.portId) || isPortDisabled(targetNode, target.portId))
        return false;
      const startType = nodeBoardType(startNode);
      const targetType = nodeBoardType(targetNode);
      if (start.kind === "detector" && target.kind === "new_group")
        return target.portId === "IN";
      if (start.kind === "detector" && target.kind === "merging") {
        if (startType !== targetType)
          return false;
        return nodePorts(targetNode).some(item => item.direction === "input" && item.id === target.portId);
      }
      if (start.kind === "merging" && target.kind === "zap") {
        if (!zapCompatibleTypes(startType).includes(targetType))
          return false;
        return nodePorts(targetNode).some(item => item.direction === "input" && item.id === target.portId);
      }
      if (start.kind === "zap" && target.kind === "cobo")
        return nodePorts(targetNode).some(item => item.direction === "input" && item.id === target.portId);
      return false;
    }

    function nodeByIdFromExperiment(experiment, nodeId) {
      if (nodeId === "__new_group__")
        return currentVisibleNodes().find(node => node.id === "__new_group__") || null;
      ensureExperimentCollections(experiment);
      for (const node of experiment?.placeholders || [])
        if (node.id === nodeId)
          return node;
      for (const node of experiment?.loose_nodes || [])
        if (node.id === nodeId)
          return node;
      for (const group of experiment?.groups || []) {
        const node = group.nodes?.find(item => item.id === nodeId);
        if (node)
          return node;
      }
      return null;
    }

    function boardConnectionStore() {
      const experiment = currentExperiment();
      if (!experiment)
        return [];
      ensureExperimentBoardConnections(experiment);
      return experiment.board_connections;
    }

    function ownerGroupOfNode(nodeId) {
      const experiment = currentExperiment();
      if (!experiment)
        return null;
      for (const group of experiment.groups || []) {
        if ((group.nodes || []).some(node => node.id === nodeId))
          return group;
      }
      return null;
    }

    function coboInputNumber(portId) {
      const match = String(portId || "").match(/^IN(\d+)$/);
      return match ? toInt(match[1], 0) : null;
    }

    function canvasConnections(group=currentGroup()) {
      const experiment = currentExperiment();
      if (!group)
        return [];
      if (group.all_view)
        return group.connections || [];
      if (group.board_view)
        return group.connections || [];
      const nodeIds = new Set(currentVisibleNodes().map(node => node.id));
      const connections = [...(group.connections || [])];
      if (experiment) {
        ensureExperimentBoardConnections(experiment);
        experiment.board_connections.forEach(connection => {
          if (nodeIds.has(connection.from.node_id) || nodeIds.has(connection.to.node_id))
            connections.push(connection);
        });
      }
      return connections;
    }

    function setConnection(fromPort, toPort) {
      const group = currentGroup();
      const experiment = currentExperiment();
      if (!group || !experiment) return;
      const fromNode = nodeByIdFromExperiment(experiment, fromPort.nodeId);
      const toNode = nodeByIdFromExperiment(experiment, toPort.nodeId);
      if (fromNode?.kind === "detector" && toNode?.kind === "new_group") {
        if (!fromNode.data)
          fromNode.data = {};
        fromNode.data.new_group_selected = true;
        stateStore.pendingConnection = null;
        stateStore.dragConnection = null;
        render();
        return;
      }
      const targetStore = (fromNode?.kind !== "detector" && toNode?.kind !== "detector") ? boardConnectionStore() : group.connections;
      for (let index = targetStore.length - 1; index >= 0; --index) {
        const connection = targetStore[index];
        const sameSource = connection.from.node_id === fromPort.nodeId && connection.from.port === fromPort.portId;
        const sameTarget = connection.to.node_id === toPort.nodeId && connection.to.port === toPort.portId;
        if (sameSource || sameTarget)
          targetStore.splice(index, 1);
      }
      targetStore.push({
        id: uid("wire"),
        from: { node_id: fromPort.nodeId, port: fromPort.portId },
        to: { node_id: toPort.nodeId, port: toPort.portId },
      });
      if (fromNode?.kind === "zap" && toNode?.kind === "cobo") {
        const inputNumber = coboInputNumber(toPort.portId);
        if (inputNumber !== null) {
          if (!fromNode.data)
            fromNode.data = {};
          fromNode.data.asad_number = inputNumber;
        }
      }
      clearExportErrors();
      stateStore.pendingConnection = null;
      stateStore.dragConnection = null;
      render();
    }

    function setConnectionNoRender(fromNode, fromPortId, toNode, toPortId) {
      const experiment = currentExperiment();
      if (!experiment || !fromNode || !toNode)
        return false;
      const targetGroup = (fromNode.kind !== "detector" && toNode.kind !== "detector")
        ? null
        : (ownerGroupOfNode(fromNode.id) || ownerGroupOfNode(toNode.id));
      if (targetGroup && !Array.isArray(targetGroup.connections))
        targetGroup.connections = [];
      const targetStore = (fromNode.kind !== "detector" && toNode.kind !== "detector")
        ? boardConnectionStore()
        : targetGroup?.connections;
      if (!targetStore)
        return false;
      for (let index = targetStore.length - 1; index >= 0; --index) {
        const connection = targetStore[index];
        const sameSource = connection.from.node_id === fromNode.id && connection.from.port === fromPortId;
        const sameTarget = connection.to.node_id === toNode.id && connection.to.port === toPortId;
        if (sameSource || sameTarget)
          targetStore.splice(index, 1);
      }
      targetStore.push({
        id: uid("wire"),
        from: { node_id: fromNode.id, port: fromPortId },
        to: { node_id: toNode.id, port: toPortId },
      });
      if (fromNode.kind === "zap" && toNode.kind === "cobo") {
        const inputNumber = coboInputNumber(toPortId);
        if (inputNumber !== null) {
          if (!fromNode.data)
            fromNode.data = {};
          fromNode.data.asad_number = inputNumber;
        }
      }
      return true;
    }

    function removeConnection(connectionId) {
      const group = currentGroup();
      const experiment = currentExperiment();
      if (!group || !experiment) return;
      group.connections = group.connections.filter(item => item.id !== connectionId);
      ensureExperimentBoardConnections(experiment);
      experiment.board_connections = experiment.board_connections.filter(item => item.id !== connectionId);
      stateStore.pendingConnection = null;
      render();
    }

    function isPortDisabled(node, portId) {
      return !!node?.data?.disabled_ports?.includes(portId);
    }

    function togglePortDisabled(nodeId, portId) {
      const node = currentGroup()?.nodes.find(item => item.id === nodeId);
      if (!node) return;
      if (node.kind === "new_group") {
        const group = currentGroup();
        if (group?.homeless_view)
          clearHomelessSelection();
        else
          clearNoGroupSelection();
        return;
      }
      if (node.kind === "detector" && portId === "OUT" && node.data?.new_group_selected) {
        delete node.data.new_group_selected;
        stateStore.pendingConnection = null;
        stateStore.dragConnection = null;
        render();
        return;
      }
      if (!Array.isArray(node.data.disabled_ports))
        node.data.disabled_ports = [];
      const index = node.data.disabled_ports.indexOf(portId);
      if (index >= 0)
        node.data.disabled_ports.splice(index, 1);
      else
        node.data.disabled_ports.push(portId);

      const group = currentGroup();
      group.connections = group.connections.filter(connection => {
        const fromMatch = connection.from.node_id === nodeId && connection.from.port === portId;
        const toMatch = connection.to.node_id === nodeId && connection.to.port === portId;
        return !(fromMatch || toMatch);
      });
      const experiment = currentExperiment();
      if (experiment) {
        ensureExperimentBoardConnections(experiment);
        experiment.board_connections = experiment.board_connections.filter(connection => {
          const fromMatch = connection.from.node_id === nodeId && connection.from.port === portId;
          const toMatch = connection.to.node_id === nodeId && connection.to.port === portId;
          return !(fromMatch || toMatch);
        });
      }
      render();
    }

    function clearNoGroupSelection() {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      (experiment.loose_nodes || []).forEach(node => {
        if (node.data)
          delete node.data.new_group_selected;
      });
      stateStore.pendingConnection = null;
      stateStore.dragConnection = null;
      render();
    }

    function connectAllNoGroupDetectors() {
      const experiment = currentExperiment();
      if (!experiment)
        return;
      ensureExperimentCollections(experiment);
      (experiment.loose_nodes || []).forEach(node => {
        if (node.kind !== "detector")
          return;
        if (!node.data)
          node.data = {};
        node.data.new_group_selected = true;
      });
      stateStore.pendingConnection = null;
      stateStore.dragConnection = null;
      render();
    }

    function firstFreeMergingSlot(group, mergingNodeId) {
      const mergingNode = group.nodes.find(item => item.id === mergingNodeId);
      const slots = nodePorts(mergingNode).filter(item => item.direction === "input").map(item => item.id);
      const connections = canvasConnections(group);
      for (const slot of slots) {
        if (isPortDisabled(mergingNode, slot))
          continue;
        const used = connections.some(connection => connection.to.node_id === mergingNodeId && connection.to.port === slot);
        if (!used) return slot;
      }
      return null;
    }

    function firstFreeZapPin(group, zapNodeId) {
      const zapNode = group.nodes.find(item => item.id === zapNodeId);
      const pins = nodePorts(zapNode).filter(item => item.direction === "input").map(item => item.id);
      const connections = canvasConnections(group);
      for (const pin of pins) {
        if (isPortDisabled(zapNode, pin))
          continue;
        const used = connections.some(connection => connection.to.node_id === zapNodeId && connection.to.port === pin);
        if (!used) return pin;
      }
      return null;
    }

    function firstFreeCoboInput(group, coboNodeId) {
      const experiment = currentExperiment();
      const coboNode = currentNodeById(coboNodeId);
      const pins = nodePorts(coboNode).filter(item => item.direction === "input").map(item => item.id);
      const connections = experiment ? boardConnectionStore() : canvasConnections(group);
      for (const pin of pins) {
        if (isPortDisabled(coboNode, pin))
          continue;
        const used = connections.some(connection => connection.to.node_id === coboNodeId && connection.to.port === pin);
        if (!used)
          return pin;
      }
      return null;
    }

    function portUsedInConnections(connections, nodeId, portId, direction) {
      return connections.some(connection => {
        if (direction === "output")
          return connection.from.node_id === nodeId && connection.from.port === portId;
        return connection.to.node_id === nodeId && connection.to.port === portId;
      });
    }

    function firstFreeInputPortFromConnections(node, connections) {
      for (const port of nodePorts(node).filter(item => item.direction === "input")) {
        if (isPortDisabled(node, port.id))
          continue;
        if (!portUsedInConnections(connections, node.id, port.id, "input"))
          return port.id;
      }
      return null;
    }

    function allStoredConnections() {
      const experiment = currentExperiment();
      if (!experiment)
        return [];
      ensureExperimentBoardConnections(experiment);
      const connections = [];
      for (const group of experiment.groups || [])
        connections.push(...(group.connections || []));
      connections.push(...(experiment.board_connections || []));
      return connections;
    }

    function autoConnectVisibleGroup() {
      const group = currentGroup();
      const experiment = currentExperiment();
      if (!group || !experiment)
        return;
      ensureExperimentBoardConnections(experiment);
      const visibleNodes = currentVisibleNodes().filter(node => node.kind !== "placeholder" && node.kind !== "new_group");
      const detectors = sortDetectorsByLabelIndex(visibleNodes.filter(node => node.kind === "detector"));
      const mergings = sortBoardNodesByTypeAndNumber(visibleNodes.filter(node => node.kind === "merging"));
      const zaps = sortBoardNodesByTypeAndNumber(visibleNodes.filter(node => node.kind === "zap"));
      const cobos = [...visibleNodes.filter(node => node.kind === "cobo")]
        .sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));
      let connected = 0;

      for (const detector of detectors) {
        if (isPortDisabled(detector, "OUT"))
          continue;
        let connections = allStoredConnections();
        if (portUsedInConnections(connections, detector.id, "OUT", "output"))
          continue;
        for (const merging of mergings) {
          if (nodeBoardType(detector) !== nodeBoardType(merging))
            continue;
          const slot = firstFreeInputPortFromConnections(merging, connections);
          if (slot === null)
            continue;
          if (setConnectionNoRender(detector, "OUT", merging, slot))
            connected++;
          break;
        }
      }

      for (const merging of mergings) {
        if (isPortDisabled(merging, "OUT"))
          continue;
        let connections = allStoredConnections();
        if (portUsedInConnections(connections, merging.id, "OUT", "output"))
          continue;
        for (const zap of zaps) {
          if (!zapCompatibleTypes(nodeBoardType(merging)).includes(nodeBoardType(zap)))
            continue;
          const pin = firstFreeInputPortFromConnections(zap, connections);
          if (pin === null)
            continue;
          if (setConnectionNoRender(merging, "OUT", zap, pin))
            connected++;
          break;
        }
      }

      for (const zap of zaps) {
        if (isPortDisabled(zap, "OUT"))
          continue;
        let connections = allStoredConnections();
        if (portUsedInConnections(connections, zap.id, "OUT", "output"))
          continue;
        for (const cobo of cobos) {
          const input = firstFreeInputPortFromConnections(cobo, connections);
          if (input === null)
            continue;
          if (setConnectionNoRender(zap, "OUT", cobo, input))
            connected++;
          break;
        }
      }

      clearExportErrors();
      stateStore.pendingConnection = null;
      stateStore.dragConnection = null;
      if (connected > 0)
        showBanner(`Connected ${connected} visible links.`);
      else
        showBanner("No visible links were added.", "error");
      render();
    }

    function autoConnectDetectorToLatestMerging(nodeId) {
      const group = currentGroup();
      if (!group) return;
      const detector = group.nodes.find(item => item.id === nodeId && item.kind === "detector");
      if (!detector) return;

      const mergingBoards = group.nodes
        .filter(item => item.kind === "merging" && nodeBoardType(item) === nodeBoardType(detector))
        .sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));
      if (mergingBoards.length === 0) {
        showBanner(`No ${nodeBoardType(detector)} merging board exists yet.`, "error");
        return;
      }

      let selectedMerging = null;
      let slot = null;
      for (const merging of mergingBoards) {
        const freeSlot = firstFreeMergingSlot(group, merging.id);
        if (freeSlot !== null) {
          selectedMerging = merging;
          slot = freeSlot;
          break;
        }
      }

      if (selectedMerging === null || slot === null) {
        showBanner(`All ${nodeBoardType(detector)} Merging boards are full.`, "error");
        return;
      }

      setConnection(
        { nodeId: detector.id, kind: "detector", portId: "OUT" },
        { nodeId: selectedMerging.id, kind: "merging", portId: slot }
      );
      showBanner(`${detector.label} connected to ${selectedMerging.label} ${slot}`);
    }

    function autoConnectMergingToZap(nodeId) {
      const group = currentGroup();
      if (!group) return;
      const merging = group.nodes.find(item => item.id === nodeId && item.kind === "merging");
      if (!merging || isPortDisabled(merging, "OUT"))
        return;

      const zapBoards = group.nodes
        .filter(item => item.kind === "zap" && zapCompatibleTypes(nodeBoardType(merging)).includes(nodeBoardType(item)))
        .sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));
      if (zapBoards.length === 0) {
        showBanner(`No ${nodeBoardType(merging)} ZAP board exists yet.`, "error");
        return;
      }

      let selectedZap = null;
      let pin = null;
      for (const zap of zapBoards) {
        const freePin = firstFreeZapPin(group, zap.id);
        if (freePin !== null) {
          selectedZap = zap;
          pin = freePin;
          break;
        }
      }

      if (selectedZap === null || pin === null) {
        showBanner(`All ${nodeBoardType(merging)} ZAP boards are full.`, "error");
        return;
      }

      setConnection(
        { nodeId: merging.id, kind: "merging", portId: "OUT" },
        { nodeId: selectedZap.id, kind: "zap", portId: pin }
      );
      showBanner(`${merging.label} connected to ${selectedZap.label} ${pin}`);
    }

    function autoConnectZapToCobo(nodeId) {
      const group = currentGroup();
      const experiment = currentExperiment();
      if (!group || !experiment) return;
      const zap = currentNodeById(nodeId);
      if (!zap || zap.kind !== "zap" || isPortDisabled(zap, "OUT"))
        return;
      const cobos = [];
      for (const expGroup of experiment.groups || []) {
        for (const node of expGroup.nodes || []) {
          if (node.kind === "cobo")
            cobos.push(node);
        }
      }
      cobos.sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));
      if (cobos.length === 0) {
        showBanner("No COBO exists yet.", "error");
        return;
      }
      let selectedCobo = null;
      let inputId = null;
      for (const cobo of cobos) {
        const freeInput = firstFreeCoboInput(group, cobo.id);
        if (freeInput !== null) {
          selectedCobo = cobo;
          inputId = freeInput;
          break;
        }
      }
      if (!selectedCobo || inputId === null) {
        showBanner("All COBO inputs are full.", "error");
        return;
      }
      setConnection(
        { nodeId: zap.id, kind: "zap", portId: "OUT" },
        { nodeId: selectedCobo.id, kind: "cobo", portId: inputId }
      );
      showBanner(`${zap.label} connected to ${selectedCobo.label} ${inputId}`);
    }

    function autoConnectFirstMergingToZapPin(nodeId, pinId) {
      const group = currentGroup();
      if (!group) return;
      const zap = group.nodes.find(item => item.id === nodeId && item.kind === "zap");
      if (!zap || isPortDisabled(zap, pinId))
        return;

      const connections = canvasConnections(group);
      const pinConnected = connections.some(connection => connection.to.node_id === nodeId && connection.to.port === pinId);
      if (pinConnected)
        return;

      const mergingBoards = group.nodes
        .filter(item => item.kind === "merging" && !isPortDisabled(item, "OUT") && zapCompatibleTypes(nodeBoardType(zap)).includes(nodeBoardType(item)))
        .sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));

      for (const merging of mergingBoards) {
        const mergingConnected = connections.some(connection => connection.from.node_id === merging.id && connection.from.port === "OUT");
        if (!mergingConnected) {
          setConnection(
            { nodeId: merging.id, kind: "merging", portId: "OUT" },
            { nodeId: zap.id, kind: "zap", portId: pinId }
          );
          return;
        }
      }

      showBanner(`No free Merging board for ${zap.label} ${pinId}`, "error");
    }

    function autoConnectFirstZapToCoboInput(nodeId, inputId) {
      const group = currentGroup();
      const experiment = currentExperiment();
      if (!group || !experiment) return;
      const cobo = currentNodeById(nodeId);
      if (!cobo || cobo.kind !== "cobo" || isPortDisabled(cobo, inputId))
        return;
      const connections = boardConnectionStore();
      const inputConnected = connections.some(connection => connection.to.node_id === nodeId && connection.to.port === inputId);
      if (inputConnected)
        return;
      const zaps = [];
      for (const expGroup of experiment.groups || []) {
        for (const node of expGroup.nodes || []) {
          if (node.kind === "zap" && !isPortDisabled(node, "OUT"))
            zaps.push(node);
        }
      }
      zaps.sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));
      for (const zap of zaps) {
        const zapConnected = connections.some(connection => connection.from.node_id === zap.id && connection.from.port === "OUT");
        if (!zapConnected) {
          setConnection(
            { nodeId: zap.id, kind: "zap", portId: "OUT" },
            { nodeId: cobo.id, kind: "cobo", portId: inputId }
          );
          return;
        }
      }
      showBanner(`No free ZAP for ${cobo.label} ${inputId}`, "error");
    }

    function autoConnectFirstDetectorToMergingSlot(nodeId, slotId) {
      const group = currentGroup();
      if (!group) return;
      const merging = group.nodes.find(item => item.id === nodeId && item.kind === "merging");
      if (!merging || isPortDisabled(merging, slotId))
        return;
      const detectors = group.nodes
        .filter(item => item.kind === "detector" && !isPortDisabled(item, "OUT") && nodeBoardType(item) === nodeBoardType(merging))
        .sort((a, b) => (a.data?.det_number ?? 0) - (b.data?.det_number ?? 0));
      const connections = canvasConnections(group);

      for (const detector of detectors) {
        const detectorConnected = connections.some(connection => connection.from.node_id === detector.id && connection.from.port === "OUT");
        const slotConnected = connections.some(connection => connection.to.node_id === nodeId && connection.to.port === slotId);
        if (!detectorConnected && !slotConnected) {
          setConnection(
            { nodeId: detector.id, kind: "detector", portId: "OUT" },
            { nodeId: nodeId, kind: "merging", portId: slotId }
          );
          return;
        }
      }
      showBanner(`No free detector for ${merging.label} ${slotId}`, "error");
    }

    function isPortConnected(nodeId, portId, direction) {
      const group = currentGroup();
      if (!group) return false;
      return canvasConnections(group).some(connection => {
        if (direction === "output")
          return connection.from.node_id === nodeId && connection.from.port === portId;
        return connection.to.node_id === nodeId && connection.to.port === portId;
      });
    }

    function isConnectionSelected(connection) {
      if (!stateStore.selectedNodeId) return false;
      const ids = connectedNodeIds(stateStore.selectedNodeId);
      return ids.has(connection.from.node_id) || ids.has(connection.to.node_id);
    }

    function nodeConnectionInfo(node) {
      const group = currentGroup();
      if (!group || !node) return [];
      const experiment = currentExperiment();
      const connections = canvasConnections(group);
      const lines = [];
      if (node.kind === "detector") {
        const connection = connections.find(item => item.from.node_id === node.id && item.from.port === "OUT");
        if (connection) {
          const target = nodeByIdFromExperiment(experiment, connection.to.node_id);
          const boardNumber = target?.data?.board_number ?? "?";
          lines.push(`Merging#${boardNumber} ${connection.to.port}`);
        }
      } else if (node.kind === "zap") {
        nodePorts(node).filter(item => item.direction === "input").forEach(pinPort => {
          const connection = connections.find(item => item.to.node_id === node.id && item.to.port === pinPort.id);
          if (connection) {
            const source = nodeByIdFromExperiment(experiment, connection.from.node_id);
            lines.push(`${pinPort.id}: ${source?.label ?? "?"}`);
          }
        });
      } else if (node.kind === "asad" || node.kind === "cobo") {
        connections.forEach(connection => {
          if (connection.to.node_id === node.id) {
            const source = nodeByIdFromExperiment(experiment, connection.from.node_id);
            lines.push(`${connection.to.port}: ${source?.label ?? "?"}`);
          }
        });
      }
      return lines;
    }

    function portPeerNode(node, portId, direction) {
      const group = currentGroup();
      if (!group || !node)
        return null;
      const connection = canvasConnections(group).find(item => {
        if (direction === "input")
          return item.to.node_id === node.id && item.to.port === portId;
        return item.from.node_id === node.id && item.from.port === portId;
      });
      if (!connection)
        return null;
      const peerNodeId = direction === "input" ? connection.from.node_id : connection.to.node_id;
      return nodeByIdFromExperiment(currentExperiment(), peerNodeId);
    }

    function portDisplayLabel(node, port) {
      const baseLabel = port.label || port.id;
      const peer = portPeerNode(node, port.id, port.direction);
      if (!peer)
        return baseLabel;
      return `${baseLabel} (${peer.label})`;
    }

    function mergingSlotLabel(node, slot) {
      const basePort = nodePorts(node).find(item => item.id === slot) || { id: slot, label: slot, direction: "input" };
      return portDisplayLabel(node, basePort);
    }

    function zapPinLabel(node, pin) {
      const basePort = nodePorts(node).find(item => item.id === pin) || { id: pin, label: pin, direction: "input" };
      return portDisplayLabel(node, basePort);
    }

    function activateNode(nodeId) {
      bringNodeToFront(nodeId);
      stateStore.selectedNodeId = nodeId;
      render(false);
    }

    function cycleSelectedNode(step=1) {
      const nodes = currentVisibleNodes();
      if (!nodes || nodes.length === 0)
        return;
      const ordered = [...nodes].sort((a, b) => {
        if ((a.y || 0) !== (b.y || 0))
          return (a.y || 0) - (b.y || 0);
        if ((a.x || 0) !== (b.x || 0))
          return (a.x || 0) - (b.x || 0);
        return String(a.label || "").localeCompare(String(b.label || ""));
      });
      const currentIndex = ordered.findIndex(node => node.id === stateStore.selectedNodeId);
      const nextIndex = currentIndex < 0
        ? (step >= 0 ? 0 : ordered.length - 1)
        : (currentIndex + step + ordered.length) % ordered.length;
      activateNode(ordered[nextIndex].id);
    }

    function renderTabs() {
      renderBoardButtons();
      const experimentTabs = document.getElementById("experimentTabs");
      experimentTabs.innerHTML = "";
      const addExperimentButton = document.createElement("button");
      addExperimentButton.className = "tab add-tab";
      addExperimentButton.textContent = "+";
      addExperimentButton.title = "Add experiment";
      addExperimentButton.onclick = () => addExperiment();
      experimentTabs.appendChild(addExperimentButton);
      const loadExperimentButton = document.createElement("button");
      loadExperimentButton.className = "tab";
      loadExperimentButton.textContent = "↓";
      loadExperimentButton.title = "Load experiment";
      loadExperimentButton.onclick = () => loadExperiment();
      experimentTabs.appendChild(loadExperimentButton);
      stateStore.state.experiments.forEach(experiment => {
        const button = makeEditableTab(
          experiment.name,
          experiment.id === stateStore.selectedExperimentId,
          () => selectExperiment(experiment.id),
          value => commitTabRename("experiment", experiment.id, value),
          experiment.id,
          "experiment"
        );
        const wrapper = document.createElement("div");
        wrapper.className = "tab-wrap";
        const close = document.createElement("button");
        close.className = "tab-close";
        close.textContent = "x";
        close.title = "Delete experiment";
        close.onclick = event => {
          event.stopPropagation();
          deleteExperiment(experiment.id);
        };
        wrapper.appendChild(button);
        wrapper.appendChild(close);
        experimentTabs.appendChild(wrapper);
      });

      const groupTabs = document.getElementById("groupTabs");
      groupTabs.innerHTML = "";
      const experiment = currentExperiment();
      if (!experiment) return;
      const addGroupButton = document.createElement("button");
      addGroupButton.className = "tab add-tab";
      addGroupButton.textContent = "+";
      addGroupButton.title = "Add group";
      addGroupButton.onclick = () => addGroup();
      groupTabs.appendChild(addGroupButton);
      const allButton = document.createElement("button");
      allButton.className = `tab ${stateStore.selectedGroupId === "__all__" && !stateStore.selectedPlaceholderId && !stateStore.boardViewMode ? "active" : ""}`;
      allButton.textContent = "All";
      allButton.onclick = () => selectAllGroups();
      groupTabs.appendChild(allButton);
      const noGroupButton = document.createElement("button");
      noGroupButton.className = `tab ${stateStore.selectedGroupId === "__nogroup__" && !stateStore.selectedPlaceholderId && !stateStore.boardViewMode ? "active" : ""}`;
      noGroupButton.textContent = "No group";
      noGroupButton.onclick = () => selectNoGroup();
      groupTabs.appendChild(noGroupButton);
      const noGroupSeparator = document.createElement("span");
      noGroupSeparator.className = "tab-separator-mark";
      noGroupSeparator.textContent = ">";
      groupTabs.appendChild(noGroupSeparator);
      experiment.groups.forEach(group => {
        const button = makeEditableTab(
          group.name,
          !stateStore.boardViewMode && group.id === stateStore.selectedGroupId,
          () => selectGroup(group.id),
          value => commitTabRename("group", group.id, value),
          group.id,
          "group"
        );
        const wrapper = document.createElement("div");
        wrapper.className = "tab-wrap";
        const close = document.createElement("button");
        close.className = "tab-close";
        close.textContent = "x";
        close.title = "Delete group";
        close.onclick = event => {
          event.stopPropagation();
          deleteGroup(group.id);
        };
        wrapper.appendChild(button);
        wrapper.appendChild(close);
        groupTabs.appendChild(wrapper);
      });
      const placeholderTabs = document.getElementById("placeholderTabs");
      if (placeholderTabs)
        placeholderTabs.innerHTML = "";
      const homelessTab = document.createElement("button");
      homelessTab.className = `tab ${stateStore.selectedGroupId === "__homeless__" && !stateStore.selectedPlaceholderId && !stateStore.boardViewMode ? "active" : ""}`;
      homelessTab.textContent = "Homeless";
      homelessTab.onclick = () => selectHomelessView();
      placeholderTabs?.appendChild(homelessTab);
      const homelessSeparator = document.createElement("span");
      homelessSeparator.className = "tab-separator-mark";
      homelessSeparator.textContent = ">";
      placeholderTabs?.appendChild(homelessSeparator);
      experimentPlaceholders(experiment).forEach(node => {
        const wrapper = document.createElement("div");
        wrapper.className = "tab-wrap";
        const button = document.createElement("button");
        button.className = `tab ${stateStore.selectedPlaceholderId === node.id ? "active" : ""}`;
        button.textContent = node.label;
        button.title = "Placeholder view";
        button.onclick = () => selectPlaceholderView(node.id);
        const close = document.createElement("button");
        close.className = "tab-close";
        close.textContent = "x";
        close.title = "Delete placeholder";
        close.onclick = event => {
          event.stopPropagation();
          deletePlaceholder(node.id);
        };
        wrapper.appendChild(button);
        wrapper.appendChild(close);
        placeholderTabs?.appendChild(wrapper);
      });
    }

    function renderBoardButtons() {
      const container = document.getElementById("detectorBoardButtons");
      if (!container)
        return;
      container.innerHTML = "";
      const addSeparator = () => {
        const sep = document.createElement("span");
        sep.className = "toolbar-separator";
        container.appendChild(sep);
      };
      const addDetectorButton = (label, detectorType) => {
        const button = document.createElement("button");
        button.textContent = label;
        button.onclick = () => addDetector(detectorType);
        container.appendChild(button);
      };
      const addBoardButton = (label, kind, boardType) => {
        const button = document.createElement("button");
        button.textContent = label;
        button.onclick = () => addNode(kind, boardType);
        container.appendChild(button);
      };
      addDetectorButton("X6", "X6");
      addBoardButton("X6 Mg", "merging", "X6");
      addBoardButton("X6 Z/A", "zap", "X6");
      addSeparator();
      addDetectorButton("QQQ5", "QQQ5");
      addBoardButton("QQQ5 Mg", "merging", "QQQ5");
      addBoardButton("QQQ5 Z/A", "zap", "QQQ5");
      addSeparator();
      addDetectorButton("BB10", "BB10");
      addBoardButton("BB10 Mg", "merging", "BB10");
      addBoardButton("BB10 Z/A", "zap", "BB10");
      addSeparator();
      addBoardButton("Cobo", "cobo", "COBO");
    }

    function renderSummary() {
      const summary = document.getElementById("groupSummary");
      summary.innerHTML = "";
      const experiment = currentExperiment();
      const group = currentGroup();
      if (!experiment) return;
      if (group)
        ensureGroupPreset(group);
      const groupOptions = stateStore.groupDb.groups
        .map(entry => `<option value="${escapeHtml(entry.name)}" ${group?.group_preset === entry.name ? "selected" : ""}>${escapeHtml(entry.name)} (${entry.capacity})</option>`)
        .join("");
      const capacity = group ? groupCapacity(group) : 0;

      const expCard = document.createElement("div");
      expCard.className = "summary-card";
      expCard.innerHTML = `
        <strong>Experiment</strong>
        <div style="margin-top:8px;">
          <input value="${escapeHtml(experiment.name)}" oninput="updateExperimentName(this.value)" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
        </div>
        <div style="margin-top:8px;">
          <button onclick="saveExperimentById('${experiment.id}')">Save</button>
        </div>
        <div class="help" style="margin-top:6px;">${experiment.groups.length} groups in this setting.</div>`;
      summary.appendChild(expCard);

      if (stateStore.boardViewMode) {
        const boardNodes = group?.nodes || [];
        const boardCard = document.createElement("div");
        boardCard.className = "group-card";
        const counts = kind => boardNodes.filter(node => node.kind === kind).length;
        boardCard.innerHTML = `
          <strong>Board View</strong>
          <div class="help" style="margin-top:8px;">
            Showing boards from all groups in this experiment.<br>
            Merging ${counts("merging")}<br>
            ZAP ${counts("zap")}<br>
            COBO ${counts("cobo")}<br>
            Connections ${group?.connections?.length || 0}
          </div>`;
        summary.appendChild(boardCard);

        const listCard = document.createElement("div");
        listCard.className = "group-card";
        listCard.innerHTML = `<strong>Boards</strong><div class="summary-list" id="summaryList"></div>`;
        summary.appendChild(listCard);
        const summaryList = listCard.querySelector("#summaryList");
        boardNodes.forEach(node => {
          const item = document.createElement("div");
          item.className = `summary-item ${node.id === stateStore.selectedNodeId ? "active" : ""}`;
          const infoLines = nodeConnectionInfo(node);
          item.innerHTML = `
            <div class="summary-item-row">
              <strong>${escapeHtml(node.label)}</strong>
              <button class="summary-delete" title="Delete" onclick="event.stopPropagation(); deleteNodeById('${node.id}')">x</button>
            </div>
            <div class="help">${infoLines.length > 0 ? infoLines.map(line => escapeHtml(line)).join("<br>") : "no connection"}</div>`;
          item.onclick = () => activateNode(node.id);
          summaryList.appendChild(item);
        });
        return;
      }

      if (!group) {
        const empty = document.createElement("div");
        empty.className = "group-card";
        empty.innerHTML = '<div class="help">No group view is empty. Use <strong>Connect All</strong>, <strong>Select placeholder</strong>, or <strong>Create group</strong>.</div>';
        summary.appendChild(empty);
        return;
      }

      const nodesByKind = kind => group.nodes.filter(node => node.kind === kind).length;
      const card = document.createElement("div");
      card.className = "group-card";
      card.innerHTML = `
        <strong>Group</strong>
        <div style="margin-top:8px;">
          <input id="summaryGroupNameInput" value="${escapeHtml(group.name)}" oninput="updateGroupName(this.value)" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
        </div>
        <div style="margin-top:8px;">
          <select onchange="updateCurrentGroupPreset(this.value)">${groupOptions}</select>
        </div>
        <div style="margin-top:8px;">
          <button onclick="autoConnectVisibleGroup()">Connect all</button>
          <button onclick="clearGroup()">Clear</button>
        </div>
        <div class="help" style="margin-top:8px;">
          Capacity ${capacity}<br>
          Detectors ${nodesByKind("detector")}<span class="pill">X6 export</span><br>
          Merging ${nodesByKind("merging")}<br>
          ZAP ${nodesByKind("zap")}<br>
          COBO ${nodesByKind("cobo")}<br>
          Connections ${group.connections.length}
        </div>`;
      summary.appendChild(card);
      if (stateStore.focusGroupNameInput) {
        queueMicrotask(() => {
          const input = document.getElementById("summaryGroupNameInput");
          if (input) {
            input.focus();
            input.select();
          }
          stateStore.focusGroupNameInput = false;
        });
      }

      const listCard = document.createElement("div");
      listCard.className = "group-card";
      listCard.innerHTML = `<strong>Items</strong><div class="summary-list" id="summaryList"></div>`;
      summary.appendChild(listCard);
      const summaryList = listCard.querySelector("#summaryList");
      group.nodes.forEach(node => {
        const item = document.createElement("div");
        item.className = `summary-item ${node.id === stateStore.selectedNodeId ? "active" : ""}`;
        const infoLines = nodeConnectionInfo(node);
        const detectorSummary = node.kind === "detector"
          ? `<div style="display:grid; grid-template-columns: auto 1fr; gap:4px 6px; align-items:center; margin-top:6px;">
               <span class="help">det#</span>
               <input type="number" min="1" max="100" value="${node.data.det_number ?? 1}" onclick="event.stopPropagation()" oninput="updateSummaryDetectorNumber('${node.id}',this.value)" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
               <span class="help">phi#</span>
               <input type="number" min="0" max="${Math.max(0, capacity - 1)}" value="${node.data.phi_number ?? 0}" onclick="event.stopPropagation()" oninput="updateSummaryDetectorPhi('${node.id}',this.value)" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
               <span class="help">phi</span>
               <span class="help">${formatPhiDegrees(node)}</span>
             </div>`
          : "";
        item.innerHTML = `
          <div class="summary-item-row">
            <strong>${escapeHtml(node.label)}</strong>
            <button class="summary-delete" title="Delete" onclick="event.stopPropagation(); deleteNodeById('${node.id}')">x</button>
          </div>
          <div class="help">${infoLines.length > 0 ? infoLines.map(line => escapeHtml(line)).join("<br>") : "no connection"}</div>
          ${detectorSummary}`;
        item.onclick = () => activateNode(node.id);
        summaryList.appendChild(item);
      });

      const slotCard = document.createElement("div");
      slotCard.className = "group-card";
      slotCard.innerHTML = `
        <strong>Board Rules</strong>
        <div class="help" style="margin-top:8px;">
          Detector -> Merging slot A/B/C/D<br>
          Merging OUT -> ZAP pin<br>
          ZAP OUT -> COBO input<br>
          ZAP/AsAd carries ASAD# internally
        </div>`;
      summary.appendChild(slotCard);
    }

    function renderInspector() {
      return;
    }

    function openNodeEditModal(nodeId) {
      const node = currentNodeById(nodeId);
      if (!node) return;
      const title = document.getElementById("nodeEditTitle");
      const content = document.getElementById("nodeEditContent");
      if (title) title.textContent = node.label || "Edit Item";
      renderNodeEditForm(content, node);
      window.lilakWebFormat.toggleModal("nodeEditModal", true);
    }

    function renderNodeEditForm(inspector, node) {
      if (!inspector || !node) return;

      const ringOptions = stateStore.groupDb.groups.map(ring => `<option value="${ring.name}" ${node.data.ring_type === ring.name ? "selected" : ""}>${ring.name}</option>`).join("");
      const detectorOptions = DETECTOR_TYPES.map(type => `<option value="${type}" ${node.data.detector_type === type ? "selected" : ""}>${type}</option>`).join("");

      let inner = `<div class="form-grid">`;

      if (node.kind === "detector") {
        const entry = detectorEntry(node.data.det_number);
        inner += `
          <div class="form-grid two">
            <div class="field">
              <label>Detector Type</label>
              <select onchange="updateNodeData('${node.id}','detector_type',this.value)">${detectorOptions}</select>
            </div>
            <div class="field">
              <label>Detector Number</label>
              <input type="number" min="0" max="100" value="${node.data.det_number ?? 0}" oninput="updateDetectorNumberFromValue('${node.id}',this.value)" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
          </div>
          <div class="form-grid two">
            <div class="field">
              <label>Group Number</label>
              <input type="number" min="0" value="${node.data.ring_number ?? 1}" oninput="updateNodeDataLive('${node.id}','ring_number',toInt(this.value,0))" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
            <div class="field">
              <label>Group Type</label>
              <select onchange="updateNodeData('${node.id}','ring_type',this.value)">${ringOptions}</select>
            </div>
          </div>
          <div class="form-grid two">
            <div class="field">
              <label>Phi Number</label>
              <input type="number" min="0" value="${node.data.phi_number ?? 0}" oninput="updateNodeDataLive('${node.id}','phi_number',toInt(this.value,0))" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
            <div class="field">
              <label>dE/E</label>
              <select onchange="updateNodeData('${node.id}','dee',this.value)">
                <option value="dE" ${node.data.dee === "dE" ? "selected" : ""}>dE</option>
                <option value="E" ${node.data.dee !== "dE" ? "selected" : ""}>E</option>
              </select>
            </div>
          </div>
          <div class="form-grid two">
            <div class="field">
              <label>det_r offset (mm)</label>
              <input type="number" step="0.1" value="${node.data.det_r_mm ?? 0}" oninput="updateNodeDataLive('${node.id}','det_r_mm',toFloat(this.value,0))" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
            <div class="field">
              <label>det_z offset (mm)</label>
              <input type="number" step="0.1" value="${node.data.det_z_mm ?? 0}" oninput="updateNodeDataLive('${node.id}','det_z_mm',toFloat(this.value,0))" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
          </div>
          <div class="form-grid two">
            <div class="field">
              <label>Manual Phi Center</label>
              <input type="number" step="0.1" value="${node.data.phi_center_deg ?? 0}" oninput="updateNodeDataLive('${node.id}','phi_center_deg',toFloat(this.value,0))" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
            <div class="field">
              <label>Manual Half Opening</label>
              <input type="number" step="0.1" value="${node.data.phi_half_opening_deg ?? 10}" oninput="updateNodeDataLive('${node.id}','phi_half_opening_deg',toFloat(this.value,10))" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
          </div>
          <div class="group-card">
            <strong>Detector DB Snapshot</strong>
            <div class="help" style="margin-top:8px;">
              Thickness: ${entry ? entry.thickness_um : "-"} um<br>
              Bias: ${entry ? entry.bias_voltage_v : "-"} V<br>
              Active area: ${entry ? entry.active_width_mm : "-"} x ${entry ? entry.active_height_mm : "-"} mm
            </div>
          </div>`;
      } else {
        inner += node.kind === "zap" ? `
          <div class="form-grid two">
            <div class="field">
              <label>ZAP Number</label>
              <input type="number" min="0" value="${node.data.board_number ?? 0}" oninput="updateNodeDataLive('${node.id}','board_number',toInt(this.value,0))" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
            <div class="field">
              <label>ASAD Number</label>
              <input type="number" min="0" value="${node.data.asad_number ?? 0}" oninput="updateNodeDataLive('${node.id}','asad_number',toInt(this.value,0))" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
          </div>` : `
          <div class="field">
            <label>${node.kind.toUpperCase()} Number</label>
            <input type="number" min="0" value="${node.data.board_number ?? 0}" oninput="updateNodeDataLive('${node.id}','board_number',toInt(this.value,0))" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
          </div>`;
      }

      inner += `</div>`;
      inspector.className = "";
      inspector.innerHTML = inner;
    }

    function escapeHtml(text) {
      return String(text ?? "")
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;");
    }

    function toInt(value, fallback=0) {
      const parsed = Number.parseInt(value, 10);
      return Number.isFinite(parsed) ? parsed : fallback;
    }

    function toFloat(value, fallback=0) {
      const parsed = Number.parseFloat(value);
      return Number.isFinite(parsed) ? parsed : fallback;
    }

    function updateNodeField(nodeId, field, value) {
      const node = currentNodeById(nodeId);
      if (!node) return;
      node[field] = value;
      render();
    }

    function updateExperimentName(value) {
      const experiment = currentExperiment();
      if (!experiment) return;
      experiment.name = (value || "").trim() || "Experiment";
    }

    function updateGroupName(value) {
      const group = currentGroup();
      if (!group) return;
      group.name = (value || "").trim() || "Group";
    }

    function updateCurrentGroupPreset(value) {
      const group = currentGroup();
      if (!group) return;
      group.group_preset = value;
      syncDetectorPhiByIndex(group);
      render(false);
    }

    function formatPhiDegrees(node) {
      const group = currentGroup();
      const capacity = groupCapacity(group);
      const phi = detectorPhiDegrees(Number(node?.data?.phi_number || 0), capacity);
      return `${phi.toFixed(1)} deg`;
    }

    function updateSummaryDetectorNumber(nodeId, value) {
      const node = currentNodeById(nodeId);
      if (!node) return;
      applyDetectorNumber(node, toInt(value, node.data.det_number ?? 1));
    }

    function updateSummaryDetectorPhi(nodeId, value) {
      const node = currentNodeById(nodeId);
      const group = currentGroup();
      if (!node || !group) return;
      const capacity = groupCapacity(group);
      let phiNumber = toInt(value, node.data.phi_number ?? 0);
      if (capacity > 0)
        phiNumber = Math.max(0, Math.min(capacity - 1, phiNumber));
      node.data.phi_number = phiNumber;
      node.data.phi_center_deg = detectorPhiDegrees(phiNumber, capacity);
    }

    function updateNodeFieldLive(nodeId, field, value) {
      const node = currentNodeById(nodeId);
      if (!node) return;
      node[field] = value;
    }

    function updateNodeDataLive(nodeId, field, value) {
      const node = currentNodeById(nodeId);
      if (!node) return;
      node.data[field] = value;
      if (field === "board_number" || field === "detector_type")
        syncNodeLabel(node);
      if (field === "phi_number") {
        const capacity = groupCapacity(currentGroup());
        node.data.phi_center_deg = detectorPhiDegrees(node.data.phi_number ?? 0, capacity);
      }
    }

    function updateNodeData(nodeId, field, value) {
      const node = currentNodeById(nodeId);
      if (!node) return;
      node.data[field] = value;
      if (field === "ring_type") {
        const group = currentGroup();
        if (group)
          group.group_preset = value;
      }
      if (field === "board_number" || field === "detector_type")
        syncNodeLabel(node);
      if (field === "phi_number") {
        const capacity = groupCapacity(currentGroup());
        node.data.phi_center_deg = detectorPhiDegrees(node.data.phi_number ?? 0, capacity);
      }
      render(false);
    }

    function detectorEntry(number) {
      return stateStore.detectorDb.detectors.find(item => Number(item.number) === Number(number)) || null;
    }

    function applyDetectorNumber(node, requestedNumber) {
      if (!node) return null;
      const detectorInfo = detectorEntry(requestedNumber);
      node.data.det_number = requestedNumber;
      node.data.detector_loaded = !!detectorInfo;
      syncNodeLabel(node);
      return detectorInfo;
    }

    function updateDetectorNumberFromValue(nodeId, value) {
      const node = currentNodeById(nodeId);
      if (!node) return;
      applyDetectorNumber(node, toInt(value, node.data.det_number ?? 0));
    }

    function loadDetectorFromDb(nodeId) {
      const node = currentNodeById(nodeId);
      if (!node) return;
      const numberInput = document.getElementById(`detectorDbInput_${nodeId}`);
      if (!numberInput) return;
      const requestedNumber = toInt(numberInput.value, node.data.det_number ?? 0);
      const detectorInfo = applyDetectorNumber(node, requestedNumber);
      if (!detectorInfo) {
        showBanner(`Detector DB entry ${requestedNumber} does not exist.`, "error");
        if (numberInput) numberInput.value = `${node.data.det_number ?? 1}`;
        return;
      }
      render(false);
      showBanner(`Loaded detector DB entry ${node.data.det_number}`);
    }

    function updateDetectorPhi(nodeId) {
      const node = currentNodeById(nodeId);
      const group = currentGroup();
      if (!node || !group) return;
      const phiInput = document.getElementById(`detectorPhiInput_${nodeId}`);
      if (!phiInput) return;
      const capacity = groupCapacity(group);
      let phiNumber = toInt(phiInput.value, node.data.phi_number ?? 0);
      if (capacity > 0)
        phiNumber = Math.max(0, Math.min(capacity - 1, phiNumber));
      node.data.phi_number = phiNumber;
      node.data.phi_center_deg = detectorPhiDegrees(phiNumber, capacity);
      render(false);
    }

    function arrangeNodesPattern(nodes, startX, yStart=56, xStep=21, yStep=42, wrapCount=4) {
      nodes.forEach((node, index) => {
        const column = index % wrapCount;
        const row = index;
        const clamped = clampNodePosition(startX + column * xStep, yStart + row * yStep, node.id);
        node.x = clamped.x;
        node.y = clamped.y;
      });
    }

    function arrangeNodesPatternSpaced(nodes, startX, yStart=56, xStep=21, yStep=42, wrapCount=4, gapEvery=4, gapSize=28) {
      nodes.forEach((node, index) => {
        const column = index % wrapCount;
        const row = index;
        const gapOffset = Math.floor(index / Math.max(1, gapEvery)) * gapSize;
        const clamped = clampNodePosition(startX + column * xStep, yStart + row * yStep + gapOffset, node.id);
        node.x = clamped.x;
        node.y = clamped.y;
      });
    }

    function boardTypeOrderValue(node) {
      const type = nodeBoardType(node);
      if (type === "X6")
        return 0;
      if (type === "QQQ5")
        return 1;
      if (type === "BB10")
        return 2;
      return 9;
    }

    function sortBoardNodesByTypeAndNumber(nodes) {
      return [...nodes].sort((a, b) => {
        const typeDiff = boardTypeOrderValue(a) - boardTypeOrderValue(b);
        if (typeDiff !== 0)
          return typeDiff;
        const typeNameDiff = String(nodeBoardType(a)).localeCompare(String(nodeBoardType(b)));
        if (typeNameDiff !== 0)
          return typeNameDiff;
        return (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0);
      });
    }

    function detectorSortIndex(node) {
      if (node?.data?.det_index !== undefined)
        return toInt(node.data.det_index, 0);
      const match = String(node?.label || "").match(/^Detector\s+(\d+)/);
      return match ? toInt(match[1], 0) : Number.MAX_SAFE_INTEGER;
    }

    function sortDetectorsByLabelIndex(nodes) {
      return [...nodes].sort((a, b) => {
        const indexDiff = detectorSortIndex(a) - detectorSortIndex(b);
        if (indexDiff !== 0)
          return indexDiff;
        return String(a.label || "").localeCompare(String(b.label || ""));
      });
    }

    function sortNodesByAnchor(nodes, anchorKinds=null) {
      return [...nodes].sort((a, b) => {
        const anchorA = anchorKinds ? firstConnectedInputPeer(a, anchorKinds) : null;
        const anchorB = anchorKinds ? firstConnectedInputPeer(b, anchorKinds) : null;
        const ay = anchorA ? (anchorA.y || 0) : Number.MAX_SAFE_INTEGER;
        const by = anchorB ? (anchorB.y || 0) : Number.MAX_SAFE_INTEGER;
        if (ay !== by)
          return ay - by;
        return (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0);
      });
    }

    function experimentConnections() {
      const experiment = currentExperiment();
      if (!experiment)
        return [];
      const connections = [];
      for (const group of experiment.groups || [])
        connections.push(...(group.connections || []));
      connections.push(...(experiment.board_connections || []));
      return connections;
    }

    function arrangeNodesPatternByType(nodes, startX, yStart=56, xStep=21, yStep=42, wrapCount=4, typeGap=34, anchorKinds=null, anchorStartX=startX) {
      const grouped = new Map();
      nodes.forEach(node => {
        const nodeType = nodeBoardType(node);
        if (!grouped.has(nodeType))
          grouped.set(nodeType, []);
        grouped.get(nodeType).push(node);
      });

      let nextTypeBaseY = yStart;
      for (const [, groupNodesRaw] of grouped) {
        const groupNodes = sortNodesByAnchor(groupNodesRaw, anchorKinds);
        const firstAnchor = anchorKinds ? firstConnectedInputPeer(groupNodes[0], anchorKinds) : null;
        const typeBaseY = Math.max(nextTypeBaseY, firstAnchor ? (firstAnchor.y || yStart) : nextTypeBaseY);
        groupNodes.forEach((node, localIndex) => {
          const column = localIndex % wrapCount;
          const row = localIndex;
          const targetX = startX + column * xStep;
          const targetY = typeBaseY + row * yStep;
          const clamped = clampNodePosition(targetX, targetY, node.id);
          node.x = clamped.x;
          node.y = clamped.y;
        });
        nextTypeBaseY = typeBaseY + groupNodes.length * yStep + typeGap;
      }
    }

    function firstConnectedInputPeer(node, acceptedKinds=null) {
      if (!node)
        return null;
      const inputPorts = nodePorts(node).filter(port => port.direction === "input");
      const connections = experimentConnections();
      for (const port of inputPorts) {
        const connection = connections.find(item => item.to.node_id === node.id && item.to.port === port.id);
        if (!connection)
          continue;
        const peer = nodeByIdFromExperiment(currentExperiment(), connection.from.node_id);
        if (!peer)
          continue;
        if (acceptedKinds && !acceptedKinds.includes(peer.kind))
          continue;
        return peer;
      }
      return null;
    }

    function alignedColumnX(anchorNode, anchorStartX, targetStartX, xStep=21) {
      if (!anchorNode)
        return targetStartX;
      const raw = Math.round((anchorNode.x - anchorStartX) / xStep);
      const column = Math.max(0, Math.min(3, Number.isFinite(raw) ? raw : 0));
      return targetStartX + column * xStep;
    }

    function arrangeNodesAnchored(nodes, targetStartX, anchorStartX, anchorKinds, fallbackStartX, yStart=56, xStep=21, yStep=42, wrapCount=4) {
      nodes.forEach((node, index) => {
        const anchor = firstConnectedInputPeer(node, anchorKinds);
        if (anchor) {
          const clamped = clampNodePosition(alignedColumnX(anchor, anchorStartX, targetStartX, xStep), anchor.y, node.id);
          node.x = clamped.x;
          node.y = clamped.y;
          return;
        }
        const column = index % wrapCount;
        const row = index;
        const clamped = clampNodePosition(fallbackStartX + column * xStep, yStart + row * yStep, node.id);
        node.x = clamped.x;
        node.y = clamped.y;
      });
    }

    function applyArrangeZOrder(nodes) {
      let z = 1;
      nodes.forEach(node => {
        node.z_order = node.kind === "placeholder" ? 0 : z++;
      });
    }

    function placeholderGeometryDefaults(kind, data={}) {
      const dl = Number(data.dl || GEOM_DL);
      if (kind === "Rectangular" || kind === "Rectangle") {
        const nx = Math.max(1, toInt(data.nx, 4));
        const ny = Math.max(1, toInt(data.ny, 3));
        const pad = dl;
        return {
          width: nx * dl + 2 * pad,
          height: ny * dl + 2 * pad,
          dl,
        };
      }
      const n = Math.max(3, toInt(data.slots, 12));
      const radius = dl / (2 * Math.sin(Math.PI / n));
      const pad = dl * 1.35;
      const size = Math.ceil(2 * (radius + dl) + 2 * pad);
      return { width: size, height: size, dl };
    }

    function placeholderSlotPositions(node) {
      const type = node.data?.placeholder_type || "Ring";
      const dl = Number(node.data?.dl || GEOM_DL);
      const defaults = placeholderGeometryDefaults(type, node.data || {});
      const width = Number(node.data?.width || defaults.width);
      const height = Number(node.data?.height || defaults.height);
      const cx = width / 2;
      const cy = height / 2;
      if (type === "Rectangular" || type === "Rectangle") {
        const nx = Math.max(1, toInt(node.data?.nx, 1));
        const ny = Math.max(1, toInt(node.data?.ny, 1));
        const cellW = dl;
        const cellH = dl;
        const left = (width - cellW * nx) / 2;
        const top = (height - cellH * ny) / 2;
        const slots = [];
        for (let y = 0; y < ny; y++) {
          for (let x = 0; x < nx; x++) {
            slots.push({
              index: y * nx + x,
              x: left + (x + 0.5) * cellW,
              y: top + (y + 0.5) * cellH,
              cellX: left + x * cellW,
              cellY: top + y * cellH,
              cellW,
              cellH,
              rotation: 0,
            });
          }
        }
        return slots;
      }
      const n = Math.max(3, toInt(node.data?.slots, 12));
      const radius = dl / (2 * Math.sin(Math.PI / n));
      const vertices = Array.from({ length: n }, (_, index) => {
        const angle = index * 2 * Math.PI / n;
        return { x: cx + Math.cos(angle) * radius, y: cy - Math.sin(angle) * radius, angle };
      });
      return Array.from({ length: n }, (_, index) => {
        const a = vertices[index];
        const b = vertices[(index + 1) % n];
        const mx = (a.x + b.x) / 2;
        const my = (a.y + b.y) / 2;
        const outwardLength = Math.hypot(mx - cx, my - cy) || 1;
        const outwardX = (mx - cx) / outwardLength;
        const outwardY = (my - cy) / outwardLength;
        const rotation = Math.atan2(b.y - a.y, b.x - a.x) * 180 / Math.PI;
        return {
          index,
          x: mx + outwardX * dl / 2,
          y: my + outwardY * dl / 2,
          sideX1: a.x,
          sideY1: a.y,
          sideX2: b.x,
          sideY2: b.y,
          rotation,
        };
      });
    }

    function renderPlaceholderBody(node) {
      const type = node.data?.placeholder_type || "Ring";
      const defaults = placeholderGeometryDefaults(type, node.data || {});
      const width = Number(node.data?.width || defaults.width);
      const height = Number(node.data?.height || defaults.height);
      const dl = Number(node.data?.dl || GEOM_DL);
      const slots = placeholderSlotPositions(node);
      if (type === "Rectangular" || type === "Rectangle") {
        const cells = slots.map(slot => `
          <rect class="placeholder-slot" x="${slot.cellX}" y="${slot.cellY}" width="${slot.cellW}" height="${slot.cellH}" rx="4"></rect>
          <text class="placeholder-label" x="${slot.x}" y="${slot.y}">${slot.index}</text>
        `).join("");
        return `<svg class="placeholder-svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">${cells}</svg>`;
      }
      const n = Math.max(3, toInt(node.data?.slots, 12));
      const cx = width / 2;
      const cy = height / 2;
      const radius = dl / (2 * Math.sin(Math.PI / n));
      const vertices = Array.from({ length: n }, (_, index) => {
        const angle = index * 2 * Math.PI / n;
        return `${cx + Math.cos(angle) * radius},${cy - Math.sin(angle) * radius}`;
      }).join(" ");
      const polygon = type === "Ring"
        ? `<polygon points="${vertices}" fill="rgba(87,117,144,0.06)" stroke="rgba(46,111,149,0.56)" stroke-width="2"></polygon>`
        : `<circle cx="${cx}" cy="${cy}" r="${radius}" fill="rgba(87,117,144,0.06)" stroke="rgba(46,111,149,0.56)" stroke-width="2"></circle>`;
      const sides = slots.map(slot => `
        <line x1="${slot.sideX1}" y1="${slot.sideY1}" x2="${slot.sideX2}" y2="${slot.sideY2}" stroke="rgba(46,111,149,0.28)" stroke-width="1"></line>
      `).join("");
      const labels = slots.map(slot => `
        <rect class="placeholder-slot" x="${slot.x - dl / 2}" y="${slot.y - dl / 2}" width="${dl}" height="${dl}" rx="4" transform="rotate(${slot.rotation} ${slot.x} ${slot.y})"></rect>
        <text class="placeholder-label" x="${slot.x}" y="${slot.y}">${slot.index}</text>
      `).join("");
      return `<svg class="placeholder-svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">${polygon}${sides}${labels}</svg>`;
    }

    function placeDetectorOnPlaceholderSlot(detector, placeholder, slotIndex) {
      const slots = placeholderSlotPositions(placeholder);
      if (!detector || !placeholder || slots.length === 0)
        return false;
      const slot = slots[((slotIndex % slots.length) + slots.length) % slots.length];
      const dl = Number(placeholder.data?.dl || GEOM_DL);
      detector.x = placeholder.x + slot.x - dl / 2;
      detector.y = placeholder.y + PLACEHOLDER_HEADER_H + slot.y - dl / 2;
      detector.data.phi_number = slot.index;
      detector.data.phi_center_deg = detectorPhiDegrees(slot.index, Math.max(1, slots.length));
      detector.data.placeholder_id = placeholder.id;
      detector.data.placeholder_type = placeholder.data?.placeholder_type || "";
      detector.data.geom_rotation_deg = slot.rotation || 0;
      return true;
    }

    function arrangeGeometryByPhi(doRender=true) {
      const group = currentGroup();
      if (!group)
        return;
      const placeholders = (group.nodes || []).filter(node => node.kind === "placeholder");
      if (placeholders.length === 0)
        return;
      const detectors = (group.nodes || []).filter(node => node.kind === "detector");
      detectors.forEach((detector) => {
        const placeholder = placeholders.find(node => node.id === detector.data?.placeholder_id) || placeholders[0];
        placeDetectorOnPlaceholderSlot(detector, placeholder, toInt(detector.data?.phi_number, 0));
      });
      if (doRender)
        render();
    }

    function arrangeCanvasItems() {
      const visibleNodes = currentVisibleNodes();
      if (visibleNodes.length === 0)
        return;

      if (stateStore.boardViewMode) {
        const mergings = sortBoardNodesByTypeAndNumber(visibleNodes.filter(node => node.kind === "merging"));
        const zaps = sortBoardNodesByTypeAndNumber(visibleNodes.filter(node => node.kind === "zap"));
        const cobos = visibleNodes.filter(node => node.kind === "cobo").sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));
        arrangeNodesPatternByType(mergings, 30, 56, 21, 42, 4, 34, null, 24);
        arrangeNodesPatternByType(zaps, 360, 56, 21, 42, 4, 34, ["merging"], 30);
        arrangeNodesPatternByType(cobos, 700, 56, 21, 42, 4, 34, ["zap", "asad"], 360);
        applyArrangeZOrder([...mergings, ...zaps, ...cobos]);
      } else if (stateStore.geomViewMode) {
        const placeholders = visibleNodes.filter(node => node.kind === "placeholder");
        const detectors = sortDetectorsByLabelIndex(visibleNodes.filter(node => node.kind === "detector"));
        placeholders.forEach((node, index) => {
          node.x = 230 + index * 28;
          node.y = 72 + index * 28;
        });
        if (placeholders.length > 0) {
          detectors.forEach((node) => {
            const placeholder = placeholders.find(item => item.id === node.data?.placeholder_id) || placeholders[0];
            placeDetectorOnPlaceholderSlot(node, placeholder, toInt(node.data?.phi_number, 0));
          });
        } else {
          detectors.forEach((node, index) => {
            node.x = 24 + (index % 4) * 110;
            node.y = 56 + Math.floor(index / 4) * 48;
          });
        }
        applyArrangeZOrder([...placeholders, ...detectors]);
      } else {
        const detectors = sortDetectorsByLabelIndex(visibleNodes.filter(node => node.kind === "detector"));
        const mergings = visibleNodes.filter(node => node.kind === "merging").sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));
        const zaps = visibleNodes.filter(node => node.kind === "zap").sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));
        const cobos = visibleNodes.filter(node => node.kind === "cobo").sort((a, b) => (a.data?.board_number ?? 0) - (b.data?.board_number ?? 0));
        arrangeNodesPattern(detectors, 24);
        arrangeNodesAnchored(mergings, 360, 24, ["detector"], 360);
        arrangeNodesAnchored(zaps, 700, 360, ["merging"], 700);
        const coboCenterX = 520;
        const coboBaseY = Math.max(
          320,
          ...visibleNodes
            .filter(node => node.kind !== "cobo")
            .map(node => (node.y || 0) + 170)
        );
        cobos.forEach((node, index) => {
          const clamped = clampNodePosition(coboCenterX + (index % 2) * 24, coboBaseY + index * 56, node.id);
          node.x = clamped.x;
          node.y = clamped.y;
        });
        applyArrangeZOrder([...detectors, ...mergings, ...zaps, ...cobos]);
      }
      render();
      showBanner("Arranged visible items");
    }

    function renderNode(node) {
      const ports = nodePorts(node);
      const wrapper = document.createElement("div");
      const unloadedClass = (node.kind === "detector" && !node.data.detector_loaded) ? "unloaded" : "";
      const geomCompactClass = (stateStore.geomViewMode && node.kind === "detector") ? "geom-compact" : "";
      wrapper.className = `node ${node.kind} ${unloadedClass} ${geomCompactClass} ${node.id === stateStore.selectedNodeId ? "selected" : ""}`;
      if (stateStore.errorNodeIds.includes(node.id))
        wrapper.classList.add("error");
      wrapper.style.left = `${node.x}px`;
      wrapper.style.top = `${node.y}px`;
      if (node.kind === "placeholder") {
        const defaults = placeholderGeometryDefaults(node.data?.placeholder_type || "Ring", node.data || {});
        wrapper.style.width = `${Number(node.data?.width || defaults.width)}px`;
        wrapper.style.minHeight = `${Number(node.data?.height || defaults.height) + 42}px`;
        wrapper.style.zIndex = "0";
      } else if (stateStore.geomViewMode && node.kind === "detector") {
        const dl = GEOM_DL;
        wrapper.style.width = `${dl}px`;
        wrapper.style.height = `${dl}px`;
        wrapper.style.zIndex = `${Math.max(20, Number(node.z_order) || 20)}`;
        wrapper.style.transform = `rotate(${Number(node.data?.geom_rotation_deg || 0)}deg)`;
        wrapper.style.transformOrigin = "50% 50%";
      } else {
        wrapper.style.zIndex = `${node.z_order || 1}`;
        wrapper.style.transform = "";
      }
      wrapper.dataset.nodeId = node.id;

      const header = document.createElement("div");
      header.className = "node-header";
      const headerLabel = stateStore.geomViewMode && node.kind === "detector"
        ? `${node.data.detector_type || "X6"} #${node.data.det_number ?? 1}`
        : node.kind === "detector"
          ? `${node.label} (${node.data.detector_type || "X6"} #${node.data.det_number ?? 1})`
        : node.label;
      const headerTitle = stateStore.geomViewMode && node.kind === "detector"
        ? `<span class="node-title">(${escapeHtml(detectorSortIndex(node))}) ${escapeHtml(node.data.detector_type || "X6")} #${escapeHtml(node.data.det_number ?? 0)}</span><button class="geom-edit-button" data-action="open-edit" title="Edit Item">edit</button>`
        : stateStore.geomViewMode
          ? `<span class="node-title">${escapeHtml(headerLabel)}</span>`
          : `<span class="node-title">${escapeHtml(headerLabel)}</span>${window.lilakWebFormat.nodeEditButton(node.id, escapeHtml, {title: "Edit Item"})}`;
      header.innerHTML = headerTitle;
      header.addEventListener("mousedown", event => {
        if (event.target.closest("button")) return;
        startNodeDrag(event, node.id);
      });
      const editButton = header.querySelector("button[data-action='open-edit']");
      if (editButton) {
        editButton.addEventListener("click", event => {
          event.stopPropagation();
          activateNode(node.id);
          openNodeEditModal(node.id);
        });
      }
      wrapper.appendChild(header);

      const body = document.createElement("div");
      body.className = "node-body";
      if (node.kind === "placeholder") {
        body.innerHTML = renderPlaceholderBody(node);
      } else if (node.kind === "detector") {
        const detectorInfo = detectorEntry(node.data.det_number);
        const capacity = groupCapacity(currentGroup());
        const phiDegrees = detectorPhiDegrees(Number(node.data.phi_number ?? 0), capacity);
        const infoText = node.data.detector_loaded && detectorInfo
          ? `thickness ${detectorInfo.thickness_um ?? "-"} um<br>bias ${detectorInfo.bias_voltage_v ?? "-"} V`
          : `thickness -<br>bias -`;
        body.innerHTML = `
          ${escapeHtml(node.data.detector_type || "X6")}<br>
          ring ${escapeHtml(node.data.ring_type || "Free")} #${node.data.ring_number || 0}
          <div style="margin-top:6px; color: var(--muted); font-size:12px;">${infoText}</div>
          <div style="margin-top:6px; color: var(--muted); font-size:12px;">phi ${phiDegrees.toFixed(1)} deg</div>
          <div class="node-input-row">
            <span>det#</span>
            <div class="node-inline-tools">
            <input id="detectorDbInput_${node.id}" type="number" min="0" max="100" value="${node.data.det_number ?? 0}" onclick="event.stopPropagation()" oninput="updateDetectorNumberFromValue('${node.id}',this.value)" onblur="render(false)" onkeydown="if(event.key==='Enter'){event.stopPropagation(); this.blur();}">
            </div>
          </div>
          <div class="node-input-row">
            <span>phi#</span>
            <div class="node-inline-tools single">
            <input id="detectorPhiInput_${node.id}" type="number" min="0" max="${Math.max(0, capacity - 1)}" value="${node.data.phi_number ?? 0}" onclick="event.stopPropagation()" onkeydown="if(event.key==='Enter'){event.stopPropagation(); updateDetectorPhi('${node.id}');}">
            </div>
          </div>`;
      } else if (node.kind === "merging") {
        body.innerHTML = `board#${node.data.board_number ?? 0}`;
      } else if (node.kind === "zap") {
        body.innerHTML = `
          board#${node.data.board_number ?? 0}<br>
          ASAD#${node.data.asad_number ?? 0}`;
      } else if (node.kind === "new_group") {
        const group = currentGroup();
        const isHomeless = !!group?.homeless_view;
        body.innerHTML = `
          selected ${node.data?.selected_count || 0}
          <div style="display:grid; gap:8px; margin-top:10px;">
            <button onclick="event.stopPropagation(); ${isHomeless ? "connectAllHomelessDetectors()" : "connectAllNoGroupDetectors()"}">Connect All</button>
            <button onclick="event.stopPropagation(); ${isHomeless ? "openPlaceholderSelectModal()" : "openPlaceholderSelectModal()"}">${isHomeless ? "Select placeholder" : "Select group"}</button>
            <button onclick="event.stopPropagation(); ${isHomeless ? "createPlaceholderFromHomelessSelection()" : "createPlaceholderFromHomelessSelection()"}">${isHomeless ? "Create placeholder" : "Create group"}</button>
          </div>`;
      } else {
        body.innerHTML = `board#${node.data.board_number ?? 0}`;
      }

      ports.forEach(port => {
        const disabled = isPortDisabled(node, port.id);
        const row = document.createElement("div");
        row.className = "port-row";
        const left = document.createElement("div");
        left.className = "port-label";
        left.textContent = node.kind === "new_group"
          ? ""
          : (port.direction === "input"
              ? (node.kind === "merging" ? mergingSlotLabel(node, port.id) : (node.kind === "zap" ? zapPinLabel(node, port.id) : portDisplayLabel(node, port)))
              : "");
        const right = document.createElement("div");
        right.className = "port-label";
        right.textContent = node.kind === "new_group" ? "" : (port.direction === "output" ? portDisplayLabel(node, port) : "");
        const cell = document.createElement("div");
        cell.className = "port-cell";
        const portEl = document.createElement("div");
        portEl.className = `port ${port.direction}`;
        if (isPortConnected(node.id, port.id, port.direction))
          portEl.classList.add("connected");
        if (disabled)
          portEl.classList.add("disabled");
        if (stateStore.pendingConnection && stateStore.pendingConnection.nodeId === node.id && stateStore.pendingConnection.portId === port.id)
          portEl.classList.add("active-connect");
        portEl.dataset.nodeId = node.id;
        portEl.dataset.portId = port.id;
        portEl.dataset.kind = node.kind;
        portEl.dataset.direction = port.direction;
        portEl.addEventListener("mousedown", event => { if (!disabled) beginWire(event, node, port); });
        portEl.addEventListener("mouseup", event => { if (!disabled) finishWire(event, node, port); });
        portEl.addEventListener("click", event => { if (!disabled) clickWire(event, node, port); });
        if (node.kind === "detector" && port.direction === "output")
          portEl.addEventListener("dblclick", event => {
            if (!disabled) {
              event.stopPropagation();
              const group = currentGroup();
              if (group?.homeless_view)
                toggleHomelessDetectorSelection(node.id);
              else if (stateStore.selectedGroupId === "__nogroup__")
                toggleNoGroupDetectorSelection(node.id);
              else
                autoConnectDetectorToLatestMerging(node.id);
            }
          });
        if (node.kind === "merging" && port.direction === "input")
          portEl.addEventListener("dblclick", event => { if (!disabled) { event.stopPropagation(); autoConnectFirstDetectorToMergingSlot(node.id, port.id); } });
        if (node.kind === "merging" && port.direction === "output")
          portEl.addEventListener("dblclick", event => { if (!disabled) { event.stopPropagation(); autoConnectMergingToZap(node.id); } });
        if (node.kind === "zap" && port.direction === "input")
          portEl.addEventListener("dblclick", event => { if (!disabled) { event.stopPropagation(); autoConnectFirstMergingToZapPin(node.id, port.id); } });
        if (node.kind === "zap" && port.direction === "output")
          portEl.addEventListener("dblclick", event => { if (!disabled) { event.stopPropagation(); autoConnectZapToCobo(node.id); } });
        if (node.kind === "cobo" && port.direction === "input")
          portEl.addEventListener("dblclick", event => { if (!disabled) { event.stopPropagation(); autoConnectFirstZapToCoboInput(node.id, port.id); } });
        const disableEl = document.createElement("div");
        disableEl.className = `port-disable ${disabled ? "disabled" : "enabled"}`;
        disableEl.title = disabled ? "Enable this port" : "Disable this port";
        disableEl.setAttribute("aria-label", disabled ? "Port disabled" : "Port enabled");
        disableEl.addEventListener("click", event => { event.stopPropagation(); togglePortDisabled(node.id, port.id); });
        cell.appendChild(portEl);
        cell.appendChild(disableEl);
        if (port.direction === "input") {
          row.append(cell, left, right);
        } else {
          row.append(left, right, cell);
        }
        body.appendChild(row);
      });
      wrapper.appendChild(body);

      wrapper.addEventListener("click", event => {
        if (!event.target.classList.contains("port")) {
          bringNodeToFront(node.id);
          stateStore.selectedNodeId = node.id;
          render(false);
        }
      });
      return wrapper;
    }

    function renderCanvas() {
      const nodeLayer = document.getElementById("nodeLayer");
      nodeLayer.innerHTML = "";
      nodeLayer.classList.toggle("geom-view", stateStore.geomViewMode);
      const group = currentGroup();
      if (!group) return;
      const visibleNodes = currentVisibleNodes();
      const orderedNodes = stateStore.geomViewMode
        ? [
            ...visibleNodes.filter(node => node.kind === "placeholder"),
            ...visibleNodes.filter(node => node.kind !== "placeholder"),
          ]
        : visibleNodes;
      orderedNodes.forEach(node => nodeLayer.appendChild(renderNode(node)));
      if (stateStore.geomViewMode) {
        document.getElementById("wireLayerBack").innerHTML = "";
        document.getElementById("wireLayerFront").innerHTML = "";
      } else {
        renderWires();
      }
    }

    function refreshPortHighlights() {
      document.querySelectorAll(".port.active-connect").forEach(port => port.classList.remove("active-connect"));
      if (!stateStore.pendingConnection) return;
      const port = document.querySelector(`.port[data-node-id="${stateStore.pendingConnection.nodeId}"][data-port-id="${stateStore.pendingConnection.portId}"]`);
      if (port) port.classList.add("active-connect");
    }

    function portCenter(nodeId, portId) {
      const nodeLayerRect = document.getElementById("nodeLayer").getBoundingClientRect();
      const port = document.querySelector(`.port[data-node-id="${nodeId}"][data-port-id="${portId}"]`);
      if (!port) return null;
      const rect = port.getBoundingClientRect();
      return {
        x: rect.left - nodeLayerRect.left + rect.width / 2,
        y: rect.top - nodeLayerRect.top + rect.height / 2,
      };
    }

    function wirePath(a, b) {
      const dx = Math.max(60, Math.abs(b.x - a.x) * 0.45);
      return `M ${a.x} ${a.y} C ${a.x + dx} ${a.y}, ${b.x - dx} ${b.y}, ${b.x} ${b.y}`;
    }

    function renderWires() {
      const layerBack = document.getElementById("wireLayerBack");
      const layerFront = document.getElementById("wireLayerFront");
      layerBack.innerHTML = "";
      layerFront.innerHTML = "";
      const group = currentGroup();
      if (!group) return;

      canvasConnections(group).forEach(connection => {
        const start = portCenter(connection.from.node_id, connection.from.port);
        const end = portCenter(connection.to.node_id, connection.to.port);
        if (!start || !end) return;
        const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
        path.setAttribute("d", wirePath(start, end));
        path.setAttribute("fill", "none");
        path.setAttribute("stroke", "#19647e");
        path.setAttribute("stroke-width", "3");
        if (isConnectionSelected(connection))
          layerFront.appendChild(path);
        else
          layerBack.appendChild(path);
      });

      if (stateStore.dragConnection) {
        const start = portCenter(stateStore.dragConnection.nodeId, stateStore.dragConnection.portId);
        const end = stateStore.dragConnection.cursor;
        if (start && end) {
          const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
          path.setAttribute("d", wirePath(start, end));
          path.setAttribute("fill", "none");
          path.setAttribute("stroke", "#f28f3b");
          path.setAttribute("stroke-width", "3");
          path.setAttribute("stroke-dasharray", "7 5");
          layerFront.appendChild(path);
        }
      }
    }

    function incomingConnectionForPort(nodeId, portId) {
      const group = currentGroup();
      if (!group)
        return null;
      return canvasConnections(group).find(connection =>
        connection.to.node_id === nodeId && connection.to.port === portId
      ) || null;
    }

    function beginWire(event, node, port) {
      if (port.direction === "input") {
        const connection = incomingConnectionForPort(node.id, port.id);
        if (!connection)
          return;
        const sourceNode = currentNodeById(connection.from.node_id) || nodeByIdFromExperiment(currentExperiment(), connection.from.node_id);
        if (!sourceNode || isPortDisabled(sourceNode, connection.from.port))
          return;
        event.stopPropagation();
        stateStore.pendingConnection = {
          nodeId: sourceNode.id,
          kind: sourceNode.kind,
          portId: connection.from.port,
        };
        const layerRect = document.getElementById("nodeLayer").getBoundingClientRect();
        stateStore.dragConnection = {
          nodeId: sourceNode.id,
          kind: sourceNode.kind,
          portId: connection.from.port,
          cursor: { x: event.clientX - layerRect.left, y: event.clientY - layerRect.top },
        };
        refreshPortHighlights();
        renderWires();
        return;
      }
      if (port.direction !== "output") return;
      event.stopPropagation();
      stateStore.pendingConnection = { nodeId: node.id, kind: node.kind, portId: port.id };
      const layerRect = document.getElementById("nodeLayer").getBoundingClientRect();
      stateStore.dragConnection = {
        nodeId: node.id,
        kind: node.kind,
        portId: port.id,
        cursor: { x: event.clientX - layerRect.left, y: event.clientY - layerRect.top },
      };
      refreshPortHighlights();
      renderWires();
    }

    function finishWire(event, node, port) {
      if (!stateStore.dragConnection || port.direction !== "input") return;
      event.stopPropagation();
      const target = { nodeId: node.id, kind: node.kind, portId: port.id };
      if (portCompatible(stateStore.dragConnection, target)) {
        setConnection(stateStore.dragConnection, target);
      } else {
        stateStore.dragConnection = null;
        renderWires();
      }
    }

    function clickWire(event, node, port) {
      event.stopPropagation();
      if (port.direction === "output") {
        if (stateStore.pendingConnection &&
            stateStore.pendingConnection.nodeId === node.id &&
            stateStore.pendingConnection.portId === port.id) {
          stateStore.pendingConnection = null;
        } else {
          stateStore.pendingConnection = { nodeId: node.id, kind: node.kind, portId: port.id };
        }
        refreshPortHighlights();
        renderWires();
        return;
      }

      if (!stateStore.pendingConnection || port.direction !== "input")
        return;

      const target = { nodeId: node.id, kind: node.kind, portId: port.id };
      if (portCompatible(stateStore.pendingConnection, target)) {
        setConnection(stateStore.pendingConnection, target);
        stateStore.pendingConnection = null;
        showBanner("Connected ports");
      } else {
        stateStore.pendingConnection = null;
        refreshPortHighlights();
        renderWires();
        showBanner("Those ports cannot be connected.", "error");
      }
    }

    function startNodeDrag(event, nodeId) {
      const node = currentNodeById(nodeId);
      if (!node) return;
      bringNodeToFront(nodeId);
      stateStore.selectedNodeId = nodeId;
      stateStore.dragNodeId = nodeId;
      stateStore.dragOffsetX = event.clientX - node.x;
      stateStore.dragOffsetY = event.clientY - node.y;
      render(false);
      event.preventDefault();
    }

    function onMouseMove(event) {
      const layerRect = document.getElementById("nodeLayer").getBoundingClientRect();
      if (stateStore.dragNodeId) {
        const node = currentNodeById(stateStore.dragNodeId);
        if (!node) return;
        const prevX = node.x;
        const prevY = node.y;
        const clamped = clampNodePosition(event.clientX - stateStore.dragOffsetX, event.clientY - stateStore.dragOffsetY, stateStore.dragNodeId);
        node.x = clamped.x;
        node.y = clamped.y;
        if (node.kind === "placeholder") {
          const dx = node.x - prevX;
          const dy = node.y - prevY;
          experimentDetectorNodes().forEach(item => {
            if (item.kind === "detector" && item.data?.placeholder_id === node.id) {
              item.x += dx;
              item.y += dy;
            }
          });
        }
        storeManagerPosition(node);
        renderCanvas();
        return;
      }
      if (stateStore.dragConnection) {
        stateStore.dragConnection.cursor = { x: event.clientX - layerRect.left, y: event.clientY - layerRect.top };
        renderWires();
      }
    }

    function snapDetectorToNearestPlaceholder(nodeId) {
      if (!stateStore.geomViewMode)
        return false;
      const group = currentGroup();
      const detector = group?.nodes?.find(node => node.id === nodeId && node.kind === "detector");
      if (!group || !detector)
        return false;
      const placeholders = (group.nodes || []).filter(node => node.kind === "placeholder");
      if (placeholders.length === 0)
        return false;
      const detectorCenter = { x: detector.x + GEOM_DL / 2, y: detector.y + GEOM_DL / 2 };
      let best = null;
      for (const placeholder of placeholders) {
        for (const slot of placeholderSlotPositions(placeholder)) {
          const x = placeholder.x + slot.x;
          const y = placeholder.y + PLACEHOLDER_HEADER_H + slot.y;
          const d2 = (detectorCenter.x - x) ** 2 + (detectorCenter.y - y) ** 2;
          if (!best || d2 < best.d2)
            best = { placeholder, slot, x, y, d2 };
        }
      }
      if (!best || best.d2 > 120 ** 2)
        return false;
      placeDetectorOnPlaceholderSlot(detector, best.placeholder, best.slot.index);
      showBanner(`${detector.label} snapped to ${best.placeholder.label} phi#${best.slot.index}`);
      return true;
    }

    function onMouseUp(event) {
      const draggedNodeId = stateStore.dragNodeId;
      stateStore.dragNodeId = null;
      if (draggedNodeId && snapDetectorToNearestPlaceholder(draggedNodeId)) {
        render();
        return;
      }
      if (stateStore.dragConnection) {
        const targetElement = document.elementFromPoint(event.clientX, event.clientY);
        if (targetElement && targetElement.classList && targetElement.classList.contains("port") && targetElement.dataset.direction === "input") {
          const target = {
            nodeId: targetElement.dataset.nodeId,
            kind: targetElement.dataset.kind,
            portId: targetElement.dataset.portId,
          };
          if (portCompatible(stateStore.dragConnection, target)) {
            setConnection(stateStore.dragConnection, target);
            return;
          }
        }
        stateStore.dragConnection = null;
        refreshPortHighlights();
        renderWires();
      }
    }

    function clampNodePosition(x, y, nodeId=null) {
      const layer = document.getElementById("nodeLayer");
      if (!layer)
        return { x: Math.max(10, x), y: Math.max(10, y) };
      const nodeEl = nodeId ? layer.querySelector(`[data-node-id="${nodeId}"]`) : null;
      const width = nodeEl?.offsetWidth || 190;
      const height = nodeEl?.offsetHeight || 250;
      const maxX = Math.max(10, layer.clientWidth - width - 10);
      const maxY = Math.max(10, layer.clientHeight - height - 10);
      return {
        x: Math.max(10, Math.min(x, maxX)),
        y: Math.max(10, Math.min(y, maxY)),
      };
    }

    function onKeyDown(event) {
      const target = event.target;
      const tagName = (target?.tagName || "").toUpperCase();
      const isEditing = tagName === "INPUT" || tagName === "TEXTAREA" || tagName === "SELECT" || target?.isContentEditable;
      if (event.key === "?" && !isEditing) {
        event.preventDefault();
        toggleShortcutsModal();
        return;
      }
      if (event.key === "Escape" && stateStore.navigator) {
        event.preventDefault();
        closeNavigator("__cancel__");
        return;
      }
      if (event.key === "Escape") {
        const detectorModal = document.getElementById("detectorDbModal");
        const groupModal = document.getElementById("groupDbModal");
        const boardTypesModal = document.getElementById("boardTypesModal");
        const checkMappingModal = document.getElementById("checkMappingModal");
        const nodeEditModal = document.getElementById("nodeEditModal");
        const placeholderCreateModal = document.getElementById("placeholderCreateModal");
        const placeholderSelectModal = document.getElementById("placeholderSelectModal");
        const shortcutsModal = document.getElementById("shortcutsModal");
        if (shortcutsModal?.classList.contains("open")) {
          event.preventDefault();
          closeModal("shortcutsModal");
          return;
        }
        if (placeholderSelectModal?.classList.contains("open")) {
          event.preventDefault();
          closeModal("placeholderSelectModal");
          return;
        }
        if (placeholderCreateModal?.classList.contains("open")) {
          event.preventDefault();
          closePlaceholderCreateModal();
          return;
        }
        if (nodeEditModal?.classList.contains("open")) {
          event.preventDefault();
          closeModal("nodeEditModal");
          return;
        }
        if (checkMappingModal?.classList.contains("open")) {
          event.preventDefault();
          closeModal("checkMappingModal");
          return;
        }
        if (boardTypesModal?.classList.contains("open")) {
          event.preventDefault();
          closeModal("boardTypesModal");
          return;
        }
        if (groupModal?.classList.contains("open")) {
          event.preventDefault();
          closeModal("groupDbModal");
          return;
        }
        if (detectorModal?.classList.contains("open")) {
          event.preventDefault();
          closeModal("detectorDbModal");
          return;
        }
      }
      if (event.key === "Tab" && !isEditing) {
        event.preventDefault();
        cycleSelectedNode(event.shiftKey ? -1 : 1);
        return;
      }
      if ((event.key === " " || event.code === "Space") && !isEditing) {
        const node = currentSelection();
        if (node && node.kind !== "placeholder") {
          event.preventDefault();
          openNodeEditModal(node.id);
          return;
        }
      }
      if ((event.metaKey || event.ctrlKey) && !event.shiftKey && !event.altKey) {
        const lowered = event.key.toLowerCase();
        if (lowered === "s") {
          event.preventDefault();
          if (stateStore.selectedExperimentId)
            saveExperimentById(stateStore.selectedExperimentId);
          return;
        }
        if (lowered === "l") {
          event.preventDefault();
          loadExperiment();
          return;
        }
        if (lowered === "a") {
          event.preventDefault();
          arrangeCanvasItems();
          return;
        }
        if (lowered === "g") {
          event.preventDefault();
          addGroup();
          return;
        }
        if (lowered === "e") {
          event.preventDefault();
          if (stateStore.selectedExperimentId)
            exportMappingByExperiment(stateStore.selectedExperimentId);
          return;
        }
        if (lowered === "b") {
          event.preventDefault();
          toggleBoardView();
          return;
        }
        if (lowered === "h") {
          event.preventDefault();
          selectHomelessView();
          return;
        }
        if (lowered === "x") {
          event.preventDefault();
          selectNoGroup();
          return;
        }
        const digit = Number.parseInt(event.key, 10);
        if (Number.isFinite(digit) && digit === 0) {
          event.preventDefault();
          selectAllGroups();
          return;
        }
        if (Number.isFinite(digit) && digit >= 1 && digit <= 9) {
          const experiment = currentExperiment();
          const targetGroup = experiment?.groups?.[digit - 1];
          if (targetGroup) {
            event.preventDefault();
            selectGroup(targetGroup.id);
            return;
          }
        }
      }
      if (event.key !== "Backspace")
        return;
      if (isEditing)
        return;
      if (!stateStore.selectedNodeId)
        return;
      event.preventDefault();
      deleteSelection();
    }

    function openDetectorDb() {
      renderDetectorDbTable();
      document.getElementById("detectorDbModal").classList.add("open");
    }

    function openGroupDb() {
      renderGroupDbTable();
      document.getElementById("groupDbModal").classList.add("open");
    }

    function openBoardTypesDb() {
      renderBoardTypesTable();
      document.getElementById("boardTypesModal").classList.add("open");
    }

    function openCheckMapping() {
      const pathInput = document.getElementById("checkMappingPath");
      const defaultPath = stateStore.lastCheckMappingPath || stateStore.lastExportDir || directoryOfPath(stateStore.lastExperimentPath) || stateStore.cwd || "";
      pathInput.value = defaultPath;
      document.getElementById("checkMappingOutput").textContent = "Waiting for output.";
      document.getElementById("checkMappingModal").classList.add("open");
      queueMicrotask(() => pathInput.focus());
    }

    function closeModal(id) {
      document.getElementById(id).classList.remove("open");
    }

    function toggleShortcutsModal() {
      window.lilakWebFormat.toggleShortcutsModal();
    }

    async function browseCheckMappingPath() {
      const selected = await openNavigator({
        mode: "directory",
        title: "Choose Mapping Folder",
        confirmLabel: "Open",
        fileName: "",
        path: document.getElementById("checkMappingPath").value || stateStore.lastCheckMappingPath || stateStore.lastExportDir || stateStore.cwd || "",
      });
      if (selected === "__cancel__" || !selected)
        return;
      document.getElementById("checkMappingPath").value = selected;
      stateStore.lastCheckMappingPath = selected;
    }

    async function runCheckMapping(action) {
      const mappingDir = (document.getElementById("checkMappingPath").value || "").trim();
      const outputBox = document.getElementById("checkMappingOutput");
      if (!mappingDir) {
        showBanner("Choose a mapping folder first.", "error");
        return;
      }
      stateStore.lastCheckMappingPath = mappingDir;
      outputBox.textContent = "Running...";
      try {
        const payload = await fetchJson("/api/check_mapping", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            mapping_dir: mappingDir,
            action,
            num_print: toInt(document.getElementById("checkMappingNumPrint").value, 20),
            query_cobo: toInt(document.getElementById("checkMappingCobo").value, 0),
            query_asad: toInt(document.getElementById("checkMappingAsad").value, 0),
            query_aget: toInt(document.getElementById("checkMappingAget").value, 3),
            query_chan: toInt(document.getElementById("checkMappingChan").value, 0),
          }),
        });
        outputBox.textContent = payload.output || "No output.";
      } catch (error) {
        outputBox.textContent = error.message || "Check failed.";
        showBanner(error.message, "error");
      }
    }

    function renderDetectorDbTable() {
      const tbody = document.getElementById("detectorDbTable");
      tbody.innerHTML = "";
      stateStore.detectorDb.detectors.forEach((entry, index) => {
        const row = document.createElement("tr");
        row.innerHTML = `
          <td>${entry.number}</td>
          <td><input value="${entry.thickness_um}" oninput="updateDetectorDb(${index},'thickness_um',toFloat(this.value,0))"></td>
          <td><input value="${entry.bias_voltage_v}" oninput="updateDetectorDb(${index},'bias_voltage_v',toFloat(this.value,0))"></td>
          <td><input value="${entry.active_width_mm}" oninput="updateDetectorDb(${index},'active_width_mm',toFloat(this.value,0))"></td>
          <td><input value="${entry.active_height_mm}" oninput="updateDetectorDb(${index},'active_height_mm',toFloat(this.value,0))"></td>
          <td><input value="${escapeHtml(entry.note || '')}" oninput="updateDetectorDb(${index},'note',this.value)"></td>`;
        tbody.appendChild(row);
      });
    }

    function renderGroupDbTable() {
      const tbody = document.getElementById("groupDbTable");
      tbody.innerHTML = "";
      stateStore.groupDb.groups.forEach((entry, index) => {
        const row = document.createElement("tr");
        row.innerHTML = `
          <td><input value="${escapeHtml(entry.name)}" oninput="updateGroupDb(${index},'name',this.value)"></td>
          <td><input value="${entry.capacity}" oninput="updateGroupDb(${index},'capacity',toInt(this.value,0))"></td>
          <td><input value="${entry.radius_mm}" oninput="updateGroupDb(${index},'radius_mm',toFloat(this.value,0))"></td>
          <td><input value="${entry.z_mm}" oninput="updateGroupDb(${index},'z_mm',toFloat(this.value,0))"></td>
          <td><input value="${entry.phi_offset_deg}" oninput="updateGroupDb(${index},'phi_offset_deg',toFloat(this.value,0))"></td>
          <td><input value="${entry.half_opening_deg}" oninput="updateGroupDb(${index},'half_opening_deg',toFloat(this.value,0))"></td>
          <td><button onclick="deleteGroupPreset(${index})">Delete</button></td>`;
        tbody.appendChild(row);
      });
    }

    function renderBoardTypesTable() {
      const root = document.getElementById("boardTypesTable");
      root.innerHTML = "";
      (stateStore.boardConfigs.board_types || []).forEach((config, configIndex) => {
        const card = document.createElement("div");
        card.className = "group-card";
        card.style.marginBottom = "12px";
        const pinRows = (config.zap_pins || []).map((pin, pinIndex) => `
          <tr>
            <td>${escapeHtml(pin.id || "")}</td>
            <td id="boardPinLabel_${configIndex}_${pinIndex}">${escapeHtml(boardPinLabelFromData(pin, config))}</td>
            <td><input type="number" value="${pin.junction_aget ?? 0}" oninput="updateBoardPin(${configIndex},${pinIndex},'junction_aget',toInt(this.value,0))"></td>
            <td><input type="number" value="${pin.junction_first_channel ?? 0}" oninput="updateBoardPin(${configIndex},${pinIndex},'junction_first_channel',toInt(this.value,0))"></td>
            <td><input type="number" value="${pin.ohmic_aget ?? 0}" oninput="updateBoardPin(${configIndex},${pinIndex},'ohmic_aget',toInt(this.value,0))"></td>
            <td><input type="number" value="${pin.ohmic_first_channel ?? 0}" oninput="updateBoardPin(${configIndex},${pinIndex},'ohmic_first_channel',toInt(this.value,0))"></td>
          </tr>`).join("");
        card.innerHTML = `
          <strong>${escapeHtml(config.key || "")}</strong>
          <div class="form-grid two" style="margin-top:8px;">
            <div class="field">
              <label>Junction Strip Count</label>
              <input type="number" value="${config.junction_strip_count ?? 0}" oninput="updateBoardConfig(${configIndex},'junction_strip_count',toInt(this.value,0))">
            </div>
            <div class="field">
              <label>Ohmic Strip Count</label>
              <input type="number" value="${config.ohmic_strip_count ?? 0}" oninput="updateBoardConfig(${configIndex},'ohmic_strip_count',toInt(this.value,0))">
            </div>
          </div>
          <div style="overflow:auto; margin-top:8px;">
            <table class="table">
              <thead>
                <tr><th>Pin</th><th>Label</th><th>J AGET</th><th>J First ch</th><th>O AGET</th><th>O First ch</th></tr>
              </thead>
              <tbody>${pinRows}</tbody>
            </table>
          </div>`;
        root.appendChild(card);
      });
    }

    function updateDetectorDb(index, field, value) {
      stateStore.detectorDb.detectors[index][field] = value;
      debounceAutosave("detector_db", autosaveDetectorDbToCommon);
    }

    function updateGroupDb(index, field, value) {
      stateStore.groupDb.groups[index][field] = value;
      debounceAutosave("group_db", autosaveGroupDbToCommon);
    }

    function updateBoardConfig(index, field, value) {
      stateStore.boardConfigs.board_types[index][field] = value;
      debounceAutosave("board_types", autosaveBoardTypesToCommon);
    }

    function updateBoardPin(configIndex, pinIndex, field, value) {
      stateStore.boardConfigs.board_types[configIndex].zap_pins[pinIndex][field] = value;
      const config = stateStore.boardConfigs.board_types[configIndex];
      const pin = stateStore.boardConfigs.board_types[configIndex].zap_pins[pinIndex];
      pin.label = boardPinLabelFromData(pin, config);
      const labelCell = document.getElementById(`boardPinLabel_${configIndex}_${pinIndex}`);
      if (labelCell)
        labelCell.textContent = pin.label;
      debounceAutosave("board_types", autosaveBoardTypesToCommon);
    }

    function addGroupPreset() {
      stateStore.groupDb.groups.push({ name: "NEW", capacity: 0, radius_mm: 0, z_mm: 0, phi_offset_deg: 0, half_opening_deg: 10 });
      renderGroupDbTable();
      debounceAutosave("group_db", autosaveGroupDbToCommon);
    }

    function deleteGroupPreset(index) {
      stateStore.groupDb.groups.splice(index, 1);
      renderGroupDbTable();
      debounceAutosave("group_db", autosaveGroupDbToCommon);
    }

    async function saveLayout() {
      try {
        const path = promptPath("Save layout to file", stateStore.statePath || "si_mapping_layout.par");
        if (!path) return;
        const payload = await fetchJson("/api/save_state", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ state: stateStore.state, detector_db: stateStore.detectorDb, group_db: stateStore.groupDb, path }),
        });
        stateStore.statePath = payload.path;
        showBanner(`Saved layout to ${payload.path}`);
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function loadLayout() {
      try {
        const loaded = await pickOpenBundle(
          ["layout"],
          [{ description: "LILAK layout", accept: { "text/plain": [".conf", ".par", ".mac"] } }],
          "Load layout from file",
          stateStore.statePath || "si_mapping_layout.conf",
          "/api/load_state"
        );
        if (!loaded) return;
        const payload = loaded.parsed?.payload || loaded.payload;
        stateStore.state = payload.state;
        stateStore.detectorDb = payload.detector_db;
        stateStore.groupDb = payload.group_db;
        stateStore.statePath = loaded.name;
        stateStore.selectedExperimentId = stateStore.state.experiments[0]?.id || null;
        stateStore.selectedGroupId = stateStore.state.experiments[0]?.groups[0]?.id || null;
        stateStore.selectedNodeId = null;
        render();
        showBanner(`Loaded layout from ${loaded.name}`);
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function saveExperimentById(experimentId) {
      const experiment = stateStore.state.experiments.find(item => item.id === experimentId);
      if (!experiment) return;
      try {
        syncExperimentNodeLabels(experiment);
        const exportDir = await openNavigator({
          mode: "directory",
          title: "Choose Save Folder",
          confirmLabel: "Save",
          fileName: "",
          path: stateStore.lastExportDir || directoryOfPath(stateStore.lastExperimentPath) || stateStore.cwd || "",
        });
        if (exportDir === "__cancel__" || !exportDir)
          return;
        const result = await fetchJson("/api/save_experiment_bundle", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            experiment,
            detector_db: stateStore.detectorDb,
            group_db: stateStore.groupDb,
            board_configs: stateStore.boardConfigs,
            output_dir: exportDir,
          }),
        });
        stateStore.lastExportDir = exportDir;
        stateStore.lastExperimentPath = result.experiment_file || stateStore.lastExperimentPath;
        try {
          const payload = await fetchJson("/api/export", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              state: stateStore.state,
              detector_db: stateStore.detectorDb,
              group_db: stateStore.groupDb,
              board_configs: stateStore.boardConfigs,
              experiment_id: experimentId,
              output_dir: exportDir,
            }),
          });
          clearExportErrors();
          logStatus(
            `export_dir   : ${payload.export_dir}\ndetector_file: ${payload.detector_file}\nchannel_file : ${payload.channel_file}\nboard_file   : ${payload.board_file}\nexperiment   : ${payload.experiment_file}\n\ndetectors=${payload.detector_count}\nchannels=${payload.channel_count}`
          );
          showBanner(`Saved experiment configuration and mapping files\n${payload.export_dir}\n${payload.experiment_file}\n${payload.detector_file}\n${payload.channel_file}\n${payload.board_file}`);
          render(false);
        } catch (error) {
          applyExportError(error.payload || {});
          logStatus(error.message);
          showBanner(`Saved experiment configuration\n${result.experiment_file}\n${result.detector_db_file}\n${result.group_db_file}\n${result.board_types_file}\n\nMapping export failed\n${error.message}`, "error");
        }
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function saveExperiment() {
      await saveExperimentById(stateStore.selectedExperimentId);
    }

    async function exportMappingByExperiment(experimentId) {
      if (!experimentId) return;
      try {
        const experiment = stateStore.state.experiments.find(item => item.id === experimentId);
        if (!experiment)
          return;
        syncExperimentNodeLabels(experiment);
        const exportDir = await openNavigator({
          mode: "directory",
          title: "Choose Export Folder",
          confirmLabel: "Export",
          fileName: "",
          path: stateStore.lastExportDir || stateStore.cwd || "",
        });
        if (exportDir === "__cancel__" || !exportDir)
          return;
        const payload = await fetchJson("/api/export", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            state: stateStore.state,
            detector_db: stateStore.detectorDb,
            group_db: stateStore.groupDb,
            board_configs: stateStore.boardConfigs,
            experiment_id: experimentId,
            output_dir: exportDir,
          }),
        });
        stateStore.lastExportDir = exportDir;
        stateStore.lastExperimentPath = payload.experiment_file || stateStore.lastExperimentPath;
        clearExportErrors();
        logStatus(
          `export_dir   : ${payload.export_dir}\ndetector_file: ${payload.detector_file}\nchannel_file : ${payload.channel_file}\nboard_file   : ${payload.board_file}\nexperiment   : ${payload.experiment_file}\n\ndetectors=${payload.detector_count}\nchannels=${payload.channel_count}`
        );
        showBanner(`Saved experiment and mapping files\n${payload.export_dir}\n${payload.experiment_file}\n${payload.detector_file}\n${payload.channel_file}\n${payload.board_file}`);
        render(false);
      } catch (error) {
        applyExportError(error.payload || {});
        logStatus(error.message);
        showBanner(error.message, "error");
      }
    }

    async function saveExperimentAndMapping(experimentId) {
      if (!experimentId) return;
      await saveExperimentById(experimentId);
    }

    async function loadExperiment() {
      try {
        const loaded = await pickOpenBundle(
          ["experiment"],
          [{ description: "LILAK experiment", accept: { "text/plain": [".conf", ".par", ".mac"] } }],
          "Load experiment from file",
          "si_mapping_experiment.conf",
          "/api/load_experiment",
          stateStore.lastExperimentPath || stateStore.cwd || ""
        );
        if (!loaded) return;
        stateStore.lastExperimentPath = loaded.name || stateStore.lastExperimentPath;
        clearExportErrors();
        const payload = loaded.parsed?.payload || loaded.parsed || loaded.payload;
        const experiment = payload.experiment;
        if (!experiment) throw new Error("Experiment payload is empty.");
        const existingExperimentIds = new Set(stateStore.state.experiments.map(item => item.id));
        if (!experiment.id || existingExperimentIds.has(experiment.id))
          experiment.id = uid("exp");
        if (!Array.isArray(experiment.groups)) experiment.groups = [];
        if (experiment.groups.length === 0) {
          experiment.groups.push({ id: uid("group"), name: "Group 1", nodes: [], connections: [] });
        }
        syncExperimentNodeLabels(experiment);
        stateStore.state.experiments.push(experiment);
        if (payload.detector_db) stateStore.detectorDb = payload.detector_db;
        if (payload.group_db) stateStore.groupDb = payload.group_db;
        if (payload.board_configs) stateStore.boardConfigs = payload.board_configs;
        selectExperiment(experiment.id);
        if (payload.missing_bundle_files && payload.missing_bundle_files.length > 0)
          showBanner(`Loaded experiment from ${loaded.name}\nMissing bundle files: ${payload.missing_bundle_files.join(", ")}\nFallback: common/si_mapping`, "error");
        else
          showBanner(`Loaded experiment from ${loaded.name}`);
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function saveGroupById(groupId) {
      const group = currentExperiment()?.groups.find(item => item.id === groupId);
      if (!group) return;
      try {
        const payload = {
          group,
          detector_db: stateStore.detectorDb,
          group_db: stateStore.groupDb,
        };
        const result = await saveBundleWithPicker(
          "group",
          payload,
          `${group.name.replaceAll(" ", "_") || "group"}.conf`,
          "/api/save_group",
          stateStore.lastGroupPath || stateStore.cwd || ""
        );
        if (!result) return;
        stateStore.lastGroupPath = result.path || stateStore.lastGroupPath;
        showBanner(`Saved group to ${result.path}`);
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function saveGroup() {
      await saveGroupById(stateStore.selectedGroupId);
    }

    async function loadGroup() {
      const experiment = currentExperiment();
      if (!experiment) return;
      try {
        const loaded = await pickOpenBundle(
          ["group"],
          [{ description: "LILAK group", accept: { "text/plain": [".conf", ".par", ".mac"] } }],
          "Load group from file",
          "si_mapping_group.conf",
          "/api/load_group"
        );
        if (!loaded) return;
        const payload = loaded.parsed?.payload || loaded.parsed || loaded.payload;
        const group = payload.group;
        if (!group) throw new Error("Group payload is empty.");
        if (!group.id) group.id = uid("group");
        if (!Array.isArray(group.nodes)) group.nodes = [];
        if (!Array.isArray(group.connections)) group.connections = [];
        for (const node of group.nodes)
          syncNodeLabel(node);
        experiment.groups.push(group);
        if (payload.detector_db) stateStore.detectorDb = payload.detector_db;
        if (payload.group_db) stateStore.groupDb = payload.group_db;
        selectGroup(group.id);
        showBanner(`Loaded group from ${loaded.name}`);
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function saveDetectorDb() {
      try {
        await fetchJson("/api/save_detector_db", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ detector_db: stateStore.detectorDb }),
        });
        closeModal("detectorDbModal");
        render(false);
        showBanner("Saved detector database");
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function saveGroupDb() {
      try {
        const result = await saveBundleWithPicker(
          "group_db",
          { group_db: stateStore.groupDb },
          "si_group_db.conf",
          "/api/save_group_db",
          stateStore.lastGroupDbPath || stateStore.commonMappingDir || stateStore.cwd || ""
        );
        if (!result) return;
        stateStore.lastGroupDbPath = result.path || stateStore.lastGroupDbPath;
        closeModal("groupDbModal");
        render(false);
        showBanner(`Saved group DB to ${result.path}`);
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function saveBoardTypesDb() {
      try {
        const result = await saveBundleWithPicker(
          "board types",
          { board_configs: stateStore.boardConfigs },
          "board_types.conf",
          "/api/save_board_configs",
          stateStore.commonMappingDir || stateStore.cwd || ""
        );
        if (!result) return;
        closeModal("boardTypesModal");
        showBanner(`Saved board types to ${result.path}`);
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function loadBoardTypesDb() {
      try {
        const loaded = await pickOpenBundle(
          ["board_types"],
          [{ description: "Board types", accept: { "text/plain": [".conf", ".par", ".mac"] } }],
          "Load board types",
          "board_types.conf",
          "/api/load_board_configs",
          stateStore.commonMappingDir || stateStore.cwd || ""
        );
        if (!loaded) return;
        const payload = loaded.parsed?.payload || loaded.payload;
        stateStore.boardConfigs = payload.board_configs || { board_types: [] };
        renderBoardButtons();
        renderBoardTypesTable();
        showBanner(`Loaded board types from ${loaded.name || payload.path}`);
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function loadGroupDb() {
      try {
        const loaded = await pickOpenBundle(
          ["group_db"],
          [{ description: "LILAK group DB", accept: { "text/plain": [".conf", ".par", ".mac"] } }],
          "Load group DB from file",
          "si_group_db.conf",
          "/api/load_group_db",
          stateStore.lastGroupDbPath || stateStore.commonMappingDir || stateStore.cwd || ""
        );
        if (!loaded) return;
        stateStore.lastGroupDbPath = loaded.name || stateStore.lastGroupDbPath;
        const result = loaded.parsed?.payload ? { path: loaded.name, group_db: loaded.parsed.payload.group_db } : loaded.parsed || loaded.payload;
        stateStore.groupDb = result.group_db;
        renderGroupDbTable();
        render(false);
        showBanner(`Loaded group DB from ${loaded.name || result.path}`);
      } catch (error) {
        showBanner(error.message, "error");
      }
    }

    async function saveMapping() {
      await exportMappingByExperiment(stateStore.selectedExperimentId);
    }

    function render(heavy=true) {
      renderTabs();
      renderSummary();
      renderInspector();
      const showAllZapsBtn = document.getElementById("showAllZapsBtn");
      const showAllCobosBtn = document.getElementById("showAllCobosBtn");
      const showAllDetectorsBtn = document.getElementById("showAllDetectorsBtn");
      const boardViewBtn = document.getElementById("boardViewBtn");
      const geomViewBtn = document.getElementById("geomViewBtn");
      const canvasTitle = document.getElementById("canvasTitle");
      if (canvasTitle)
        canvasTitle.textContent = stateStore.geomViewMode ? "Board Canvas (View From Upstream)" : "Board Canvas";
      if (showAllZapsBtn) {
        showAllZapsBtn.textContent = "Show all ZAPs";
        showAllZapsBtn.classList.toggle("active-toggle", stateStore.showAllZaps);
      }
      if (showAllCobosBtn) {
        showAllCobosBtn.textContent = "Show all Cobos";
        showAllCobosBtn.classList.toggle("active-toggle", stateStore.showAllCobos);
      }
      if (showAllDetectorsBtn) {
        showAllDetectorsBtn.textContent = "Show all Detectors";
        showAllDetectorsBtn.classList.toggle("active-toggle", stateStore.showAllDetectors);
      }
      if (boardViewBtn) {
        boardViewBtn.textContent = "Board View";
        boardViewBtn.classList.toggle("active-toggle", stateStore.boardViewMode);
      }
      if (geomViewBtn) {
        geomViewBtn.textContent = "Upst. View";
        geomViewBtn.classList.toggle("active-toggle", stateStore.geomViewMode);
      }
      if (heavy) renderCanvas();
      else {
        renderInspector();
        renderSummary();
        renderCanvas();
      }
    }

    async function bootstrap() {
      const payload = await fetchJson("/api/bootstrap");
      stateStore.state = payload.state;
      stateStore.detectorDb = payload.detector_db;
      stateStore.groupDb = payload.group_db;
      stateStore.boardConfigs = payload.board_configs || { board_types: [] };
      stateStore.statePath = payload.state_path;
      stateStore.cwd = payload.cwd || "";
      stateStore.commonMappingDir = payload.lilak_mapping_dir || "";
      if (stateStore.state.experiments.length === 0) {
        stateStore.state.experiments.push({
          id: uid("exp"),
          name: "Experiment 1",
          groups: [{ id: uid("group"), name: "Group 1", nodes: [], connections: [] }],
          placeholders: [],
          loose_nodes: [],
        });
      }
      stateStore.state.experiments.forEach(experiment => {
        ensureExperimentCollections(experiment);
        if (!Array.isArray(experiment.groups))
          experiment.groups = [];
        if (experiment.groups.length === 0) {
          experiment.groups.push({ id: uid("group"), name: "Group 1", nodes: [], connections: [] });
        }
        experiment.groups.forEach(group => {
          if (!Array.isArray(group.nodes))
            group.nodes = [];
          if (!Array.isArray(group.connections))
            group.connections = [];
        });
        ensureExperimentBoardConnections(experiment);
        syncExperimentNodeLabels(experiment);
      });
      stateStore.selectedExperimentId = stateStore.state.experiments[0].id;
      const firstGroup = stateStore.state.experiments[0].groups[0];
      stateStore.selectedGroupId = firstGroup ? firstGroup.id : null;
      render();
      if (payload.state_path) {
        showBanner(`Loaded ${payload.state_path}`);
      }
    }

    window.addEventListener("mousemove", onMouseMove);
    window.addEventListener("mouseup", onMouseUp);
    window.addEventListener("keydown", onKeyDown);
    window.addEventListener("DOMContentLoaded", bootstrap);
  </script>
</body>
</html>
"""

HTML_PAGE = rebuild_legacy_page(HTML_PAGE, "LILAK Silicon Mapping Editor")


def main():
    load_detector_db_file()
    load_rings_db_file()
    load_board_configs_file()
    if not group_db_path().exists():
        save_parameter_file(group_db_path(), "group_db", {"group_db": DEFAULT_GROUP_DB})

    parser = argparse.ArgumentParser(description="Open the LILAK silicon mapping editor.")
    parser.add_argument("file", nargs="?", help="Optional saved si-mapping layout file")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()

    initial_path = resolve_initial_input(args.file)

    port = args.port or pick_port()
    server = SiMappingServer((args.host, port), initial_path)
    url = f"http://{args.host}:{port}/"

    if initial_path is None:
        print("Opening empty silicon mapping editor")
    else:
        print(f"Opening {initial_path}")
    print(f"Silicon mapping editor: {url}")
    print("Press Ctrl-C to stop the server.")

    if not args.no_browser:
      webbrowser.open(url)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping silicon mapping editor.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
