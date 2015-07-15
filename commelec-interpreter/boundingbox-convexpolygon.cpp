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

#include <commelec-interpreter/boundingbox-convexpolygon.hpp>
#include <seidel_lp/lp.h>
#include <ctime>
#include <stdexcept>

std::vector<double> solve_lp2(const std::vector<double> &_halves,
                              const std::vector<double> &c) {

  // the trivial halfspace (0,0,1) should be at position 0 in halves
  std::vector<double> halves{0, 0, 1};
  halves.insert(halves.end(), _halves.begin(), _halves.end());

  // setup n_vec and d_vec for linear programming
  std::vector<double> n_vec(c);
  n_vec.push_back(0);
  std::vector<double> d_vec = {0, 0, 1};

  auto m = halves.size() / 3;
  auto d = 2;

  std::vector<int> next(m);
  std::vector<int> prev(m);
  std::vector<int> perm(m - 1);
  std::vector<double> opt(d + 1);
  std::vector<double> work((m + 3) * (d + 2) * (d - 1) / 2);

  srand(time(NULL));            // needed?
  randperm(m - 1, perm.data()); // randomize the input planes
  prev[0] = 1234567890;         // previous to 0 should never be used
  next[0] = perm[0] + 1;        // link the zero position in at the beginning
  prev[perm[0] + 1] = 0;        // link the other planes
  for (int i = 0; i < m - 2; i++) {
    next[perm[i] + 1] = perm[i + 1] + 1;
    prev[perm[i + 1] + 1] = perm[i] + 1;
  }
  // flag the last plane
  next[perm[m - 2] + 1] = m;
  auto status = linprog(halves.data(), 0, m, n_vec.data(), d_vec.data(), d,
                        opt.data(), work.data(), next.data(), prev.data(), m);

  switch (status) {
  case INFEASIBLE:
    //(void)printf("no feasible solution\n");
    throw std::runtime_error("error in computing bounding box: LP infeasible");
  // break;
  case MINIMUM:
  case AMBIGUOUS:
    //  ambiguous occurs for example if we minimize x and the solution contains
    //  all points on a line-segment that is parallel to the y axis
    if (opt[d] != 0.0)
      return {opt[0] / opt[d], opt[1] / opt[d]};
    else
      // solution at infinity?
      throw std::runtime_error(
          "error in computing bounding box: LP solution at infinity?");
  case UNBOUNDED:
    throw std::runtime_error(
        "error in computing bounding box: LP region is unbounded");
  default:
    throw std::runtime_error(
        "error in computing bounding box: unknown problem with LP solver");
  }
}
