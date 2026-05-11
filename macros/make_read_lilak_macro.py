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


def safe_function_name(path: Path) -> str:
    stem = path.name
    if stem.endswith(".root"):
        stem = stem[:-5]
    return safe_name(f"read_{stem}", "read_file")


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


def load_getter_conf(path: Path) -> dict[str, list[dict[str, str]]]:
    values: dict[str, list[dict[str, str]]] = {}
    if not path.is_file():
        return values
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(maxsplit=1)
        if len(parts) < 2:
            continue
        values[parts[0]] = []
        for entry in split_conf_entries(parts[1]):
            type_expr = entry.split(maxsplit=1)
            if len(type_expr) < 2:
                continue
            values[parts[0]].append({"type": type_expr[0].strip(), "expr": type_expr[1].strip()})
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

void inspect_lilak_read_file()
{{
    auto file = TFile::Open("{cpp_string(str(input_file))}", "read");
    if (file == nullptr || file->IsZombie()) {{
        std::cout << "LILAK_READ_ERROR\\tCannot open file" << std::endl;
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
        std::cout << "LILAK_READ_ERROR\\tNo TTree found" << std::endl;
        return;
    }}

    std::cout << "LILAK_READ_TREE\\t" << tree->GetName() << std::endl;
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

        std::cout << "LILAK_READ_BRANCH\\t"
                  << branchName << "\\t"
                  << branchClassName << "\\t"
                  << elementClassName << std::endl;
    }}
}}
'''

    with tempfile.TemporaryDirectory(prefix="lilak_read_inspect_") as temp_dir:
        macro_file = Path(temp_dir) / "inspect_lilak_read_file.C"
        macro_file.write_text(inspector_code, encoding="utf-8")
        command = ["root", "-l", "-b", "-q", str(macro_file)]
        result = subprocess.run(command, text=True, capture_output=True)

    tree_name = "event"
    branches: list[dict[str, str]] = []
    for line in result.stdout.splitlines():
        if line.startswith("LILAK_READ_TREE\t"):
            tree_name = line.split("\t", 1)[1].strip()
        elif line.startswith("LILAK_READ_BRANCH\t"):
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
                "var": safe_name(branch_name, "Branch"),
            })

    if result.returncode != 0 and not branches:
        print(result.stderr, file=sys.stderr)
    return tree_name, branches


def make_branch_globals(branches: list[dict[str, str]]) -> str:
    if not branches:
        return "// No branches were found.\n"
    lines = []
    for branch in branches:
        lines.append(f'TClonesArray* f{branch["var"]}Array = nullptr;')
    return "\n".join(lines) + "\n"


def make_draw_active_globals(branches: list[dict[str, str]]) -> str:
    if not branches:
        return ""
    lines = []
    for branch in branches:
        active_var = safe_name(f"fActive{branch['name']}", "fActive")
        lines.append(f"bool {active_var} = true;")
    return "\n".join(lines) + "\n"


def make_branch_setters(branches: list[dict[str, str]]) -> str:
    if not branches:
        return '    std::cout << "No branches were found in tree." << std::endl;\n'
    lines = []
    for branch in branches:
        lines.append(f'    fReadTree->SetBranchAddress("{branch["name"]}", &f{branch["var"]}Array);')
    return "\n".join(lines) + "\n"


def make_branch_prints(branches: list[dict[str, str]]) -> str:
    if not branches:
        return '    std::cout << "No branches were found in tree." << std::endl;\n'
    lines = ['    std::cout << "# Branches" << std::endl;']
    for branch in branches:
        element_class = branch["element_class"] or "unknown"
        lines.append(f'    std::cout << "  {branch["name"]} : TClonesArray<{element_class}>" << std::endl;')
    return "\n".join(lines) + "\n"


def make_edit_area_summary(branches: list[dict[str, str]]) -> str:
    lines = [
        "// XXX USER EDIT AREA INDEX:",
        "//   XXX-1: Global run controls. Change fInputFileName, fMaxEvents, fDump, fPrintAfter, fRunLoop, fDrawBranch.",
        "//   XXX-2: Draw commands. Add fReadTree->Draw(\"...\") examples in draw_branch().",
    ]
    for index, branch in enumerate(branches, start=3):
        lines.append(f"//   XXX-{index}: Branch loop for {branch['name']} ({branch['element_class']}).")
    return "\n".join(lines) + "\n"


def make_getter_examples(element_var: str, element_class: str, getter_map: dict[str, list[dict[str, str]]]) -> list[str]:
    getters = getter_map.get(element_class, [])
    if not getters:
        return [
            f"                // No getter examples configured for {element_class}.",
            f"                // Add entries to meta/lilak_read_getters.conf to generate examples here.",
        ]
    lines = []
    for getter in getters:
        getter_expr = getter["expr"]
        getter_type = getter["type"]
        value_var = safe_name(f"value_{getter_expr}", "value")
        access_expr = f"{element_var}->{getter_expr}"
        lines.append(f"                {getter_type} {value_var} = {access_expr};")
    return lines


def make_event_loop(branches: list[dict[str, str]], getter_map: dict[str, list[dict[str, str]]]) -> str:
    lines = []
    for edit_index, branch in enumerate(branches, start=3):
        var = branch["var"]
        element_class = branch["element_class"] or "TObject"
        element_var = safe_name(f"el_{var}", "el")
        index_var = safe_name(f"i_{var}", "i")
        num_var = safe_name(f"num_{var}", "num")
        lines.extend([
            "        //===============================================================================",
            f"        // XXX-{edit_index} USER EDIT AREA BEGIN: inspect {branch['name']} branch here.",
            "        //===============================================================================",
            f"        // {branch['name']} : {element_class}",
            f"        // reference: https://lilak-project.github.io/lilak_doxygen/class{element_class}.html",
            f"        auto {num_var} = (f{var}Array != nullptr ? f{var}Array->GetEntriesFast() : 0);",
            f"        for (auto {index_var} = 0; {index_var} < {num_var}; ++{index_var}) {{",
            f"            auto {element_var} = ({element_class}*) f{var}Array->At({index_var});",
            f"            if ({element_var} == nullptr)",
            "                continue;",
            f"            if (dump > 0) {{",
            f"                {element_var}->Print();",
            *make_getter_examples(element_var, element_class, getter_map),
            "            }",
            "        }",
            "        //===============================================================================",
            f"        // XXX-{edit_index} USER EDIT AREA END",
            "        //===============================================================================",
            "",
        ])
    if not lines:
        lines.append("        // No branch loop was generated.")
    return "\n".join(lines).rstrip() + "\n"


def make_draw_examples(branches: list[dict[str, str]], member_map: dict[str, list[str]]) -> str:
    if not branches:
        return '    //   fReadTree->Draw("branch.member");\n'
    lines = []
    for branch in branches:
        active_var = safe_name(f"fActive{branch['name']}", "fActive")
        members = member_map.get(branch["element_class"], [])
        if not members:
            lines.append(f'    // No draw member examples configured for {branch["element_class"]}.')
            lines.append(f'    // Add entries to meta/lilak_read_members.conf to generate examples for {branch["name"]}.')
            lines.append(f'    // fReadTree->Draw("{branch["name"]}.member");')
            lines.append("")
            continue
        array_name = safe_name(f"drawMembers_{branch['name']}", "drawMembers")
        canvas_name = safe_name(f"cvs_{branch['name']}", "cvs")
        index_name = safe_name(f"i_draw_{branch['name']}", "i_draw")
        count_name = safe_name(f"n_draw_{branch['name']}", "n_draw")
        lines.append(f'    if ({active_var}) {{')
        lines.append(f'        // {branch["name"]} : {branch["element_class"]}')
        lines.append(f'        // reference: https://lilak-project.github.io/lilak_doxygen/class{branch["element_class"]}.html')
        member_values = " ,".join(
            f'"{":".join(f"{branch["name"]}.{part}" for part in member.split(":"))}"'
            for member in members
        )
        lines.append(f'        std::vector<TString> {array_name} = {{{member_values}}};')
        lines.append(f'        int nx, ny, {count_name} = {array_name}.size();')
        lines.append(f'        if ({count_name}>0) {{')
        lines.append(f'            painter -> PreDividePad({count_name}, nx, ny);')
        lines.append(f'            auto {index_name} = 1;')
        lines.append(f'            auto {canvas_name} = painter->CanvasResize("{canvas_name}", 500*nx, 380*ny, 1.0);')
        lines.append(f'            painter -> DividePad({canvas_name}, {count_name});')
        lines.append(f'            for (auto member : {array_name})')
        lines.append('            {')
        lines.append(f'                std::cout << "Draw: " << member << std::endl;')
        lines.append(f'                {canvas_name}->cd({index_name}++);')
        lines.append('                if (member.Contains(":"))')
        lines.append('                    fReadTree->Draw(member, "", "colz");')
        lines.append('                else')
        lines.append('                    fReadTree->Draw(member);')
        lines.append('            }')
        lines.append(f'            {canvas_name}->Modified();')
        lines.append(f'            {canvas_name}->Update();')
        lines.append('        }')
        lines.append(f'    }}')
        lines.append("")
    return "\n".join(lines) + "\n"


def make_macro(
    input_file: Path,
    function_name: str,
    tree_name: str,
    branches: list[dict[str, str]],
    getter_map: dict[str, list[dict[str, str]]],
    member_map: dict[str, list[str]],
) -> str:
    return f'''// This macro was generated by:
