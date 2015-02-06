// The MIT License (MIT)
// 
// Copyright (c) 2015 Niek J. Bouman
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// 
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
