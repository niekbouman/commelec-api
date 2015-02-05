#ifndef REALEXPRCONV_HPP
#define REALEXPRCONV_HPP

// Niek J. Bouman (EPFL, 2015)
//
// Convenience function for (type-safe) definition of a RealExpr'ession 
// in a Commelec Advertisement
//

#include <string>
#include <capnp/message.h>
#include "schema.capnp.h"
#include "polynomial-convenience.hpp"

//#include<iostream> 
// some macros for repetitive stuff

#define BINOP(name) struct name { \
  void setOp(msg::BinaryOperation::Builder binOp){ \
    binOp.initOperation().set ## name(); \
  } \
};

#define UNOP(name) struct name { \
  void setOp(msg::UnaryOperation::Builder unaryOp){ \
    unaryOp.initOperation().set ## name(); \
  } \
};

#define UNOPFUN(name,type) template<typename TypeA> \
UnaryOp<TypeA,type> name(TypeA a) \
{ \
  return UnaryOp<TypeA,type>(a); \
}

#define BINOPFUN(name,type) template<typename TypeA, typename TypeB> \
BinaryOp<TypeA,TypeB,type> name(TypeA a, TypeB b) \
{ \
  return BinaryOp<TypeA,TypeB,type>(a,b); \
}

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
UNOP(Sgn)

// instantiate classes for binary operations
BINOP(Prod)
BINOP(Sum)
BINOP(LessEqThan)
BINOP(GreaterThan)
BINOP(Min)
BINOP(Max)

// Every expression is a RealExprBase
template<typename Kind,typename... CtorArgs>
struct RealExprBase : public Kind 
{
  RealExprBase(CtorArgs... args)
  : Kind(args...) {}
};

struct _Ref {
  // Symbolic variable, for example Ref X("X")
  _Ref(const std::string &var) : _varname(var) {}
  const std::string &getName() const { return _varname; }
  void build(msg::RealExpr::Builder realExpr) {
    realExpr.setReference(_varname);
  }
private:
  std::string _varname;
};
using Ref = RealExprBase<_Ref, const std::string &>;

template <typename Expr>
struct _Name {
  // can be used to name a (sub-) expression
  _Name(const Ref &ref , Expr ex) : _name(ref.getName()), _expr(ex) {}
  void build(msg::RealExpr::Builder realExpr) {
    realExpr.setName(_name);
    _expr.build(realExpr);
  }
private:
  Expr _expr;
  std::string _name;
};

template <typename Expr>
using Name = RealExprBase<_Name<Expr>,const Ref&, Expr>;

template<typename Expr>
Name<Expr> name(const Ref& ref, Expr ex)
{
  return Name<Expr>(ref,ex);
}

// a special unary
template <typename ExprA,typename ExprB>
struct _Pow {
  // can be used to name a (sub-) expression
  _Pow(ExprA base, ExprB exponent) : _base(base),_exponent(exponent) {}
  void build(msg::RealExpr::Builder realExpr) {
    auto unaryop = realExpr.initUnaryOperation();
    _base.build(unaryop.initArg());
    _exponent.build(unaryop.initOperation().initPow());
  }
private:
  ExprA _base;
  ExprB _exponent;
};

template <typename ExprA,typename ExprB>
using Pow = RealExprBase<_Pow<ExprA,ExprB>,ExprA, ExprB>;

template<typename ExprA,typename ExprB>
Pow<ExprA,ExprB> pow(ExprA base, ExprB exp)
{
  return Pow<ExprA,ExprB>(base,exp);
}

struct _Real {
  _Real(double val): _value(val){}
  void setValue(double val)  {
    _value=val;
  }
  void build(msg::RealExpr::Builder realExpr) {
    realExpr.setReal(_value);
  }
private:
  double _value=0.0;
};
using Real = RealExprBase<_Real,double>;

struct _Polynomial {
  _Polynomial(MonomialSum m) : _expr(m) {}
  void build(msg::RealExpr::Builder realExpr) {
    buildPolynomial(realExpr.getPolynomial(),_expr);
  }
private:
  MonomialSum _expr;
};
using Polynomial = RealExprBase<_Polynomial, MonomialSum>;

