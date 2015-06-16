cmake_minimum_required (VERSION 2.6)
project (CommelecAPI)

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  set(MACOSX true)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
  set(LINUX true)
endif()

if(MACOSX)
  set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")
  # on macosx we link to libc++

  set(CMAKE_MACOSX_RPATH 1)
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  # for if XCode is used
elseif(LINUX)
  set(CMAKE_CXX_FLAGS "-std=c++11") 
  set (EXTRA_LIBS ${EXTRA_LIBS} pthread)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
#set(CMAKE_CXX_FLAGS "-std=c++11 -DASIO_STANDALONE ${ADDITIONAL_CXX_FLAGS}") 

#option(ASIO_STANDALONE True)

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

find_package(Boost COMPONENTS system filesystem coroutine context REQUIRED)
# the order of the libs matters for linking!
include_directories(${Boost_INCLUDE_DIR})
link_directories (${Boost_LIBRARY_DIRS})

include_directories ("${PROJECT_SOURCE_DIR}") 
include_directories ("${PROJECT_SOURCE_DIR}/externals/rapidjson/include") 
include_directories ("${PROJECT_SOURCE_DIR}/externals/spdlog/include") 

add_subdirectory(commelec-api)
