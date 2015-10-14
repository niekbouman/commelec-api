#define SPDLOG_DEBUG_ON
// enable logging macros

#include <capnp/message.h>
#include <kj/io.h>

#include <commelec-api/serialization.hpp>
#include <commelec-api/schema.capnp.h>
#include <commelec-api/hlapi-internal.hpp>
#include <commelec-api/sender-policies.hpp>
#include <commelec-api/json.hpp>
#include <commelec-api/adv-validation.hpp>
#include <commelec-api/coroutine-exception.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <spdlog/spdlog.h>
// logging framework

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/filesystem.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>

#include <stdexcept>


using boost::asio::ip::udp;
using PortNumberType = unsigned short;
using AgentIdType = uint32_t;

using namespace boost::filesystem;

enum class Resource {
  battery,
  pv,
  fuelcell,
  uncontrollableLoad,
  uncontrollableGenerator,
  discrete,
  discreteUnif,
  custom
};
using ResourceMap = std::unordered_map<std::string, Resource> ;

enum {
  networkBufLen = 2048, // length of data buffer for incoming requests 
  maxUDPsize = 65536,
  maxRetransmissions = 10,
  interPacketSendDelay_ms = 2
};

void createBattAdv(msg::Message::Builder msg, rapidjson::Document& d) {
  auto Pmin = getDouble(d,"Pmin");
  auto Pmax = getDouble(d,"Pmax");
  auto Srated = getDouble(d,"Srated");
  auto coeffP = getDouble(d,"coeffP");
  auto coeffPsquared = getDouble(d,"coeffPsquared");
  auto Pimp = getDouble(d,"Pimp");
  auto Qimp = getDouble(d,"Qimp");

  _BatteryAdvertisement(msg.initAdvertisement(), Pmin, Pmax, Srated, coeffP,
                        coeffPsquared, 0.0, Pimp, Qimp);
  return;
}

void createUncontrLoadAdv(msg::Message::Builder msg, rapidjson::Document &d) {
  auto Srated = getDouble(d,"Srated");
  auto Pexp = getDouble(d,"Pexp");
  auto Qexp = getDouble(d,"Qexp");
  auto dPup = getDouble(d,"dPup");
  auto dPdown = getDouble(d,"dPdown");
  auto dQup = getDouble(d,"dQup");
  auto dQdown = getDouble(d,"dQdown");
  auto Pimp = getDouble(d,"Pimp");
  auto Qimp = getDouble(d,"Qimp");

  _uncontrollableLoad(msg.initAdvertisement(), Pexp,Qexp, Srated, dPup, dPdown, dQup,
                      dQdown, Pimp, Qimp);

  return;
}

void createUncontrGenAdv(msg::Message::Builder msg, rapidjson::Document &d) {
  auto Srated = getDouble(d,"Srated");
  auto Pexp = getDouble(d,"Pexp");
  auto Qexp = getDouble(d,"Qexp");
  auto dPup = getDouble(d,"dPup");
  auto dPdown = getDouble(d,"dPdown");
  auto dQup = getDouble(d,"dQup");
  auto dQdown = getDouble(d,"dQdown");
  auto Pimp = getDouble(d,"Pimp");
  auto Qimp = getDouble(d,"Qimp");
  auto maxPowerAbsorbtion = getDouble(d,"PmaxAbsorb");

  _uncontrollableGenerator(msg.initAdvertisement(), Pexp,Qexp, Srated, dPup, dPdown, dQup,
                      dQdown, Pimp, Qimp,maxPowerAbsorbtion);

  return;
}

void createDiscreteAdv(msg::Message::Builder msg, rapidjson::Document &d) {
  auto Pmin = getDouble(d,"Pmin");
  auto Pmax = getDouble(d,"Pmax");
  auto error = getDouble(d,"error");

  std::vector<double> points;
  auto &pointList = d["points"];
  for (auto itr = pointList.Begin(); itr != pointList.End(); ++itr)
    points.push_back(itr->GetDouble());

  auto coeffP = getDouble(d,"coeffP");
  auto coeffPsquared = getDouble(d,"coeffPsquared");
  auto Pimp = getDouble(d,"Pimp");

  _realDiscreteDeviceAdvertisement(msg.initAdvertisement(), Pmin, Pmax, points,
                                   error, coeffPsquared, coeffP, Pimp);
  return;
}

