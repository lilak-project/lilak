#!/usr/bin/env python3
import argparse
import bisect
import cgi
import json
import math
import os
import posixpath
import re
import statistics
import threading
import urllib.parse
import webbrowser
from dataclasses import dataclass, field
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
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
                self.channels[key] = detector
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


def frame_size_of(data: bytes, offset: int, block_size: int, frame_type: int) -> int:
    # 0xFF11 is the MFM file header blob, whose size field is little endian.
    if frame_type == 0xFF11:
        return read_le24(data, offset) * block_size
    return read_be24(data, offset) * block_size


def layered_is_little_endian(data: bytes, frame_type: int, block_size: int, size: int) -> bool:
    header_be = read_be16(data, 8) * block_size
    header_le = read_le16(data, 8) * block_size
    use_le = frame_type == 0xFF11
    if use_le and not 20 <= header_le <= size:
        use_le = False
    if not use_le and not 20 <= header_be <= size and 20 <= header_le <= size:
        use_le = True
    return use_le


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
            if layered_is_little_endian(header, frame_type, block_size, frame_size):
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

        p2_block = header[0] & 0x0F
        block_size = 1 if p2_block == 0 else 1 << p2_block
        frame_size = frame_size_of(header, 1, block_size, resolve_frame_type(header, 5))
        if frame_size < 8:
            raise ParseError(f"invalid frame size {frame_size} at byte {offset}")

        rest = handle.read(frame_size - 8)
        if len(rest) != frame_size - 8:
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
            if layered_is_little_endian(data, frame_type, block_size, len(data)):
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
                child_size = read_be24(data, offset + 1) * child_block
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

    entries = []
    for child in target.iterdir():
        try:
            is_directory = child.is_dir()
            stat = child.stat()
        except OSError:
            continue
        entries.append(
            {
                "name": child.name,
                "path": str(child.resolve()),
                "isDirectory": is_directory,
                "size": 0 if is_directory else stat.st_size,
            }
        )
    entries.sort(key=lambda item: (not item["isDirectory"], item["name"].lower()))
    parent = str(target.parent) if target.parent != target else None
    return {"path": str(target), "parent": parent, "entries": entries}


