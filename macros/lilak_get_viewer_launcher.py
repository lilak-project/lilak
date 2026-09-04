#!/usr/bin/env python3

"""Launch and manage the LK GET web viewer."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import time
from typing import Dict, List, Optional, Tuple


DEFAULT_ADDRESS = "0.0.0.0:8765"


def existing_file(value: str) -> str:
    path = Path(value).expanduser().resolve()
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"file not found: {value}")
    return str(path)


def server_address(value: str) -> str:
    match = re.fullmatch(r"(?:(?P<host>[A-Za-z0-9_.-]+):)?(?P<port>[0-9]+)", value)
    if match is None:
        raise argparse.ArgumentTypeError("address must be HOST:PORT or PORT")
    port = int(match.group("port"))
    if not 1 <= port <= 65535:
        raise argparse.ArgumentTypeError("port must be between 1 and 65535")
    return f"{match.group('host') or '0.0.0.0'}:{port}"


def split_address(address: str) -> Tuple[str, int]:
    host, port = address.rsplit(":", 1)
    return host, int(port)


def safe_address(address: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]", "_", address)


def state_path(address: str) -> Path:
    return Path(f"/tmp/lilak_get_viewer_{safe_address(address)}.json")


def log_path(address: str) -> Path:
    return Path(f"/tmp/lilak_get_viewer_{safe_address(address)}.log")


def process_exists(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def is_viewer_process(pid: int, server_script: Path) -> bool:
    try:
        arguments = Path(f"/proc/{pid}/cmdline").read_bytes().split(b"\0")
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        return False
    decoded = [item.decode(errors="replace") for item in arguments if item]
    return str(server_script) in decoded


def discover_servers(server_script: Path) -> List[Dict]:
    servers = []
    for process_dir in Path("/proc").glob("[0-9]*"):
        pid = int(process_dir.name)
        try:
            arguments = (process_dir / "cmdline").read_bytes().split(b"\0")
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            continue
        decoded = [item.decode(errors="replace") for item in arguments if item]
        if str(server_script) not in decoded:
            continue
        host = "127.0.0.1"
        port = 8765
        files = []
        try:
            if "--host" in decoded:
                host = decoded[decoded.index("--host") + 1]
            if "--port" in decoded:
                port = int(decoded[decoded.index("--port") + 1])
            if "--file" in decoded:
                start = decoded.index("--file") + 1
                files = [value for value in decoded[start:] if not value.startswith("--")]
        except (IndexError, ValueError):
            continue
        address = f"{host}:{port}"
        servers.append(
            {
                "pid": pid,
                "address": address,
                "files": files,
                "log": str(log_path(address)) if log_path(address).exists() else "-",
            }
        )
    return servers


def read_state(path: Path) -> Optional[Dict]:
    try:
        payload = json.loads(path.read_text())
        payload["pid"] = int(payload["pid"])
        return payload
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError):
        return None


def interface_addresses() -> List[str]:
    addresses = set()
    try:
        result = subprocess.run(
            ["hostname", "-I"], check=False, capture_output=True, text=True
        )
        if result.returncode == 0:
            for value in result.stdout.split():
                try:
                    parsed = socket.inet_aton(value)
                except OSError:
                    continue
                if parsed and not value.startswith("127."):
                    addresses.add(value)
    except OSError:
        pass
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            value = info[4][0]
            if not value.startswith("127."):
                addresses.add(value)
    except OSError:
        pass
    return sorted(addresses)


def access_urls(address: str) -> List[str]:
    host, port = split_address(address)
    if host == "0.0.0.0":
        hosts = interface_addresses()
        return [f"http://{value}:{port}/" for value in hosts] or [f"http://127.0.0.1:{port}/"]
    return [f"http://{host}:{port}/"]


def print_server(payload: dict, server_script: Path) -> bool:
    pid = int(payload["pid"])
    if not is_viewer_process(pid, server_script):
        return False
    print(f"PID     : {pid}")
    print(f"Address : {payload['address']}")
    print(f"Files   : {len(payload.get('files', []))}")
    for url in access_urls(payload["address"]):
        print(f"URL     : {url}")
    print(f"Log     : {payload.get('log', '-')}")
    print(f"Stop    : lilak get_viewer -K -I {payload['address']}")
    return True


def list_servers(server_script: Path) -> None:
    found = False
    listed_pids = set()
    for path in sorted(Path("/tmp").glob("lilak_get_viewer_*.json")):
        payload = read_state(path)
        if payload is not None and print_server(payload, server_script):
            found = True
            listed_pids.add(payload["pid"])
            print()
        elif payload is None or not process_exists(int(payload.get("pid", -1))):
            path.unlink(missing_ok=True)
    for payload in discover_servers(server_script):
        if payload["pid"] in listed_pids:
            continue
        if print_server(payload, server_script):
            found = True
            print()
    if not found:
        print("No lilak get_viewer servers are running.")


def stop_server(address: str, server_script: Path) -> None:
    path = state_path(address)
    payload = read_state(path)
    if payload is None:
        payload = next(
            (item for item in discover_servers(server_script) if item["address"] == address),
            None,
        )
        if payload is None:
            print(f"No lilak get_viewer server is running at {address}.")
            return
    pid = payload["pid"]
    if not is_viewer_process(pid, server_script):
        path.unlink(missing_ok=True)
        print(f"No lilak get_viewer server is running at {address}.")
        return

    print(f"Stopping lilak get_viewer PID {pid} at {address}...")
    os.kill(pid, signal.SIGTERM)
    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline and process_exists(pid):
        time.sleep(0.1)
    if process_exists(pid):
        print(f"PID {pid} did not stop; sending SIGKILL.")
        os.kill(pid, signal.SIGKILL)
    path.unlink(missing_ok=True)
    print("lilak get_viewer stopped.")


def port_is_available(host: str, port: int) -> bool:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as check_socket:
            check_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            check_socket.bind((host, port))
        return True
    except OSError:
        return False


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="lilak get_viewer",
        description="Run the LK GET web viewer in the background.",
        epilog="""
