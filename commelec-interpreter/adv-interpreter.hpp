#ifndef ADVFUNC_HPP
#define ADVFUNC_HPP

#include <commelec-api/schema.capnp.h>
#include <capnp/message.h>
#include <kj/string.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cassert>
#include <iostream>

#include <Eigen/Dense>
#include <Eigen/Core>
#include <Eigen/Geometry>

#include <boost/format.hpp>

#ifndef NO_CAPNP_REFLECTION      
#include <capnp/dynamic.h> // only for error messaging purpose
// can be disabled if necessary (e.g., for Windows builds, where Cap'n Proto
// reflection is, at the time of this writing, not supported.)
#endif

template <typename StructType>
const char *getUnionFieldName(StructType whichable) {
// Get the string representation of a Cap'n Proto union via the dynamic reflection api.
//
// This function returns a pointer to the C-string if successful, or it returns
// nullptr if reflection is not supported (on Windows), or the name cannot be
// retrieved for some other reason.
//
// Note: We merely use this function for more insightfull error messages

#ifndef NO_CAPNP_REFLECTION
  capnp::DynamicStruct::Reader dynWhichable = whichable;
  KJ_IF_MAYBE(field_ptr, dynWhichable.which()) {
    return field_ptr->getProto().getName().cStr();
  }
  else
#endif
      return nullptr;
}

struct EvaluationError: public std::runtime_error {
  EvaluationError(const std::string& msg = ""): std::runtime_error(msg) {}
};

struct UnknownReference: public std::runtime_error {
  UnknownReference(const std::string& ref, const std::string& msg = ""): std::runtime_error(msg), _ref(ref) {}
  std::string _ref;
};

struct UnknownVariable: public std::runtime_error {
  UnknownVariable(const std::string& ref, const std::string& msg = ""): std::runtime_error(msg), _ref(ref) {}
  std::string _ref;
};

struct NoConvergence: public std::runtime_error {
  NoConvergence(const std::string& msg = ""): std::runtime_error(msg) {}
};

struct WhichError : public std::runtime_error {
  WhichError(int w = -1, const std::string& msg = ""): std::runtime_error(msg), which(w) {}
  int which;
};

using ValueMap = std::unordered_map<std::string, double>;
using RealExprRefMap = std::unordered_map<std::string, msg::RealExpr::Reader>;
using SetExprRefMap = std::unordered_map<std::string, msg::SetExpr::Reader>;
using PointType = std::vector<double>;
using PointTypePP = const PointType&; // passing policy: pass-by-ref (can be changed here to pass-by-val)

struct HalfSpace
{
  Eigen::VectorXd a;
  double b;
};

template <typename T> int sgn(T val) {
// signum function
    return (T(0) < val) - (val < T(0));
}


struct AdvFunc
{
  const int MAX_NESTING_DEPTH = 10000;
  // This prevents that the evaluator can get stuck in an infinite loop by
  // following paths of references that contain cycles.
  // (Attackers might try to send a message with a cycle on purpose, or
  // inexperienced users may accidentally do this)

  AdvFunc(msg::Advertisement::Reader adv);

  AdvFunc(): _advValid(false) {};

  void setAdv(msg::Advertisement::Reader adv)
  {
    _adv = adv;
    _advValid = true;
    findReferences();
  };

  // Top-level user functions:
  bool testMembership(msg::SetExpr::Reader set, PointTypePP point, const ValueMap &bound_vars);
  double evaluate(msg::RealExpr::Reader, const ValueMap &bound_vars);
  PointType project(msg::SetExpr::Reader, PointTypePP point, const ValueMap &bound_vars);

  template <typename Derived>
  Eigen::Vector2d project(msg::SetExpr::Reader set,
                          const Eigen::MatrixBase<Derived> &point,
                          const ValueMap &bound_vars) {
    assert(_advValid);
    _bound_vars = &bound_vars;
    _nesting_depth = 0;
    return proj(set, point);
  }

