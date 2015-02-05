#include "polytope-convenience.hpp"
#include "mathfunctions.h"

void cv::buildConvexPolytope(Eigen::MatrixXd A, Eigen::VectorXd b,
                             msg::ConvexPolytope::Builder poly) {
  auto numConstraints = b.size();
  assert(A.rows() == numConstraints);
  initFromEigen(A, poly.initA(numConstraints));
  initFromEigen(b, poly.initB(numConstraints));
}
