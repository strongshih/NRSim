file(GLOB QSU_SOURCES "*.cpp")
file(GLOB QSU_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER QSU_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_QSU testbench.cpp ${QSU_SOURCES} ${QSU_HEADERS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS} ${SIMFLAGS} ${LIBFLAGS}")