  Eigen::AlignedBoxXd rectangularHull(msg::SetExpr::Reader set, const ValueMap &bound_vars);
  double evalPartialDerivative(msg::RealExpr::Reader expr,std::string diffVariable, const ValueMap &bound_vars); 

private:
  Eigen::VectorXd evalToVector(capnp::List<msg::RealExpr>::Reader list);
  // convert cap'n proto list of realexpr to evaluated vector of doubles
  
  Eigen::MatrixXd evalToMatrix(capnp::List<capnp::List<msg::RealExpr>>::Reader mx);
  // convert cap'n proto list of realexpr to evaluated vector of doubles

  // ==========================================================
  // Functions for testing membership:  (set, point) -> boolean
  // ==========================================================
  
  bool membership(msg::SetExpr::Reader set, PointTypePP pt);
  // membership function for users that prefer using STL container over eigen type to store point

  template <typename Derived>
  bool membership(msg::SetExpr::Reader set,
                  const Eigen::MatrixBase<Derived> &point) {

    ++_nesting_depth;
    if (_nesting_depth > MAX_NESTING_DEPTH) {
      //throw EvaluationError("max nesting depth reached");
      
      std::cout << " max nesting depth reached"  << std::endl;
      _nesting_depth=0;
    }

    auto type = set.which();
    switch (type) {
    case msg::SetExpr::SINGLETON:
      return membership(set.getSingleton(), point);
    case msg::SetExpr::BALL:
      return membership(set.getBall(), point);
    case msg::SetExpr::RECTANGLE:
      return membership(set.getRectangle(), point);
    case msg::SetExpr::CONVEX_POLYTOPE:
      return membership(set.getConvexPolytope(), point);
    case msg::SetExpr::INTERSECTION:
      return membership(set.getIntersection(), point);
    case msg::SetExpr::REFERENCE:
      return membership(_set_expr_refs[set.getReference()], point);
    // case msg::SetExpr::LIST_OPERATION:
    //  return membership(set.getListOperation(), point);
    default:
      // membership operation is not defined for the provided set: we'll throw
      // an exception

      // try to format an error msg containing a human-readable name of the type
      // via Cap'n Proto's Dynamic API
      boost::format msg;

      KJ_IF_MAYBE(fieldname, getUnionFieldName(set)) {
        msg = boost::format(
                  "AdvFunc::membership in msgfunction.cpp [.which()=%1%,%2%]") %
              int(type) % fieldname;
      }
      else
          // unable to retrieve the human-readable name for type, we'll just
          // throw
          // the exception without this name
          msg = boost::format(
                    "AdvFunc::membership in msgfunction.cpp [.which()=%1%]") %
                int(type);

      throw WhichError(type, msg.str());
    }
  }

  template <typename Derived>
  bool membership(capnp::List<msg::RealExpr>::Reader singleton,
                  const Eigen::MatrixBase<Derived> &point) {
    auto sz = singleton.size();
    assert(point.size() == sz);
    Eigen::VectorXd sing = evalToVector(singleton);
    return point.isApprox(sing);
  }

  template <typename Derived>
  bool membership(msg::Ball::Reader ball,
                  const Eigen::MatrixBase<Derived> &point) {
    auto center = ball.getCenter();
    assert(point.size() == center.size());

    // evaluate center and store result in cent
    Eigen::VectorXd cent = evalToVector(center); // (sz);
    auto r = eval(ball.getRadius());
    return (cent.array() - point.array()).square().sum() <= r * r;
  }

  template <typename Derived>
  bool membership(capnp::List<msg::BoundaryPair>::Reader rect,
                  const Eigen::MatrixBase<Derived> &point) {
    assert(point.size() == rect.size());
    auto i = 0;
    for (auto boundspair : rect) {

      auto val1 = eval(boundspair.getBoundA());
      auto val2 = eval(boundspair.getBoundB());
      auto minVal = std::min(val1, val2);
      auto maxVal = std::max(val1, val2);

      if ((point(i) < minVal) || (point(i) > maxVal))
        return false;
      ++i;
    }
    return true;
  }

  template <typename Derived>
  bool membership(msg::ConvexPolytope::Reader poly,
                  const Eigen::MatrixBase<Derived> &point) {
    Eigen::MatrixXd A(evalToMatrix(poly.getA()));
    Eigen::VectorXd b(evalToVector(poly.getB()));
    return ((A * point).array() <= b.array()).all();
    // coefficient-wise comparison using .array() method
  }

