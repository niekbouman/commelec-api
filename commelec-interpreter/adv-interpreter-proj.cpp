#include <commelec-interpreter/adv-interpreter.hpp>

using namespace msg;

PointType AdvFunc::project(SetExpr::Reader set, PointTypePP point, const ValueMap &bound_vars)
{
  assert(_advValid);
  _bound_vars = &bound_vars;
  _nesting_depth = 0;
  return proj(set,point);
}

PointType AdvFunc::proj(SetExpr::Reader set, PointTypePP pt)
// the user of the proj function need not use Eigen types
{
  Eigen::Map<const Eigen::VectorXd> point(pt.data(), pt.size());
  PointType result(pt.size());
  Eigen::Map<Eigen::VectorXd> res(result.data(), result.size());
  res = proj(set, point); // this effectively modifies result
  return result;
}

double AdvFunc::proj_intval(double x, double min, double max){
  if (x <= min) return min;
  else if (x >= max) return max;
  else return x;
}

double AdvFunc::dist(PointTypePP a, PointTypePP b){
  Eigen::Map<const Eigen::VectorXd> aa(a.data(), a.size());
  Eigen::Map<const Eigen::VectorXd> bb(b.data(), b.size());
  return (aa - bb).norm();
}

