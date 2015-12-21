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
#include <commelec-api/hlapi.h>
#include <commelec-api/hlapi-internal.hpp>
#include <commelec-api/schema.capnp.h>
#include <commelec-api/serialization.hpp>
#include <commelec-api/polytope-convenience.hpp>
#include <commelec-api/polynomial-convenience.hpp>
#include <commelec-api/realexpr-convenience.hpp>

#include <cstring>
#include <vector>
#include <limits>
#include <algorithm>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <Eigen/Core>
#include <boost/asio.hpp>

int32_t parseRequest(const uint8_t *inBuffer, int32_t bufSize, double *P,
                     double *Q, uint32_t *senderId) {
  try {
    ::kj::ArrayPtr<const capnp::byte> capnp_buffer(inBuffer, bufSize);
    ::kj::ArrayInputStream is(capnp_buffer);

    ::capnp::PackedMessageReader message(is);
    auto msg = message.getRoot<msg::Message>();
    *senderId = msg.getAgentId();
    auto req = msg.getRequest();

    if (!req.hasSetpoint())
      return 0;

    auto sp = req.getSetpoint();
    *P = sp[0];
    *Q = sp[1];
    return 1;

  } catch (...) {
    return hlapi_unknown_error;
  }
}

int32_t getImplSetpointFromAdv(const uint8_t *inBuffer, int32_t bufSize,
                               double *Pimp, double *Qimp, uint32_t *senderId) {
  try {
    ::kj::ArrayPtr<const capnp::byte> capnp_buffer(inBuffer, bufSize);
    ::kj::ArrayInputStream is(capnp_buffer);
    ::capnp::PackedMessageReader message(is);
    auto msg = message.getRoot<msg::Message>();
    *senderId = msg.getAgentId();

    if (!msg.hasAdvertisement()) {
      return hlapi_malformed_advertisement;
    }

    auto adv = msg.getAdvertisement();

    auto sp = adv.getImplementedSetpoint();
    *Pimp = sp[0];
    *Qimp = sp[1];
    return 1;

  } catch (...) {
    return hlapi_unknown_error;
  }
}

/*
void _sendToLocalhost(::capnp::MallocMessageBuilder &builder,
                      uint16_t destPort) {
  using boost::asio::ip::udp;
  boost::asio::io_service io_service;
  udp::socket s(io_service, udp::endpoint(udp::v4(), 0));
  s.connect(udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"),
                          destPort));
  auto fd = s.native_handle();
  writePackedMessageToFd(fd, builder);
  s.shutdown(boost::asio::ip::udp::socket::shutdown_both);
  s.close();
}

int32_t sendBatteryAdvertisement(uint16_t localPort, uint32_t agentId,
                                 double Pmin, double Pmax, double Srated,
                                 double coeffP, double coeffPsquared,
                                 double coeffPcubed, double Pimp, double Qimp) {
  try {
    ::capnp::MallocMessageBuilder builder;
    auto msg = builder.initRoot<msg::Message>();
    msg.setAgentId(agentId);
    _BatteryAdvertisement(msg.initAdvertisement(), Pmin, Pmax, Srated, coeffP, coeffPsquared,
                          coeffPcubed,Pimp,Qimp);
    _sendToLocalhost(builder,localPort);
    // write directly into the socket
    
    return 0;
  } catch (...) {
    return hlapi_unknown_error;
  }
}
*/

int32_t makeBatteryAdvertisement(uint8_t *outBuffer, int32_t maxBufSize,
                                 int32_t *packedBytesize, uint32_t agentId,
                                 double Pmin, double Pmax, double Srated,
                                 double coeffP, double coeffPsquared,
                                 double Pimp, double Qimp) {
  try {
    ::capnp::MallocMessageBuilder builder;
    auto msg = builder.initRoot<msg::Message>();
    msg.setAgentId(agentId);
    auto adv = msg.initAdvertisement();

    _BatteryAdvertisement(adv, Pmin, Pmax, Srated, coeffP, coeffPsquared,
                          Pimp,Qimp);

    auto byteSize = packToByteArray(builder, outBuffer, maxBufSize);
    *packedBytesize = byteSize;

    if (byteSize > maxBufSize)
      return hlapi_buffer_too_small;
    else
      return 0;
  } catch (...) {
    return hlapi_unknown_error;
  }
}

