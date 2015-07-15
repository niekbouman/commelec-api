#include "lest.hpp" 
// a header-only C++11 unit testing framework

#include <commelec-api/realexpr-convenience.hpp>
#include <commelec-api/polynomial-convenience.hpp>
#include <commelec-interpreter/adv-interpreter.hpp>
#include <capnp/message.h>

const lest::test specification[] =
{
  CASE("Evaluation of a RealExpr")
  {

    // Build a message
    ::capnp::MallocMessageBuilder message;
    auto adv = message.initRoot<msg::Advertisement>();
    auto expr = adv.initCostFunction();

    using namespace cv;

    double a = 3.0;
    double b = 6.0;
    double c = 2.4;

    Var x("X");
    Var y("Y");

    buildRealExpr(expr, Real(a) * x + sin(y));
    // this uses expression templates, to construct the expression tree at
    // compile time

    AdvFunc interpreter(adv);

    EXPECT( std::abs(interpreter.evaluate(expr,{{"X",b},{"Y",c}}) - (a * b + std::sin(c))) < 1e-8);

  },
  CASE("Testing evaluation and differentiation of a polynomial-like expression")
  {

    // Build a message
    ::capnp::MallocMessageBuilder message;
    auto adv = message.initRoot<msg::Advertisement>();
    auto expr = adv.initCostFunction();

    cv::PolyVar Pvar("P");
    cv::PolyVar Qvar("Q");
    cv::buildPolynomial(expr.initPolynomial(), (Pvar^2)+3*(Pvar|Qvar^3));
    // the polynomial P^2 + 3*P*Q^2, encoded via the (dedicated) Polynomial RealExpr type

    AdvFunc interpreter(adv);
    // initialize the interpreter (holds some state)

    EXPECT(interpreter.evaluate(expr,{{"P",3},{"Q",5}})==1134);
    // evaluate the expression for P = 3 and Q = 5 (should equal 1134)
    
    EXPECT(interpreter.evalPartialDerivative(expr,"P",{{"P",2},{"Q",3}})==85);
    // compute the partial derivative w.r.t. P and evaluate it for P = 2 and Q = 3 (should be 85)


    // Now, we will make a new advertisement
    adv = message.initRoot<msg::Advertisement>();
    auto expr2 = adv.initCostFunction();

    cv::Var P("P");
    cv::Var Q("Q");
    cv::buildRealExpr(expr2,cv::pow(P,cv::Real(2))+cv::Real(3)*P*cv::pow(Q,cv::Real(3)));
    // the same polynomial (P^2 + 3*P*Q^2), but now encoded as a (generic) RealExpr


    AdvFunc newInterpreter(adv);

    EXPECT(newInterpreter.evaluate(expr2,{{"P",3},{"Q",5}})==1134);
    EXPECT(newInterpreter.evalPartialDerivative(expr2,"P",{{"P",2},{"Q",3}})==85);

  },
};

int main( int argc, char * argv[] )
{
  return lest::run(specification, argc, argv);
}
