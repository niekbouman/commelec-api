#include <commelec-interpreter/adv-interpreter.hpp>
#include <cmath>
#include <boost/format.hpp>

using namespace msg;

double AdvFunc::evaluate(RealExpr::Reader expr, const ValueMap &bound_vars) {
  _bound_vars = &bound_vars;
  _nesting_depth = 0;
  return eval(expr);
}

double AdvFunc::evalPartialDerivative(RealExpr::Reader expr,std::string diffVariable , const ValueMap &bound_vars) {
  _bound_vars = &bound_vars;
  _nesting_depth = 0;
  return evalPartialDerivative(expr, diffVariable);
}

double AdvFunc::eval(RealExpr::Reader expr) {
  ++_nesting_depth;
  if (_nesting_depth > MAX_NESTING_DEPTH) {
      throw EvaluationError("max nesting depth reached");
  }

  switch (expr.which()) {
  case RealExpr::REAL:
    return expr.getReal();
  case RealExpr::UNARY_OPERATION:
    return eval(expr.getUnaryOperation());
  case RealExpr::BINARY_OPERATION:
    return eval(expr.getBinaryOperation());
  case RealExpr::LIST_OPERATION:
    return eval(expr.getListOperation());
  case RealExpr::POLYNOMIAL:
    return eval(expr.getPolynomial());
  case RealExpr::CASE_DISTINCTION:
    return eval(expr.getCaseDistinction());
  case RealExpr::REFERENCE:
    return evalRef(expr.getReference());
  case RealExpr::VARIABLE:
    return evalVar(expr.getVariable());
    
    // case RealExpr::UNIFORM_GRID_SAMPLED_FUNCTION: return operation();
    // case RealExpr::NON_UNIFORM_GRID_SAMPLED_FUNCTION: return operation();
    // case RealExpr::SAMPLED_FUNCTION: return operation();
  }
  //throw;
}

double AdvFunc::evalPartialDerivative(RealExpr::Reader expr, const std::string& diffVariable ) {
  ++_nesting_depth;
  if (_nesting_depth > MAX_NESTING_DEPTH) {
      throw EvaluationError("max nesting depth reached");
  }

  switch (expr.which()) {
  case RealExpr::REAL:
    return 0; // derivative of a constant is zero
  case RealExpr::UNARY_OPERATION:
    return evalPartialDerivative(expr.getUnaryOperation(), diffVariable);
  case RealExpr::BINARY_OPERATION:
    return evalPartialDerivative(expr.getBinaryOperation(), diffVariable);
  case RealExpr::LIST_OPERATION:
    return evalPartialDerivative(expr.getListOperation(), diffVariable);
  case RealExpr::CASE_DISTINCTION:
    return evalPartialDerivative(expr.getCaseDistinction(), diffVariable);
  case RealExpr::POLYNOMIAL:
    return evalPartialDerivative(expr.getPolynomial(), diffVariable);
  case RealExpr::REFERENCE:
    return evalPartialDerivativeRef(expr.getReference(), diffVariable);
  case RealExpr::VARIABLE:
    return evalPartialDerivativeVar(expr.getVariable(), diffVariable);

    // case RealExpr::UNIFORM_GRID_SAMPLED_FUNCTION: return operation();
    // case RealExpr::NON_UNIFORM_GRID_SAMPLED_FUNCTION: return operation();
    // case RealExpr::SAMPLED_FUNCTION: return operation();
  }
  throw;
}

double AdvFunc::eval(UnaryOperation::Reader op) {
  auto arg = op.getArg();
  auto operation = op.getOperation();
  switch (operation.which()) {
  case UnaryOperation::Operation::NEGATE:
    return -eval(arg);
  case UnaryOperation::Operation::EXP:
    return std::exp(eval(arg));
  case UnaryOperation::Operation::SIN:
    return std::sin(eval(arg));
  case UnaryOperation::Operation::COS:
    return std::cos(eval(arg));
  case UnaryOperation::Operation::TAN:
    return std::tan(eval(arg));
  case UnaryOperation::Operation::SQUARE:
  {
      double x = eval(arg);
      return x * x;
  }
  case UnaryOperation::Operation::SQRT:
    return std::sqrt(eval(arg));
  case UnaryOperation::Operation::LOG10:
    return std::log10(eval(arg));
  case UnaryOperation::Operation::LN:
    return std::log(eval(arg));
  case UnaryOperation::Operation::MULT_INV:
    return 1.0 / eval(arg);
  case UnaryOperation::Operation::ROUND:
    return std::round(eval(arg));
  case UnaryOperation::Operation::FLOOR:
    return std::floor(eval(arg));
  case UnaryOperation::Operation::CEIL:
    return std::ceil(eval(arg));
  case UnaryOperation::Operation::ABS:
    return std::abs(eval(arg));
  case UnaryOperation::Operation::SIGN:
    return sgn(eval(arg)); // >= 0) ? 1 : -1;
  }
}

