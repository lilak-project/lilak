#!/usr/bin/env python3

import os
import sys

def print_c(message):
    print_head = '\033[94m'
    print_end = '\033[0m'
    print(print_head+message+print_end)

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
    print(print_head+"##"+print_end+full_message)

print_h("LILAK configuration macro")

lilak_path = os.path.dirname(os.path.abspath(__file__))
log_path = os.path.join(lilak_path,"log")
lilak_path_is_set = (os.getenv('LILAK_PATH')==lilak_path)
print()
print("   LILAK_PATH is", lilak_path)

build_option_file_name = os.path.join(log_path, "build_options.cmake")

main_project = "lilak"
options0 = {
    "ACTIVATE_EVE": False,
    "BUILD_GEANT4_SIM": False,
    "BUILD_MFM_CONVERTER": False,
    "BUILD_JSONCPP": False,
    #"BUILD_DOXYGEN_DOC": False,
    #"CREATE_GIT_LOG": True,
}
options = options0.copy()

list_top_directories = ["build","data","log","macros","source"]
list_prj_subdir_link = ["container","detector","tool","task"]
list_prj_subdir_xlink = ["source"]
list_sub_packages = ["geant4", "get", "fftw"]

project_list = []
def print_project_list(numbering=False):
    global project_list
    global main_project
    if main_project=="lilak":
        print(f"   >> Main: {main_project}")
    if len(project_list)==0:
        print(f"   No Projects")
    elif numbering:
        for idx, line in enumerate(project_list):
            if idx+1<10:
                idxalp = str(idx+1)
            else:
                idxalp = chr((idx-10) + 97)
            if line==main_project:  print(f"   {idxalp}) Project: {line} (main)")
            else:                   print(f"   {idxalp}) Project: {line}")
    else:
        for line in project_list:
            if line==main_project:  print(f"   Project: {line} (main)")
            else:                   print(f"   Project: {line}")

def input_e0(question="<Enter/0>",possible_options=['0','']):
    while True:
        user_input = input(question)
        if user_input in possible_options:
            if user_input=='':
                user_input = 1
            return int(user_input)
        else:
            print("Invalid input. Please try again.")
    return user_input


def input_options(options,question="Type option number(s) to Add. Type <Enter> if non: "):
    list(options)
    idx_option = {}
    for idx, key in enumerate(list(options)):
        if idx+1<10:
            idxalp = str(idx+1)
        else:
            idxalp = chr((idx-10) + 97)
        print(f"    {idxalp}) {key}")
        idx_option[idxalp] = key
    print()
    user_options = input(question)
    print()

    print("Selected option(s):")
    print()
    list_chosen_key = []
    if len(user_options)==0:
        print(f"    --")
    else:
        for idxalp in user_options:
            key = idx_option[idxalp]
            list_chosen_key.append(key)
            print(f"    {idxalp}) {key}")
    return list_chosen_key

if os.path.exists(build_option_file_name):
    with open(build_option_file_name, "r") as f:
        for line in f:
            line = line.strip()
            if line.find("set(")==0:
                tokens = line[line.find("set(")+4:].split()
                if len(tokens)>1:
                    if tokens[0]=="LILAK_PROJECT_MAIN":
                        main_project = tokens[1]
                    elif tokens[0]!="LILAK_PROJECT_LIST":
                        options[tokens[0]] = (True if tokens[1]=="ON" else False)
            elif len(line)>0 and line!=")" and line.strip().find("CACHE INTERNAL")<0 and line.strip().find("LILAK")!=0:
                comment = ""
                if line.find("#")>0:
                    line, comment = line[:line.find("#")].strip(), line[line.find("#")+1:].strip()
                project_list.append(line)

