#include "serialization.hpp"
#include <capnp/serialize-packed.h>

size_t messageByteSize(::capnp::MessageBuilder &builder) {
  size_t sz = 0;
  auto segments = builder.getSegmentsForOutput();
  for (auto segment : segments) {
    sz += segment.asBytes().size();
  }
  return sz;
}

VectorBuffer::VectorBuffer(std::vector<kj::byte> &vec)
    : _vector(vec), bufferPos(vec.data()) {}
kj::ArrayPtr<kj::byte> VectorBuffer::getWriteBuffer() {
  return kj::ArrayPtr<kj::byte>(bufferPos, available());
}

size_t VectorBuffer::available() {
  return _vector.data() + _vector.size() - bufferPos;
}

void VectorBuffer::write(const void *src, size_t size) {
  if (src == bufferPos) {
    // caller wrote directly into our buffer.
    bufferPos += size;
  } else {
    if (size > available()) {
      // resize vector (can be expensive, but can be avoided by initialising
      // the vector with a large enough size)
      _vector.resize(_vector.size() + size);
    }
    memcpy(bufferPos, src, size);
    bufferPos += size;
  }
}

void writePackedMessage(std::vector<uint8_t> &output,
                        ::capnp::MessageBuilder &builder) {
  VectorBuffer buf(output);
  writePackedMessage(buf, builder.getSegmentsForOutput());
  output.resize(output.size() - buf.available());
}

size_t packToByteArray(capnp::MallocMessageBuilder &builder, uint8_t *buffer,
                       size_t bufsize) {

  std::vector<uint8_t> packedDataBuffer(messageByteSize(builder));
  // buffer to hold the packed advertisement
  // ( messageByteSize(msg) is a crude upper bound for the buffer size.
  // The exact size that we will actually need is unknown at this point)

  writePackedMessage(packedDataBuffer, builder);
  // will resize packedDataBuffer to the exact size of the packed data

  auto advBytesize = packedDataBuffer.size();

  if (advBytesize <= bufsize)
    memcpy(buffer, packedDataBuffer.data(), advBytesize);

  // do not copy data if there is too little space in the buffer

  return advBytesize;
}

