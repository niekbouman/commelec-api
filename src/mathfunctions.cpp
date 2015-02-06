#include "mathfunctions.hpp"

using namespace msg;

void initFromEigen(const Eigen::MatrixXd &eigen_matrix,
                   capnp::List<capnp::List<msg::RealExpr>>::Builder capnp_mat) {
  auto rows = eigen_matrix.rows(), cols = eigen_matrix.cols();
  for (auto i = 0; i < rows; ++i) {
    auto row = capnp_mat.init(i, cols);
    for (auto j = 0; j < cols; ++j)
      row[j].setReal(eigen_matrix(i, j));
  }
}

void initFromEigen(const Eigen::VectorXd &eigen_vec,
                   capnp::List<msg::RealExpr>::Builder capnp_vec) {
  auto sz = eigen_vec.size();
  for (auto i = 0; i < sz; ++i) {
    capnp_vec[i].setReal(eigen_vec(i));
  }
}
