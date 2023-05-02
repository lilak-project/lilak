# CMakeLists.txt LILAK project.
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(LILAK)

#---Configuration
set(ACTIVATE_EVE       OFF CACHE INTERNAL "") # Activate Root Eve package
set(CREATE_GIT_LOG     OFF CACHE INTERNAL "") # Create Git log
set(BUILD_GEANT4_SIM   OFF CACHE INTERNAL "") # Build Geant4 simulation
set(BUILD_DOXYGEN_DOC  OFF CACHE INTERNAL "") # Build Doxygen documentation with HTML
set(LILAK_PROJECT_LIST     CACHE INTERNAL "") # List of LILAK projects to build
set(LILAK_PROJECT_MAIN     CACHE INTERNAL "")

#---Configuration
include(log/build_options.cmake)

if(ACTIVATE_EVE)
    add_definitions(-DACTIVATE_EVE)
endif(ACTIVATE_EVE)

message(STATUS "Activate Eve package   : " ${ACTIVATE_EVE})
message(STATUS "Create Git log         : " ${CREATE_GIT_LOG})
message(STATUS "Build Geant4 simulation: " ${BUILD_GEANT4_SIM})
message(STATUS "Build Doxygen document : " ${BUILD_DOXYGEN_DOC})
message(STATUS "Main Project           : " ${LILAK_PROJECT_MAIN})

#---Add source directories. Directory should be under "/path/to/LILAK/"
set(LILAK_SOURCE_DIRECTORY_LIST
    ${CMAKE_SOURCE_DIR}/source/base
    ${CMAKE_SOURCE_DIR}/source/tool
    ${CMAKE_SOURCE_DIR}/source/geometry
    ${CMAKE_SOURCE_DIR}/source/detector
    ${CMAKE_SOURCE_DIR}/source/container
    ${CMAKE_SOURCE_DIR}/source/container/track
    CACHE INTERNAL ""
)

#---Add source directories without LinkDef.h. Directory should be under "/path/to/LILAK/".
set(LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF
    ${CMAKE_SOURCE_DIR}/source/jsoncpp
    CACHE INTERNAL ""
)

#---Add geant4 source directories. Directory should be under "/path/to/LILAK/".
set(LILAK_GEANT4_SOURCE_DIRECDTORY_LIST
    ${CMAKE_SOURCE_DIR}/source/geant4
    CACHE INTERNAL ""
)

#---Add geant4 main programs. Directory should be under "/path/to/LILAK/"
set(LILAK_GEANT4_EXECUTABLE_LIST
    CACHE INTERNAL ""
)

foreach(lilak_project ${LILAK_PROJECT_LIST})
    message(STATUS "Project directory      : " ${lilak_project})
    add_subdirectory(${lilak_project})
endforeach(lilak_project)

#=====================================================================================
# You need to tell CMake where to find the ROOT installation. This can be done in a number of ways:
#   - ROOT built with CMake. Add in CMAKE_PREFIX_PATH the installation prefix for ROOT
list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})

#---Locate the ROOT package and defines a number of variables (e.g. ROOT_INCLUDE_DIRS)
#---libs: Core RIO Net Hist Graf Graf3d Gpad Tree Rint Postscript Matrix Physics MathCore Thread MultiProc pthread
#---evelibs: Eve EG TreePlayer Geom Ged RGL Gui Core RIO Net Hist Graf Graf3d Gpad Tree Rint Postscript Matrix Physics MathCore Thread MultiProc pthread
if(ACTIVATE_EVE)
    find_package(ROOT REQUIRED COMPONENTS RIO Net Eve EG RGL Geom Gui Graf Graf3d Ged Matrix MathCore)
else(ACTIVATE_EVE)
    find_package(ROOT REQUIRED COMPONENTS RIO Net Geom Gui Graf Graf3d Ged Matrix MathCore)
endif(ACTIVATE_EVE)

include(${ROOT_USE_FILE})

if(BUILD_GEANT4_SIM)
    find_package(Geant4 REQUIRED ui_all vis_all)
    include(${Geant4_USE_FILE})
endif(BUILD_GEANT4_SIM)

