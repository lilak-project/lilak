"""lilak collect_par <file>: rewrite a parameter file with the parameters LILAK reads from it.

LKRun is initialized in dry mode (run_lilak.C(..., 0, 0, 1)) and writes the collected
parameters over <file>. This script then puts back what the collection cannot know about:
the user's lilak/ lines, the comment block at the top, include lines, and lines no task read.
The previous file is kept as <file>.old only when the file actually changed.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

KEPT_MARK = "# kept by lilak collect_par: not read during collection"


def parse(lines):
    """Yield (kind, name, line, flat) the way LKParameterContainer::AddLine groups lines.

    kind is "blank", "comment", "group", "include" or "par". For "par", name has group
    prefixes applied, leading header marks (* & ! @ -) removed and ?condition segments
    dropped, and flat is the line rewritten without indentation as LILAK reads it.
    """
    groups = []  # (indent, prefix)
    for line in lines:
        expanded = re.sub(r"^[ \t]+", lambda m: m.group(0).replace("\t", "    "), line.rstrip())
        stripped = expanded.lstrip(" ")
        indent = len(expanded) - len(stripped)
        if not stripped:
            yield "blank", None, line, None
            continue
        if stripped.startswith("#"):
            yield "comment", None, line, None
            continue
        while groups and (indent == 0 or groups[-1][0] >= indent):
            groups.pop()
        prefix = groups[-1][1] if groups else ""
        token, _, rest = re.sub(r"[ \t]+", " ", stripped, count=1).partition(" ")
        rest = rest.strip()
        if token.startswith("<"):
            yield "include", None, line, None
            continue
        if token.endswith("/") and (not rest or rest.startswith("#")):
            groups.append((indent, prefix + token))
            yield "group", None, line, None
            continue
        name = (prefix + token).lstrip("*&!@-")
        name = "/".join(part for part in name.split("/") if part and not part.startswith("?"))
        yield "par", name, line, (prefix + token + "  " + rest).rstrip()


def merge(original_lines, output_lines):
    """Return the collected file with the user's own lines put back."""
    original = list(parse(original_lines))
    active_lilak = [line.strip() for kind, name, line, _ in original
                    if kind == "par" and name.startswith("lilak/") and not name.startswith("lilak/collect_par")]

    # Replace the collected lilak/ block with the user's active lilak/ lines (as before).
    rewritten, inside_lilak, inserted_lilak = [], False, False
    for line in output_lines:
        stripped = line.lstrip()
        if re.match(r"#?lilak/", stripped):
            if not inserted_lilak:
                rewritten.extend(active_lilak)
                inserted_lilak = True
            inside_lilak = True
            continue
        if inside_lilak and stripped == "":
            inside_lilak = False
        rewritten.append(line)
    if not inserted_lilak and active_lilak:
        rewritten += [""] + active_lilak

    collected = {name for kind, name, _, _ in parse(rewritten) if kind == "par"}
    header = []
    for kind, _, line, _ in original:
        if kind not in ("blank", "comment"):
            break
        header.append(line)
    while header and not header[-1].strip():
        header.pop()
    includes =[line.strip() for kind, _, line, _ in original if kind == "include"]
    kept = [flat for kind, name, _, flat in original
            if kind == "par" and not name.startswith("lilak/") and name not in collected]

    result = header + includes
    if result and rewritten and rewritten[0].strip():
        result.append("")
    result += rewritten
    if kept:
        result += ["", KEPT_MARK] + kept
    return result


def main():
    if len(sys.argv) != 2:
        print("Usage: lilak collect_par [parameter file]")
        return 1
    source = Path(sys.argv[1])
    if not source.is_file():
        print("Error: Parameter file not found: {}".format(source))
        return 1
    lilak_path = os.environ.get("LILAK_PATH", str(Path(__file__).resolve().parent.parent))

    original_text = source.read_text()
    with tempfile.TemporaryDirectory(prefix="lilak_collect_par.") as tmp:
        backup = Path(tmp) / "backup.mac"
        shutil.copy2(str(source), str(backup))
        temp_par = Path(tmp) / "collect.mac"
        temp_par.write_text(original_text + "\nlilak/collect_par {}\n".format(source))
        # dry init (LKRun::SetDryInit): no run, no real output file, no logs under data/, no elog.
        # Its temporary output and init log go to TMPDIR, i.e. into this directory.
        macro = '{}/macros/run_lilak.C("{}",0,0,1)'.format(lilak_path, temp_par)
        env = dict(os.environ, TMPDIR=tmp)
        run_status = subprocess.call(["root", "-l", "-b", "-q", macro], env=env)

        if not source.is_file() or source.read_text() == original_text:
            print("Parameter file was not changed: {}".format(source))
            return run_status
        merged_text = "\n".join(merge(original_text.splitlines(), source.read_text().splitlines())) + "\n"
        source.write_text(merged_text)
        if merged_text == original_text:
            print("Parameter file was not changed: {}".format(source))
            return run_status
        backup_par = Path(str(source) + ".old")
        shutil.copy2(str(backup), str(backup_par))
        print("Backup: {}".format(backup_par))
    return run_status


if __name__ == "__main__":
    sys.exit(main())