void createDiscreteUnifAdv(msg::Message::Builder msg, rapidjson::Document &d) {
  auto Pmin = getDouble(d,"Pmin");
  auto Pmax = getDouble(d,"Pmax");
  auto stepsize = getDouble(d,"stepsize");
  auto error = getDouble(d,"error");

  auto coeffP = getDouble(d,"coeffP");
  auto coeffPsquared = getDouble(d,"coeffPsquared");
  auto Pimp = getDouble(d,"Pimp");

  _uniformRealDiscreteDeviceAdvertisement(msg.initAdvertisement(), Pmin, Pmax,
                                          stepsize, error, coeffPsquared,
                                          coeffP, Pimp);
  return;
}

void createFuelCellAdv(msg::Message::Builder msg, rapidjson::Document& d) {
  createBattAdv(msg, d);
}

void createPVAdv(msg::Message::Builder msg, rapidjson::Document& d) {
  auto Pmax = getDouble(d,"Pmax");
  auto Pdelta = getDouble(d,"Pdelta");
  auto Srated = getDouble(d,"Srated");

  auto cosPhi = getDouble(d,"cosPhi"); // power factor (PF) = cos(phi)
  auto tanPhi = std::sqrt( 1.0 - std::pow(cosPhi,2))/cosPhi; // tan(phi) = sqrt(1 - PF^2) / PF 

  auto a_pv = getDouble(d,"a_pv");
  auto b_pv = getDouble(d,"b_pv");

  auto Pimp = getDouble(d,"Pimp");
  auto Qimp = getDouble(d,"Qimp");

  _PVAdvertisement(msg.initAdvertisement(), Srated, Pmax, Pdelta, tanPhi, a_pv,
                   b_pv, Pimp, Qimp);

  return;
}

// The "packing"-policy lets a user of the class choose between packed
// serialisation and de-serialisation (default) or a variant that omits packing
// See also: https://capnproto.org/encoding.html#packing
// The PackedSerialization class can be found in "messaging/SenderPolicies.hpp"
template <typename PackingPolicy = PackedSerialization>
class CommelecDaemon : private PackingPolicy {
  using PackingPolicy::serializeAndAsyncSend;
  using typename PackingPolicy::CapnpReader;

public:
  CommelecDaemon(boost::asio::io_service &io_service,AgentIdType agentId, Resource resourceType,
                 PortNumberType localhost_listen_port,
                 PortNumberType network_listen_port,
                 std::vector<boost::asio::ip::udp::endpoint>& req_endpoints,
                 std::vector<boost::asio::ip::udp::endpoint>& adv_endpoints, bool debug = false)
      : _debug(debug), _agentId(agentId), _resourceType(resourceType), _strand(io_service),
        _local_socket(io_service,
                      udp::endpoint(udp::v4(), localhost_listen_port)),
        _network_socket(io_service,
                        udp::endpoint(udp::v4(), network_listen_port)),
        _outgoing_req_endpoints(req_endpoints),
        _outgoing_adv_endpoints(adv_endpoints), _timer(io_service),
        logger(spdlog::stdout_logger_mt("console"))

