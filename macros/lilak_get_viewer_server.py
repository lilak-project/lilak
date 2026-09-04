#!/usr/bin/env python3
import argparse
import cgi
import json
import math
import os
import posixpath
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


def channel_matches_tuple(key: Tuple[int, int, int, int], selection: dict) -> bool:
    cobo, asad, aget, chan = key
    return (
        (selection.get("cobo") is None or cobo == selection["cobo"])
        and (selection.get("asad") is None or asad == selection["asad"])
        and (selection.get("aget") is None or aget == selection["aget"])
        and (selection.get("channel") is None or chan == selection["channel"])
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

    def navigate(self, action: str, selection: Optional[dict] = None) -> dict:
        with self.lock:
            if action == "first":
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
        include_fpn: bool,
        max_events: int,
    ) -> dict:
        with self.lock:
            scan_selection = scope_selection(selection, scope)
            start_index = self.current_event + 1 if self.current_event >= 0 else 0
            scanned = 0
            event_index = start_index
            while scanned < max_events:
                if not self.ensure_index(event_index):
                    return {
                        "found": False,
                        "scanned": scanned,
                        "eof": self.eof,
                        "message": "end of file",
                    }
                frame = self.read_event_frame(event_index)
                channels = unpack_get_event(frame)
                for key, values in sorted(channels.items()):
                    if not include_fpn and key[3] in FPN_CHANNELS:
                        continue
                    if not channel_matches_tuple(key, scan_selection):
                        continue
                    stats = analyze_waveform(values)
                    if stats["amplitude"] >= threshold:
                        self.current_event = event_index
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
                            "event": self.serialize_event(event_index, frame, channels, scan_selection),
                        }
                scanned += 1
                event_index += 1
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
            all_summaries.append(summary)
            if channel_matches_tuple(key, selection):
                selected_channels.append({**summary, "waveform": values})

        first_data_frame = next(iter(iter_data_frames(frame)), frame)
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
            }
        )
    return cobo_nodes


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
        try:
            parsed = urllib.parse.urlparse(self.path)
            if parsed.path == "/api/upload":
                self.handle_upload()
                return

            payload = self.read_json()
            selection = normalize_selection(payload.get("selection"))
            if parsed.path == "/api/open":
                # Unmerged CoBo data is one file per source, so the browser
                # sends every selected path and the state merges them by event
                # id. A lone "path" is still accepted.
                requested = payload.get("paths") or [payload["path"]]
                paths = [str(item) for item in requested]
                self.send_json(STATE.open_paths(paths, selection=selection))
                return
            if parsed.path == "/api/navigate":
                self.send_json(STATE.navigate(str(payload.get("action", "current")), selection=selection))
                return
            if parsed.path == "/api/scan":
                self.send_json(
                    STATE.scan_signal(
                        selection=selection,
                        scope=str(payload.get("scope", "channel")),
                        threshold=float(payload.get("threshold", 50)),
                        include_fpn=bool(payload.get("includeFpn", False)),
                        max_events=max(1, int(payload.get("maxEvents", 10000))),
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
    <title>LK GET Web Viewer</title>
    <link rel="stylesheet" href="/static/styles.css">
  </head>
  <body>
    <header class="topbar">
      <div class="brand">
        <span class="brand-mark"></span>
        <div>
          <h1>LK GET Web Viewer</h1>
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
        <div class="browser-columns"><span>Name</span><span>Size</span></div>
        <div id="browserRows" class="browser-rows"></div>
        <div class="browser-footer">
          <span id="browserSelection">Select one or more files</span>
          <button id="browserSelectAll" type="button" class="secondary" disabled>Select All</button>
          <button id="browserOpen" type="button" disabled>Open selected</button>
        </div>
      </section>
    </div>

    <main class="workspace">
      <aside class="side-panel">
        <section class="control-band">
          <div class="section-title">Event</div>
          <div class="nav-row">
            <button data-nav="first">First</button>
            <button data-nav="previous">Prev</button>
            <button data-nav="next">Next</button>
          </div>
          <div class="jump-row">
            <input id="jumpInput" type="number" min="0" placeholder="Event index">
            <button id="jumpButton" type="button">Go</button>
          </div>
          <div class="auto-next-row">
            <label><input id="autoNextInterval" type="number" min="0.1" step="0.1" value="1.0"><span>sec</span></label>
            <button id="autoNextButton" type="button">Auto Next</button>
          </div>
          <div class="event-readout">
            <span id="eventIndex">-</span>
            <span id="eventMeta">-</span>
          </div>
        </section>

        <section class="control-band">
          <div class="section-title">Signal Scan</div>
          <div class="scan-row">
            <label class="field-label"><span>Scan scope</span>
              <select id="scanScope" title="Search within the selected channel, AGET, AsAd, or CoBo">
                <option value="channel">Channel</option>
                <option value="aget">AGET</option>
                <option value="asad">AsAd</option>
                <option value="cobo">Cobo</option>
                <option value="selection">Selection</option>
              </select>
            </label>
            <label class="field-label"><span>Min amplitude</span>
              <input id="thresholdInput" type="number" min="0" value="50" title="Minimum absolute deviation from the pedestal">
            </label>
          </div>
          <div class="scan-row">
            <label class="field-label"><span>Max events</span>
              <input id="scanLimit" type="number" min="1" value="10000" title="Stop after scanning this many events">
            </label>
            <label class="field-label"><span>Include FPN</span>
              <span class="check-label"><input id="includeFpn" type="checkbox"> Ch 11, 22, 45, 56</span>
            </label>
          </div>
          <p class="scan-help">Scope uses the Cobo/AsAd/AGET/Chan selection in the right panel; a parent scope scans all children. Amplitude is the largest absolute signal deviation from the pedestal (median of TB 0–63). The scan stops at the first matching event.</p>
          <button id="scanButton" class="wide-button" type="button">Scan Signal</button>
        </section>

        <section class="tree-panel">
          <div class="section-title">Cobo / AsAd / AGET</div>
          <div id="groupTree" class="group-tree"></div>
        </section>
      </aside>

      <section class="plot-panel">
        <div class="plot-toolbar">
          <div>
            <strong id="plotTitle">Waveforms</strong>
            <span id="plotSubtitle"></span>
            <span class="plot-hint">wheel zooms, drag pans, double click resets; over a tick strip only that axis moves</span>
          </div>
          <div class="plot-actions">
            <button id="autoScale" type="button">Autoscale</button>
            <button id="showAll" type="button">Show Event</button>
          </div>
        </div>
        <canvas id="waveCanvas"></canvas>
        <div id="statusLine" class="status-line">Ready</div>
      </section>

      <aside class="detail-panel">
        <section class="control-band">
          <div class="section-title">Selection</div>
          <div class="selection-grid">
            <label>Cobo<input id="selCobo" type="number" min="0" placeholder="Any"></label>
            <label>AsAd<input id="selAsad" type="number" min="0" placeholder="Any"></label>
            <label>AGET<input id="selAget" type="number" min="0" placeholder="Any"></label>
            <label>Chan<input id="selChannel" type="number" min="0" max="67" placeholder="Any"></label>
          </div>
          <div class="action-row selection-actions">
            <button id="applySelection" type="button">Apply</button>
            <button id="clearSelection" type="button">Clear</button>
            <button id="restoreScanSelection" type="button" class="secondary" disabled>Restore Scan</button>
          </div>
        </section>

        <section class="channel-panel">
          <div class="section-title">Channels</div>
          <div class="table-wrap">
            <table>
              <thead>
                <tr>
                  <th>CAA</th>
                  <th>Ch</th>
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
.nav-row button,
#clearSelection,
#autoScale,
#showAll {
  background: #ffffff;
  color: var(--text);
}

button:hover,
.upload-button:hover {
  border-color: var(--blue);
}

button:disabled {
  opacity: 0.55;
  cursor: wait;
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
  width: 20px;
  height: 20px;
  border-radius: 4px;
  background: linear-gradient(135deg, var(--blue), var(--teal) 55%, var(--amber));
  flex: 0 0 auto;
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
  grid-template-rows: auto auto auto minmax(0, 1fr) auto;
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
  border-top: 1px solid var(--line);
  border-bottom: 0;
}

#browserSelection {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.workspace {
  height: calc(100vh - 68px);
  display: grid;
  grid-template-columns: 300px minmax(360px, 1fr) 340px;
  grid-template-rows: 100%;
  overflow: hidden;
}

