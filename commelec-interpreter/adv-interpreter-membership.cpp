#include <commelec-interpreter/adv-interpreter.hpp>
#include <eigen3/Eigen/Core>

using namespace msg;

bool AdvFunc::membership(SetExpr::Reader set, PointTypePP pt)
// the user of the membership function need not use Eigen types
{
  Eigen::Map<const Eigen::VectorXd> point(pt.data(), pt.size());
  return membership(set, point);
}

bool AdvFunc::testMembership(SetExpr::Reader set, PointTypePP point,
                    const ValueMap &bound_vars) {
  assert(_advValid);
  _bound_vars = &bound_vars;
  _nesting_depth = 0;
  return membership(set, point);
}