  template <typename Derived>
  bool membership(capnp::List<msg::SetExpr>::Reader intersection,
                  const Eigen::MatrixBase<Derived> &point) {
    for (auto child_set : intersection) { // logical and (greedy)
      if (!membership(child_set, point))
        return false;
    }
    return true;
  }

  template<typename Derived> bool membership(const HalfSpace& hs,const Eigen::MatrixBase<Derived>& point)
  //test membership for a single halfspace
  {
    return (hs.a).transpose() * point <= hs.b;
  }  

  
  template<typename Derived> bool membership(const std::vector<HalfSpace>& vhs,const Eigen::MatrixBase<Derived>& point)
  //test membership for a list of intersected halfspaces
  {
    bool member = true;
    for (auto hs:vhs){
      if (!membership(hs, point)) {
        member =false;
        break;
      }
    }
    return member;
  }
  
  // ==================================================================
  // Functions for evaluating a RealExpr'ession:  (real expr) -> double
  // ==================================================================
  
  double eval(msg::RealExpr::Reader expr);
  double eval(msg::UnaryOperation::Reader op);
  double eval(msg::BinaryOperation::Reader bin_op);
  double eval(msg::ListOperation::Reader list_op);
  double eval(msg::Polynomial::Reader poly,int dVar=-1) const;
  double eval(msg::CaseDistinction<msg::RealExpr>::Reader cases);
  double evalRef(const kj::StringPtr ref);
  double evalVar(const kj::StringPtr var);

  double evalPartialDerivative(msg::RealExpr::Reader expr, const std::string& diffVariable); 
  double evalPartialDerivative(msg::Polynomial::Reader poly, const std::string& diffVariable); 
  double evalPartialDerivative(msg::UnaryOperation::Reader unop, const std::string& diffVariable);
  double evalPartialDerivative(msg::BinaryOperation::Reader binop, const std::string& diffVariable);
  double evalPartialDerivative(msg::ListOperation::Reader listop, const std::string& diffVariable);
  double evalPartialDerivative(msg::CaseDistinction<msg::RealExpr>::Reader cases,const std::string& diffVariable);

  // below, we add Ref and Var as suffixes to distinguish between them (this is
  // needed because the function prototypes are identical)
  double evalPartialDerivativeRef(const kj::StringPtr ref,const std::string& diffVariable);
  double evalPartialDerivativeVar(const kj::StringPtr var,const std::string &diffVariable);

  // =========================================
  // Functions for projecting a point to a set
  // =========================================

  double proj_intval(double x, double min, double max);

  PointType proj(msg::SetExpr::Reader set, PointTypePP pt);
  // projection function for users that prefer using STL container over eigen
  // type to store point

  double dist(PointTypePP a, PointTypePP b); 

  template <typename DerivedA, typename DerivedB>
  typename DerivedA::Scalar dist(const Eigen::MatrixBase<DerivedA> &a,
                                 const Eigen::MatrixBase<DerivedB> &b) {
    return (a - b).norm();
  }

