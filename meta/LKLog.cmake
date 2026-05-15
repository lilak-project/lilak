set(LILAK_PATH /Users/jungwoo/Research/lilak)
set(CREATE_GIT_LOG ON)
set(BUILD_DOXYGEN_DOC OFF)
set(LILAK_PROJECT_MAIN )
set(LILAK_ROOT_LINKED_CLASS_LIST LKClassFactory;LKCompiled;LKGear;LKLogger;LKParameter;LKParameterContainer;LKRun;LKTestRun;LKVirtualRun;LKListFileParser;LKMisc;LKNoiseAnalyzer;LKODRFitter;LKPadInteractive;LKPadInteractiveManager;LKRunTimeMeasure;LKSAM;LKWithOption;LKAttenuationCalculator;LKBeamPID;LKBeamPIDControl;LKBinning;LKBinning1;LKCut;LKDataViewer;LKDrawing;LKDrawingGroup;LKPainter;LKGETFrameParser;LKGETRawConverter;LKGETRawFrame;LKHTLineTracker;LKImagePoint;LKParamPointRT;LKChannelAnalyzer;LKChannelSimulator;LKPulse;LKPulseAnalyzer;LKPulseFitParameter;LKGeo2DBox;LKGeoBox;LKGeoBoxStack;LKGeoCircle;LKGeoHelix;LKGeoLine;LKGeoPlane;LKGeoPlaneWithCenter;LKGeoPolygon;LKGeoRotated;LKGeoSphere;LKGeometry;LKVector3;LKDetector;LKDetectorPlane;LKDetectorSystem;LKEvePlane;LKMicromegas;LKPolygonPadPlane;LKContainer;LKEventHeader;LKMCTagged;LKPulseFitData;LKElectronicsTask;LKEveTask;LKGETChannelViewer;LKGETConversionTask;LKHTTrackingTask;LKNoiseSubtractionTask;LKPulseExtractionTask;LKPulseShapeAnalysisTask;LKTask;LKExtrapolatedTrack;LKHelixTrack;LKLinearTrack;LKMCTrack;LKTracklet;GETChannel;GETChannelMapData;GETParameters;LKBufferD;LKBufferI;LKChannel;LKPad;LKSiChannel;LKSiDetector;LKHit;LKHitArray;LKMCStep;LKVertex;LKWPoint;EKRecoHit;EKSiHit;SKRecoHeader;SKSiHit;ELARK;LKSiliconArray;SKSiArrayPlane;STARK;SKAnalysisDK;SKAnalysisJW;SKAnalysisTA;SKDrawCalibratedEventStatisticsTask;SKDrawEventStatisticsTask;SKDrawWaveformTask;SKEnergyRestorationTask;SKGatingTask;SKPairMatchingTask;SKScintillatorFilterTask;SKSetSiChannelTask;LKMTEMerger;LKSiCalibrationHistogramBuilder;LKSiCalibratorC0;LKSiCalibratorC1;LKSiCalibratorC2;LKSiliconMapping;SKEnergyHandler;ATMicromegas;AToMX;ATFastHitReconstruction)
set(LILAK_PROJECT_LIST stark;atomx)

find_package(Git)
if(CREATE_GIT_LOG)
    if(NOT GIT_FOUND)
        message(FATAL_ERROR "Git is needed to create git-log.")
        set(CREATE_GIT_LOG off)
    endif()
endif()

