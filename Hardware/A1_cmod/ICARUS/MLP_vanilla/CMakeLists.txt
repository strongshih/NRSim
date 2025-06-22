file(GLOB MLP_VANILLA_SOURCES "*.cpp")
file(GLOB MLP_VANILLA_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER MLP_VANILLA_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include ../MLP_shared)

add_executable(sim_MLP_VANILLA testbench.cpp ${MLP_VANILLA_SOURCES} ${MLP_VANILLA_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