double AdvFunc::eval(BinaryOperation::Reader bin_op) {

  auto operation = bin_op.getOperation();
  auto arg1 = bin_op.getArgA();
  auto arg2 = bin_op.getArgB();

  switch (operation.which()) {
  case BinaryOperation::Operation::SUM:
    return eval(arg1) + eval(arg2);
  case BinaryOperation::Operation::PROD:
    return eval(arg1) * eval(arg2);
  case BinaryOperation::Operation::LESS_EQ_THAN:
    return ( eval(arg1) <= eval(arg2) ) ? 1.0 : 0.0; 
  case BinaryOperation::Operation::GREATER_THAN:
    return ( eval(arg1) > eval(arg2) ) ? 1.0 : 0.0; 
  case BinaryOperation::Operation::MIN:
    return std::min( eval(arg1), eval(arg2));
  case BinaryOperation::Operation::MAX:
    return std::max( eval(arg1), eval(arg2));
  case BinaryOperation::Operation::POW:
    return std::pow(eval(arg1), eval(arg2));
  }
}

double AdvFunc::evalPartialDerivative(msg::BinaryOperation::Reader bin_op,
                                      const std::string &diffVariable) {

  auto operation = bin_op.getOperation();
  auto arg1 = bin_op.getArgA();
  auto arg2 = bin_op.getArgB();

  switch (operation.which()) {
  case BinaryOperation::Operation::SUM:
    return evalPartialDerivative(arg1, diffVariable) +
           evalPartialDerivative(arg2, diffVariable);
  case BinaryOperation::Operation::PROD:
    return eval(arg1) * evalPartialDerivative(arg2, diffVariable) +
           eval(arg2) * evalPartialDerivative(arg1, diffVariable);
  case BinaryOperation::Operation::LESS_EQ_THAN:
    return 0.0;
  case BinaryOperation::Operation::GREATER_THAN:
    return 0.0;
  case BinaryOperation::Operation::MIN:
    return (eval(arg1) <= eval(arg2))
               ? evalPartialDerivative(arg1, diffVariable)
               : evalPartialDerivative(arg2, diffVariable);
  case BinaryOperation::Operation::MAX:
    return (eval(arg1) > eval(arg2))
               ? evalPartialDerivative(arg1, diffVariable)
               : evalPartialDerivative(arg2, diffVariable);
  case BinaryOperation::Operation::POW: {
    // d/dx f(x)^{g(x)} = f(x)^{g(x)-1} ( g(x) f'(x) + f(x) \log (f(x)) g'(x) )
    auto base = eval(arg1);
    auto expon = eval(arg2);
    return std::pow(base, expon - 1.0) *
           (expon * evalPartialDerivative(arg1, diffVariable) +
            base * std::log(base) * evalPartialDerivative(arg2, diffVariable));
  }
  }
}

double AdvFunc::eval(ListOperation::Reader list_op) {

  auto op = list_op.getOperation();
  auto args = list_op.getArgs();

  switch (op.which()) {
  case ListOperation::Operation::SUM: {
    double accum = 0;
    for (const auto &arg : args) {
      accum += eval(arg);
    }
    return accum;
  }
  case ListOperation::Operation::PROD: {
    double mult = 1.0;
    for (const auto &arg : args) {
      mult *= eval(arg);
    }
    return mult;
  }
  }
}

