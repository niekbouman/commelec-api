set(hlapi_sources
  hlapi.cpp
  mathfunctions.cpp
  polytope-convenience.cpp
  serialization.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  set(EXTRA_LIBS ws2_32)
endif()

set_source_files_properties(${CAPNP_SRCS} PROPERTIES GENERATED TRUE)
set(CAPNPC_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS schema.capnp)
add_library(hlapi SHARED ${hlapi_sources} ${CAPNP_SRCS})
add_library(hlapi_static ${hlapi_sources} ${CAPNP_SRCS})
target_link_libraries(hlapi ${CAPNP_LIBRARIES_LITE} ${EXTRA_LIBS} ${Boost_LIBRARIES})
target_link_libraries(hlapi_static ${CAPNP_LIBRARIES_LITE} ${EXTRA_LIBS} ${Boost_LIBRARIES})

add_executable(commelecd daemon.cpp json.cpp)
target_link_libraries(commelecd ${EXTRA_LIBS} ${CAPNP_LIBRARIES_LITE} ${Boost_LIBRARIES} hlapi_static)
