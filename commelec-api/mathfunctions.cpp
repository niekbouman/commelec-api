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
#include <commelec-api/mathfunctions.hpp>
#include <vector>
#include <set>
#include <tuple>

using namespace msg;

std::tuple<bool,int,int> 
check_capnp_matrix(capnp::List<capnp::List<RealExpr>>::Reader matrix){ 
// verify that each inner list has the same length
// matrix encoding should be row-major

  int rows = matrix.size();  
  std::vector<int> col_sizes{};
  for (auto row: matrix) col_sizes.push_back(row.size());   
  std::set<int> col_sizes_set(col_sizes.cbegin(),col_sizes.cend());
  auto it_max = std::max_element(col_sizes_set.cbegin(),col_sizes_set.cend());
  int cols = (it_max != col_sizes_set.cend()) ? *it_max : 0;
  return std::make_tuple( col_sizes_set.size() != 1, rows, cols);
}

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