Examples:
  lilak get_viewer run_0084.dat
  lilak get_viewer run_0084.dat.*
  lilak get_viewer -I 0.0.0.0:8877 run_0084.dat
  lilak get_viewer -L
  lilak get_viewer -K
""",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-I", metavar="HOST:PORT", type=server_address, default=DEFAULT_ADDRESS,
        help=f"listen address (default: {DEFAULT_ADDRESS})",
    )
    parser.add_argument("-K", action="store_true", help="stop the server at the selected address")
    parser.add_argument("-L", action="store_true", help="list running viewer servers")
    parser.add_argument("-F", action="store_true", help="run in the foreground")
    parser.add_argument(
        "--no-browser", action="store_true",
        help="do not open a browser in foreground mode (background mode never opens one)",
    )
    parser.add_argument("files", nargs="*", type=existing_file, help="GET raw source file(s)")
    return parser.parse_args()


def main() -> None:
    arguments = parse_arguments()
    server_script = Path(__file__).resolve().parent / "lilak_get_viewer_server.py"
    if not server_script.is_file():
        raise SystemExit(f"GET viewer server not found: {server_script}")

    if arguments.K:
        stop_server(arguments.I, server_script)
        return
    if arguments.L:
        list_servers(server_script)
        return

    host, port = split_address(arguments.I)
    current = read_state(state_path(arguments.I))
    if current is not None and is_viewer_process(current["pid"], server_script):
        raise SystemExit(
            f"A lilak get_viewer server is already running at {arguments.I} "
            f"(PID {current['pid']}). Use lilak get_viewer -K first."
        )
    if not port_is_available(host, port):
        raise SystemExit(f"Cannot listen at {arguments.I}: address or port is unavailable.")

    command = [sys.executable, str(server_script), "--host", host, "--port", str(port)]
    if arguments.files:
        command.extend(["--file", *arguments.files])
    # A browser launched by a detached process can inherit a stale SSH/X11
    # channel and take the viewer down with it. Background mode only serves the
    # printed URLs; opening a browser is reserved for explicit foreground use.
    if arguments.no_browser or not arguments.F:
        command.append("--no-browser")

    output_log = log_path(arguments.I)
    if arguments.F:
        os.execv(sys.executable, command)

    with output_log.open("wb") as log_file:
        process = subprocess.Popen(
            command,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
    time.sleep(0.25)
    if process.poll() is not None:
        try:
            details = output_log.read_text(errors="replace").strip()
        except OSError:
            details = ""
        message = f"lilak get_viewer failed to start (exit {process.returncode})."
        if details:
            message += f"\nLog: {output_log}\n{details[-2000:]}"
        raise SystemExit(message)
    payload = {
        "pid": process.pid,
        "address": arguments.I,
        "files": arguments.files,
        "log": str(output_log),
    }
    state_path(arguments.I).write_text(json.dumps(payload, indent=2) + "\n")

    print(f"Started lilak get_viewer PID {process.pid} in the background.")
    print(f"Listening on all network interfaces at port {port}." if host == "0.0.0.0" else f"Listening at {arguments.I}.")
    for url in access_urls(arguments.I):
        print(f"URL: {url}")
    print(f"Log: {output_log}")
    print("List: lilak get_viewer -L")
    print(f"Stop: lilak get_viewer -K -I {arguments.I}")
    if host not in ("127.0.0.1", "localhost"):
        print("WARNING: anyone who can reach this port can use the viewer and open server-side files.")


if __name__ == "__main__":
    main()
