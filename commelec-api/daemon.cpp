#define SPDLOG_DEBUG_ON
// enable logging macros

#include <capnp/message.h>
#include <kj/io.h>

#include <commelec-api/serialization.hpp>
#include <commelec-api/schema.capnp.h>
#include <commelec-api/hlapi-internal.hpp>
#include <commelec-api/sender-policies.hpp>
#include <commelec-api/json.hpp>

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

#include <iostream>
#include <string>
#include <unordered_map>

using boost::asio::ip::udp;
using PortNumberType = unsigned short;
using AgentIdType = uint32_t;

using namespace boost::filesystem;

enum class Resource { battery, pv, fuelcell };
using ResourceMap = std::unordered_map<std::string, Resource> ;

enum {
  networkBufLen = 2048, // length of data buffer for incoming requests 
  maxUDPsize = 65536,
  maxRetransmissions = 10,
  interPacketSendDelay_ms = 2
};

void createBattAdv(msg::Message::Builder msg, rapidjson::Document& d) {
  auto Pmin = d["Pmin"].GetDouble();
  auto Pmax = d["Pmax"].GetDouble();
  auto Srated = d["Srated"].GetDouble();
  auto coeffP = d["coeffP"].GetDouble();
  auto coeffPsquared = d["coeffPsquared"].GetDouble();
  auto Pimp = d["Pimp"].GetDouble();
  auto Qimp = d["Qimp"].GetDouble();

  _BatteryAdvertisement(msg.initAdvertisement(), Pmin, Pmax, Srated, coeffP,
                        coeffPsquared, 0.0, Pimp, Qimp);
  return;
}

void createFuelCellAdv(msg::Message::Builder msg, rapidjson::Document& d) {
  createBattAdv(msg, d);
}

void createPVAdv(msg::Message::Builder msg, rapidjson::Document& d) {
  auto Pmax = d["Pmax"].GetDouble();
  auto Pdelta = d["Pdelta"].GetDouble();
  auto Srated = d["Srated"].GetDouble();

  auto cosPhi = d["cosPhi"].GetDouble(); // power factor (PF) = cos(phi)
  auto tanPhi = std::sqrt( 1.0 - std::pow(cosPhi,2))/cosPhi; // tan(phi) = sqrt(1 - PF^2) / PF 

  auto a_pv = d["a_pv"].GetDouble();
  auto b_pv = d["b_pv"].GetDouble();

  auto Pimp = d["Pimp"].GetDouble();
  auto Qimp = d["Qimp"].GetDouble();

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
                 const boost::asio::ip::udp::endpoint &local_dest_endpoint,
                 const boost::asio::ip::udp::endpoint &network_dest_endpoint)
      : _agentId(agentId), _resourceType(resourceType), _strand(io_service),
        _local_socket(io_service,
                      udp::endpoint(udp::v4(), localhost_listen_port)),
        _network_socket(io_service,
                        udp::endpoint(udp::v4(), network_listen_port)),
        _local_dest_endpoint(local_dest_endpoint),
        _network_dest_endpoint(network_dest_endpoint), _timer(io_service),
        logger(spdlog::stdout_logger_mt("console"))

  {
    boost::asio::spawn(_strand,
                [this](boost::asio::yield_context yield) { listenGAside(yield); });
    boost::asio::spawn(_strand,
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
      size_t bytes_received =
          _network_socket.async_receive_from(asio_buffer, sender_endpoint, yield);
      // wait for incoming packet

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

      double P, Q;
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
      _local_socket.async_send_to(boost::asio::buffer(payload), _local_dest_endpoint,
                                    yield);
      // flatten to string and send packet
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

      Document d;
      _local_data[bytes_received]=0; // terminate data as C-string
      d.Parse(reinterpret_cast<const char*>(_local_data)); 
      // parse JSON
      

      auto msg = _builder.initRoot<msg::Message>();
      msg.setAgentId(_agentId);
      // make advertisement, depending on which resource

      switch (_resourceType){
        case Resource::pv:
          createPVAdv(msg,d);
          break;

        case Resource::fuelcell:
          createFuelCellAdv(msg,d);
          break;

        case Resource::battery:
          createBattAdv(msg,d);
          break;
      }

      serializeAndAsyncSend(_builder, _network_socket, _network_dest_endpoint,yield);
      // send packet

    }
  }


  //##################
  // class attributes
  //##################


  AgentIdType _agentId;
  Resource _resourceType;

  boost::asio::io_service::strand _strand;
  boost::asio::ip::udp::socket _local_socket;
  boost::asio::ip::udp::socket _network_socket;
  boost::asio::ip::udp::endpoint _network_dest_endpoint;
  boost::asio::ip::udp::endpoint _local_dest_endpoint;
  boost::asio::high_resolution_timer _timer;
  // asio stuff

  capnp::byte _local_data[maxUDPsize]; //2^16 bytes (max UDP packet size is 65,507 bytes)
  capnp::byte _network_data[networkBufLen];
  // persistent arrays for storing incoming udp packets

  capnp::MallocMessageBuilder _builder;
  // builder for storing outgoing (repeated) packets

  std::shared_ptr<spdlog::logger> logger;
};

udp::endpoint make_endpoint(std::string ip, int portnum) {
  return udp::endpoint(boost::asio::ip::address::from_string(ip), portnum);
}

void generateDefaultConfiguration(const char *configFile){
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
      // populate JSON object

      writeJSONfile(configFile, d);
      // write JSON object to disk
}


int commandLineParser(int argc, char *argv[], std::string configFile, ResourceMap& resources) {
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
      cout << "Generating configuration file... (This file can "
              "be edited by hand)" << endl;

      generateDefaultConfiguration(configFile.c_str());

      cout << "Done." << endl;
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


// main function
int main(int argc, char *argv[]) {

  ResourceMap resources({{"pv", Resource::pv},
                         {"battery", Resource::battery},
                         {"fuelcell", Resource::fuelcell}});

  boost::asio::io_service io_service;
  // needed for ASIO's eventloop

  auto configFile = "daemon-cfg.json";
  auto status = commandLineParser(argc, argv, configFile, resources);
  // possibly updates 'configFile' filename

  rapidjson::Document cfg;
  switch (status) {
  case 0:
    cfg = readJSONfile(configFile);
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
    CommelecDaemon<> daemon(
        io_service, getInt(cfg, "agent-id"),
        resources.at(getString(cfg, "resource-type")),
        static_cast<PortNumberType>(getInt(cfg, "listenport-RA-side")),
        static_cast<PortNumberType>(getInt(cfg, "listenport-GA-side")),
        make_endpoint(getString(cfg, "RA-ip"),
                      static_cast<PortNumberType>(getInt(cfg, "RA-port"))),
        make_endpoint(getString(cfg, "GA-ip"),
                      static_cast<PortNumberType>(getInt(cfg, "GA-port"))));
    //instantiate our main class, with the parameters as set by the user in the config file

    io_service.run();
    // run asio's event-loop; used for asynchronous network IO using coroutines


  } catch (std::runtime_error e) {
    std::cout << "Config error: '" << e.what() << "' missing" << std::endl;
    return -1;
  } catch (std::out_of_range e) {
    std::cout << "Config error - unknown resource type: "
              << getString(cfg, "resource-type") << std::endl;
    return -1;
  }
}