//   lilak read {input_file}
//
// Purpose:
//   Open a LILAK output ROOT file, connect the event tree branches,
//   read ParameterContainer and RunHeader, and provide an editable event-loop skeleton.
//
// Use dump > 0 to loop over only dump entries and call Print() for every branch element.
// Use dump = 0 for normal looping without automatic element Print().
// Getter examples come from meta/lilak_read_getters.conf.
// Format:
//   ClassName    type Method(), type member, ...
// Draw examples come from meta/lilak_read_members.conf.
//
{make_edit_area_summary(branches)}

//===============================================================================
// XXX-1 USER EDIT AREA BEGIN: global input file and loop controls.
//===============================================================================
TString fInputFileName = "{cpp_string(str(input_file))}";
Long64_t fMaxEvents = -1;
int fDump = 0;
Long64_t fPrintAfter = 2000;
bool fRunLoop = false;
bool fDrawBranch = true;
{make_draw_active_globals(branches)}
//===============================================================================
// XXX-1 USER EDIT AREA END
//===============================================================================

TFile* fReadFile = nullptr;
TObject* fReadPar = nullptr;
TObject* fReadRunHeader = nullptr;
TTree* fReadTree = nullptr;

{make_branch_globals(branches)}
void open_file_and_set_branch(TString fileName);
void read_header_and_parameters(bool print = false);
void read_data(Long64_t maxEvents = -1, int dump = 0, Long64_t printAfter = 2000);
void draw_branch();

