#!/usr/bin/env python3

import os

count_block = 0
def bline():
    global count_block
    count_block = count_block + 1
    print(f"\n== {count_block} ===============================================================================")

bline()
print("## LILAK configuration macro")

lilak_path = os.path.dirname(os.path.abspath(__file__))
log_path = os.path.join(lilak_path,"log")
os.environ["LILAK_PATH"] = lilak_path
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
questions = {
    "ACTIVATE_EVE":         "1) Activate ROOT EVE?            <1/0>: ",
    "BUILD_GEANT4_SIM":     "2) Build Geant4 Simulation?      <1/0>: ",
    "BUILD_MFM_CONVERTER":  "3) Build MFM Converter?          <1/0>: ",
    "BUILD_JSONCPP":        "4) Build JSONCPP?                <1/0>: ",
    "CREATE_GIT_LOG":       "5) Create Git Log? [Recommanded] <1/0>: ",
    "BUILD_DOXYGEN_DOC":    "6) Build Doxygen documentation?  <1/0>: ",
}

list_top_directories = ["build","data","log","macros","source"]
list_prj_subdir_link = ["container","detector","tool","task"]
list_prj_subdir_xlink = ["source"]

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
            if line==main_project:  print(f"   {idx}) Project: {line} (main)")
            else:                   print(f"   {idx}) Project: {line}")
    else:
        for line in project_list:
            if line==main_project:  print(f"   Project: {line} (main)")
            else:                   print(f"   Project: {line}")

def input01(question="<1/0>",possible_options=['0','1']):
    while True:
        user_input = input(question)
        if user_input in ['0', '1']:
            return int(user_input)
        else:
            print("Invalid input. Please try again.")
    return user_input

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
    bline()
    print("## Loading configuration from", build_option_file_name)
    print()
    for key, value in options.items():
        print(f"   {key} = {value}")
    print()
    print_project_list()
    print()
    if confirm==0:
        confirm = input01("Use above options? <1/0>: ")
    if confirm==1:
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
                print("creating CMakeLists.txt for ", project_cmake_file_name)
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

                f.write("set(LILAK_GEANT4_SOURCE_DIRECDTORY_LIST ${LILAK_GEANT4_SOURCE_DIRECDTORY_LIST}\n")
                for directory_name in ls_project:
                    if directory_name=="geant4":
                        f.write("    ${CMAKE_CURRENT_SOURCE_DIR}/geant4\n")
                        break
                f.write('    CACHE INTERNAL ""\n)\n\n')

                f.write("set(LILAK_MFM_SOURCE_DIRECDTORY_LIST ${LILAK_MFM_SOURCE_DIRECDTORY_LIST}\n")
                for directory_name in ls_project:
                    if directory_name=="mfm":
                        f.write("    ${CMAKE_CURRENT_SOURCE_DIR}/mfm\n")
                        break
                f.write('    CACHE INTERNAL ""\n)\n\n')

                f.write("""file(GLOB MACROS_FOR_EXECUTABLE_PROCESS ${CMAKE_CURRENT_SOURCE_DIR}/macros/*.cc)

set(LILAK_EXECUTABLE_LIST ${LILAK_EXECUTABLE_LIST}
    ${MACROS_FOR_EXECUTABLE_PROCESS}
    CACHE INTERNAL ""
)""")
        break

    bline()
    print("## Options")
    options = options0.copy()
    for key, value in options.items():
        options[key] = input01(questions[key])

    print()
    user_input_project = "x"
    project_list = []
    while len(user_input_project)>0:
        user_input_project = input("Type project name to add. Type <Enter> if non: ")
        if len(user_input_project)>0:
            if os.path.exists(user_input_project) and os.path.isdir(user_input_project):
                print(f"Adding project {user_input_project}")
                if user_input_project in project_list:
                    print(f"Project {user_input_project} already added!")
                else:
                    project_list.append(user_input_project)
            else:
                print(f"Directory {user_input_project} do not exist in lilak home directory!")
        else:
            bline()
            print("## List of projects")
            print_project_list(True)
            main_project = "lilak"
            while True:
                if len(project_list)==0:
                    break
                main_project = input("Select index (or name) to set as main project. Type <Enter> to set main as lilak: ")
                if main_project in project_list:
                    print(f"Main project is {main_project}")
                    break
                elif main_project.isdigit() and int(main_project) < len(project_list):
                    main_project = project_list[int(main_project)]
                    print(f"Main project is {main_project}")
                    break
                elif len(main_project)==0:
                    main_project = "lilak"
                    print(f"Main project is {main_project}")
                    break
                else:
                    print(f"Project must be one in the list!")
    confirm = 1

bline()
print( "## How to build lilak")
print()
print(f"   1) Open login script and put:")
print(f"      export LILAK_PATH=\"{lilak_path}\"")
print()
print(f"   2) Open ~/.rootrc and put:")
print(f"      Rint.Logon: {lilak_path}/macros/rootlogon.C")
print()
print(f"   3) Build:")
print(f"      cd {lilak_path}")
print(f"      mkdir build")
print(f"      cd build")
print(f"      cmake ..")
print(f"      make")
