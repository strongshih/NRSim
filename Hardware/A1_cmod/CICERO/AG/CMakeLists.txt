file(GLOB AG_SOURCES "*.cpp")
file(GLOB AG_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER AG_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_AG testbench.cpp ${AG_SOURCES} ${AG_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
