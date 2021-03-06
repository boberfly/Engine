## ======================================================================== ##
## Copyright 2009-2017 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

# ##################################################################
# add macro INCLUDE_DIRECTORIES_ISPC() that allows to specify search
# paths for ISPC sources
# ##################################################################
SET(ISPC_INCLUDE_DIR "")
MACRO (INCLUDE_DIRECTORIES_ISPC)
  SET(ISPC_INCLUDE_DIR ${ISPC_INCLUDE_DIR} ${ARGN})
ENDMACRO ()

IF (ENABLE_ISPC_SUPPORT)

# ISPC versions to look for, in decending order (newest first)
SET(ISPC_VERSION_WORKING "1.9.2" "1.9.1")
LIST(GET ISPC_VERSION_WORKING -1 ISPC_VERSION_REQUIRED)

IF (NOT ISPC_EXECUTABLE)
  # try sibling folder as hint for path of ISPC
  IF (APPLE)
    SET(ISPC_DIR_SUFFIX "osx")
  ELSEIF(WIN32)
    SET(ISPC_DIR_SUFFIX "windows")
    IF (MSVC14)
      LIST(APPEND ISPC_DIR_SUFFIX "windows-vs2015")
    ELSE()
      LIST(APPEND ISPC_DIR_SUFFIX "windows-vs2013")
    ENDIF()
  ELSE()
    SET(ISPC_DIR_SUFFIX "linux")
  ENDIF()
  FOREACH(ver ${ISPC_VERSION_WORKING})
    FOREACH(suffix ${ISPC_DIR_SUFFIX})
      LIST(APPEND ISPC_DIR_HINT ${PROJECT_SOURCE_DIR}/../ispc-v${ver}-${suffix})
    ENDFOREACH()
  ENDFOREACH()

  FIND_PROGRAM(ISPC_EXECUTABLE ispc PATHS ${ISPC_DIR_HINT} DOC "Path to the ISPC executable.")
  IF (NOT ISPC_EXECUTABLE)
    MESSAGE(FATAL_ERROR "Intel SPMD Compiler (ISPC) not found. Disable ENABLE_ISPC_SUPPORT or install ISPC.")
  ELSE()
    MESSAGE(STATUS "Found Intel SPMD Compiler (ISPC): ${ISPC_EXECUTABLE}")
  ENDIF()
ENDIF()

# check ISPC version
EXECUTE_PROCESS(COMMAND ${ISPC_EXECUTABLE} --version OUTPUT_VARIABLE ISPC_OUTPUT)
STRING(REGEX MATCH " ([0-9]+[.][0-9]+[.][0-9]+)(dev|knl)? " DUMMY "${ISPC_OUTPUT}")
SET(ISPC_VERSION ${CMAKE_MATCH_1})

IF (ISPC_VERSION VERSION_LESS ISPC_VERSION_REQUIRED)
  MESSAGE(FATAL_ERROR "Need at least version ${ISPC_VERSION_REQUIRED} of Intel SPMD Compiler (ISPC).")
ENDIF()

GET_FILENAME_COMPONENT(ISPC_DIR ${ISPC_EXECUTABLE} PATH)

