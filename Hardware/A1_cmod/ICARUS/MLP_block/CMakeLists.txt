file(GLOB MLP_BLOCK_SOURCES "*.cpp")
file(GLOB MLP_BLOCK_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER MLP_BLOCK_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include ../MLP_shared)

add_executable(sim_MLP_BLOCK testbench.cpp ${MLP_BLOCK_SOURCES} ${MLP_BLOCK_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
