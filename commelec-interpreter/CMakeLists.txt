add_library(cl_interpreter 
  adv-interpreter-eval.cpp  adv-interpreter-proj.cpp  adv-interpreter.cpp
  adv-interpreter-membership.cpp  adv-interpreter-recthull.cpp  boundingbox-convexpolygon.cpp
  ../commelec-api/mathfunctions.cpp ${CAPNP_SRCS})
  
target_link_libraries(cl_interpreter ${CAPNP_LIBRARIES} seidel)

#install (TARGETS messaging DESTINATION lib)
#install (FILES AdvFunc.hpp PolynomialConvenience.hpp DESTINATION include)

