file(GLOB MLP_SSA_SOURCES "*.cpp")
file(GLOB MLP_SSA_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER MLP_SSA_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include ../MLP_shared ../fixedpoint_mul)

add_executable(sim_MLP_SSA testbench.cpp ${MLP_SSA_SOURCES} ${MLP_SSA_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
