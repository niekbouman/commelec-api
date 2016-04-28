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
#ifndef POLYNOMIALCONV_HPP
#define POLYNOMIALCONV_HPP

// Niek J. Bouman (EPFL, 2014)
//
/*! \file
 * \brief Convenience function for (type-safe) definition of a polynomial in a Commelec Advertisement
 * 
 * Although the polynomial type is used in the high-level API, they are deprecated, because 
 * it is also possible to encode a polynomial using a RealExpr.
*/

// Example:
//
//  to initialize a cost function as the polynomial f(P,Q) = 4 + 4 P + 5 P^3 Q^2,
//  one does the following:
//
//    ::capnp::MallocMessageBuilder message;
//    auto adv = message.initRoot<Advertisement>();
//    auto cf = adv.getCostFunction();
//
//    PolyVar P {"P"};
//    PolyVar Q {"Q"};
//
//    BuildPolynomial(cf.initPolynomial(), 4 + 4*P + 5*(P^3|Q^2));

#include <string>
#include <vector>
#include <algorithm>
#include <iterator>
#include <set>
#include <cmath>
#include <capnp/message.h>
#include "schema.capnp.h"

namespace cv {


struct PolyVar {
  // Symbolic variable, for example PolyVar X{"X"}
  PolyVar(const std::string &var) : _varname(var) {}
  const std::string &getName() { return _varname; }

private:
  std::string _varname;
};

struct Power {
  // PolyVar raised to some power (for example, X^3)
  PolyVar var;
  int pow;
  // TODO think about this: are negative powers allowed?
};

struct Monomial {
  Monomial(PolyVar v) : coeff(1.0) { prodOfPowers.push_back(Power{v, 1}); }
  // Constructor to support automatic conversion from "X" -> "X^1"

  Monomial(double c = 1.0) : coeff(c) {}
  // Constructor to support constants

  friend Monomial operator|(Monomial monom_a, Monomial monom_b);
  friend Monomial operator*(double c, Monomial m);
  friend Monomial operator^(PolyVar var, int power);

  int maxVarDegree() const {
    // compute maxVarDegree from a monomial
    auto max_el = std::max_element(
        prodOfPowers.begin(), prodOfPowers.end(),
        [](const Power &a, const Power b) { return a.pow < b.pow; });

    if (max_el != prodOfPowers.end())
      return max_el->pow;
    else
      return 0;
  }

  double getCoeff() { return coeff; }
  // the constant in from of the powers of the indeterminates

  const std::vector<Power> &getPowList() { return prodOfPowers; }
  // getter for internal thing (overkill?)

private:
  double coeff;
  std::vector<Power> prodOfPowers;
};

inline Monomial operator|(Monomial monom_a, Monomial monom_b) {
  // operator for concatenating powers, for example (X^3|Y^4)  (parentheses are
  // necessary)
  Monomial m(monom_a);
  m.prodOfPowers.insert(m.prodOfPowers.end(), monom_b.prodOfPowers.begin(),
                        monom_b.prodOfPowers.end());
  return m;
}

inline Monomial operator^(PolyVar var, int power) {
  Monomial m;
  m.prodOfPowers.push_back(Power{var, power});
  return m;
}

inline Monomial
operator*(double c, Monomial m) {
  m.coeff *= c;
  return m;
}

struct MonomialSum {
  friend MonomialSum operator+(Monomial a, Monomial b);
  friend MonomialSum operator+(MonomialSum s, Monomial a);

  int maxVarDegree() {
    auto max_el = std::max_element(m.begin(), m.end(),
                                   [](const Monomial &a, const Monomial b) {
      return a.maxVarDegree() < b.maxVarDegree();
    });
    if (max_el != m.end())
      return max_el->maxVarDegree();
    else
      return 0;
  }

  std::vector<std::string> variables() {
    // return list of variables occurring in the polynomial
    std::set<std::string> vars;
    for (auto monom : m)
      for (auto power : monom.getPowList())
        vars.insert(power.var.getName());
    return std::vector<std::string>(vars.begin(), vars.end());
  }

  int numTerms() { return m.size(); }
  const std::vector<Monomial> &getMonomials() { return m; }

private:
  std::vector<Monomial> m;
};

inline MonomialSum operator+(Monomial a, Monomial b) {
  MonomialSum s;
  s.m.push_back(a);
  s.m.push_back(b);
  return s;
}

inline MonomialSum operator+(MonomialSum s, Monomial a) {
  s.m.push_back(a);
  return s;
}

/**
Function to easily describe a uni- or multivariate polynomial over the reals [deprecated]

This functionality is deprecated, because the same behavior can be obtained using RealExpr-essions.
Nonetheless, we include a description because the Polynomial type is used in the high-level API.

The basic idea is that one first declares the indeterminates (using the PolyVar type), and then 
build the polynomial expression.

Let us start by encoding a univariate second-degree polynomial in x, for example: 3 x^2 + 2 x + 5 

~~~~{.cpp}
namespace cv;
using msg::RealExpr;

::capnp::MallocMessageBuilder mesg;

auto expr = mesg.initRoot<RealExpr>();

PolyVar x("X");

buildRealExpr(expr.initPolynomial(), 3 * (x^2) + 2 * x + 5));
~~~~  
Note the parenthesis, which are mandatory, around the monomial x^2.

To create a multivariate polynomial, one uses the | operator.
Let us demonstrate this by writing the polynomial 2 x^2 y^3 + 3 x y,

~~~~{.cpp}
namespace cv;
using msg::RealExpr;

::capnp::MallocMessageBuilder mesg;

auto expr = mesg.initRoot<RealExpr>();

PolyVar x("X");
PolyVar y("Y");

buildRealExpr(expr.initPolynomial(),  2 * (x^2|y^3) + 3 * (x|y));
~~~~  

*/

inline void buildPolynomial(msg::Polynomial::Builder poly,MonomialSum m) {
  // initialize polynomial in capnp message based on
  // a MonomialSum expression (which is easy to enter by hand and type-safe)

  auto vars = m.variables();

  // copy vars into capnproto variables
  auto variables = poly.initVariables(vars.size());
  int i = 0;
  for (auto var : vars) {
    variables.set(i, var);
    ++i;
  }

  auto maxvar = m.maxVarDegree();

  // set maxvardegree
  poly.setMaxVarDegree(maxvar);

  // write collection of coefficients: {(offset,value)}
  auto coeffs = poly.initCoefficients(m.numTerms());
  double d = maxvar + 1;
  int varIndex = 0;
  for (auto monom : m.getMonomials()) {
    coeffs[varIndex].setValue(monom.getCoeff());
    // set coefficient

    // compute offset using an ordering on the monomials that depends on the
    // order of the variables (ordered alphabetically, by std::set) and
    // maxVarDegree
    int offset = 0;
    for (auto pow : monom.getPowList()) {
      // use ordering of variables defined by vars
      auto it = std::find(vars.begin(), vars.end(), pow.var.getName());
      if (it != vars.end()) {
        int i = std::distance(vars.begin(), it);
        offset += pow.pow * int(std::pow(d, i));
      } else
        throw;
    }
    coeffs[varIndex].setOffset(offset);
    ++varIndex;
  }
}
}
#endif