class ViewerState:
    def __init__(self) -> None:
        self.parser = FrameParser()
        self.lock = threading.RLock()
        self.path: Optional[str] = None
        self.paths: List[str] = []
        self.handle = None
        self.source_handles = []
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
        if self.handle is not None:
            self.handle.close()
        for handle in self.source_handles:
            handle.close()
        self.handle = None
        self.source_handles = []
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

    def open_paths(self, paths: List[str], selection: Optional[dict] = None) -> dict:
        with self.lock:
            expanded_paths = [os.path.abspath(os.path.expanduser(path)) for path in paths]
            if not expanded_paths:
                raise RuntimeError("no input files were provided")
            for expanded in expanded_paths:
                if not os.path.isfile(expanded):
                    raise FileNotFoundError(expanded)
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
            # one source. Mirror LKCoboFrameMergerTask: index every source,
            # group all type 1/2 frames by event id, then expose sorted events.
            grouped: Dict[int, List[Tuple[int, int]]] = {}
            self.source_handles = [open(path, "rb") for path in expanded_paths]
            self.truncated_sources = []
            for source_index, handle in enumerate(self.source_handles):
                print(f"indexing source {source_index + 1}/{len(expanded_paths)}: "
                      f"{expanded_paths[source_index]}", flush=True)
                while True:
                    offset = handle.tell()
                    try:
                        frame = self.parser.index_next_frame(handle)
                    except TruncatedFrameError as error:
                        # A run cut off mid frame is routine; keep what is whole.
                        self.truncated_sources.append(
                            {"path": expanded_paths[source_index], "detail": str(error)}
                        )
                        print(f"  incomplete final frame: {error}", flush=True)
                        break
                    if frame is None:
                        break
                    if frame.frame_type in (1, 2):
                        grouped.setdefault(frame.event_idx, []).append((source_index, offset))
                    elif frame.is_layered:
                        grouped.setdefault(frame.event_idx, []).append((source_index, offset))
            self.merged_event_refs = sorted(grouped.items())
            self.eof = True
            print(f"indexed {len(self.merged_event_refs)} events "
                  f"from {len(expanded_paths)} source(s)", flush=True)
            if not self.merged_event_refs:
                raise ParseError("no GET data frames found in input files")
            return self.load_event(0, selection=selection)

    def status(self) -> dict:
        with self.lock:
            indexed_events = (
                len(self.merged_event_refs)
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
                "mappingPath": str(MAPPING.directory) if MAPPING is not None else None,
                "mappingOptions": MAPPING.options if MAPPING is not None else None,
                "browsePath": BROWSE_START_PATH,
            }

    def ensure_open(self) -> None:
        if self.handle is None and not self.source_handles:
            raise RuntimeError("no file is open")

    def ensure_index(self, event_index: int) -> bool:
        self.ensure_open()
        if event_index < 0:
            return False
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
        if self.merged_event_refs is not None:
            event_id, refs = self.merged_event_refs[event_index]
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
            frame = self.read_event_frame(event_index)
            self.current_event = event_index
            channels = unpack_get_event(frame)
            return self.serialize_event(event_index, frame, channels, normalize_selection(selection))

    def event_index_for_frame(self, frame_number: int) -> int:
        self.ensure_open()
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
                frame = self.read_event_frame(event_index)
                channels = unpack_get_event(frame)
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
                        self.current_event = event_index
                        event_payload = self.serialize_event(
                            event_index,
                            frame,
                            channels,
                            normalize_selection({}),
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
    ) -> dict:
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
            if parsed.path == "/api/open":
                # Unmerged CoBo data is one file per source, so the browser
                # sends every selected path and the state merges them by event
                # id. A lone "path" is still accepted.
                requested = payload.get("paths") or [payload["path"]]
                paths = [str(item) for item in requested]
                self.send_json(STATE.open_paths(paths, selection=selection))
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
                min_tb = None if min_tb_value in (None, "") else int(min_tb_value)
                max_tb = None if max_tb_value in (None, "") else int(max_tb_value)
                if min_tb is not None and max_tb is not None and min_tb > max_tb:
                    raise ValueError("maximum TB must be greater than or equal to minimum TB")
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
                    )
                )
                return
            self.send_error_json(RuntimeError(f"unknown endpoint {parsed.path}"), HTTPStatus.NOT_FOUND)
        except Exception as exc:
            self.send_error_json(exc)

    def handle_upload(self) -> None:
        UPLOAD_ROOT.mkdir(parents=True, exist_ok=True)
        form = cgi.FieldStorage(fp=self.rfile, headers=self.headers, environ={"REQUEST_METHOD": "POST"})
        item = form["file"] if "file" in form else None
        if item is None or not item.filename:
            raise RuntimeError("upload field 'file' is missing")
        name = os.path.basename(item.filename)
        target = UPLOAD_ROOT / name
        counter = 1
        while target.exists():
            target = UPLOAD_ROOT / f"{Path(name).stem}_{counter}{Path(name).suffix}"
            counter += 1
        with open(target, "wb") as output:
            while True:
                chunk = item.file.read(1024 * 1024)
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


