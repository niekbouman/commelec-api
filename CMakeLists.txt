cmake_minimum_required (VERSION 2.6)
project (CommelecAPI)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_CXX_FLAGS "-std=c++11 ${ADDITIONAL_CXX_FLAGS}") 
set(CMAKE_SHARED_LINKER_FLAGS "-static-libgcc -static-libstdc++ ${ADDITIONAL_LINKER_FLAGS}")

#set(CAPNP_LIBRARIES capnp kj)
find_package(Eigen3 REQUIRED)
find_package(CapnProto 0.5 REQUIRED)
include_directories(${CAPNP_INCLUDE_DIRS})
add_definitions(${CAPNP_DEFINITIONS})


# compile the Cap'n Proto schema automatically -> currently commented out because does not work well
#set(CAPNPC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS ${PROJECT_SOURCE_DIR}/src/schema.capnp)
#set(CAPNP_SRCS ${PROJECT_SOURCE_DIR}/messaging/schema.capnp.c++)
#set(CAPNP_HDRS ${PROJECT_SOURCE_DIR}/messaging/schema.capnp.h)


add_subdirectory(src)