void _BatteryAdvertisement(msg::Advertisement::Builder adv, double Pmin,
                           double Pmax, double Srated, double coeffP,
                           double coeffPsquared,
                           double Pimp, double Qimp) {

  auto setpoint = adv.initImplementedSetpoint(2);
  setpoint.set(0, Pimp);
  setpoint.set(1, Qimp);

  // PQ Profile: intersection of disk and band
  auto pqprof = adv.initPQProfile();
  auto intsect = pqprof.initIntersection(2);
  auto disk = intsect[0].initBall();
  disk.getRadius().setReal(Srated);
  auto center = disk.initCenter(2);
  center[0].setReal(0);
  center[1].setReal(0);

  Eigen::MatrixXd A(2, 2);
  A << 1, 0, -1, 0;

  Eigen::VectorXd b(2);
  b << Pmax, -Pmin;

  cv::buildConvexPolytope(A, b, intsect[1].initConvexPolytope());

  // Identity Belief Function
  auto bf = adv.initBeliefFunction();
  auto singleton = bf.initSingleton(2);
  singleton[0].setVariable("P");
  singleton[1].setVariable("Q");

  // Polynomial Cost Function
  auto cf = adv.initCostFunction();
  cv::PolyVar Pvar{"P"};
  //cv::buildPolynomial(cf.initPolynomial(), coeffPcubed * (Pvar ^ 3) +
  //                                             coeffPsquared * (Pvar ^ 2) +
  //                                             coeffP * Pvar);

  cv::buildPolynomial(cf.initPolynomial(),
                      0.5 * (Pvar ^ 2) + coeffP / (2.0 * coeffPsquared) * Pvar);
}

/*
int32_t sendFuelCellAdvertisement(uint16_t localPort, uint32_t agentId,
                                  double Pmin, double Pmax, double Srated,
                                  double coeffP, double coeffPsquared,
                                  double coeffPcubed, double Pimp,
                                  double Qimp) {
  // fuel cell is like battery but cannot consume power
  if ((Pmin < 0) || (Pmax < 0))
    return hlapi_illegal_input;
  return sendBatteryAdvertisement(localPort, agentId, Pmin, Pmax, Srated,
                                  coeffP, coeffPsquared, coeffPcubed, Pimp,
                                  Qimp);
}
*/

int32_t makeFuelCellAdvertisement(uint8_t *outBuffer, int32_t maxBufSize,
                                  int32_t *packedBytesize, uint32_t agentId,
                                  double Pmin, double Pmax, double Srated,
                                  double coeffP, double coeffPsquared,
                                  double Pimp,
                                  double Qimp) {
  // fuel cell is like battery but cannot consume power
  if ((Pmin < 0) || (Pmax < 0))
    return hlapi_illegal_input;
  return makeBatteryAdvertisement(outBuffer, maxBufSize, packedBytesize,
                                  agentId, Pmin, Pmax, Srated, coeffP,
                                  coeffPsquared, Pimp,Qimp);
}

/*
int32_t sendPVAdvertisement(uint16_t localPort, uint32_t agentId,
                            double Srated, double Pmax, double Pdelta,
                            double tanPhi, double a_pv, double b_pv,
                            double Pimp, double Qimp) {
  try {

    if (a_pv <= 0.0)
      return hlapi_illegal_input;
    if (b_pv <= 0.0)
      return hlapi_illegal_input;
    // TODO: should we check here that Pdelta > 0 ?

    ::capnp::MallocMessageBuilder builder;
    auto msg = builder.initRoot<msg::Message>();
    msg.setAgentId(agentId);
    _PVAdvertisement(msg.initAdvertisement(), Srated, Pmax, Pdelta, tanPhi, a_pv, b_pv, Pimp, Qimp);
    _sendToLocalhost(builder,localPort);
    // write directly into the socket
    
    return 0;
  } catch (...) {
    return hlapi_unknown_error;
  }
}
*/