  template <typename Derived>
  typename Derived::PlainObject proj(msg::SetExpr::Reader set,
                                     const Eigen::MatrixBase<Derived> &point) {
    ++_nesting_depth;
    if (_nesting_depth > MAX_NESTING_DEPTH) {
      //throw EvaluationError("max nesting depth reached");
      
      std::cout << " max nesting depth reached"  << std::endl;
      _nesting_depth=0;
    }

    auto type = set.which();
    switch (type) {
    case msg::SetExpr::SINGLETON:
      return proj(set.getSingleton(), point);
    case msg::SetExpr::BALL:
      return proj(set.getBall(), point);
    case msg::SetExpr::RECTANGLE:
      return proj(set.getRectangle(), point);
    case msg::SetExpr::CONVEX_POLYTOPE:
      return proj(set.getConvexPolytope(), point);
    case msg::SetExpr::INTERSECTION:
      return proj(set.getIntersection(), point);
    // case SetExpr::BINARY_OPERATION:
    //  return proj(set.getBinaryOperation(), point);
    // case SetExpr::LIST_OPERATION:
    //  return proj(set.getListOperation(), point);

    // case SetExpr::GENERIC_DISK:
    //  return proj(set.getGenericDisk(), point);
    // case SetExpr::INTERVAL:
    //  return proj(set.getInterval(), point);
    default:
      // proj operation is not defined for the provided set: we'll throw an
      // exception

      // try to format an error msg containing a human-readable name of the type
      // via Cap'n Proto's Dynamic API

      // TODO : make a function of this logic

      boost::format msg;

      KJ_IF_MAYBE(fieldname, getUnionFieldName(set)) {
        msg = boost::format("AdvFunc::proj [.which()=%1%,%2%]") % int(type) %
              fieldname;
      }
      else
      {
        // unable to retrieve the human-readable name for type, we'll just throw
        // the exception without this name
        msg = boost::format("AdvFunc::proj [.which()=%1%]") % int(type);
      }
      throw WhichError(type, msg.str());
    }
  }

  template <typename Derived>
  typename Derived::PlainObject
  proj(capnp::List<msg::RealExpr>::Reader singleton,
       const Eigen::MatrixBase<Derived> &point) {
    // projecting to a singleton is trivial, we just return the (evaluated)
    // singleton
    return evalToVector(singleton);
  }

  template <typename Derived>
  typename Derived::PlainObject proj(msg::Ball::Reader ball,
                                     const Eigen::MatrixBase<Derived> &point) {
    if (membership(ball, point))
      return point;
    else {
      Eigen::VectorXd center(evalToVector(ball.getCenter()));
      double r = eval(ball.getRadius());
      return center + r * (point - center).normalized();
    }
  }

  template <typename Derived>
  typename Derived::PlainObject
  proj(capnp::List<msg::BoundaryPair>::Reader rect,
       const Eigen::MatrixBase<Derived> &point) {
    Eigen::VectorXd result(point.size());
    int i = 0;
    for (auto bpair : rect) {

      auto val1 = eval(bpair.getBoundA());
      auto val2 = eval(bpair.getBoundB());
      auto minVal = std::min(val1, val2);
      auto maxVal = std::max(val1, val2);

      result(i) = proj_intval(point(i), minVal, maxVal);
      ++i;
    }
    return result;
  }

  template <typename Derived>
  typename Derived::PlainObject proj(msg::ConvexPolytope::Reader poly,
                                     const Eigen::MatrixBase<Derived> &point) {
    if (membership(poly, point))
      return point;

    // represent polytope as intersection of half-planes
    // and lets us use Dykstra's projection algorithm

    std::vector<HalfSpace> altPolygonRepr;

    auto A = poly.getA();
    auto b = poly.getB();

    auto sz = A.size();

    for (auto i = 0; i < sz; ++i) {
      altPolygonRepr.push_back(HalfSpace{evalToVector(A[i]), eval(b[i])});
    }

    Eigen::VectorXd result;
    if (dykstraProjectionAlgorithm(altPolygonRepr, point, result))
      return result;
    else
      throw NoConvergence("Dykstra's algorithm did not converge.");
  }

  template <typename Derived>
  typename Derived::PlainObject proj(capnp::List<msg::SetExpr>::Reader intsect,
                                     const Eigen::MatrixBase<Derived> &point) {
    if (membership(intsect, point))
      return point;

    Eigen::VectorXd result;
    if (dykstraProjectionAlgorithm(intsect, point, result))
      return result;
    else
      throw NoConvergence("Dykstra's algorithm did not converge.");
  }

  template <typename Derived>
  typename Derived::PlainObject proj(const HalfSpace &hs,
                                     const Eigen::MatrixBase<Derived> &point) {
    if (membership(hs, point))
      return point;
    return point -
           hs.a / hs.a.squaredNorm() * ((hs.a).transpose() * point - hs.b);
  }