def run_self_test() -> None:
    import io
    import tempfile

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

    # Unmerged sources may contain frames in non-event order. Verify that the
    # viewer groups them by event id and combines channels across sources.
    with tempfile.TemporaryDirectory(prefix="lk_get_viewer_test_") as directory:
        source0 = Path(directory) / "source0.graw"
        source1 = Path(directory) / "source1.graw"
        source0.write_bytes(
            make_type1_frame(8, 0, 0, [(0, 1, 0, 108)])
            + make_type1_frame(7, 0, 0, [(0, 2, 0, 107)])
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
        state.close()
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
    <link rel="icon" href="/static/favicon.svg" type="image/svg+xml">
    <link rel="stylesheet" href="/static/styles.css">
  </head>
  <body>
    <header class="topbar">
      <div class="brand">
        <svg class="brand-mark" viewBox="0 0 256 256" aria-hidden="true">
          <path d="M216,40H40A16,16,0,0,0,24,56V200a16,16,0,0,0,16,16H216a16,16,0,0,0,16-16V56A16,16,0,0,0,216,40Zm-4.78,91.44c-16.68,35-31.06,50.56-46.65,50.56-19.68,0-31.39-24.56-43.79-50.56C112,113,101,90,91.43,90c-3.74,0-14.37,4-32.21,41.44a8,8,0,0,1-14.44-6.88C61.46,89.59,75.84,74,91.43,74c19.68,0,31.39,24.56,43.79,50.56C144,143,155,166,164.57,166c3.74,0,14.37-4,32.21-41.44a8,8,0,1,1,14.44,6.88Z"></path>
        </svg>
        <div>
          <h1>LILAK GET Web Viewer</h1>
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
        <div class="browser-columns"><span>Name</span><span>Size</span></div>
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
              <span class="info-tooltip">Electronic filters accept comma-separated values, ranges, and exclusions (for example: 10,15-20,!17). Blank fields match any value. Amplitude is the largest absolute signal deviation from the pedestal (median of TB 0&ndash;63), and TB range applies to its peak TB. The scan stops at the first matching event.</span>
            </span>
            <button id="clearScanFilters" type="button">Clear</button>
          </div>
          <div class="filter-group electronic-filter-group">
            <div class="selection-grid">
              <label>Cobo:<input id="scanCobo" type="text" placeholder="Any" title="Example: 0,2-4,!3"></label>
              <label>AsAd:<input id="scanAsad" type="text" placeholder="Any" title="Example: 0,2-3,!2"></label>
              <label>AGET:<input id="scanAget" type="text" placeholder="Any" title="Example: 0,2-3,!2"></label>
              <label>Chan:<input id="scanChannel" type="text" placeholder="Any" title="Example: 10,15-20,!17"></label>
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
          </div>
          <div class="scan-value-row scan-options-row">
            <span class="scan-field-title">Max event:</span>
            <input id="scanLimit" type="number" min="1" value="10000" title="Stop after scanning this many events">
            <label class="inline-check"><span>FPN</span>
              <input id="includeFpn" type="checkbox">
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
            <button id="saveWaveform" type="button">Save PNG</button>
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
              <colgroup><col><col><col><col><col><col></colgroup>
              <thead>
                <tr>
                  <th>CAA</th>
                  <th>Ch</th>
                  <th>Detector</th>
                  <th>Ring</th>
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
#showAll {
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
#saveWaveform {
  border-color: #bcc7d2;
  background: #dde4eb;
  color: var(--text);
}

#showAll.filter-active,
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
  color: var(--blue);
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
  grid-template-columns: minmax(0, 1fr) 110px;
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

.browser-columns span:last-child,
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
  gap: 4px;
  font-size: 13px;
}

.tree-row {
  display: grid;
  grid-template-columns: 1fr auto auto;
  gap: 8px;
  align-items: center;
  min-height: 28px;
  border: 1px solid transparent;
  border-radius: 6px;
  padding: 4px 7px;
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
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
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

.channel-panel col:nth-child(1) { width: 52px; }
.channel-panel col:nth-child(2) { width: 42px; }
.channel-panel col:nth-child(3) { width: 70px; }
.channel-panel col:nth-child(4) { width: 44px; }
.channel-panel col:nth-child(5) { width: 58px; }
.channel-panel col:nth-child(6) { width: 44px; }

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
  saveWaveform: document.getElementById("saveWaveform"),
  waveCanvas: document.getElementById("waveCanvas"),
  detectorInfoPanel: document.getElementById("detectorInfoPanel"),
  detectorInfoRows: document.getElementById("detectorInfoRows"),
  statusLine: document.getElementById("statusLine"),
  channelRows: document.getElementById("channelRows"),
  saveEventBookmark: document.getElementById("saveEventBookmark"),
  eventBookmarks: document.getElementById("eventBookmarks"),
};

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
let detectorInfoVisible = false;
let detectorInfoEntries = [];
let detectorInfoMappingPath = null;
let bookmarkedEvents = [];
let bookmarkPath = null;
// null means the axes follow the data; a view object holds a zoomed range that
// survives event navigation until Autoscale or a double click resets it.
let plotView = null;
let plotGeometry = null;

