#ifndef POLYTOPE_CONVENIENCE_HPP
#define POLYTOPE_CONVENIENCE_HPP

#include <Eigen/Core>
#include "schema.capnp.h"

namespace cv {
void buildConvexPolytope(Eigen::MatrixXd A, Eigen::VectorXd b,
                         msg::ConvexPolytope::Builder poly);
}
#endif
