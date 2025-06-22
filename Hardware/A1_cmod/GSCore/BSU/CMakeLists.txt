file(GLOB BSU_SOURCES "*.cpp")
file(GLOB BSU_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER BSU_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_BSU testbench.cpp ${BSU_SOURCES} ${BSU_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
