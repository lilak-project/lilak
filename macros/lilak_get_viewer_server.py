#!/usr/bin/env python3
import argparse
import bisect
import fcntl
import json
import math
import mmap
import os
import posixpath
import re
import shlex
import shutil
import statistics
import struct
import subprocess
import tempfile
import threading
import urllib.parse
import webbrowser
from dataclasses import dataclass, field
from email import policy
from email.parser import BytesParser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from io import BytesIO
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple


UPLOAD_ROOT = Path("/tmp/lk_get_viewer_uploads")
FPN_CHANNELS = {11, 22, 45, 56}


class DetectorMapping:
    def __init__(self, mapping_path: str) -> None:
        supplied = Path(mapping_path).expanduser().resolve()
        self.directory = supplied if supplied.is_dir() else supplied.parent
        channel_file = self.directory / "channel_mapping.txt"
        detector_file = self.directory / "detector_mapping.txt"
        if not channel_file.is_file() or not detector_file.is_file():
            raise FileNotFoundError(
                f"mapping requires channel_mapping.txt and detector_mapping.txt in {self.directory}"
            )

        detectors = {}
        for row in self._read_table(detector_file, "det_type"):
            det_idx = int(row["det_idx"])
            detectors[det_idx] = {
                "detectorType": row["det_type"],
                "detectorNumber": int(row["det_number"]),
                "detectorLabel": f"{row['det_type']}-{row['det_number']}",
                "ringType": row["ring_type"],
            }

        self.channels = {}
        for row in self._read_table(channel_file, "ch_idx"):
            key = tuple(int(row[name]) for name in ("cobo", "asad", "aget", "ch"))
            detector = detectors.get(int(row["det_idx"]))
            if detector is not None:
                self.channels[key] = {
                    **detector,
                    "strip": int(row["strip"]),
                }
        metadata = list(self.channels.values())
        self.options = {
            "detectorTypes": sorted({item["detectorType"] for item in metadata}),
            "detectorNumbers": sorted({item["detectorNumber"] for item in metadata}),
            "ringTypes": sorted({item["ringType"] for item in metadata}),
        }

    @staticmethod
    def _read_table(path: Path, first_column: str) -> Iterable[dict]:
        lines = path.read_text().splitlines()
        header_index = next(
            (index for index, line in enumerate(lines) if line.split("\t", 1)[0] == first_column),
            None,
        )
        if header_index is None:
            raise ParseError(f"table header not found in {path}")
        columns = lines[header_index].split("\t")
        for line in lines[header_index + 1 :]:
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            values = line.split("\t")
            if len(values) == len(columns):
                yield dict(zip(columns, values))


MAPPING: Optional[DetectorMapping] = None
BROWSE_START_PATH = str(Path.cwd().resolve())


def read_multipart_file(headers, stream) -> Tuple[str, BytesIO]:
    content_type = headers.get("Content-Type", "")
    try:
        content_length = int(headers.get("Content-Length", "0"))
    except ValueError as exc:
        raise RuntimeError("invalid upload content length") from exc
    if content_length <= 0:
        raise RuntimeError("upload body is empty")

    body = stream.read(content_length)
    if len(body) != content_length:
        raise RuntimeError("upload body is cut off")

    mime_headers = (
        f"Content-Type: {content_type}\r\n"
        "MIME-Version: 1.0\r\n\r\n"
    ).encode("iso-8859-1")
    message = BytesParser(policy=policy.default).parsebytes(mime_headers + body)
    if message.get_content_type() != "multipart/form-data" or not message.is_multipart():
        raise RuntimeError("upload requires multipart/form-data with a boundary")

    for part in message.walk():
        if part.is_multipart():
            continue
        if part.get_content_disposition() != "form-data":
            continue
        if part.get_param("name", header="content-disposition") != "file":
            continue
        filename = part.get_filename() or ""
        if not filename:
            raise RuntimeError("upload field 'file' has no filename")
        content = part.get_payload(decode=True)
        if content is None:
            raise RuntimeError("upload field 'file' has no content")
        return filename, BytesIO(content)

    raise RuntimeError("upload field 'file' is missing")


class ParseError(Exception):
    pass


class TruncatedFrameError(ParseError):
    """The file ends before the frame it announces. Indexing stops here."""


def read_be16(data: bytes, offset: int = 0) -> int:
    return (data[offset] << 8) | data[offset + 1]


def read_be24(data: bytes, offset: int = 0) -> int:
    return (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]


def read_be32(data: bytes, offset: int = 0) -> int:
    return (
        (data[offset] << 24)
        | (data[offset + 1] << 16)
        | (data[offset + 2] << 8)
        | data[offset + 3]
    )


def read_be48(data: bytes, offset: int = 0) -> int:
    value = 0
    for index in range(6):
        value = (value << 8) | data[offset + index]
    return value


def read_le16(data: bytes, offset: int = 0) -> int:
    return (data[offset + 1] << 8) | data[offset]


def read_le24(data: bytes, offset: int = 0) -> int:
    return (data[offset + 2] << 16) | (data[offset + 1] << 8) | data[offset]


def read_le32(data: bytes, offset: int = 0) -> int:
    return (
        (data[offset + 3] << 24)
        | (data[offset + 2] << 16)
        | (data[offset + 1] << 8)
        | data[offset]
    )


BLOB_FRAME_TYPES = (0x0007, 0x0008, 0xFF11)
LAYERED_FRAME_TYPES = (0xFF01, 0xFF02)
BIG_ENDIAN_FRAME_TYPES = (0x0001, 0x0002, 0x0007, 0x0008, 0xFF01, 0xFF02)
LITTLE_ENDIAN_FRAME_TYPES = BIG_ENDIAN_FRAME_TYPES + (0xFF11,)


def resolve_frame_type(data: bytes, offset: int = 0) -> int:
    """MFM writes some frame types little endian; mirror LKGETFrameParser."""
    type_be = read_be16(data, offset)
    if type_be in BIG_ENDIAN_FRAME_TYPES:
        return type_be
    type_le = read_le16(data, offset)
    if type_le in LITTLE_ENDIAN_FRAME_TYPES:
        return type_le
    return type_be


def layered_is_little_endian(data: bytes, offset: int, block_size: int) -> bool:
    """Choose the byte order shared by a layered frame's primary/secondary headers."""
    if len(data) < offset + 20:
        raise ParseError("layered frame header shorter than 20 bytes")

    candidates = []
    for little_endian in (False, True):
        byte_order = "little" if little_endian else "big"
        frame_size = int.from_bytes(data[offset + 1 : offset + 4], byte_order) * block_size
        header_size = int.from_bytes(data[offset + 8 : offset + 10], byte_order) * block_size
        item_count = int.from_bytes(data[offset + 12 : offset + 16], byte_order)
        valid = (
            20 <= header_size <= frame_size
            and item_count <= (frame_size - header_size) // 8
        )
        candidates.append((valid, header_size, little_endian))

    if candidates[0][0] != candidates[1][0]:
        return candidates[1][0]
    if candidates[0][0] and candidates[0][1] != candidates[1][1]:
        return candidates[1][1] < candidates[0][1]
    return False


def frame_size_of(data: bytes, offset: int, block_size: int, frame_type: int) -> int:
    # The file-header blob is little endian. Layered frames exist in both byte
    # orders, while basic GET frames remain big endian.
    if frame_type == 0xFF11:
        return read_le24(data, offset) * block_size
    if frame_type in LAYERED_FRAME_TYPES:
        frame_offset = offset - 1
        if layered_is_little_endian(data, frame_offset, block_size):
            return read_le24(data, offset) * block_size
    return read_be24(data, offset) * block_size


@dataclass
class RawFrame:
    meta_type: int = 0
    data_source: int = 0
    frame_type: int = 0
    revision: int = 0
    block_size: int = 1
    frame_size_bytes: int = 0
    header_size_bytes: int = 8
    item_size_bytes: int = 0
    item_count: int = 0
    event_time: int = 0
    event_idx: int = 0
    cobo_idx: int = 0
    asad_idx: int = 0
    is_blob: bool = False
    is_layered: bool = False
    file_offset: int = 0
    file_end: int = 0
    data: bytes = b""
    children: List["RawFrame"] = field(default_factory=list)

    @property
    def payload(self) -> bytes:
        if self.header_size_bytes >= len(self.data):
            return b""
        return self.data[self.header_size_bytes :]


class FrameParser:
    def index_next_frame(self, handle) -> Optional[RawFrame]:
        """Read only the frame header and seek past its payload."""
        offset = handle.tell()
        header = handle.read(28)
        if len(header) == 0:
            return None
        if len(header) < 8:
            raise ParseError(f"short frame header at byte {offset}")

        p2_block = header[0] & 0x0F
        block_size = 1 if p2_block == 0 else 1 << p2_block
        frame_type = resolve_frame_type(header, 5)
        frame_size = frame_size_of(header, 1, block_size, frame_type)
        if frame_size < 8:
            raise ParseError(f"invalid frame size {frame_size} at byte {offset}")
        file_size = os.fstat(handle.fileno()).st_size
        if offset + frame_size > file_size:
            raise TruncatedFrameError(
                f"frame at byte {offset} needs {frame_size} bytes but only "
                f"{file_size - offset} remain"
            )

        frame = RawFrame(
            meta_type=header[0],
            data_source=header[4],
            frame_type=frame_type,
            revision=header[7],
            block_size=block_size,
            frame_size_bytes=frame_size,
            is_blob=frame_type in BLOB_FRAME_TYPES,
            is_layered=frame_type in LAYERED_FRAME_TYPES,
            file_offset=offset,
            file_end=offset + frame_size,
        )
        if frame.is_layered and len(header) >= 20:
            if layered_is_little_endian(header, 0, block_size):
                frame.event_idx = read_le32(header, 16)
            else:
                frame.event_idx = read_be32(header, 16)
        elif frame_type in (1, 2) and len(header) >= 26:
            frame.event_idx = read_be32(header, 22)
            frame.cobo_idx = header[26] if len(header) > 26 else 0
            frame.asad_idx = header[27] if len(header) > 27 else 0
        handle.seek(offset + frame_size)
        return frame

    def read_next_frame(self, handle) -> Optional[RawFrame]:
        offset = handle.tell()
        header = handle.read(8)
        if len(header) == 0:
            return None
        if len(header) != 8:
            raise ParseError(f"short frame header at byte {offset}")

        frame_type = resolve_frame_type(header, 5)
        if frame_type in LAYERED_FRAME_TYPES:
            secondary_header = handle.read(12)
            if len(secondary_header) != 12:
                raise ParseError(f"layered frame header is cut off at byte {offset}")
            header += secondary_header

        p2_block = header[0] & 0x0F
        block_size = 1 if p2_block == 0 else 1 << p2_block
        frame_size = frame_size_of(header, 1, block_size, frame_type)
        if frame_size < len(header):
            raise ParseError(f"invalid frame size {frame_size} at byte {offset}")

        rest = handle.read(frame_size - len(header))
        if len(rest) != frame_size - len(header):
            raise TruncatedFrameError(f"frame at byte {offset} is cut off at end of file")

        frame = self.parse_frame_bytes(header + rest, offset)
        frame.file_end = handle.tell()
        return frame

    def parse_frame_bytes(self, data: bytes, file_offset: int = 0) -> RawFrame:
        if len(data) < 8:
            raise ParseError("frame shorter than 8 bytes")

        p2_block = data[0] & 0x0F
        block_size = 1 if p2_block == 0 else 1 << p2_block
        frame_type = resolve_frame_type(data, 5)
        frame_size = frame_size_of(data, 1, block_size, frame_type)

        frame = RawFrame(
            meta_type=data[0],
            data_source=data[4],
            frame_type=frame_type,
            revision=data[7],
            block_size=block_size,
            frame_size_bytes=frame_size if frame_size == len(data) else len(data),
            is_blob=frame_type in BLOB_FRAME_TYPES,
            is_layered=frame_type in LAYERED_FRAME_TYPES,
            file_offset=file_offset,
            file_end=file_offset + len(data),
            data=data,
        )

        if frame.is_layered:
            if len(data) < 20:
                raise ParseError(f"layered frame too short at byte {file_offset}")
            if layered_is_little_endian(data, 0, block_size):
                frame.header_size_bytes = read_le16(data, 8) * block_size
                frame.item_size_bytes = read_le16(data, 10)
                frame.item_count = read_le32(data, 12)
                frame.event_idx = read_le32(data, 16)
            else:
                frame.header_size_bytes = read_be16(data, 8) * block_size
                frame.item_size_bytes = read_be16(data, 10)
                frame.item_count = read_be32(data, 12)
                frame.event_idx = read_be32(data, 16)

            offset = frame.header_size_bytes
            for _ in range(frame.item_count):
                if offset + 8 > len(data):
                    raise ParseError(f"child frame header outside parent at byte {file_offset + offset}")
                child_p2 = data[offset] & 0x0F
                child_block = 1 if child_p2 == 0 else 1 << child_p2
                child_type = resolve_frame_type(data, offset + 5)
                child_size = frame_size_of(data, offset + 1, child_block, child_type)
                if child_size < 8 or offset + child_size > len(data):
                    raise ParseError(f"child frame outside parent at byte {file_offset + offset}")
                child = self.parse_frame_bytes(data[offset : offset + child_size], file_offset + offset)
                frame.children.append(child)
                offset += child_size
            return frame

        if frame.frame_type in (1, 2):
            if len(data) < 88:
                raise ParseError(f"GET frame too short at byte {file_offset}")
            frame.header_size_bytes = read_be16(data, 8) * block_size
            frame.item_size_bytes = read_be16(data, 10)
            frame.item_count = read_be32(data, 12)
            frame.event_time = read_be48(data, 16)
            frame.event_idx = read_be32(data, 22)
            frame.cobo_idx = data[26]
            frame.asad_idx = data[27]

        return frame


def iter_data_frames(frame: RawFrame) -> Iterable[RawFrame]:
    if frame.is_layered:
        for child in frame.children:
            yield from iter_data_frames(child)
        return
    if frame.frame_type in (1, 2) and frame.item_count > 0:
        yield frame


def blank_waveform() -> List[int]:
    return [0] * 512


def channel_key(cobo: int, asad: int, aget: int, chan: int) -> Tuple[int, int, int, int]:
    return (int(cobo), int(asad), int(aget), int(chan))


def put_sample(channels: Dict[Tuple[int, int, int, int], List[int]], key, bucket: int, sample: int) -> None:
    if bucket < 0 or bucket >= 512:
        return
    if key not in channels:
        channels[key] = blank_waveform()
    channels[key][bucket] = int(sample)


def unpack_get_event(frame: RawFrame) -> Dict[Tuple[int, int, int, int], List[int]]:
    channels: Dict[Tuple[int, int, int, int], List[int]] = {}
    for data_frame in iter_data_frames(frame):
        if data_frame.item_size_bytes <= 0:
            continue

        payload = data_frame.payload
        max_items = min(data_frame.item_count, len(payload) // data_frame.item_size_bytes)

        if data_frame.frame_type == 1:
            if data_frame.item_size_bytes < 4:
                continue
            for item_index in range(max_items):
                start = item_index * data_frame.item_size_bytes
                word = read_be32(payload, start)
                aget = (word >> 30) & 0x3
                chan = (word >> 23) & 0x7F
                bucket = (word >> 14) & 0x1FF
                sample = word & 0xFFF
                if chan > 67:
                    continue
                key = channel_key(data_frame.cobo_idx, data_frame.asad_idx, aget, chan)
                put_sample(channels, key, bucket, sample)

        elif data_frame.frame_type == 2:
            if data_frame.item_size_bytes < 2:
                continue
            chan_index = [0, 0, 0, 0]
            bucket_index = [0, 0, 0, 0]
            for item_index in range(max_items):
                start = item_index * data_frame.item_size_bytes
                word = read_be16(payload, start)
                aget = (word >> 14) & 0x3
                if chan_index[aget] >= 68:
                    chan_index[aget] = 0
                    bucket_index[aget] += 1
                if bucket_index[aget] >= 512:
                    continue
                key = channel_key(data_frame.cobo_idx, data_frame.asad_idx, aget, chan_index[aget])
                put_sample(channels, key, bucket_index[aget], word & 0xFFF)
                chan_index[aget] += 1

    return channels


def analyze_waveform(values: List[int]) -> Dict[str, float]:
    if not values:
        return {"pedestal": 0.0, "min": 0, "max": 0, "amplitude": 0.0, "peakTb": 0}

    baseline = values[: min(64, len(values))]
    pedestal = float(statistics.median(baseline)) if baseline else 0.0
    min_value = min(values)
    max_value = max(values)
    pos_amp = max_value - pedestal
    neg_amp = pedestal - min_value
    if neg_amp > pos_amp:
        peak_tb = min(range(len(values)), key=lambda index: values[index])
        amplitude = neg_amp
    else:
        peak_tb = max(range(len(values)), key=lambda index: values[index])
        amplitude = pos_amp
    return {
        "pedestal": round(pedestal, 3),
        "min": int(min_value),
        "max": int(max_value),
        "amplitude": round(float(amplitude), 3),
        "peakTb": int(peak_tb),
    }


def normalize_selection(payload: Optional[dict]) -> dict:
    payload = payload or {}
    selection = {}
    for key in ("cobo", "asad", "aget", "channel"):
        value = payload.get(key)
        if value in (None, "", "all", "Any", "any"):
            selection[key] = None
            continue
        selection[key] = int(value)
    return selection


def parse_number_expression(value, field: str):
    if value in (None, "", "all", "Any", "any"):
        return None
    text = str(value).strip()
    if not text or text.lower() in ("all", "any"):
        return None
    included = set()
    excluded = set()
    for raw_token in text.split(","):
        token = raw_token.strip()
        if not token:
            raise ValueError(f"empty item in {field} filter")
        is_excluded = token.startswith("!")
        body = token[1:].strip() if is_excluded else token
        match = re.fullmatch(r"(\d+)(?:\s*-\s*(\d+))?", body)
        if match is None:
            raise ValueError(f"invalid {field} filter item: {token}")
        start = int(match.group(1))
        end = int(match.group(2)) if match.group(2) is not None else start
        if end < start:
            raise ValueError(f"invalid descending range in {field}: {token}")
        if end - start > 10000:
            raise ValueError(f"range is too large in {field}: {token}")
        target = excluded if is_excluded else included
        target.update(range(start, end + 1))
    return {
        "include": sorted(included) if included else None,
        "exclude": sorted(excluded),
    }


def normalize_scan_selection(payload: Optional[dict]) -> dict:
    payload = payload or {}
    return {
        key: parse_number_expression(payload.get(key), key)
        for key in ("cobo", "asad", "aget", "channel")
    }


def normalize_detector_selection(payload: Optional[dict]) -> dict:
    payload = payload or {}
    detector_type = payload.get("detectorType")
    detector_number = payload.get("detectorNumber")
    ring_type = payload.get("ringType")
    return {
        "detectorType": None if detector_type in (None, "", "Any", "any") else str(detector_type),
        "detectorNumber": (None if detector_number in (None, "", "Any", "any")
                           else int(detector_number)),
        "ringType": None if ring_type in (None, "", "Any", "any") else str(ring_type),
    }


def channel_matches_detector(key: Tuple[int, int, int, int], selection: dict) -> bool:
    if not any(value is not None for value in selection.values()):
        return True
    if MAPPING is None:
        return False
    metadata = MAPPING.channels.get(key)
    if metadata is None:
        return False
    return all(
        selection.get(field) is None or metadata.get(field) == selection[field]
        for field in ("detectorType", "detectorNumber", "ringType")
    )


def channel_matches_tuple(key: Tuple[int, int, int, int], selection: dict) -> bool:
    def matches(value: int, constraint) -> bool:
        if constraint is None:
            return True
        if isinstance(constraint, dict):
            included = constraint.get("include")
            excluded = constraint.get("exclude") or []
            return (included is None or value in included) and value not in excluded
        return value == constraint

    return all(
        matches(value, selection.get(field))
        for field, value in zip(("cobo", "asad", "aget", "channel"), key)
    )


def scope_selection(selection: dict, scope: str) -> dict:
    scoped = {key: None for key in ("cobo", "asad", "aget", "channel")}
    if scope in ("cobo", "asad", "aget", "channel", "selection"):
        scoped["cobo"] = selection.get("cobo")
    if scope in ("asad", "aget", "channel", "selection"):
        scoped["asad"] = selection.get("asad")
    if scope in ("aget", "channel", "selection"):
        scoped["aget"] = selection.get("aget")
    if scope in ("channel", "selection"):
        scoped["channel"] = selection.get("channel")
    return scoped


def list_directory(path: str) -> dict:
    target = Path(path or os.getcwd()).expanduser().resolve()
    if target.is_file():
        target = target.parent
    if not target.is_dir():
        raise NotADirectoryError(str(target))

    event_ranges = {}
    for ranges_file in target.glob("*" + MFM_RANGES_SUFFIX):
        metadata = load_valid_mfm_ranges(ranges_file)
        if metadata is None:
            continue
        for source in metadata["sources"]:
            event_ranges[str((target / source["name"]).resolve())] = {
                "minEvent": source.get("minEvent"),
                "maxEvent": source.get("maxEvent"),
                "frameCount": source.get("frameCount"),
            }

    entries = []
    for child in target.iterdir():
        if (child.name.endswith(MFM_INDEX_SUFFIX)
                or child.name.endswith(MFM_RANGES_SUFFIX)
                or child.name.endswith(MFM_INDEX_SUFFIX + ".lock")
                or (child.name.startswith(".") and ".tmp-" in child.name)):
            continue
        try:
            is_directory = child.is_dir()
            stat = child.stat()
        except OSError:
            continue
        resolved = str(child.resolve())
        entries.append({
            "name": child.name,
            "path": resolved,
            "isDirectory": is_directory,
            "size": 0 if is_directory else stat.st_size,
            **event_ranges.get(resolved, {}),
        })
    entries.sort(key=lambda item: (
        not item["isDirectory"],
        tuple(int(part) if part.isdigit() else part.lower()
              for part in re.split(r"(\d+)", item["name"])),
    ))
    parent = str(target.parent) if target.parent != target else None
    return {"path": str(target), "parent": parent, "entries": entries}


def root_reader_environment(root_config: Path) -> dict:
    environment = os.environ.copy()
    root_library = str(root_config.parent.parent / "lib")
    lilak_root = Path(__file__).resolve().parent.parent
    if not (lilak_root / "build" / "libLILAK.so").is_file():
        lilak_root = Path(environment.get("LILAK_PATH", "/root/lilak")).resolve()
    lilak_library = str(lilak_root / "build")
    inherited = environment.get("LD_LIBRARY_PATH", "")
    environment["LD_LIBRARY_PATH"] = ":".join(
        item for item in (root_library, lilak_library, inherited) if item
    )
    return environment


def ensure_root_reader() -> Tuple[Path, Path]:
    source = Path(__file__).resolve().with_name("lilak_get_viewer_root_reader.cpp")
    if not source.is_file():
        raise RuntimeError(f"ROOT reader source is missing: {source}")
    candidates = sorted(Path("/root/opt").glob("root-*/bin/root-config"), reverse=True)
    system_root_config = shutil.which("root-config")
    if system_root_config:
        candidates.append(Path(system_root_config))
    if not candidates:
        raise RuntimeError("ROOT root-config was not found")
    root_config = candidates[0]
    binary = Path(f"/tmp/lilak_get_viewer_root_reader_{os.getuid()}")
    lilak_root = Path(__file__).resolve().parent.parent
    if not (lilak_root / "build" / "libLILAK.so").is_file():
        lilak_root = Path(os.environ.get("LILAK_PATH", "/root/lilak")).resolve()
    lilak_library = lilak_root / "build" / "libLILAK.so"
    rebuild = (not binary.is_file()
               or binary.stat().st_mtime < source.stat().st_mtime
               or (lilak_library.is_file()
                   and binary.stat().st_mtime < lilak_library.stat().st_mtime))
    if rebuild:
        cflags = shlex.split(subprocess.check_output(
            [str(root_config), "--cflags"], text=True
        ))
        libraries = shlex.split(subprocess.check_output(
            [str(root_config), "--libs"], text=True
        ))
        include_directories = [
            lilak_root / "source" / "container",
            lilak_root / "source" / "container" / "readout",
            lilak_root / "source" / "container" / "measurement",
            lilak_root / "source" / "base",
            lilak_root / "source" / "tool",
            lilak_root / "source" / "geometry",
        ]
        command = ["g++", "-O2", *cflags]
        command.extend(f"-I{directory}" for directory in include_directories)
        command.extend([
            str(source), "-o", str(binary), f"-L{lilak_root / 'build'}", "-lLILAK",
            *libraries, f"-Wl,-rpath,{root_config.parent.parent / 'lib'}",
            f"-Wl,-rpath,{lilak_root / 'build'}",
        ])
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode != 0:
            detail = (result.stderr or result.stdout).strip()
            raise RuntimeError(f"failed to build ROOT reader: {detail[-2000:]}")
    return binary, root_config


def ensure_raw_indexer() -> Path:
    source = Path(__file__).resolve().with_name("lilak_get_viewer_raw_indexer.cpp")
    if not source.is_file():
        raise RuntimeError(f"raw indexer source is missing: {source}")
    lilak_root = Path(__file__).resolve().parent.parent
    header = lilak_root / "source" / "tool" / "get_convter" / "LKGETOfflineFrameIndex.h"
    if not header.is_file():
        lilak_root = Path(os.environ.get("LILAK_PATH", "/root/lilak")).resolve()
        header = lilak_root / "source" / "tool" / "get_convter" / "LKGETOfflineFrameIndex.h"
    binary = Path(f"/tmp/lilak_get_viewer_raw_indexer_{os.getuid()}")
    rebuild = (not binary.is_file()
               or binary.stat().st_mtime < source.stat().st_mtime
               or binary.stat().st_mtime < header.stat().st_mtime)
    if rebuild:
        command = [
            "g++", "-std=c++11", "-O2", f"-I{header.parent}",
            str(source), "-o", str(binary),
        ]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode != 0:
            detail = (result.stderr or result.stdout).strip()
            raise RuntimeError(f"failed to build raw indexer: {detail[-2000:]}")
    return binary


MFM_INDEX_SUFFIX = ".mfm_event_index"
MFM_RANGES_SUFFIX = ".mfm_event_ranges.json"


def natural_name_key(path: Path) -> tuple:
    return tuple(
        int(part) if part.isdigit() else part.lower()
        for part in re.split(r"(\d+)", path.name)
    )


def mfm_family(path: str) -> Tuple[Path, List[Path]]:
    selected = Path(path).resolve()
    numbered = re.fullmatch(r"(.+)\.(\d+)", selected.name)
    candidate = selected.with_name(numbered.group(1)) if numbered else selected
    base = candidate if candidate.is_file() else selected
    members = [base] if base.is_file() else []
    members.extend(
        child for child in base.parent.glob(base.name + ".*")
        if child.is_file() and re.fullmatch(re.escape(base.name) + r"\.\d+", child.name)
    )
    return base, sorted({item.resolve() for item in members}, key=natural_name_key)


def persistent_mfm_cache(paths: List[str]) -> Optional[Tuple[Path, List[Path], Path, Path]]:
    if not paths:
        return None
    base, members = mfm_family(paths[0])
    selected = {Path(path).resolve() for path in paths}
    if not members or selected != set(members):
        return None
    for path in paths[1:]:
        other_base, _other_members = mfm_family(path)
        if other_base != base:
            return None
    index_path = Path(str(base) + MFM_INDEX_SUFFIX)
    ranges_path = Path(str(base) + MFM_RANGES_SUFFIX)
    return base, members, index_path, ranges_path


def source_stat(path: Path) -> dict:
    stat = path.stat()
    return {"name": path.name, "size": stat.st_size, "mtimeNs": stat.st_mtime_ns}


def load_valid_mfm_ranges(path: Path, expected_paths: Optional[List[Path]] = None) -> Optional[dict]:
    try:
        payload = json.loads(path.read_text())
        if payload.get("format") != "lilak-mfm-event-ranges" or payload.get("version") != 1:
            return None
        directory = path.parent
        sources = payload["sources"]
        if expected_paths is not None and [item["name"] for item in sources] != [p.name for p in expected_paths]:
            return None
        for item in sources:
            source = directory / item["name"]
            current = source_stat(source)
            if current["size"] != item["size"] or current["mtimeNs"] != item["mtimeNs"]:
                return None
        return payload
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError):
        return None


