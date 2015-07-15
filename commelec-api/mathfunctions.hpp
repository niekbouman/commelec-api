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
#ifndef MATHFUNCTIONS_H
#define MATHFUNCTIONS_H

#include <Eigen/Dense>
#include <Eigen/Core>
#include <capnp/message.h>
#include <commelec-api/schema.capnp.h>

std::tuple<bool,int,int> 
check_capnp_matrix(capnp::List<capnp::List<msg::RealExpr>>::Reader matrix); 
// check dimensions of capnp matrix

void initFromEigen(const Eigen::MatrixXd &eigen_matrix,
                   capnp::List<capnp::List<msg::RealExpr>>::Builder capnp_mat);
void initFromEigen(const Eigen::VectorXd &eigen_vec,
                   capnp::List<msg::RealExpr>::Builder capnp_vec);
// from an Eigen vector type to capnp

#endif