int32_t makePVAdvertisement(uint8_t *outBuffer, int32_t maxBufSize,
                            int32_t *packedBytesize, uint32_t agentId,
                            double Srated, double Pmax, double Pdelta,
                            double tanPhi, double a_pv, double b_pv,
                            double Pimp, double Qimp) {
  try {

    if (a_pv <= 0.0)
      return hlapi_illegal_input;
    if (b_pv <= 0.0)
      return hlapi_illegal_input;
    // TODO: should we check here that Pdelta > 0 ?

    ::capnp::MallocMessageBuilder builder;
    auto msg = builder.initRoot<msg::Message>();
    msg.setAgentId(agentId);
    auto adv = msg.initAdvertisement();

    _PVAdvertisement(adv, Srated, Pmax, Pdelta, tanPhi, a_pv, b_pv, Pimp, Qimp);

    auto byteSize = packToByteArray(builder, outBuffer, maxBufSize);
    *packedBytesize = byteSize;
    if (byteSize > maxBufSize)
      return hlapi_buffer_too_small;
    else
      return 0;
  } catch (...) {
    return hlapi_unknown_error;
  }
}

void _PVAdvertisement(msg::Advertisement::Builder adv, double Srated,
                      double Pmax, double Pdelta, double tanPhi, double a_pv,
                      double b_pv, double Pimp, double Qimp) {

  using namespace cv;

  auto setpoint = adv.initImplementedSetpoint(2);
  setpoint.set(0, Pimp);
  setpoint.set(1, Qimp);

  // PQ profile: intersection of polytope and disk
  auto pqprof = adv.initPQProfile();
  auto intsect = pqprof.initIntersection(2);

  Eigen::MatrixXd A(3, 2);
  A <<       1,  0,
       -tanPhi,  1, 
       -tanPhi, -1;

  Eigen::VectorXd b(3);
  b << Pmax, 
          0, 
          0;

  cv::buildConvexPolytope(A, b, intsect[0].initConvexPolytope());
  // the triangular shape, parameterized by Pmax and +/- tanPhi

  auto disk = intsect[1].initBall();
  disk.initRadius().setReal(Srated);
  // disk has radius equal to Srated

  auto center = disk.initCenter(2);
  center[0].setReal(0);
  center[1].setReal(0);
  // disk is centered at the origin

  // Belief function:
  //
  //  BF_delta(p,q) = Rectangle( (p1,q1) , (p2,q2) )
  //  where
  //    p1 := p
  //    q1 := q
  //    p2 := max(0,p-delta)
  //    q2 := sgn(q) * min(|q|, p2 * tan(phi))

  auto bf = adv.initBeliefFunction();
  auto rect = bf.initRectangle(2);

  Ref p2("a");
  Var P("P");
  Var Q("Q");

  buildRealExpr(rect[0].initBoundA(), P);
  buildRealExpr(rect[0].initBoundB(), name(p2, max(Real(0), P + Real(-Pdelta))));
  buildRealExpr(rect[1].initBoundA(), Q);
  buildRealExpr(rect[1].initBoundB(), sign(Q) * min(abs(Q), p2 * Real(tanPhi)));

  // Polynomial Cost Function
  auto cf = adv.initCostFunction();
  PolyVar Pvar("P");
  PolyVar Qvar("Q");
  buildPolynomial(cf.initPolynomial(), -a_pv * Pvar + b_pv * (Qvar ^ 2));

  //double c_pv;

  //buildRealExpr(adv.initCostFunction(), Real(a_pv) * pow(P, Real(2.0)) +
  //                                      Real(c_pv) * P +
  //                                      Real(b_pv) * pow(Q, Real(2)));
}

