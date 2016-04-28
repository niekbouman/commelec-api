#ifndef SENDERPOLICIES_H
#define SENDERPOLICIES_H

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <boost/asio/spawn.hpp>
#include <boost/asio/basic_datagram_socket.hpp>
#include <commelec-api/serialization.hpp>
#include <commelec-api/schema.capnp.h>
#include <commelec-api/asio-kj-interop.hpp>
#include <commelec-api/adv-validation.hpp>

/*! \file
\brief Packed vs. unpacked serialization and de-serialization (policy classes)

One can use this policy in the following way:
 
~~~~{.cpp}
template <typename PackingPolicy = PackedSerialization>
class Foo : private PackingPolicy {
  using PackingPolicy::serializeAndAsyncSend;
  using typename PackingPolicy::CapnpReader; // note the typename keyword here

public:

  // etc

};
~~~~

Then, one can use the class and easily switch between packed and unpacked serialization/de-serialization
~~~~{.cpp}
Foo<> myfoo; // Packed serialization (from default setting)
Foo<PackedSerialization> bar; // Packed serialization (explicitly)
Foo<NonPackedSerialization> baz; // Non-Packed serialization (explicitly)
~~~~
*/

/** 
Policy class for easily using <a href="http://capnproto.org">Cap'n Proto</a> with the <a href="http://think-async.com">Boost.asio</a> library
-- Packed serialization and deserialization

Here "packed" means that we use Cap'n Proto's elementary compression feature

The actual policy here is about whether or not one uses packing. The NonPackedSerialization can be used as a drop-in replacement to disable packing.
*/ 
struct PackedSerialization {
public:
  /** Serialize and pack data and send it over UDP (to possibly multiple endpoints)
   
  If debug is true, it is assumed that the message which is transmitted is a Commelec advertisement, 
and some elementary checks are performed on this advertisement by using the AdvValidator class.
   */ 
  inline void
  serializeAndAsyncSend(capnp::MallocMessageBuilder &builder,
                        boost::asio::ip::udp::socket &socket,
                        const std::vector<boost::asio::ip::udp::endpoint> &endpoints,
                        boost::asio::yield_context yield, bool debug = false)

  {

    std::vector<uint8_t> packedDataBuffer(messageByteSize(builder));
    //packedDataBuffer.resize(messageByteSize(builder));


    // buffer to hold the packed advertisement
    // ( messageByteSize(msg) is a crude upper bound for the buffer size.
    // The exact size that we will actually need is unknown at this point)

    writePackedMessage(packedDataBuffer, builder);
    // will resize packedDataBuffer to the exact size of the packed data

    if (debug) {
      auto buf =
          boost::asio::const_buffer(boost::asio::buffer(packedDataBuffer));
      AdvValidator<PackedSerialization> val(buf);
    }

    for (const auto &ep : endpoints) {
      auto bytesWritten = socket.async_send_to(
          boost::asio::buffer(packedDataBuffer), ep, yield);
      if (packedDataBuffer.size() != bytesWritten)
        throw std::runtime_error(
            "Could not write message in its entirety to the socket");
    }
  }

  /** Overload of the PackedSerialization::serializeAndAsyncSend function for transmitting to a single endpoint
   */ 
  inline void serializeAndAsyncSend(capnp::MallocMessageBuilder &builder,
                             boost::asio::ip::udp::socket &socket,
                             boost::asio::ip::udp::endpoint endpoint,
                             boost::asio::yield_context yield, bool debug = false)
  {
    serializeAndAsyncSend(builder, socket, std::vector<boost::asio::ip::udp::endpoint>{endpoint}, yield, debug);
  }


  /** Policy class to read a Commelec message from a buffer.
   */
  struct CapnpReader {
    CapnpReader(capnp::byte *bufferPtr, size_t bufferSizeInBytes)
        : is(::kj::ArrayPtr<const capnp::byte>(bufferPtr, bufferSizeInBytes)),
          reader(is) {}
    CapnpReader(const boost::asio::const_buffer buffer)
        : is(::kj::ArrayPtr<const capnp::byte>(
              boost::asio::buffer_cast<const capnp::byte *>(buffer),
              boost::asio::buffer_size(buffer))),
          reader(is) {}

