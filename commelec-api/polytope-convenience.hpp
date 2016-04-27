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
#ifndef POLYTOPE_CONVENIENCE_HPP
#define POLYTOPE_CONVENIENCE_HPP

#include <Eigen/Core>
#include "schema.capnp.h"

namespace cv {
 
/**
Convenience function to define a convex polytope, described in the half-space respresentation, and stored in Eigen3-datatypes (matrix A and vector b).

Example:
~~~~{.cpp}
using namespace cv;
using msg::SetExpr;

::capnp::MallocMessageBuilder mesg;
auto setexpr = mesg.initRoot<SetExpr>();
    
Eigen::MatrixXd A(3, 2);
A <<    1,  0,
     -0.3,  1, 
     -0.3, -1;

Eigen::VectorXd b(3);
b << 10, 
     0, 
     0;

buildConvexPolytope(A, b, setexpr.initConvexPolytope());
~~~~
*/
void buildConvexPolytope(Eigen::MatrixXd A, Eigen::VectorXd b,
                         msg::ConvexPolytope::Builder poly);
}
#endif
