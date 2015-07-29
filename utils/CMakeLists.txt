add_executable(viz visualization_server.cpp ${CAPNP_SRCS})
target_link_libraries (viz ${EXTRA_LIBS} ${CAPNP_LIBRARIES_LITE} ${Boost_LIBRARIES} hlapi_static seidel cl_interpreter) 
