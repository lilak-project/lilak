#!/usr/bin/env python3

import os
import sys

current_path = os.path.dirname(os.path.abspath(__file__))
lilak_path = os.getenv('LILAK_PATH')
if lilak_path is None:
    lilak_path = current_path

sys.path.append(lilak_path+'/macros/dummy_class_writter/')
from lilakcc import lilakcc

def print_c(message):
    print_head = '\033[94m'
    print_end = '\033[0m'
    print(print_head+message+print_end)

def print_l(messeage,i=0):
    if i==0:
        print("  ",message)
    if i>0:
        print("  ",("  "*i)+message)

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

list_top_directories = ["build","data","log","macros","source"]
list_proj_subdir = ["macros","container","detector","common","tool","task","geant4"]
list_creator_subdir = ["container","detector","tool","task","geant4"]

ls_top = os.listdir("./")
list_prj_directories = []
for directory_name in ls_top:
    if directory_name in list_top_directories:
        continue
    if (os.path.isdir(directory_name)):
        list_prj_directories.append(directory_name)

print_h("Creating new project");

project_name = ""
num_arg = len(sys.argv)
if num_arg>=2:
    project_name = sys.argv[1]

while True:
    print()
    if project_name in list_top_directories:
        print("   Project name cannot be",list_top_directories)
        project_name = ""

    if project_name in list_prj_directories:
        print(f'   Project already exist with name "{project_name}"')
        project_name = ""

    if len(project_name)>0:
        break
    else:
        user_input = ""
        while len(user_input)==0:
            user_input = input("   Enter project name: ")
            project_name = user_input

print("   Project name is", project_name)

print_h("Creating project", project_name)
print()

project_path = os.path.join(current_path,project_name)
print("   LILAK   path is ",lilak_path)
print("   Current path is ",current_path)
print("   Project path is ",project_path)
print()

os.system(f'mkdir -p {project_path}')
os.system(f'cp {lilak_path}/.gitignore {project_path}/')

print_h("Creating sub-directories")

print()
print(f"   You can create dummy classes using scripts in macros/dummy_class_writer/")

#list_commands = []
#list_commands.append(os.chdir(project_path))
#for subdir_name in list_proj_subdir:
#    print_h(f'Creating {subdir_name}')
#    subdir_path = os.path.join(project_path,subdir_name)
#    list_commands.append(os.system(f'mkdir -p {subdir_path}'))
#    list_commands.append(os.chdir(subdir_path))
#    list_commands.append(os.system(f'touch .{subdir_name}'))
#    if subdir_name=="macros":
#        list_commands.append(os.system(f'mkdir -p data'))
#        list_commands.append(os.chdir('data'))
#        list_commands.append(os.system(f'touch .data'))
#    elif subdir_name in list_creator_subdir:
#        while True:
#            if subdir_name=="geant4":
#                class_name = input(f"   Enter class name to create Geant4 Detector-Construction class <[name]/Enter>: ")
#            else:
#                class_name = input(f"   Enter class name to create {subdir_name} class <[name]/Enter>: ")
#            if class_name=='':
#                break
#            if class_name!='':
#                content = f"+class {class_name}\n+path {subdir_path}"
#                content_dp = f"+class {class_name}Plane\n+path {subdir_path}"
#                if subdir_name=="container":    list_commands.append(lilakcc(content).print_container())
#                if subdir_name=="geant4":       list_commands.append(lilakcc(content).print_geant4dc())
#                if subdir_name=="task":         list_commands.append(lilakcc(content).print_task())
#                if subdir_name=="tool":         list_commands.append(lilakcc(content).print_tool())
#                if subdir_name=="detector":
#                    list_commands.append(lilakcc(content).print_detector())
#                    list_commands.append(lilakcc(content_dp).print_detector_plane())

os.chdir(project_path)
for subdir_name in list_proj_subdir:
    print_h(f'Creating {subdir_name}')
    subdir_path = os.path.join(project_path,subdir_name)
    os.system(f'mkdir -p {subdir_path}')
    os.chdir(subdir_path)
    os.system(f'touch .{subdir_name}')

    if subdir_name=="macros":
        os.system(f'mkdir -p data')
        os.chdir('data')
        os.system(f'touch .data')
    elif subdir_name in list_creator_subdir:
        while True:
            if subdir_name=="geant4":
                class_name = input(f"   Enter class name to create Geant4 Detector-Construction class <[name]/Enter>: ")
            else:
                class_name = input(f"   Enter class name to create {subdir_name} class <[name]/Enter>: ")
            if class_name=='':
                break
            if class_name!='':
                content = f"+class {class_name}\n+path {subdir_path}"
                content_dp = f"+class {class_name}Plane\n+path {subdir_path}"
                if subdir_name=="container": lilakcc(content).print_container()
                if subdir_name=="geant4": lilakcc(content).print_geant4dc()
                if subdir_name=="task": lilakcc(content).print_task()
                if subdir_name=="tool": lilakcc(content).print_tool()
                if subdir_name=="detector":
                    lilakcc(content).print_detector()
                    lilakcc(content_dp).print_detector_plane()

print_h("What now?")
print()
print("   It's all set! Now you might want to")
print()
print("   1. Setup git for the project")
print("   2. Create and write up the classes.")
print("   3. Compile the project, by running configure.py")
print("   4. See https://github.com/lilak-project/lilak/wiki for more information")
print()