class CompactRawIndex:
    HEADER = struct.Struct("<8sIIQQQQQQQ")
    SOURCE = struct.Struct("<QQ")
    FRAME = struct.Struct("<QII")
    EVENT = struct.Struct("<QII")
    AUXILIARY = struct.Struct("<QII")

    def __init__(self, paths: List[str]) -> None:
        self.path = None
        self.handle = None
        self.mapping = None
        self.temporary = True
        resolved_paths = [Path(path).resolve() for path in paths]
        cache = persistent_mfm_cache(paths)
        try:
            if cache is None:
                descriptor, index_path = tempfile.mkstemp(
                    prefix="lilak_get_viewer_index_", suffix=".bin"
                )
                os.close(descriptor)
                self.path = index_path
                self._build(index_path, resolved_paths)
                self._open(resolved_paths)
                return

            _base, canonical_paths, index_path, ranges_path = cache
            resolved_paths = canonical_paths
            metadata = load_valid_mfm_ranges(ranges_path, canonical_paths)
            if metadata is not None and index_path.is_file():
                self.path = str(index_path)
                self.temporary = False
                try:
                    self._open(canonical_paths)
                    print(f"using cached MFM event index: {index_path}", flush=True)
                    return
                except ParseError:
                    self.close()

            lock_path = Path(str(index_path) + ".lock")
            with open(lock_path, "a+") as lock:
                fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
                metadata = load_valid_mfm_ranges(ranges_path, canonical_paths)
                if metadata is not None and index_path.is_file():
                    self.path = str(index_path)
                    self.temporary = False
                    try:
                        self._open(canonical_paths)
                        print(f"using cached MFM event index: {index_path}", flush=True)
                        return
                    except ParseError:
                        self.close()

                index_fd, temporary_index = tempfile.mkstemp(
                    prefix=f".{index_path.name}.tmp-", dir=str(index_path.parent)
                )
                os.close(index_fd)
                ranges_fd, temporary_ranges = tempfile.mkstemp(
                    prefix=f".{ranges_path.name}.tmp-", dir=str(ranges_path.parent)
                )
                os.close(ranges_fd)
                try:
                    source_ranges = self._build(temporary_index, canonical_paths)
                    header = self._read_header(temporary_index, canonical_paths)
                    sources = []
                    for source_id, source in enumerate(canonical_paths):
                        item = source_stat(source)
                        item.update(source_ranges[source_id])
                        sources.append(item)
                    payload = {
                        "format": "lilak-mfm-event-ranges",
                        "version": 1,
                        "indexFile": index_path.name,
                        "frameCount": header[3],
                        "eventCount": header[4],
                        "sources": sources,
                    }
                    Path(temporary_ranges).write_text(
                        json.dumps(payload, indent=2, sort_keys=True) + "\n"
                    )
                    os.replace(temporary_index, index_path)
                    os.replace(temporary_ranges, ranges_path)
                    os.chmod(index_path, 0o664)
                    os.chmod(ranges_path, 0o664)
                finally:
                    for temporary in (temporary_index, temporary_ranges):
                        try:
                            os.unlink(temporary)
                        except FileNotFoundError:
                            pass
                self.path = str(index_path)
                self.temporary = False
                self._open(canonical_paths)
                print(f"saved MFM event index: {index_path}", flush=True)
        except Exception:
            self.close()
            raise

    def _build(self, index_path: str, paths: List[Path]) -> dict:
        result = subprocess.run(
            [str(ensure_raw_indexer()), str(index_path), *(str(path) for path in paths)],
            capture_output=True, text=True, check=False,
        )
        if result.stderr:
            print(result.stderr, end="", flush=True)
        if result.returncode != 0:
            raise ParseError(
                f"raw indexing failed: {(result.stderr or result.stdout).strip()}"
            )
        ranges = {}
        for line in result.stdout.splitlines():
            fields = line.split()
            if len(fields) == 5 and fields[0] == "LKGET_SOURCE":
                source_id, frame_count = int(fields[1]), int(fields[2])
                ranges[source_id] = {
                    "frameCount": frame_count,
                    "minEvent": None if fields[3] == "-" else int(fields[3]),
                    "maxEvent": None if fields[4] == "-" else int(fields[4]),
                }
        if set(ranges) != set(range(len(paths))):
            raise ParseError("raw indexer did not return every source event range")
        return ranges

    @classmethod
    def _read_header(cls, path: str, sources: List[Path]) -> tuple:
        with open(path, "rb") as handle:
            data = handle.read(cls.HEADER.size)
            handle.seek(0, os.SEEK_END)
            file_size = handle.tell()
        if len(data) != cls.HEADER.size:
            raise ParseError("raw index is truncated")
        header = cls.HEADER.unpack(data)
        (magic, version, indexed_sources, frames, events, auxiliary_count,
         source_offset, frame_offset, event_offset, auxiliary_offset) = header
        expected_size = auxiliary_offset + auxiliary_count * cls.AUXILIARY.size
        if (magic != b"LKGVIX2\0" or version != 2 or indexed_sources != len(sources)
                or source_offset != cls.HEADER.size
                or frame_offset != source_offset + indexed_sources * cls.SOURCE.size
                or event_offset != frame_offset + frames * cls.FRAME.size
                or auxiliary_offset != event_offset + events * cls.EVENT.size
                or expected_size != file_size):
            raise ParseError("raw index has an invalid format")
        with open(path, "rb") as handle:
            handle.seek(source_offset)
            for source in sources:
                stored = handle.read(cls.SOURCE.size)
                if len(stored) != cls.SOURCE.size:
                    raise ParseError("raw index source table is truncated")
                file_size, modification_time_ns = cls.SOURCE.unpack(stored)
                current = source_stat(source)
                if (file_size != current["size"]
                        or modification_time_ns != current["mtimeNs"]):
                    raise ParseError("raw index does not match its source files")
        return header

    def _open(self, sources: List[Path]) -> None:
        assert self.path is not None
        self._read_header(self.path, sources)
        self.handle = open(self.path, "rb")
        self.mapping = mmap.mmap(self.handle.fileno(), 0, access=mmap.ACCESS_READ)
        (_magic, _version, _sources, self.frame_count, self.event_count,
         self.auxiliary_count, self.source_offset, self.frame_offset,
         self.event_offset, self.auxiliary_offset) = self.HEADER.unpack_from(self.mapping)

    def close(self) -> None:
        if self.mapping is not None:
            self.mapping.close()
            self.mapping = None
        if self.handle is not None:
            self.handle.close()
            self.handle = None
        path = getattr(self, "path", None)
        if path and self.temporary:
            try:
                os.unlink(path)
            except FileNotFoundError:
                pass
        self.path = None

    def __len__(self) -> int:
        return self.event_count

    def event_refs(self, event_index: int) -> Tuple[int, List[Tuple[int, int]]]:
        if not 0 <= event_index < self.event_count:
            raise IndexError("event is outside the indexed file range")
        assert self.mapping is not None
        first, event_id, count = self.EVENT.unpack_from(
            self.mapping, self.event_offset + event_index * self.EVENT.size
        )
        refs = []
        for frame_index in range(first, first + count):
            file_offset, indexed_event_id, source_id = self.FRAME.unpack_from(
                self.mapping, self.frame_offset + frame_index * self.FRAME.size
            )
            if indexed_event_id != event_id:
                raise ParseError("raw index event table is inconsistent")
            refs.append((source_id, file_offset))
        return event_id, refs

    def event_index_for_id(self, event_id: int) -> int:
        assert self.mapping is not None
        low, high = 0, self.event_count
        while low < high:
            middle = (low + high) // 2
            _first, middle_id, _count = self.EVENT.unpack_from(
                self.mapping, self.event_offset + middle * self.EVENT.size
            )
            if middle_id < event_id:
                low = middle + 1
            else:
                high = middle
        if low < self.event_count:
            _first, found_id, _count = self.EVENT.unpack_from(
                self.mapping, self.event_offset + low * self.EVENT.size
            )
            if found_id == event_id:
                return low
        raise IndexError(f"frame {event_id} was not found")


def probe_root_file(path: str) -> dict:
    binary, root_config = ensure_root_reader()
    result = subprocess.run(
        [str(binary), path, "-"], capture_output=True, text=True,
        env=root_reader_environment(root_config), check=False,
    )
    line = result.stdout.strip().splitlines()
    if result.returncode != 0 or not line or not line[0].startswith("BRANCHES "):
        detail = (result.stderr or result.stdout).strip()
        raise ParseError(f"could not inspect ROOT file: {detail or 'invalid event tree'}")
    fields = line[0].split()
    return {"entries": int(fields[1]), "branches": fields[2:]}


class RootEventReader:
    def __init__(self, path: str, branch: str) -> None:
        binary, root_config = ensure_root_reader()
        self.process = subprocess.Popen(
            [str(binary), path, branch], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, text=True, bufsize=1,
            env=root_reader_environment(root_config),
        )
        ready = self.process.stdout.readline().strip() if self.process.stdout else ""
        if not ready.startswith("READY "):
            self.close()
            raise ParseError(f"could not open ROOT branch {branch}")
        self.entries = int(ready.split()[1])

    def close(self) -> None:
        process = getattr(self, "process", None)
        if process is None:
            return
        if process.stdin:
            process.stdin.close()
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=1)
            except subprocess.TimeoutExpired:
                process.kill()
        self.process = None

    def read_event(self, event_index: int) -> tuple:
        if self.process is None or self.process.poll() is not None:
            raise RuntimeError("ROOT reader is not running")
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(f"{event_index}\n")
        self.process.stdin.flush()
        header = self.process.stdout.readline().strip().split()
        if len(header) != 4 or header[0] != "EVENT":
            raise ParseError("ROOT reader returned an invalid event header")
        frame_number, event_time, channel_count = map(int, header[1:])
        channels = {}
        metadata = {}
        for _ in range(channel_count):
            fields = self.process.stdout.readline().strip().split()
            if len(fields) != 523:
                raise ParseError("ROOT reader returned an invalid channel waveform")
            key = tuple(map(int, fields[:4]))
            pedestal, stored_time, stored_amplitude, noise_scale = map(float, fields[4:8])
            strip, detector_number, inverted = map(int, fields[8:11])
            channels[key] = list(map(int, fields[11:]))
            info = {
                "storedPedestal": pedestal,
                "storedTime": stored_time,
                "storedAmplitude": stored_amplitude,
                "noiseScale": noise_scale,
                "inverted": bool(inverted),
            }
            if strip >= 0:
                info["strip"] = strip
            if detector_number >= 0:
                info["detectorNumber"] = detector_number
            metadata[key] = info
        if self.process.stdout.readline().strip() != "END":
            raise ParseError("ROOT reader event was not terminated")
        return frame_number, event_time, channels, metadata


