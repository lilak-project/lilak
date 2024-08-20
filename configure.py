#!/usr/bin/env python3

import os
import sys

def print_c(message):
    print_head = '\033[94m'
    print_end = '\033[0m'
    print(print_head + message + print_end)

count_block = 0
def bline():
    global count_block
    count_block = count_block + 1
    print_c(f"\n== {count_block} ===============================================================================")

def print_h(*messages):
    print_head = '\033[94m'
    print_end = '\033[0m'
    full_message = ""
    for message in messages:
        full_message = full_message + " " + message
    bline()
    print(print_head + "##" + print_end + full_message)

print_h("LILAK configuration macro")

lilak_path = os.path.dirname(os.path.abspath(__file__))
log_path = os.path.join(lilak_path, "log")
lilak_path_is_set = (os.getenv('LILAK_PATH') == lilak_path)
print()
print("   LILAK_PATH is")
print("  ", lilak_path)

build_option_file_name = os.path.join(log_path, "build_options.cmake")

main_project = "lilak"
build_options0 = {
    "ACTIVATE_EVE": False,
    "BUILD_GEANT4_SIM": False,
    "BUILD_MFM_CONVERTER": False,
    "BUILD_JSONCPP": False,
    #"BUILD_DOXYGEN_DOC": False,
    #"CREATE_GIT_LOG": True,
}
build_options = build_options0.copy()

GRU_dir = "/usr/local/gru"
GET_dir = "/usr/local/get"

list_top_directories = ["build", "data", "log", "macros", "source"]
list_prj_subdir_link = ["container", "detector", "tool", "task"]
list_prj_subdir_xlink = ["source"]
list_sub_packages = ["geant4", "get", "fftw", "mfm"]

project_list = []
def print_project_list(numbering=False):
    global project_list
    global main_project
    if main_project == "lilak":
        print(f"   >> Main: {main_project}")
    if len(project_list) == 0:
        print(f"   No Projects")
    elif numbering:
        for idx, line in enumerate(project_list):
            if idx + 1 < 10:
                idxalp = str(idx + 1)
            else:
                idxalp = chr((idx - 10) + 97)
            if line == main_project:
                print(f"   {idxalp}) Project: {line} (main)")
            else:
                print(f"   {idxalp}) Project: {line}")
    else:
        for line in project_list:
            if line == main_project:
                print(f"   Project: {line} (main)")
            else:
                print(f"   Project: {line}")

def input_e0(question="<Enter/0>", possible_options=['0', '']):
    while True:
        user_input = input(question)
        if user_input in possible_options:
            if user_input == '':
                user_input = 1
            return int(user_input)
        else:
            print("Invalid input. Please try again.")
    return user_input

def input_options(options, question="Type option number(s) to Add. Type <Enter> if non: "):
    list(options)
    idx_option = {}
    for idx, key in enumerate(list(options)):
        if idx + 1 < 10:
            idxalp = str(idx + 1)
        else:
            idxalp = chr((idx - 10) + 97)
        print(f"    {idxalp}) {key}")
        idx_option[idxalp] = key
    print()
    user_options = input(question)
    print()

    print("Selected option(s):")
    print()
    list_chosen_key = []
    if len(user_options) == 0:
        print(f"    --")
    else:
        for idxalp in user_options:
            key = idx_option[idxalp]
            list_chosen_key.append(key)
            print(f"    {idxalp}) {key}")
    return list_chosen_key

first_lilak_configuration = False
if os.path.exists(build_option_file_name):
    with open(build_option_file_name, "r") as f:
        for line in f:
            line = line.strip()
            if line.find("set(") == 0:
                tokens = line[line.find("set(") + 4:].split()
                if len(tokens) > 1:
                    if tokens[0] == "LILAK_PROJECT_MAIN":
                        main_project = tokens[1]
                    elif tokens[0] == "GRUDIR":
                        GRU_dir = tokens[1]
                    elif tokens[0] == "GETDIR":
                        GET_dir = tokens[1]
                    elif tokens[0] != "LILAK_PROJECT_LIST":
                        build_options[tokens[0]] = (True if tokens[1] == "ON" else False)
            elif len(line) > 0 and line != ")" and line.strip().find("CACHE INTERNAL") < 0 and line.strip().find("LILAK") != 0:
                comment = ""
                if line.find("#") > 0:
                    line, comment = line[:line.find("#")].strip(), line[line.find("#") + 1:].strip()
                project_list.append(line)
