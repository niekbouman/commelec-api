cmake_minimum_required (VERSION 2.6)
project (CommelecAPI)

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  set(MACOSX true)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
  set(LINUX true)
endif()

if(MACOSX)
  set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++ -Wall")
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

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  set(CMAKE_SHARED_LINKER_FLAGS "${ADDITIONAL_LINKER_FLAGS}")
else()
  set(CMAKE_SHARED_LINKER_FLAGS "-static-libgcc -static-libstdc++ ${ADDITIONAL_LINKER_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ ${ADDITIONAL_LINKER_FLAGS}")
endif()

find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR})

find_package(CapnProto 0.5 REQUIRED)
include_directories(${CAPNP_INCLUDE_DIRS})
add_definitions(${CAPNP_DEFINITIONS})

#set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS system filesystem coroutine context REQUIRED)
# the order of the libs matters for linking!
include_directories(${Boost_INCLUDE_DIR})
link_directories (${Boost_LIBRARY_DIRS})

include_directories ("${PROJECT_SOURCE_DIR}") 
include_directories ("${PROJECT_SOURCE_DIR}/externals/rapidjson/include") 
include_directories ("${PROJECT_SOURCE_DIR}/externals/spdlog/include") 
include_directories ("${PROJECT_SOURCE_DIR}/externals") 

add_subdirectory(commelec-api)
# commelec-api compiles the Cap'n Proto schema automatically
set(CAPNP_SRCS "${CAPNP_SRCS}")
set(CAPNP_HDRS "${CAPNP_HDRS}")
set_source_files_properties(${CAPNP_SRCS} PROPERTIES GENERATED TRUE)
# redefine with correct scope

add_subdirectory(externals/seidel_lp)
add_subdirectory(commelec-interpreter)
add_subdirectory(utils)

# Testing
add_subdirectory(tests)
enable_testing()
add_test(interpreter_test tests/interpreter_test)