confirm = 0
while True:
    print_h("Loading configuration from", build_option_file_name)
    print()
    for key, value in options.items():
        print(f"   {key} = {value}")
    print()
    print_project_list()
    print()
    if confirm==0:
        confirm = input_e0("Use above options? <Enter/0>: ")
    if confirm==1:
        print()
        print("saving options to", build_option_file_name)
        with open(build_option_file_name, "w") as f:
            for key, value in options.items():
                vonoff = "ON" if value==1 else "OFF"
                f.write(f"set({key} {vonoff} CACHE INTERNAL \"\")\n")
            project_all = ""
            for project_name in project_list:
                project_all = project_all+'\n    '+project_name
            f.write("\nset(LILAK_PROJECT_LIST ${LILAK_PROJECT_LIST}")
            f.write(f"{project_all}")
            f.write('\n    CACHE INTERNAL ""\n)')
            if main_project!="lilak":
                f.write(f'\n\nset(LILAK_PROJECT_MAIN {main_project} CACHE INTERNAL "")')

        for project_name in project_list:
            projct_path = os.path.join(lilak_path,project_name)
            project_cmake_file_name = os.path.join(projct_path, "CMakeLists.txt")
            with open(project_cmake_file_name, "w") as f:
                print("creating", project_cmake_file_name)
                ls_project = os.listdir(project_name)
                f.write("set(LILAK_SOURCE_DIRECTORY_LIST ${LILAK_SOURCE_DIRECTORY_LIST}\n")
                for directory_name in ls_project:
                    if directory_name in list_prj_subdir_link:
                        f.write("    ${CMAKE_CURRENT_SOURCE_DIR}/"+directory_name+"\n")
                f.write('    CACHE INTERNAL ""\n)\n\n')

                f.write("set(LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF ${LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF}\n")

                for directory_name in ls_project:
                    if directory_name in list_prj_subdir_xlink:
                        f.write("    ${CMAKE_CURRENT_SOURCE_DIR}/"+directory_name+"\n")
                        break
                f.write('    CACHE INTERNAL ""\n)\n\n')

                for directory_name in ls_project:
                    if directory_name in list_sub_packages:
                        f.write("set(LILAK_"+directory_name.upper()+"_SOURCE_DIRECDTORY_LIST ${LILAK_"+directory_name.upper()+"_SOURCE_DIRECDTORY_LIST}\n")
                        f.write("    ${CMAKE_CURRENT_SOURCE_DIR}/"+directory_name+"\n")
                        f.write('    CACHE INTERNAL ""\n)\n\n')

                # executables
                f.write("""file(GLOB MACROS_FOR_EXECUTABLE_PROCESS ${CMAKE_CURRENT_SOURCE_DIR}/macros/*.cc)

set(LILAK_EXECUTABLE_LIST ${LILAK_EXECUTABLE_LIST}
    ${MACROS_FOR_EXECUTABLE_PROCESS}
    CACHE INTERNAL ""
)""")
        break

    print_h("Build options")
    print()
    options = options0.copy()

    list_chosen_key = input_options(options,question="Type option number(s) to Add. Type <Enter> if non: ")
    for key in list_chosen_key:
        options[key] = True

    print_h("Add Project")
    print()
    user_input_project = "x"
    project_list = []
    while len(user_input_project)>0:
        ls_top = os.listdir("./")
        list_prj_directory = []
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
                   list_prj_directory.append(directory_name)


        list_chosen_key = input_options(list_prj_directory,question="Type project directory number(s) to Add. Type <Enter> if non: ")
        for key in list_chosen_key:
            project_list.append(key)
        break

    if True:
        print_h("Select main project")
        print()
        main_project = "lilak"
        if len(project_list)==1:
            main_project = project_list[0]
        print_project_list(True)
        print()
        while True:
            if len(project_list)==0:
                break
            input_main_project = input(f"Select index (or name) to set as main project. Type <Enter> to set main as {main_project}: ")
            if input_main_project in project_list:
                print(f"Main project is {main_project}")
                break
            elif input_main_project.isdigit() and int(input_main_project) < len(project_list):
                main_project = project_list[int(input_main_project)]
                print(f"Main project is {main_project}")
                break
            elif len(input_main_project)==0:
                #main_project = "lilak"
                print(f"Main project is {main_project}")
                break
            else:
                print(f"Project must be one in the list!")
    confirm = 1


print_h("Building lilak")

if lilak_path_is_set==False:
    print()
    print("   LILAK_PATH is not set. I will set it for you just this time!")
    print()
    os.environ["LILAK_PATH"] = lilak_path

os.system('mkdir -p build')
os.chdir('build')
os.system('cmake ..')
os.system('make -j4')

print_h("How to run lilak")
print(f"""
   1) Copy and paste the line below into the login shell script and source the script!

      export LILAK_PATH=\"{lilak_path}\"

   2) Copy and paste the line below into ~/.rootrc:

      Rint.Logon: {lilak_path}/macros/rootlogon.C

   3) Now Run root!
""")