if(BUILD_DOXYGEN_DOC)
    find_package(Doxygen)
    if(NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is needed to build the documentation.")
        set(BUILD_DOXYGEN_DOC off)
    endif()
endif()

if(CREATE_GIT_LOG)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE LILAK_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE 
        WORKING_DIRECTORY ${LILAK_PATH})
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count ${LILAK_GIT_BRANCH}
        OUTPUT_VARIABLE LILAK_GIT_COMMIT_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${LILAK_PATH})
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short ${LILAK_GIT_BRANCH}
        OUTPUT_VARIABLE LILAK_GIT_HASH_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${LILAK_PATH})
    if("${LILAK_GIT_COMMIT_COUNT}" STREQUAL "")
        set(LILAK_GIT_COMMIT_COUNT 0)
    endif()
    set(LILAK_VERSION "${LILAK_GIT_BRANCH}.${LILAK_GIT_COMMIT_COUNT}.${LILAK_GIT_HASH_SHORT}")
    set(LILAK_HASH "${LILAK_GIT_HASH_SHORT}")
    message(STATUS "lilak version: " ${LILAK_VERSION})

    execute_process(COMMAND ${GIT_EXECUTABLE} branch -l
        OUTPUT_VARIABLE LILAK_GIT_BRANCH_LIST 
        OUTPUT_STRIP_TRAILING_WHITESPACE 
        WORKING_DIRECTORY ${LILAK_PATH})
    string(REPLACE "\n" ";" LILAK_GIT_BRANCH_LIST "${LILAK_GIT_BRANCH_LIST}")
    string(REPLACE " " "" LILAK_GIT_BRANCH_LIST "${LILAK_GIT_BRANCH_LIST}")
    string(REPLACE "*" "" LILAK_GIT_BRANCH_LIST "${LILAK_GIT_BRANCH_LIST}")

    foreach(_branch ${LILAK_GIT_BRANCH_LIST})
        execute_process(COMMAND ${GIT_EXECUTABLE} log --reverse --oneline ${_branch} OUTPUT_VARIABLE LILAK_GIT_BRANCH_LOGS)
        list(APPEND LILAK_GIT_BRANCH_LOG_LIST "__BRANCH__ ${_branch}")
        foreach(_log ${LILAK_GIT_BRANCH_LOGS})
            list(APPEND LILAK_GIT_BRANCH_LOG_LIST ${_log})
        endforeach(_log)
    endforeach(_branch)

    string(REPLACE ";" "\n" LILAK_GIT_BRANCH_LOG_LIST "${LILAK_GIT_BRANCH_LOG_LIST}")

    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE LILAK_MAINPROJECT_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${LILAK_PATH}/${LILAK_PROJECT_MAIN})
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count ${LILAK_MAINPROJECT_GIT_BRANCH}
        OUTPUT_VARIABLE LILAK_MAINPROJECT_GIT_COMMIT_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${LILAK_PATH}/${LILAK_PROJECT_MAIN})
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short ${LILAK_MAINPROJECT_GIT_BRANCH}
        OUTPUT_VARIABLE LILAK_MAINPROJECT_GIT_HASH_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${LILAK_PATH}/${LILAK_PROJECT_MAIN})
    set(LILAK_MAINPROJECT_VERSION "${LILAK_MAINPROJECT_GIT_BRANCH}.${LILAK_MAINPROJECT_GIT_COMMIT_COUNT}.${LILAK_MAINPROJECT_GIT_HASH_SHORT}")
    set(LILAK_MAINPROJECT_HASH "${LILAK_MAINPROJECT_GIT_HASH_SHORT}")
    message(STATUS "Main project version: " ${LILAK_MAINPROJECT_VERSION})

    ####################################################################################
    list(LENGTH LILAK_PROJECT_LIST N_LILAK_PROJECT)
    set(N_LILAK_PROJECT ${N_LILAK_PROJECT})
    list(JOIN LILAK_PROJECT_LIST ":" LILAK_PROJECT_ALL)
    set(_NAMES_INIT "")
    set(_VERSIONS_INIT "")
    set(_TAG_LINES_INIT "")
    set(LILAK_PROJECT_VERSIONS_LIST)  # (원한다면 리스트 형태도 유지)

    foreach(lilak_project IN LISTS LILAK_PROJECT_LIST)
        execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
            OUTPUT_VARIABLE LILAK_PROJECT_GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${LILAK_PATH}/${lilak_project})
        execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count ${LILAK_PROJECT_GIT_BRANCH}
            OUTPUT_VARIABLE LILAK_PROJECT_GIT_COMMIT_COUNT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${LILAK_PATH}/${lilak_project})
        execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short ${LILAK_PROJECT_GIT_BRANCH}
            OUTPUT_VARIABLE LILAK_PROJECT_GIT_HASH_SHORT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${LILAK_PATH}/${lilak_project})
        set(_VERSION_STRING "${LILAK_PROJECT_GIT_BRANCH}.${LILAK_PROJECT_GIT_COMMIT_COUNT}.${LILAK_PROJECT_GIT_HASH_SHORT}")
        list(APPEND LILAK_PROJECT_VERSIONS_LIST "${_VERSION_STRING}")
        if(NOT _NAMES_INIT STREQUAL "")
            string(APPEND _NAMES_INIT ", ")
            string(APPEND _VERSIONS_INIT ", ")
        endif()
        string(APPEND _NAMES_INIT "\"${lilak_project}\"")
        string(APPEND _VERSIONS_INIT "\"${_VERSION_STRING}\"")
        if(NOT _TAG_LINES_INIT STREQUAL "")
            string(APPEND _TAG_LINES_INIT "\n")
        endif()
        string(APPEND _TAG_LINES_INIT "${lilak_project} ${_VERSION_STRING}")
        message(STATUS "[LILAK] ${lilak_project} version: ${_VERSION_STRING}")
    endforeach()

    set(LILAK_PROJECT_NAMES_INIT "${_NAMES_INIT}")
    set(LILAK_PROJECT_VERSIONS_INIT "${_VERSIONS_INIT}")
    set(LILAK_PROJECT_TAG_LINES "${_TAG_LINES_INIT}")
    ###################################################################################

    execute_process(COMMAND ${GIT_EXECUTABLE} log --pretty=format:"%h %an %ar : %s" -1 OUTPUT_VARIABLE LILAK_GIT_LOG_TEMP)
    execute_process(COMMAND ${GIT_EXECUTABLE} status -s      OUTPUT_VARIABLE LILAK_GIT_STATUS OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} diff -U0 OUTPUT_VARIABLE LILAK_GIT_DIFF OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REPLACE \" "" LILAK_GIT_LOG ${LILAK_GIT_LOG_TEMP})

    cmake_host_system_information(RESULT LILAK_HOSTNAME QUERY HOSTNAME)

    set(LILAK_USERNAME $ENV{USER})

    set(LILAK_HOSTUSER ${LILAK_HOSTNAME}.${LILAK_USERNAME})
    message(${LILAK_HOSTUSER})

    string(REPLACE ";" "\n" LILAK_ROOT_LINKED_CLASS_LIST "${LILAK_ROOT_LINKED_CLASS_LIST}")

    configure_file(${LILAK_PATH}/meta/LKCompiled.h.in         ${LILAK_PATH}/source/base/LKCompiled.h @ONLY)
    configure_file(${LILAK_PATH}/meta/LKCompiledTags.log.in   ${LILAK_PATH}/meta/LKCompiledTags.log @ONLY)
    configure_file(${LILAK_PATH}/meta/LKCompiledStatus.log.in ${LILAK_PATH}/meta/LKCompiledStatus.log @ONLY)
    configure_file(${LILAK_PATH}/meta/LKCompiledDiff.log.in   ${LILAK_PATH}/meta/LKCompiledDiff.log @ONLY)
    configure_file(${LILAK_PATH}/meta/LKBranchList.log.in     ${LILAK_PATH}/meta/LKBranchList.log @ONLY)
    configure_file(${LILAK_PATH}/meta/LKClassList.log.in      ${LILAK_PATH}/meta/LKClassList.log @ONLY)
endif()

configure_file(${LILAK_PATH}/meta/doxy.conf.in ${LILAK_PATH}/meta/doxy.conf @ONLY)
if(BUILD_DOXYGEN_DOC)
  execute_process(COMMAND ${DOXYGEN_EXECUTABLE} ${LILAK_PATH}/meta/doxy.conf 
                  WORKING_DIRECTORY ${LILAK_PATH})
endif()
