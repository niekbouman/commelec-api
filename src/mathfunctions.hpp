#ifndef MATHFUNCTIONS_H
#define MATHFUNCTIONS_H

#include <Eigen/Dense>
#include <Eigen/Core>
#include <capnp/message.h>
#include "schema.capnp.h"

void initFromEigen(const Eigen::MatrixXd &eigen_matrix,
                   capnp::List<capnp::List<msg::RealExpr>>::Builder capnp_mat);
void initFromEigen(const Eigen::VectorXd &eigen_vec,
                   capnp::List<msg::RealExpr>::Builder capnp_vec);
// from an Eigen vector type to capnp

#endif