.side-panel,
.detail-panel {
  min-width: 0;
  overflow-y: auto;
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

.nav-row,
.jump-row,
.auto-next-row,
.action-row,
.scan-row {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 8px;
  margin-bottom: 8px;
}

.jump-row {
  grid-template-columns: 1fr auto;
}

.auto-next-row {
  grid-template-columns: 1fr auto;
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

.selection-actions {
  grid-template-columns: 1fr 1fr 1.35fr;
}

.event-readout {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 8px;
  padding-top: 4px;
}

#eventIndex {
  font-size: 30px;
  font-weight: 750;
  color: var(--blue);
}

#eventMeta {
  color: var(--muted);
  font-size: 12px;
  text-align: right;
}

.selection-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
  margin-bottom: 8px;
}

.selection-grid label {
  display: grid;
  gap: 4px;
  font-size: 12px;
  color: var(--muted);
}

.field-label {
  display: grid;
  gap: 4px;
  color: var(--muted);
  font-size: 12px;
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

.scan-help {
  margin: 2px 0 10px;
  color: var(--muted);
  font-size: 11px;
  line-height: 1.4;
}

.wide-button {
  width: 100%;
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
  background: var(--panel);
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

.plot-hint {
  display: block;
  color: var(--muted);
  font-size: 11px;
}

.table-wrap {
  overflow: auto;
  flex: 1 1 auto;
}

#waveCanvas {
  touch-action: none;
  cursor: crosshair;
}

#waveCanvas.panning {
  cursor: grabbing;
}

