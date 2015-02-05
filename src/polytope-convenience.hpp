#ifndef POLYTOPE_CONVENIENCE_HPP
#define POLYTOPE_CONVENIENCE_HPP

#include "schema.capnp.h"
#include <eigen3/Eigen/Core>

namespace cv {
void buildConvexPolytope(Eigen::MatrixXd A, Eigen::VectorXd b,
                         msg::ConvexPolytope::Builder poly);
}
#endif
