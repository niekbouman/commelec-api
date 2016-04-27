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
#ifndef REALEXPRCONV_HPP
#define REALEXPRCONV_HPP

// Niek J. Bouman (EPFL, 2015)
// 
/*! \file
 * \brief Convenience function for (type-safe) definition of a RealExpr'ession in a Commelec Advertisement
 * 
 * Real Expressions are used to encode, for example, a cost function
 */

#include <string>
#include <capnp/message.h>
#include "schema.capnp.h"
#include "polynomial-convenience.hpp"

// some macros for repetitive stuff
#define BINOP(name) struct name { \
  void setOp(msg::BinaryOperation::Builder binOp) const { \
    binOp.initOperation().set ## name(); \
  } \
};

#define UNOP(name) struct name { \
  void setOp(msg::UnaryOperation::Builder unaryOp) const { \
    unaryOp.initOperation().set ## name(); \
  } \
};

#define UNOPFUN(name,type) template<typename TypeA> \
Expr<UnaryOp<Expr<TypeA>,type>> name(Expr<TypeA> a) \
{ \
  return Expr<UnaryOp<Expr<TypeA>,type>>(a); \
}

#define BINOPFUN(name,type) template<typename TypeA, typename TypeB> \
Expr<BinaryOp<Expr<TypeA>,Expr<TypeB>,type>> name(Expr<TypeA> a, Expr<TypeB> b) \
{ \
  return Expr<BinaryOp<Expr<TypeA>,Expr<TypeB>,type>>(a,b); \
}

/**
 * Convenience function namespace
 */
