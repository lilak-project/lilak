#!/usr/bin/env python3

import re
import subprocess
import sys
import tempfile
from pathlib import Path


def safe_name(value: str, fallback: str) -> str:
    name = re.sub(r"[^0-9A-Za-z_]", "_", value)
    name = re.sub(r"_+", "_", name).strip("_")
    if not name:
        name = fallback
    if re.match(r"^[0-9]", name):
        name = f"{fallback}_{name}"
    return name


def cpp_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def split_conf_entries(text: str) -> list[str]:
    if "," not in text:
        return [item for item in text.split() if item]
    items: list[str] = []
    current: list[str] = []
    depth = 0
    for char in text:
        if char == "," and depth == 0:
            entry = "".join(current).strip()
            if entry:
                items.append(entry)
            current = []
            continue
        if char == "(":
            depth += 1
        elif char == ")" and depth > 0:
            depth -= 1
        current.append(char)
    entry = "".join(current).strip()
    if entry:
        items.append(entry)
    return items


def load_recommendation_conf(path: Path) -> dict[str, list[str]]:
    values: dict[str, list[str]] = {}
    if not path.is_file():
        return values
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(maxsplit=1)
        if len(parts) < 2:
            continue
        values[parts[0]] = split_conf_entries(parts[1])
    return values


def inspect_root_file(input_file: Path) -> tuple[str, list[dict[str, str]]]:
    inspector_code = f'''
#include "TBranch.h"
#include "TBranchElement.h"
#include "TClass.h"
#include "TClonesArray.h"
#include "TFile.h"
#include "TKey.h"
#include "TLeaf.h"
#include "TObjArray.h"
#include "TTree.h"

#include <iostream>
#include <vector>

void inspect_lilak_draw_file()
{{
    auto file = TFile::Open("{cpp_string(str(input_file))}", "read");
    if (file == nullptr || file->IsZombie()) {{
        std::cout << "LILAK_DRAW_ERROR\\tCannot open file" << std::endl;
        return;
    }}

    TTree* tree = dynamic_cast<TTree*>(file->Get("event"));
    if (tree == nullptr) {{
        TIter nextKey(file->GetListOfKeys());
        TKey* key = nullptr;
        while ((key = dynamic_cast<TKey*>(nextKey())) != nullptr) {{
            auto object = key->ReadObj();
            tree = dynamic_cast<TTree*>(object);
            if (tree != nullptr)
                break;
        }}
    }}
    if (tree == nullptr) {{
        std::cout << "LILAK_DRAW_ERROR\\tNo TTree found" << std::endl;
        return;
    }}

    std::cout << "LILAK_DRAW_TREE\\t" << tree->GetName() << std::endl;
    auto branches = tree->GetListOfBranches();
    std::vector<TClonesArray*> branchArrays(branches->GetEntriesFast(), nullptr);
    for (auto iBranch = 0; iBranch < branches->GetEntriesFast(); ++iBranch) {{
        auto branch = dynamic_cast<TBranch*>(branches->At(iBranch));
        if (branch == nullptr)
            continue;
        tree->SetBranchAddress(branch->GetName(), &branchArrays[iBranch]);
    }}
    if (tree->GetEntries() > 0)
        tree->GetEntry(0);

    for (auto iBranch = 0; iBranch < branches->GetEntriesFast(); ++iBranch) {{
        auto branch = dynamic_cast<TBranch*>(branches->At(iBranch));
        if (branch == nullptr)
            continue;

        TString branchName = branch->GetName();
        TString branchClassName = branch->GetClassName();
        TString elementClassName = "";

        if (branchArrays[iBranch] != nullptr && branchArrays[iBranch]->GetClass() != nullptr)
            elementClassName = branchArrays[iBranch]->GetClass()->GetName();

        auto branchElement = dynamic_cast<TBranchElement*>(branch);
        if (elementClassName.IsNull() && branchElement != nullptr && branchElement->GetClonesName() != nullptr)
            elementClassName = branchElement->GetClonesName();

        TClass* expectedClass = nullptr;
        EDataType expectedType = kOther_t;
        branch->GetExpectedType(expectedClass, expectedType);
        if (branchClassName.IsNull() && expectedClass != nullptr)
            branchClassName = expectedClass->GetName();

        if (elementClassName.IsNull()) {{
            auto leaf = dynamic_cast<TLeaf*>(branch->GetListOfLeaves()->At(0));
            if (leaf != nullptr)
                elementClassName = leaf->GetTypeName();
        }}

        std::cout << "LILAK_DRAW_BRANCH\\t"
                  << branchName << "\\t"
                  << branchClassName << "\\t"
                  << elementClassName << std::endl;
    }}
}}
'''

    with tempfile.TemporaryDirectory(prefix="lilak_draw_inspect_") as temp_dir:
        macro_file = Path(temp_dir) / "inspect_lilak_draw_file.C"
        macro_file.write_text(inspector_code, encoding="utf-8")
        command = ["root", "-l", "-b", "-q", str(macro_file)]
        result = subprocess.run(command, text=True, capture_output=True)

    tree_name = "event"
    branches: list[dict[str, str]] = []
    for line in result.stdout.splitlines():
        if line.startswith("LILAK_DRAW_TREE\t"):
            tree_name = line.split("\t", 1)[1].strip()
        elif line.startswith("LILAK_DRAW_BRANCH\t"):
            parts = line.split("\t")
            while len(parts) < 4:
                parts.append("")
            branch_name = parts[1].strip()
            branch_class = parts[2].strip()
            element_class = parts[3].strip()
            if not branch_name:
                continue
            if not element_class or element_class == "TClonesArray":
                element_class = branch_class
            if element_class == "TClonesArray":
                element_class = "TObject"
            branches.append({
                "name": branch_name,
                "class": branch_class,
                "element_class": element_class,
            })

    if result.returncode != 0 and not branches:
        print(result.stderr, file=sys.stderr)
    return tree_name, branches