else:
    first_lilak_configuration = True

confirm = 0
while True:
    ### Loading configuration
    if first_lilak_configuration:
        print_h("First LILAK configuration!")
    else:
        print_h("Loading configuration from", build_option_file_name)
        print()
        for key, value in build_options.items():
            print(f"   {key} = {value}")
        print()
        print_project_list()
        print()
        if confirm == 0:
            confirm = input_e0("Use above options? <Enter/0>: ")
    ### using previous build options
    if confirm == 1:
        print()
        #print("saving options to", build_option_file_name)

        # Generate the new contents for the build options file
        new_contents = ""
        for key, value in build_options.items():
            vonoff = "ON" if value == 1 else "OFF"
            new_contents += f"set({key} {vonoff} CACHE INTERNAL \"\")\n"
        project_all = ""
        for project_name in project_list:
            project_all += '\n    ' + project_name
        new_contents += "\nset(LILAK_PROJECT_LIST ${LILAK_PROJECT_LIST}"
        new_contents += f"{project_all}"
        new_contents += '\n    CACHE INTERNAL ""\n)'
        new_contents += f'\n\nset(GRUDIR {GRU_dir} CACHE INTERNAL "")'
        new_contents += f'\nset(GETDIR {GET_dir} CACHE INTERNAL "")'
        if main_project != "lilak":
            new_contents += f'\n\nset(LILAK_PROJECT_MAIN {main_project} CACHE INTERNAL "")'

        # Check if the file exists and compare the contents
        if os.path.exists(build_option_file_name):
            with open(build_option_file_name, "r") as f:
                current_contents = f.read()

            # If the contents are the same, skip writing
            if current_contents == new_contents:
                print(f"No changes in {build_option_file_name}")
            else:
                print(f"Updating {build_option_file_name}")
                with open(build_option_file_name, "w") as f:
                    f.write(new_contents)
        else:
            print(f"Creating {build_option_file_name}")
            with open(build_option_file_name, "w") as f:
                f.write(new_contents)
