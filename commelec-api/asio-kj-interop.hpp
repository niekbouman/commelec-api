#ifndef ASIOKJINTEROP_H
#define ASIOKJINTEROP_H

#include <kj/io.h>
#include <vector>
#include <boost/asio.hpp>

class AsioKJOutBufferAdapter : public kj::OutputStream {
  // This is an adapter that lets Cap'n Proto write to Asio's "scatter-gatter"
  // buffer sequences.
  // This class does "zero-copy", i.e., no actual data is copied, just the
  // pointers to the buffers.
  // Hence, the owner of the cap'n proto buffers must retain the memory until
  // the asio code that will actually use the data is finished with it
public:
  AsioKJOutBufferAdapter() : totalLen(0), first(true) {}
  void write(const void *buffer, size_t size) override {
    if (first) {
      // we have to copy the data of the first call to write (this contains the
      // segment table)
      auto buf = static_cast<const kj::byte *>(buffer);
      table.assign(buf, buf + size);
      bufs.push_back(boost::asio::buffer(table.data(), size));
      first = false;
    } else {
      bufs.push_back(boost::asio::buffer(buffer, size));
    }
    totalLen += size;
  }
  const std::vector<boost::asio::const_buffer> &get_buffer_sequence() {
    return bufs;
  }
  size_t totalSize() const { return totalLen; }

private:
  size_t totalLen;
  bool first;
  std::vector<boost::asio::const_buffer> bufs;
  std::vector<kj::byte> table;
};

#endif 
