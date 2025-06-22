file(GLOB PEU_SOURCES "*.cpp")
file(GLOB PEU_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER PEU_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_PEU testbench.cpp ${PEU_SOURCES} ${PEU_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
