#ifndef HIGHLEVEL_API_INT_HPP
#define HIGHLEVEL_API_INT_HPP

#include "schema.capnp.h"
//#include "schema.capnp.h"

void _BatteryAdvertisement(msg::Advertisement::Builder adv, double Pmin, double Pmax, double Srated,
                                 double coeffP, double coeffPsquared,
                                 double coeffPcubed, double Pimp, double Qimp);

void _PVAdvertisement(msg::Advertisement::Builder adv, double Srated, double Pmax, double Pdelta,
                            double tanPhi, double a_pv, double b_pv, double Pimp, double Qimp);
#endif

