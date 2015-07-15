// Function to compute bounding box of a convex polytope
// Niek Bouman / Andrey Bernstein
//
// solve_lp2 re-uses code from Michael Hohmeyer, for which
// license below applies:
//
// Copyright (c) 1990 Michael E. Hohmeyer, 
//    hohmeyer@icemcfd.com
//
// Permission is granted to modify and re-distribute this code in any manner
// as long as this notice is preserved.  All standard disclaimers apply.

#ifndef BBPOLYTOPE_HPP
#define BBPOLYTOPE_HPP

#include <vector>
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Geometry>

std::vector<double> solve_lp2(const std::vector<double> &_halves,
                              const std::vector<double> &c);

template <typename DerivedA, typename DerivedB>
Eigen::AlignedBoxXd
computeAABBConvexPolytope(const Eigen::MatrixBase<DerivedA> &A,
                          const Eigen::MatrixBase<DerivedB> &b) {
  auto dim = 2;
  auto numConstraints = A.rows();

  std::vector<double> halves(numConstraints * (dim + 1));

  // overlay with Eigen matrix map
  Eigen::Map<
      Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
      eigHalves(halves.data(), numConstraints, dim + 1);

  // concat A with b
  eigHalves << A, b;

  // normalize A|b
  auto sz = eigHalves.rows();
  for (auto i = 0; i < sz; ++i)
    eigHalves.row(i).normalize();

  // solve the four linear programs to find, respectively, max x1, min x1, max
  // x2, min x2. (for the variable x=(x1,x2))
  // subject to Ax=b
  auto sol = solve_lp2(halves, {1, 0});
  auto xmax = -sol[0];
  auto sol2 = solve_lp2(halves, {-1, 0});
  auto xmin = -sol2[0];
  auto sol3 = solve_lp2(halves, {0, 1});
  auto ymax = -sol3[1];
  auto sol4 = solve_lp2(halves, {0, -1});
  auto ymin = -sol4[1];

  Eigen::Vector2d minvec;
  minvec << xmin, ymin;
  Eigen::Vector2d maxvec;
  maxvec << xmax, ymax;
  return Eigen::AlignedBoxXd(minvec, maxvec);
}

#endif