table {
  border-collapse: collapse;
  width: 100%;
  font-size: 13px;
}

th,
td {
  padding: 7px 6px;
  text-align: right;
  border-bottom: 1px solid var(--line);
  font-variant-numeric: tabular-nums;
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

tbody tr:hover {
  background: #eef4fb;
  cursor: pointer;
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
    min-height: calc(100vh - 86px);
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
  browserUp: document.getElementById("browserUp"),
  browserRows: document.getElementById("browserRows"),
  browserSelection: document.getElementById("browserSelection"),
  browserOpen: document.getElementById("browserOpen"),
  browserSelectAll: document.getElementById("browserSelectAll"),
  eventIndex: document.getElementById("eventIndex"),
  eventMeta: document.getElementById("eventMeta"),
  jumpInput: document.getElementById("jumpInput"),
  jumpButton: document.getElementById("jumpButton"),
  autoNextInterval: document.getElementById("autoNextInterval"),
  autoNextButton: document.getElementById("autoNextButton"),
  selCobo: document.getElementById("selCobo"),
  selAsad: document.getElementById("selAsad"),
  selAget: document.getElementById("selAget"),
  selChannel: document.getElementById("selChannel"),
  applySelection: document.getElementById("applySelection"),
  clearSelection: document.getElementById("clearSelection"),
  restoreScanSelection: document.getElementById("restoreScanSelection"),
  scanScope: document.getElementById("scanScope"),
  thresholdInput: document.getElementById("thresholdInput"),
  scanLimit: document.getElementById("scanLimit"),
  includeFpn: document.getElementById("includeFpn"),
  scanButton: document.getElementById("scanButton"),
  groupTree: document.getElementById("groupTree"),
  plotTitle: document.getElementById("plotTitle"),
  plotSubtitle: document.getElementById("plotSubtitle"),
  autoScale: document.getElementById("autoScale"),
  showAll: document.getElementById("showAll"),
  waveCanvas: document.getElementById("waveCanvas"),
  statusLine: document.getElementById("statusLine"),
  channelRows: document.getElementById("channelRows"),
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
let autoNextTimer = null;
let scanSelectionSnapshot = null;
let hoveredChannelKey = null;
// null means the axes follow the data; a view object holds a zoomed range that
// survives event navigation until Autoscale or a double click resets it.
let plotView = null;
let plotGeometry = null;

function setBusy(isBusy) {
  busyCount += isBusy ? 1 : -1;
  if (busyCount < 0) busyCount = 0;
  document.querySelectorAll("button, input, select").forEach((el) => {
    if (el.id !== "pathInput") el.disabled = busyCount > 0;
  });
  if (busyCount === 0 && scanSelectionSnapshot === null) {
    els.restoreScanSelection.disabled = true;
  }
}

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

function inputValue(input) {
  const raw = input.value.trim();
  if (raw === "") return null;
  const parsed = Number.parseInt(raw, 10);
  return Number.isFinite(parsed) ? parsed : null;
}

function readSelection() {
  return {
    cobo: inputValue(els.selCobo),
    asad: inputValue(els.selAsad),
    aget: inputValue(els.selAget),
    channel: inputValue(els.selChannel),
  };
}

function writeSelection(selection) {
  els.selCobo.value = selection.cobo ?? "";
  els.selAsad.value = selection.asad ?? "";
  els.selAget.value = selection.aget ?? "";
  els.selChannel.value = selection.channel ?? "";
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

async function openPaths(paths) {
  if (!paths.length) return;
  setBusy(true);
  try {
    setStatus(paths.length > 1 ? `Opening ${paths.length} files...` : "Opening file...");
    const payload = await postJson("/api/open", { paths, selection: readSelection() });
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

async function browseDirectory(path) {
  els.browserRows.textContent = "Loading...";
  browserSelectedPaths = [];
  refreshBrowserSelection();
  try {
    const payload = await requestJson(`/api/files?path=${encodeURIComponent(path || ".")}`);
    browserParent = payload.parent;
    els.browserPath.value = payload.path;
    els.browserUp.disabled = !browserParent;
    els.browserRows.replaceChildren();
    payload.entries.forEach((entry) => {
      const row = document.createElement("button");
      row.type = "button";
      row.className = "browser-entry";
      if (!entry.isDirectory) row.dataset.path = entry.path;

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
    if (!payload.entries.length) els.browserRows.textContent = "This directory is empty.";
    els.browserSelectAll.disabled = !payload.entries.some((entry) => !entry.isDirectory);
  } catch (error) {
    els.browserRows.textContent = error.message;
  }
}

function closeFileBrowser() {
  els.fileBrowser.hidden = true;
}

async function navigate(action) {
  setBusy(true);
  try {
    const payload = await postJson("/api/navigate", { action, selection: readSelection() });
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
  els.autoNextButton.textContent = "Auto Next";
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
  autoNextTimer = window.setTimeout(runAutoNext, delay);
  setStatus(`Auto-next every ${delay / 1000} seconds`, "good");
}

async function scanSignal() {
  setBusy(true);
  try {
    setStatus("Scanning...");
    const selection = readSelection();
    const scope = els.scanScope.value;
    scanSelectionSnapshot = { selection: { ...selection }, scope };
    els.restoreScanSelection.disabled = false;
    const payload = await postJson("/api/scan", {
      selection,
      scope,
      threshold: Number.parseFloat(els.thresholdInput.value || "0"),
      includeFpn: els.includeFpn.checked,
      maxEvents: Number.parseInt(els.scanLimit.value || "10000", 10),
    });
    if (payload.found) {
      renderPayload(payload.event);
      const m = payload.match;
      setStatus(
        `Found event ${payload.event.event.index}: C${m.cobo} A${m.asad} G${m.aget} Ch${m.channel}, amp ${m.amplitude}`,
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
  const sourceSuffix = status.sourceCount > 1 ? ` (+${status.sourceCount - 1} sources)` : "";
  els.fileLabel.textContent = `${compactPath(status.path)}${sourceSuffix}`;
  els.fileLabel.title = (status.paths || [status.path]).filter(Boolean).join("\n");
  els.pathInput.value = status.path || els.pathInput.value;
  els.eventIndex.textContent = event.index;
  els.jumpInput.value = event.index;
  els.eventMeta.textContent = `GET ${event.eventIdx} | ${event.channelCount} ch`;
  els.plotTitle.textContent = `Event ${event.index}`;
  els.plotSubtitle.textContent = `${event.selectedCount}/${event.channelCount} channels`;
  renderTree(event.groups);
  renderTable(event.channels);
  drawWaveforms(event.channels);
}

function makeRow(level, values, name, count, amp, className = "") {
  const row = document.createElement("div");
  row.className = `tree-row tree-${level} ${className}`.trim();
  if (isSelected(values, level)) row.classList.add("selected");
  row.innerHTML = `
    <span class="name"></span>
    <span class="count"></span>
    <span class="amp"></span>
  `;
  row.querySelector(".name").textContent = name;
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
      makeRow("cobo", { cobo: cobo.cobo, asad: null, aget: null, channel: null }, `Cobo ${cobo.cobo}`, cobo.count, cobo.maxAmplitude),
    );
    cobo.asads.forEach((asad) => {
      els.groupTree.appendChild(
        makeRow(
          "asad",
          { cobo: cobo.cobo, asad: asad.asad, aget: null, channel: null },
          `AsAd ${asad.asad}`,
          asad.count,
          asad.maxAmplitude,
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
  if (currentPayload) drawWaveforms(currentPayload.event.channels);
}

function renderTable(channels) {
  els.channelRows.replaceChildren();
  channels.forEach((channel) => {
    const tr = document.createElement("tr");
    if (isSelected(channel, "channel")) tr.classList.add("selected");
    tr.innerHTML = `
      <td></td>
      <td></td>
      <td></td>
      <td></td>
    `;
    tr.children[0].textContent = `${channel.cobo}/${channel.asad}/${channel.aget}`;
    tr.children[1].textContent = channel.isFpn ? `${channel.channel} FPN` : channel.channel;
    tr.children[2].textContent = channel.amplitude.toFixed(0);
    tr.children[3].textContent = channel.peakTb;
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
  const tbMax = plotView ? plotView.tbMax : lastTb;
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
    ctx.globalAlpha = hoveredChannelKey ? Math.min(baseAlpha, 0.18) : baseAlpha;
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
  browseDirectory(els.pathInput.value.trim() || ".");
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
els.browserOpen.addEventListener("click", () => {
  if (!browserSelectedPaths.length) return;
  const paths = browserSelectedPaths.slice();
  closeFileBrowser();
  openPaths(paths);
});
els.browserSelectAll.addEventListener("click", () => {
  browserSelectedPaths = [];
  els.browserRows.querySelectorAll(".browser-entry[data-path]").forEach((row) => {
    browserSelectedPaths.push(row.dataset.path);
    row.classList.add("selected");
  });
  refreshBrowserSelection();
});
document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && !els.fileBrowser.hidden) closeFileBrowser();
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
els.autoNextButton.addEventListener("click", () => {
  if (autoNextTimer === null) startAutoNext();
  else stopAutoNext("Auto-next stopped");
});

els.applySelection.addEventListener("click", () => navigate("current"));
els.clearSelection.addEventListener("click", () => {
  writeSelection({ cobo: null, asad: null, aget: null, channel: null });
  navigate("current");
});
els.restoreScanSelection.addEventListener("click", async () => {
  if (!scanSelectionSnapshot) return;
  writeSelection(scanSelectionSnapshot.selection);
  els.scanScope.value = scanSelectionSnapshot.scope;
  if (await navigate("current")) setStatus("Scan selection restored", "good");
});
els.showAll.addEventListener("click", () => {
  writeSelection({ cobo: null, asad: null, aget: null, channel: null });
  navigate("current");
});

// ---- plot zoom and pan -------------------------------------------------
// The wheel zooms about the cursor and a drag pans. Over the tick strips only
// that axis responds, so a run can be stretched in time without touching the
// amplitude scale.
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
  return plotView || { tbMin: 0, tbMax: g.lastTb, yMin: g.autoMin, yMax: g.autoMax };
}

function applyView(view) {
  const g = plotGeometry;
  const minSpanTb = 4;
  const minSpanY = 4;
  let { tbMin, tbMax, yMin, yMax } = view;
  if (tbMax - tbMin < minSpanTb) {
    const middle = (tbMin + tbMax) / 2;
    tbMin = middle - minSpanTb / 2;
    tbMax = middle + minSpanTb / 2;
  }
  if (yMax - yMin < minSpanY) {
    const middle = (yMin + yMax) / 2;
    yMin = middle - minSpanY / 2;
    yMax = middle + minSpanY / 2;
  }
  tbMin = Math.max(0, tbMin);
  tbMax = Math.min(g.lastTb, tbMax);
  if (tbMax - tbMin < minSpanTb) tbMax = Math.min(g.lastTb, tbMin + minSpanTb);
  plotView = { tbMin, tbMax, yMin, yMax };
  if (currentPayload) drawWaveforms(currentPayload.event.channels);
}

function resetView() {
  plotView = null;
  if (currentPayload) drawWaveforms(currentPayload.event.channels);
}

els.waveCanvas.addEventListener("wheel", (event) => {
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

let panState = null;

els.waveCanvas.addEventListener("pointerdown", (event) => {
  const point = canvasPoint(event);
  const region = plotRegion(point);
  if (!region) return;
  event.preventDefault();
  els.waveCanvas.setPointerCapture(event.pointerId);
  panState = { region, point, view: currentView() };
  els.waveCanvas.classList.add("panning");
});

els.waveCanvas.addEventListener("pointermove", (event) => {
  if (!panState || !plotGeometry) return;
  const g = plotGeometry;
  const point = canvasPoint(event);
  const view = panState.view;
  const next = { ...view };
  if (panState.region === "plot" || panState.region === "x") {
    const shift = ((point.x - panState.point.x) / g.plotW) * (view.tbMax - view.tbMin);
    next.tbMin = view.tbMin - shift;
    next.tbMax = view.tbMax - shift;
  }
  if (panState.region === "plot" || panState.region === "y") {
    const shift = ((point.y - panState.point.y) / g.plotH) * (view.yMax - view.yMin);
    next.yMin = view.yMin + shift;
    next.yMax = view.yMax + shift;
  }
  applyView(next);
});

function endPan(event) {
  if (!panState) return;
  panState = null;
  els.waveCanvas.classList.remove("panning");
  if (els.waveCanvas.hasPointerCapture(event.pointerId)) {
    els.waveCanvas.releasePointerCapture(event.pointerId);
  }
}

els.waveCanvas.addEventListener("pointerup", endPan);
els.waveCanvas.addEventListener("pointercancel", endPan);
els.waveCanvas.addEventListener("dblclick", (event) => {
  event.preventDefault();
  resetView();
});

els.autoScale.addEventListener("click", resetView);
els.scanButton.addEventListener("click", scanSignal);

window.addEventListener("resize", () => {
  if (currentPayload) drawWaveforms(currentPayload.event.channels);
});

requestJson("/api/status")
  .then((payload) => {
    if (payload.status.path) {
      return navigate("current");
    }
    return null;
  })
  .catch((error) => setStatus(error.message, "error"));
"""


STATIC_ASSETS: Dict[str, Tuple[str, str]] = {
    "index.html": ("text/html; charset=utf-8", INDEX_HTML),
    "static/styles.css": ("text/css; charset=utf-8", STYLES_CSS),
    "static/app.js": ("application/javascript; charset=utf-8", APP_JS),
}


def main() -> None:
    parser = argparse.ArgumentParser(description="Web-based GET raw frame viewer")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--file", nargs="+", help="open and merge these raw source files on startup")
    parser.add_argument("--no-browser", action="store_true", help="do not open a web browser")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

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