  {
    //boost::asio::spawn
    spawn_coroutine(_strand,
                [this](boost::asio::yield_context yield) { listenGAside(yield); });
    //boost::asio::spawn
    spawn_coroutine(_strand,
                [this](boost::asio::yield_context yield) { listenRAside(yield); });

    logger->set_level(spdlog::level::debug);
    SPDLOG_DEBUG(logger, "Started coroutines");
    // run listeners as coroutines
  }

private:
  void listenGAside(boost::asio::yield_context yield) {
    // listen on the 'network side' for Cap'n Proto-encoded requests sent by a
    // GA

    using namespace rapidjson;
    for (;;) { // run endlessly

      auto asio_buffer = boost::asio::buffer(_network_data, networkBufLen);
      boost::asio::ip::udp::endpoint sender_endpoint;
      size_t bytes_received = _network_socket.async_receive_from(
          asio_buffer, sender_endpoint, yield);
      // wait for incoming packet

      if (_resourceType == Resource::custom) {

        // forward payload to client(s)
        auto writeBuf = boost::asio::buffer(_network_data,bytes_received);
        for(const auto& ep : _outgoing_req_endpoints)
          _local_socket.async_send_to(writeBuf, ep, yield);

      } else {

        CapnpReader reader(asio_buffer);
        msg::Message::Reader msg = reader.getMessage();
        if (!msg.hasRequest())
          throw;
        auto req = msg.getRequest();
        // parse Capnp

        SPDLOG_DEBUG(logger, "Request received from GA");

        Document d;
        d.SetObject();

        Value spValid;
        auto valid = req.hasSetpoint();
        spValid.SetBool(valid);

        double P = 0.0, Q = 0.0;
        if (valid) {
          auto sp = req.getSetpoint();
          P = sp[0];
          Q = sp[1];
        }

        auto &allocator = d.GetAllocator();
        // must pass an allocator when the object may need to allocate memory

        d.AddMember("setpointValid", spValid, allocator);
        d.AddMember("senderId", msg.getAgentId(), allocator);
        d.AddMember("P", P, allocator);
        d.AddMember("Q", Q, allocator);
        // construct the JSON object

        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        d.Accept(writer);
        std::string payload = buffer.GetString();

        for(const auto& ep : _outgoing_req_endpoints)
          _local_socket.async_send_to(boost::asio::buffer(payload), ep, yield);
        // flatten to string and send packet(s)
      }
    }
  }
  void listenRAside(boost::asio::yield_context yield) {
    // listen for JSON-encoded advertisement-parameters from the RA

    using namespace rapidjson;
    for (;;) { // run endlessly

      auto asio_buffer = boost::asio::buffer(_local_data, maxUDPsize);
      boost::asio::ip::udp::endpoint sender_endpoint;
      size_t bytes_received = _local_socket.async_receive_from(asio_buffer, sender_endpoint, yield);
      // wait for incoming packet

      SPDLOG_DEBUG(logger, "Packet received from RA, bytes: {}", bytes_received);

      auto read_buffer = boost::asio::buffer(_local_data, bytes_received);
      if (_resourceType == Resource::custom) {
        // a "custom" resource prepares packed Cap'n Proto advertisements by itself,
        // we merely need to forward this payload 
        //
        // TODO (later, we will add transport-layer logic here)
        
        if(_debug){
            auto buf = boost::asio::const_buffer(read_buffer);
            AdvValidator<PackingPolicy> val(buf);
            // throws if advertisement does not pass checks
        }

        // auto bytesWritten =
        for(const auto& ep : _outgoing_adv_endpoints)
          _network_socket.async_send_to(read_buffer, ep, yield);

      } else {

        Document d;
        _local_data[bytes_received] = 0; // terminate data as C-string
        d.Parse(reinterpret_cast<const char *>(_local_data));

        if (!d.IsObject())
          throw std::runtime_error("JSON object invalid");

        // parse JSON

        capnp::MallocMessageBuilder builder;
        auto msg = builder.initRoot<msg::Message>();
        msg.setAgentId(_agentId);
        // make advertisement, depending on which resource

        switch (_resourceType) {
        case Resource::pv:
          createPVAdv(msg, d);
          break;

        case Resource::fuelcell:
          createFuelCellAdv(msg, d);
          break;

        case Resource::battery:
          createBattAdv(msg, d);
          break;

        case Resource::uncontrollableLoad:
          createUncontrLoadAdv(msg, d);
          break;

        case Resource::uncontrollableGenerator:
          createUncontrGenAdv(msg, d);
          break;

        case Resource::discrete:
          createDiscreteAdv(msg, d);
          break;

        case Resource::discreteUnif:
          createDiscreteUnifAdv(msg, d);
          break;

        default:
          break;
        }

        serializeAndAsyncSend(builder, _network_socket,
                              _outgoing_adv_endpoints, yield, _debug);
        // send packet(s)
      }
    }
  }

