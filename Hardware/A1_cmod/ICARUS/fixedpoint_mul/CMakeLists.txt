file(GLOB FIXEDPOINT_MUL_SOURCES "*.cpp")
file(GLOB FIXEDPOINT_MUL_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER FIXEDPOINT_MUL_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_fixedpoint_mul testbench.cpp ${FIXEDPOINT_MUL_SOURCES} ${FIXEDPOINT_MUL_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
