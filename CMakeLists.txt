cmake_minimum_required (VERSION 2.6)
project (CommelecAPI)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_CXX_FLAGS "-std=c++11 ${ADDITIONAL_CXX_FLAGS}") 

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  set(CMAKE_SHARED_LINKER_FLAGS "${ADDITIONAL_LINKER_FLAGS}")
else()
  set(CMAKE_SHARED_LINKER_FLAGS "-static-libgcc -static-libstdc++ ${ADDITIONAL_LINKER_FLAGS}")
endif()


find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR})

find_package(CapnProto 0.5 REQUIRED)
include_directories(${CAPNP_INCLUDE_DIRS})
add_definitions(${CAPNP_DEFINITIONS})

find_package(Boost COMPONENTS system REQUIRED)
include_directories(${Boost_INCLUDE_DIR})
link_directories (${Boost_LIBRARY_DIRS})


add_subdirectory(src)