#    if confirm == 1:
#        print()
#        print("saving options to", build_option_file_name)
#        with open(build_option_file_name, "w") as f:
#            for key, value in build_options.items():
#                vonoff = "ON" if value == 1 else "OFF"
#                f.write(f"set({key} {vonoff} CACHE INTERNAL \"\")\n")
#            project_all = ""
#            for project_name in project_list:
#                project_all = project_all + '\n    ' + project_name
#            f.write("\nset(LILAK_PROJECT_LIST ${LILAK_PROJECT_LIST}")
#            f.write(f"{project_all}")
#            f.write('\n    CACHE INTERNAL ""\n)')
#            f.write(f'\n\nset(GRUDIR {GRU_dir} CACHE INTERNAL "")')
#            f.write(f'\nset(GETDIR {GET_dir} CACHE INTERNAL "")')
#            if main_project != "lilak":
#                f.write(f'\n\nset(LILAK_PROJECT_MAIN {main_project} CACHE INTERNAL "")')

        top_common_dir = f"{lilak_path}/common/"
        print()
        for item in os.listdir(top_common_dir):
            item_path = os.path.join(top_common_dir, item)
            if os.path.islink(item_path):
                #print(f"Removing existing link: {item_path}")
                os.remove(item_path)
        for project_name in project_list:
            projct_path = os.path.join(lilak_path, project_name)
            ls_directory = os.listdir(projct_path)
            if "common" in ls_directory:
                proj_common_dir = f"{projct_path}/common/"
                ls_proj_common = os.listdir(proj_common_dir)
                for file1 in sorted(os.listdir(proj_common_dir)):
                    if file1.endswith(".common"):
                        continue
                    src_file = os.path.join(proj_common_dir, file1)
                    dest_link = os.path.join(top_common_dir, file1)
                    if os.path.exists(dest_link) or os.path.islink(dest_link):
                        print(f"link already exists: {dest_link}")
                        continue
                    else:
                        print(f"linking {src_file} to {dest_link}")
                        os.symlink(src_file, dest_link)
                        #print(f"Link created for {src_file} -> {dest_link}")
        print()
        for project_name in project_list:
            projct_path = os.path.join(lilak_path, project_name)
            project_cmake_file_name = os.path.join(projct_path, "CMakeLists.txt")

            # Generate the new contents
            new_contents = "set(LILAK_SOURCE_DIRECTORY_LIST ${LILAK_SOURCE_DIRECTORY_LIST}\n"
            ls_project = os.listdir(lilak_path+"/"+project_name)
            for directory_name in ls_project:
                if directory_name in list_prj_subdir_link:
                    new_contents += "    ${CMAKE_CURRENT_SOURCE_DIR}/" + directory_name + "\n"
            new_contents += '    CACHE INTERNAL ""\n)\n\n'

            new_contents += "set(LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF ${LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF}\n"
            for directory_name in ls_project:
                if directory_name in list_prj_subdir_xlink:
                    new_contents += "    ${CMAKE_CURRENT_SOURCE_DIR}/" + directory_name + "\n"
                    break
            new_contents += '    CACHE INTERNAL ""\n)\n\n'

            for directory_name in ls_project:
                if directory_name in list_sub_packages:
                    new_contents += "set(LILAK_" + directory_name.upper() + "_SOURCE_DIRECTORY_LIST ${LILAK_" + directory_name.upper() + "_SOURCE_DIRECTORY_LIST}\n"
                    new_contents += "    ${CMAKE_CURRENT_SOURCE_DIR}/" + directory_name + "\n"
                    new_contents += '    CACHE INTERNAL ""\n)\n\n'

            new_contents += """file(GLOB MACROS_FOR_EXECUTABLE_PROCESS ${CMAKE_CURRENT_SOURCE_DIR}/macros*/*.cc)

set(LILAK_EXECUTABLE_LIST ${LILAK_EXECUTABLE_LIST}
    ${MACROS_FOR_EXECUTABLE_PROCESS}
    CACHE INTERNAL ""
)"""

            # Check if the file exists and compare the contents
            if os.path.exists(project_cmake_file_name):
                with open(project_cmake_file_name, "r") as f:
                    current_contents = f.read()

                # If the contents are the same, skip writing
                if current_contents == new_contents:
                    print(f"No changes in {project_cmake_file_name}")
                    continue
                else:
                    print(f"Updating {project_cmake_file_name}")
            else:
                print(f"Creating {project_cmake_file_name}")

            # Write the new contents to the file
            with open(project_cmake_file_name, "w") as f:
                f.write(new_contents)

