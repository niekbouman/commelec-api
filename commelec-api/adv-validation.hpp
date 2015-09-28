#ifndef ADVVALIDATE_HPP
#define ADVVALIDATE_HPP

#include <commelec-api/schema.capnp.h>
#include <commelec-api/sender-policies.hpp>
#include <commelec-interpreter/adv-interpreter.hpp>
#include <iostream>
#include <Eigen/Geometry>
#include <stdexcept>
#include <boost/asio.hpp>

struct InvalidAdvertisement : public std::runtime_error {
  InvalidAdvertisement(const std::string &msg = "") : std::runtime_error(msg) {}
};
struct UninitializedPQProfile: public InvalidAdvertisement {};
struct UninitializedBeliefFunction: public InvalidAdvertisement {};
struct UninitializedCostFunction: public InvalidAdvertisement {};
struct UninitializedImplementedSetpoint: public InvalidAdvertisement {};

enum { cfEvaluations = 100 , bfEvaluations = 100 };

template <typename PackingPolicy>
class AdvValidator : private PackingPolicy {
  using typename PackingPolicy::CapnpReader;

public:
  AdvValidator(boost::asio::const_buffer asio_buffer): beliefBB(2)  {
    std::cout << "[Commelec advertisement validation procedure]" << std::endl;

    // this constructor performs unpacking and deep-copies the capnproto
    // message as additional verification steps (recommended)

    std::cout << "Validating structure of the Cap'n Proto message...";
    
    CapnpReader reader(asio_buffer);
    _advStorage.setRoot(reader.getMessage());
    // enforce a deep copy of the data to a MallocMessageBuilder
    // (a deep copy forces validation of the cap'n proto object (see cap'n proto
    // google groups) )
    //

    auto msg = _advStorage.getRoot<msg::Message>().asReader();
    // by using asReader() on a MallocMessageBuilder, we disable the capnp
    // traversal limit

    if (!msg.hasAdvertisement())
      throw InvalidAdvertisement{};

    _adv = msg.getAdvertisement();
    interpreter.setAdv(_adv);


    std::cout << "done" << std::endl;
    initAndValidate();

    std::cout << "[End of validation procedure]" << std::endl;
  }

  AdvValidator(msg::Advertisement::Reader adv)
      : _adv(adv), interpreter(adv), beliefBB(2) {
    // this constructor assumes that the message has already been unpacked

    std::cout << "[Commelec advertisement validation procedure]" << std::endl;
    initAndValidate();
    std::cout << "[End of validation procedure]" << std::endl;
  }

private:
  void initAndValidate() {

    checkDefinitions();
    // check if PQ profile, Cost Function, and Belief Function are defined, and
    // whether implemented setpoint has proper length (2)

    std::cout << "PQ profile, Cost Function, and Belief Function are defined, "
                 "and Implemented Setpoint has proper length" << std::endl;

    computeBoundingBoxAroundPQProfile();
      
    // check evaluation of Cost Function (on random points)
    std::cout << "Evaluating Cost Function on " << cfEvaluations << " random points in the PQ profile...";
    dummy = 0;
    for (auto i = 0; i < cfEvaluations; ++i) {
      dummy += evalCostFuncOnRandomPoint();
      // prevent compiler from optimizing away unused results
    }
    std::cout << "done" << std::endl;

    std::cout << "Evaluating Belief Function on " << bfEvaluations
              << " random points in the PQ profile, and computing the "
                 "axis-aligned bounding box of each uncertainty set...";
    beliefBB.setEmpty();
    // check evaluation of the Belief Function (on random points)
    // and the computation of the rectangular hull around this evaluated set
    for (auto i = 0; i < bfEvaluations; ++i) {
      beliefBB.merged(evalBeliefBoxOnRandomPoint());
      // prevent compiler from optimizing away unused results
    }
    std::cout << "done" << std::endl;
  }

  void checkDefinitions() {
    if (!_adv.hasPQProfile()) throw UninitializedPQProfile{};
    if (!_adv.hasCostFunction()) throw UninitializedCostFunction{};
    if (!_adv.hasBeliefFunction()) throw UninitializedBeliefFunction{};

    auto iSP = _adv.getImplementedSetpoint();
    if (iSP.size() != 2) throw UninitializedImplementedSetpoint{};
  }

  double evalCostFuncOnRandomPoint() {
    // Evaluate cost function on a random point in the PQ profile

      Eigen::VectorXd randomPoint(2);
      
      randomPoint = sampleSetpoint();
    // sample random point in the PQ profile

    return interpreter.evaluate(_adv.getCostFunction(), {{"P", randomPoint[0]}, {"Q", randomPoint[1]}});
  }

  Eigen::AlignedBoxXd evalBeliefBoxOnRandomPoint() {
    // Evaluate belief function on a random point in the PQ profile, and compute its rectangular hull

    Eigen::VectorXd randomPoint(sampleSetpoint());
    // sample random point in the PQ profile

    return interpreter.rectangularHull(_adv.getBeliefFunction(), {{"P", randomPoint[0]}, {"Q", randomPoint[1]}});
  }

  void computeBoundingBoxAroundPQProfile() {
    PQprofBB = interpreter.rectangularHull(_adv.getPQProfile(), {});
    // try to compute axis aligned bounding box around PQ profile
    // this could (or will?) fail if the PQ profile is unbounded
  }

  inline Eigen::VectorXd sample(const Eigen::AlignedBoxXd &b) const {
    Eigen::VectorXd r(b.dim());
    for (auto d = 0; d < b.dim(); ++d) {
      r[d] = Eigen::internal::random(b.min()[d], b.max()[d]);
    }
    return r;
  }

  Eigen::VectorXd sampleSetpoint()
  // sample a random point in the PQ profile
  {
    // method: sample random point in the bounding box,
    //  accept if: point is inside the PQ profile,
    //  otherwise: try again

    auto PQprof = _adv.getPQProfile();
    Eigen::VectorXd randomPoint(2);
    do {
        randomPoint = sample(PQprofBB);
        //randomPoint = PQprofBB.sample();
        // the line above does currently not work, seems to be a bug in Eigen
        // the sample() function is a work-around
        
    } while (!interpreter.testMembership(PQprof, {randomPoint[0], randomPoint[1]}, {}));
    return randomPoint;
  }

  capnp::MallocMessageBuilder _advStorage;
  AdvFunc interpreter;
  Eigen::AlignedBoxXd PQprofBB;
  Eigen::AlignedBoxXd beliefBB;
  msg::Advertisement::Reader _adv;
  double dummy;
};

#endif
