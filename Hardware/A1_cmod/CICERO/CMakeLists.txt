cmake_minimum_required(VERSION 3.22)
project(CICERO)

set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_COLOR_DIAGNOSTICS ON)

message(STATUS "High Level Synthesis C Model Project")

# Check for CATAPULT_HOME
if(NOT DEFINED ENV{CATAPULT_HOME})
    message(FATAL_ERROR "Environment variable CATAPULT_HOME is not set.")
endif()

# Check for MATCHLIB_HOME
if(NOT DEFINED ENV{MATCHLIB_HOME})
    message(FATAL_ERROR "Environment variable MATCHLIB_HOME is not set.")
endif()

# Get values after confirming they're set
set(CATAPULT_HOME $ENV{CATAPULT_HOME})
set(MATCHLIB_HOME $ENV{MATCHLIB_HOME})

cmake_host_system_information(RESULT HOSTNAME QUERY HOSTNAME)
message(STATUS "Hostname: ${HOSTNAME}")
message(STATUS "Processor architecture: ${CMAKE_HOST_SYSTEM_PROCESSOR}")

message(STATUS "Catapult home set to: ${CATAPULT_HOME}")
message(STATUS "Matchlib home set to: ${MATCHLIB_HOME}")

FIND_PACKAGE(Boost)
IF (Boost_FOUND)
    message(STATUS "Boost found")
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
    ADD_DEFINITIONS( "-DHAS_BOOST" )
ENDIF()

# set catapult compiler
set(CMAKE_C_COMPILER ${CATAPULT_HOME}/Mgc_home/pkgs/dcs_gcc/gcc-10.3.0/bin/gcc)
set(CMAKE_CXX_COMPILER ${CATAPULT_HOME}/Mgc_home/pkgs/dcs_gcc/gcc-10.3.0/bin/g++)

# matchlib nvlibs
include_directories(${MATCHLIB_HOME})

# catapult systemc libs
include_directories(${CATAPULT_HOME}/Mgc_home/shared/include)
link_directories(${CATAPULT_HOME}/Mgc_home/shared/lib)
link_libraries(${CATAPULT_HOME}/Mgc_home/shared/lib/libsystemc.a)

link_directories(${CATAPULT_HOME}/Mgc_home/pkgs/dcs_gcc/gcc-10.3.0/lib64)
link_libraries(${CATAPULT_HOME}/Mgc_home/pkgs/dcs_gcc/gcc-10.3.0/lib64/libstdc++.a)

# boost libs
include_directories(${BOOST_INCLUDE_DIR})
link_directories(${BOOST_LIBRARYDIR})
#link_libraries(${BOOST_LIBRARYDIR})

# shared compilation options
set(CFLAGS "-Wall -Wno-unknown-pragmas -DDEBUG_LEVEL=1")
set(LIBFLAGS "-lstdc++ -lsystemc -lm -lpthread -lboost_timer -lboost_chrono -lboost_system")
set(SIMFLAGS "-DCONNECTIONS_ACCURATE_SIM -DSC_INCLUDE_DYNAMIC_PROCESSES")# -DHLS_CATAPULT")# -DCONN_RAND_STALL")
set(SIMFLAGS_CYCACC "-DCONNECTIONS_ACCURATE_SIM -DSC_INCLUDE_DYNAMIC_PROCESSES")

# add subdirectories
add_subdirectory(NPU_PE)
add_subdirectory(NPU)
add_subdirectory(AG)
add_subdirectory(reducer)
