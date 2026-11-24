"""Checks lilak_collect_par.merge() without ROOT. Run: python3 macros/test_lilak_collect_par.py"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lilak_collect_par import KEPT_MARK, merge  # noqa: E402

original = """# my analysis

<inc   inc.mac
lilak/add      LKPulseExtractionTask
LKRun/Name     demo
grp/
\tread     1
    unread   2   # note
*debug/x 1
?!missing/opt/z 9
""".splitlines()

# What LKRun writes: only parameters that were read, lilak/ lines in the default block.
collected = """
LKRun/Name                    demo  # name of the run
grp/read        1
#lilak/add                     LKTask  # add task or detector class
""".splitlines()

result = merge(original, collected)
assert result[0] == "# my analysis", result
assert result[1] == "<inc   inc.mac", result
assert "lilak/add      LKPulseExtractionTask" in result
assert "#lilak/add                     LKTask  # add task or detector class" not in result
assert result.count(KEPT_MARK) == 1
kept = result[result.index(KEPT_MARK) + 1:]
assert kept == ["grp/unread  2   # note", "*debug/x  1", "?!missing/opt/z  9"], kept
assert merge(result, collected) == result, "a second pass must not change the file"
print("test_lilak_collect_par: OK")
