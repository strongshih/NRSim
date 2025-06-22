file(GLOB NPU_PE_SOURCES "*.cpp")
file(GLOB NPU_PE_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER NPU_PE_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_NPU_PE testbench.cpp ${NPU_PE_SOURCES} ${NPU_PE_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
