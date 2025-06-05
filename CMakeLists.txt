# CMakeLists.txt LILAK project.
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(LILAK)

set(LILAK_PATH ${CMAKE_SOURCE_DIR})

#---Configuration
set(ACTIVATE_EVE        OFF CACHE INTERNAL "") # Activate Root Eve package
set(CREATE_GIT_LOG       ON CACHE INTERNAL "") # Create Git log
set(BUILD_GEANT4_SIM    OFF CACHE INTERNAL "") # Build Geant4 simulation
set(BUILD_DOXYGEN_DOC   OFF CACHE INTERNAL "") # Build Doxygen documentation with HTML
set(BUILD_MFM_CONVERTER OFF CACHE INTERNAL "")
set(BUILD_JSONCPP       OFF CACHE INTERNAL "")
set(LILAK_PROJECT_LIST      CACHE INTERNAL "") # List of LILAK projects to build
set(LILAK_PROJECT_MAIN      CACHE INTERNAL "")
set(GRUDIR               "" CACHE INTERNAL "")
set(GETDIR               "" CACHE INTERNAL "")
# set(BUILD_NPTOOL         ON CACHE INTERNAL "") # NPTool simulation

#---Configuration
include(log/build_options.cmake)

add_compile_definitions(LILAK_COMPILED)

if(BUILD_JSONCPP)
    add_compile_definitions(LILAK_BUILD_JSONCPP)
endif(BUILD_JSONCPP)

if(ACTIVATE_EVE)
    add_compile_definitions(ACTIVATE_EVE)
endif(ACTIVATE_EVE)

if(BUILD_GEANT4_SIM)
    add_compile_definitions(LILAK_GEANT4)
endif(BUILD_GEANT4_SIM)

if(BUILD_MFM_CONVERTER)
    add_compile_definitions(BUILD_MFM_CONVERTER)
    add_compile_definitions(TIXML_USE_STL)
endif(BUILD_MFM_CONVERTER)

message(STATUS "Create Git log         : " ${CREATE_GIT_LOG})
message(STATUS "Activate Eve package   : " ${ACTIVATE_EVE})
message(STATUS "Build Geant4 simulation: " ${BUILD_GEANT4_SIM})
message(STATUS "Build Doxygen document : " ${BUILD_DOXYGEN_DOC})
message(STATUS "Build MFM converter    : " ${BUILD_MFM_CONVERTER})
message(STATUS "Main Project           : " ${LILAK_PROJECT_MAIN})

#---Add source directories. Directory should be under "/path/to/LILAK/"
set(LILAK_SOURCE_DIRECTORY_LIST
    ${LILAK_PATH}/source/base
    ${LILAK_PATH}/source/tool
    ${LILAK_PATH}/source/tool/pulse_analysis
    ${LILAK_PATH}/source/tool/hough_transform
    ${LILAK_PATH}/source/tool/drawing
    ${LILAK_PATH}/source/geometry
    ${LILAK_PATH}/source/detector
    ${LILAK_PATH}/source/container
    ${LILAK_PATH}/source/task
    ${LILAK_PATH}/source/container/track
    ${LILAK_PATH}/source/container/readout
    ${LILAK_PATH}/source/container/measurement
    CACHE INTERNAL ""
)

#---Add source directories that do not need rootcling. Directory should be under "/path/to/LILAK/".
set(LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF
    CACHE INTERNAL ""
)

if(BUILD_JSONCPP)
    set(LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF ${LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF}
        ${LILAK_PATH}/source/jsoncpp
        CACHE INTERNAL ""
    )
endif(BUILD_JSONCPP)

#---Add geant4 source directories. Directory should be under "/path/to/LILAK/".
set(LILAK_GEANT4_SOURCE_DIRECTORY_LIST
    ${LILAK_PATH}/source/geant4
    CACHE INTERNAL ""
)

set(LILAK_MFM_SOURCE_DIRECTORY_LIST
    ${LILAK_PATH}/source/mfm
    CACHE INTERNAL ""
)
#---Add geant4 main programs. Directory should be under "/path/to/LILAK/"
set(LILAK_EXECUTABLE_LIST
    CACHE INTERNAL ""
)
foreach(lilak_project ${LILAK_PROJECT_LIST})
    message(STATUS "Project directory      : " ${lilak_project})
    add_subdirectory(${lilak_project})
endforeach(lilak_project)

