file(GLOB NPU_SOURCES "*.cpp")
file(GLOB NPU_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER NPU_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_NPU testbench.cpp ${NPU_SOURCES} ${NPU_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