class ViewerState:
    def __init__(self) -> None:
        self.parser = FrameParser()
        self.lock = threading.RLock()
        self.path: Optional[str] = None
        self.paths: List[str] = []
        self.handle = None
        self.source_handles = []
        self.root_reader: Optional[RootEventReader] = None
        self.raw_index: Optional[CompactRawIndex] = None
        self.root_branch: Optional[str] = None
        self.pending_paths: List[str] = []
        self.merged_event_refs: Optional[List[Tuple[int, List[Tuple[int, int]]]]] = None
        self.truncated_sources: List[dict] = []
        self.file_size = 0
        self.event_offsets: List[int] = []
        self.event_ends: List[int] = []
        self.event_ids: List[int] = []
        self.index_cursor = 0
        self.eof = False
        self.current_event = -1

    def close(self) -> None:
        if self.root_reader is not None:
            self.root_reader.close()
        if self.raw_index is not None:
            self.raw_index.close()
        if self.handle is not None:
            self.handle.close()
        for handle in self.source_handles:
            handle.close()
        self.handle = None
        self.source_handles = []
        self.root_reader = None
        self.raw_index = None
        self.root_branch = None
        self.pending_paths = []
        self.merged_event_refs = None
        self.path = None
        self.paths = []
        self.file_size = 0
        self.event_offsets = []
        self.event_ends = []
        self.event_ids = []
        self.index_cursor = 0
        self.eof = False
        self.current_event = -1

    def open_path(self, path: str, selection: Optional[dict] = None) -> dict:
        return self.open_paths([path], selection=selection)

    def open_paths(
        self, paths: List[str], selection: Optional[dict] = None,
        root_branch: Optional[str] = None,
    ) -> dict:
        with self.lock:
            expanded_paths = [os.path.abspath(os.path.expanduser(path)) for path in paths]
            if not expanded_paths:
                raise RuntimeError("no input files were provided")
            for expanded in expanded_paths:
                if not os.path.isfile(expanded):
                    raise FileNotFoundError(expanded)
            root_paths = [path for path in expanded_paths if path.lower().endswith(".root")]
            if root_paths:
                if len(expanded_paths) != 1:
                    raise ValueError("open one ROOT file at a time")
                root_info = probe_root_file(expanded_paths[0])
                branches = root_info["branches"]
                if not branches:
                    raise ParseError("ROOT event tree has neither RawData nor SiChannel")
                if root_branch not in branches:
                    if root_branch is not None:
                        raise ValueError(f"ROOT branch is not available: {root_branch}")
                    if len(branches) > 1:
                        raise ValueError("choose RawData or SiChannel before opening this ROOT file")
                    root_branch = branches[0]
                self.close()
                self.path = expanded_paths[0]
                self.paths = expanded_paths
                self.file_size = os.path.getsize(expanded_paths[0])
                self.root_branch = root_branch
                self.root_reader = RootEventReader(expanded_paths[0], root_branch)
                self.eof = True
                return self.load_event(0, selection=selection)
            cache = persistent_mfm_cache(expanded_paths)
            if cache is not None:
                expanded_paths = [str(path) for path in cache[1]]
            self.close()
            self.path = expanded_paths[0]
            self.paths = expanded_paths
            self.file_size = sum(os.path.getsize(path) for path in expanded_paths)

            # A single layered stream is already merged and can retain the
            # cheap, lazy offset index used by the original viewer.
            probe = open(expanded_paths[0], "rb")
            first_frame = self.parser.index_next_frame(probe)
            probe.close()
            if len(expanded_paths) == 1 and first_frame is not None and first_frame.is_layered:
                self.handle = open(expanded_paths[0], "rb")
                return self.load_event(0, selection=selection)

            # Basic CoBo frames are not necessarily ordered by event id inside
            # one source. Build the shared compact C++ offset index and mmap its
            # on-disk representation instead of retaining Python objects.
            self.source_handles = [open(path, "rb") for path in expanded_paths]
            self.truncated_sources = []
            try:
                self.raw_index = CompactRawIndex(expanded_paths)
            except Exception:
                for handle in self.source_handles:
                    handle.close()
                self.source_handles = []
                raise
            self.eof = True
            print(f"indexed {len(self.raw_index)} events "
                  f"from {len(expanded_paths)} source(s)", flush=True)
            if not self.raw_index:
                self.close()
                raise ParseError("no GET data frames found in input files")
            return self.load_event(0, selection=selection)

    def status(self) -> dict:
        with self.lock:
            indexed_events = (
                self.root_reader.entries
                if self.root_reader is not None
                else len(self.raw_index)
                if self.raw_index is not None
                else len(self.merged_event_refs)
                if self.merged_event_refs is not None
                else len(self.event_offsets)
            )
            return {
                "path": self.path,
                "paths": self.paths,
                "sourceCount": len(self.paths),
                "truncatedSources": list(self.truncated_sources),
                "fileSize": self.file_size,
                "currentEvent": self.current_event,
                "indexedEvents": indexed_events,
                "eof": self.eof,
                "inputType": "ROOT" if self.root_reader is not None else "GET",
                "rootBranch": self.root_branch,
                "pendingPaths": list(self.pending_paths),
                "mappingPath": str(MAPPING.directory) if MAPPING is not None else None,
                "mappingOptions": MAPPING.options if MAPPING is not None else None,
                "browsePath": BROWSE_START_PATH,
            }

    def ensure_open(self) -> None:
        if (self.handle is None and not self.source_handles
                and self.root_reader is None and self.raw_index is None):
            raise RuntimeError("no file is open")

    def ensure_index(self, event_index: int) -> bool:
        self.ensure_open()
        if event_index < 0:
            return False
        if self.root_reader is not None:
            return event_index < self.root_reader.entries
        if self.raw_index is not None:
            return event_index < len(self.raw_index)
        if self.merged_event_refs is not None:
            return event_index < len(self.merged_event_refs)
        if len(self.event_offsets) > event_index:
            return True
        if self.eof:
            return False

        self.handle.seek(self.index_cursor)
        while len(self.event_offsets) <= event_index:
            start = self.handle.tell()
            frame = self.parser.read_next_frame(self.handle)
            if frame is None:
                self.eof = True
                self.index_cursor = self.handle.tell()
                break
            end = self.handle.tell()
            if not frame.is_blob:
                self.event_offsets.append(start)
                self.event_ends.append(end)
                self.event_ids.append(frame.event_idx)
        self.index_cursor = self.handle.tell()
        return len(self.event_offsets) > event_index

    def read_event_frame(self, event_index: int) -> RawFrame:
        if not self.ensure_index(event_index):
            raise IndexError("event is outside the indexed file range")
        if self.raw_index is not None or self.merged_event_refs is not None:
            event_id, refs = (self.raw_index.event_refs(event_index)
                              if self.raw_index is not None
                              else self.merged_event_refs[event_index])
            children = []
            total_size = 0
            for source_index, offset in refs:
                handle = self.source_handles[source_index]
                handle.seek(offset)
                frame = self.parser.read_next_frame(handle)
                if frame is None:
                    raise ParseError("unexpected EOF while reading indexed source frame")
                total_size += frame.frame_size_bytes
                if frame.is_layered:
                    children.extend(frame.children)
                else:
                    children.append(frame)
            return RawFrame(
                frame_type=0xFF01,
                is_layered=True,
                event_idx=event_id,
                item_count=len(children),
                frame_size_bytes=total_size,
                children=children,
            )
        self.handle.seek(self.event_offsets[event_index])
        frame = self.parser.read_next_frame(self.handle)
        if frame is None:
            raise ParseError("unexpected EOF while reading indexed event")
        return frame

    def load_event(self, event_index: int, selection: Optional[dict] = None) -> dict:
        with self.lock:
            frame, channels, channel_metadata = self.read_event_data(event_index)
            self.current_event = event_index
            return self.serialize_event(
                event_index, frame, channels, normalize_selection(selection), channel_metadata
            )

    def read_event_data(self, event_index: int) -> tuple:
        if self.root_reader is not None:
            if not self.ensure_index(event_index):
                raise IndexError("event is outside the ROOT tree range")
            frame_number, event_time, channels, metadata = self.root_reader.read_event(event_index)
            frame = RawFrame(event_idx=frame_number, event_time=event_time)
            return frame, channels, metadata
        frame = self.read_event_frame(event_index)
        return frame, unpack_get_event(frame), {}

    def event_index_for_frame(self, frame_number: int) -> int:
        self.ensure_open()
        if self.root_reader is not None:
            # ROOT files normally store entries in frame order. Check the
            # requested entry first, then fall back to a linear header search.
            if 0 <= frame_number < self.root_reader.entries:
                actual, _time, _channels, _metadata = self.root_reader.read_event(frame_number)
                if actual == frame_number:
                    return frame_number
            for index in range(self.root_reader.entries):
                actual, _time, _channels, _metadata = self.root_reader.read_event(index)
                if actual == frame_number:
                    return index
            raise IndexError(f"frame {frame_number} was not found")
        if self.raw_index is not None:
            return self.raw_index.event_index_for_id(frame_number)
        if self.merged_event_refs is not None:
            index = bisect.bisect_left(self.merged_event_refs, (frame_number,))
            if (index < len(self.merged_event_refs)
                    and self.merged_event_refs[index][0] == frame_number):
                return index
            raise IndexError(f"frame {frame_number} was not found")

        for index, event_id in enumerate(self.event_ids):
            if event_id == frame_number:
                return index
        while not self.eof:
            next_index = len(self.event_offsets)
            if not self.ensure_index(next_index):
                break
            if self.event_ids[-1] == frame_number:
                return len(self.event_ids) - 1
        raise IndexError(f"frame {frame_number} was not found")

    def navigate(self, action: str, selection: Optional[dict] = None) -> dict:
        with self.lock:
            if action.startswith("frame:"):
                target = self.event_index_for_frame(int(action.split(":", 1)[1]))
            elif action == "first":
                target = 0
            elif action == "previous":
                target = max(0, self.current_event - 1)
            elif action == "next":
                target = self.current_event + 1 if self.current_event >= 0 else 0
            elif action == "current":
                target = self.current_event if self.current_event >= 0 else 0
            else:
                target = int(action)
            return self.load_event(target, selection=selection)

    def scan_signal(
        self,
        selection: dict,
        scope: str,
        threshold: float,
        max_threshold: Optional[float],
        include_fpn: bool,
        max_events: int,
        direction: str = "forward",
        detector_selection: Optional[dict] = None,
        min_tb: Optional[int] = None,
        max_tb: Optional[int] = None,
        min_channels: Optional[int] = None,
        max_channels: Optional[int] = None,
    ) -> dict:
        with self.lock:
            scan_selection = scope_selection(selection, scope)
            detector_selection = normalize_detector_selection(detector_selection)
            step = -1 if direction == "backward" else 1
            start_index = self.current_event + step if self.current_event >= 0 else 0
            scanned = 0
            event_index = start_index
            while scanned < max_events:
                if not self.ensure_index(event_index):
                    return {
                        "found": False,
                        "scanned": scanned,
                        "eof": step > 0 and self.eof,
                        "message": "beginning of file" if step < 0 else "end of file",
                    }
                frame, channels, channel_metadata = self.read_event_data(event_index)
                channel_count = len(channels)
                if ((min_channels is not None and channel_count < min_channels)
                        or (max_channels is not None and channel_count > max_channels)):
                    scanned += 1
                    event_index += step
                    continue
                signal_matches = []
                for key, values in sorted(channels.items()):
                    if not include_fpn and key[3] in FPN_CHANNELS:
                        continue
                    if not channel_matches_tuple(key, scan_selection):
                        continue
                    if not channel_matches_detector(key, detector_selection):
                        continue
                    stats = analyze_waveform(values)
                    if (stats["amplitude"] >= threshold
                            and (max_threshold is None or stats["amplitude"] <= max_threshold)
                            and (min_tb is None or stats["peakTb"] >= min_tb)
                            and (max_tb is None or stats["peakTb"] <= max_tb)):
                        signal_matches.append((key, stats))

                # Multiple values in Chan are conjunctive: "1-4" only matches
                # an event when channels 1, 2, 3, and 4 all pass the remaining
                # filters and the amplitude/TB limits. Other CAAC fields remain
                # value sets because they define where those channels may live.
                channel_constraint = scan_selection.get("channel")
                required_channels = set()
                if isinstance(channel_constraint, dict):
                    required_channels.update(channel_constraint.get("include") or [])
                    required_channels.difference_update(channel_constraint.get("exclude") or [])
                matched_channels = {key[3] for key, _stats in signal_matches}
                if signal_matches and required_channels.issubset(matched_channels):
                    key, stats = signal_matches[0]
                    self.current_event = event_index
                    event_payload = self.serialize_event(
                        event_index,
                        frame,
                        channels,
                        normalize_selection({}),
                        channel_metadata,
                    )
                    matching_summaries = []
                    for mapped_key, mapped_values in channels.items():
                        if not channel_matches_tuple(mapped_key, scan_selection):
                            continue
                        if not channel_matches_detector(mapped_key, detector_selection):
                            continue
                        mapped_summary = {
                            "cobo": mapped_key[0],
                            "asad": mapped_key[1],
                            "aget": mapped_key[2],
                            "channel": mapped_key[3],
                        }
                        if MAPPING is not None:
                            mapped_summary.update(MAPPING.channels.get(mapped_key, {}))
                        matching_summaries.append(mapped_summary)
                    event_payload["event"]["scanSelection"] = scan_selection
                    event_payload["event"]["scanDetectorSelection"] = detector_selection
                    event_payload["event"]["selectionMapping"] = mapping_summary(
                        matching_summaries
                    )
                    return {
                        "found": True,
                        "scanned": scanned + 1,
                        "match": {
                            "cobo": key[0],
                            "asad": key[1],
                            "aget": key[2],
                            "channel": key[3],
                            "amplitude": stats["amplitude"],
                            "peakTb": stats["peakTb"],
                        },
                        "event": event_payload,
                    }
                scanned += 1
                event_index += step
            return {
                "found": False,
                "scanned": scanned,
                "eof": False,
                "message": "scan limit reached",
            }

    def serialize_event(
        self,
        event_index: int,
        frame: RawFrame,
        channels: Dict[Tuple[int, int, int, int], List[int]],
        selection: dict,
        channel_metadata: Optional[Dict[Tuple[int, int, int, int], dict]] = None,
    ) -> dict:
        channel_metadata = channel_metadata or {}
        selected_channels = []
        all_summaries = []
        for key in sorted(channels.keys()):
            values = channels[key]
            stats = analyze_waveform(values)
            summary = {
                "cobo": key[0],
                "asad": key[1],
                "aget": key[2],
                "channel": key[3],
                "isFpn": key[3] in FPN_CHANNELS,
                **stats,
            }
            if MAPPING is not None:
                summary.update(MAPPING.channels.get(key, {}))
            summary.update(channel_metadata.get(key, {}))
            all_summaries.append(summary)
            if channel_matches_tuple(key, selection):
                selected_channels.append({**summary, "waveform": values})

        first_data_frame = next(iter(iter_data_frames(frame)), frame)
        selection_summaries = [
            item for item in all_summaries
            if channel_matches_tuple(
                (item["cobo"], item["asad"], item["aget"], item["channel"]),
                selection,
            )
        ]
        return {
            "status": self.status(),
            "event": {
                "index": event_index,
                "fileOffset": frame.file_offset,
                "fileEnd": frame.file_end,
                "frameType": frame.frame_type,
                "eventIdx": first_data_frame.event_idx,
                "eventTime": first_data_frame.event_time,
                "channelCount": len(all_summaries),
                "selectedCount": len(selected_channels),
                "selection": selection,
                "selectionMapping": mapping_summary(selection_summaries),
                "groups": build_group_tree(all_summaries),
                "channels": selected_channels,
            },
        }


def build_group_tree(summaries: List[dict]) -> List[dict]:
    grouped: Dict[int, Dict[int, Dict[int, List[dict]]]] = {}
    for item in summaries:
        grouped.setdefault(item["cobo"], {}).setdefault(item["asad"], {}).setdefault(item["aget"], []).append(item)

    cobo_nodes = []
    for cobo in sorted(grouped):
        asad_nodes = []
        cobo_count = 0
        cobo_max = 0.0
        for asad in sorted(grouped[cobo]):
            aget_nodes = []
            asad_count = 0
            asad_max = 0.0
            for aget in sorted(grouped[cobo][asad]):
                channels = sorted(grouped[cobo][asad][aget], key=lambda row: row["channel"])
                aget_count = len(channels)
                aget_max = max((row["amplitude"] for row in channels), default=0.0)
                aget_nodes.append(
                    {
                        "aget": aget,
                        "count": aget_count,
                        "maxAmplitude": aget_max,
                        "channels": channels,
                        **mapping_summary(channels),
                    }
                )
                asad_count += aget_count
                asad_max = max(asad_max, aget_max)
            asad_nodes.append(
                {
                    "asad": asad,
                    "count": asad_count,
                    "maxAmplitude": asad_max,
                    "agets": aget_nodes,
                    **mapping_summary(
                        [channel for aget in aget_nodes for channel in aget["channels"]]
                    ),
                }
            )
            cobo_count += asad_count
            cobo_max = max(cobo_max, asad_max)
        cobo_nodes.append(
            {
                "cobo": cobo,
                "count": cobo_count,
                "maxAmplitude": cobo_max,
                "asads": asad_nodes,
                **mapping_summary(
                    [
                        channel
                        for asad in asad_nodes
                        for aget in asad["agets"]
                        for channel in aget["channels"]
                    ]
                ),
            }
        )
    return cobo_nodes


def mapping_summary(summaries: List[dict]) -> dict:
    return {
        "detectorLabels": sorted(
            {item["detectorLabel"] for item in summaries if item.get("detectorLabel")}
        ),
        "ringTypes": sorted(
            {item["ringType"] for item in summaries if item.get("ringType")}
        ),
    }


STATE = ViewerState()


class ViewerHandler(BaseHTTPRequestHandler):
    server_version = "LKGETWebViewer/0.1"

    def log_message(self, fmt: str, *args) -> None:
        print("%s - - %s" % (self.address_string(), fmt % args))

    def send_json(self, payload: dict, status: HTTPStatus = HTTPStatus.OK) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_error_json(self, exc: Exception, status: HTTPStatus = HTTPStatus.BAD_REQUEST) -> None:
        self.send_json({"error": str(exc), "type": exc.__class__.__name__}, status)

    def read_json(self) -> dict:
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length > 0 else b"{}"
        return json.loads(raw.decode("utf-8") or "{}")

    def do_GET(self) -> None:
        try:
            parsed = urllib.parse.urlparse(self.path)
            if parsed.path == "/api/status":
                self.send_json({"status": STATE.status()})
                return
            if parsed.path == "/api/mapping-info":
                entries = []
                if MAPPING is not None:
                    for key, metadata in MAPPING.channels.items():
                        entries.append({
                            **metadata,
                            "cobo": key[0],
                            "asad": key[1],
                            "aget": key[2],
                            "channel": key[3],
                        })
                    entries.sort(key=lambda item: (
                        item["detectorType"], item["detectorNumber"],
                        item["ringType"], item["cobo"], item["asad"],
                        item["aget"], item["channel"],
                    ))
                self.send_json({
                    "mappingPath": (str(MAPPING.directory) if MAPPING is not None else None),
                    "entries": entries,
                })
                return
            if parsed.path == "/api/files":
                query = urllib.parse.parse_qs(parsed.query)
                self.send_json(list_directory(query.get("path", [os.getcwd()])[0]))
                return
            if parsed.path == "/api/event":
                query = urllib.parse.parse_qs(parsed.query)
                index = int(query.get("index", [STATE.current_event if STATE.current_event >= 0 else 0])[0])
                selection = normalize_selection({key: query.get(key, [None])[0] for key in ("cobo", "asad", "aget", "channel")})
                self.send_json(STATE.load_event(index, selection=selection))
                return
            self.serve_static(parsed.path)
        except Exception as exc:
            self.send_error_json(exc)

    def do_HEAD(self) -> None:
        try:
            parsed = urllib.parse.urlparse(self.path)
            self.serve_static(parsed.path, head_only=True)
        except Exception:
            self.send_error(HTTPStatus.NOT_FOUND)

    def do_POST(self) -> None:
        global MAPPING
        try:
            parsed = urllib.parse.urlparse(self.path)
            if parsed.path == "/api/upload":
                self.handle_upload()
                return

            payload = self.read_json()
            selection = (normalize_scan_selection(payload.get("selection"))
                         if parsed.path == "/api/scan"
                         else normalize_selection(payload.get("selection")))
            if parsed.path == "/api/root-info":
                path = os.path.abspath(os.path.expanduser(str(payload["path"])))
                if not os.path.isfile(path):
                    raise FileNotFoundError(path)
                self.send_json(probe_root_file(path))
                return
            if parsed.path == "/api/open":
                # Unmerged CoBo data is one file per source, so the browser
                # sends every selected path and the state merges them by event
                # id. A lone "path" is still accepted.
                requested = payload.get("paths") or [payload["path"]]
                paths = [str(item) for item in requested]
                self.send_json(STATE.open_paths(
                    paths, selection=selection, root_branch=payload.get("rootBranch")
                ))
                return
            if parsed.path == "/api/mapping":
                mapping = DetectorMapping(str(payload["path"]))
                with STATE.lock:
                    MAPPING = mapping
                self.send_json(
                    {"mappingPath": str(mapping.directory), "status": STATE.status()}
                )
                return
            if parsed.path == "/api/mapping-selection":
                summaries = []
                if MAPPING is not None:
                    for key, metadata in MAPPING.channels.items():
                        if channel_matches_tuple(key, selection):
                            summaries.append(metadata)
                self.send_json({"mapping": mapping_summary(summaries)})
                return
            if parsed.path == "/api/navigate":
                self.send_json(STATE.navigate(str(payload.get("action", "current")), selection=selection))
                return
            if parsed.path == "/api/scan":
                max_amplitude = payload.get("maxAmplitude")
                min_tb_value = payload.get("minTb")
                max_tb_value = payload.get("maxTb")
                min_channels_value = payload.get("minChannels")
                max_channels_value = payload.get("maxChannels")
                min_tb = None if min_tb_value in (None, "") else int(min_tb_value)
                max_tb = None if max_tb_value in (None, "") else int(max_tb_value)
                min_channels = (None if min_channels_value in (None, "")
                                else int(min_channels_value))
                max_channels = (None if max_channels_value in (None, "")
                                else int(max_channels_value))
                if min_tb is not None and max_tb is not None and min_tb > max_tb:
                    raise ValueError("maximum TB must be greater than or equal to minimum TB")
                if min_channels is not None and min_channels < 0:
                    raise ValueError("minimum channel count cannot be negative")
                if max_channels is not None and max_channels < 0:
                    raise ValueError("maximum channel count cannot be negative")
                if (min_channels is not None and max_channels is not None
                        and min_channels > max_channels):
                    raise ValueError(
                        "maximum channel count must be greater than or equal to minimum channel count"
                    )
                self.send_json(
                    STATE.scan_signal(
                        selection=selection,
                        scope=str(payload.get("scope", "channel")),
                        threshold=float(payload.get("threshold", 50)),
                        max_threshold=(None if max_amplitude in (None, "")
                                       else float(max_amplitude)),
                        include_fpn=bool(payload.get("includeFpn", False)),
                        max_events=max(1, int(payload.get("maxEvents", 10000))),
                        direction=str(payload.get("direction", "forward")),
                        detector_selection=payload.get("detectorSelection"),
                        min_tb=min_tb,
                        max_tb=max_tb,
                        min_channels=min_channels,
                        max_channels=max_channels,
                    )
                )
                return
            self.send_error_json(RuntimeError(f"unknown endpoint {parsed.path}"), HTTPStatus.NOT_FOUND)
        except Exception as exc:
            self.send_error_json(exc)

    def handle_upload(self) -> None:
        UPLOAD_ROOT.mkdir(parents=True, exist_ok=True)
        filename, source = read_multipart_file(self.headers, self.rfile)
        name = os.path.basename(filename)
        target = UPLOAD_ROOT / name
        counter = 1
        while target.exists():
            target = UPLOAD_ROOT / f"{Path(name).stem}_{counter}{Path(name).suffix}"
            counter += 1
        with open(target, "wb") as output:
            while True:
                chunk = source.read(1024 * 1024)
                if not chunk:
                    break
                output.write(chunk)
        self.send_json(STATE.open_path(str(target)))

    def serve_static(self, path: str, head_only: bool = False) -> None:
        name = "index.html" if path == "/" else posixpath.normpath(
            urllib.parse.unquote(path)).lstrip("/")
        asset = STATIC_ASSETS.get(name)
        if asset is None:
            self.send_error(HTTPStatus.NOT_FOUND)
            return

        content_type, body = asset
        data = body.encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if not head_only:
            self.wfile.write(data)


def make_type1_frame(event_idx: int, cobo: int, asad: int, samples: List[Tuple[int, int, int, int]]) -> bytes:
    header_size = 88
    item_size = 4
    frame_size = header_size + len(samples) * item_size
    data = bytearray(frame_size)
    data[0] = 0
    data[1:4] = frame_size.to_bytes(3, "big")
    data[4] = 0
    data[5:7] = (1).to_bytes(2, "big")
    data[7] = 0
    data[8:10] = header_size.to_bytes(2, "big")
    data[10:12] = item_size.to_bytes(2, "big")
    data[12:16] = len(samples).to_bytes(4, "big")
    data[16:22] = (123456).to_bytes(6, "big")
    data[22:26] = event_idx.to_bytes(4, "big")
    data[26] = cobo
    data[27] = asad
    offset = header_size
    for aget, chan, bucket, sample in samples:
        word = ((aget & 0x3) << 30) | ((chan & 0x7F) << 23) | ((bucket & 0x1FF) << 14) | (sample & 0xFFF)
        data[offset : offset + 4] = word.to_bytes(4, "big")
        offset += 4
    return bytes(data)


def make_layered_frame(event_idx: int, children: List[bytes], byte_order: str) -> bytes:
    header_size = 24
    frame_size = header_size + sum(len(child) for child in children)
    data = bytearray(frame_size)
    data[0] = 0x80
    data[1:4] = frame_size.to_bytes(3, byte_order)
    data[5:7] = (0xFF01).to_bytes(2, "big")
    data[7] = 1
    data[8:10] = header_size.to_bytes(2, byte_order)
    data[10:12] = (0).to_bytes(2, byte_order)
    data[12:16] = len(children).to_bytes(4, byte_order)
    data[16:20] = event_idx.to_bytes(4, byte_order)
    offset = header_size
    for child in children:
        data[offset : offset + len(child)] = child
        offset += len(child)
    return bytes(data)