const scanFilterControls = [
  els.scanCobo, els.scanAsad, els.scanAget, els.scanChannel,
  els.scanRingType, els.scanDetectorType, els.scanDetectorNumber,
  els.thresholdInput, els.maxAmplitudeInput, els.minTbInput, els.maxTbInput,
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
  control.addEventListener("input", updateFilterHighlights);
  control.addEventListener("change", updateFilterHighlights);
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

function waveformDownloadName(path, eventIndex) {
  const fileName = fileNameFromPath(path);
  const runMatch = fileName.match(/^(run_\d+)\.dat\./);
  return runMatch
    ? `${runMatch[1]}.${eventIndex}.png`
    : `${fileName}_event_${eventIndex}.png`;
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
    els.plotSubtitle.textContent = `${event.selectedCount}/${event.channelCount} channels`;
    els.plotFileName.textContent = displayedFileName(status);
    els.plotFileName.title = status.path || "";
    drawWaveforms(event.channels);
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

async function openPaths(paths) {
  if (!paths.length) return;
  setBusy(true);
  try {
    setStatus(paths.length > 1 ? `Opening ${paths.length} files...` : "Opening file...");
    const payload = await postJson("/api/open", { paths, selection: readSelection() });
    activeScanSelection = null;
    activeScanDetectorSelection = null;
    renderPayload(payload);
    setStatus(paths.length > 1 ? `Merged ${paths.length} sources` : "File loaded", "good");
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
    row.append(name, size);

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
    const payload = await postJson("/api/scan", {
      selection,
      detectorSelection: readScanDetectorSelection(),
      scope: "selection",
      threshold: minAmplitude,
      maxAmplitude,
      minTb,
      maxTb,
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
  hoveredChannelKey = null;
  const status = payload.status;
  const event = payload.event;
  activeScanSelection = event.scanSelection || activeScanSelection;
  activeScanDetectorSelection = event.scanDetectorSelection || activeScanDetectorSelection;
  updateMappingControls(status);
  loadBookmarks(status.path);
  const sourceSuffix = status.sourceCount > 1 ? ` (+${status.sourceCount - 1} sources)` : "";
  els.fileLabel.textContent = `${compactPath(status.path)}${sourceSuffix}`;
  els.fileLabel.title = (status.paths || [status.path]).filter(Boolean).join("\n");
  els.pathInput.value = status.path || els.pathInput.value;
  els.jumpInput.value = event.index;
  els.frameInput.value = event.eventIdx;
  els.plotTitle.textContent = `Event ${event.index}`;
  els.plotSubtitle.textContent = `${event.selectedCount}/${event.channelCount} channels`;
  els.plotFileName.textContent = displayedFileName(status);
  els.plotFileName.title = status.path || "";
  els.showAll.classList.toggle("filter-active", event.selectedCount < event.channelCount);
  renderTree(event.groups);
  renderTable(event.channels);
  if (detectorInfoVisible) renderDetectorInfo();
  else drawWaveforms(event.channels);
}

function makeRow(level, values, name, count, amp, className = "", mapping = {}) {
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
    els.groupTree.appendChild(
      makeRow("cobo", { cobo: cobo.cobo, asad: null, aget: null, channel: null }, `Cobo ${cobo.cobo}`, cobo.count, cobo.maxAmplitude, "", cobo),
    );
    cobo.asads.forEach((asad) => {
      els.groupTree.appendChild(
        makeRow(
          "asad",
          { cobo: cobo.cobo, asad: asad.asad, aget: null, channel: null },
          `AsAd ${asad.asad}`,
          asad.count,
          asad.maxAmplitude,
          "",
          asad,
        ),
      );
      asad.agets.forEach((aget) => {
        els.groupTree.appendChild(
          makeRow(
            "aget",
            { cobo: cobo.cobo, asad: asad.asad, aget: aget.aget, channel: null },
            `AGET ${aget.aget}`,
            aget.count,
            aget.maxAmplitude,
            "",
            aget,
          ),
        );
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

function setHoveredChannel(key) {
  if (hoveredChannelKey === key) return;
  hoveredChannelKey = key;
  els.channelRows.querySelectorAll("tr[data-channel-key]").forEach((row) => {
    row.classList.toggle("keyboard-highlight", key !== null && row.dataset.channelKey === key);
  });
  if (currentPayload) drawWaveforms(currentPayload.event.channels);
}

function moveChannelHighlight(step) {
  const channels = currentPayload?.event?.channels || [];
  if (!channels.length) return;
  let index = channels.findIndex((channel) => channelKey(channel) === hoveredChannelKey);
  if (index < 0) index = step > 0 ? 0 : channels.length - 1;
  else index = Math.max(0, Math.min(channels.length - 1, index + step));
  const key = channelKey(channels[index]);
  setHoveredChannel(key);
  const row = els.channelRows.querySelector(`tr[data-channel-key="${key}"]`);
  if (row) row.scrollIntoView({ block: "nearest" });
}

function renderTable(channels) {
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
    `;
    tr.children[0].textContent = `${channel.cobo}/${channel.asad}/${channel.aget}`;
    tr.children[1].textContent = channel.isFpn ? `${channel.channel} FPN` : channel.channel;
    tr.children[2].textContent = channel.detectorLabel || "-";
    tr.children[3].textContent = channel.ringType || "-";
    tr.children[4].textContent = channel.amplitude.toFixed(0);
    tr.children[5].textContent = channel.peakTb;
    tr.addEventListener("click", () => {
      writeSelection({
        cobo: channel.cobo,
        asad: channel.asad,
        aget: channel.aget,
        channel: channel.channel,
      });
      navigate("current");
    });
    const key = channelKey(channel);
    tr.dataset.channelKey = key;
    tr.addEventListener("mouseenter", () => setHoveredChannel(key));
    tr.addEventListener("mouseleave", () => setHoveredChannel(null));
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
    channel.waveform.forEach((value) => {
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
  for (let i = 0; i <= 5; i += 1) {
    const y = margin.top + (i / 5) * plotH;
    const value = yMax - (i / 5) * (yMax - yMin);
    ctx.strokeStyle = i === 0 || i === 5 ? "#cfd6df" : "#e7ebef";
    ctx.beginPath();
    ctx.moveTo(margin.left, y);
    ctx.lineTo(margin.left + plotW, y);
    ctx.stroke();
    ctx.fillStyle = "#657280";
    ctx.fillText(value.toFixed(0), margin.left - 8, y);
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
    strokeWaveform(ctx, channel.waveform, xFor, yFor);
  });
  if (hovered) {
    ctx.strokeStyle = COLORS[hovered.index % COLORS.length];
    ctx.globalAlpha = 1;
    ctx.lineWidth = Math.max(2.2, baseWidth + 1.2);
    strokeWaveform(ctx, hovered.channel.waveform, xFor, yFor);
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
  writeSelection({ cobo: null, asad: null, aget: null, channel: null });
  navigate("current");
});
els.clearScanFilters.addEventListener("click", () => {
  scanFilterControls.forEach((control) => { control.value = ""; });
  els.includeFpn.checked = false;
  activeScanSelection = null;
  activeScanDetectorSelection = null;
  updateFilterHighlights();
  if (currentPayload) {
    renderTable(currentPayload.event.channels);
    if (!detectorInfoVisible) drawWaveforms(currentPayload.event.channels);
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
  link.download = waveformDownloadName(currentPayload.status.path, currentPayload.event.index);
  link.href = exportCanvas.toDataURL("image/png");
  link.click();
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
  if (currentPayload) drawWaveforms(currentPayload.event.channels);
}

function resetView() {
  plotView = null;
  els.autoScale.classList.remove("filter-active");
  if (currentPayload) drawWaveforms(currentPayload.event.channels);
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
  if (currentPayload) drawWaveforms(currentPayload.event.channels);
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
    if (currentPayload) drawWaveforms(currentPayload.event.channels);
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
  applyView({ tbMin: 0, tbMax: 512, yMin: 0, yMax: 4096 });
});
els.scanBackwardButton.addEventListener("click", () => scanSignal("backward"));
els.scanForwardButton.addEventListener("click", () => scanSignal("forward"));
els.electronicsTab.addEventListener("click", () => activateRightTab("electronics"));
els.channelsTab.addEventListener("click", () => activateRightTab("channels"));

window.addEventListener("resize", () => {
  if (currentPayload) drawWaveforms(currentPayload.event.channels);
});

requestJson("/api/status")
  .then((payload) => {
    browseStartPath = payload.status.browsePath || ".";
    updateMappingControls(payload.status);
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
        STATE.open_paths(args.file)

    server = ThreadingHTTPServer((args.host, args.port), ViewerHandler)
    port = server.server_address[1]
    url = f"http://{args.host}:{port}/"
    print(f"LK GET web viewer listening on {url}")
    if args.file:
        print(f"Opened {len(args.file)} source file(s):")
        for path in args.file:
            print(f"  {os.path.abspath(path)}")
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
