// modified from:
//
// echo_server.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <commelec-api/sender-policies.hpp>
#include <commelec-interpreter/adv-interpreter.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <iostream>
#include <memory>
#include <limits>
#include <vector>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <Eigen/Core> 
using boost::asio::ip::tcp;

enum { headerLen = 4 };

struct header_t {
  uint32_t jsonLen;
  uint32_t capnpLen; 
};

template <typename PackingPolicy = PackedSerialization>
class TCPSession : public std::enable_shared_from_this<TCPSession<>>,
                private PackingPolicy {
  using PackingPolicy::serializeAndAsyncSend;
  using typename PackingPolicy::CapnpReader;

public:
  enum class ErrorCode : int { not_an_advertisement = 1 };

  explicit TCPSession(tcp::socket socket)
      : socket_(std::move(socket)), timer_(socket_.get_io_service()),
        strand_(socket_.get_io_service()) {}

  void addError(rapidjson::Value &errorsArray,
                rapidjson::Document::AllocatorType &allocator, ErrorCode code,
                const std::string &msg) {

    using namespace rapidjson;

    Value error(kObjectType);
    error.AddMember("code", code, allocator);
    error.AddMember("msg", msg, allocator);
    errorsArray.PushBack(error, allocator);
  }

  void renderCostFunction(rapidjson::Document &request,
                          rapidjson::Document &response,
                          msg::Advertisement::Reader adv) {

    AdvFunc interpreter(adv);
    auto pqProf = adv.getPQProfile();
    Eigen::AlignedBoxXd bb = interpreter.rectangularHull(pqProf, {});

    Eigen::VectorXd min(bb.min());
    Eigen::VectorXd max(bb.max());

    Eigen::VectorXd p, q;
    if (request.HasMember("resP") && request.HasMember("resQ")) {
      auto resP = request["resP"].GetDouble();
      auto resQ = request["resQ"].GetDouble();

      auto minP = resP * std::floor(min(0) / resP);
      auto maxP = resP * std::ceil(max(0) / resP);

      auto minQ = resQ * std::floor(min(1) / resQ);
      auto maxQ = resQ * std::ceil(max(1) / resQ);

      p.setLinSpaced(resP, minP, maxP);
      q.setLinSpaced(resQ, minQ, maxQ);

    } else if (request.HasMember("dimP") && request.HasMember("dimQ")) {
      p.setLinSpaced(request["dimP"].GetInt(), min(0), max(0));
      q.setLinSpaced(request["dimQ"].GetInt(), min(1), max(1));
    } else {
      // specify dim or res
      return;
    }

    auto dimP = p.size();
    auto dimQ = q.size();

    Eigen::MatrixXd result(dimP, dimQ);

    auto costFun = adv.getCostFunction();

    double nan = std::numeric_limits<double>::quiet_NaN();

    for (auto i = 0; i < dimP; ++i) {
      for (auto j = 0; j < dimQ; ++j) {
        auto isMember = interpreter.testMembership(pqProf, {p(i), q(j)}, {});
        if (isMember) {
          result(i, j) =
              interpreter.evaluate(costFun, {{"P", p(i)}, {"Q", q(j)}});
        } else {
          result(i, j) = nan;
        }
      }
    }

    using namespace rapidjson;
    rapidjson::Value compressedMatrix(kArrayType);

    auto totalDim = dimP*dimQ;
    auto data = result.data();

    int i =0;
    int count = 0;
    while (i < totalDim) {
      auto datum = data[i];

      if (datum == nan) {
        ++count;
        continue;
      }

      if (count > 0) {
        compressedMatrix.PushBack(Value().SetDouble(nan), allocator);
        compressedMatrix.PushBack(Value().SetInt(count), allocator);
        count = 0;
      } else
        // write char
        compressedMatrix.PushBack(Value().SetDouble(datum), allocator);
    }
    // terminate properly
    if (count > 0) {
        compressedMatrix.PushBack(Value().SetDouble(nan), allocator);
        compressedMatrix.PushBack(Value().SetInt(count), allocator);
    } 




  }

  void sessionLoop() {
    using namespace rapidjson;
    auto self(shared_from_this());
    boost::asio::spawn(strand_, [this, self](boost::asio::yield_context yield) {
      try {
        for (;;) {
          // main loop per client

          timer_.expires_from_now(std::chrono::seconds(10));
          // in-case of no activity, session expires after this duration

          Document jsonReply;
          jsonReply.SetObject();

          Value errors(kArrayType);
          // json array for reporting errors back to the client

          // read header (contains length)
          header_t header;
          boost::asio::async_read(
              socket_, boost::asio::buffer(&header, sizeof(header)), yield);

          // read payload
          jsonData.resize(header.jsonLen); // increase buffer size if necessary
          boost::asio::async_read(socket_, boost::asio::buffer(jsonData),
                                  yield);
          Document d;
          d.Parse(jsonData.c_str());

          capnpData.resize(
              header.capnpLen); // increase buffer size if necessary
          auto asio_buffer = boost::asio::buffer(capnpData);
          boost::asio::async_read(socket_, asio_buffer, yield);
          CapnpReader reader(asio_buffer);

          msg::Message::Reader msg = reader.getMessage();
          if (!msg.hasAdvertisement()) {
            // capnproto msg is not an advertisement

            addError(errors, jsonReply.GetAllocator(),
                     ErrorCode::not_an_advertisement,
                     "capnproto payload is not an advertisement");
          } else {

            AdvFunc interpreter(msg.getAdvertisement());
            // renderCostFunction();
            // renderBeliefFunction();

            // runlengthCompression();
          }

 
          // serialize to json

          if (errors.MemberCount() > 0)
          {
            // add errors to json object if there were any
            jsonReply.AddMember("errors", errors, jsonReply.GetAllocator());
          }

          StringBuffer buffer;
          Writer<StringBuffer> writer(buffer);
          jsonReply.Accept(writer);
          std::string payload = buffer.GetString();

          uint32_t len = payload.size();
          std::string lenHeader{sizeof(len), '0'};
          // initialize string with the right length (the '0' char is an
          // arbitrary choice)

          auto headerPtr = const_cast<decltype(len) *>(
              reinterpret_cast<const decltype(len) *>(lenHeader.data()));
          // make (non-const) pointer to string-data

          using ::boost::asio::detail::socket_ops::host_to_network_long;
          *headerPtr = host_to_network_long(len);
          // write length in network-order (big-endian) to string

          boost::asio::async_write(
              socket_, boost::asio::buffer(lenHeader + payload), yield);
        }
      } catch (std::exception &e) {
        socket_.close();
        timer_.cancel();
      }
    });

    boost::asio::spawn(strand_, [this, self](boost::asio::yield_context yield) {
      while (socket_.is_open()) {
        boost::system::error_code ignored_ec;
        timer_.async_wait(yield[ignored_ec]);
        if (timer_.expires_from_now() <= std::chrono::seconds(0))
          socket_.close();
      }
    });
  }

private:
  std::vector<uint8_t> capnpData;
  std::string jsonData;

  tcp::socket socket_;
  boost::asio::steady_timer timer_;
  boost::asio::io_service::strand strand_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: " << argv[0] << " <port>\n";
      return 1;
    }

    boost::asio::io_service io_service;

    boost::asio::spawn(io_service,
        [&](boost::asio::yield_context yield)
        {
          tcp::acceptor acceptor(io_service,
            tcp::endpoint(tcp::v4(), std::atoi(argv[1])));

          for (;;)
          {
            boost::system::error_code ec;
            tcp::socket socket(io_service);
            acceptor.async_accept(socket, yield[ec]);
            if (!ec) std::make_shared<TCPSession<>>(std::move(socket))->sessionLoop();
          }
        });

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