#        for project_name in project_list:
#            projct_path = os.path.join(lilak_path, project_name)
#            project_cmake_file_name = os.path.join(projct_path, "CMakeLists.txt")
#            with open(project_cmake_file_name, "w") as f:
#                print("creating", project_cmake_file_name)
#                ls_project = os.listdir(f"{lilak_path}/{project_name}")
#                f.write("set(LILAK_SOURCE_DIRECTORY_LIST ${LILAK_SOURCE_DIRECTORY_LIST}\n")
#                for directory_name in ls_project:
#                    if directory_name in list_prj_subdir_link:
#                        f.write("    ${CMAKE_CURRENT_SOURCE_DIR}/" + directory_name + "\n")
#                f.write('    CACHE INTERNAL ""\n)\n\n')
#
#                f.write("set(LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF ${LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF}\n")
#
#                for directory_name in ls_project:
#                    if directory_name in list_prj_subdir_xlink:
#                        f.write("    ${CMAKE_CURRENT_SOURCE_DIR}/" + directory_name + "\n")
#                        break
#                f.write('    CACHE INTERNAL ""\n)\n\n')
#
#                for directory_name in ls_project:
#                    if directory_name in list_sub_packages:
#                        f.write("set(LILAK_" + directory_name.upper() + "_SOURCE_DIRECTORY_LIST ${LILAK_" + directory_name.upper() + "_SOURCE_DIRECTORY_LIST}\n")
#                        f.write("    ${CMAKE_CURRENT_SOURCE_DIR}/" + directory_name + "\n")
#                        f.write('    CACHE INTERNAL ""\n)\n\n')
#
#                # executables
#                f.write("""file(GLOB MACROS_FOR_EXECUTABLE_PROCESS ${CMAKE_CURRENT_SOURCE_DIR}/macros*/*.cc)
#
#set(LILAK_EXECUTABLE_LIST ${LILAK_EXECUTABLE_LIST}
#    ${MACROS_FOR_EXECUTABLE_PROCESS}
#    CACHE INTERNAL ""
#)""")
        break

    ### for new build options
    print_h("Build options")
    print()
    build_options = build_options0.copy()

    list_chosen_key = input_options(build_options, question="Type option number(s) to Add. Type <Enter> if non: ")
    for key in list_chosen_key:
        build_options[key] = True
        if key == "BUILD_MFM_CONVERTER":
            print()
            print(f"       GRU directory is")
            print(f"       {GRU_dir}")
            user_input_gg = input(f"       Type path to change directory, or else press <Enter>: ")
            if user_input_gg != '':
                GRU_dir = user_input_gg
            print()
            print(f"       GET directory is")
            print(f"       {GET_dir}")
            user_input_gg = input(f"       Type path to change directory, or else press <Enter>: ")
            if user_input_gg != '':
                GET_dir = user_input_gg

    ### project
    print_h("Add Project")
    print()
    user_input_project = "x"
    project_list = []
    while len(user_input_project) > 0:
        ls_top = os.listdir("./")
        list_prj_directories = []
        for directory_name in ls_top:
            if directory_name in list_top_directories:
                continue
            if (os.path.isdir(directory_name)):
                ls_directory = os.listdir(directory_name)
                is_project_directory = False
                for subdir in ls_directory:
                    if subdir in list_prj_subdir_link:
                        is_project_directory = True
                        break
                if is_project_directory:
                    list_prj_directories.append(directory_name)

        list_chosen_key = input_options(list_prj_directories, question="Type project directory number(s) to Add. Type <Enter> if non: ")
        for key in list_chosen_key:
            project_list.append(key)
        break

    ### main project
    if False:
        print_h("Select main project")
        print()
        main_project = "lilak"
        if len(project_list) == 1:
            main_project = project_list[0]
        print_project_list(True)
        print()
        while True:
            if len(project_list) == 0:
                break
            input_main_project = input(f"Select index (or name) to set as main project. Type <Enter> to set main as {main_project}: ")
            if input_main_project in project_list:
                print(f"Main project is {main_project}")
                break
            elif input_main_project.isdigit() and int(input_main_project) <= len(project_list):
                main_project = project_list[int(input_main_project) - 1]
                print(f"Main project is {main_project}")
                break
            elif len(input_main_project) == 0:
                print(f"Main project is {main_project}")
                break
            else:
                print(f"Project must be one in the list!")
    confirm = 1
    first_lilak_configuration = False

print_h("Building lilak")
print()

if lilak_path_is_set == False:
    print("   LILAK_PATH is not set. I will set it for you just this time!")
    print()
    os.environ["LILAK_PATH"] = lilak_path

#####################################################################
if True:
    add_class_lines = ""
    include_lines = ""
    list_of_classes = []

    # Predefined list of directories to search within
    search_list = project_list
    search_list.append("source")

    # Function to scan directories and collect class names
    def scan_directories(base_path, directories):
        for directory in directories:
            dir_path = os.path.join(base_path, directory)
            for root, dirs, files in os.walk(dir_path):
                for file in files:
                    if file.endswith(".h"):
                        class_name = file[:-2]  # Remove the '.h' extension to get the class name
                        source_file = f"{class_name}.cpp"
                        # Check if the corresponding source file exists in the same directory
                        if source_file in files:
                            relative_path = os.path.relpath(root, lilak_path)
                            # Add the class name and its relative path to the list
                            list_of_classes.append((class_name, relative_path))
                            relative_path = os.path.relpath(root, lilak_path)

    # Scan the directories in project_list
    scan_directories(lilak_path, search_list)

    # Sort the list of classes by the length of the class name
    list_of_classes.sort(key=lambda x: len(x[0]))

    # List of keywords to exclude
    exclude_name = ["LKDetectorSystem"]
    exclude_path = ["zzz", "temp", "figures", "data"]
    include_path = ["detector", "task"]

    # Construct the lines for adding classes and including headers
    for class_name, path_name in list_of_classes:
        if any(keyword in path_name for keyword in exclude_path):
            continue

        if any(keyword in class_name for keyword in exclude_name):
            continue

        if any(keyword in path_name for keyword in include_path):
            #print(class_name, path_name)
            if not add_class_lines: add_class_lines  = f'    if      (name=="{class_name}") {{ e_info << "Adding {class_name}" << endl; fRun -> Add(new {class_name}); }}\n'
            else:                   add_class_lines += f'    else if (name=="{class_name}") {{ e_info << "Adding {class_name}" << endl; fRun -> Add(new {class_name}); }}\n'
            include_lines += f'#include "{class_name}.h"\n'
    add_class_lines += f'    else {{ e_warning << "Class {class_name} is not in the class factory!" << endl; }}\n'

    # Create the source file content
    source_content = f"""#include "LKClassFactory.h"
#include "LKRun.h"


{include_lines}
void LKClassFactory::Add(TString name)
{{
{add_class_lines}}}
"""

    # Create the header file content
    header_content = """#ifndef LKCLASSFACTORY_HH
#define LKCLASSFACTORY_HH

#include "TString.h"
#include "LKLogger.h"

class LKRun;

class LKClassFactory
{
    public:
        LKClassFactory(LKRun* run) { fRun = run; }
        virtual ~LKClassFactory() {}

        void Add(TString name);

        LKRun* fRun;
};

#endif
"""

    # Write the source and header files
    source_file_path = os.path.join(lilak_path, "source/base/LKClassFactory.cpp")
    header_file_path = os.path.join(lilak_path, "source/base/LKClassFactory.h")

    with open(source_file_path, 'w') as source_file: print(source_content, file=source_file)
    with open(header_file_path, 'w') as header_file: print(header_content, file=header_file)

    print(source_file_path)
    print(header_file_path)

