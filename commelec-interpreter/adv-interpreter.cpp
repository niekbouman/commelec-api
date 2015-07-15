#include <commelec-interpreter/adv-interpreter.hpp>
#include <commelec-api/mathfunctions.hpp>

using namespace msg;

AdvFunc::AdvFunc(Advertisement::Reader adv) : _adv(adv)
{
  findReferences();
  // populates _real_expr_refs and _set_expr_refs
}

Eigen::VectorXd AdvFunc::evalToVector(capnp::List<RealExpr>::Reader list){
  Eigen::VectorXd result(list.size());
  auto i=0;
  for (auto el:list){
    result(i) = eval(el);
    ++i;
  }
  return result;
}

Eigen::MatrixXd AdvFunc::evalToMatrix(capnp::List<capnp::List<RealExpr>>::Reader mx){
  bool error;
  int rows, cols;
  std::tie(error, rows, cols) = check_capnp_matrix(mx);
  if (error) {
    // matrix incorrectly specified
    throw;
  }

  Eigen::MatrixXd result(rows,cols);
  auto rw=0;
  for (auto row:mx){
    auto cl=0;
    for (auto el: row)
    {
      result(rw,cl) = eval(el);
      ++cl;
    }
    ++rw;
  }
  return result;
}

Eigen::AlignedBoxXd AdvFunc::rectangularHull(SetExpr::Reader set, const ValueMap &bound_vars){
  _bound_vars = &bound_vars;
  _nesting_depth = 0;
  return rectHull(set);
}


void AdvFunc::findReferences()
{
  _nesting_depth=0;

  if (_adv.hasPQProfile()){
    findReferences(_adv.getPQProfile());
    _nesting_depth=0;
  }
  if (_adv.hasBeliefFunction()){
    findReferences(_adv.getBeliefFunction());
    _nesting_depth=0;
  }
  if (_adv.hasCostFunction()){
    findReferences(_adv.getCostFunction());
    _nesting_depth=0;
  }
}

void AdvFunc::findReferences(RealExpr::Reader expr) {
  ++_nesting_depth;
  if (_nesting_depth > MAX_NESTING_DEPTH) {
    throw EvaluationError("max nesting depth reached");
  }

  if (expr.hasName()) {
    _real_expr_refs[expr.getName()] = expr;
  }

  // traverse children
  switch (expr.which()) {
  case RealExpr::UNARY_OPERATION:
    findReferences(expr.getUnaryOperation().getArg());
    return;
  case RealExpr::BINARY_OPERATION: {
    auto binaryop = expr.getBinaryOperation();
    findReferences(binaryop.getArgA());
    findReferences(binaryop.getArgB());
    return;
  }
  case RealExpr::LIST_OPERATION:
    for (auto real_expr : expr.getListOperation().getArgs())
      findReferences(real_expr);
    return;
  case RealExpr::CASE_DISTINCTION:
    for (auto cs : expr.getCaseDistinction().getCases()) {
      findReferences(cs.getSet());
      findReferences(cs.getExpression());
    }
    return;
  default:
    // case RealExpr::REFERENCE: // maybe gather all names
    //  expr.getReference()
    return;
  }
}

void AdvFunc::findReferences(SetExpr::Reader set) {
  ++_nesting_depth;
  if (_nesting_depth > MAX_NESTING_DEPTH) {
    throw EvaluationError("max nesting depth reached");
  }

  if (set.hasName())
    _set_expr_refs[set.getName()] = set;

  auto type = set.which();
  switch (type) {
  case SetExpr::SINGLETON:
    for (auto expr : set.getSingleton())
      findReferences(expr);
    return;
  case SetExpr::BALL: {
    auto ball = set.getBall();
    for (auto expr : ball.getCenter())
      findReferences(expr);
    findReferences(ball.getRadius());
    return;
  }
  case SetExpr::RECTANGLE:
    for (auto bpair : set.getRectangle())
    {
      findReferences(bpair.getBoundA());
      findReferences(bpair.getBoundB());
    }
    return;
  case SetExpr::CONVEX_POLYTOPE: {
    auto poly = set.getConvexPolytope();
    for (auto list : poly.getA())
      for (auto expr : list)
        findReferences(expr);
    for (auto expr : poly.getB())
      findReferences(expr);
    return;
  }
  case SetExpr::INTERSECTION: 
    for (auto expr : set.getIntersection())
      findReferences(expr);
    return;
  
  /*
  case SetExpr::BINARY_OPERATION: {
    auto binop = set.getBinaryOperation();
    findReferences(binop.getSetA());
    findReferences(binop.getSetB());
    return;
  }
  case SetExpr::LIST_OPERATION:
    for (auto setexpr : set.getListOperation().getSets())
      findReferences(setexpr);
    return;
  case SetExpr::PARAMETERIZED_OPERATION: {
    auto paramop = set.getParameterizedOperation();
    findReferences(paramop.getSet());
    findReferences(paramop.getParamSet());
    return;
  }
  case SetExpr::PROJECTION: {
    auto proj = set.getProjection();
    findReferences(proj.getSet());
    findReferences(proj.getOntoSet());
    return;
                              
  }
  */
  case SetExpr::CASE_DISTINCTION:
    for (auto cs : set.getCaseDistinction().getCases()) 
    {
      findReferences(cs.getSet());
      findReferences(cs.getExpression());
    }
    return;
  default:
    return;
  }
}

