#include <boost/asio.hpp>
#include <capnp/serialize-packed.h>
#include <commelec-api/schema.capnp.h>
#include <commelec-api/asio-kj-interop.hpp>
#include <commelec-api/hlapi.h>
#include <commelec-api/hlapi-internal.hpp>
#include <commelec-api/serialization.hpp>
#include <commelec-api/sender-policies.hpp>
#include <Eigen/Core>
#include <iostream>
#include <vector>

using boost::asio::ip::udp;

enum { default_port_number = 12345 };

int main(int argc, char *argv[]) {

  using CapnpReader = PackedSerialization::CapnpReader;

  auto portnum = 0;
  if (argc < 2) {
    std::cout << "No port number specified. Using port " << default_port_number
              << " (on localhost)." << std::endl;
    std::cout << "To specify a custom port number, use: " << argv[0]
              << " [port number]" << std::endl;
    portnum = default_port_number;
  } else {
    portnum = atoi(argv[1]);
  }

  boost::asio::io_service io_service;
  auto addr = boost::asio::ip::address::from_string("127.0.0.1");

  udp::socket socket(io_service, udp::endpoint(udp::v4(), 10000));

  capnp::MallocMessageBuilder builder;
  capnp::MallocMessageBuilder validator;

  auto msg = builder.initRoot<msg::Message>();

  msg.setAgentId(10);

  _PVAdvertisement(msg.initAdvertisement(), 12.1, 12.7, 7,
                   std::tan(15.0 / 180.0 * M_PI), 1, 1, 1, 1);
  //_BatteryAdvertisement(msg.initAdvertisement(), -5,10, 12 , 1,1,1,1,1);

  std::vector<uint8_t> packedDataBuffer(messageByteSize(builder));

  // buffer to hold the packed advertisement
  // ( messageByteSize(msg) is a crude upper bound for the buffer size.
  // The exact size that we will actually need is unknown at this point)

  writePackedMessage(packedDataBuffer, builder);

  // This is merely for testing purposes
  CapnpReader reader(boost::asio::buffer(packedDataBuffer));
  validator.setRoot(reader.getMessage());


  // Write the data to the udp socket
  udp::endpoint endpoint(addr, portnum);
  auto bytes_written =
      socket.send_to(boost::asio::buffer(packedDataBuffer), endpoint);
  std::cout << "Number of bytes written to the socket: " << bytes_written
            << std::endl;
  return 0;

}
