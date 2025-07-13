#!/usr/bin/env python3

import os
import re
import argparse

class lilak_configuration:
    def __init__(self):
        pass

    def run_configuration(self):
        self.init_configuration()
        self.args = self.arg_parser.parse_args()
        #if self.args.project is not None:
        #    print(self.args.project)
        #    return
        #run_cmake = self.args.cmake
        print(self.args)
        run_cmake = False
        self.configure_and_build(run_cmake)

    def init_configuration(self):
        self.count_headers = [0,0]
        self.print_header("Init configuration")
        #self.lilak_path = os.path.dirname(os.path.abspath(__file__))
        self.lilak_path = os.getcwd()
        if self.lilak_path.endswith('/macros'):
            self.lilak_path = self.lilak_path[:-7]
        print(f'LILAK path is\n{self.lilak_path}')
        max_jobs = os.cpu_count()
        if max_jobs>12: max_jobs = 12
        else: max_jobs = max_jobs - 1
        self.arg_parser = argparse.ArgumentParser(description='Configure and build LILAK')
        self.arg_parser.add_argument('-j', '--jobs', type=int, default=max_jobs, help='Number of jobs to run in parallel for make')
        self.arg_parser.add_argument('-t', '--test', action='store_true', help='configuration test without make')
        #self.arg_parser.add_argument('-c', '--cmake', type=int, default=1, help='run cmake')
        #self.arg_parser.add_argument('-p', '--project', type=str, nargs='?', const="default_project", help='Create a new project')

    def configure_and_build(self, run_cmake=False):
        self.init_build()
        self.check_previous_configuration()
        while True:
            option = self.confirm_build_options()
            if option=='': break
            elif option=='r':
                run_cmake = True
                break
            self.select_build_options()
            self.select_and_add_project()
            self.select_main_project()
        if os.path.exists(self.lilak_path+"/build/"):
            if len(os.listdir(self.lilak_path+"/build/"))==0:
                run_cmake = True
        change0 = self.create_cmakelists_for_each_project()
        change1 = self.create_build_option_file()
        change2 = self.create_classfactory()
        change2 = self.g4dc_classfactory()
        #change3 = self.create_lilak_class_reference()
        lf_changes = [change0, change1, change2]
        self.copy_common_files_from_project()
        run_cmake = run_cmake or (True in lf_changes)
        self.create_lilak_command_script()
        if self.args.test==False:
            self.build_lilak(run_cmake)
        self.summary()

    def find_with_expression(self, expression, options):
        pattern = re.escape(expression).replace(r'\*', '.*')
        regex = re.compile(f"^{pattern}$")  # Ensure full string match
        return [option for option in options if regex.match(option)]

    def print_header(self, message, rank=0):
        if rank>1:
            print(f'\033[94m##\033[0m' + message)
            return
        self.count_headers[rank] = self.count_headers[rank] + 1
        section = str(self.count_headers[rank])
        if rank==1 : section = str(self.count_headers[0]) + "-" + str(self.count_headers[1])
        else: self.count_headers[1] = 0
        print(f'\n\033[94m== {section} ===============================================================================\033[0m')
        print(f'\033[94m## \033[0m' + message)
        print()

    def print_warn(self, *messages):
        print_head = '\033[93m'
        print_end = '\033[0m'
        full_message = ""
        for message in messages:
            full_message = full_message + " " + message
        print(print_head + "WARN" + print_end + full_message)

    def print_error(self, *messages):
        print_head = '\033[91m'
        print_end = '\033[0m'
        full_message = ""
        for message in messages:
            full_message = full_message + " " + message
        print(print_head + "ERROR" + print_end + full_message)

    def print_debug(self, *messages):
        print_head = '\033[36m'
        print_end = '\033[0m'
        full_message = ""
        for message in messages:
            full_message = full_message + " " + message
        print(print_head + "<><><" + print_end + full_message)

    def print_item_list(self, numbering, nm_main, item_title, lf_items):
        if len(lf_items) == 0:
            print(f"   No {item_title}s")
        elif numbering:
            for idx, line in enumerate(lf_items):
                if idx + 1 < 10:
                    idxalp = str(idx + 1)
                else:
                    idxalp = chr((idx - 10) + 97)
                if len(nm_main)>0 and line==nm_main:
                    print(f"   {idxalp}) {item_title}: {line} (main)")
                else:
                    print(f"   {idxalp}) {item_title}: {line}")
        else:
            for line in lf_items:
                if len(nm_main)>0 and line==nm_main:
                    print(f"   {item_title}: {line} (main)")
                else:
                    print(f"   {item_title}: {line}")

    def print_project_list(self, numbering=False):
        self.print_item_list(numbering, self.nm_main_project, "Project", self.lf_lilak_projects)

    def init_build(self):
        #self.print_header("Init build")
        self.GRU_DIR = "/usr/local/gru"
        self.GET_DIR = "/usr/local/get"
        self.NPTOOL_DIR = ""
        self.fn_build_option = os.path.join(os.path.join(self.lilak_path, "log"), "build_options.cmake")
        self.nm_main_project = "lilak"
        self.df_build_options0 = {
            #"ACTIVATE_EVE": False,
            "BUILD_MFM_CONVERTER": False,
            "BUILD_GEANT4_SIM": False,
            "BUILD_NPTOOL" : False,
            #"BUILD_JSONCPP": False,
            #"BUILD_DOXYGEN_DOC": False,
            #"CREATE_GIT_LOG": True,
        }
        self.df_build_options = self.df_build_options0.copy()
        self.lf_top_directories  = ["build", "data", "log", "macros", "source", "examples", ".git", "common", "doc"]
        self.lf_prj_subdir_link  = ["container", "detector", "tool", "task"]
        self.lf_prj_subdir_xlink = ["source"]
        self.lf_sub_package_dirs = ["geant4", "get", "fftw", "mfm", "nptool"]
        self.lf_exclude_name_classfactory = ["LKDetectorSystem", "LKFrameBuilder"]
        self.lf_exclude_path_classfactory = ["zzz", "temp", "figures", "data"]
        self.lf_include_path_classfactory1 = ["detector"]
        self.lf_include_path_classfactory2 = ["task"]
        self.lf_include_path_classfactory3 = []
        self.lf_include_path_classfactory = [["detector"],["task"]]
        self.lf_include_name_classfactory = ["DetectorConstruction", "DC"]
        self.lf_executable_path_candidate = ["macros","simulation"]
        self.lf_lilak_projects = []
        #self.lf_nptoollib_list = []
        self.lf_project_readme = []
        self.lf_cmake_projects = []
        self.lf_xmake_projects = []
        self.first_lilak_configuration = False

    def select_one_option(self, question="", possible_options=[]):
        if len(question)==0 and len(possible_options)==0:
            question = """   Q. Select from Below.
   - [Enter] Keep above options
   - [0]     Select new options
   - [r]     Keep above options but clean and recompile
   - [x]     Exit configuration
:"""
            possible_options = ['','0','x','q','r']
        while True:
            user_input = input(question)
            if user_input in ['x','q']: exit()
            if user_input in possible_options:
                return user_input
            else:
                print("Invalid input. Please try again.")
        return user_input

    def select_multiple_options(self, options, question="", prev_options=[]):
        too_many_options = False
        lf_option = list(options)
        lf_option.sort()
        if len(lf_option)>20:
            too_many_options = True
        if len(question)==0:
            if too_many_options:
                question = f"""   Q. Select options
   - [Enter] Skip
   - [index] Add project by indices ex) 1:2:3
   - [name]  Add project by names ex) {lf_option[0]}:{lf_option[1]}
   - [p]     Use previous options
   - [a]     Add all ex) a, a-3
   - [x]     Exit configuration
:"""
            else:
                if len(lf_option)>1:
                    question = f"""   Q. Select options
   - [Enter] Skip
   - [index] Add project by indices ex) 12c
   - [name]  Add project by names ex) {lf_option[0]}:{lf_option[1]}
   - [p]     Use previous options
   - [a]     Add all ex) a, a-3
   - [x]     Exit configuration
:"""
                else:
                    question = f"""   Q. Select options
   - [Enter] Skip
   - [index] Add project by indices ex) 1
   - [name]  Add project by names ex) lf_option[0]
   - [p]     Use previous options
   - [a]     Add all ex) a, a-3
   - [x]     Exit configuration
:"""
        idx_option = {}
        ##################################
        if too_many_options:
            for idx, key in enumerate(lf_option):
                idxalp = str(idx + 1)
                print(f"    {idxalp}) {key}")
                idx_option[idxalp] = key
        ##################################
        else:
            for idx, key in enumerate(lf_option):
                if idx < 9:
                    idxalp = str(idx + 1)
                elif idx < 21:
                    idxalp = chr((idx - 10) + 99)
                else:
                    continue
                print(f"    {idxalp}) {key}")
                idx_option[idxalp] = key
        print()
        user_options = input(question)
        print()
        print("Selected option(s):")
        print()
        user_options2 = user_options
        if too_many_options:
            user_options += ":"
        lf_final_keys = []
        ##################################
        if len(user_options)==0:
            print(f"    --")
        ##################################
        elif user_options2 in ['p','=']:
            lf_final_keys = prev_options
        ##################################
        elif user_options2 in ['x','q']:
            exit()
        elif user_options2 in ['a','*']:
            lf_final_keys = lf_option
            print(f"    All")
        ##################################
        elif user_options[0:2] in ["a-","*-"]:
            print(f"    All but")
            lf_final_keys = lf_option
            #if user_options[2:].isdecimal():
            for idxalp in user_options[2:]:
                if idxalp not in idx_option:
                    self.print_error(f"    Invalid input {idxalp}!")
                else:
                    key = idx_option[idxalp]
                    lf_final_keys.remove(key)
                    print(f"   -{idxalp}) {key}")
        ##################################
        elif ':' in user_options:
            lf_splits = user_options.split(':')
            if lf_splits[0]=='a':
                print(f"    All but")
                lf_final_keys = lf_option
                lf_splits = lf_splits[1:]
            for option_next in lf_splits:
                option_next = option_next.strip()
                if len(option_next)==0:
                    continue
                header = option_next[0]
                if header in ['-','+'] and len(option_next)>1:
                    option_next = option_next[1:]
                else:
                    header = '+'
                lf_found = self.find_with_expression(option_next, lf_option)
                if header=='+':
                    for key in lf_found:
                        lf_final_keys.append(key)
                        print(f"   + {key}")
                elif header=='-':
                    for key in lf_found:
                        if key in lf_final_keys:
                            lf_final_keys.remove(key)
                        print(f"   - {key}")
        ##################################
        else:
            for idxalp in user_options:
                if idxalp not in idx_option:
                    self.print_error(f"    Invalid input {idxalp}!")
                else:
                    key = idx_option[idxalp]
                    lf_final_keys.append(key)
                    print(f"    {idxalp}) {key}")
        ##################################
        return lf_final_keys

    def check_previous_configuration(self):
        self.first_lilak_configuration = False
        if os.path.exists(self.fn_build_option):
            with open(self.fn_build_option, "r") as f:
                lastOption = ""
                for line in f:
                    line = line.strip()
                    if line.find("set(") == 0:
                        tokens = line[4:].split()
                        if len(tokens) > 1:
                            if   tokens[0] == "LILAK_PROJECT_MAIN": self.nm_main_project = tokens[1]
                            elif tokens[0] == "GRU_DIR": self.GRU_DIR = tokens[1]
                            elif tokens[0] == "GET_DIR": self.GET_DIR = tokens[1]
                            elif tokens[0] == "NPTOOL_DIR": self.NPTOOL_DIR = tokens[1]
                            elif tokens[0] in ["LILAK_PROJECT_LIST", "LILAK_BIND_LIST"]:#, "NPTOOL_NPSLIB_LIST"]:
                                lastOption = tokens[0]
                            elif tokens[0] not in ["LILAK_PROJECT_LIST", "LILAK_BIND_LIST"]:#, "NPTOOL_NPSLIB_LIST"]:
                                self.df_build_options[tokens[0]] = (True if tokens[1] == "ON" else False)
                    elif len(line) > 0 and line != ")" and line.strip().find("CACHE INTERNAL") < 0 and line.strip().find("LILAK") != 0:
                        comment = ""
                        if line.find("#") > 0:
                            line, comment = line[:line.find("#")].strip(), line[line.find("#") + 1:].strip()
                        elif lastOption in ["LILAK_PROJECT_LIST", "LILAK_BIND_LIST"]:
                            self.lf_lilak_projects.append(line)
                        #elif lastOption in ["NPTOOL_NPSLIB_LIST"]:
                        #    self.lf_nptoollib_list.append(line)
        else:
            self.first_lilak_configuration = True

    def confirm_build_options(self):
        self.print_header("Confirm build options")
        if self.first_lilak_configuration:
            print("First LILAK configuration!")
            #return False
        print("Loading configuration from", self.fn_build_option)
        print()
        for key, value in self.df_build_options.items():
            print(f"   {key} = {value}")
        print()
        self.print_project_list()
        print()
        return self.select_one_option()

    def select_build_options(self):
        self.print_header("Build options")
        prev_build_options = [key for key, value in self.df_build_options.items() if value]
        self.df_build_options = self.df_build_options0.copy()
        lf_chosen_key = set(self.select_multiple_options(self.df_build_options, prev_options=prev_build_options))
        for key in lf_chosen_key:
            self.df_build_options[key] = True
            ###################################
            if key == "BUILD_MFM_CONVERTER":
                print()
                print(f"       GRU directory is")
                print(f"       {self.GRU_DIR}")
                user_input_gg = input(f"       Type path to changes directory, or else press <Enter>: ")
                if user_input_gg != '':
                    self.GRU_DIR = user_input_gg
                print()
                print(f"       GET directory is")
                print(f"       {self.GET_DIR}")
                user_input_gg = input(f"       Type path to changes directory, or else press <Enter>: ")
                if user_input_gg != '':
                    self.GET_DIR = user_input_gg
            ###################################

    def select_and_add_project(self):
        self.print_header("Add Project")
        print()
        user_input_project = "x"
        prev_lilak_projects = self.lf_lilak_projects
        self.lf_lilak_projects = []
        while len(user_input_project) > 0:
            ls_top = os.listdir("./")
            list_prj_directories = []
            for directory_name in ls_top:
                if directory_name in self.lf_top_directories:
                    continue
                if os.path.isdir(directory_name):
                    ls_directory = os.listdir(directory_name)
                    is_project_directory = False
                    for subdir in ls_directory:
                        if subdir in self.lf_prj_subdir_link:
                            is_project_directory = True
                            break
                        if subdir=="macros":
                            is_project_directory = True
                            break
                        if subdir==".lilak":
                            is_project_directory = True
                            break
                    if is_project_directory:
                        list_prj_directories.append(directory_name)
            lf_chosen_key = set(self.select_multiple_options(list_prj_directories, prev_options=prev_lilak_projects))
            for key in lf_chosen_key:
                self.lf_lilak_projects.append(key)
            break

    def select_main_project(self):
        if False:
            self.print_header("Select main project")
            print()
            self.nm_main_project = "lilak"
            if len(self.lf_lilak_projects) == 1:
                self.nm_main_project = self.lf_lilak_projects[0]
            self.print_project_list(True)
            print()
            while True:
                if len(self.lf_lilak_projects) == 0:
                    break
                input_main_project = input(f"Select index (or name) to set as main project. Type <Enter> to set main as {self.nm_main_project}: ")
                if input_main_project in self.lf_lilak_projects:
                    print(f"Main project is {self.nm_main_project}")
                    break
                elif input_main_project.isdigit() and int(input_main_project) <= len(self.lf_lilak_projects):
                    self.nm_main_project = self.lf_lilak_projects[int(input_main_project) - 1]
                    print(f"Main project is {self.nm_main_project}")
                    break
                elif len(input_main_project) == 0:
                    print(f"Main project is {self.nm_main_project}")
                    break
                else:
                    print(f"Project must be one in the list!")

    def create_cmakelists_for_each_project(self):
        change_in_cmakelists = False
        self.print_header("Creating CMakeLists.txt for projects",1)
        for project_name in self.lf_lilak_projects:
            project_path = os.path.join(self.lilak_path, project_name)
            project_comment = ""
            project_readme = ""
            project_readme_file_name = os.path.join(project_path, ".lilak")
            if os.path.exists(project_readme_file_name):
                with open(project_readme_file_name, "r") as f:
                    for line in f:
                        project_comment = line.strip()
                        break
            #print(f"-- {project_name} # {project_comment}")
            if len(project_comment)==0: project_comment = '-'
            project_readme = f'{project_name:18} {project_comment}'
            self.lf_project_readme.append('"'+project_readme+'"')
            ls_project = os.listdir(self.lilak_path+"/"+project_name)
            project_cmake_contents1 = ""
            if any(directory_name in self.lf_prj_subdir_link for directory_name in ls_project):
                project_cmake_contents1 += "set(LILAK_SOURCE_DIRECTORY_LIST ${LILAK_SOURCE_DIRECTORY_LIST}\n"
                project_cmake_contents1 += "".join("    ${CMAKE_CURRENT_SOURCE_DIR}/"+f"{directory_name}\n" for directory_name in ls_project if directory_name in self.lf_prj_subdir_link)
                project_cmake_contents1 += '    CACHE INTERNAL ""\n)\n\n'
            project_cmake_contents2 = ""
            if any(directory_name in self.lf_prj_subdir_xlink for directory_name in ls_project):
                project_cmake_contents2 += "set(LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF ${LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF}\n"
                project_cmake_contents2 += "".join("    ${CMAKE_CURRENT_SOURCE_DIR}/"+f"{directory_name}\n" for directory_name in ls_project if directory_name in self.lf_prj_subdir_xlink)
                project_cmake_contents2 += '    CACHE INTERNAL ""\n)\n\n'
            project_cmake_contents3 = ""
            for directory_name in ls_project:
                if directory_name in self.lf_sub_package_dirs:
                    upper_name = directory_name.upper()
                    if directory_name=="nptool": upper_name = "NPTOOL"
                    cmake_name3 = "LILAK_" + upper_name + "_SOURCE_DIRECTORY_LIST"
                    project_cmake_contents3 += f"set({cmake_name3} ${{{cmake_name3}}}\n"
                    project_cmake_contents3 += "    ${CMAKE_CURRENT_SOURCE_DIR}/" + directory_name + "\n"
                    project_cmake_contents3 += '    CACHE INTERNAL ""\n)\n\n'
            project_cmake_contents4 = ""
            lf_compile_required_files = []
            if self.df_build_options["BUILD_GEANT4_SIM"]==True:
                lf_executable_path = []
                ls_top = os.listdir(project_path)
                for directory_name in ls_top:
                    for candidate_path in self.lf_executable_path_candidate:
                        if directory_name.startswith(candidate_path):
                            lf_executable_path.append(directory_name)
                for directory_name in lf_executable_path:
                    ########################################################
                    project_macro_path = f'{self.lilak_path}/{project_name}/{directory_name}/'
                    if os.path.exists(project_macro_path):
                        lf_temp_files = [f for f in os.listdir(project_macro_path) if f.endswith(".cc")]
                        for compile_required_file in lf_temp_files:
                            lf_compile_required_files.append(f'{directory_name}/{compile_required_file}')
                    ########################################################
                if lf_compile_required_files:
                    jl_compile_required_files = ''
                    for compile_required_file in lf_compile_required_files:
                        jl_compile_required_files += '    ${CMAKE_CURRENT_SOURCE_DIR}/'+compile_required_file+'\n'
                    project_cmake_contents4 += f"""set(LILAK_EXECUTABLE_LIST ${{LILAK_EXECUTABLE_LIST}}
{jl_compile_required_files}    CACHE INTERNAL ""
)"""
            project_cmake_contents = project_cmake_contents1 + project_cmake_contents2 + project_cmake_contents3 + project_cmake_contents4
            project_cmake_file_name = os.path.join(project_path, "CMakeLists.txt")
            current_contents = ""
            update_project_cmake_lists = False
            remove_project_cmake_lists = False
            ####################################
            if os.path.exists(project_cmake_file_name):
                with open(project_cmake_file_name, "r") as f:
                    current_contents = f.read()
                if len(current_contents)==0 or len(project_cmake_contents)==0:
                    print(f"-- {project_name:10} : Removing {project_cmake_file_name}")
                    remove_project_cmake_lists = True
                elif current_contents==project_cmake_contents:
                    print(f"-- {project_name:10} : No changes in {project_cmake_file_name}")
                else:
                    print(f"-- {project_name:10} : Updating {project_cmake_file_name}")
                    update_project_cmake_lists = True
            else:
                if len(project_cmake_contents)>0:
                    print(f"-- {project_name:10} : Creating {project_cmake_file_name}")
                    update_project_cmake_lists = True
                else:
                    print(f"-- {project_name:10} : Not a cmake project")
            if lf_compile_required_files:
                title = "executable"
                for compile_required_file in lf_compile_required_files:
                    #print(f"   {title} {compile_required_file}")
                    print(f"   > {compile_required_file}")
            ####################################
            # Write the new contents to the file
            if remove_project_cmake_lists:
                print(f"Removing {project_cmake_file_name}")
                os.remove(project_cmake_file_name)
                change_in_cmakelists = True
            elif update_project_cmake_lists:
                print(f"Updating {project_cmake_file_name}")
                with open(project_cmake_file_name, "w") as f:
                    f.write(project_cmake_contents)
                change_in_cmakelists = True
            ####################################
            # add to list
            if len(project_cmake_contents)>0:
                self.lf_cmake_projects.append(project_name)
            else:
                self.lf_xmake_projects.append(project_name)
        return change_in_cmakelists

    def create_build_option_file(self):
        change_in_build_option = False
        self.print_header("Creating build option file",1)
        current_contents = ""
        new_contents = ""
        for key, value in self.df_build_options.items():
            vonoff = "ON" if value == 1 else "OFF"
            new_contents += f"set({key} {vonoff} CACHE INTERNAL \"\")\n"
        jl_cmake_projects = '\n'.join(self.lf_cmake_projects)
        jl_xmake_projects = '\n'.join(self.lf_xmake_projects)
        new_contents +=  "\nset(LILAK_PROJECT_LIST ${LILAK_PROJECT_LIST}"
        new_contents += f"\n{jl_cmake_projects}"
        new_contents +=  '\nCACHE INTERNAL ""\n)' #####
        new_contents += f'\n\nset(GRU_DIR {self.GRU_DIR} CACHE INTERNAL "")' #####
        new_contents += f'\nset(GET_DIR {self.GET_DIR} CACHE INTERNAL "")' #####
        new_contents +=  "\nset(LILAK_BIND_LIST ${LILAK_BIND_LIST}" #####
        new_contents += f"\n{jl_xmake_projects}"
        new_contents +=  '\nCACHE INTERNAL ""\n)'
        new_contents += f'\n\nset(NPTOOL_DIR {self.NPTOOL_DIR} CACHE INTERNAL "")' #####
        if self.nm_main_project != "lilak":
            new_contents += f'\n\nset(LILAK_PROJECT_MAIN {self.nm_main_project} CACHE INTERNAL "")'
        if os.path.exists(self.fn_build_option):
            with open(self.fn_build_option, "r") as f:
                current_contents = f.read()
                current_contents = current_contents.strip()
            if current_contents==new_contents:
                #print(f"No changes in {self.fn_build_option}")
                print(f"{self.fn_build_option} # No changes")
            else:
                print(f"Updating {self.fn_build_option}")
                with open(self.fn_build_option, "w") as f:
                    print("******************\n")
                    print(new_contents)
                    f.write(new_contents)
                    change_in_build_option = True
        else:
            print(f"Creating {self.fn_build_option}")
            with open(self.fn_build_option, "w") as f:
                print("******************\n")
                print(new_contents)
                f.write(new_contents)
                change_in_build_option = True
        return change_in_build_option

    def copy_common_files_from_project(self):
        self.print_header("Copy common files from the projects",1)
        top_common_dir = f"{self.lilak_path}/common/"
        for item in os.listdir(top_common_dir):
            item_path = os.path.join(top_common_dir, item)
            if os.path.islink(item_path):
                os.remove(item_path)
        for project_name in self.lf_lilak_projects:
            project_path = os.path.join(self.lilak_path, project_name)
            ls_directory = os.listdir(project_path)
            if "common" in ls_directory:
                proj_common_dir = os.path.join(project_path,"common")
                ls_proj_common = os.listdir(proj_common_dir)
                lf_link_files = sorted(os.listdir(proj_common_dir))
                if '.common' in lf_link_files:
                    lf_link_files.remove('.common')
                print(f"-- {proj_common_dir} ({len(lf_link_files)})")
                for file1 in lf_link_files:
                    src_file = os.path.join(proj_common_dir, file1)
                    dest_link = os.path.join(top_common_dir, file1)
                    if os.path.exists(dest_link) or os.path.islink(dest_link):
                        self.print_warn(f"link already exists: {file1}")
                        continue
                    else:
                        os.symlink(src_file, dest_link)

    def scan_directories(self, base_path, directories):
        """
        Function to scan directories and collect class names
        Scan the directories in self.lf_lilak_projects
        """
        lf_classes = []
        for directory in directories:
            dir_path = os.path.join(base_path, directory)
            for root, lf_dirs, lf_files in os.walk(dir_path):
                for file_name in lf_files:
                    if file_name.endswith(".h"):
                        class_name = file_name[:-2]  # Remove the '.h' extension to get the class name
                        source_file = f"{class_name}.cpp"
                        # Check if the corresponding source file_name exists in the same directory
                        if source_file in lf_files:
                            relative_path = os.path.relpath(root, self.lilak_path)
                            # Add the class name and its relative path to the list
                            lf_classes.append((class_name, relative_path))
                            relative_path = os.path.relpath(root, self.lilak_path)
        return lf_classes

    def g4dc_classfactory(self):
        change_in_class_factory = False
        for mode in [0,1]: # 0:geant4 1:nptool
            jl_source_main = ""
            jl_include_headers = ""
            jl_source_print = ""
            lf_search_path = []
            lf_include_directory = []
            factory_name = ""
            factory_path = ""
            if mode==0: lf_include_directory = ["geant4"]
            if mode==1: lf_include_directory = ["geant4","nptool"]
            if mode==0: factory_name = "LKG4DetectorConstructionFactory"
            if mode==1: factory_name = "LKNPDetectorConstructionFactory"
            if mode==0: factory_path = "source/geant4"
            if mode==1: factory_path = "source/nptool"
            for project in self.lf_lilak_projects:
                for include_directory in lf_include_directory:
                    include_path_full = project + "/" + include_directory
                    lf_search_path.append(include_path_full)
            for include_directory in lf_include_directory:
                lf_search_path.append("source/"+include_directory)
            lf_classes = self.scan_directories(self.lilak_path, lf_search_path)
            lf_classes.sort()
            last_header = ''
            for class_name, path_name in lf_classes:
                candidate_file_name = self.lilak_path + "/" + path_name + "/" + class_name + ".h"
                candidate_file_path = os.path.join(self.lilak_path, candidate_file_name)
                is_detector_construction = True
                with open(candidate_file_path, "r") as candidate_file:
                    contents_of_file = candidate_file.read()
                    if mode==0: is_detector_construction = (contents_of_file.find('public G4VUserDetectorConstruction')>0)
                    if mode==1: is_detector_construction = (contents_of_file.find('public DetectorConstruction')>0 or contents_of_file.find('public G4VUserDetectorConstruction')>0)
                if not is_detector_construction:
                    continue
                if not jl_source_main:
                    jl_source_main  = f'    if      (name=="{class_name}") {{ e_info << "Found {class_name}" << endl; return (new {class_name}); }} // {path_name}\n'
                else:
                    jl_source_main += f'    else if (name=="{class_name}") {{ e_info << "Found {class_name}" << endl; return (new {class_name}); }} // {path_name}\n'
                jl_source_print    += f'    e_cout << "{class_name}" << endl;\n'
                jl_include_headers += f'#include "{class_name}.h"\n'
                last_header = class_name[:2]
            jl_source_main += f'\n'
            jl_source_main += f'    else {{ e_error << "Class " << name << " is not in the factory!" << endl; }}\n'
            jl_source_main += f'    return ((G4VUserDetectorConstruction*) nullptr);\n'
            # Create the source file content
            source_content = f"""#include "{factory_name}.h"
#include "LKRun.h"\n
{jl_include_headers}
void {factory_name}::Print()
{{
{jl_source_print}}}\n
G4VUserDetectorConstruction* {factory_name}::GetDetectorConstruction(TString name)
{{
{jl_source_main}}}"""
            # Create the header file content
            header_content = f"""#ifndef {factory_name.upper()}_HH
#define {factory_name.upper()}_HH\n
#include "TString.h"
#include "LKLogger.h"
#include "G4VUserDetectorConstruction.hh"\n
class {factory_name}
{{
    public:
        {factory_name}() {{}}
        virtual ~{factory_name}() {{}}\n
        void Print();
        G4VUserDetectorConstruction* GetDetectorConstruction(TString name);
}};\n
#endif"""
            if (self.check_content_and_recreate_file(f"{factory_path}/{factory_name}.cpp", source_content)): change_in_class_factory = True 
            if (self.check_content_and_recreate_file(f"{factory_path}/{factory_name}.h",   header_content)): change_in_class_factory = True 
        return change_in_class_factory

    def create_classfactory(self):
        change_in_class_factory = False
        jl_source_main = ""
        jl_source_main += f'    if (fRun==nullptr)\n'
        jl_source_main += f'        return;\n\n'
        jl_init_class_lines1 = ""
        jl_init_class_lines2 = ""
        jl_init_class_lines3 = ""
        jl_init_class_lines4 = ""
        jl_include_headers = ""
        jl_source_print = ""
        lf_search_path = []
        for project in self.lf_lilak_projects:
            lf_search_path.append(project)
        lf_search_path.append("source")
        lf_classes = self.scan_directories(self.lilak_path, lf_search_path)
        lf_classes.sort()
        last_header = ''
        lf_group_names = []
        if self.df_build_options["BUILD_MFM_CONVERTER"]==True:
            self.lf_include_path_classfactory3.append(['mfm'])
        for class_name, path_name in lf_classes:
            if any(keyword in path_name for keyword in self.lf_exclude_path_classfactory):
                continue
            if any(keyword in class_name for keyword in self.lf_exclude_name_classfactory):
                continue
            for i in [1,2,3]:
                if i==1: include_path = self.lf_include_path_classfactory1
                if i==2: include_path = self.lf_include_path_classfactory2
                if i==3: include_path = self.lf_include_path_classfactory3
                if any(keyword in path_name for keyword in include_path):
                    if not jl_source_main:
                        jl_source_main += f'    if      (name=="{class_name}") {{ e_info << "Adding {class_name}" << endl; fRun -> Add(new {class_name}); }} // {path_name}\n'
                    else:
                        #if last_header!=class_name[:2]: jl_source_main += f'\n'
                        jl_source_main += f'    else if (name=="{class_name}") {{ e_info << "Adding {class_name}" << endl; fRun -> Add(new {class_name}); }} // {path_name}\n'
                    jl_source_print    += f'    e_cout << "{class_name}" << endl;\n'
                    jl_include_headers += f'#include "{class_name}.h"\n'
                    last_header = class_name[:2]
                    if True:
                        i_group = path_name.find('/')
                        group_name = path_name[:i_group]
                        if group_name not in lf_group_names:
                            lf_group_names.append(group_name)
                            jl_init_class_lines1 += f'     fLD.push_back("{group_name}"); // {group_name}\n'
                            jl_init_class_lines2 += f'     std::vector<TString> lf_classes_in_{group_name}; // {group_name}\n'
                            jl_init_class_lines4 += f'     fLL.push_back(lf_classes_in_{group_name}); // {group_name}\n'
                        jl_init_class_lines3 += f'     lf_classes_in_{group_name}.push_back("{class_name}");// {path_name}\n'
        jl_source_main += f'\n'
        jl_source_main += f'    else {{ e_warning << "Class " << name << " is not in the class factory!" << endl; }}\n'
        # Create the source file content
        source_content = f"""#include "LKClassFactory.h"
#include "LKRun.h"\n
{jl_include_headers}
void LKClassFactory::Print()
{{
{jl_source_print}}}\n
void LKClassFactory::Init()
{{
{jl_init_class_lines1}
{jl_init_class_lines2}
{jl_init_class_lines3}
{jl_init_class_lines4}}}\n
void LKClassFactory::Add(TString name)
{{
{jl_source_main}}}"""
        # Create the header file content
        header_content = """#ifndef LKCLASSFACTORY_HH
#define LKCLASSFACTORY_HH\n
#include "TString.h"
#include "LKLogger.h"\n
class LKRun;\n
class LKClassFactory
{
    private:
        LKRun* fRun = nullptr;
        std::vector<TString> fLD; // list of directories
        std::vector<std::vector<TString>> fLL; // list of "list of classes" for each directory\n
    public:
        LKClassFactory() { Init(); }
        LKClassFactory(LKRun* run) { fRun = run; Init(); }
        virtual ~LKClassFactory() {}\n
        void Print();
        void Init();
        void Add(TString name);\n
        std::vector<TString> GetListOfDirectories() { return fLD; }
        std::vector<std::vector<TString>> GetListOfLD() { return fLL; }
};\n
#endif"""
        if (self.check_content_and_recreate_file("source/base/LKClassFactory.cpp", source_content)): change_in_class_factory = True 
        if (self.check_content_and_recreate_file("source/base/LKClassFactory.h",   header_content)): change_in_class_factory = True 
        #update_source = False
        #update_header = False
        #source_file_path = os.path.join(self.lilak_path, "source/base/LKClassFactory.cpp")
        #header_file_path = os.path.join(self.lilak_path, "source/base/LKClassFactory.h")
        #exist_source = os.path.exists(source_file_path)
        #exist_header = os.path.exists(header_file_path)
        #update_source = (exist_source==False)
        #update_header = (exist_header==False)
        #if exist_source:
        #    with open(source_file_path, "r") as source_file: update_source = (source_file.read().strip() != source_content)
        #if exist_header:
        #    with open(header_file_path, "r") as header_file: update_header = (header_file.read().strip() != header_content)
        #if update_source:
        #    print("Updating", source_file_path)
        #    with open(source_file_path, "w") as source_file: print(source_content, file=source_file)
        #    change_in_class_factory = True
        #else:
        #    print("No changes in", source_file_path)
        #if update_header:
        #    print("Updating", header_file_path)
        #    with open(header_file_path, "w") as header_file: print(header_content, file=header_file)
        #    change_in_class_factory = True
        #else:
        #    print("No changes in", header_file_path)
        return change_in_class_factory

    def check_content_and_recreate_file(self, source_name, source_content):
        source_file_path = os.path.join(self.lilak_path, source_name)
        exist_source = os.path.exists(source_file_path)
        update_source = (exist_source==False)
        if exist_source:
            with open(source_file_path, "r") as source_file:
                update_source = (source_file.read().strip() != source_content)
        if update_source:
            print(source_file_path, "# Updated")
            with open(source_file_path, "w") as source_file:
                print(source_content, file=source_file)
            change_in_class_factory = True
        else:
            print(source_file_path, "# No changes")
        return update_source

    def create_lilak_class_reference(self):
        return False
        pass

    def build_lilak(self, run_cmake):
        self.print_header("Building lilak")
        if (os.getenv('LILAK_PATH') == self.lilak_path) == False:
            os.environ["LILAK_PATH"] = self.lilak_path
        original_directory = os.getcwd()
        if not os.path.exists(f'{self.lilak_path}/build'):
            print(f'mkdir {self.lilak_path}/build')
            os.makedirs(f'{self.lilak_path}/build')
            run_cmake = True
        print(f'{self.lilak_path}/build')
        os.chdir(f'{self.lilak_path}/build')
        if run_cmake:
            print('cmake ..')
            os.system('cmake ..')
        print(f'make -j{self.args.jobs}')
        os.system(f'make -j{self.args.jobs}')
        os.chdir(f'{original_directory}')

    def create_lilak_command_script(self):
        lilak_command_sh_content = f"""#!/bin/bash

# Add the lilak directory to the PATH
export LILAK_PATH="{self.lilak_path}"
export PATH="$LILAK_PATH:$PATH"

# Unset any previous definition of the lilak function if it exists
if declare -f lilak > /dev/null; then
    unset -f lilak
fi

# Define a function to update git repositories
update_git_repos() {{
    local original_dir=$(pwd)
    local dirs_to_update=("{self.lilak_path}" {" ".join([os.path.join(self.lilak_path, project) for project in self.lf_lilak_projects])})
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
        echo
        echo "LILAK (https://github.com/lilak-project)"
        echo
        echo "Usage: lilak {{home|config_test|build|build_new|new|update|example|doc|find|geant4|run}} [input]"
        echo
        echo "Commands:"
        echo "  home               Navigate to the lilak home directory."
        echo "  config_test        Configuration test without make"
        echo "  build [input]      Build LILAK with [input]; parallel jobs flag. ex) lilak build -j10"
        echo "  build_new          Clean build-directory and rebuild the package (asks for confirmation)."
        echo "  new                Run script to create new project"
        echo "  update             Check the current branch and pull updates for lilak and project directories."
        echo "  example            Navigate to the lilak examples directory."
        echo "  doc [input]        Print the reference link to the class documentation."
        echo "  find [input]       Find and navigate to the directory containing [input]."
        echo "  g4sim [input]      Execute the default Geant4 simulatoin program with the provided [input]."
        echo "  nptool [input]     Execute the default nptool simulatoin program with the provided [input]."
        echo "  run [input]        Execute the ROOT script with the provided [input]."
        echo
        echo "Project commands:"
        for project_readme in {" ".join(self.lf_project_readme)}; do
            printf "  %s\n" "$project_readme"
        done
        return
    fi

    case $1 in
        home)
            cd "$LILAK_PATH" || echo "Directory not found: $LILAK_PATH"
            echo "Changed directory to: $(pwd)"
            ;;
        config_test)
            local original_dir=$(pwd)
            cd "$LILAK_PATH"
            "$LILAK_PATH/lilak.sh" -t
            export LD_LIBRARY_PATH="$LILAK_PATH/build:$LD_LIBRARY_PATH"
            cd "$original_dir"
            ;;
        build)
            local original_dir=$(pwd)
            cd "$LILAK_PATH"
            if [[ "$2" =~ ^[0-9]+$ ]]; then
                "$LILAK_PATH/lilak.sh" -j"$2"
            elif [ -z "$2" ]; then
                "$LILAK_PATH/lilak.sh"
            else
                "$LILAK_PATH/lilak.sh" -j"$2"
            fi
            export LD_LIBRARY_PATH="$LILAK_PATH/build:$LD_LIBRARY_PATH"
            cd "$original_dir"
            ;;
        make)
            local original_dir=$(pwd)
            cd "$LILAK_PATH"
            if [ -z "$2" ]; then
                "$LILAK_PATH/lilak.sh"
            else
                "$LILAK_PATH/lilak.sh" -j"$2"
            fi
            export LD_LIBRARY_PATH="$LILAK_PATH/build:$LD_LIBRARY_PATH"
            cd "$original_dir"
            ;;
        build_new)
            echo -n "Are you sure you want to clean the build directory? (y/n): "
            read confirm
            if [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]]; then
                local original_dir=$(pwd)
                rm -rf "$LILAK_PATH/build"
                mkdir -p "$LILAK_PATH/build"
                cd "$LILAK_PATH"
                if [ -z "$2" ]; then
                    "$LILAK_PATH/lilak.sh"
                else
                    "$LILAK_PATH/lilak.sh" -j"$2"
                fi
                export LD_LIBRARY_PATH="$LILAK_PATH/build:$LD_LIBRARY_PATH"
                cd "$original_dir"
            else
                echo "Clean build canceled."
            fi
            ;;
        new)
            local original_dir=$(pwd)
            cd "$LILAK_PATH"
            "$LILAK_PATH/macros/project_class_creator.py"
            cd "$original_dir"
            ;;
        update)
            update_git_repos
            ;;
        example)
            cd "$LILAK_PATH/examples" || echo "Directory not found: $LILAK_PATH/examples"
            echo "Changed directory to: $(pwd)"
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
                file=$(find "$LILAK_PATH" \
                        -not -path "$LILAK_PATH/build/*" \
                        -not -path "$LILAK_PATH/doc/*" \
                        -not -path "$LILAK_PATH/temp/*" \
                        -not -path "$LILAK_PATH/zzz/*" \
                        -name "*$2*" -print -quit)
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
        g4sim)
            if [ -z "$2" ]; then
                {self.lilak_path}/macros/geant4_simulation.exe
            else
                {self.lilak_path}/macros/geant4_simulation.exe "$2"
            fi
            ;;
        nptool)
            if [ -z "$2" ]; then
                {self.lilak_path}/macros/nptool_simulation.exe
            else
                {self.lilak_path}/macros/nptool_simulation.exe "$2"
            fi
            ;;
        run)
            if [ -z "$2" ]; then
                root -l '{self.lilak_path}/macros/run_lilak.C()'
            else
                root -l '{self.lilak_path}/macros/run_lilak.C("'"$2"'")'
            fi
            ;;
        *)
            if [[ " {" ".join(self.lf_lilak_projects)} " =~ " $1 " ]]; then
                target_project_dir="$LILAK_PATH/$1/"
                if [ -n "$2" ]; then
                    target_subdir="$target_project_dir/$2"
                    if [ -d "$target_subdir" ]; then
                        cd "$target_subdir"
                        echo "Changed directory to: $(pwd)"
                    else
                        echo "Subdirectory not found: $target_subdir"
                    fi
                elif [ -d "$target_project_dir/" ]; then
                    cd "$target_project_dir/"
                    echo "Changed directory to: $(pwd)"
                else
                    cd "$target_project_dir"
                    echo "Changed directory to: $(pwd)"
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
    local commands="home config_test build build_new new update example doc find g4sim nptool run {" ".join(self.lf_lilak_projects)}"
    local subdir_projects="{" ".join(self.lf_lilak_projects)}"

    if [[ ${{COMP_CWORD}} == 1 ]]; then
        COMPREPLY=( $(compgen -W "${{commands}}" -- "${{curr_word}}") )
    elif [[ $prev_word == "g4sim" ]]; then
        COMPREPLY=( $(compgen -f "${{curr_word}}") )
    elif [[ $prev_word == "nptool" ]]; then
        COMPREPLY=( $(compgen -f "${{curr_word}}") )
    elif [[ $prev_word == "run" ]]; then
        COMPREPLY=( $(compgen -f "${{curr_word}}") )
    elif [[ ${{COMP_CWORD}} == 2 && " $subdir_projects " =~ " ${{COMP_WORDS[1]}} " ]]; then
        local project_path="$LILAK_PATH/${{COMP_WORDS[1]}}"
        if [ -d "$project_path" ]; then
            local subdirs=$(find "$project_path" -maxdepth 1 -mindepth 1 -type d -exec basename {{}} \;)
            COMPREPLY=( $(compgen -W "${{subdirs}}" -- "${{curr_word}}") )
        fi
    fi

    return 0
}}

# Enable command completion for lilak
complete -F _lilak_completions lilak

# Optional: Add a message to confirm the script is sourced correctly
echo "LILAK is set. Use 'lilak {{home|config_test|build|build_new|new|update|example|doc|find|g4sim|nptool|run}}'"
"""
        fn_lilak_command_sh = os.path.join(self.lilak_path, "macros/command_lilak.sh")
        with open(fn_lilak_command_sh, "w") as lilak_command_sh_file:
            lilak_command_sh_file.write(lilak_command_sh_content)
        os.chmod(fn_lilak_command_sh, 0o755)

    def summary(self):
        #print_out_source_lilak = False
        print_out_source_lilak = True
        print_out_login_script = True
        print_out_create_rootrc = False
        print_out_root_logon = True
        pt_home = os.environ['HOME']
        lf_shell_info = [
            ['bash','.bash_profile','.bashrc'],
            ['zsh' ,'.zprofile'    ,'.zshrc'],
            ['ksh' ,'.profile'     ,'.kshrc'],
            ['csh' ,'.login'       ,'.cshrc'],
            ['tcsh','.login,'      ,'.tcshrc']
        ]
        current_shell = 'bash'
        if "SHELL" in os.environ:
            #current_shell = os.environ['SHELL'];
            current_shell = os.environ.get('SHELL', '/bin/sh')
        else:
            self.print_warn("SHELL do not exist in os.environ! Assumming bash shell ...")
        shell_script = ''
        for shell_info in lf_shell_info:
            if shell_info[0] in current_shell:
                shell_script = shell_info[2] 
                break
        source_lilak_sh = f'source {self.lilak_path}/lilak.sh'
        if len(shell_script)>0:
            shell_script = os.path.join(pt_home,shell_script)
            if os.path.exists(shell_script):
                with open(shell_script, "r") as f:
                    for line in f:
                        line = line.strip()
                        if source_lilak_sh==line:
                            print_out_login_script=False
                            break
        else:
            shell_script = 'your login script'
        fn_rootrc = os.path.join(pt_home,".rootrc")
        if os.path.exists(fn_rootrc):
            with open(fn_rootrc, "r") as f:
                for line in f:
                    line = line.strip()
                    if (f'Rint.Logon: {self.lilak_path}/macros/rootlogon.C' in line) and line[0]!='#':
                        print_out_root_logon=False
                        break
        else:
            print_out_create_rootrc = True
        self.print_header("Summary")
        if print_out_source_lilak==False and print_out_login_script==False and print_out_create_rootrc==False and print_out_root_logon==False and print_out_source_lilak==False:
            print("Good!\n")
        else:
            if print_out_source_lilak:
                print(f"""
== Set up LILAK environment
{source_lilak_sh}""")
            if print_out_login_script:
                print(f"""
== Add following to {shell_script}
{source_lilak_sh}""")
            if print_out_create_rootrc:
                print("You don't have rootrc!")
                print(f"""
== Create .rootrc and set logon script
echo Rint.Logon: {self.lilak_path}/macros/rootlogon.C >> {fn_rootrc}""")
            elif print_out_root_logon:
#                print(f"""
#== Add following to .rootrc at $HOME
#Rint.Logon: {self.lilak_path}/macros/rootlogon.C
#""")
                print(f"""
== Add following to {os.getenv('HOME')}/.rootrc
Rint.Logon: {self.lilak_path}/macros/rootlogon.C
""")

if __name__ == "__main__":
    conf = lilak_configuration()
    conf.run_configuration()