def make_draw_parameter_file(
    input_file: Path,
    tree_name: str,
    branches: list[dict[str, str]],
    member_map: dict[str, list[str]],
) -> str:
    lines = [
        "# This parameter file was generated by:",
        f"#   lilak draw {input_file}",
        "#",
        "# Draw member examples come from meta/lilak_read_members.conf.",
        "",
        f'draw/input "{input_file}" "{tree_name}"',
        "",
    ]

    for branch in branches:
        element_class = branch["element_class"] or "unknown"
        members = member_map.get(element_class, [])
        if not members:
            continue
        lines.append(f"# {branch['name']} : {element_class}")
        lines.append(f"# reference: https://lilak-project.github.io/lilak_doxygen/class{element_class}.html")
        for member in members:
            expr = ":".join(f"{branch['name']}.{part}" for part in member.split(":"))
            lines.append(f"draw/{branch['name']} {expr}")
        lines.append("")

    if len(lines) <= 7:
        lines.extend([
            "# No configured draw member examples matched branches in this file.",
            "# Add entries to meta/lilak_read_members.conf and run again.",
            "",
        ])

    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    if len(sys.argv) < 2 or not sys.argv[1].strip():
        print("Usage: lilak draw [output.root] [parameter.mac]", file=sys.stderr)
        return 1

    input_file = Path(sys.argv[1]).expanduser()
    if not input_file.is_absolute():
        input_file = (Path.cwd() / input_file).resolve()

    if len(sys.argv) >= 3 and sys.argv[2].strip():
        output_file = Path(sys.argv[2]).expanduser()
        if not output_file.is_absolute():
            output_file = (Path.cwd() / output_file).resolve()
    else:
        output_file = Path.cwd() / f"tree_draw_{safe_name(input_file.stem, 'file')}.mac"

    tree_name, branches = inspect_root_file(input_file)
    repo_root = Path(__file__).resolve().parent.parent
    member_map = load_recommendation_conf(repo_root / "meta" / "lilak_read_members.conf")
    content = make_draw_parameter_file(input_file, tree_name, branches, member_map)

    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(content, encoding="utf-8")
    print(f"Created {output_file}")
    print(f"Tree: {tree_name}")
    print(f"Branches: {len(branches)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