if(BUILD_MFM_CONVERTER)
    foreach(_directory ${LILAK_MFM_SOURCE_DIRECTORY_LIST})
        list(APPEND LILAK_SOURCE_DIRECTORY_LIST ${_directory})
    endforeach(_directory)
endif(BUILD_MFM_CONVERTER)

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

if(BUILD_MFM_CONVERTER)
    #set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/usr/local/etc/cmake")
    #add_definitions( -DTIXML_USE_STL ) ###
    #include_directories(${MFMHistServer_SOURCE_DIR}/source)
    #find_package(ROOT REQUIRED)
    #set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ROOT_CXX_FLAGS} -std=c++14 -pthread -Wno-deprecated-declarations")
    #set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lncurses" )
    #include_directories(${ROOT_INCLUDE_DIR} ${CMAKE_SOURCE_DIR})
    #link_directories(${ROOT_LIBRARY_DIR})
    #set(GRUDIR "/usr/local/gru")
    #include_directories(${GRUDIR}/vigru)
    #include_directories(${GRUDIR}/gsoap)
    #set(GETDIR2 "/opt/software/get_src/20210208/MultiFrame/src")
    #include_directories(${GETDIR2})

    message(STATUS GRUDIR:\ ${GRUDIR})
    include_directories(${GRUDIR}/include)
    link_directories(${GRUDIR}/lib)

    message(STATUS GETDIR:\ ${GETDIR})
    include_directories(${GETDIR}/include)
    link_directories(${GETDIR}/lib)
endif(BUILD_MFM_CONVERTER)

if(BUILD_GEANT4_SIM)
    find_package(Geant4 REQUIRED ui_all vis_all)
    include(${Geant4_USE_FILE})
endif(BUILD_GEANT4_SIM)

