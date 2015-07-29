// visualization server
// Copyright (c) 2015 Niek J. Bouman
//
//
// modified from:
// echo_server.cpp (from Boost Asio documentation, by 
// Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Boost Software License, Version 1.0.)

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
#include <set>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <Eigen/Core> 
using boost::asio::ip::tcp;

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

  template <typename DerivedA, typename DerivedB>
  Eigen::MatrixXd
  evalFunOnPQDomain(msg::RealExpr::Reader pqFun, msg::SetExpr::Reader pqDomain, 
                    AdvFunc &interpreter,
                    const Eigen::MatrixBase<DerivedA> &evalPointsP,
                    const Eigen::MatrixBase<DerivedB> &evalPointsQ)

  {
    // Evaluate a function in P and Q (e.g., a cost function) on a domain in the
    // PQ plane (e.g., a PQ profile)
    //
    // The vectors of evaluation points (evalPointsP and evalPointsQ) define a
    // rectangular grid in the PQ plane. The result of this function (a
    // double-valued matrix) has the same dimensions as this rectangular grid.
    //
    // For every point on the grid, we first test whether the grid point lies in
    // the specified domain:
    //  if not, we set the corresponding entry in the result-matrix to "NaN"
    //  if the point lies in the domain, then we evaluate the function on this
    //  point and store the function value in the result-matrix

    auto dimP = evalPointsP.size();
    auto dimQ = evalPointsQ.size();

    Eigen::MatrixXd result(dimP, dimQ);

    double nan = std::numeric_limits<double>::quiet_NaN();

    for (auto i = 0; i < dimP; ++i)
      for (auto j = 0; j < dimQ; ++j) {
        auto isMember = interpreter.testMembership(
            pqDomain, {evalPointsP(i), evalPointsQ(j)}, {});
        if (isMember)
          result(i, j) = interpreter.evaluate(
              pqFun, {{"P", evalPointsP(i)}, {"Q", evalPointsQ(j)}});
        else
          result(i, j) = nan;
      }
  }

  rapidjson::Value
  runlengthCompress(const Eigen::MatrixXd &m, std::set<double> compressVals,
                    rapidjson::Document::AllocatorType &allocator) {

    // apply runlength compression to a matrix and output the compressed matrix
    // as a json array:
    //
    // example:
    // compressVals = {NaN}
    // m = [NaN, 1.0]
    //     [NaN, 2.0]
    //
    // m has column-major storage (Eigen's default).
    //
    // Output (json)
    //
    // {[NaN, 2, 1.0, 2.0]}

    using namespace rapidjson;
    Value result(kArrayType);

    size_t totalDim = m.rows() * m.cols();
    auto data = m.data();

    int i = 0;
    int count = 0;
    double activeVal;

    while (i < totalDim) {
      auto datum = data[i];

      if (count > 0) {
        if (datum == activeVal) {
          ++count;
          continue;
        } else {
          result.PushBack(Value().SetDouble(activeVal), allocator);
          result.PushBack(Value().SetInt(count), allocator);
          result.PushBack(Value().SetDouble(datum), allocator);
          count = 0;
          continue;
        }
      } else if (compressVals.count(datum)) {
        activeVal = datum;
        count = 1;
        continue;
      } else
        // an 'ordinary' value, to which our compression method does not apply
        result.PushBack(Value().SetDouble(datum), allocator);
    }
    // terminate properly
    if (count > 0) {
      result.PushBack(Value().SetDouble(activeVal), allocator);
      result.PushBack(Value().SetInt(count), allocator);
    }
  }

  void renderCostFunction(rapidjson::Document &request,
                          rapidjson::Document &response,
                          msg::Advertisement::Reader adv) {

    AdvFunc interpreter(adv);
    auto pqProf = adv.getPQProfile();
    Eigen::AlignedBoxXd bb = interpreter.rectangularHull(pqProf, {});
    // Compute the axis-aligned bounding box around the PQ profile (which is the
    // domain of the cost function)

    // Construct a suitable grid of evaluation points
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

    // rasterize cost function on given grid
    Eigen::MatrixXd rasterizedCF(
        evalFunOnPQDomain(adv.getCostFunction(), pqProf, interpreter, p, q));

    // apply runlength compression to the elements NaN and zero (it is expected
    // that there will be many 'burst' patters of both elements, hence runlength
    // compression makes sense here)
    auto& allocator = response.GetAllocator();
    auto compressedM(runlengthCompress(
        rasterizedCF,
        {static_cast<double>(0), std::numeric_limits<double>::quiet_NaN()},
        allocator));

    rapidjson::Value cf(rapidjson::kObjectType);
    cf.AddMember("data",compressedM,allocator);
    cf.AddMember("originP",p(0),allocator);
    cf.AddMember("originQ",q(0),allocator);
    cf.AddMember("dimP",p.size(),allocator);
    cf.AddMember("dimQ",q.size(),allocator);

    response.AddMember("cf", cf, allocator);
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
          Document jsonRequest;
          jsonRequest.Parse(jsonData.c_str());

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

            renderCostFunction(jsonRequest,jsonReply,msg.getAdvertisement());

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