    /** Get a Cap'n Proto reader for the Message type
    
    Example: Receive a packet from a socket and unpack it using Cap 'n Proto, 
    and get a msg::Message::Reader to access the Commelec message
    
    ~~~~{.cpp}
    
    std::array<uint8_t, 1 << 16> _data; // create buffer
    auto asio_buffer = boost::asio::buffer(_data); // asio buffer type
    boost::asio::ip::udp::endpoint sender_endpoint; // asio endpoint
    
    size_t bytes_received = _socket.async_receive_from(asio_buffer, sender_endpoint, yield);
    // receive packet asynchronously. 'yield' is the yield context
    
    CapnpReader reader(asio_buffer); // perform the unpacking
    
    auto msg = reader.getMessage(); //msg is of type msg::Message::Reader
    
    if(msg.hasAdvertisement()) {
      // ...
    }
    ~~~~

    For more details about coroutines and yield contexts, see <a href="http://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/spawn.html">asio's documentation on this topic.</a>
    */
    inline msg::Message::Reader getMessage() {
      return reader.getRoot<msg::Message>();
    }

  private:
    ::kj::ArrayInputStream is;
    ::capnp::PackedMessageReader reader;
  };
};

struct NonPackedSerialization {
  // serialize data and send it over UDP (packing is omitted)
  inline void serializeAndAsyncSend(
      capnp::MallocMessageBuilder &builder,
      boost::asio::ip::udp::socket &socket,
      const std::vector<boost::asio::ip::udp::endpoint> &endpoints,
      boost::asio::yield_context yield, bool debug = false) {
    AsioKJOutBufferAdapter adapter;
    writeMessage(adapter, builder);
    // almost no data is copied (except the first segment), instead, a list of tuples (pointer to data, data size)
    // is created, which async_send_to can use as a "scatter-gatter" buffer
    // (i.e., a buffer that consist of several sub-buffers at various memory
    // locations)

    if (debug) {
      // Copy data into a vector and re-interpret this data as an advertisement
      // and verify the validity
      std::vector<uint8_t> tmpBuf;
      tmpBuf.reserve(adapter.totalSize());

      auto iterBegin = buffers_begin(adapter.get_buffer_sequence());
      auto iterEnd = buffers_end(adapter.get_buffer_sequence());

      tmpBuf.assign(iterBegin, iterEnd);

      auto asio_buf = boost::asio::buffer(tmpBuf);
      auto const_asio_buf = boost::asio::const_buffer(asio_buf);
      AdvValidator<NonPackedSerialization> val(const_asio_buf);
    }

    for (const auto &ep : endpoints) {
      auto bytesWritten =
          socket.async_send_to(adapter.get_buffer_sequence(), ep, yield);
      // data are read directly from the MallocMessageBuilder (hence, builder
      // should not be destroyed before the async write operation finishes

      if (adapter.totalSize() != bytesWritten)
        throw std::runtime_error(
            "Could not write message in its entirety to the socket");
    }
  }

  inline void serializeAndAsyncSend(capnp::MallocMessageBuilder &builder,
                                    boost::asio::ip::udp::socket &socket,
                                    boost::asio::ip::udp::endpoint endpoint,
                                    boost::asio::yield_context yield,
                                    bool debug = false) {

    serializeAndAsyncSend(
        builder, socket,
        std::vector<boost::asio::ip::udp::endpoint>{endpoint}, yield,
        debug);
  }

  // TODO: validate that the buffer size (in bytes) is a multiple of 8
  struct CapnpReader {
    CapnpReader(capnp::byte *bufferPtr, size_t bufferSizeInBytes)
        : reader(::kj::ArrayPtr<const capnp::word>(reinterpret_cast<capnp::word*>(bufferPtr), bufferSizeInBytes/8 ))
          {}
    CapnpReader(const boost::asio::const_buffer buffer)
        : reader(::kj::ArrayPtr<const capnp::word>(
              boost::asio::buffer_cast<const capnp::word *>(buffer),
              boost::asio::buffer_size(buffer)/8))
          {}
    inline msg::Message::Reader getMessage() {
      return reader.getRoot<msg::Message>();
    }
  private:
    ::capnp::FlatArrayMessageReader reader;
  };
};

#endif