foreach(_directory ${LILAK_SOURCE_DIRECTORY_LIST_XLINKDEF})
    message(STATUS "source directory : " ${_directory})
    include_directories(${_directory})
    string(REPLACE / _ _name ${_directory})
    list(APPEND LILAK_SOURCE_NAME_LIST_X ${_name})
    file(GLOB LILAK_${_name}_SOURCE_FILES_X ${_directory}/*.cpp)
    file(GLOB LILAK_${_name}_HEADER_FILES_X ${_directory}/*.h)
endforeach(_directory)

set(LILAK_ROOT_LINKED_CLASS_LIST)

foreach(_directory ${LILAK_SOURCE_DIRECTORY_LIST})
    message(STATUS "root-link directory : " ${_directory})
    include_directories(${_directory})
    string(REPLACE / _ _name ${_directory})
    list(APPEND LILAK_SOURCE_NAME_LIST ${_name})
    set(LILAK_${_name}_G_NAME G__${_name}_Dict)

    file(GLOB LILAK_${_name}_SOURCE_FILES ${_directory}/*.cpp)
    file(GLOB LILAK_${_name}_HEADER_FILES ${_directory}/*.h)

    # list separate strings using ";". list append add semicolon automatically just after previous string.
    set(PRAGMA_LINE_CPP_CLASS_LIST)
    foreach(_full_name ${LILAK_${_name}_HEADER_FILES})
        get_filename_component(file_name ${_full_name} NAME)
        string(REPLACE .h "" class_name ${file_name})
        set(pragma_line "\n#pragma link C++ class ${class_name}+")
        list(APPEND PRAGMA_LINE_CPP_CLASS_LIST ${pragma_line})
        list(APPEND LILAK_ROOT_LINKED_CLASS_LIST ${class_name})
    endforeach(_full_name)
    list(APPEND PRAGMA_LINE_CPP_CLASS_LIST "\n")
    configure_file(${LILAK_PATH}/log/LinkDef.h.in ${CMAKE_BINARY_DIR}/link_${_name}_LinkDef.h @ONLY)
    file(GLOB LILAK_${_name}_LINKDEF_FILE ${CMAKE_BINARY_DIR}/link_${_name}_LinkDef.h)
endforeach(_directory)

if(BUILD_GEANT4_SIM)
    foreach(_directory ${LILAK_GEANT4_SOURCE_DIRECTORY_LIST})
        message(STATUS "geant4    directory : " ${_directory})
        file(GLOB LILAK_GEANT4_SOURCE_FILES ${_directory}/*.cpp)
        file(GLOB LILAK_GEANT4_HEADER_FILES ${_directory}/*.h)
        foreach(_file ${LILAK_GEANT4_SOURCE_FILES})
            list(APPEND LILAK_SOURCE_FILES ${_file})
        endforeach(_file)
        foreach(_file ${LILAK_GEANT4_HEADER_FILES})
            list(APPEND LILAK_HEADER_FILES ${_file})
        endforeach(_file)
    endforeach(_directory)
endif(BUILD_GEANT4_SIM)

if(BUILD_GEANT4_SIM)
    foreach(_g4directory ${LILAK_GEANT4_SOURCE_DIRECTORY_LIST})
        include_directories(${_g4directory})
    endforeach(_g4directory)
endif(BUILD_GEANT4_SIM)

include_directories(${LILAK_PATH}/log)

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
        list(APPEND LILAK_SOURCE_FILES ${_file})
    endforeach(_file)
    foreach(_file ${LILAK_${_name}_HEADER_FILES})
        list(APPEND LILAK_HEADER_FILES ${_file})
    endforeach(_file)
    list(APPEND LILAK_LIB_LIST ${LILAK_${_name}_G_NAME}.cxx)
endforeach(_name)

foreach(_name ${LILAK_SOURCE_NAME_LIST_X})
    #foreach(_file ${LILAK_${_name}_SOURCE_FILES_X})
    #    list(APPEND LILAK_LIB_LIST ${_file})
    #endforeach(_file)
    foreach(_file ${LILAK_${_name}_SOURCE_FILES_X})
        list(APPEND LILAK_SOURCE_FILES ${_file})
    endforeach(_file)
    foreach(_file ${LILAK_${_name}_HEADER_FILES_X})
        list(APPEND LILAK_HEADER_FILES ${_file})
    endforeach(_file)
endforeach(_name)

if(BUILD_GEANT4_SIM)
    foreach(_directory ${LILAK_GEANT4_SOURCE_DIRECDTORY_LIST})
        message(STATUS "geant4    directory : " ${_directory})
        file(GLOB LILAK_GEANT4_SOURCE_FILES ${_directory}/*.cpp)
        file(GLOB LILAK_GEANT4_HEADER_FILES ${_directory}/*.h)
        foreach(_file ${LILAK_GEANT4_SOURCE_FILES})
            list(APPEND LILAK_SOURCE_FILES ${_file})
        endforeach(_file)
        foreach(_file ${LILAK_GEANT4_HEADER_FILES})
            list(APPEND LILAK_HEADER_FILES ${_file})
        endforeach(_file)
    endforeach(_directory)
endif(BUILD_GEANT4_SIM)

add_library(LILAK SHARED ${LILAK_LIB_LIST})

if(BUILD_MFM_CONVERTER)
    #target_link_libraries(LILAK ${ROOT_LIBRARIES} Html Spectrum XMLIO GRU MultiFrame)
    target_link_libraries(LILAK ${ROOT_LIBRARIES} Html Spectrum XMLIO MultiFrame)
else(BUILD_MFM_CONVERTER)
    target_link_libraries(LILAK ${ROOT_LIBRARIES})
endif(BUILD_MFM_CONVERTER)

foreach(_executable ${LILAK_EXECUTABLE_LIST})
    get_filename_component(_executable_name ${_executable} NAME)
    get_filename_component(_executable_directory ${_executable} DIRECTORY)
    string(REPLACE .cc  .exe _executable_name ${_executable_name})
    message(STATUS "Executable : " ${_executable_directory}/${_executable_name})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${_executable_directory})
    add_executable(
        ${_executable_name}
        ${_executable}
        ${LILAK_SOURCE_FILES}
        ${LILAK_HEADER_FILES}
    )
    execute_process(COMMAND root-config --libs OUTPUT_VARIABLE ROOT_LD_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(CMAKE_EXE_LINKER_FLAGS ${ROOT_LD_FLAGS})
    if(BUILD_MFM_CONVERTER)
        target_link_libraries(${_executable_name} ${Geant4_LIBRARIES} LILAK)
    else(BUILD_MFM_CONVERTER)
        target_link_libraries(${_executable_name} ${Geant4_LIBRARIES} LILAK)
    endif(BUILD_MFM_CONVERTER)
endforeach(_executable)

configure_file(${LILAK_PATH}/log/LKLog.cmake.in ${LILAK_PATH}/log/LKLog.cmake @ONLY)
add_custom_target(LILAK_LOG ALL
                  COMMAND ${CMAKE_COMMAND} -P ${LILAK_PATH}/log/LKLog.cmake
                  VERBATIM)
add_dependencies(LILAK LILAK_LOG)

file(MAKE_DIRECTORY ${LILAK_PATH}/data)
