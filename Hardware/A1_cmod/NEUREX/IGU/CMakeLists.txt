file(GLOB IGU_SOURCES "*.cpp")
file(GLOB IGU_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER IGU_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_IGU testbench.cpp ${IGU_SOURCES} ${IGU_HEADERS})

set(CMAKE_CXX_FLIGUS "${CMAKE_CXX_FLIGUS} ${CFLIGUS} ${SIMFLIGUS} ${LIBFLIGUS}")