void _uncontrollableResource(msg::Advertisement::Builder adv, double Pexp,
                             double Qexp, double Srated, double dPup,
                             double dPdown, double dQup, double dQdown,
                             double Pimp, double Qimp, ResourceType resType,
                             double Pgap) {

  using namespace cv;
  constexpr auto dim = 2; // dimension of the PQ plane

  // Implemented setpoint
  auto setpoint = adv.initImplementedSetpoint(dim);
  setpoint.set(0, Pimp);
  setpoint.set(1, Qimp);

  // PQ profile: singleton
  auto sing = adv.initPQProfile().initSingleton(dim);
  sing[0].setReal(Pexp);
  sing[1].setReal(Qexp);

  // Belief: intersection of rectangle (A) with disk (B) and optionally P halfplane (C)
  auto intsect = adv.initBeliefFunction().initIntersection( (resType==ResourceType::bidirectional) ? 2 : 3);
  //auto intsect = adv.initBeliefFunction().initIntersection(3);

  // A: rectangle around (P,Q) 
  auto rect = intsect[0].initRectangle(dim);
  Var P("P");
  Var Q("Q");
  buildRealExpr(rect[0].initBoundA(), P-Real(dPdown));
  buildRealExpr(rect[0].initBoundB(), P+Real(dPup));
  buildRealExpr(rect[1].initBoundA(), Q-Real(dQdown));
  buildRealExpr(rect[1].initBoundB(), Q+Real(dQup));

  // B: converter rating (disk)
  auto disk = intsect[1].initBall();
  disk.initRadius().setReal(Srated);
  auto center = disk.initCenter(dim);
  center[0].setReal(0);
  center[1].setReal(0);

  // C: optionally positive (generator) or negative (load) halfplane in P
  if (resType != ResourceType::bidirectional) {
    Eigen::MatrixXd A(1, dim);

    A << ((resType == ResourceType::load) ? 1.0 : -1.0), 0;
    //A << -1, 0;

    Eigen::VectorXd b(1);
    b << Pgap;
    cv::buildConvexPolytope(A, b, intsect[2].initConvexPolytope());
  }

  // zero cost
  adv.initCostFunction().setReal(0);
}

void addCase(msg::ExprCase<msg::RealExpr>::Builder caseItem, double value,
             double leftIntvalBoundary, double rightIntvalBoundary) {
  // small helper function to add a case to the case distinction structure
  auto interval = caseItem.initSet().initRectangle(1);
  interval[0].initBoundA().setReal(leftIntvalBoundary);
  interval[0].initBoundB().setReal(rightIntvalBoundary);
  caseItem.initExpression().setReal(value);
}

class RoundBelief {
public:
  void makeBelief(msg::SetExpr::Builder bf, double stepSize, double error,
                  double Pmin, double Pmax) {
    using namespace cv;
    auto roundBelief = bf.initSingleton(2);
    Var P("P");

    Real step(stepSize);
    Real err(error);

    Real corr(fmod(Pmin, stepSize));
    // correction to align rounding to the value of Pmin

    auto cd = roundBelief[0].initCaseDistinction();
    cd.initVariables(1).set(0, "P"); // case distinction on P

    auto inf = std::numeric_limits<double>::infinity();
    auto cases = cd.initCases(3);
    addCase(cases[0], Pmin, -inf, Pmin + error);
    addCase(cases[1], Pmax, Pmax + error, inf);
    // if (P - error) lies outside the PQ profile, we project to the boundary

    auto interval = cases[2].initSet().initRectangle(1);
    interval[0].initBoundA().setReal(-inf);
    interval[0].initBoundB().setReal(inf);
    // else we perform rounding 
    // (we implement the "else" clause using the interval (-infinity,+infinity)
    // which is safe because case-distinction evaluation order follows list order,
    // hence this interval is checked last)

    buildRealExpr(cases[2].initExpression(),
                  step * round((P - err - corr) / step) + corr);
    roundBelief[1].setVariable("Q"); // and directly forward Q
  }
};