void {function_name}()
{{
    open_file_and_set_branch(fInputFileName);
    read_header_and_parameters();
    if (fRunLoop)
        read_data(fMaxEvents, fDump, fPrintAfter);
    if (fDrawBranch)
        draw_branch();
}}

void open_file_and_set_branch(TString fileName)
{{
    fReadFile = TFile::Open(fileName, "read");
    if (fReadFile == nullptr || fReadFile->IsZombie()) {{
        std::cout << "Cannot open " << fileName << std::endl;
        fReadFile = nullptr;
        return;
    }}

    fReadTree = dynamic_cast<TTree*>(fReadFile->Get("{cpp_string(tree_name)}"));
    if (fReadTree == nullptr) {{
        TIter nextKey(fReadFile->GetListOfKeys());
        TKey* key = nullptr;
        while ((key = dynamic_cast<TKey*>(nextKey())) != nullptr) {{
            auto object = key->ReadObj();
            fReadTree = dynamic_cast<TTree*>(object);
            if (fReadTree != nullptr)
                break;
        }}
    }}
    if (fReadTree == nullptr) {{
        std::cout << "No TTree found in " << fileName << std::endl;
        return;
    }}

{make_branch_setters(branches)}
}}

void read_header_and_parameters(bool print)
{{
    if (fReadFile == nullptr) {{
        std::cout << "No file is open." << std::endl;
        return;
    }}

    fReadPar = fReadFile->Get("ParameterContainer");
    fReadRunHeader = fReadFile->Get("RunHeader");

    std::cout << "# LILAK output file" << std::endl;
    std::cout << "File: " << fReadFile->GetName() << std::endl;
    std::cout << "ParameterContainer: " << (fReadPar != nullptr ? "loaded" : "missing") << std::endl;
    std::cout << "RunHeader: " << (fReadRunHeader != nullptr ? "loaded" : "missing") << std::endl;
    std::cout << "Event tree: " << (fReadTree != nullptr ? fReadTree->GetName() : "missing") << std::endl;
{make_branch_prints(branches)}
    if (!print)
        return;

    if (fReadPar != nullptr) {{
        std::cout << std::endl << "# ParameterContainer" << std::endl;
        fReadPar->Print();
    }}
    if (fReadRunHeader != nullptr) {{
        std::cout << std::endl << "# RunHeader" << std::endl;
        fReadRunHeader->Print();
    }}
}}