def run_self_test() -> None:
    import io
    import tempfile

    boundary = "----lilak-get-viewer-test"
    uploaded_bytes = b"\r\n\x00raw-data\n"
    upload_body = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="note"\r\n\r\n'
        "ignored\r\n"
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="file"; filename="run.dat"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n"
    ).encode("ascii") + uploaded_bytes + f"\r\n--{boundary}--\r\n".encode("ascii")
    upload_headers = {
        "Content-Type": f'multipart/form-data; boundary="{boundary}"',
        "Content-Length": str(len(upload_body)),
    }
    upload_name, upload_stream = read_multipart_file(upload_headers, io.BytesIO(upload_body))
    assert upload_name == "run.dat"
    assert upload_stream.read() == uploaded_bytes

    multi_selection = normalize_scan_selection({"channel": "10, 15-20, !17"})
    assert channel_matches_tuple((0, 0, 0, 10), multi_selection)
    assert channel_matches_tuple((0, 0, 0, 15), multi_selection)
    assert channel_matches_tuple((0, 0, 0, 20), multi_selection)
    assert not channel_matches_tuple((0, 0, 0, 14), multi_selection)
    assert not channel_matches_tuple((0, 0, 0, 17), multi_selection)
    exclusion_only = normalize_scan_selection({"aget": "!1-2"})
    assert channel_matches_tuple((0, 0, 0, 0), exclusion_only)
    assert not channel_matches_tuple((0, 0, 1, 0), exclusion_only)
    try:
        normalize_scan_selection({"cobo": "4-2"})
        raise AssertionError("descending scan range was accepted")
    except ValueError:
        pass

    raw = make_type1_frame(
        7,
        2,
        1,
        [
            (3, 57, 10, 100),
            (3, 57, 11, 800),
            (0, 2, 5, 20),
        ],
    )
    parser = FrameParser()
    frame = parser.read_next_frame(io.BytesIO(raw))
    assert frame is not None
    assert frame.frame_type == 1
    channels = unpack_get_event(frame)
    assert channels[(2, 1, 3, 57)][11] == 800
    assert channels[(2, 1, 0, 2)][5] == 20
    stats = analyze_waveform(channels[(2, 1, 3, 57)])
    assert stats["amplitude"] == 800
    tree = build_group_tree(
        [
            {"cobo": key[0], "asad": key[1], "aget": key[2], "channel": key[3], "amplitude": analyze_waveform(values)["amplitude"]}
            for key, values in channels.items()
        ]
    )
    assert tree[0]["cobo"] == 2
    assert tree[0]["asads"][0]["asad"] == 1

    for byte_order in ("big", "little"):
        layered_raw = make_layered_frame(37, [raw], byte_order)
        layered = parser.read_next_frame(io.BytesIO(layered_raw))
        assert layered is not None
        assert layered.frame_size_bytes == len(layered_raw)
        assert layered.header_size_bytes == 24
        assert layered.item_count == 1
        assert layered.event_idx == 37
        assert len(layered.children) == 1
        assert unpack_get_event(layered)[(2, 1, 3, 57)][11] == 800

    # Unmerged sources may contain frames in non-event order. Verify that the
    # viewer groups them by event id and combines channels across sources.
    with tempfile.TemporaryDirectory(prefix="lk_get_viewer_test_") as directory:
        source0 = Path(directory) / "run_test.graw"
        source1 = Path(directory) / "run_test.graw.1"
        source0.write_bytes(
            make_type1_frame(8, 0, 0, [(0, 1, 0, 108)])
            + make_type1_frame(7, 0, 0, [(0, 2, 0, 107)])
            + make_type1_frame(9, 0, 0, [(0, 1, 0, 109), (0, 2, 0, 209)])
        )
        source1.write_bytes(make_type1_frame(7, 1, 0, [(0, 3, 0, 207)]))
        state = ViewerState()
        event = state.open_paths([str(source0), str(source1)])
        assert event["event"]["eventIdx"] == 7
        assert event["event"]["channelCount"] == 2
        assert event["status"]["sourceCount"] == 2
        next_event = state.navigate("next")
        assert next_event["event"]["eventIdx"] == 8
        assert state.navigate("frame:7")["event"]["index"] == 0
        assert state.navigate("frame:8")["event"]["index"] == 1
        previous_match = state.scan_signal(
            selection=normalize_selection({}),
            scope="selection",
            threshold=50,
            max_threshold=None,
            include_fpn=False,
            max_events=10,
            direction="backward",
        )
        assert previous_match["found"]
        assert previous_match["event"]["event"]["eventIdx"] == 7
        filtered_match = state.scan_signal(
            selection=normalize_scan_selection({"channel": "1,3,!3"}),
            scope="selection",
            threshold=50,
            max_threshold=None,
            include_fpn=False,
            max_events=10,
            direction="forward",
        )
        assert filtered_match["found"]
        assert filtered_match["event"]["event"]["eventIdx"] == 8
        assert filtered_match["match"]["channel"] == 1
        conjunctive_channel_match = state.scan_signal(
            selection=normalize_scan_selection({"channel": "1-2"}),
            scope="selection",
            threshold=50,
            max_threshold=None,
            include_fpn=False,
            max_events=10,
            direction="forward",
        )
        assert conjunctive_channel_match["found"]
        assert conjunctive_channel_match["event"]["event"]["eventIdx"] == 9
        channel_count_match = state.scan_signal(
            selection=normalize_scan_selection({}),
            scope="selection",
            threshold=50,
            max_threshold=None,
            include_fpn=False,
            max_events=10,
            direction="backward",
            min_channels=2,
            max_channels=2,
        )
        assert channel_count_match["found"]
        assert channel_count_match["event"]["event"]["eventIdx"] == 7
        assert channel_count_match["event"]["event"]["channelCount"] == 2
        state.close()
        index_path = Path(str(source0) + MFM_INDEX_SUFFIX)
        ranges_path = Path(str(source0) + MFM_RANGES_SUFFIX)
        assert index_path.is_file()
        metadata = load_valid_mfm_ranges(ranges_path, [source0, source1])
        assert metadata is not None
        assert metadata["sources"][0]["minEvent"] == 7
        assert metadata["sources"][0]["maxEvent"] == 9
        listed = {item["name"]: item for item in list_directory(directory)["entries"]}
        assert listed[source0.name]["minEvent"] == 7
        assert listed[source1.name]["maxEvent"] == 7
        assert index_path.name not in listed
        cached_state = ViewerState()
        cached_event = cached_state.open_paths([str(source1), str(source0)])
        assert cached_event["event"]["eventIdx"] == 7
        cached_state.close()
        source_stat = source1.stat()
        os.utime(
            source1,
            ns=(source_stat.st_atime_ns, source_stat.st_mtime_ns + 1),
        )
        assert load_valid_mfm_ranges(ranges_path, [source0, source1]) is None
    print("self-test passed")


# The page is served from this module so that the viewer stays a single
# file, the way the other lilak web tools in this directory are packaged.
# Edit the literals below; they are plain HTML, CSS and JavaScript.

INDEX_HTML = r"""<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>LILAK GET Web Viewer</title>
    <link id="favicon" rel="icon" href="/static/favicon.svg" type="image/svg+xml">
    <link rel="stylesheet" href="/static/styles.css">
  </head>
  <body>
    <header class="topbar">
      <div class="brand">
        <svg class="brand-mark" viewBox="0 0 256 256" aria-hidden="true">
          <path d="M216,40H40A16,16,0,0,0,24,56V200a16,16,0,0,0,16,16H216a16,16,0,0,0,16-16V56A16,16,0,0,0,216,40Zm-4.78,91.44c-16.68,35-31.06,50.56-46.65,50.56-19.68,0-31.39-24.56-43.79-50.56C112,113,101,90,91.43,90c-3.74,0-14.37,4-32.21,41.44a8,8,0,0,1-14.44-6.88C61.46,89.59,75.84,74,91.43,74c19.68,0,31.39,24.56,43.79,50.56C144,143,155,166,164.57,166c3.74,0,14.37-4,32.21-41.44a8,8,0,1,1,14.44,6.88Z"></path>
        </svg>
        <div>
          <h1>LILAK GET Web Viewer <span id="instancePort" class="instance-port"></span></h1>
          <p id="fileLabel">No file loaded</p>
        </div>
      </div>
      <form id="openForm" class="file-form">
        <input id="pathInput" name="path" type="text" autocomplete="off" placeholder="/path/to/run.dat">
        <button id="browseButton" type="button" class="secondary">Browse</button>
        <button type="submit">Open</button>
        <label class="upload-button">
          Upload
          <input id="uploadInput" type="file">
        </label>
      </form>
    </header>

    <div id="fileBrowser" class="modal-backdrop" hidden>
      <section class="file-browser" role="dialog" aria-modal="true" aria-labelledby="browserTitle">
        <div class="browser-header">
          <h2 id="browserTitle">Open server file</h2>
          <button id="browserClose" type="button" class="secondary" aria-label="Close">Close</button>
        </div>
        <form id="browserPathForm" class="browser-path-row">
          <button id="browserUp" type="button" class="secondary">Up</button>
          <input id="browserPath" type="text" autocomplete="off" aria-label="Directory path">
          <button type="submit">Go</button>
        </form>
        <div class="browser-search-row">
          <input id="browserSearch" type="search" autocomplete="off" placeholder="Search files in this folder" aria-label="Search files in this folder">
        </div>
        <div class="browser-columns"><span>Name</span><span>Event range</span><span>Size</span></div>
        <div id="browserRows" class="browser-rows"></div>
        <div class="browser-footer">
          <span id="browserSelection">Select one or more files</span>
          <div class="browser-footer-actions">
            <button id="browserSelectAll" type="button" class="secondary" disabled>Select All</button>
            <button id="browserOpenMapping" type="button" class="secondary">Open Mapping</button>
            <button id="browserOpen" type="button" disabled>Open selected</button>
          </div>
        </div>
      </section>
    </div>

    <div id="rootBranchChooser" class="modal-backdrop" hidden>
      <section class="root-branch-chooser" role="dialog" aria-modal="true" aria-labelledby="rootBranchTitle">
        <h2 id="rootBranchTitle">Choose ROOT branch</h2>
        <p id="rootBranchFile"></p>
        <p>This file contains more than one waveform branch. Choose which data to view.</p>
        <div id="rootBranchButtons" class="root-branch-buttons"></div>
        <button id="rootBranchCancel" type="button" class="secondary">Cancel</button>
      </section>
    </div>

    <div id="channelTooltip" class="channel-tooltip" role="tooltip" hidden></div>

    <main class="workspace">
      <aside class="side-panel">
        <section class="control-band">
          <div class="event-jump-row">
            <label for="jumpInput">Event:</label>
            <input id="jumpInput" type="number" min="0" placeholder="Event index">
            <button id="jumpButton" type="button">Go</button>
          </div>
          <div class="event-jump-row">
            <label for="frameInput">Frame:</label>
            <input id="frameInput" type="number" min="0" placeholder="Frame number">
            <button id="frameButton" type="button">Go</button>
          </div>
          <div class="nav-row">
            <button data-nav="first">First</button>
            <button data-nav="previous">Prev</button>
            <button data-nav="next">Next</button>
          </div>
          <div class="auto-next-row">
            <label><input id="autoNextInterval" type="number" min="0.1" step="0.1" value="1.0"><span>sec</span></label>
            <button id="autoNextButton" type="button">Auto</button>
          </div>
        </section>

        <section class="control-band filter-scan-band">
          <div class="section-title section-title-with-info"><span>Filter &amp; Scan</span>
            <span class="info-icon" tabindex="0" aria-label="Filter and Scan information">i
              <span class="info-tooltip">Electronic filters accept comma-separated values, ranges, and exclusions (for example: 10,15-20,!17). Blank fields match any value. Amplitude is the largest absolute signal deviation from the pedestal (median of TB 0&ndash;63), TB range applies to its peak TB, and #Ch range uses the event's total decoded channel count including FPN. The scan stops at the first matching event.</span>
            </span>
            <button id="clearScanFilters" type="button">Clear</button>
          </div>
          <div class="filter-group electronic-filter-group">
            <div class="selection-grid">
              <label>Cobo:<input id="scanCobo" type="text" placeholder="Any" title="Example: 0,2-4,!3"></label>
              <label>AsAd:<input id="scanAsad" type="text" placeholder="Any" title="Example: 0,2-3,!2"></label>
              <label>AGET:<input id="scanAget" type="text" placeholder="Any" title="Example: 0,2-3,!2"></label>
              <label>Chan:<input id="scanChannel" type="text" placeholder="Any" title="All listed channels are required. Example: 10,15-20,!17"></label>
            </div>
          </div>
          <div class="filter-group detector-filter-group">
            <div class="detector-selection-grid">
              <div class="ring-selection-row">
                <span class="scan-field-title">Det. Ring:</span>
                <select id="scanRingType" disabled><option value="">Any</option></select>
                <button id="detectorInfoButton" type="button" disabled>Det info</button>
              </div>
            </div>
            <div class="scan-value-row">
              <span class="scan-field-title">Det. type-#:</span>
              <select id="scanDetectorType" disabled><option value="">Any</option></select>
              <span>&ndash;</span>
              <input id="scanDetectorNumber" type="number" min="0" placeholder="Any" disabled>
            </div>
          </div>
          <div class="filter-group range-filter-group">
            <div class="scan-value-row">
              <span class="scan-field-title">Amp. range:</span>
              <input id="thresholdInput" type="number" min="0" placeholder="Any" title="Optional minimum absolute deviation from the pedestal">
              <span>&ndash;</span>
              <input id="maxAmplitudeInput" type="number" min="0" placeholder="Any" title="Optional maximum absolute deviation from the pedestal">
            </div>
            <div class="scan-value-row">
              <span class="scan-field-title">TB range:</span>
              <input id="minTbInput" type="number" min="0" max="511" placeholder="Any" title="Optional minimum peak TB">
              <span>&ndash;</span>
              <input id="maxTbInput" type="number" min="0" max="511" placeholder="Any" title="Optional maximum peak TB">
            </div>
            <div class="scan-value-row">
              <span class="scan-field-title">#Ch range:</span>
              <input id="minChannelCountInput" type="number" min="0" placeholder="Any" title="Optional minimum total channel count">
              <span>&ndash;</span>
              <input id="maxChannelCountInput" type="number" min="0" placeholder="Any" title="Optional maximum total channel count">
            </div>
          </div>
          <div class="scan-value-row scan-options-row">
            <span class="scan-field-title">Max event:</span>
            <input id="scanLimit" type="number" min="1" value="10000" title="Stop after scanning this many events">
            <label class="inline-check"><span>FPN</span>
              <input id="includeFpn" type="checkbox" checked>
            </label>
          </div>
          <div class="scan-direction-row">
            <button id="scanBackwardButton" type="button">Backward</button>
            <button id="scanForwardButton" type="button">Forward</button>
          </div>
        </section>
      </aside>

      <section class="plot-panel">
        <div class="plot-toolbar">
          <div>
            <strong id="plotTitle">Waveforms</strong>
            <span id="plotSubtitle"></span>
            <span id="plotFileName" class="plot-file-name">No file loaded</span>
          </div>
          <div id="plotActions" class="plot-actions">
            <button id="autoScale" type="button">Autoscale</button>
            <button id="fullScale" type="button" title="Set TB to 0–512 and ADC to 0–4096">Full scale</button>
            <button id="showAll" type="button">Show all</button>
            <button id="filterOnly" type="button" aria-pressed="false" title="Hide channels that do not match Filter &amp; Scan">Filt. only</button>
            <button id="saveWaveform" type="button">Save PNG</button>
            <button id="saveCsv" type="button">Save CSV</button>
          </div>
        </div>
        <canvas id="waveCanvas"></canvas>
        <div id="detectorInfoPanel" class="detector-info-panel" hidden>
          <table class="detector-info-table">
            <thead><tr><th>D-Type</th><th>D-#</th><th>Ring</th><th>Cobo</th><th>AsAd</th><th>AGET</th><th>Chan</th></tr></thead>
            <tbody id="detectorInfoRows"></tbody>
          </table>
        </div>
        <div id="statusLine" class="status-line">Ready</div>
      </section>

      <aside class="detail-panel">
        <div class="panel-tabs" role="tablist">
          <button id="electronicsTab" type="button" class="panel-tab" role="tab">CAAC</button>
          <button id="channelsTab" type="button" class="panel-tab active" role="tab">Channels</button>
        </div>
        <section id="electronicsPanel" class="tab-panel" role="tabpanel" hidden>
          <div id="groupTree" class="group-tree"></div>
        </section>
        <section id="channelsPanel" class="tab-panel channel-panel active" role="tabpanel">
          <div class="table-wrap">
            <table>
              <colgroup><col><col><col><col><col><col><col></colgroup>
              <thead>
                <tr>
                  <th>CAA</th>
                  <th>Ch</th>
                  <th>Ring</th>
                  <th>Detector</th>
                  <th>Strip</th>
                  <th>Amp</th>
                  <th>TB</th>
                </tr>
              </thead>
              <tbody id="channelRows"></tbody>
            </table>
          </div>
        </section>
      </aside>
    </main>

    <footer class="event-bookmark-bar">
      <button id="clearSavedEvents" type="button" class="secondary">Clear Saves</button>
      <button id="saveEventBookmark" type="button">Save Event</button>
      <div id="eventBookmarks" class="event-bookmarks" aria-label="Saved events"></div>
    </footer>

    <script src="/static/app.js"></script>
  </body>
</html>
"""