MACRO (ISPC_COMPILE)
  SET(ISPC_ADDITIONAL_ARGS "")

  SET(ISPC_TARGET_EXT ${CMAKE_CXX_OUTPUT_EXTENSION})
  STRING(REPLACE ";" "," ISPC_TARGET_ARGS "${ISPC_TARGETS}")

  IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(ISPC_ARCHITECTURE "x86-64")
  ELSE()
    SET(ISPC_ARCHITECTURE "x86")
  ENDIF()

  SET(ISPC_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR})
  SET(ISPC_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

  IF(ISPC_INCLUDE_DIR)
    STRING(REPLACE ";" ";-I;" ISPC_INCLUDE_DIR_PARMS "${ISPC_INCLUDE_DIR}")
    SET(ISPC_INCLUDE_DIR_PARMS "-I" ${ISPC_INCLUDE_DIR_PARMS})
  ENDIF()

  SET(ISPC_OPT_FLAGS_DEBUG -O0 --no-omit-frame-pointer -g)
  SET(ISPC_OPT_FLAGS_OPTIMIZED -O3 --no-omit-frame-pointer -g)
  SET(ISPC_OPT_FLAGS_RELEASE -O3)

  IF (WIN32)
    SET(ISPC_ADDITIONAL_ARGS ${ISPC_ADDITIONAL_ARGS} --dllexport)
  ELSE()
    SET(ISPC_ADDITIONAL_ARGS ${ISPC_ADDITIONAL_ARGS} --pic)
  ENDIF()

  SET(ISPC_OBJECTS "")

  FOREACH(src ${ARGN})
    GET_FILENAME_COMPONENT(fname ${src} NAME_WE)
    GET_FILENAME_COMPONENT(dir ${src} PATH)

    SET(outdir "${ISPC_TARGET_DIR}/${dir}")
    SET(input ${CMAKE_CURRENT_SOURCE_DIR}/${src})

    SET(deps "")
    IF (EXISTS ${outdir}/${fname}.dev.idep)
      FILE(READ ${outdir}/${fname}.dev.idep contents)
      STRING(REPLACE " " ";"     contents "${contents}")
      STRING(REPLACE ";" "\\\\;" contents "${contents}")
      STRING(REPLACE "\n" ";"    contents "${contents}")
      FOREACH(dep ${contents})
        IF (EXISTS ${dep})
          SET(deps ${deps} ${dep})
        ENDIF (EXISTS ${dep})
      ENDFOREACH(dep ${contents})
    ENDIF ()

    SET(results "${outdir}/${fname}.dev${ISPC_TARGET_EXT}")

    # if we have multiple targets add additional object files
    LIST(LENGTH ISPC_TARGETS NUM_TARGETS)
    IF (NUM_TARGETS GREATER 1)
      FOREACH(target ${ISPC_TARGETS})
        # in v1.9.0 ISPC changed the ISA suffix of avx512knl-i32x16 to just 'avx512knl'
        IF (${target} STREQUAL "avx512knl-i32x16" AND NOT ISPC_VERSION VERSION_LESS "1.9.0")
          SET(target "avx512knl")
        ELSEIF (${target} STREQUAL "avx512skx-i32x16")
          SET(target "avx512skx")
        ENDIF()
        SET(results ${results} "${outdir}/${fname}.dev_${target}${ISPC_TARGET_EXT}")
      ENDFOREACH()
    ENDIF()

    ADD_CUSTOM_COMMAND(
      OUTPUT ${results} ${ISPC_TARGET_DIR}/ispc/${fname}_ispc.h
      COMMAND ${CMAKE_COMMAND} -E make_directory ${outdir}
      COMMAND ${ISPC_EXECUTABLE}
      -I ${CMAKE_CURRENT_SOURCE_DIR}
      ${ISPC_INCLUDE_DIR_PARMS}
      --arch=${ISPC_ARCHITECTURE}
      ${ISPC_OPT_FLAGS_RELEASE}
      --target=${ISPC_TARGET_ARGS}
      --opt=fast-math
      ${ISPC_ADDITIONAL_ARGS}
      -h ${ISPC_HEADER_DIR}/ispc/${fname}_ispc.h
      -MMM  ${outdir}/${fname}.dev.idep
      -o ${outdir}/${fname}.dev${ISPC_TARGET_EXT}
      ${input}
      DEPENDS ${input} ${deps}
      COMMENT "Building ISPC object ${outdir}/${fname}.dev${ISPC_TARGET_EXT}"
    )

    SET(ISPC_OBJECTS ${ISPC_OBJECTS} ${results})
  ENDFOREACH()
ENDMACRO()

MACRO (ADD_ISPC_EXECUTABLE name)
   SET(ISPC_SOURCES "")
   SET(OTHER_SOURCES "")
   FOREACH(src ${ARGN})
    GET_FILENAME_COMPONENT(ext ${src} EXT)
    IF (ext STREQUAL ".ispc")
      SET(ISPC_SOURCES ${ISPC_SOURCES} ${src})
    ELSE ()
      SET(OTHER_SOURCES ${OTHER_SOURCES} ${src})
    ENDIF ()
  ENDFOREACH()
  ISPC_COMPILE(${ISPC_SOURCES})
  ADD_EXECUTABLE(${name} ${ISPC_SOURCES} ${ISPC_OBJECTS} ${OTHER_SOURCES})
ENDMACRO()

MACRO (ADD_ISPC_LIBRARY name type)
   SET(ISPC_SOURCES "")
   SET(OTHER_SOURCES "")
   FOREACH(src ${ARGN})
    GET_FILENAME_COMPONENT(ext ${src} EXT)
    IF (ext STREQUAL ".ispc")
      SET(ISPC_SOURCES ${ISPC_SOURCES} ${src})
    ELSE ()
      SET(OTHER_SOURCES ${OTHER_SOURCES} ${src})
    ENDIF ()
  ENDFOREACH()
  ISPC_COMPILE(${ISPC_SOURCES})
  ADD_LIBRARY(${name} ${type} ${ISPC_SOURCES} ${ISPC_OBJECTS} ${OTHER_SOURCES})
ENDMACRO()

ELSE (ENABLE_ISPC_SUPPORT)

MACRO (ADD_ISPC_EXECUTABLE name)
   SET(ISPC_SOURCES "")
   SET(OTHER_SOURCES "")
   FOREACH(src ${ARGN})
    GET_FILENAME_COMPONENT(ext ${src} EXT)
    IF (ext STREQUAL ".ispc")
      SET(ISPC_SOURCES ${ISPC_SOURCES} ${src})
    ELSE ()
      SET(OTHER_SOURCES ${OTHER_SOURCES} ${src})
    ENDIF ()
  ENDFOREACH()
  ADD_EXECUTABLE(${name} ${OTHER_SOURCES})
ENDMACRO()

MACRO (ADD_ISPC_LIBRARY name type)
   SET(ISPC_SOURCES "")
   SET(OTHER_SOURCES "")
   FOREACH(src ${ARGN})
    GET_FILENAME_COMPONENT(ext ${src} EXT)
    IF (ext STREQUAL ".ispc")
      SET(ISPC_SOURCES ${ISPC_SOURCES} ${src})
    ELSE ()
      SET(OTHER_SOURCES ${OTHER_SOURCES} ${src})
    ENDIF ()
  ENDFOREACH()
  ADD_LIBRARY(${name} ${type} ${OTHER_SOURCES})
ENDMACRO()

ENDIF (ENABLE_ISPC_SUPPORT)