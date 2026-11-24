"""Checks the parameter editor's text <-> rows round trip without a browser.
Run: python3 macros/test_lilak_parameter_editor.py"""
import copy
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lilak_parameter_editor as E  # noqa: E402

TEXT = """# header
lilak/add  LKPulseExtractionTask

grp/
\tread     1
# note between
    other  2   # c

?run>100/cut 3
"""


def names(text):
    return [(r["kind"], r["group"], r["name"], r["value"]) for r in E.parse_parameter_text(text)]


rows = E.parse_parameter_text(TEXT)
assert E.serialize_rows(rows) == TEXT, "an untouched file must be written back byte for byte"
assert ("parameter", "grp", "other", "2") in names(TEXT), "a column-0 comment must not end the group"
assert ("parameter", "?run>100", "cut", "3") in names(TEXT)

edited = copy.deepcopy(rows)
next(r for r in edited if r["name"] == "other")["value"] = "5"
out = E.serialize_rows(edited)
assert "    other  5  # c" in out and "grp/\n" in out, out  # edited in place, still indented

added = copy.deepcopy(rows)
index = next(i for i, r in enumerate(added) if r["name"] == "read")
added.insert(index + 1, {"kind": "parameter", "enabled": True, "group": "grp", "name": "new", "value": "7", "unit": "", "comment": ""})
out = E.serialize_rows(added)
assert "\tnew  7" in out and ("parameter", "grp", "new", "7") in names(out), out

# Removing the first row of a group: the browser hands its "pre" (blank line + header) to the next row.
carried = copy.deepcopy(rows)
first = next(i for i, r in enumerate(carried) if r["name"] == "read")
carried[first + 1]["pre"] = carried[first]["pre"] + carried[first + 1]["pre"]
del carried[first]
out = E.serialize_rows(carried)
assert "grp/\n" in out and ("parameter", "grp", "other", "2") in names(out), out

# Without that hand-over the shape cannot be kept: fall back to flat lines with the same meaning.
lost = copy.deepcopy(rows)
del lost[next(i for i, r in enumerate(lost) if r["name"] == "read")]
out = E.serialize_rows(lost)
assert ("parameter", "grp", "other", "2") in names(out) and "grp/other" in out, out

moved = copy.deepcopy(rows)
moved.reverse()
out = E.serialize_rows(moved)
expected = [(r["kind"], r["group"], r["name"], r["value"]) for r in moved]
assert names(out) == expected, out  # any order is written with the meaning of that order
print("test_lilak_parameter_editor: OK")