namespace cv {

// instantiate classes for unary operations
UNOP(Negate)
UNOP(Exp)
UNOP(Sin)
UNOP(Cos)
UNOP(Square)
UNOP(Sqrt)
UNOP(Log10)
UNOP(Ln)
UNOP(Round)
UNOP(Floor)
UNOP(Ceil)
UNOP(MultInv)
UNOP(Abs)
UNOP(Sign)

// instantiate classes for binary operations
BINOP(Sum)
BINOP(Prod)
BINOP(Pow)
BINOP(Min)
BINOP(Max)
BINOP(LessEqThan)
BINOP(GreaterThan)

// define +,-,*,/ operators that bind only to RealExprBase expressions

template <typename T> class Expr {};



class _Ref {};
template <> class Expr<_Ref> {
  // Used to refer to named RealExpr- or SetExpr-essions, for example Ref a("a")
  const std::string _refname;

public:
  Expr(const std::string &ref) : _refname(ref) {}
  const std::string &getName() const { return _refname; }
  void build(msg::RealExpr::Builder realExpr) const {
    realExpr.setReference(_refname);
  }
};

/**
 * Reference type
 *
 * This type is used in the ::name function, to give a name to an expression, in
 * order to be able to refer back to that expression at another place.
 *
 * See the ::name function for more information.
 */
using Ref = Expr<_Ref>;

class _Var {};
template <> class Expr<_Var> {
  // Symbolic variable, for example Var X("X")
  const std::string _varname;

public:
  Expr(const std::string &var) : _varname(var) {}
  const std::string &getName() const { return _varname; }
  void build(msg::RealExpr::Builder realExpr) const  {
    realExpr.setVariable(_varname);
  }
};

/**
Symbolic variable type

This type is used to declare a variable, and to set the label that will be used to encode the variable is the serialized message.

Example:
~~~~{.cpp}
using namespace cv;
Var x("X"); // Creates symbolic variable "x" that can be used in expressions. The variable will be labeled as "X" in the serialized message.
~~~~

See the Expr<_Var> class for details about the constructor.
*/
using Var = Expr<_Var>;

template <typename T> class _Name {};
template <typename T> class Expr<_Name<Expr<T>>> {
  // can be used to name a (sub-) expression
  const std::string _name;
  Expr<T> _expr;

public:
  Expr(const Ref &ref, const Expr<T>& ex) : _name(ref.getName()), _expr(ex) {}
  void build(msg::RealExpr::Builder realExpr) const  {
    realExpr.setName(_name);
    _expr.build(realExpr);
  }
};

/**
Bind a reference to an expression
 
This function is used to give a name to an expression, in order to be able to refer back to that expression at another place 

Example:
~~~~{.cpp}
    namespace cv;
    using msg::RealExpr;

    ::capnp::MallocMessageBuilder mesg, mesg2;

    auto builder = mesg.initRoot<RealExpr>();
    auto builder2 = mseg2.initRoot<RealExpr>();

    Ref named_sum("a");
    Var x("X");

    buildRealExpr(builder, name(named_sum, Real(3) + x));
    buildRealExpr(builder2, Real(5) * named_sum); // equivalent to 5 * (3 + x)
~~~~
*/
template <typename T>
Expr<_Name<Expr<T>>> name(const Ref &ref, const Expr<T> &ex) {
  return Expr<_Name<Expr<T>>>(ref, ex);
}

class _Real {};
template <> class Expr<_Real> {
  double _value = 0.0;

public:
  Expr(double val) : _value(val) {}
  void setValue(double val) { _value = val; }
  void build(msg::RealExpr::Builder realExpr) const  { realExpr.setReal(_value); }
};

/**
Numeric (double) constant type

This type is used to declare a numeric constant, which is internally represented as a double (a double-precision floating-point number).

Example:
~~~~{.cpp}
using namespace cv;
Real c(3.14); 
~~~~

See the Expr<_Real> class for details about the constructor.
*/
using Real = Expr<_Real>;

class _Polynomial {};
template <> class Expr<_Polynomial> {
  MonomialSum _expr;

public:
  Expr(const MonomialSum& m) : _expr(m) {}
  void build(msg::RealExpr::Builder realExpr) const  {
    buildPolynomial(realExpr.getPolynomial(), _expr);
  }
};

using Polynomial = Expr<_Polynomial>;

template <typename TypeA, typename Operation> class UnaryOp {};
template <typename T, typename Operation>
class Expr<UnaryOp<Expr<T>, Operation>> : public Operation {
  Expr<T> arg;

public:
  Expr(const Expr<T> &a) : arg(a) {}
  void build(msg::RealExpr::Builder realExpr) const  {
    auto unaryop = realExpr.initUnaryOperation();
    Operation::setOp(unaryop);
    arg.build(unaryop.initArg());
  }
};

// template specialization
template <typename TypeA, typename TypeB, typename Operation> class BinaryOp {};
template <typename T1, typename T2, typename Operation>
class Expr<BinaryOp<Expr<T1>, Expr<T2>, Operation>> : public Operation {
  Expr<T1> argA;
  Expr<T2> argB;

public:
  Expr(const Expr<T1> &a, const Expr<T2> &b) : argA(a), argB(b) {}
  void build(msg::RealExpr::Builder realExpr) const  {
    auto binop = realExpr.initBinaryOperation();
    Operation::setOp(binop);
    argA.build(binop.initArgA());
    argB.build(binop.initArgB());
  }
};

// instantiate convenience functions for using the unary operation classes 
UNOPFUN(exp,Exp)
UNOPFUN(sin,Sin)
UNOPFUN(cos,Sin)
UNOPFUN(sqrt,Sqrt)
UNOPFUN(square,Square)
UNOPFUN(log10,Log10)
UNOPFUN(ln,Ln)
UNOPFUN(round,Round)
UNOPFUN(floor,Floor)
UNOPFUN(ceil,Ceil)
UNOPFUN(abs,Abs)
UNOPFUN(sign,Sign)

// instantiate convenience functions for using the binary operation classes 
BINOPFUN(max,Max)
BINOPFUN(min,Min)
BINOPFUN(pow,Pow)


// binary operators
template<typename TypeA,typename TypeB>
Expr<BinaryOp<Expr<TypeA>,Expr<TypeB>,Sum>>
operator+(Expr<TypeA> a, Expr<TypeB> b) { 
  return Expr<BinaryOp<Expr<TypeA>,Expr<TypeB>,Sum>>(a,b);
}
template<typename TypeA,typename TypeB>
Expr<BinaryOp<Expr<TypeA>,Expr<UnaryOp<Expr<TypeB>,Negate>>,Sum>>
operator-(Expr<TypeA> a, Expr<TypeB> b) { 
  return Expr<BinaryOp<Expr<TypeA>,Expr<UnaryOp<Expr<TypeB>,Negate>>,Sum>>(a,Expr<UnaryOp<Expr<TypeB>,Negate>>(b));
}
template<typename TypeA,typename TypeB>
Expr<BinaryOp<Expr<TypeA>,Expr<TypeB>,Prod>>
operator*(Expr<TypeA> a, Expr<TypeB> b) { 
  return Expr<BinaryOp<Expr<TypeA>,Expr<TypeB>,Prod>>(a,b);
}
template<typename TypeA,typename TypeB>
Expr<BinaryOp<Expr<TypeA>,Expr<UnaryOp<Expr<TypeB>,MultInv>>,Prod>>
operator/(Expr<TypeA> a, Expr<TypeB> b) { 
  return Expr<BinaryOp<Expr<TypeA>,Expr<UnaryOp<Expr<TypeB>,MultInv>>,Prod>>(a,Expr<UnaryOp<Expr<TypeB>,MultInv>>(b));
}

// toplevel build function

 
/** 
Function to build a RealExpr

Example:
Here we show how we can encode the expression (3.0 * x)^2, where x is a symbolic variable, encoded in the message as the string literal "X"

~~~~{.cpp}
    namespace cv;

    ::capnp::MallocMessageBuilder mesg;

    auto builder = mesg.initRoot<msg::RealExpr>();

    Var x("X"); // this links the name "x" (used in the C++ code) to the string "X", which will be used as a label for the variable in the serialized message

    buildRealExpr(builder, pow(Real(3) * x, Real(2)));
~~~~

*/
template<typename T> void buildRealExpr(msg::RealExpr::Builder realExpr,Expr<T> e) {
  e.build(realExpr);
}

} // namespace cv

#endif
