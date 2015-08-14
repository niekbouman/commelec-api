#include <iostream>
#include <boost/asio.hpp>
#include <commelec-api/schema.capnp.h>
#include <commelec-api/serialization.hpp>

using boost::asio::ip::udp;

enum { default_port_number = 12345 };

int main(int argc, char *argv[]) {
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

  ::capnp::MallocMessageBuilder builder;

  auto msgRoot = builder.initRoot<msg::Message>();

  msgRoot.setAgentId(1500);

  auto req = msgRoot.initRequest();
  auto sp = req.initSetpoint(2);
  sp.set(0, 10);
  sp.set(1, 5);

  std::vector<uint8_t> packedDataBuffer(messageByteSize(builder));
  writePackedMessage(packedDataBuffer, builder);
  // will resize packedDataBuffer to the exact size of the packed data

  udp::endpoint endpoint(addr, portnum);
  auto bytes_written =
      socket.send_to(boost::asio::buffer(packedDataBuffer), endpoint);

  std::cout << "Number of bytes written to the socket: " << bytes_written
            << std::endl;
  return 0;
}