template <typename TypeA, typename Operation> 
struct _UnaryOp : public Operation {
  _UnaryOp(TypeA a) : argA(a) {}
  void build(msg::RealExpr::Builder realExpr) {
    auto unaryop = realExpr.initUnaryOperation();
    Operation::setOp(unaryop);    
    argA.build(unaryop.initArg());
  }
private:
  TypeA argA;
};
template <typename Type, typename Operation>
using UnaryOp = RealExprBase<_UnaryOp<Type,Operation>,Type>;

template <typename TypeA, typename TypeB, typename Operation>
struct _BinaryOp : public Operation {
  _BinaryOp(TypeA a, TypeB b) : argA(a), argB(b) {}
  void build(msg::RealExpr::Builder realExpr) {
    auto binop = realExpr.initBinaryOperation();
    Operation::setOp(binop);  
    argA.build(binop.initArgA());
    argB.build(binop.initArgB());
  }
private:
  TypeA argA;
  TypeB argB;
};
template <typename TypeA, typename TypeB, typename Operation>
using BinaryOp = RealExprBase<_BinaryOp<TypeA,TypeB,Operation>,TypeA,TypeB>;

// instantiate convenience functions for using the unary operation classes 
UNOPFUN(exp,Exp)
UNOPFUN(sin,Sin)
UNOPFUN(cos,Sin)
UNOPFUN(sqrt,Sqrt)
UNOPFUN(log10,Log10)
UNOPFUN(ln,Ln)
UNOPFUN(round,Round)
UNOPFUN(floor,Floor)
UNOPFUN(ceil,Ceil)
UNOPFUN(abs,Abs)
UNOPFUN(sign,Sgn)

// instantiate convenience functions for using the binary operation classes 
BINOPFUN(max,Max)
BINOPFUN(min,Min)

// define +,-,*,/ operators that bind only to RealExprBase expressions
template<typename TypeA,typename TypeB, typename... CtorArgsA, typename... CtorArgsB>
BinaryOp<RealExprBase<TypeA,CtorArgsA...>, RealExprBase<TypeB,CtorArgsB...>, Sum> 
operator+(RealExprBase<TypeA,CtorArgsA...> a, RealExprBase<TypeB,CtorArgsB...> b) { 
  return BinaryOp<RealExprBase<TypeA,CtorArgsA...>,RealExprBase<TypeB,CtorArgsB...>,Sum>(a, b); 
} 
template<typename TypeA,typename TypeB, typename... CtorArgsA, typename... CtorArgsB>
BinaryOp<RealExprBase<TypeA,CtorArgsA...>, RealExprBase<TypeB,CtorArgsB...>, Sum> 
operator-(RealExprBase<TypeA,CtorArgsA...> a, RealExprBase<TypeB,CtorArgsB...> b) { 
  return BinaryOp<RealExprBase<TypeA,CtorArgsA...>,RealExprBase<TypeB,CtorArgsB...>,Sum>(a, UnaryOp<RealExprBase<TypeB,CtorArgsB...>,Negate>(b)); 
} 
template<typename TypeA,typename TypeB, typename... CtorArgsA, typename... CtorArgsB>
BinaryOp<RealExprBase<TypeA,CtorArgsA...>, RealExprBase<TypeB,CtorArgsB...>, Prod> 
operator*(RealExprBase<TypeA,CtorArgsA...> a, RealExprBase<TypeB,CtorArgsB...> b) { 
  return BinaryOp<RealExprBase<TypeA,CtorArgsA...>,RealExprBase<TypeB,CtorArgsB...>,Prod>(a, b); 
} 
template<typename TypeA,typename TypeB, typename... CtorArgsA, typename... CtorArgsB>
BinaryOp<RealExprBase<TypeA,CtorArgsA...>, RealExprBase<TypeB,CtorArgsB...>, Sum> 
operator/(RealExprBase<TypeA,CtorArgsA...> a, RealExprBase<TypeB,CtorArgsB...> b) { 
  return BinaryOp<RealExprBase<TypeA,CtorArgsA...>,RealExprBase<TypeB,CtorArgsB...>,Sum>(a, UnaryOp<RealExprBase<TypeB,CtorArgsB...>,MultInv>(b)); 
} 

// toplevel build function
template<typename Type,typename... CtorArgs>
void buildRealExpr(msg::RealExpr::Builder realExpr,RealExprBase<Type,CtorArgs...> e) {
  e.build(realExpr);
}

} // namespace cv

#endif