#####################################################################
original_directory = os.getcwd()
if not os.path.exists(f'{lilak_path}/build'): os.makedirs(f'{lilak_path}/build')
os.chdir(f'{lilak_path}/build')
os.system('cmake ..')
os.system('make -j4')
os.chdir(f'{original_directory}')

#####################################################################
if True:
    lilak_sh_content = f"""#!/bin/bash

# Add the lilak directory to the PATH
export LILAK_PATH="{lilak_path}"
export PATH="$LILAK_PATH:$PATH"

# Unset any previous definition of the lilak function if it exists
if declare -f lilak > /dev/null; then
    unset -f lilak
fi

# Define a function to update git repositories
update_git_repos() {{
    local original_dir=$(pwd)
    local dirs_to_update=("{lilak_path}" {" ".join([os.path.join(lilak_path, project) for project in project_list])})
    for directory in "${{dirs_to_update[@]}}"; do
        if [ -d "$directory" ]; then
            cd "$directory"
            branch=$(git rev-parse --abbrev-ref HEAD)
            if [ $? -eq 0 ]; then
                echo "Updating $directory on branch $branch"
                git pull origin "$branch"
            else
                echo "Failed to get current branch in $directory"
            fi
        fi
    done
    cd "$original_dir"
}}

# Define the lilak function
lilak() {{
    if [ -z "$1" ]; then
        echo "Usage: lilak {{home|build|clean-build|update|example|list-project|doc|find|run|github}} [input]"
        echo
        echo "Commands:"
        echo "  home               Navigate to the lilak home directory."
        echo "  build              Run the configure.py script to build the package."
        echo "  clean-build        Clean the build directory and rebuild the package (asks for confirmation)."
        echo "  update             Check the current branch and pull updates for lilak and project directories."
        echo "  example            Navigate to the lilak examples directory."
        echo "  list-project       List projects that are being compiled together."
        echo "  doc [input]        Print the reference link to the class documentation."
        echo "  find [input]       Find and navigate to the directory containing files with the specified input term, and print the path."
        echo "  run [input]        Execute the ROOT script with the provided input."
        echo "  github             Print the GitHub URL of the lilak project."
        echo
        echo "Project commands:"
        for project in {" ".join(project_list)}; do
            echo "  $project           Navigate to the macros directory of the project $project."
        done
        return
    fi

    case $1 in
        home)
            cd "$LILAK_PATH" || echo "Directory not found: $LILAK_PATH"
            echo "Changed directory to: $(pwd)"
            ;;
        build)
            local original_dir=$(pwd)
            cd "$LILAK_PATH"
            python3 "$LILAK_PATH/configure.py"
            export LD_LIBRARY_PATH="$LILAK_PATH/build:$LD_LIBRARY_PATH"
            cd "$original_dir"
            ;;
        clean-build)
            echo -n "Are you sure you want to clean the build directory? (y/n): "
            read confirm
            if [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]]; then
                local original_dir=$(pwd)
                rm -rf "$LILAK_PATH/build"
                mkdir -p "$LILAK_PATH/build"
                cd "$LILAK_PATH"
                python3 "$LILAK_PATH/configure.py"
                export LD_LIBRARY_PATH="$LILAK_PATH/build:$LD_LIBRARY_PATH"
                cd "$original_dir"
            else
                echo "Clean build canceled."
            fi
            ;;
        update)
            update_git_repos
            ;;
        example)
            cd "$LILAK_PATH/examples" || echo "Directory not found: $LILAK_PATH/examples"
            echo "Changed directory to: $(pwd)"
            ;;
        list-project)
            echo "Projects being compiled together:"
            echo "{os.linesep.join(project_list)}"
            ;;
        doc)
            if [ -z "$2" ]; then
                echo "Documentation link: https://lilak-project.github.io/lilak_doxygen/index.html"
            else
                url="https://lilak-project.github.io/lilak_doxygen/class$2.html"
                if curl --output /dev/null --silent --head --fail "$url"; then
                    echo "Documentation link: $url"
                else
                    echo "No documentation found for class $2."
                fi
            fi
            ;;
        find)
            if [ -z "$2" ]; then
                echo "Error: Please provide the search term."
            else
                file=$(find "$LILAK_PATH" -name "*$2*" -print -quit)
                if [ -n "$file" ]; then
                    dir=$(dirname "$file")
                    echo "Found '$file'"
                    if cd "$dir"; then
                        echo "Changing directory to: $(pwd)"
                    else
                        echo "Directory not found: $dir"
                    fi
                else
                    echo "No files containing '$2' found."
                fi
            fi
            ;;
        run)
            if [ -z "$2" ]; then
                echo "Error: Please provide the input for the ROOT script."
            else
                root -l '{lilak_path}/macros/run_lilak.C("'"$2"'")'
            fi
            ;;
        github)
            echo "https://github.com/lilak-project"
            ;;
        *)
            if [[ " {" ".join(project_list)} " =~ " $1 " ]]; then
                target_dir="$LILAK_PATH/$1/macros"
                if cd "$target_dir"; then
                    echo "Changed directory to: $(pwd)"
                else
                    echo "Directory not found: $target_dir"
                fi
            else
                echo "Unknown command or project: $1"
                lilak
            fi
            ;;
    esac
}}

# Define the lilak command completion function
_lilak_completions() {{
    COMPREPLY=()
    local curr_word prev_word
    curr_word="${{COMP_WORDS[COMP_CWORD]}}"
    prev_word="${{COMP_WORDS[COMP_CWORD-1]}}"
    local commands="home build clean-build update example list-project doc find run github {" ".join(project_list)}"

    if [[ ${{COMP_CWORD}} == 1 ]]; then
        COMPREPLY=( $(compgen -W "${{commands}}" -- "${{curr_word}}") )
    elif [[ $prev_word == "run" ]]; then
        COMPREPLY=( $(compgen -f "${{curr_word}}") )
    fi

    return 0
}}

# Enable command completion for lilak
complete -F _lilak_completions lilak

# Optional: Add a message to confirm the script is sourced correctly
echo "LILAK is set. Use 'lilak {{home|build|clean-build|update|example|list-project|doc|find|run|github}}'"
"""

    # Path to the lilak.sh file
    lilak_sh_path = os.path.join(lilak_path, "lilak.sh")

    # Write the content to the lilak.sh file
    with open(lilak_sh_path, "w") as lilak_sh_file:
        lilak_sh_file.write(lilak_sh_content)

    # Make the lilak.sh script executable
    os.chmod(lilak_sh_path, 0o755)

    print(f"""
1. Set up LILAK environment
source {lilak_path}/lilak.sh

2. Add following to .rootrc:
Rint.Logon: {lilak_path}/macros/rootlogon.C
""")
else:
    print_h("How to run lilak")
    print(f"""
1) Copy and paste the line below into the login shell script and source the script!

   export LILAK_PATH=\"{lilak_path}\"

2) Copy and paste the line below into ~/.rootrc:

   Rint.Logon: {lilak_path}/macros/rootlogon.C

3) Now Run root!
""")