STYLES_CSS = r""":root {
  color-scheme: light;
  --bg: #f3f5f7;
  --instance-color: #2563a6;
  --instance-soft: #dbeafe;
  --panel: #ffffff;
  --line: #cfd6df;
  --line-strong: #aab6c3;
  --text: #18212b;
  --muted: #657280;
  --blue: #2563a6;
  --teal: #0f766e;
  --amber: #b45309;
  --red: #b91c1c;
  --green: #15803d;
  --button: #27364a;
  --button-text: #ffffff;
  --select: #dbeafe;
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}

* {
  box-sizing: border-box;
}

html,
body {
  height: 100%;
}

body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  overflow: hidden;
}

button,
input,
select {
  font: inherit;
}

button {
  border: 1px solid var(--line-strong);
  background: var(--button);
  color: var(--button-text);
  border-radius: 6px;
  min-height: 34px;
  padding: 0 12px;
  cursor: pointer;
}

button.secondary,
.plot-actions button,
#autoScale,
#showAll,
#filterOnly {
  background: #ffffff;
  color: var(--text);
}

#scanBackwardButton,
#scanForwardButton {
  border-color: #2f6f49;
  background: #3b8257;
  color: #ffffff;
}

.nav-row button,
.event-jump-row button,
#autoNextButton {
  border-color: #40546a;
  background: #4b6076;
  color: #ffffff;
}

#autoNextButton.running {
  border-color: #e19a9a;
  background: #f5c2c2;
  color: #7f1d1d;
}

#clearSavedEvents,
#saveEventBookmark,
#browseButton,
#openForm button[type="submit"],
#openForm .upload-button,
#browserOpen,
#autoScale,
#fullScale,
#showAll,
#filterOnly,
#saveWaveform,
#saveCsv {
  border-color: #bcc7d2;
  background: #dde4eb;
  color: var(--text);
}

#showAll.filter-active,
#filterOnly.filter-active,
#autoScale.filter-active {
  border-color: #60a5fa;
  background: #dbeafe;
  color: #1d4f91;
}

button:hover,
.upload-button:hover {
  border-color: var(--blue);
}

button:disabled {
  opacity: 0.55;
  cursor: wait;
}

body.request-busy button {
  pointer-events: none;
}

input,
select {
  border: 1px solid var(--line);
  background: #ffffff;
  color: var(--text);
  border-radius: 6px;
  min-height: 34px;
  padding: 0 9px;
  width: 100%;
}

input[type="number"] {
  font-size: 16px;
}

input[type="number"] {
  appearance: textfield;
  -moz-appearance: textfield;
}

input[type="number"]::-webkit-inner-spin-button,
input[type="number"]::-webkit-outer-spin-button {
  margin: 0;
  -webkit-appearance: none;
}

input:focus,
select:focus {
  outline: 2px solid #bfdbfe;
  border-color: var(--blue);
}

.topbar {
  height: 68px;
  display: grid;
  grid-template-columns: minmax(320px, 1fr) minmax(420px, 760px);
  align-items: center;
  gap: 20px;
  padding: 0 18px;
  border-bottom: 1px solid var(--line);
  background: #ffffff;
}

.brand {
  display: flex;
  align-items: center;
  min-width: 0;
  gap: 12px;
}

.brand-mark {
  width: 40px;
  height: 40px;
  flex: 0 0 auto;
  color: var(--instance-color);
}

.brand-mark path {
  fill: currentColor;
}

h1 {
  margin: 0;
  font-size: 18px;
  line-height: 1.1;
  letter-spacing: 0;
}

.instance-port {
  display: inline-block;
  margin-left: 6px;
  padding: 2px 7px;
  border-radius: 999px;
  background: var(--instance-soft);
  color: var(--instance-color);
  font-size: 12px;
  vertical-align: 2px;
}

.brand p {
  margin: 4px 0 0;
  color: var(--muted);
  font-size: 13px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  max-width: 64vw;
}

.file-form {
  display: grid;
  grid-template-columns: 1fr auto auto auto;
  gap: 8px;
  align-items: center;
}

.upload-button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-height: 34px;
  padding: 0 12px;
  border-radius: 6px;
  border: 1px solid var(--line-strong);
  background: #ffffff;
  cursor: pointer;
}

.upload-button input {
  display: none;
}

.modal-backdrop {
  position: fixed;
  inset: 0;
  z-index: 100;
  display: grid;
  place-items: center;
  padding: 24px;
  background: rgb(15 23 42 / 55%);
}

.modal-backdrop[hidden] {
  display: none;
}

.file-browser {
  width: min(820px, 96vw);
  height: min(680px, 88vh);
  display: grid;
  grid-template-rows: auto auto auto auto minmax(0, 1fr) auto;
  overflow: hidden;
  border: 1px solid var(--line-strong);
  border-radius: 10px;
  background: #ffffff;
  box-shadow: 0 24px 70px rgb(15 23 42 / 35%);
}

.root-branch-chooser {
  width: min(430px, 94vw);
  padding: 20px;
  border: 1px solid var(--line-strong);
  border-radius: 10px;
  background: #ffffff;
  box-shadow: 0 24px 70px rgb(15 23 42 / 35%);
}

.root-branch-chooser h2 {
  margin: 0 0 8px;
  font-size: 17px;
}

.root-branch-chooser p {
  margin: 0 0 14px;
  color: var(--muted);
  font-size: 13px;
}

#rootBranchFile {
  overflow: hidden;
  color: var(--text);
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.root-branch-buttons {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 10px;
  margin-bottom: 12px;
}

.root-branch-buttons button {
  min-height: 42px;
  background: #dbeafe;
  color: #173a63;
}

.channel-tooltip {
  position: fixed;
  z-index: 200;
  max-width: 280px;
  padding: 8px 10px;
  border: 1px solid #8291a2;
  border-radius: 6px;
  background: #18212b;
  color: #ffffff;
  box-shadow: 0 6px 18px rgb(15 23 42 / 28%);
  font-size: 12px;
  line-height: 1.45;
  pointer-events: none;
  white-space: pre-line;
}

.channel-tooltip[hidden] {
  display: none;
}

.browser-header,
.browser-footer {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 12px 16px;
  border-bottom: 1px solid var(--line);
}

.browser-header h2 {
  margin: 0;
  font-size: 17px;
}

.browser-path-row {
  display: grid;
  grid-template-columns: auto 1fr auto;
  gap: 8px;
  padding: 12px 16px;
}

.browser-search-row {
  padding: 0 16px 12px;
}

.browser-search-row input {
  width: 100%;
}

.browser-columns,
.browser-entry {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 180px 110px;
  gap: 12px;
  align-items: center;
}

.browser-columns {
  padding: 7px 22px;
  border-top: 1px solid var(--line);
  border-bottom: 1px solid var(--line);
  color: var(--muted);
  font-size: 12px;
  font-weight: 700;
}

.browser-columns span:nth-child(n+2),
.browser-entry .browser-event-range,
.browser-entry .browser-size {
  text-align: right;
}

.browser-rows {
  overflow: auto;
  padding: 4px 8px;
}

.browser-entry {
  width: 100%;
  min-height: 36px;
  padding: 5px 12px;
  border: 1px solid transparent;
  background: #ffffff;
  color: var(--text);
  text-align: left;
}

.browser-entry:hover,
.browser-entry.selected {
  border-color: #93c5fd;
  background: var(--select);
}

.browser-name {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.browser-size,
#browserSelection {
  color: var(--muted);
  font-size: 12px;
}

.browser-footer {
  display: grid;
  grid-template-columns: minmax(0, 1fr);
  align-items: stretch;
  justify-content: stretch;
  gap: 8px;
  border-top: 1px solid var(--line);
  border-bottom: 0;
}

.browser-footer-actions {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  flex-wrap: wrap;
}

#browserSelection {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.workspace {
  height: calc(100vh - 116px);
  display: grid;
  grid-template-columns: 310px minmax(360px, 1fr) 340px;
  grid-template-rows: 100%;
  overflow: hidden;
}

.side-panel,
.detail-panel {
  min-width: 0;
  overflow-y: auto;
  overflow-x: hidden;
  border-right: 1px solid var(--line);
  background: #f8fafc;
  display: flex;
  flex-direction: column;
}

.detail-panel {
  border-right: 0;
  border-left: 1px solid var(--line);
}

.control-band,
.tree-panel,
.channel-panel {
  padding: 14px;
}

.control-band {
  border-bottom: 1px solid var(--line);
  flex: 0 0 auto;
  min-width: 0;
}

.side-panel input,
.side-panel select,
.side-panel label {
  min-width: 0;
}

.channel-panel {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 220px;
}

.section-title {
  font-size: 12px;
  font-weight: 700;
  color: var(--muted);
  text-transform: uppercase;
  letter-spacing: 0.08em;
  margin-bottom: 10px;
}

.section-title-with-info {
  display: flex;
  align-items: center;
  gap: 7px;
}

#clearScanFilters {
  min-height: 24px;
  margin-left: auto;
  padding: 0 8px;
  border-color: #c1c9d2;
  background: #e4e8ed;
  color: var(--text);
  font-size: 11px;
  font-weight: 600;
  letter-spacing: normal;
  text-transform: none;
}

.control-band input.filter-value,
.control-band select.filter-value {
  border-color: #3b82c4;
  box-shadow: 0 0 0 1px #3b82c4;
}

.control-band input.filter-invalid {
  border-color: #dc2626;
  box-shadow: 0 0 0 1px #dc2626;
}

.filter-group {
  position: relative;
  padding-left: 12px;
  margin-bottom: 10px;
}

.filter-group::before {
  content: "";
  position: absolute;
  top: 0;
  bottom: 0;
  left: 0;
  width: 3px;
  border-radius: 2px;
  background: #718da8;
}

.detector-filter-group::before {
  background: #c49a35;
}

.range-filter-group::before {
  background: #718da8;
}

.filter-group > :last-child {
  margin-bottom: 0;
}

.info-icon {
  position: relative;
  display: inline-grid;
  place-items: center;
  width: 17px;
  height: 17px;
  border: 1px solid var(--line-strong);
  border-radius: 50%;
  background: #ffffff;
  color: var(--blue);
  font-size: 11px;
  font-weight: 800;
  text-transform: none;
  cursor: help;
}

.info-tooltip {
  position: absolute;
  z-index: 20;
  top: 23px;
  left: -80px;
  width: 276px;
  padding: 9px 10px;
  border: 1px solid var(--line-strong);
  border-radius: 6px;
  background: #18212b;
  color: #ffffff;
  box-shadow: 0 8px 22px rgb(15 23 42 / 24%);
  font-size: 11px;
  font-weight: 400;
  line-height: 1.45;
  letter-spacing: 0;
  text-transform: none;
  visibility: hidden;
  opacity: 0;
  transition: opacity 120ms ease;
}

.info-icon:hover .info-tooltip,
.info-icon:focus .info-tooltip {
  visibility: visible;
  opacity: 1;
}

.nav-row,
.auto-next-row,
.action-row,
.scan-row {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 8px;
  margin-bottom: 8px;
}

.event-jump-row {
  display: grid;
  grid-template-columns: 48px minmax(0, 1fr) 54px;
  align-items: center;
  gap: 7px;
  margin-bottom: 8px;
}

.event-jump-row label {
  color: var(--muted);
  font-size: 13px;
}

.event-jump-row button {
  width: 54px;
  padding: 0 6px;
}

.auto-next-row {
  grid-template-columns: minmax(0, 1fr) 76px;
}

.auto-next-row button {
  width: 76px;
  padding: 0 6px;
}

.auto-next-row label {
  display: grid;
  grid-template-columns: 1fr auto;
  align-items: center;
  gap: 6px;
  color: var(--muted);
  font-size: 12px;
}

.action-row,
.scan-row {
  grid-template-columns: 1fr 1fr;
}

.selection-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
  margin-bottom: 8px;
}

.selection-grid label {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr);
  align-items: center;
  gap: 6px;
  font-size: 12px;
  color: var(--muted);
}

.detector-selection-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
  margin-bottom: 8px;
}

.detector-selection-grid label {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr);
  align-items: center;
  gap: 6px;
  min-width: 0;
  color: var(--muted);
  font-size: 12px;
}

.detector-selection-grid .ring-selection-row {
  grid-column: 1 / -1;
  display: grid;
  grid-template-columns: 76px minmax(0, 1fr) 10px minmax(0, 1fr);
  align-items: center;
  gap: 7px;
  color: var(--muted);
  font-size: 12px;
}

.detector-selection-grid .ring-selection-row #detectorInfoButton {
  grid-column: 4;
}

#detectorInfoButton {
  min-height: 30px;
  padding: 0 10px;
  background: #e4e8ed;
  color: var(--text);
}

#detectorInfoButton.active {
  border-color: #6096c8;
  background: #dbeafe;
}

.detector-selection-grid select:disabled,
.detector-selection-grid input:disabled,
.scan-value-row select:disabled,
.scan-value-row input:disabled {
  background: #eef2f6;
  color: #94a3b8;
}

.field-label {
  display: grid;
  gap: 4px;
  color: var(--muted);
  font-size: 12px;
}

.scan-value-row {
  display: grid;
  grid-template-columns: 76px minmax(0, 1fr) 10px minmax(0, 1fr);
  align-items: center;
  gap: 7px;
  margin-bottom: 8px;
  color: var(--muted);
  font-size: 12px;
}

.scan-field-title {
  white-space: nowrap;
}

.scan-options-row .inline-check {
  grid-column: 3 / 5;
  justify-self: end;
}

.scan-options-row {
  margin-left: 12px;
}

.inline-check {
  display: flex;
  align-items: center;
  gap: 7px;
}

.inline-check input {
  width: auto;
  min-height: auto;
}

.check-label {
  min-height: 34px;
  display: flex;
  align-items: center;
  gap: 8px;
  border: 1px solid var(--line);
  border-radius: 6px;
  padding: 0 9px;
  background: #ffffff;
}

.check-label input {
  width: auto;
  min-height: auto;
}

.wide-button {
  width: 100%;
}

.scan-direction-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
}

.panel-tabs {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 0;
  flex: 0 0 auto;
  border-bottom: 1px solid var(--line);
  background: #ffffff;
}

.panel-tab {
  border: 0;
  border-radius: 0;
  border-right: 1px solid var(--line);
  background: #ffffff;
  color: var(--muted);
  font-size: 12px;
  font-weight: 700;
}

.panel-tab.active {
  background: #f8fafc;
  color: var(--blue);
  box-shadow: inset 0 2px 0 var(--blue);
}

.tab-panel {
  display: none;
  min-height: 0;
  padding: 12px;
  overflow: auto;
  flex: 1 1 auto;
}

.tab-panel.active {
  display: flex;
  flex-direction: column;
}

.tab-panel[hidden] {
  display: none;
}

.tree-panel {
  overflow: auto;
  flex: 1 1 auto;
}

.group-tree {
  display: grid;
  gap: 0;
  font-size: 13px;
}

.tree-row {
  display: grid;
  grid-template-columns: 1fr auto auto;
  gap: 6px;
  align-items: center;
  min-height: 26px;
  border: 1px solid transparent;
  border-radius: 4px;
  padding: 2px 5px;
  cursor: pointer;
}

.tree-row:hover {
  border-color: var(--line);
  background: #ffffff;
}

.tree-row.selected {
  border-color: #93c5fd;
  background: var(--select);
}

.tree-row .name {
  display: flex;
  align-items: center;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.tree-toggle {
  flex: 0 0 14px;
  width: 14px;
  height: 18px;
  min-height: 0;
  margin: 0 3px 0 0;
  padding: 0;
  border: 0;
  border-radius: 3px;
  background: transparent;
  color: var(--muted);
  font-size: 11px;
  line-height: 18px;
}

.tree-toggle:hover {
  border: 0;
  background: #e5edf5;
  color: var(--text);
}

.tree-row .count,
.tree-row .amp {
  color: var(--muted);
  font-variant-numeric: tabular-nums;
}

.tree-cobo {
  font-weight: 750;
}

.tree-asad {
  margin-left: 14px;
  font-weight: 650;
}

.tree-aget {
  margin-left: 28px;
}

.tree-channel {
  margin-left: 42px;
  color: #26313d;
}

.tree-channel.fpn {
  color: var(--amber);
}

.plot-panel {
  min-width: 0;
  min-height: 0;
  display: grid;
  grid-template-rows: 52px minmax(0, 1fr) 34px;
  margin: 8px;
  overflow: hidden;
  border: 1px solid var(--line-strong);
  border-radius: 8px;
  background: var(--panel);
}

.plot-panel [hidden] {
  display: none !important;
}

.plot-toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 0 16px;
  border-bottom: 1px solid var(--line);
}

#plotTitle {
  font-size: 16px;
}

#plotSubtitle {
  color: var(--muted);
  margin-left: 8px;
  font-size: 13px;
}

.plot-actions {
  display: flex;
  gap: 8px;
}

#waveCanvas {
  width: 100%;
  height: 100%;
  display: block;
}

.detector-info-panel {
  min-width: 0;
  min-height: 0;
  overflow: auto;
  padding: 12px 16px 20px;
  background: #ffffff;
}

.detector-info-panel[hidden] {
  display: none;
}

.detector-info-table {
  width: 100%;
  table-layout: auto;
  font-size: 12px;
}

.detector-info-table th,
.detector-info-table td {
  padding: 6px 10px;
  border-bottom: 1px solid #e5e9ee;
  text-align: center;
  white-space: nowrap;
}

.detector-info-table th {
  position: sticky;
  top: 0;
  z-index: 1;
  background: #eef2f6;
  color: #3f4d5c;
}

.detector-info-table tbody tr:nth-child(even) {
  background: #f8fafc;
}

.status-line {
  display: flex;
  align-items: center;
  padding: 0 16px;
  color: var(--muted);
  font-size: 13px;
  border-top: 1px solid var(--line);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.status-line.error {
  color: var(--red);
}

.status-line.good {
  color: var(--green);
}

.plot-file-name {
  display: block;
  color: var(--muted);
  font-size: 11px;
  max-width: 52vw;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.table-wrap {
  overflow: auto;
  flex: 1 1 auto;
}

#waveCanvas {
  touch-action: none;
  cursor: crosshair;
}

#waveCanvas.selecting {
  cursor: crosshair;
}

table {
  border-collapse: collapse;
  width: 310px;
  max-width: none;
  table-layout: fixed;
  font-size: 11px;
}

.channel-panel col:nth-child(1) { width: 48px; }
.channel-panel col:nth-child(2) { width: 34px; }
.channel-panel col:nth-child(3) { width: 38px; }
.channel-panel col:nth-child(4) { width: 62px; }
.channel-panel col:nth-child(5) { width: 38px; }
.channel-panel col:nth-child(6) { width: 48px; }
.channel-panel col:nth-child(7) { width: 34px; }

th,
td {
  padding: 6px 3px;
  text-align: right;
  border-bottom: 1px solid var(--line);
  font-variant-numeric: tabular-nums;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

th:first-child,
td:first-child {
  text-align: left;
}

tbody tr {
  cursor: pointer;
}

tbody tr:hover {
  background: #eef6ff;
}

tbody tr.selected {
  background: var(--select);
}

tbody tr.scan-dimmed {
  opacity: 0.28;
}

tbody tr:hover {
  background: #eef4fb;
  cursor: pointer;
}

tbody tr.keyboard-highlight {
  background: #cfe8ff;
  outline: 1px solid #70aee3;
  outline-offset: -1px;
}

.event-bookmark-bar {
  height: 48px;
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 7px 14px;
  border-top: 1px solid var(--line);
  background: #ffffff;
}

.event-bookmarks {
  min-width: 0;
  display: flex;
  align-items: center;
  gap: 7px;
  overflow-x: auto;
  scrollbar-width: thin;
}

.event-bookmarks button {
  flex: 0 0 auto;
  min-height: 30px;
  background: #ffffff;
  color: var(--blue);
}

@media (max-width: 1100px) {
  body {
    overflow: auto;
  }

  .topbar {
    height: auto;
    min-height: 86px;
    grid-template-columns: 1fr;
    padding: 12px;
  }

  .workspace {
    height: auto;
    min-height: calc(100vh - 134px);
    grid-template-columns: 1fr;
    grid-template-rows: auto 560px auto;
  }

  .side-panel,
  .detail-panel {
    border-right: 0;
    border-left: 0;
    border-bottom: 1px solid var(--line);
  }

  .tree-panel {
    max-height: 340px;
  }

  .channel-panel {
    max-height: 360px;
  }

  .tab-panel {
    min-height: 360px;
  }

  .event-bookmark-bar {
    position: sticky;
    bottom: 0;
  }
}
"""