void draw_branch()
{{
    if (fReadTree == nullptr) {{
        std::cout << "No event tree is loaded." << std::endl;
        return;
    }}
    auto painter = LKPainter::GetPainter();

    //===============================================================================
    // XXX-2 USER EDIT AREA BEGIN: add branch drawing commands here.
    //===============================================================================
    // Examples:
    //   fReadTree->Draw("RawData.fChannel");
    //   fReadTree->Draw("SiHit.fEnergy");
    //   fReadTree->Draw("SiHit.fEnergy:SiHit.fX");
    //
    // Edit or comment out generated examples below for this file.
{make_draw_examples(branches, member_map)}
    //===============================================================================
    // XXX-2 USER EDIT AREA END
    //===============================================================================
}}

void read_data(Long64_t maxEvents, int dump, Long64_t printAfter)
{{
    if (fReadTree == nullptr) {{
        std::cout << "No event tree is loaded." << std::endl;
        return;
    }}

    auto numEntries = fReadTree->GetEntries();
    auto numLoop = numEntries;
    if (maxEvents >= 0 && maxEvents < numLoop)
        numLoop = maxEvents;
    if (dump > 0)
        numLoop = dump;

    std::cout << "# read_data" << std::endl;
    std::cout << "numEntries: " << numEntries << std::endl;
    std::cout << "maxEvents: " << maxEvents << std::endl;
    std::cout << "dump: " << dump << std::endl;
    std::cout << "printAfter: " << printAfter << std::endl;
    std::cout << "numLoop: " << numLoop << std::endl;

    for (auto entry = 0LL; entry < numLoop; ++entry) {{
        if (printAfter > 0 && entry % printAfter == 0)
            std::cout << "entry: " << entry << " / " << numLoop << std::endl;
        fReadTree->GetEntry(entry);

{make_event_loop(branches, getter_map)}    }}
}}
'''


def main() -> int:
    if len(sys.argv) < 2 or not sys.argv[1].strip():
        print("Usage: lilak read [output.root] [macro.C]", file=sys.stderr)
        return 1

    input_file = Path(sys.argv[1]).expanduser()
    if not input_file.is_absolute():
        input_file = (Path.cwd() / input_file).resolve()

    if len(sys.argv) >= 3 and sys.argv[2].strip():
        output_file = Path(sys.argv[2]).expanduser()
        if not output_file.is_absolute():
            output_file = (Path.cwd() / output_file).resolve()
    else:
        output_file = Path.cwd() / f"{safe_function_name(input_file)}.C"

    function_name = safe_name(output_file.stem, safe_function_name(input_file))
    tree_name, branches = inspect_root_file(input_file)
    repo_root = Path(__file__).resolve().parent.parent
    getter_map = load_getter_conf(repo_root / "meta" / "lilak_read_getters.conf")
    member_map = load_recommendation_conf(repo_root / "meta" / "lilak_read_members.conf")
    content = make_macro(input_file, function_name, tree_name, branches, getter_map, member_map)

    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(content, encoding="utf-8")
    print(f"Created {output_file}")
    print(f"Tree: {tree_name}")
    print(f"Branches: {len(branches)}")
    for branch in branches:
        print(f"  {branch['name']} : {branch['element_class']}")
    print(f"ROOT command: root -l {output_file}")
    print(f"Function: {function_name}()")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
