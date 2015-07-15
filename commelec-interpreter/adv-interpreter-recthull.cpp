#include <commelec-interpreter/adv-interpreter.hpp>
#include <commelec-interpreter/boundingbox-convexpolygon.hpp>
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Geometry>
#include <boost/format.hpp>
#include <capnp/dynamic.h> // only for error messaging purpose

#include <iostream>

using namespace msg;

/*
bool contains(capnp::List<SetExpr>::Reader iterable, SetExpr::Which whichVal)
// function to test whether a list of SetExpr's contains at least one SetExpr of
// type whichVal (i.e. Ball, Rectangle, etc)
{
  return std::any_of(iterable.begin(), iterable.end(),
                     [=](SetExpr::Reader s) { return s.which() == whichVal; });
}
*/

Eigen::AlignedBoxXd AdvFunc::rectHull(SetExpr::Reader set)
{
  ++_nesting_depth;
  if (_nesting_depth > MAX_NESTING_DEPTH) {
      throw EvaluationError("max nesting depth reached");
  }

  auto type = set.which();
  switch (type) {
  case SetExpr::SINGLETON:
    return rectHull(set.getSingleton());
  case SetExpr::BALL:
    return rectHull(set.getBall());
  case SetExpr::RECTANGLE:
    return rectHull(set.getRectangle());
  case SetExpr::CONVEX_POLYTOPE:
    return rectHull(set.getConvexPolytope());
  case SetExpr::INTERSECTION:
    return rectHull(set.getIntersection());
  //case SetExpr::LIST_OPERATION:
  //  return membership(set.getListOperation(), point);
  default:
    // membership operation is not defined for the provided set: we'll throw an
    // exception

    // try to format an error msg containing a human-readable name of the type
    // via Cap'n Proto's Dynamic API
    boost::format msg;
    capnp::DynamicStruct::Reader tmp = set;
    KJ_IF_MAYBE(field_ptr, tmp.which()) {
      auto fieldname = field_ptr->getProto().getName().cStr();
      msg =
          boost::format(
              "AdvFunc::rectangularHull [.which()=%1%,%2%]") %
          int(type) % fieldname;
    }
    else {
      // unable to retrieve the human-readable name for type, we'll just throw
      // the exception without this name
      msg = boost::format(
                "AdvFunc::rectangularHull [.which()=%1%]") %
            int(type);
    }
    throw WhichError(type, msg.str());
  }

}

Eigen::AlignedBoxXd AdvFunc::rectHull(capnp::List<RealExpr>::Reader singleton)
{
    return Eigen::AlignedBoxXd(evalToVector(singleton));
}

Eigen::AlignedBoxXd AdvFunc::rectHull(Ball::Reader ball)
{
    Eigen::VectorXd center(evalToVector(ball.getCenter())); 
    auto r = eval(ball.getRadius());
    return Eigen::AlignedBoxXd(center.array()-r,center.array()+r);
}

Eigen::AlignedBoxXd AdvFunc::rectHull(capnp::List<BoundaryPair>::Reader rect)
{
  auto sz = rect.size();
  Eigen::VectorXd minValues(sz);
  Eigen::VectorXd maxValues(sz);
  int i=0;
  for(auto mmpair:rect){
    auto val1 = eval(mmpair.getBoundA());
    auto val2 = eval(mmpair.getBoundB());
    minValues(i) = std::min(val1,val2);
    maxValues(i) = std::max(val1,val2);
    ++i;
  }
  return Eigen::AlignedBoxXd(minValues,maxValues);
}

Eigen::AlignedBoxXd AdvFunc::rectHull(ConvexPolytope::Reader poly) {
  // here, we assume that the convex polytope is bounded
  Eigen::MatrixXd A(evalToMatrix(poly.getA()));
  Eigen::VectorXd b(evalToVector(poly.getB()));
  return computeAABBConvexPolytope(A, b);
}

Eigen::AlignedBoxXd AdvFunc::rectHull(capnp::List<SetExpr>::Reader intersection)
{
    // ConvexPolytope might be itself unbounded, which means that we cannot
    // directly find its bounding box. To find the axis-aligned bounding box of
    // a convex polytope, we solve 2*dim (= 4 in our 2-dimensional case) linear
    // programs. (Per dimension, one for the minimum and one for the maximum.)
    //
    // Hence, if 'intersection' contains at least on Polytope, we need to take
    // special care.
    //
    // The COMMELEC protocol requires that the intersection of multiple convex
    // sets should be bounded.
    //
    // Hence, the algorithm here is as follows. We split the sets in the
    // intersection into two groups: convex polytopes go into set A, other
    // SetExpr's go into set B.
    // For set A, we merge the constraints of all convex polytopes into
    // MergedConstraints.
    // For each SetExpr in B, we compute its the bounding box (by calling the
    // rectHull functions) and intersect their bounding boxes, which produces
    // one AlignedBox. We then extract the corners of this AlignedBox and add
    // them as constraints to MergedConstraints.
    //
    // Now, (given that the sender of the Advertisement complies with COMMELEC's
    // rules) MergedConstraints is a bounded convex polytope, for which we can
    // find the bounding box using linear programming.

    Eigen::MatrixXd Merged_A;
    Eigen::VectorXd Merged_b;

    bool firstNonPoly = true;
    bool firstPoly = true;

    Eigen::AlignedBoxXd tempResult;

    for (auto set : intersection) {
      if (set.which() == SetExpr::CONVEX_POLYTOPE) {
        auto pt = set.getConvexPolytope();

        Eigen::VectorXd b(evalToVector(pt.getB()));
        Eigen::MatrixXd A(evalToMatrix(pt.getA()));

        if (firstPoly) {
          Merged_A = A;
          Merged_b = b;
          firstPoly = false;
        } else {
          // concatenate Merged_A with A, and Merged_b with b
          Merged_A.resize(Merged_A.rows() + A.rows(), Merged_A.cols());
          Merged_A << Merged_A, A;
          Merged_b.resize(Merged_b.size() + b.size());
          Merged_b << Merged_b, b;
        }

      } else // not a convex polytope
      {
        if (firstNonPoly) {
          firstNonPoly = false;
          tempResult = rectHull(set);
          continue;
        }
        tempResult = tempResult.intersection(rectHull(set));
      }
    }
    if (firstPoly) // if firstPoly is still true, then the intersection did not
                   // contain any ConvexPolytope and we're done
      return tempResult;

    else {
      // extract constraints from bounding box and add them to Merged_A and
      // Merged_b
      Eigen::VectorXd minimums(tempResult.min());
      Eigen::VectorXd maximums(tempResult.max());
      // eye matrix with -1s ?

      auto sz = minimums.size();

      Eigen::MatrixXd eye = Eigen::MatrixXd::Identity(sz, sz);

      Eigen::MatrixXd Final_A(Merged_A.rows() + 2 * sz, Merged_A.cols());
      Final_A << Merged_A, eye, -eye;
      Eigen::VectorXd Final_b(Merged_b.size() + 2 * sz);
      Final_b << Merged_b, maximums, -minimums;

      //std::cout << Final_A <<std::endl;
      //std::cout << Final_b <<std::endl;

      // call bounding box solver
      return computeAABBConvexPolytope(Final_A, Final_b);
    }
}

