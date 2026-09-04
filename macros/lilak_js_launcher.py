#!/usr/bin/env python3

"""Launch the generic LILAK JSROOT server from the ``lilak js`` command."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time


def existing_file(value: str) -> str:
    path = Path(value).expanduser().resolve()
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"file not found: {value}")
    return str(path)


def existing_directory(value: str) -> str:
    path = Path(value).expanduser().resolve()
    if not path.is_dir():
        raise argparse.ArgumentTypeError(f"directory not found: {value}")
    return str(path)


def server_address(value: str) -> str:
    match = re.fullmatch(r"(?:(?P<host>[A-Za-z0-9.-]+):)?(?P<port>[0-9]+)", value)
    if match is None:
        raise argparse.ArgumentTypeError("address must be HOST:PORT or PORT")
    port = int(match.group("port"))
    if not 1 <= port <= 65535:
        raise argparse.ArgumentTypeError("port must be between 1 and 65535")
    host = match.group("host") or "127.0.0.1"
    return f"{host}:{port}"


def process_command(pid: int) -> str:
    result = subprocess.run(
        ["ps", "-p", str(pid), "-o", "command="],
        check=False,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def is_server_process(pid: int, server_macro: Path) -> bool:
    try:
        arguments = Path(f"/proc/{pid}/cmdline").read_bytes().split(b"\0")
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        return False
    decoded = [value.decode(errors="replace") for value in arguments if value]
    return (
        bool(decoded)
        and Path(decoded[0]).name in ("root", "root.exe")
        and str(server_macro) in decoded
    )


def process_exists(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def listener_pids(port: int) -> list[int]:
    """Find listener PIDs without requiring lsof."""
    socket_inodes = set()
    for table in (Path("/proc/net/tcp"), Path("/proc/net/tcp6")):
        try:
            lines = table.read_text().splitlines()[1:]
        except OSError:
            continue
        for line in lines:
            fields = line.split()
            if fields[3] == "0A" and int(fields[1].rsplit(":", 1)[1], 16) == port:
                socket_inodes.add(fields[9])

    pids = set()
    for process_dir in Path("/proc").glob("[0-9]*"):
        try:
            for fd in (process_dir / "fd").iterdir():
                target = os.readlink(fd)
                if target.startswith("socket:[") and target[8:-1] in socket_inodes:
                    pids.add(int(process_dir.name))
                    break
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            continue
    return sorted(pids)


def process_environment(pid: int) -> dict[str, str]:
    try:
        entries = Path(f"/proc/{pid}/environ").read_bytes().split(b"\0")
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        return {}
    environment = {}
    for entry in entries:
        if b"=" in entry:
            name, value = entry.split(b"=", 1)
            environment[name.decode(errors="replace")] = value.decode(errors="replace")
    return environment


def list_servers(server_macro: Path) -> None:
    servers = []
    for process_dir in Path("/proc").glob("[0-9]*"):
        pid = int(process_dir.name)
        if not is_server_process(pid, server_macro):
            continue
        environment = process_environment(pid)
        address = environment.get("LILAK_JS_ADDRESS", "<unknown>")
        try:
            port = int(address.rsplit(":", 1)[1])
            state = "LISTEN" if pid in listener_pids(port) else "STARTING"
        except (IndexError, ValueError):
            state = "UNKNOWN"
        content = environment.get("LILAK_JS_DIRECTORY", "-")
        runners = []
        for name, label in (
            ("LILAK_JS_SHELL", "sh"),
            ("LILAK_JS_PYTHON", "py"),
            ("LILAK_JS_ROOT", "ROOT"),
        ):
            if environment.get(name):
                runners.append(f"{label}:{environment[name].replace(chr(10), ',')}")
        servers.append((pid, state, address, content, ", ".join(runners) or "-",
                        environment.get("LILAK_JS_LOG", "-")))

    if not servers:
        print("No lilak js servers are running.")
        return

    listening_addresses = {
        address for _, state, address, _, _, _ in servers if state == "LISTEN"
    }
    servers = [
        server for server in servers
        if server[1] == "LISTEN" or server[2] not in listening_addresses
    ]

    print("PID     STATE     ADDRESS              WATCHED DIRECTORY")
    for pid, state, address, content, runners, log_path in sorted(servers):
        print(f"{pid:<7} {state:<9} {address:<20} {content}")
        print(f"        runners: {runners}")
        print(f"        log:     {log_path}")
        print(f"        stop:    lilak js -K -I {address}  (or: kill {pid})")


def kill_server(address: str, server_macro: Path) -> None:
    port = int(address.rsplit(":", 1)[1])
    lsof_executable = shutil.which("lsof")
    if lsof_executable is None and Path("/usr/sbin/lsof").is_file():
        lsof_executable = "/usr/sbin/lsof"
    if lsof_executable is None:
        pids = listener_pids(port)
    else:
        result = subprocess.run(
            [lsof_executable, "-nP", f"-iTCP:{port}", "-sTCP:LISTEN", "-t"],
            check=False,
            capture_output=True,
            text=True,
        )
        pids = sorted({
            int(line) for line in result.stdout.splitlines() if line.strip().isdigit()
        })
    if not pids:
        print(f"No process is listening at {address}.")
        return

    server_pids = []
    unrelated = []
    for pid in pids:
        command = process_command(pid)
        if is_server_process(pid, server_macro):
            server_pids.append(pid)
        else:
            unrelated.append((pid, command or "<unknown command>"))

    if not server_pids:
        details = "\n".join(
            f"  PID {pid}: {command}" for pid, command in unrelated
        )
        raise SystemExit(
            f"Refusing to kill a non-LILAK process listening at {address}:\n"
            f"{details}"
        )

    for pid in server_pids:
        print(f"Stopping lilak js server PID {pid} at {address}...")
        os.kill(pid, signal.SIGTERM)

    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline:
        if not any(process_exists(pid) for pid in server_pids):
            break
        time.sleep(0.1)

    for pid in server_pids:
        if process_exists(pid):
            print(f"PID {pid} did not stop; sending SIGKILL.")
            os.kill(pid, signal.SIGKILL)

    print("lilak js server stopped.")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="lilak js",
        description="Run the generic LILAK JSROOT server.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Each configured runner receives one run-number argument.

Optional stdout protocol for web monitoring:
  LILAK_JS_STATUS message
  LILAK_JS_PROGRESS 42.5 message

Examples:
  lilak js
  lilak js -L
  lilak js -F
  lilak js -K
  lilak js -K -I 192.168.1.35:9091
  lilak js -D ./results
  lilak js -R run_reco.C -P run_reco.py -S run_reco.sh -D ./results
  lilak js -I 0.0.0.0:8080 -D ./results
""",
    )
    parser.add_argument(
        "-I",
        metavar="HOST:PORT",
        type=server_address,
        default="127.0.0.1:9091",
        help="listen address (default: 127.0.0.1:9091)",
    )
    parser.add_argument(
        "-K",
        action="store_true",
        help="stop the lilak js server listening at the selected address",
    )
    parser.add_argument(
        "-L",
        action="store_true",
        help="list running lilak js servers and the content they serve",
    )
    parser.add_argument(
        "-F",
        action="store_true",
        help="run in the foreground instead of the default background mode",
    )
    parser.add_argument(
        "-S",
        metavar="SCRIPT",
        type=existing_file,
        action="append",
        help="shell script receiving one run-number argument (repeatable)",
    )
    parser.add_argument(
        "-P",
        metavar="SCRIPT",
        type=existing_file,
        help="Python script receiving one run-number argument",
    )
    parser.add_argument(
        "-R",
        metavar="MACRO",
        type=existing_file,
        help="ROOT macro whose file-name function receives one run number",
    )
    parser.add_argument(
        "-D",
        metavar="DIRECTORY",
        type=existing_directory,
        help="directory containing ROOT files to load and watch",
    )
    return parser.parse_args()