foreach(_directory ${LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF})
    include_directories(${_directory})
    string(REPLACE / _ _name ${_directory})
    list(APPEND LILAK_SOURCE_NAME_LIST_X ${_name})
    file(GLOB LILAK_${_name}_SOURCE_FILES_X ${_directory}/*.cpp)
endforeach(_directory)

foreach(_directory ${LILAK_SOURCE_DIRECTORY_LIST})
    include_directories(${_directory})
    string(REPLACE / _ _name ${_directory})
    list(APPEND LILAK_SOURCE_NAME_LIST ${_name})
    set(LILAK_${_name}_G_NAME G__${_name}Dict)
    file(GLOB LILAK_${_name}_LINKDEF_FILE ${_directory}/LinkDef.h)
    file(GLOB LILAK_${_name}_SOURCE_FILES ${_directory}/*.cpp)
    file(GLOB LILAK_${_name}_HEADER_FILES ${_directory}/*.hpp)
endforeach(_directory)

if(BUILD_GEANT4_SIM)
    foreach(_g4directory ${LILAK_GEANT4_SOURCE_DIRECDTORY_LIST})
        include_directories(${_g4directory})
    endforeach(_g4directory)
endif(BUILD_GEANT4_SIM)

include_directories(${CMAKE_SOURCE_DIR}/log)

add_definitions(${ROOT_CXX_FLAGS})

foreach(_name ${LILAK_SOURCE_NAME_LIST})
    ROOT_GENERATE_DICTIONARY(
        ${LILAK_${_name}_G_NAME}
        ${LILAK_${_name}_HEADER_FILES}
        LINKDEF ${LILAK_${_name}_LINKDEF_FILE}
    )
endforeach(_name)

foreach(_name ${LILAK_SOURCE_NAME_LIST})
    foreach(_file ${LILAK_${_name}_SOURCE_FILES})
        list(APPEND LILAK_LIB_LIST ${_file})
    endforeach(_file)
    list(APPEND LILAK_LIB_LIST ${LILAK_${_name}_G_NAME}.cxx)
endforeach(_name)

foreach(_name ${LILAK_SOURCE_NAME_LIST_X})
    foreach(_file ${LILAK_${_name}_SOURCE_FILES_X})
        list(APPEND LILAK_LIB_LIST ${_file})
    endforeach(_file)
endforeach(_name)

add_library(LILAK SHARED ${LILAK_LIB_LIST})
target_link_libraries(LILAK ${ROOT_LIBRARIES})

if(BUILD_GEANT4_SIM)
    foreach(_directory ${LILAK_GEANT4_SOURCE_DIRECDTORY_LIST})
        set(LILAK_GEANT4_SOURCE_FILES_TEMP)
        file(GLOB LILAK_GEANT4_SOURCE_FILES_TEMP ${_directory}/*.cpp)
        foreach(_file ${LILAK_GEANT4_SOURCE_FILES_TEMP})
        list(APPEND LILAK_GEANT4_SOURCE_FILES ${_file})
        endforeach(_file)

        set(LILAK_GEANT4_HEADER_FILES_TEMP)
        file(GLOB LILAK_GEANT4_HEADER_FILES_TEMP ${_directory}/*.hpp)
        foreach(_file ${LILAK_GEANT4_HEADER_FILES_TEMP})
        list(APPEND LILAK_GEANT4_HEADER_FILES ${_file})
        endforeach(_file)
    endforeach(_directory)

    foreach(_g4sim ${LILAK_GEANT4_EXECUTABLE_LIST})
        get_filename_component(_g4sim_name ${_g4sim} NAME)
        get_filename_component(_g4sim_directory ${_g4sim} DIRECTORY)
        string(REPLACE .cpp "" _executable_head ${_g4sim_name})
        string(REPLACE .cpp .g4sim _executable_name ${_g4sim_name})

        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${_g4sim_directory})
        add_executable(${_executable_name} ${_g4sim} ${LILAK_GEANT4_SOURCE_FILES} ${LILAK_GEANT4_HEADER_FILES})
        execute_process(COMMAND root-config --libs OUTPUT_VARIABLE ROOT_LD_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(CMAKE_EXE_LINKER_FLAGS ${ROOT_LD_FLAGS})
        target_link_libraries(${_executable_name} ${Geant4_LIBRARIES} LILAK)
    endforeach(_g4sim)
endif(BUILD_GEANT4_SIM)

set(LILAK_PATH ${CMAKE_SOURCE_DIR})
configure_file(${LILAK_PATH}/log/LKLog.cmake.in ${LILAK_PATH}/log/LKLog.cmake @ONLY)
add_custom_target(LILAK_LOG ALL
                  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/log/LKLog.cmake
                  VERBATIM)
add_dependencies(LILAK LILAK_LOG)

file(MAKE_DIRECTORY ${LILAK_PATH}/data)