  //##################
  // class attributes
  //##################

  bool _debug;

  AgentIdType _agentId;
  Resource _resourceType;

  boost::asio::io_service::strand _strand;
  boost::asio::ip::udp::socket _local_socket;
  boost::asio::ip::udp::socket _network_socket;

  //boost::asio::ip::udp::endpoint _local_dest_endpoint;

  std::vector<boost::asio::ip::udp::endpoint>& _outgoing_req_endpoints; //_network_dest_endpoint;
  std::vector<boost::asio::ip::udp::endpoint>& _outgoing_adv_endpoints; //_network_dest_endpoint;
  
  boost::asio::high_resolution_timer _timer;
  // asio stuff

  capnp::byte _local_data[maxUDPsize]; //2^16 bytes (max UDP packet size is 65,507 bytes)
  capnp::byte _network_data[networkBufLen];
  // persistent arrays for storing incoming udp packets

  std::shared_ptr<spdlog::logger> logger;
};

udp::endpoint make_endpoint(std::string ip, int portnum) {
  return udp::endpoint(boost::asio::ip::address::from_string(ip), portnum);
}

void generateDefaultConfiguration(const char *configFile) {
  // generate a default configuration and write it to disk

  rapidjson::Document d;
  d.SetObject();
  auto &allocator = d.GetAllocator();
  // init JSON object

  d.AddMember("resource-type", "battery", allocator);
  d.AddMember("agent-id", 1000, allocator);
  d.AddMember("GA-ip", "127.0.0.1", allocator);
  d.AddMember("GA-port", 12345, allocator);
  d.AddMember("RA-ip", "127.0.0.1", allocator);
  d.AddMember("RA-port", 12342, allocator);
  d.AddMember("listenport-RA-side", 12340, allocator);
  d.AddMember("listenport-GA-side", 12341, allocator);
  d.AddMember("debug-mode", false, allocator);
  // populate JSON object

  writeJSONfile(configFile, d);
  // write JSON object to disk
}

int commandLineParser(int argc, char *argv[], std::string& configFile, ResourceMap& resources) {
  // This function handles command line arguments:
  //   * no command line arguments
  //   * '--generate' option
  //   * '--list-resouces' option
  //   * config file as argument
  //   # malformed arguments (i.e., more than one cl argument)
  //
  //   possibly updates configFile pointer
  //
  //   returns:
  //     false - some error occurred, program should terminate
  //     true  - configFile contains valid filename
  
  using std::cout;
  using std::endl;

  if (argc == 2) {
    if (std::string(argv[1]) == "--generate") {
      if (exists(configFile)) {

        cout << "Attempting to create '" << configFile
             << "', but that file already exists. Please remove that one first."
             << endl;
        return -1;
      }
      cout << "Generating configuration file: " << configFile << " ... ";

      generateDefaultConfiguration(configFile.c_str());

      cout << "Done." << endl << "You can now edit and customize this file." << endl;

      return -1;
    }
    else if (std::string(argv[1]) == "--list-resources") {
      cout << "Available resource types:" << endl; 
      for(auto& res: resources)
        cout << res.first << endl;
      return -1;

    } else { // command line arg is not an option
      configFile = (argv[1]);
      if (std::string(configFile) == "-")
        // read config from std input
        return 1;
      if (!exists(configFile) || !is_regular_file(configFile)) {
        cout << "Configuration file '" << configFile << "' could not be found."
             << endl;
        return -1;
      }
      return 0;
    }
  } else {
    cout << "Usage: " << argv[0] << " [config file]" << endl << endl;
    cout << "Other options:" << endl;
    cout << argv[0] << " -                    (read configuration from standard input)" << endl;
    cout << argv[0] << " --generate           (generates a default configuration)" << endl;
    cout << argv[0] << " --list-resources     (lists available resources)" << endl;
    return -1;
  }
}

