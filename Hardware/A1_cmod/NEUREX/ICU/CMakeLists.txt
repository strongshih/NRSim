file(GLOB ICU_SOURCES "*.cpp")
file(GLOB ICU_HEADERS "*.h")



# exclude files including tb_ from sources
list(FILTER ICU_SOURCES EXCLUDE REGEX "tb_")

include_directories(../include)

add_executable(sim_ICU testbench.cpp ${ICU_SOURCES} ${ICU_HEADERS})

set(CMAKE_CXX_FLICUS "${CMAKE_CXX_FLICUS} ${CFLICUS} ${SIMFLICUS} ${LIBFLICUS}")
