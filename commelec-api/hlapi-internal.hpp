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
#ifndef HIGHLEVEL_API_INT_HPP
#define HIGHLEVEL_API_INT_HPP

// This header file exposes functions that do not have a C interface (but a C++ one instead)
// (The hlapi.h header exposes functions as extern "C")

#include <commelec-api/schema.capnp.h> 
#include <capnp/message.h>
#include <vector>

void _BatteryAdvertisement(msg::Advertisement::Builder adv, double Pmin,
                           double Pmax, double Srated, double coeffP,
                           double coeffPsquared,
                           double Pimp, double Qimp);

void _PVAdvertisement(msg::Advertisement::Builder adv, double Srated,
                      double Pmax, double Pdelta, double tanPhi, double a_pv,
                      double b_pv, double Pimp, double Qimp);

enum class ResourceType { load, generator, bidirectional };
void _uncontrollableResource(msg::Advertisement::Builder adv, double Pexp,
                             double Qexp, double Srated, double dPup,
                             double dPdown, double dQup, double dQdown,
                             double Pimp, double Qimp, ResourceType resType,
                             double Pgap);

inline void _uncontrollableLoad(msg::Advertisement::Builder adv, double Pexp,
                                double Qexp, double Srated, double dPup,
                                double dPdown, double dQup, double dQdown,
                                double Pimp, double Qimp) {
  _uncontrollableResource(adv, Pexp, Qexp, Srated, dPup, dPdown, dQup, dQdown,
                          Pimp, Qimp, ResourceType::load, 0.0);
};
// Advertisement for an uncontrollable load:
//   PQ profile = singleton(Pexp,Qexp)
//   Belief = intersection of:
//                  Rectangle ( (P-dPdown, Q-dQdown), (P+dPup, Q+dQup) )
//                  Disk (center=(0,0),radius=Srated)
//                  Halfplane (P<=0) (only consumption of real power)
//   Cost = 0 (constant function)
//   Implemented setpoint = (Pimp,Qimp)

inline void _uncontrollableGenerator(msg::Advertisement::Builder adv, double Pexp,
                                double Qexp, double Srated, double dPup,
                                double dPdown, double dQup, double dQdown,
                                double Pimp, double Qimp, double Pgap) {
  _uncontrollableResource(adv, Pexp, Qexp, Srated, dPup, dPdown, dQup, dQdown,
                          Pimp, Qimp, ResourceType::generator,Pgap);
};
// Advertisement for an uncontrollable generator:
//   PQ profile = singleton(Pexp, Qexp)
//   Belief = intersection of:
//                  Rectangle ( (P-dPdown, Q-dQdown), (P+dPup, Q+dQup) )
//                  Disk (center=(0,0),radius=Srated)
//                  Halfplane (P>=0) (only production of real power)
//   Cost = 0 (constant function)
//   Implemented setpoint = (Pimp,Qimp)

void _realDiscreteDeviceAdvertisement(
    msg::Advertisement::Builder adv, double Pmin,
    double Pmax, // active power bounds
    std::vector<double> &discretizationPoints,
    double accumulatedError,   //
    double alpha, double beta, // cost function: f(P,Q) = alpha P^2 + beta P
    double Pimp, double Qimp);

void _uniformRealDiscreteDeviceAdvertisement(
    msg::Advertisement::Builder adv, double Pmin,
    double Pmax, // active power bounds
    double stepSize,
    double accumulatedError,   //
    double alpha, double beta, // cost function: f(P,Q) = alpha P^2 + beta P
    double Pimp, double Qimp);

void _zenoneAdvertisement(
    msg::Advertisement::Builder adv, double Pmin,
    double Pmax, // active power bounds
    double stepSize,
    double accumulatedError,   //
    double alpha, double beta, // cost function: f(P,Q) = alpha P^2 + beta P
    double Pimp, double Qimp);


#endif