double AdvFunc::evalPartialDerivative(ListOperation::Reader list_op,const std::string& diffVariable) {

  auto op = list_op.getOperation();
  auto args = list_op.getArgs();

  switch (op.which()) {
  case ListOperation::Operation::SUM: {
    double accum = 0.0;
    for (const auto &arg : args) {
      accum += evalPartialDerivative(arg, diffVariable);
    }
    return accum;
  }
  case ListOperation::Operation::PROD: {
    double accum = 0.0;
    Eigen::VectorXd evaluations(evalToVector(args));
    auto sz = evaluations.size();

    auto i = 0;
    for (const auto &arg : args) {
      auto mult = evalPartialDerivative(arg, diffVariable);
      for(auto j=0; j<sz;++j){
        if (j!=i){
          mult *= evaluations[j];
        }
      }
      ++i;
      accum+=mult;
    }
    return accum;
  }
  }
}


double AdvFunc::evalPartialDerivative(msg::CaseDistinction<msg::RealExpr>::Reader casedist,const std::string& diffVariable) {

  // prepare the evaluation point (usually this will be the values in
  // _bound_vars belonging to "P" and "Q")
  // this structure allows for case distinctions in sets of arbitrary dimension
  std::vector<double> point;
  for (auto var : casedist.getVariables()) {
    auto value_it = _bound_vars->find(var);
    if (value_it != _bound_vars->end())
      point.push_back(value_it->second);
    else
      throw EvaluationError("Variable specified in CaseDistinction not found "
                            "in VariableMap bound_vars.");
  }

  // determine which case we need to evaluate
  for (auto re_case : casedist.getCases()) {
    auto set = re_case.getSet();
    if (membership(set, point))
      return evalPartialDerivative(re_case.getExpression(),diffVariable);
  }
  throw EvaluationError("Unhandled case in CaseDistinction");
}


double AdvFunc::evalPartialDerivative(Polynomial::Reader poly,const std::string& diffVariable)
{
  auto vars = poly.getVariables();
  int i=0;
  for (auto var:vars){
    if (diffVariable == std::string(var))
      return eval(poly,i);
    ++i;
  }
  return 0;
}

double AdvFunc::evalPartialDerivative(msg::UnaryOperation::Reader unop,
                             const std::string &diffVariable) {

  auto arg = unop.getArg();
  auto operation = unop.getOperation();
  switch (operation.which()) {
  case UnaryOperation::Operation::NEGATE:
    return -evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::EXP:
    return std::exp(eval(arg)) * evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::SIN:
    return std::cos(eval(arg)) * evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::COS:
    return -std::sin(eval(arg)) * evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::TAN: {
    auto x = std::cos(eval(arg));
    return 1.0 / (x * x) * evalPartialDerivative(arg, diffVariable);
  }
  case UnaryOperation::Operation::SQUARE:
    return 2.0 * evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::SQRT:
    return 1.0 / (2.0 * std::sqrt(eval(arg))) * evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::LOG10:
    return M_LOG10E / eval(arg) * evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::LN:
    return 1.0 / eval(arg) * evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::MULT_INV: {
    auto x = eval(arg);
    return -1.0 / (x * x) * evalPartialDerivative(arg, diffVariable);
  }
  case UnaryOperation::Operation::ROUND: // we ignore the rounding here; TODO: should issue a warning here
    return evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::FLOOR: // we ignore the rounding here; TODO: should issue a warning here
    return evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::CEIL: // we ignore the rounding here; TODO: should issue a warning here
    return evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::ABS:
    return sgn(eval(arg)) * evalPartialDerivative(arg, diffVariable);
  case UnaryOperation::Operation::SIGN:
    return 0;
  }
}

double AdvFunc::evalPartialDerivativeRef(const kj::StringPtr ref,const std::string &diffVariable) {
   auto referenced_real_expr = _real_expr_refs.find(ref);
  // try to locate reference in refs
  if (referenced_real_expr != _real_expr_refs.end()) {
    // reference found, evaluate it by calling ourselves (will be handled by the
    // method that deals with RealExpr::Reader types
    return evalPartialDerivative(referenced_real_expr->second,diffVariable);
  }

  // if we reach this, then the message refers to some non-existing reference
  auto msg = boost::format("AdvFunc::evalPartialDerivative(std::string ref, ...) in "
                      "Evaluate.cpp [ref=%1%]") % ref.cStr();
  throw UnknownReference(ref, str(msg));
}