APP_JS = r"""const els = {
  favicon: document.getElementById("favicon"),
  instancePort: document.getElementById("instancePort"),
  fileLabel: document.getElementById("fileLabel"),
  openForm: document.getElementById("openForm"),
  pathInput: document.getElementById("pathInput"),
  browseButton: document.getElementById("browseButton"),
  uploadInput: document.getElementById("uploadInput"),
  fileBrowser: document.getElementById("fileBrowser"),
  browserClose: document.getElementById("browserClose"),
  browserPathForm: document.getElementById("browserPathForm"),
  browserPath: document.getElementById("browserPath"),
  browserSearch: document.getElementById("browserSearch"),
  browserUp: document.getElementById("browserUp"),
  browserRows: document.getElementById("browserRows"),
  browserSelection: document.getElementById("browserSelection"),
  browserOpen: document.getElementById("browserOpen"),
  browserOpenMapping: document.getElementById("browserOpenMapping"),
  browserSelectAll: document.getElementById("browserSelectAll"),
  rootBranchChooser: document.getElementById("rootBranchChooser"),
  rootBranchFile: document.getElementById("rootBranchFile"),
  rootBranchButtons: document.getElementById("rootBranchButtons"),
  rootBranchCancel: document.getElementById("rootBranchCancel"),
  jumpInput: document.getElementById("jumpInput"),
  jumpButton: document.getElementById("jumpButton"),
  frameInput: document.getElementById("frameInput"),
  frameButton: document.getElementById("frameButton"),
  autoNextInterval: document.getElementById("autoNextInterval"),
  autoNextButton: document.getElementById("autoNextButton"),
  clearSavedEvents: document.getElementById("clearSavedEvents"),
  scanCobo: document.getElementById("scanCobo"),
  scanAsad: document.getElementById("scanAsad"),
  scanAget: document.getElementById("scanAget"),
  scanChannel: document.getElementById("scanChannel"),
  scanDetectorType: document.getElementById("scanDetectorType"),
  scanDetectorNumber: document.getElementById("scanDetectorNumber"),
  scanRingType: document.getElementById("scanRingType"),
  detectorInfoButton: document.getElementById("detectorInfoButton"),
  clearScanFilters: document.getElementById("clearScanFilters"),
  thresholdInput: document.getElementById("thresholdInput"),
  maxAmplitudeInput: document.getElementById("maxAmplitudeInput"),
  minTbInput: document.getElementById("minTbInput"),
  maxTbInput: document.getElementById("maxTbInput"),
  minChannelCountInput: document.getElementById("minChannelCountInput"),
  maxChannelCountInput: document.getElementById("maxChannelCountInput"),
  scanLimit: document.getElementById("scanLimit"),
  includeFpn: document.getElementById("includeFpn"),
  scanBackwardButton: document.getElementById("scanBackwardButton"),
  scanForwardButton: document.getElementById("scanForwardButton"),
  groupTree: document.getElementById("groupTree"),
  electronicsTab: document.getElementById("electronicsTab"),
  channelsTab: document.getElementById("channelsTab"),
  electronicsPanel: document.getElementById("electronicsPanel"),
  channelsPanel: document.getElementById("channelsPanel"),
  plotTitle: document.getElementById("plotTitle"),
  plotSubtitle: document.getElementById("plotSubtitle"),
  plotFileName: document.getElementById("plotFileName"),
  plotActions: document.getElementById("plotActions"),
  autoScale: document.getElementById("autoScale"),
  fullScale: document.getElementById("fullScale"),
  showAll: document.getElementById("showAll"),
  filterOnly: document.getElementById("filterOnly"),
  saveWaveform: document.getElementById("saveWaveform"),
  saveCsv: document.getElementById("saveCsv"),
  waveCanvas: document.getElementById("waveCanvas"),
  detectorInfoPanel: document.getElementById("detectorInfoPanel"),
  detectorInfoRows: document.getElementById("detectorInfoRows"),
  statusLine: document.getElementById("statusLine"),
  channelRows: document.getElementById("channelRows"),
  channelTooltip: document.getElementById("channelTooltip"),
  saveEventBookmark: document.getElementById("saveEventBookmark"),
  eventBookmarks: document.getElementById("eventBookmarks"),
};

const viewerPort = window.location.port || (window.location.protocol === "https:" ? "443" : "80");
const portNumber = Number.parseInt(viewerPort, 10) || 0;
const instancePalette = [
  { color: "#2563a6", soft: "#dbeafe" },
  { color: "#c2410c", soft: "#ffedd5" },
  { color: "#0f766e", soft: "#ccfbf1" },
  { color: "#7c3aed", soft: "#ede9fe" },
  { color: "#b45309", soft: "#fef3c7" },
  { color: "#be185d", soft: "#fce7f3" },
];
const paletteIndex = ((portNumber - 8877) % instancePalette.length + instancePalette.length)
  % instancePalette.length;
const instanceTheme = instancePalette[paletteIndex];
document.documentElement.style.setProperty("--instance-color", instanceTheme.color);
document.documentElement.style.setProperty("--instance-soft", instanceTheme.soft);
els.instancePort.textContent = `:${viewerPort}`;
document.title = `LILAK GET Web Viewer :${viewerPort}`;
const iconPath = document.querySelector(".brand-mark path")?.getAttribute("d");
if (iconPath) {
  const iconSvg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="${instanceTheme.color}"><path d="${iconPath}"/></svg>`;
  els.favicon.href = `data:image/svg+xml,${encodeURIComponent(iconSvg)}`;
}

const COLORS = [
  "#2563a6",
  "#0f766e",
  "#b45309",
  "#7c3aed",
  "#be123c",
  "#15803d",
  "#0e7490",
  "#a16207",
  "#4f46e5",
  "#c2410c",
];

let currentPayload = null;
let busyCount = 0;
let browserParent = null;
let browserSelectedPaths = [];
let browserEntries = [];
let browseStartPath = ".";
let lastBrowserPath = null;
let autoNextTimer = null;
let hoveredChannelKey = null;
let viewSelection = { cobo: null, asad: null, aget: null, channel: null };
let activeScanSelection = null;
let activeScanDetectorSelection = null;
let filterOnlyEnabled = false;
const collapsedTreeNodes = new Set();
let detectorInfoVisible = false;
let detectorInfoEntries = [];
let detectorInfoMappingPath = null;
let bookmarkedEvents = [];
let bookmarkPath = null;
const FULL_SCALE_VIEW = { tbMin: 0, tbMax: 512, yMin: 0, yMax: 4096 };
// Start each file at the complete GET time-bucket and ADC ranges. A view object
// survives event navigation until Autoscale or a double click resets it.
let plotView = { ...FULL_SCALE_VIEW };
let plotGeometry = null;

function useDefaultFullScale() {
  plotView = { ...FULL_SCALE_VIEW };
  els.autoScale.classList.remove("filter-active");
}

const scanFilterControls = [
  els.scanCobo, els.scanAsad, els.scanAget, els.scanChannel,
  els.scanRingType, els.scanDetectorType, els.scanDetectorNumber,
  els.thresholdInput, els.maxAmplitudeInput, els.minTbInput, els.maxTbInput,
  els.minChannelCountInput, els.maxChannelCountInput,
];
const electronicScanControls = [els.scanCobo, els.scanAsad, els.scanAget, els.scanChannel];

function isValidNumberExpression(rawValue) {
  const text = rawValue.trim();
  if (!text || ["any", "all"].includes(text.toLowerCase())) return true;
  return text.split(",").every((rawToken) => {
    const token = rawToken.trim();
    if (!token) return false;
    const body = token.startsWith("!") ? token.slice(1).trim() : token;
    const match = body.match(/^(\d+)(?:\s*-\s*(\d+))?$/);
    if (!match) return false;
    const start = Number(match[1]);
    const end = match[2] === undefined ? start : Number(match[2]);
    return Number.isSafeInteger(start) && Number.isSafeInteger(end)
      && end >= start && end - start <= 10000;
  });
}

function updateFilterHighlights() {
  scanFilterControls.forEach((control) => {
    const invalid = electronicScanControls.includes(control)
      && !isValidNumberExpression(control.value);
    control.classList.toggle("filter-invalid", invalid);
    control.classList.toggle("filter-value",
      !invalid && !control.disabled && control.value.trim() !== "");
    if (electronicScanControls.includes(control)) {
      control.setAttribute("aria-invalid", invalid ? "true" : "false");
    }
  });
}

scanFilterControls.forEach((control) => {
  control.addEventListener("input", () => {
    updateFilterHighlights();
    if (filterOnlyEnabled && currentPayload) renderCurrentEventViews();
  });
  control.addEventListener("change", () => {
    updateFilterHighlights();
    if (filterOnlyEnabled && currentPayload) renderCurrentEventViews();
  });
});
els.includeFpn.addEventListener("change", () => {
  if (filterOnlyEnabled && currentPayload) renderCurrentEventViews();
});

function setBusy(isBusy) {
  busyCount += isBusy ? 1 : -1;
  if (busyCount < 0) busyCount = 0;
  document.body.classList.toggle("request-busy", busyCount > 0);
  document.body.setAttribute("aria-busy", busyCount > 0 ? "true" : "false");
}

document.addEventListener("click", (event) => {
  if (busyCount > 0 && event.target instanceof Element
      && event.target.closest("button, .upload-button")) {
    event.preventDefault();
    event.stopImmediatePropagation();
  }
}, true);

// Mouse-clicked controls otherwise retain focus, causing the global shortcut
// handler to treat subsequent keys as control input. Keep keyboard-originated
// clicks focused so Tab/Enter navigation remains accessible.
document.addEventListener("click", (event) => {
  if (event.detail <= 0 || !(event.target instanceof Element)) return;
  const button = event.target.closest("button");
  if (button instanceof HTMLElement) button.blur();
});

document.addEventListener("submit", (event) => {
  if (busyCount > 0) {
    event.preventDefault();
    event.stopImmediatePropagation();
  }
}, true);

document.addEventListener("keydown", (event) => {
  if (busyCount > 0 && ["Enter", " ", "ArrowLeft", "ArrowRight", "Backspace"].includes(event.key)) {
    event.preventDefault();
    event.stopImmediatePropagation();
  }
}, true);

function setStatus(message, kind = "") {
  els.statusLine.textContent = message;
  els.statusLine.className = `status-line ${kind}`.trim();
}

function compactPath(path) {
  if (!path) return "No file loaded";
  const parts = path.split("/");
  if (parts.length <= 4) return path;
  return `${parts[0] || "/" + parts[1]}/.../${parts.slice(-2).join("/")}`.replace("//", "/");
}

function fileNameFromPath(path) {
  if (!path) return "No file loaded";
  return path.split("/").filter(Boolean).pop() || path;
}

function eventDownloadName(path, eventIndex, extension) {
  const fileName = fileNameFromPath(path);
  const runMatch = fileName.match(/^(run_\d+)\.dat\./);
  return runMatch
    ? `${runMatch[1]}.${eventIndex}.${extension}`
    : `${fileName}_event_${eventIndex}.${extension}`;
}

function csvCell(value) {
  const text = String(value ?? "");
  return /[",\r\n]/.test(text) ? `"${text.replace(/"/g, '""')}"` : text;
}

function displayedFileName(status) {
  const fileName = fileNameFromPath(status.path);
  return status.sourceCount > 1 ? `${fileName} (${status.sourceCount} files)` : fileName;
}

function inputValue(input) {
  const raw = input.value.trim();
  if (raw === "") return null;
  const parsed = Number.parseInt(raw, 10);
  return Number.isFinite(parsed) ? parsed : null;
}

function readSelection() {
  return { ...viewSelection };
}

function writeSelection(selection) {
  viewSelection = {
    cobo: selection.cobo ?? null,
    asad: selection.asad ?? null,
    aget: selection.aget ?? null,
    channel: selection.channel ?? null,
  };
}

function readScanSelection() {
  return {
    cobo: els.scanCobo.value.trim() || null,
    asad: els.scanAsad.value.trim() || null,
    aget: els.scanAget.value.trim() || null,
    channel: els.scanChannel.value.trim() || null,
  };
}

function readScanDetectorSelection() {
  return {
    detectorType: els.scanDetectorType.value || null,
    detectorNumber: inputValue(els.scanDetectorNumber),
    ringType: els.scanRingType.value || null,
  };
}

function replaceSelectOptions(select, values) {
  const previous = select.value;
  select.replaceChildren();
  const any = document.createElement("option");
  any.value = "";
  any.textContent = "Any";
  select.appendChild(any);
  values.forEach((value) => {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = value;
    select.appendChild(option);
  });
  select.value = values.includes(previous) ? previous : "";
}

function updateMappingControls(status) {
  const options = status.mappingOptions;
  const enabled = Boolean(status.mappingPath && options);
  if (detectorInfoMappingPath !== status.mappingPath) {
    detectorInfoEntries = [];
    detectorInfoMappingPath = status.mappingPath || null;
    if (detectorInfoVisible) setDetectorInfoVisible(false);
  }
  replaceSelectOptions(els.scanDetectorType, enabled ? options.detectorTypes : []);
  replaceSelectOptions(els.scanRingType, enabled ? options.ringTypes : []);
  els.scanDetectorType.disabled = !enabled;
  els.scanDetectorNumber.disabled = !enabled;
  els.scanRingType.disabled = !enabled;
  els.detectorInfoButton.disabled = !enabled;
  if (!enabled) els.scanDetectorNumber.value = "";
  updateFilterHighlights();
}

function renderDetectorInfo() {
  const rows = document.createDocumentFragment();
  detectorInfoEntries.forEach((entry) => {
    const tr = document.createElement("tr");
    ["detectorType", "detectorNumber", "ringType", "cobo", "asad", "aget", "channel"]
      .forEach((field) => {
        const td = document.createElement("td");
        td.textContent = entry[field] ?? "";
        tr.appendChild(td);
      });
    rows.appendChild(tr);
  });
  els.detectorInfoRows.replaceChildren(rows);
  els.plotTitle.textContent = "Detector mapping";
  els.plotSubtitle.textContent = `${detectorInfoEntries.length} channels`;
  els.plotFileName.textContent = detectorInfoMappingPath
    ? fileNameFromPath(detectorInfoMappingPath) : "No mapping loaded";
  els.plotFileName.title = detectorInfoMappingPath || "";
}

function setDetectorInfoVisible(visible) {
  detectorInfoVisible = visible;
  els.waveCanvas.hidden = visible;
  els.detectorInfoPanel.hidden = !visible;
  els.plotActions.hidden = visible;
  els.detectorInfoButton.classList.toggle("active", visible);
  els.detectorInfoButton.setAttribute("aria-pressed", visible ? "true" : "false");
  if (visible) {
    renderDetectorInfo();
  } else if (currentPayload) {
    const { event, status } = currentPayload;
    els.plotTitle.textContent = `Event ${event.index}`;
    els.plotFileName.textContent = displayedFileName(status);
    els.plotFileName.title = status.path || "";
    renderCurrentEventViews();
  }
}

function matchesSelection(channel, selection) {
  if (!selection) return true;
  const matchesValue = (value, constraint) => {
    if (constraint === null || constraint === undefined) return true;
    if (typeof constraint === "object") {
      const included = constraint.include;
      const excluded = constraint.exclude || [];
      return (included === null || included === undefined || included.includes(value))
        && !excluded.includes(value);
    }
    return value === constraint;
  };
  return ["cobo", "asad", "aget", "channel"].every(
    (key) => matchesValue(channel[key], selection[key]),
  );
}

function matchesDetectorSelection(channel, selection) {
  if (!selection) return true;
  return (
    (selection.detectorType === null || selection.detectorType === undefined
      || channel.detectorType === selection.detectorType)
    && (selection.detectorNumber === null || selection.detectorNumber === undefined
      || channel.detectorNumber === selection.detectorNumber)
    && (selection.ringType === null || selection.ringType === undefined
      || channel.ringType === selection.ringType)
  );
}

function matchesActiveScan(channel) {
  return matchesSelection(channel, activeScanSelection)
    && matchesDetectorSelection(channel, activeScanDetectorSelection);
}

function normalizedNumberExpression(rawValue) {
  const text = rawValue.trim();
  if (!text || ["any", "all"].includes(text.toLowerCase())) return null;
  if (!isValidNumberExpression(text)) return { invalid: true };
  const include = [];
  const exclude = [];
  text.split(",").forEach((rawToken) => {
    const token = rawToken.trim();
    const excluded = token.startsWith("!");
    const body = excluded ? token.slice(1).trim() : token;
    const [startText, endText = startText] = body.split("-").map((item) => item.trim());
    const start = Number.parseInt(startText, 10);
    const end = Number.parseInt(endText, 10);
    const target = excluded ? exclude : include;
    for (let value = start; value <= end; value += 1) target.push(value);
  });
  return { include: include.length ? include : null, exclude };
}

function readFrameFilterSelection() {
  return {
    cobo: normalizedNumberExpression(els.scanCobo.value),
    asad: normalizedNumberExpression(els.scanAsad.value),
    aget: normalizedNumberExpression(els.scanAget.value),
    channel: normalizedNumberExpression(els.scanChannel.value),
  };
}

function matchesFrameFilter(channel) {
  if (!filterOnlyEnabled) return true;
  const selection = readFrameFilterSelection();
  if (Object.values(selection).some((constraint) => constraint?.invalid)) return false;
  if (!matchesSelection(channel, selection)) return false;
  if (!matchesDetectorSelection(channel, readScanDetectorSelection())) return false;
  if (!els.includeFpn.checked && channel.isFpn) return false;

  const minAmplitude = els.thresholdInput.value.trim() === ""
    ? null : Number.parseFloat(els.thresholdInput.value);
  const maxAmplitude = els.maxAmplitudeInput.value.trim() === ""
    ? null : Number.parseFloat(els.maxAmplitudeInput.value);
  const minTb = inputValue(els.minTbInput);
  const maxTb = inputValue(els.maxTbInput);
  return (minAmplitude === null || channel.amplitude >= minAmplitude)
    && (maxAmplitude === null || channel.amplitude <= maxAmplitude)
    && (minTb === null || channel.peakTb >= minTb)
    && (maxTb === null || channel.peakTb <= maxTb);
}

function filteredGroupTree(groups) {
  if (!filterOnlyEnabled) return groups || [];
  return (groups || []).map((cobo) => {
    const asads = cobo.asads.map((asad) => {
      const agets = asad.agets.map((aget) => {
        const channels = aget.channels.filter(matchesFrameFilter);
        return {
          ...aget,
          channels,
          count: channels.length,
          maxAmplitude: Math.max(0, ...channels.map((channel) => channel.amplitude)),
        };
      }).filter((aget) => aget.channels.length > 0);
      return {
        ...asad,
        agets,
        count: agets.reduce((total, aget) => total + aget.count, 0),
        maxAmplitude: Math.max(0, ...agets.map((aget) => aget.maxAmplitude)),
      };
    }).filter((asad) => asad.agets.length > 0);
    return {
      ...cobo,
      asads,
      count: asads.reduce((total, asad) => total + asad.count, 0),
      maxAmplitude: Math.max(0, ...asads.map((asad) => asad.maxAmplitude)),
    };
  }).filter((cobo) => cobo.asads.length > 0);
}

function groupChannelCount(groups) {
  return groups.reduce(
    (total, cobo) => total + cobo.asads.reduce(
      (asadTotal, asad) => asadTotal + asad.agets.reduce(
        (agetTotal, aget) => agetTotal + aget.channels.length, 0,
      ), 0,
    ), 0,
  );
}

function displayedChannels() {
  const channels = currentPayload?.event?.channels || [];
  return filterOnlyEnabled ? channels.filter(matchesFrameFilter) : channels;
}

function renderCurrentEventViews() {
  if (!currentPayload) return;
  const { event } = currentPayload;
  const groups = filteredGroupTree(event.groups);
  const channels = displayedChannels();
  const filteredCount = groupChannelCount(groups);
  els.filterOnly.classList.toggle("filter-active", filterOnlyEnabled);
  els.filterOnly.setAttribute("aria-pressed", filterOnlyEnabled ? "true" : "false");
  els.plotSubtitle.textContent = filterOnlyEnabled
    ? `${filteredCount}/${event.channelCount} filtered · ${channels.length} plotted`
    : `${event.selectedCount}/${event.channelCount} channels`;
  renderTree(groups);
  renderTable(channels);
  if (detectorInfoVisible) renderDetectorInfo();
  else drawWaveforms(channels);
}

function isSelected(values, level) {
  const selection = readSelection();
  const keys = level === "cobo"
    ? ["cobo"]
    : level === "asad"
      ? ["cobo", "asad"]
      : level === "aget"
        ? ["cobo", "asad", "aget"]
        : ["cobo", "asad", "aget", "channel"];
  return keys.every((key) => selection[key] === values[key]);
}

async function requestJson(url, options = {}) {
  const response = await fetch(url, options);
  const body = await response.text();
  let payload;
  try {
    payload = JSON.parse(body);
  } catch (error) {
    throw new Error(
      `Server returned ${response.status} ${response.statusText} instead of JSON. `
      + "Restart the viewer server and refresh this page."
    );
  }
  if (!response.ok || payload.error) {
    throw new Error(payload.error || response.statusText);
  }
  return payload;
}

async function postJson(url, payload) {
  return requestJson(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
}

function bookmarkStorageKey(path) {
  return `lilak-get-viewer-bookmarks:${path || "no-file"}`;
}

function renderBookmarks() {
  els.eventBookmarks.replaceChildren();
  bookmarkedEvents.forEach((eventIndex) => {
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = `Event ${eventIndex}`;
    button.title = `Go to event ${eventIndex}`;
    button.addEventListener("click", () => navigate(String(eventIndex)));
    els.eventBookmarks.appendChild(button);
  });
}

function loadBookmarks(path) {
  if (bookmarkPath === path) return;
  bookmarkPath = path;
  try {
    const stored = JSON.parse(localStorage.getItem(bookmarkStorageKey(path)) || "[]");
    bookmarkedEvents = Array.isArray(stored)
      ? stored.filter(Number.isInteger).filter((value) => value >= 0)
      : [];
  } catch (_) {
    bookmarkedEvents = [];
  }
  renderBookmarks();
}

function saveBookmarks() {
  try {
    localStorage.setItem(bookmarkStorageKey(bookmarkPath), JSON.stringify(bookmarkedEvents));
  } catch (_) {
    // Bookmarks still work for this page session if browser storage is unavailable.
  }
  renderBookmarks();
}

function closeRootBranchChooser() {
  els.rootBranchChooser.hidden = true;
  els.rootBranchButtons.replaceChildren();
}

function showRootBranchChooser(paths, branches) {
  els.rootBranchFile.textContent = fileNameFromPath(paths[0]);
  els.rootBranchFile.title = paths[0];
  els.rootBranchButtons.replaceChildren();
  branches.forEach((branch) => {
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = branch;
    button.addEventListener("click", () => {
      closeRootBranchChooser();
      openPaths(paths, branch);
    });
    els.rootBranchButtons.appendChild(button);
  });
  els.rootBranchChooser.hidden = false;
}

async function openPaths(paths, rootBranch = null) {
  if (!paths.length) return;
  setBusy(true);
  try {
    setStatus(paths.length > 1 ? `Opening ${paths.length} files...` : "Opening file...");
    if (!rootBranch && paths.length === 1 && paths[0].toLowerCase().endsWith(".root")) {
      const info = await postJson("/api/root-info", { path: paths[0] });
      if ((info.branches || []).length > 1) {
        showRootBranchChooser(paths, info.branches);
        setStatus("Choose the ROOT waveform branch");
        return;
      }
      if (!(info.branches || []).length) {
        throw new Error("ROOT event tree has neither RawData nor SiChannel");
      }
      [rootBranch] = info.branches;
    }
    const payload = await postJson("/api/open", {
      paths, rootBranch, selection: readSelection(),
    });
    activeScanSelection = null;
    activeScanDetectorSelection = null;
    useDefaultFullScale();
    renderPayload(payload);
    const branchMessage = payload.status.rootBranch ? ` (${payload.status.rootBranch})` : "";
    setStatus(paths.length > 1 ? `Merged ${paths.length} sources` : `File loaded${branchMessage}`, "good");
  } catch (error) {
    setStatus(error.message, "error");
  } finally {
    setBusy(false);
  }
}

function formatFileSize(size) {
  if (!Number.isFinite(size) || size <= 0) return "";
  const units = ["B", "KB", "MB", "GB", "TB"];
  let value = size;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }
  return `${value >= 10 || unit === 0 ? value.toFixed(0) : value.toFixed(1)} ${units[unit]}`;
}

function refreshBrowserSelection() {
  const count = browserSelectedPaths.length;
  els.browserOpen.disabled = count === 0;
  if (count === 0) els.browserSelection.textContent = "Select one or more files";
  else if (count === 1) els.browserSelection.textContent = browserSelectedPaths[0];
  else els.browserSelection.textContent = `${count} files will be merged by event id`;
}

function toggleBrowserFile(path, row) {
  const at = browserSelectedPaths.indexOf(path);
  if (at >= 0) browserSelectedPaths.splice(at, 1);
  else browserSelectedPaths.push(path);
  row.classList.toggle("selected", at < 0);
  refreshBrowserSelection();
}

function selectVisibleBrowserFiles() {
  browserSelectedPaths = [];
  els.browserRows.querySelectorAll(".browser-entry[data-path]").forEach((row) => {
    browserSelectedPaths.push(row.dataset.path);
    row.classList.add("selected");
  });
  refreshBrowserSelection();
}

function renderBrowserEntries() {
  const query = els.browserSearch.value.trim().toLowerCase();
  const entries = query
    ? browserEntries.filter((entry) => !entry.isDirectory && entry.name.toLowerCase().includes(query))
    : browserEntries;
  els.browserRows.replaceChildren();
  entries.forEach((entry) => {
    const row = document.createElement("button");
    row.type = "button";
    row.className = "browser-entry";
    if (!entry.isDirectory) {
      row.dataset.path = entry.path;
      row.classList.toggle("selected", browserSelectedPaths.includes(entry.path));
    }

    const name = document.createElement("span");
    name.className = "browser-name";
    name.textContent = `${entry.isDirectory ? "📁" : "📄"} ${entry.name}`;
    const size = document.createElement("span");
    size.className = "browser-size";
    size.textContent = entry.isDirectory ? "Directory" : formatFileSize(entry.size);
    const eventRange = document.createElement("span");
    eventRange.className = "browser-event-range";
    eventRange.textContent = (
      Number.isInteger(entry.minEvent) && Number.isInteger(entry.maxEvent)
        ? `${entry.minEvent.toLocaleString()} – ${entry.maxEvent.toLocaleString()}`
        : ""
    );
    row.append(name, eventRange, size);

    if (entry.isDirectory) {
      row.addEventListener("click", () => browseDirectory(entry.path));
    } else {
      row.addEventListener("click", () => toggleBrowserFile(entry.path, row));
      row.addEventListener("dblclick", () => {
        els.fileBrowser.hidden = true;
        openPaths([entry.path]);
      });
    }
    els.browserRows.appendChild(row);
  });
  if (!entries.length) {
    els.browserRows.textContent = query ? "No matching files." : "This directory is empty.";
  }
  els.browserSelectAll.disabled = !entries.some((entry) => !entry.isDirectory);
}

async function browseDirectory(path) {
  els.browserRows.textContent = "Loading...";
  browserSelectedPaths = [];
  refreshBrowserSelection();
  try {
    const payload = await requestJson(`/api/files?path=${encodeURIComponent(path || ".")}`);
    lastBrowserPath = payload.path;
    browserParent = payload.parent;
    els.browserPath.value = payload.path;
    els.browserUp.disabled = !browserParent;
    browserEntries = payload.entries;
    renderBrowserEntries();
  } catch (error) {
    browserEntries = [];
    els.browserRows.textContent = error.message;
  }
}

function closeFileBrowser() {
  els.fileBrowser.hidden = true;
}

function activateRightTab(name) {
  const electronicsActive = name === "electronics";
  els.electronicsTab.classList.toggle("active", electronicsActive);
  els.channelsTab.classList.toggle("active", !electronicsActive);
  els.electronicsPanel.classList.toggle("active", electronicsActive);
  els.channelsPanel.classList.toggle("active", !electronicsActive);
  els.electronicsPanel.hidden = !electronicsActive;
  els.channelsPanel.hidden = electronicsActive;
}

async function navigate(action) {
  setBusy(true);
  try {
    const payload = await postJson("/api/navigate", { action, selection: readSelection() });
    activeScanSelection = null;
    activeScanDetectorSelection = null;
    renderPayload(payload);
    setStatus(`Event ${payload.event.index}`);
    return true;
  } catch (error) {
    setStatus(error.message, "error");
    return false;
  } finally {
    setBusy(false);
  }
}

function stopAutoNext(message = "") {
  if (autoNextTimer !== null) window.clearTimeout(autoNextTimer);
  autoNextTimer = null;
  els.autoNextButton.textContent = "Auto";
  els.autoNextButton.classList.remove("running");
  if (message) setStatus(message);
}

function autoNextDelayMs() {
  const seconds = Number.parseFloat(els.autoNextInterval.value);
  return Number.isFinite(seconds) && seconds >= 0.1 ? seconds * 1000 : null;
}

async function runAutoNext() {
  if (autoNextTimer === null) return;
  const loaded = await navigate("next");
  if (!loaded) {
    stopAutoNext();
    return;
  }
  const delay = autoNextDelayMs();
  if (delay === null) {
    stopAutoNext();
    setStatus("Auto-next interval must be at least 0.1 seconds.", "error");
    return;
  }
  autoNextTimer = window.setTimeout(runAutoNext, delay);
}

function startAutoNext() {
  const delay = autoNextDelayMs();
  if (delay === null) {
    setStatus("Auto-next interval must be at least 0.1 seconds.", "error");
    return;
  }
  els.autoNextButton.textContent = "Stop";
  els.autoNextButton.classList.add("running");
  autoNextTimer = window.setTimeout(runAutoNext, delay);
  setStatus(`Auto-next every ${delay / 1000} seconds`, "good");
}

async function scanSignal(direction) {
  setBusy(true);
  try {
    const invalidControl = electronicScanControls.find(
      (control) => !isValidNumberExpression(control.value),
    );
    if (invalidControl) {
      updateFilterHighlights();
      invalidControl.focus();
      throw new Error("Use comma-separated numbers, ranges, or ! exclusions in the red filter field.");
    }
    setStatus("Scanning...");
    const selection = readScanSelection();
    const minAmplitude = Number.parseFloat(els.thresholdInput.value || "0");
    const maxRaw = els.maxAmplitudeInput.value.trim();
    const maxAmplitude = maxRaw === "" ? null : Number.parseFloat(maxRaw);
    if (maxAmplitude !== null && maxAmplitude < minAmplitude) {
      throw new Error("Max amplitude must be greater than or equal to Min amplitude.");
    }
    const minTb = inputValue(els.minTbInput);
    const maxTb = inputValue(els.maxTbInput);
    if (minTb !== null && maxTb !== null && maxTb < minTb) {
      throw new Error("Maximum TB must be greater than or equal to minimum TB.");
    }
    const minChannels = inputValue(els.minChannelCountInput);
    const maxChannels = inputValue(els.maxChannelCountInput);
    if (minChannels !== null && maxChannels !== null && maxChannels < minChannels) {
      throw new Error("Maximum #Ch must be greater than or equal to minimum #Ch.");
    }
    const payload = await postJson("/api/scan", {
      selection,
      detectorSelection: readScanDetectorSelection(),
      scope: "selection",
      threshold: minAmplitude,
      maxAmplitude,
      minTb,
      maxTb,
      minChannels,
      maxChannels,
      includeFpn: els.includeFpn.checked,
      maxEvents: Number.parseInt(els.scanLimit.value || "10000", 10),
      direction,
    });
    if (payload.found) {
      renderPayload(payload.event);
      const m = payload.match;
      setStatus(
        `Found ${direction === "backward" ? "previous" : "next"} event ${payload.event.event.index}: C${m.cobo} A${m.asad} G${m.aget} Ch${m.channel}, amp ${m.amplitude}`,
        "good",
      );
    } else {
      setStatus(`${payload.message}; scanned ${payload.scanned}`, payload.eof ? "error" : "");
    }
  } catch (error) {
    setStatus(error.message, "error");
  } finally {
    setBusy(false);
  }
}

function renderPayload(payload) {
  currentPayload = payload;
  const status = payload.status;
  const event = payload.event;
  hoveredChannelKey = selectedSingleChannelKey(event.channels);
  activeScanSelection = event.scanSelection || activeScanSelection;
  activeScanDetectorSelection = event.scanDetectorSelection || activeScanDetectorSelection;
  updateMappingControls(status);
  loadBookmarks(status.path);
  const sourceSuffix = status.sourceCount > 1 ? ` (+${status.sourceCount - 1} sources)` : "";
  const branchSuffix = status.rootBranch ? ` [${status.rootBranch}]` : "";
  els.fileLabel.textContent = `${compactPath(status.path)}${sourceSuffix}${branchSuffix}`;
  els.fileLabel.title = (status.paths || [status.path]).filter(Boolean).join("\n");
  els.pathInput.value = status.path || els.pathInput.value;
  els.jumpInput.value = event.index;
  els.frameInput.value = event.eventIdx;
  els.plotTitle.textContent = `Event ${event.index}`;
  els.plotFileName.textContent = `${displayedFileName(status)}${branchSuffix}`;
  els.plotFileName.title = status.path || "";
  els.showAll.classList.toggle("filter-active", event.selectedCount < event.channelCount);
  renderCurrentEventViews();
}

function makeRow(level, values, name, count, amp, className = "", mapping = {}, collapsible = null) {
  const row = document.createElement("div");
  row.className = `tree-row tree-${level} ${className}`.trim();
  if (isSelected(values, level)) row.classList.add("selected");
  row.innerHTML = `
    <span class="name"><span class="row-name"></span></span>
    <span class="count"></span>
    <span class="amp"></span>
  `;
  row.querySelector(".row-name").textContent = name;
  row.querySelector(".count").textContent = count;
  row.querySelector(".amp").textContent = amp.toFixed(0);
  if (collapsible?.hasChildren) {
    const collapsed = collapsedTreeNodes.has(collapsible.key);
    const toggle = document.createElement("button");
    toggle.type = "button";
    toggle.className = "tree-toggle";
    toggle.textContent = collapsed ? "▶" : "▼";
    toggle.title = `${collapsed ? "Expand" : "Collapse"} ${name}`;
    toggle.setAttribute("aria-label", toggle.title);
    toggle.setAttribute("aria-expanded", collapsed ? "false" : "true");
    toggle.addEventListener("click", (event) => {
      event.stopPropagation();
      if (collapsed) collapsedTreeNodes.delete(collapsible.key);
      else collapsedTreeNodes.add(collapsible.key);
      renderCurrentEventViews();
    });
    row.querySelector(".name").prepend(toggle);
  }
  row.addEventListener("click", () => {
    writeSelection(values);
    navigate("current");
  });
  return row;
}

function renderTree(groups) {
  els.groupTree.replaceChildren();
  if (!groups || groups.length === 0) {
    const empty = document.createElement("div");
    empty.className = "tree-row";
    empty.textContent = "No channels";
    els.groupTree.appendChild(empty);
    return;
  }

  groups.forEach((cobo) => {
    const coboKey = `cobo:${cobo.cobo}`;
    els.groupTree.appendChild(
      makeRow("cobo", { cobo: cobo.cobo, asad: null, aget: null, channel: null }, `Cobo ${cobo.cobo}`, cobo.count, cobo.maxAmplitude, "", cobo, { key: coboKey, hasChildren: cobo.asads.length > 0 }),
    );
    if (collapsedTreeNodes.has(coboKey)) return;
    cobo.asads.forEach((asad) => {
      const asadKey = `asad:${cobo.cobo}/${asad.asad}`;
      els.groupTree.appendChild(
        makeRow(
          "asad",
          { cobo: cobo.cobo, asad: asad.asad, aget: null, channel: null },
          `AsAd ${asad.asad}`,
          asad.count,
          asad.maxAmplitude,
          "",
          asad,
          { key: asadKey, hasChildren: asad.agets.length > 0 },
        ),
      );
      if (collapsedTreeNodes.has(asadKey)) return;
      asad.agets.forEach((aget) => {
        const agetKey = `aget:${cobo.cobo}/${asad.asad}/${aget.aget}`;
        els.groupTree.appendChild(
          makeRow(
            "aget",
            { cobo: cobo.cobo, asad: asad.asad, aget: aget.aget, channel: null },
            `AGET ${aget.aget}`,
            aget.count,
            aget.maxAmplitude,
            "",
            aget,
            { key: agetKey, hasChildren: aget.channels.length > 0 },
          ),
        );
        if (collapsedTreeNodes.has(agetKey)) return;
        aget.channels.forEach((channel) => {
          els.groupTree.appendChild(
            makeRow(
              "channel",
              { cobo: cobo.cobo, asad: asad.asad, aget: aget.aget, channel: channel.channel },
              `Ch ${channel.channel}`,
              channel.isFpn ? "FPN" : "",
              channel.amplitude,
              channel.isFpn ? "fpn" : "",
              channel,
            ),
          );
        });
      });
    });
  });
}

function channelKey(channel) {
  return `${channel.cobo}/${channel.asad}/${channel.aget}/${channel.channel}`;
}

function selectedSingleChannelKey(channels = currentPayload?.event?.channels || []) {
  if (channels.length !== 1 || viewSelection.channel === null) return null;
  const channel = channels[0];
  const fields = ["cobo", "asad", "aget", "channel"];
  return fields.every((field) => viewSelection[field] === channel[field])
    ? channelKey(channel)
    : null;
}

function setHoveredChannel(key) {
  if (hoveredChannelKey === key) return;
  hoveredChannelKey = key;
  els.channelRows.querySelectorAll("tr[data-channel-key]").forEach((row) => {
    row.classList.toggle("keyboard-highlight", key !== null && row.dataset.channelKey === key);
  });
  if (currentPayload) drawWaveforms(displayedChannels());
}

function moveChannelHighlight(step) {
  const channels = displayedChannels();
  if (!channels.length) return;
  let index = channels.findIndex((channel) => channelKey(channel) === hoveredChannelKey);
  if (index < 0) index = step > 0 ? 0 : channels.length - 1;
  else index = Math.max(0, Math.min(channels.length - 1, index + step));
  const key = channelKey(channels[index]);
  setHoveredChannel(key);
  const row = els.channelRows.querySelector(`tr[data-channel-key="${key}"]`);
  if (row) row.scrollIntoView({ block: "nearest" });
}

function channelUsesAdcInversion(channel) {
  const rawPedestal = channel.pedestal;
  const storedPedestal = channel.storedPedestal;
  if (Number.isFinite(rawPedestal) && Number.isFinite(storedPedestal)
      && storedPedestal >= 0) {
    const directDistance = Math.abs(rawPedestal - storedPedestal);
    const invertedDistance = Math.abs((4096 - rawPedestal) - storedPedestal);
    return invertedDistance < directDistance;
  }
  return Boolean(channel.inverted);
}

function displayedPedestal(channel) {
  const pedestal = channel.storedPedestal;
  if (!Number.isFinite(pedestal) || pedestal < 0) return pedestal;
  // SiChannel metadata may store the pedestal in the corrected orientation,
  // while the viewer deliberately draws the original ADC samples unchanged.
  return channelUsesAdcInversion(channel) ? 4096 - pedestal : pedestal;
}

function channelSignalInfo(channel) {
  const pedestal = displayedPedestal(channel);
  const time = channel.storedTime;
  const amplitude = channel.storedAmplitude;
  if (!Number.isFinite(pedestal) || pedestal < 0
      || !Number.isFinite(time) || time < 0
      || !Number.isFinite(amplitude) || amplitude < 0) return null;
  const waveform = plottedWaveform(channel);
  const sampleIndex = Math.max(0, Math.min(waveform.length - 1, Math.round(time)));
  const direction = waveform[sampleIndex] < pedestal ? -1 : 1;
  return {
    pedestal, time, amplitude, direction,
    signalLevel: pedestal + direction * amplitude,
  };
}

function channelDetailLines(channel) {
  if (!Number.isFinite(channel.storedPedestal) || channel.storedPedestal < 0) return [];
  const pedestal = displayedPedestal(channel);
  const amplitude = Number.isFinite(channel.storedAmplitude) && channel.storedAmplitude >= 0
    ? channel.storedAmplitude
    : null;
  const signalInfo = channelSignalInfo(channel);
  const signalLevel = signalInfo?.signalLevel ?? null;
  const noise = Number.isFinite(channel.noiseScale) ? Math.abs(channel.noiseScale) : null;
  const signalToNoise = amplitude !== null && noise !== null && noise > 0
    ? amplitude / noise
    : null;
  const noiseRatio = noise !== null && pedestal !== 0
    ? 100 * noise / Math.abs(pedestal)
    : null;
  const details = [
    `CAA/Ch: ${channel.cobo}/${channel.asad}/${channel.aget}/${channel.channel}`,
    `Pedestal: ${pedestal.toFixed(3)}`,
    `Time: ${Number.isFinite(channel.storedTime) && channel.storedTime >= 0 ? channel.storedTime.toFixed(3) : "-"}`,
    `Amplitude: ${amplitude === null ? "-" : amplitude.toFixed(3)}`,
    `Signal level: ${signalLevel === null ? "-" : signalLevel.toFixed(3)}${signalInfo ? ` (pedestal ${signalInfo.direction < 0 ? "-" : "+"} amplitude)` : ""}`,
    `Noise: ${noise === null ? "-" : noise.toFixed(3)}${noiseRatio === null ? "" : ` (${noiseRatio.toFixed(3)}% of pedestal)`}`,
    `S/N ratio: ${signalToNoise === null ? "-" : signalToNoise.toFixed(3)}`,
    `fInverted: ${channel.inverted ? "yes" : "no"}`,
    "ADC display: raw (no inversion)",
  ];
  if (channel.noiseScale < 0) details.push("Noise sign: bipolar pulse flag");
  return details;
}

function moveChannelTooltip(event) {
  if (els.channelTooltip.hidden) return;
  const gap = 14;
  const left = Math.min(event.clientX + gap, window.innerWidth - els.channelTooltip.offsetWidth - 8);
  const top = Math.min(event.clientY + gap, window.innerHeight - els.channelTooltip.offsetHeight - 8);
  els.channelTooltip.style.left = `${Math.max(8, left)}px`;
  els.channelTooltip.style.top = `${Math.max(8, top)}px`;
}

function showChannelTooltip(channel, event) {
  const details = channelDetailLines(channel);
  if (!details.length) return;
  els.channelTooltip.textContent = details.join("\n");
  els.channelTooltip.hidden = false;
  moveChannelTooltip(event);
}

function hideChannelTooltip() {
  els.channelTooltip.hidden = true;
}

function renderTable(channels) {
  hideChannelTooltip();
  els.channelRows.replaceChildren();
  channels.forEach((channel) => {
    const tr = document.createElement("tr");
    if (isSelected(channel, "channel")) tr.classList.add("selected");
    if ((activeScanSelection || activeScanDetectorSelection) && !matchesActiveScan(channel)) {
      tr.classList.add("scan-dimmed");
    }
    tr.innerHTML = `
      <td></td>
      <td></td>
      <td></td>
      <td></td>
      <td></td>
      <td></td>
      <td></td>
    `;
    tr.children[0].textContent = `${channel.cobo}/${channel.asad}/${channel.aget}`;
    tr.children[1].textContent = channel.isFpn ? `${channel.channel} FPN` : channel.channel;
    if (channel.isFpn) tr.children[1].title = "FPN channel";
    tr.children[2].textContent = channel.ringType || "-";
    tr.children[3].textContent = channel.detectorLabel || "-";
    tr.children[4].textContent = channel.strip ?? "-";
    tr.children[5].textContent = channel.amplitude.toFixed(0);
    tr.children[6].textContent = channel.peakTb;
    const details = channelDetailLines(channel);
    if (details.length) tr.setAttribute("aria-label", details.join(", "));
    const key = channelKey(channel);
    tr.dataset.channelKey = key;
    if (hoveredChannelKey === key) tr.classList.add("keyboard-highlight");
    tr.addEventListener("click", () => {
      writeSelection({
        cobo: channel.cobo,
        asad: channel.asad,
        aget: channel.aget,
        channel: channel.channel,
      });
      navigate("current");
    });
    tr.addEventListener("mouseenter", (event) => {
      setHoveredChannel(key);
      showChannelTooltip(channel, event);
    });
    tr.addEventListener("mousemove", moveChannelTooltip);
    tr.addEventListener("mouseleave", () => {
      setHoveredChannel(selectedSingleChannelKey());
      hideChannelTooltip();
    });
    els.channelRows.appendChild(tr);
  });
}

function resizeCanvas() {
  const canvas = els.waveCanvas;
  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  const width = Math.max(320, Math.floor(rect.width));
  const height = Math.max(260, Math.floor(rect.height));
  canvas.width = Math.floor(width * dpr);
  canvas.height = Math.floor(height * dpr);
  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return { ctx, width, height };
}

function niceStep(range, target) {
  const raw = Math.max(range, 1) / Math.max(target, 1);
  const power = Math.pow(10, Math.floor(Math.log10(raw)));
  const normalized = raw / power;
  const multiple = normalized <= 1 ? 1 : normalized <= 2 ? 2 : normalized <= 5 ? 5 : 10;
  return Math.max(1, multiple * power);
}

function strokeWaveform(ctx, waveform, xFor, yFor) {
  ctx.beginPath();
  waveform.forEach((value, tb) => {
    const x = xFor(tb);
    const y = yFor(value);
    if (tb === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();
}

function plottedWaveform(channel) {
  return channel.waveform;
}

function drawWaveforms(channels) {
  const { ctx, width, height } = resizeCanvas();
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, width, height);

  const margin = { left: 56, top: 26, right: 18, bottom: 36 };
  const plotW = width - margin.left - margin.right;
  const plotH = height - margin.top - margin.bottom;

  ctx.strokeStyle = "#cfd6df";
  ctx.lineWidth = 1;
  ctx.strokeRect(margin.left, margin.top, plotW, plotH);

  if (!channels || channels.length === 0) {
    ctx.fillStyle = "#657280";
    ctx.font = "14px sans-serif";
    ctx.textAlign = "center";
    ctx.fillText("No channels in selection", width / 2, height / 2);
    plotGeometry = null;
    return;
  }

  let autoMin = Infinity;
  let autoMax = -Infinity;
  let lastTb = 0;
  channels.forEach((channel) => {
    if (channel.waveform.length - 1 > lastTb) lastTb = channel.waveform.length - 1;
    plottedWaveform(channel).forEach((value) => {
      if (value < autoMin) autoMin = value;
      if (value > autoMax) autoMax = value;
    });
  });
  if (!Number.isFinite(autoMin) || !Number.isFinite(autoMax)) {
    autoMin = 0;
    autoMax = 4096;
  }
  if (autoMax === autoMin) {
    autoMin -= 1;
    autoMax += 1;
  }
  const pad = Math.max(12, (autoMax - autoMin) * 0.08);
  autoMin = Math.max(0, Math.floor(autoMin - pad));
  autoMax = Math.min(4096, Math.ceil(autoMax + pad));
  if (autoMax <= autoMin) autoMax = autoMin + 1;
  if (lastTb <= 0) lastTb = 511;

  const tbMin = plotView ? plotView.tbMin : 0;
  const tbMax = plotView ? plotView.tbMax : 512;
  const yMin = plotView ? plotView.yMin : autoMin;
  const yMax = plotView ? plotView.yMax : autoMax;

  const xFor = (tb) => margin.left + ((tb - tbMin) / (tbMax - tbMin)) * plotW;
  const yFor = (value) => margin.top + (1 - (value - yMin) / (yMax - yMin)) * plotH;

  plotGeometry = {
    margin, plotW, plotH, tbMin, tbMax, yMin, yMax, lastTb,
    autoMin, autoMax,
  };

  ctx.font = "12px sans-serif";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  const yStep = niceStep(yMax - yMin, 6);
  const firstYTick = Math.ceil(yMin / yStep) * yStep;
  for (let value = firstYTick; value <= yMax; value += yStep) {
    const y = yFor(value);
    ctx.strokeStyle = "#e7ebef";
    ctx.beginPath();
    ctx.moveTo(margin.left, y);
    ctx.lineTo(margin.left + plotW, y);
    ctx.stroke();
    ctx.fillStyle = "#657280";
    ctx.fillText(String(Math.round(value)), margin.left - 8, y);
  }

  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  const tbStep = niceStep(tbMax - tbMin, 8);
  for (let tb = Math.ceil(tbMin / tbStep) * tbStep; tb <= tbMax; tb += tbStep) {
    const x = xFor(tb);
    ctx.strokeStyle = "#eef1f4";
    ctx.beginPath();
    ctx.moveTo(x, margin.top);
    ctx.lineTo(x, margin.top + plotH);
    ctx.stroke();
    ctx.fillStyle = "#657280";
    ctx.fillText(String(Math.round(tb)), x, margin.top + plotH + 8);
  }

  // Zoomed traces run past the frame, so keep them inside the axes.
  ctx.save();
  ctx.beginPath();
  ctx.rect(margin.left, margin.top, plotW, plotH);
  ctx.clip();
  const baseAlpha = channels.length > 40 ? 0.34 : channels.length > 12 ? 0.58 : 0.92;
  const baseWidth = channels.length > 20 ? 1 : 1.4;
  let hovered = null;
  channels.forEach((channel, index) => {
    if (channelKey(channel) === hoveredChannelKey) {
      hovered = { channel, index };
      return;
    }
    ctx.strokeStyle = COLORS[index % COLORS.length];
    const scanAlpha = (activeScanSelection || activeScanDetectorSelection) && !matchesActiveScan(channel)
      ? Math.min(baseAlpha, 0.1)
      : baseAlpha;
    ctx.globalAlpha = hoveredChannelKey ? Math.min(scanAlpha, 0.18) : scanAlpha;
    ctx.lineWidth = baseWidth;
    strokeWaveform(ctx, plottedWaveform(channel), xFor, yFor);
  });
  if (hovered) {
    const pedestal = displayedPedestal(hovered.channel);
    if (Number.isFinite(pedestal) && pedestal >= 0) {
      const noise = Number.isFinite(hovered.channel.noiseScale)
        ? Math.abs(hovered.channel.noiseScale)
        : 0;
      if (noise > 0) {
        const bandTop = yFor(pedestal + noise);
        const bandBottom = yFor(pedestal - noise);
        ctx.fillStyle = "#60a5fa";
        ctx.globalAlpha = 0.16;
        ctx.fillRect(margin.left, bandTop, plotW, bandBottom - bandTop);
      }
      ctx.strokeStyle = "#2563a6";
      ctx.globalAlpha = 0.8;
      ctx.lineWidth = 1;
      ctx.setLineDash([6, 4]);
      ctx.beginPath();
      ctx.moveTo(margin.left, yFor(pedestal));
      ctx.lineTo(margin.left + plotW, yFor(pedestal));
      ctx.stroke();
      ctx.setLineDash([]);
    }
    ctx.strokeStyle = COLORS[hovered.index % COLORS.length];
    ctx.globalAlpha = 1;
    ctx.lineWidth = Math.max(2.2, baseWidth + 1.2);
    strokeWaveform(ctx, plottedWaveform(hovered.channel), xFor, yFor);
    const signalInfo = channelSignalInfo(hovered.channel);
    if (signalInfo) {
      const hitX = xFor(signalInfo.time);
      const hitY = yFor(signalInfo.signalLevel);
      const pedestalY = yFor(signalInfo.pedestal);
      ctx.strokeStyle = COLORS[hovered.index % COLORS.length];
      ctx.lineWidth = 1.5;
      ctx.setLineDash([3, 3]);
      ctx.beginPath();
      ctx.moveTo(hitX, pedestalY);
      ctx.lineTo(hitX, hitY);
      ctx.stroke();
      ctx.setLineDash([]);
      ctx.fillStyle = COLORS[hovered.index % COLORS.length];
      ctx.beginPath();
      ctx.arc(hitX, hitY, 4, 0, Math.PI * 2);
      ctx.fill();
      ctx.font = "bold 12px sans-serif";
      ctx.textAlign = hitX > margin.left + plotW * 0.7 ? "right" : "left";
      const labelX = hitX + (ctx.textAlign === "right" ? -7 : 7);
      const lineHeight = 15;
      const noiseScale = Number.isFinite(hovered.channel.noiseScale)
        ? Math.abs(hovered.channel.noiseScale)
        : null;
      const signalToNoise = noiseScale !== null && noiseScale > 0
        ? signalInfo.amplitude / noiseScale
        : null;
      const infoLines = [
        `Time ${signalInfo.time.toFixed(1)}`,
        `Amplitude ${signalInfo.amplitude.toFixed(1)}`,
        `Noise scale ${noiseScale === null ? "-" : noiseScale.toFixed(1)}`,
        `S/N ratio ${signalToNoise === null ? "-" : signalToNoise.toFixed(2)}`,
      ];
      const labelHeight = infoLines.length * lineHeight;
      const roomAbove = hitY - labelHeight - 7 >= margin.top;
      const roomBelow = hitY + 7 + labelHeight <= margin.top + plotH;
      const preferBelow = signalInfo.direction < 0;
      const labelBelow = preferBelow ? (roomBelow || !roomAbove) : (!roomAbove && roomBelow);
      ctx.textBaseline = "top";
      const desiredLabelY = labelBelow ? hitY + 7 : hitY - labelHeight - 7;
      const labelY = Math.max(
        margin.top + 2,
        Math.min(desiredLabelY, margin.top + plotH - labelHeight - 2),
      );
      infoLines.forEach((line, index) => ctx.fillText(line, labelX, labelY + index * lineHeight));
    }
  }
  ctx.restore();
  ctx.globalAlpha = 1;

  drawZoomSelection();

  const legend = channels.slice(0, 8);
  ctx.textAlign = "left";
  ctx.textBaseline = "top";
  ctx.font = "12px sans-serif";
  legend.forEach((channel, index) => {
    const x = margin.left + 8 + index * Math.max(76, Math.min(118, plotW / Math.max(1, legend.length)));
    const y = 8;
    ctx.fillStyle = COLORS[index % COLORS.length];
    ctx.fillRect(x, y + 4, 14, 3);
    ctx.fillStyle = "#18212b";
    ctx.fillText(`${channel.cobo}/${channel.asad}/${channel.aget}/${channel.channel}`, x + 18, y);
  });
}

els.openForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const path = els.pathInput.value.trim();
  if (path) openPaths([path]);
});

els.browseButton.addEventListener("click", () => {
  els.fileBrowser.hidden = false;
  browseDirectory(lastBrowserPath || browseStartPath);
  requestAnimationFrame(() => {
    els.browserSearch.focus();
    els.browserSearch.select();
  });
});
els.browserClose.addEventListener("click", closeFileBrowser);
els.fileBrowser.addEventListener("click", (event) => {
  if (event.target === els.fileBrowser) closeFileBrowser();
});
els.rootBranchCancel.addEventListener("click", closeRootBranchChooser);
els.rootBranchChooser.addEventListener("click", (event) => {
  if (event.target === els.rootBranchChooser) closeRootBranchChooser();
});
els.browserPathForm.addEventListener("submit", (event) => {
  event.preventDefault();
  browseDirectory(els.browserPath.value.trim() || ".");
});
els.browserUp.addEventListener("click", () => {
  if (browserParent) browseDirectory(browserParent);
});
els.browserSearch.addEventListener("input", renderBrowserEntries);
els.browserSearch.addEventListener("keydown", (event) => {
  if (event.key !== "Enter" || event.isComposing) return;
  event.preventDefault();
  if (event.ctrlKey || event.metaKey) {
    if (!els.browserOpen.disabled) els.browserOpen.click();
  } else {
    selectVisibleBrowserFiles();
  }
});
els.browserOpen.addEventListener("click", () => {
  if (!browserSelectedPaths.length) return;
  const paths = browserSelectedPaths.slice();
  closeFileBrowser();
  openPaths(paths);
});
els.browserOpenMapping.addEventListener("click", async () => {
  const path = els.browserPath.value.trim();
  if (!path) return;
  setBusy(true);
  try {
    const payload = await postJson("/api/mapping", { path });
    updateMappingControls(payload.status);
    closeFileBrowser();
    if (currentPayload) await navigate("current");
    setStatus(`Mapping loaded: ${payload.mappingPath}`, "good");
  } catch (error) {
    setStatus(error.message, "error");
  } finally {
    setBusy(false);
  }
});
els.browserSelectAll.addEventListener("click", selectVisibleBrowserFiles);
document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && !els.rootBranchChooser.hidden) {
    closeRootBranchChooser();
    return;
  }
  if (event.key === "Escape" && !els.fileBrowser.hidden) {
    closeFileBrowser();
    return;
  }
  const commandKey = event.ctrlKey || event.metaKey;
  if (commandKey && !event.altKey) {
    const key = event.key.toLowerCase();
    if (key === "o") {
      event.preventDefault();
      els.browseButton.click();
      return;
    }
    if (key === "s") {
      event.preventDefault();
      if (currentPayload) els.saveWaveform.click();
      return;
    }
  }
  const target = event.target;
  const editing = target instanceof HTMLElement && (
    ["INPUT", "SELECT", "TEXTAREA", "BUTTON"].includes(target.tagName) || target.isContentEditable
  );
  if (editing || !els.fileBrowser.hidden || busyCount > 0 || !currentPayload) return;
  if (event.key === "ArrowRight") {
    event.preventDefault();
    navigate("next");
  } else if (event.key === "ArrowLeft") {
    event.preventDefault();
    navigate("previous");
  } else if (event.key === " ") {
    event.preventDefault();
    scanSignal("forward");
  } else if (event.key === "Backspace") {
    event.preventDefault();
    scanSignal("backward");
  } else if (event.key === "ArrowDown") {
    event.preventDefault();
    activateRightTab("channels");
    moveChannelHighlight(1);
  } else if (event.key === "ArrowUp") {
    event.preventDefault();
    activateRightTab("channels");
    moveChannelHighlight(-1);
  } else if (event.key === "Enter") {
    event.preventDefault();
    els.saveEventBookmark.click();
  }
});

els.uploadInput.addEventListener("change", async () => {
  const file = els.uploadInput.files[0];
  if (!file) return;
  setBusy(true);
  try {
    setStatus("Uploading...");
    const form = new FormData();
    form.append("file", file);
    const payload = await requestJson("/api/upload", { method: "POST", body: form });
    activeScanSelection = null;
    activeScanDetectorSelection = null;
    useDefaultFullScale();
    renderPayload(payload);
    setStatus("Upload loaded", "good");
  } catch (error) {
    setStatus(error.message, "error");
  } finally {
    els.uploadInput.value = "";
    setBusy(false);
  }
});

document.querySelectorAll("[data-nav]").forEach((button) => {
  button.addEventListener("click", () => navigate(button.dataset.nav));
});

els.jumpButton.addEventListener("click", () => {
  const value = Number.parseInt(els.jumpInput.value || "0", 10);
  if (Number.isFinite(value) && value >= 0) navigate(String(value));
});
els.jumpInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    event.preventDefault();
    els.jumpButton.click();
  }
});
els.frameButton.addEventListener("click", () => {
  const value = Number.parseInt(els.frameInput.value || "0", 10);
  if (Number.isFinite(value) && value >= 0) navigate(`frame:${value}`);
});
els.frameInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    event.preventDefault();
    els.frameButton.click();
  }
});
els.autoNextButton.addEventListener("click", () => {
  if (autoNextTimer === null) startAutoNext();
  else stopAutoNext("Auto-next stopped");
});
els.clearSavedEvents.addEventListener("click", () => {
  bookmarkedEvents = [];
  saveBookmarks();
  setStatus("Saved events cleared", "good");
});
els.showAll.addEventListener("click", () => {
  filterOnlyEnabled = false;
  writeSelection({ cobo: null, asad: null, aget: null, channel: null });
  navigate("current");
});
els.filterOnly.addEventListener("click", async () => {
  if (!filterOnlyEnabled) {
    const invalidControl = electronicScanControls.find(
      (control) => !isValidNumberExpression(control.value),
    );
    if (invalidControl) {
      updateFilterHighlights();
      invalidControl.focus();
      setStatus("Fix the invalid filter before enabling Filt. only.", "error");
      return;
    }
    filterOnlyEnabled = true;
    writeSelection({ cobo: null, asad: null, aget: null, channel: null });
    const loaded = currentPayload ? await navigate("current") : true;
    if (!currentPayload) renderCurrentEventViews();
    if (loaded) setStatus("Showing only channels that match Filter & Scan", "good");
  } else {
    filterOnlyEnabled = false;
    renderCurrentEventViews();
    setStatus("Filter-only mode off", "good");
  }
});
els.clearScanFilters.addEventListener("click", () => {
  scanFilterControls.forEach((control) => { control.value = ""; });
  els.includeFpn.checked = true;
  activeScanSelection = null;
  activeScanDetectorSelection = null;
  updateFilterHighlights();
  if (currentPayload) {
    renderCurrentEventViews();
  }
  setStatus("Filter & Scan fields cleared", "good");
});
els.detectorInfoButton.addEventListener("click", async () => {
  if (detectorInfoVisible) {
    setDetectorInfoVisible(false);
    return;
  }
  setBusy(true);
  try {
    const payload = await requestJson("/api/mapping-info");
    if (!payload.mappingPath) throw new Error("Load a mapping first");
    detectorInfoMappingPath = payload.mappingPath;
    detectorInfoEntries = payload.entries || [];
    setDetectorInfoVisible(true);
    setStatus(`Showing ${detectorInfoEntries.length} mapped channels`, "good");
  } catch (error) {
    setStatus(error.message, "error");
  } finally {
    setBusy(false);
  }
});
els.saveWaveform.addEventListener("click", () => {
  if (!currentPayload) return;
  const source = els.waveCanvas;
  const dpr = window.devicePixelRatio || 1;
  const headerHeight = Math.round(58 * dpr);
  const exportCanvas = document.createElement("canvas");
  exportCanvas.width = source.width;
  exportCanvas.height = source.height + headerHeight;
  const ctx = exportCanvas.getContext("2d");
  const displayName = displayedFileName(currentPayload.status);
  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, exportCanvas.width, exportCanvas.height);
  ctx.fillStyle = "#18212b";
  ctx.font = `700 ${18 * dpr}px sans-serif`;
  ctx.textBaseline = "top";
  ctx.fillText(`Event ${currentPayload.event.index}`, 16 * dpr, 9 * dpr);
  ctx.fillStyle = "#657280";
  ctx.font = `${12 * dpr}px sans-serif`;
  ctx.fillText(displayName, 16 * dpr, 34 * dpr, exportCanvas.width - 32 * dpr);
  ctx.strokeStyle = "#cfd6df";
  ctx.beginPath();
  ctx.moveTo(0, headerHeight - 0.5 * dpr);
  ctx.lineTo(exportCanvas.width, headerHeight - 0.5 * dpr);
  ctx.stroke();
  ctx.drawImage(source, 0, headerHeight);
  ctx.strokeStyle = "#aab6c3";
  ctx.lineWidth = dpr;
  ctx.strokeRect(0.5 * dpr, 0.5 * dpr, exportCanvas.width - dpr, exportCanvas.height - dpr);
  const link = document.createElement("a");
  link.download = eventDownloadName(currentPayload.status.path, currentPayload.event.index, "png");
  link.href = exportCanvas.toDataURL("image/png");
  link.click();
});
els.saveCsv.addEventListener("click", () => {
  if (!currentPayload) return;
  const { event, status } = currentPayload;
  const channels = event.channels || [];
  if (!channels.length) {
    setStatus("There are no visible channels to save.", "error");
    return;
  }
  const caac = (channel) => `${channel.cobo}/${channel.asad}/${channel.aget}/${channel.channel}`;
  const rows = [
    ["File", displayedFileName(status)],
    ["ROOT branch", status.rootBranch || ""],
    ["Event", event.index],
    ["Frame", event.eventIdx],
    [],
    ["TB", ...channels.map(caac)],
    ["Detector", ...channels.map((channel) => channel.detectorLabel || "")],
    ["Ring", ...channels.map((channel) => channel.ringType || "")],
    ["Strip", ...channels.map((channel) => channel.strip ?? "")],
  ];
  if (status.rootBranch) {
    rows.push(
      ["Stored pedestal", ...channels.map((channel) => channel.storedPedestal ?? "")],
      ["Stored time", ...channels.map((channel) => channel.storedTime ?? "")],
      ["Stored amplitude", ...channels.map((channel) => channel.storedAmplitude ?? "")],
      ["Noise scale", ...channels.map((channel) => channel.noiseScale ?? "")],
    );
  }
  const sampleCount = channels.reduce(
    (maximum, channel) => Math.max(maximum, channel.waveform.length), 0,
  );
  for (let tb = 0; tb < sampleCount; tb += 1) {
    rows.push([tb, ...channels.map((channel) => channel.waveform[tb] ?? "")]);
  }
  const csv = `\ufeff${rows.map((row) => row.map(csvCell).join(",")).join("\r\n")}\r\n`;
  const blob = new Blob([csv], { type: "text/csv;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.download = eventDownloadName(status.path, event.index, "csv");
  link.href = url;
  link.click();
  window.setTimeout(() => URL.revokeObjectURL(url), 1000);
  setStatus(`Saved CSV for Event ${event.index} (${channels.length} channels)`, "good");
});
els.saveEventBookmark.addEventListener("click", () => {
  const eventIndex = currentPayload?.event?.index;
  if (!Number.isInteger(eventIndex)) return;
  if (!bookmarkedEvents.includes(eventIndex)) {
    bookmarkedEvents.push(eventIndex);
    bookmarkedEvents.sort((a, b) => a - b);
    saveBookmarks();
  }
});

// ---- plot zoom and pan -------------------------------------------------
// Drag selects a zoom rectangle; tick strips select just one axis.
function canvasPoint(event) {
  const rect = els.waveCanvas.getBoundingClientRect();
  return { x: event.clientX - rect.left, y: event.clientY - rect.top };
}

function plotRegion(point) {
  if (!plotGeometry) return null;
  const { margin, plotW, plotH } = plotGeometry;
  const insideX = point.x >= margin.left && point.x <= margin.left + plotW;
  const insideY = point.y >= margin.top && point.y <= margin.top + plotH;
  if (insideX && insideY) return "plot";
  if (insideX && point.y > margin.top + plotH) return "x";
  if (insideY && point.x < margin.left) return "y";
  return null;
}

function currentView() {
  const g = plotGeometry;
  return plotView || { tbMin: 0, tbMax: 512, yMin: g.autoMin, yMax: g.autoMax };
}

function applyView(view) {
  const boundedRange = (low, high, limit) => {
    const span = Math.min(limit, Math.max(0.001, high - low));
    const start = Math.max(0, Math.min(limit - span, (low + high - span) / 2));
    return [start, start + span];
  };
  if (!Object.values(view).every(Number.isFinite)) return;
  const [tbMin, tbMax] = boundedRange(view.tbMin, view.tbMax, 512);
  const [yMin, yMax] = boundedRange(view.yMin, view.yMax, 4096);
  plotView = { tbMin, tbMax, yMin, yMax };
  els.autoScale.classList.add("filter-active");
  if (currentPayload) drawWaveforms(displayedChannels());
}

function resetView() {
  plotView = null;
  els.autoScale.classList.remove("filter-active");
  if (currentPayload) drawWaveforms(displayedChannels());
}

els.waveCanvas.addEventListener("wheel", (event) => {
  if (dragState) return;
  const point = canvasPoint(event);
  const region = plotRegion(point);
  if (!region) return;
  event.preventDefault();
  const g = plotGeometry;
  const view = currentView();
  const factor = event.deltaY > 0 ? 1.2 : 1 / 1.2;
  const next = { ...view };
  if (region === "plot" || region === "x") {
    const fraction = (point.x - g.margin.left) / g.plotW;
    const anchor = view.tbMin + fraction * (view.tbMax - view.tbMin);
    next.tbMin = anchor - (anchor - view.tbMin) * factor;
    next.tbMax = anchor + (view.tbMax - anchor) * factor;
  }
  if (region === "plot" || region === "y") {
    const fraction = (point.y - g.margin.top) / g.plotH;
    const anchor = view.yMax - fraction * (view.yMax - view.yMin);
    next.yMin = anchor - (anchor - view.yMin) * factor;
    next.yMax = anchor + (view.yMax - anchor) * factor;
  }
  applyView(next);
}, { passive: false });

let dragState = null;

function clampedPlotPoint(event, g) {
  const point = canvasPoint(event);
  return {
    x: Math.max(g.margin.left, Math.min(g.margin.left + g.plotW, point.x)),
    y: Math.max(g.margin.top, Math.min(g.margin.top + g.plotH, point.y)),
  };
}

function drawZoomSelection() {
  if (!dragState) return;
  const { start, end, region, geometry: g } = dragState;
  const x = region === "y" ? g.margin.left : Math.min(start.x, end.x);
  const y = region === "x" ? g.margin.top : Math.min(start.y, end.y);
  const w = region === "y" ? g.plotW : Math.abs(end.x - start.x);
  const h = region === "x" ? g.plotH : Math.abs(end.y - start.y);
  const ctx = els.waveCanvas.getContext("2d");
  ctx.save();
  ctx.fillStyle = "rgba(37, 99, 166, 0.15)";
  ctx.strokeStyle = "#2563a6";
  ctx.lineWidth = 1;
  ctx.setLineDash([5, 3]);
  ctx.fillRect(x, y, w, h);
  ctx.strokeRect(x, y, w, h);
  ctx.restore();
}

els.waveCanvas.addEventListener("pointerdown", (event) => {
  if (event.button !== 0 || dragState) return;
  const region = plotRegion(canvasPoint(event));
  if (!region) return;
  event.preventDefault();
  const point = clampedPlotPoint(event, plotGeometry);
  dragState = { region, start: point, end: point, view: { ...currentView() },
    geometry: plotGeometry, pointerId: event.pointerId };
  els.waveCanvas.setPointerCapture(event.pointerId);
  els.waveCanvas.classList.add("selecting");
});

els.waveCanvas.addEventListener("pointermove", (event) => {
  if (!dragState || event.pointerId !== dragState.pointerId) return;
  dragState.end = clampedPlotPoint(event, dragState.geometry);
  if (currentPayload) drawWaveforms(displayedChannels());
});

function endDrag(event) {
  if (!dragState || event.pointerId !== dragState.pointerId) return;
  const { start, region, view, geometry: g } = dragState;
  const end = clampedPlotPoint(event, g);
  dragState = null;
  els.waveCanvas.classList.remove("selecting");
  if (els.waveCanvas.hasPointerCapture(event.pointerId)) {
    els.waveCanvas.releasePointerCapture(event.pointerId);
  }
  const useX = region !== "y";
  const useY = region !== "x";
  if (event.type !== "pointerup" || (useX && Math.abs(end.x - start.x) < 4)
      || (useY && Math.abs(end.y - start.y) < 4)) {
    if (currentPayload) drawWaveforms(displayedChannels());
    return;
  }
  const next = { ...view };
  const xValue = (x) => view.tbMin + (x - g.margin.left) / g.plotW * (view.tbMax - view.tbMin);
  const yValue = (y) => view.yMax - (y - g.margin.top) / g.plotH * (view.yMax - view.yMin);
  if (useX) {
    next.tbMin = xValue(Math.min(start.x, end.x));
    next.tbMax = xValue(Math.max(start.x, end.x));
  }
  if (useY) {
    next.yMin = yValue(Math.max(start.y, end.y));
    next.yMax = yValue(Math.min(start.y, end.y));
  }
  applyView(next);
}

els.waveCanvas.addEventListener("pointerup", endDrag);
els.waveCanvas.addEventListener("pointercancel", endDrag);
els.waveCanvas.addEventListener("lostpointercapture", endDrag);
els.waveCanvas.addEventListener("dblclick", (event) => {
  event.preventDefault();
  resetView();
});

els.autoScale.addEventListener("click", resetView);
els.fullScale.addEventListener("click", () => {
  applyView(FULL_SCALE_VIEW);
});
els.scanBackwardButton.addEventListener("click", () => scanSignal("backward"));
els.scanForwardButton.addEventListener("click", () => scanSignal("forward"));
els.electronicsTab.addEventListener("click", () => activateRightTab("electronics"));
els.channelsTab.addEventListener("click", () => activateRightTab("channels"));

window.addEventListener("resize", () => {
  if (currentPayload) drawWaveforms(displayedChannels());
});

requestJson("/api/status")
  .then((payload) => {
    browseStartPath = payload.status.browsePath || ".";
    updateMappingControls(payload.status);
    if ((payload.status.pendingPaths || []).length) {
      return openPaths(payload.status.pendingPaths);
    }
    if (payload.status.path) {
      return navigate("current");
    }
    return null;
  })
  .catch((error) => setStatus(error.message, "error"));
"""