  template <typename Container, typename Derived, typename OtherDerived>
  bool /*converged?*/ alternatingProjectionAlgorithm(
      /*std::vector<SetExpr::Reader>*/ Container sets,
      const Eigen::MatrixBase<Derived> &point,
      const Eigen::MatrixBase<OtherDerived> &res, double convergence_threshold,
      int max_iterations) {

    // in this case, just do normal alternating projection, like in the original
    // grid agent code
    Eigen::MatrixBase<OtherDerived> &result =
        const_cast<Eigen::MatrixBase<OtherDerived> &>(res);

    auto point_sz = point.size();
    result.derived().resize(point_sz);
    Eigen::VectorXd x = point;

    for (auto iter = 0; iter < max_iterations; ++iter) {
      for (auto set : sets) {
        x = proj(set, x);

        // test if we're already done
        if (membership(sets, x)) {
          result = x;
          return true;
        }
      }
    }
    return false;
    // no convergence
  }

  template <typename Container, typename Derived, typename OtherDerived>
  bool /*converged?*/ dykstraProjectionAlgorithm(
      Container /*typically: <msg::SetExpr::Reader>*/ sets,
      const Eigen::MatrixBase<Derived> &point,
      const Eigen::MatrixBase<OtherDerived> &res,
      double convergence_threshold = 1.0e-3, int max_iterations = 1000) {

    // do Dykstra's alternating projection algorithm
    // reference: Shih-Ping Han - A Successive Projection Method
    //            Mathematical Programming 40 (1988) p. 1-14

    Eigen::MatrixBase<OtherDerived> &result =
        const_cast<Eigen::MatrixBase<OtherDerived> &>(
            res); // this is needed  (see Eigen3 docs: "Writing Functions Taking
                  // Eigen Types as Parameters")

    auto point_sz = point.size();
    result.derived().resize(point_sz);

    auto m = sets.size();

    std::vector<Eigen::VectorXd> xv;
    std::vector<Eigen::VectorXd> yv;

    for (auto i = 0; i <= m; ++i) {
      xv.emplace_back(Eigen::VectorXd(point_sz));
      yv.emplace_back(Eigen::VectorXd::Zero(point_sz));
    }

    bool converged = false;

    xv[m] = point;
    result = point;

    Eigen::VectorXd z(point_sz);

    auto iter = 0;
    //for (auto iter = 0; iter < max_iterations; ++iter) {
    for (; iter < max_iterations; ++iter) {

      for (auto i = 1; i <= m; ++i) {
        xv[0] = xv[m];
        z = (xv[i - 1]) + (yv[i]);
        xv[i] = proj(sets[i - 1], z);
        yv[i] = z - xv[i];
      }
      
      //std::cout << "dist(x[k],x[k-1]): "<<  dist(xv[m], result) << std::endl;

      if (dist(xv[m], result) < convergence_threshold) {
        converged = true;
        break;
      }
      result = xv[m];
    }
    //std::cout << "iterations: " << iter+1 << std::endl;
    return converged;
  }

  Eigen::AlignedBoxXd rectHull(msg::SetExpr::Reader set);
  Eigen::AlignedBoxXd rectHull(capnp::List<msg::RealExpr>::Reader singleton);
  Eigen::AlignedBoxXd rectHull(msg::Ball::Reader ball);
  Eigen::AlignedBoxXd rectHull(capnp::List<msg::BoundaryPair>::Reader rect);
  Eigen::AlignedBoxXd rectHull(msg::ConvexPolytope::Reader poly);
  Eigen::AlignedBoxXd rectHull(capnp::List<msg::SetExpr>::Reader intersection);

  // Scan through the message for named RealExpressions and named SetExpressions and store pointers (Reader objects) to these objects in the "RefMaps" _real_expr_refs and _set_expr_refs respectively. When encountering a reference during evaluation of an expression, we can then find the reference using these RefMaps. The findReferences method is called by the constructor of AdvFunc, assuming that a new AdvFunc object is constructed each time an advertisement is received.
  void findReferences();
  void findReferences(msg::RealExpr::Reader expr);
  void findReferences(msg::SetExpr::Reader expr);

  int _nesting_depth;
  msg::Advertisement::Reader _adv;
  bool _advValid;
  const ValueMap* _bound_vars;
  RealExprRefMap _real_expr_refs;
  SetExprRefMap _set_expr_refs;
};

#endif