def main() -> None:
    arguments = parse_arguments()

    lilak_path = Path(
        os.environ.get("LILAK_PATH", Path(__file__).resolve().parents[1])
    ).expanduser().resolve()
    server_macro = lilak_path / "macros" / "lilak_js.C"
    if not server_macro.is_file():
        raise SystemExit(f"JSROOT server macro not found: {server_macro}")

    if arguments.K:
        kill_server(arguments.I, server_macro)
        return
    if arguments.L:
        list_servers(server_macro)
        return

    if not any((arguments.S, arguments.P, arguments.R, arguments.D)):
        arguments.D = str(Path.cwd().resolve())

    root_executable = shutil.which("root")
    if root_executable is None:
        raise SystemExit("ROOT executable not found in PATH")

    environment = os.environ.copy()
    option_environment = {
        "LILAK_JS_ADDRESS": arguments.I,
        "LILAK_JS_SHELL": (
            "\n".join(arguments.S) if arguments.S else None
        ),
        "LILAK_JS_PYTHON": arguments.P,
        "LILAK_JS_ROOT": arguments.R,
        "LILAK_JS_DIRECTORY": arguments.D,
    }
    for name, value in option_environment.items():
        if value is None:
            environment.pop(name, None)
        else:
            environment[name] = value

    safe_address = re.sub(r"[^A-Za-z0-9_.-]", "_", arguments.I)
    log_path = Path(f"/tmp/lilak_js_{safe_address}.log")
    environment["LILAK_JS_LOG"] = str(log_path)

    print(f"JSROOT address   : {arguments.I}")
    if arguments.S:
        for script in arguments.S:
            print(f"Shell runner     : {script}")
    if arguments.P:
        print(f"Python runner    : {arguments.P}")
    if arguments.R:
        print(f"ROOT runner      : {arguments.R}")
    if arguments.D:
        print(f"Watched directory: {arguments.D}")
    if not arguments.I.startswith(("127.0.0.1:", "localhost:")):
        print("WARNING: this server is reachable beyond the loopback interface.")

    sys.stdout.flush()
    root_command = [root_executable, "-l", str(server_macro)]
    if arguments.F:
        os.execvpe(root_executable, root_command, environment)

    environment["LILAK_JS_DAEMON"] = "1"
    with log_path.open("ab") as log_file:
        process = subprocess.Popen(
            root_command,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            start_new_session=True,
            env=environment,
        )
    print(f"Started lilak js server PID {process.pid} in the background.")
    print(f"URL: http://{arguments.I}")
    print(f"Log: {log_path}")
    print("List: lilak js -L")
    print(f"Stop: lilak js -K -I {arguments.I}")


if __name__ == "__main__":
    main()
