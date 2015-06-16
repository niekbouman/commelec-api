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
#include "hlapi.h"
#include "hlapi-internal.hpp"
#include "schema.capnp.h"
#include "serialization.hpp"
#include "polytope-convenience.hpp"
#include "polynomial-convenience.hpp"
#include "realexpr-convenience.hpp"

#include <cstring>
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
                                 double coeffPcubed, double Pimp, double Qimp) {
  try {
    ::capnp::MallocMessageBuilder builder;
    auto msg = builder.initRoot<msg::Message>();
    msg.setAgentId(agentId);
    auto adv = msg.initAdvertisement();

    _BatteryAdvertisement(adv, Pmin, Pmax, Srated, coeffP, coeffPsquared,
                          coeffPcubed,Pimp,Qimp);

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
                           double coeffPsquared, double coeffPcubed,
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
  cv::buildPolynomial(cf.initPolynomial(), coeffPcubed * (Pvar ^ 3) +
                                               coeffPsquared * (Pvar ^ 2) +
                                               coeffP * Pvar);
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
                                  double coeffPcubed, double Pimp,
                                  double Qimp) {
  // fuel cell is like battery but cannot consume power
  if ((Pmin < 0) || (Pmax < 0))
    return hlapi_illegal_input;
  return makeBatteryAdvertisement(outBuffer, maxBufSize, packedBytesize,
                                  agentId, Pmin, Pmax, Srated, coeffP,
                                  coeffPsquared, coeffPcubed,Pimp,Qimp);
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
}