double AdvFunc::evalPartialDerivativeVar(const kj::StringPtr var,const std::string &diffVariable) {

  auto value_it = _bound_vars->find(var);
  if (value_it != _bound_vars->end()) {
      if (var == diffVariable)
          return static_cast<double>(1);
      else return static_cast<double>(0);
  }

  // if we reach this, then the message refers to some non-existing variable
  auto msg = boost::format("AdvFunc::evalPartialDerivativeVar(const kj::StringPtr var, ...) in "
                      "Evaluate.cpp [ref=%1%]") % var.cStr();
  throw UnknownVariable(var, str(msg));
}

double AdvFunc::eval(Polynomial::Reader poly,int dVar) const {
  // TODO: use Horner's evaluation method for multivariate polynomials
  
  // dVar - evaluate partial derivative with respect to variable i 
  // (dVar= -1 means ordinary evaluation of the polynomial itself)

  // evaluate variables
  std::vector<double> evalpoint;
  auto vars = poly.getVariables();
  for(auto var:vars)
    evalpoint.push_back(_bound_vars->at(var));

  // evaluate each monomial
  int d = poly.getMaxVarDegree()+1;
  double result = 0;
  for (auto coeff: poly.getCoefficients()){
    double monom=1;
    //int var=0;
    int offset = coeff.getOffset();

    // convert offset value into sequence of powers of the monomial
    int sz = vars.size();
    for(int var=0; var<sz; ++var){
      int rem = offset / static_cast<int>(std::pow(d,var)+.5) % d;
      if(dVar == var){
        // compute partial derivative
        if (rem == 0){
          // monomial does not contain the variable with respect to which we take the derivative, hence skip it)
          monom=0;
          break;
        }
        if (rem > 1) monom *= rem*std::pow(evalpoint[var],rem-1); 
      }
      else
        monom *= std::pow(evalpoint[var],rem); 
    }
    result+= coeff.getValue() * monom;
  }
  return result;
}



double AdvFunc::eval(CaseDistinction<RealExpr>::Reader casedist) {

  // prepare the evaluation point (usually this will be the values in
  // _bound_vars belonging to "P" and "Q")
  // this structure allows for case distinctions in sets of arbitrary dimension
  std::vector<double> point;
  for (auto var : casedist.getVariables()) {
    auto value_it = _bound_vars->find(var);
    if (value_it != _bound_vars->end())
      point.push_back(value_it->second);
    else
      throw EvaluationError("Variable specified in CaseDistinction not found "
                            "in VariableMap bound_vars.");
  }

  // determine which case we need to evaluate
  for (auto re_case : casedist.getCases()) {
    auto set = re_case.getSet();
    if (membership(set, point))
      return eval(re_case.getExpression());
  }
  throw EvaluationError("Unhandled case in CaseDistinction");
}

//double AdvFunc::eval(std::string ref) {
double AdvFunc::evalRef(const kj::StringPtr ref) {
  auto referenced_real_expr = _real_expr_refs.find(ref);
  // try to locate reference in refs
  if (referenced_real_expr != _real_expr_refs.end()) {
    // reference found, evaluate it by calling ourselves (will be handled by the
    // method that deals with RealExpr::Reader types
    return eval(referenced_real_expr->second);
  }

  // if we reach this, then the message refers to some non-existing reference
  auto msg = boost::format("AdvFunc::evalRef(std::string ref, ...) in "
                      "Evaluate.cpp [ref=%1%]") % ref.cStr();
  throw UnknownReference(ref, str(msg));
}

double AdvFunc::evalVar(const kj::StringPtr var) {
  auto value_it = _bound_vars->find(var);
  if (value_it != _bound_vars->end()) {
    return value_it->second;
    // return the double-value retrieved from bound_vars
  }

  // if we reach this, then the message refers to some non-existing variable
  auto msg = boost::format("AdvFunc::evalVar(std::string var, ...) in "
                      "Evaluate.cpp [var=%1%]") % var.cStr();
  throw UnknownVariable(var, str(msg));
}
