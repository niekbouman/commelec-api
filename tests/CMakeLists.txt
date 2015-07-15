add_executable(interpreter_test interpreter.cpp ${CAPNP_SRCS})
target_link_libraries (interpreter_test ${CAPNP_LIBRARIES} seidel hlapi cl_interpreter) 
