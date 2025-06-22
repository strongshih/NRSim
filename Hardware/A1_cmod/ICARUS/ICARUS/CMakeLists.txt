file(GLOB ICARUS_SOURCES "*.cpp")
file(GLOB ICARUS_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER ICARUS_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include ../VRU ../PEU ../MLP_vanilla) # TODO: Change MLP implementation

add_executable(sim_ICARUS testbench.cpp ${ICARUS_SOURCES} ${ICARUS_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