class ProjBelief {

public:
  void makeBelief(
      msg::SetExpr::Builder bf,
      std::vector<double> &points, // the functions modifies (sorts) the vector
      double error) {
    // build belief function: (P,Q) -> {(proj_S(P - error), Q)}
    // where the set S is here encoded as a vector<double> (named 'points')
    std::sort(points.begin(), points.end());
    auto sz = points.size();

    if (sz == 0)
      throw std::runtime_error(
          "List of implementable setpoints is empty, which is not allowed.");
    auto projBelief = bf.initSingleton(2);
    auto projFunction = projBelief[0].initCaseDistinction();
    projFunction.initVariables(1)
        .set(0, "P");               // we make a case distinction on P
    projBelief[1].setVariable("Q"); // and directly forward Q
    auto caseItems = projFunction.initCases(sz);
    auto inf = std::numeric_limits<double>::infinity();

    // For a given set of discretization points S = {P_1, P_2, ..., P_n},
    // the interpreter should round an input P to the nearest point in S.
    // We will implement this such that the interpreter first checks whether
    // P lies in [-inf, (P_1 + P_2)/2 ],
    // then whether P lies is  [-inf, (P_2 + P_3)/2 ],
    // etc., and finally, whether P lies in [-inf,inf] (which always holds)
    //
    // Hence, the intervals do not form a partition but are overlapping,
    // but this is not a problem as the order of evaluation by the interpreter
    // is
    // fixed by the list ordering of the cases in the case distinction.
    for (auto i = 0; i < sz - 1; ++i) {
      auto boundary = (points[i] + points[i + 1]) / 2.0;
      addCase(caseItems[i], points[i], -inf, boundary + error);
      // instead of subtracting the error from P, we shift the boundaries of the
      // intervals by the error
    }
    addCase(caseItems[sz - 1], points[sz - 1], -inf, inf);
  }
};

class ImplSetpoint {
  public:
  void setImplSetpoint(msg::Advertisement::Builder adv, double Pimp, double Qimp){
    auto setpoint = adv.initImplementedSetpoint(2);
    setpoint.set(0, Pimp);
    setpoint.set(1, Qimp);
  }
};

template <class BeliefPolicy>
class DiscreteDevice : public ImplSetpoint, public BeliefPolicy {
  using BeliefPolicy::makeBelief;
  using ImplSetpoint::setImplSetpoint;

public:
  template <typename... Parameters>
  void
  makeAdvertisement(msg::Advertisement::Builder adv, double Pmin, double Pmax,
                    double alpha,
                    double beta, // cost function: f(P,Q) = alpha P^2 + beta P
                    double Pimp, double Qimp, Parameters... params) {

    // PQ profile: rectangle
    auto rectangularPQprof = adv.initPQProfile().initRectangle(2);
    rectangularPQprof[0].initBoundA().setReal(Pmin);
    rectangularPQprof[0].initBoundB().setReal(Pmax);
    rectangularPQprof[1].initBoundA().setReal(Qimp);
    rectangularPQprof[1].initBoundB().setReal(Qimp);

    makeBelief(adv.initBeliefFunction(), params...);

    // Polynomial Cost Function
    auto cf = adv.initCostFunction();
    cv::PolyVar Pvar("P");
    //cv::buildPolynomial(cf.initPolynomial(), alpha * (Pvar ^ 2) + beta * Pvar);

    cv::buildPolynomial(cf.initPolynomial(),
                        0.5 * (Pvar ^ 2) + beta / (2.0 * alpha) * Pvar);

    setImplSetpoint(adv, Pimp, Qimp);
  }
};

void _realDiscreteDeviceAdvertisement(
    msg::Advertisement::Builder adv, 
    double Pmin, double Pmax,    // active power bounds
    std::vector<double> &points, // the functions modifies (sorts) the vector
    double error, double alpha,
    double beta, // cost function: f(P,Q) = alpha P^2 + beta P
    double Pimp,
    double Qimp) // implemented setpoint
{
  DiscreteDevice<ProjBelief>{}.makeAdvertisement(adv, Pmin, Pmax, alpha, beta,
                                                 Pimp, Qimp, points, error);
}

void _uniformRealDiscreteDeviceAdvertisement(
    msg::Advertisement::Builder adv, 
    double Pmin, double Pmax,  // active power bounds
    double stepSize,
    double accumulatedError,   //
    double alpha, double beta, // cost function: f(P,Q) = alpha P^2 + beta P
    double Pimp,
    double Qimp) {
  DiscreteDevice<RoundBelief>{}.makeAdvertisement(
      adv, Pmin, Pmax, alpha, beta, Pimp, Qimp, stepSize, accumulatedError, Pmin, Pmax);
}

