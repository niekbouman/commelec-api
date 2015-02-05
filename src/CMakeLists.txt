set(hlapi_sources
  hlapi.cpp
  mathfunctions.cpp
  polytope-convenience.cpp
  serialization.cpp
)

add_library(hlapi SHARED ${hlapi_sources} ${CAPNP_SRCS})
add_library(hlapi_static ${hlapi_sources} ${CAPNP_SRCS})
target_link_libraries(hlapi ${CAPNP_LIBRARIES})
target_link_libraries(hlapi_static ${CAPNP_LIBRARIES})