void parseEndpointList(const std::string &name, rapidjson::Value &jsonObj,
                       std::vector<udp::endpoint> &epVec) {
  if (jsonObj.HasMember(name.c_str())) {
    auto &epList = jsonObj[name.c_str()];

    if (!epList.IsArray()) {
      throw std::runtime_error("Error: " + name +
                               " argument should be a list of "
                               "{\"ip\":\"<string>\",\"port\":<num>} "
                               "pairs.");
    }

    //for (auto itr = epList.Begin(); itr != epList.End(); ++itr) {
    //  epVec.emplace_back(
    //      make_endpoint(getString(*itr, "RA-ip"),
    //                    static_cast<PortNumberType>(getInt(*itr, "RA-port"))));
    //}
    //
    //
    std::for_each(epList.Begin(), epList.End(),[&epVec](const rapidjson::Value& ep) {
      epVec.emplace_back(make_endpoint(getString(ep, "ip"),
                         static_cast<PortNumberType>(getInt(ep, "port")))); });

  }
}

// main function
int main(int argc, char *argv[]) {

  ResourceMap resources({{"pv", Resource::pv},
                         {"battery", Resource::battery},
                         {"fuelcell", Resource::fuelcell},
                         {"uncontr-load", Resource::uncontrollableLoad},
                         {"uncontr-gen", Resource::uncontrollableGenerator},
                         {"custom", Resource::custom},
                         {"discrete-uniform", Resource::discreteUnif},
                         {"discrete", Resource::discrete}});

  boost::asio::io_service io_service;
  // needed for ASIO's eventloop

  auto configFile = std::string("daemon-cfg.json");
  auto status = commandLineParser(argc, argv, configFile, resources);
  // possibly updates 'configFile' filename

  rapidjson::Document cfg;
  switch (status) {
  case 0:
    cfg = readJSONfile(configFile.c_str());
    break;
  case 1:
    cfg = readJSONstdin();
    break;
  default:
    return status;
  }
  // read command line parameters

  if (!cfg.IsObject()) {
    std::cout << "Malformed JSON object in configuration file" << std::endl;
    return -1;
  }
  // read the configuration parameters from disk

  try {
    std::vector<udp::endpoint> req_dests;
    std::vector<udp::endpoint> adv_dests;

    req_dests.emplace_back(make_endpoint(getString(cfg, "RA-ip"),
                static_cast<PortNumberType>(getInt(cfg, "RA-port"))));
    adv_dests.emplace_back(make_endpoint(getString(cfg, "GA-ip"),
                static_cast<PortNumberType>(getInt(cfg, "GA-port"))));

    parseEndpointList("clone-req",cfg,req_dests);
    parseEndpointList("clone-adv",cfg,adv_dests);
    // possibly more destinations to which packets (requests or advertisements)
    // should be sent

    CommelecDaemon<> daemon(
        io_service, getInt(cfg, "agent-id"),
        resources.at(getString(cfg, "resource-type")),
        static_cast<PortNumberType>(getInt(cfg, "listenport-RA-side")),
        static_cast<PortNumberType>(getInt(cfg, "listenport-GA-side")),
        req_dests,adv_dests,
        getBool(cfg, "debug-mode", false));
    //instantiate our main class, with the parameters as set by the user in the config file
    // debug-mode is an optional parameter

    io_service.run();
    // run asio's event-loop; used for asynchronous network IO using coroutines

  } catch (std::runtime_error& e) {
    std::cout << "Exception: '" << e.what() << std::endl;
    return -1;
  } catch (std::out_of_range& e) {
    std::cout << "Config error - unknown resource type: "
              << getString(cfg, "resource-type") << std::endl;
    return -1;    
  } 
  
#ifndef __arm__
// armel arch does not support exception_ptr 
//
  catch (const std::exception_ptr &ep)
  // Exceptions coming from inside coroutines (these are wrapped in an
  // exception_ptr and have to be rethrown, see below)
  {
    try {
      std::rethrow_exception(ep);
      // rethrow the exception that the exception pointer points to
    } catch (const std::exception &e) {
      std::cout << "Exception occurred!" << std::endl;
      std::cout << e.what() << std::endl;
    }
  }
#endif

}