# Phosphor Icons wave-sine-fill, MIT licensed.
FAVICON_SVG = r"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="#2563a6"><path d="M216,40H40A16,16,0,0,0,24,56V200a16,16,0,0,0,16,16H216a16,16,0,0,0,16-16V56A16,16,0,0,0,216,40Zm-4.78,91.44c-16.68,35-31.06,50.56-46.65,50.56-19.68,0-31.39-24.56-43.79-50.56C112,113,101,90,91.43,90c-3.74,0-14.37,4-32.21,41.44a8,8,0,0,1-14.44-6.88C61.46,89.59,75.84,74,91.43,74c19.68,0,31.39,24.56,43.79,50.56C144,143,155,166,164.57,166c3.74,0,14.37-4,32.21-41.44a8,8,0,1,1,14.44,6.88Z"/></svg>"""


STATIC_ASSETS: Dict[str, Tuple[str, str]] = {
    "index.html": ("text/html; charset=utf-8", INDEX_HTML),
    "static/styles.css": ("text/css; charset=utf-8", STYLES_CSS),
    "static/app.js": ("application/javascript; charset=utf-8", APP_JS),
    "static/favicon.svg": ("image/svg+xml", FAVICON_SVG),
}


def main() -> None:
    parser = argparse.ArgumentParser(description="Web-based GET raw frame viewer")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--file", nargs="+", help="open and merge these raw source files on startup")
    parser.add_argument("--mapping", help="mapping directory containing channel_mapping.txt and detector_mapping.txt")
    parser.add_argument("--browse-path", help="initial directory shown by the file browser")
    parser.add_argument("--no-browser", action="store_true", help="do not open a web browser")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    global MAPPING, BROWSE_START_PATH
    if args.mapping:
        MAPPING = DetectorMapping(args.mapping)
    browse_path = Path(args.browse_path or os.getcwd()).expanduser().resolve()
    if not browse_path.is_dir():
        raise NotADirectoryError(str(browse_path))
    BROWSE_START_PATH = str(browse_path)

    if args.self_test:
        run_self_test()
        return

    if args.file:
        startup_paths = [os.path.abspath(os.path.expanduser(path)) for path in args.file]
        if len(startup_paths) == 1 and startup_paths[0].lower().endswith(".root"):
            root_info = probe_root_file(startup_paths[0])
            if len(root_info["branches"]) > 1:
                STATE.pending_paths = startup_paths
            else:
                STATE.open_paths(startup_paths)
        else:
            STATE.open_paths(startup_paths)

    server = ThreadingHTTPServer((args.host, args.port), ViewerHandler)
    port = server.server_address[1]
    url = f"http://{args.host}:{port}/"
    print(f"LK GET web viewer listening on {url}")
    if args.file and not STATE.pending_paths:
        print(f"Opened {len(args.file)} source file(s):")
        for path in args.file:
            print(f"  {os.path.abspath(path)}")
    elif STATE.pending_paths:
        print(f"Waiting for ROOT branch selection: {STATE.pending_paths[0]}")
    if not args.no_browser:
        browser_host = "127.0.0.1" if args.host == "0.0.0.0" else args.host
        webbrowser.open(f"http://{browser_host}:{port}/")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        STATE.close()
        server.server_close()


if __name__ == "__main__":
    main()
