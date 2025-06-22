file(GLOB reducer_SOURCES "*.cpp")
file(GLOB reducer_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER reducer_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_reducer testbench.cpp ${reducer_SOURCES} ${reducer_HEADERS})

set(CMAKE_CXX_FLreducerS "${CMAKE_CXX_FLreducerS} ${CFLreducerS} ${SIMFLreducerS} ${LIBFLreducerS}")
