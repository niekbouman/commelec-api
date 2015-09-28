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

//######################################################################
// Packed vs unpacked serialization (policy classes)
//######################################################################
struct PackedSerialization {
  // serialize and pack data and send it over UDP
  inline void serializeAndAsyncSend(capnp::MallocMessageBuilder &builder,
                             boost::asio::ip::udp::socket &socket,
                             boost::asio::ip::udp::endpoint endpoint,
                             boost::asio::yield_context yield, bool debug = false) {
    std::vector<uint8_t> packedDataBuffer(messageByteSize(builder));
    // buffer to hold the packed advertisement
    // ( messageByteSize(msg) is a crude upper bound for the buffer size.
    // The exact size that we will actually need is unknown at this point)

    writePackedMessage(packedDataBuffer, builder);
    // will resize packedDataBuffer to the exact size of the packed data
    
    if (debug)
    {
      auto buf = boost::asio::const_buffer(boost::asio::buffer(packedDataBuffer));
      AdvValidator<PackedSerialization> val(buf);
    }

    auto bytesWritten = socket.async_send_to(
        boost::asio::buffer(packedDataBuffer), endpoint, yield);
    if (packedDataBuffer.size() != bytesWritten)
      throw std::runtime_error(
          "Could not write message in its entirety to the socket");
  }

  struct CapnpReader {
    CapnpReader(capnp::byte *bufferPtr, size_t bufferSizeInBytes)
        : is(::kj::ArrayPtr<const capnp::byte>(bufferPtr, bufferSizeInBytes)),
          reader(is) {}
    CapnpReader(const boost::asio::const_buffer buffer)
        : is(::kj::ArrayPtr<const capnp::byte>(
              boost::asio::buffer_cast<const capnp::byte *>(buffer),
              boost::asio::buffer_size(buffer))),
          reader(is) {}
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
  inline void serializeAndAsyncSend(capnp::MallocMessageBuilder &builder,
                             boost::asio::ip::udp::socket &socket, boost::asio::ip::udp::endpoint endpoint,
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

    auto bytesWritten =
        socket.async_send_to(adapter.get_buffer_sequence(), endpoint, yield);
    // data are read directly from the MallocMessageBuilder (hence, builder
    // should not be destroyed before the async write operation finishes

    if (adapter.totalSize() != bytesWritten)
      throw std::runtime_error(
          "Could not write message in its entirety to the socket");
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
