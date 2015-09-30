set_source_files_properties(${CAPNP_SRCS} PROPERTIES GENERATED TRUE)

add_executable(viz visualization_server.cpp ${CAPNP_SRCS})
target_link_libraries (viz ${EXTRA_LIBS} ${Boost_LIBRARIES} ${CAPNP_LIBRARIES} cl_interpreter hlapi_static seidel  ) 
